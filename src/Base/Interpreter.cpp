/***************************************************************************
 *   Copyright (c) 2002 Jürgen Riegel <juergen.riegel@web.de>              *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Library General Public License (LGPL)   *
 *   as published by the Free Software Foundation; either version 2 of     *
 *   the License, or (at your option) any later version.                   *
 *   for detail see the LICENCE text file.                                 *
 *                                                                         *
 *   FreeCAD is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with FreeCAD; if not, write to the Free Software        *
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  *
 *   USA                                                                   *
 *                                                                         *
 ***************************************************************************/


#include "PreCompiled.h"

#ifndef _PreComp_
#   include <sstream>
#   include <boost/regex.hpp>
#endif

#include "Console.h"
#include "Interpreter.h"
#include "FileInfo.h"
#include "Stream.h"
#include "PyTools.h"
#include "Exception.h"
#include "PyObjectBase.h"
#include <CXX/Extensions.hxx>
#include <frameobject.h>
#include <traceback.h>

#include "ExceptionFactory.h"

#if PY_MAJOR_VERSION >= 3
# define PY_AS_LONG(pyObj) PyLong_AS_LONG(pyObj)
# define PY_LONG_CHECK(pyObj) PyLong_Check(pyObj)
# define PY_AS_C_STRING(pyObj) PyUnicode_AsUTF8(pyObj)
#else
# define PY_AS_LONG(pyObj) PyInt_AS_LONG(pyObj)
# define PY_LONG_CHECK(pyObj) PyInt_Check(pyObj)
# define PY_AS_C_STRING(pyObj) PyBytes_AsString(pyObj)
#endif


char format2[1024];  //Warning! Can't go over 512 characters!!!
unsigned int format2_len = 1024;

using namespace Base;

#if PY_VERSION_HEX <= 0x02070000
#error "Use Python2.7.x or higher"
#endif

PyException::SwapIn::SwapIn(PyThreadState *newState) :
    m_oldState(nullptr)
{
    // ensure we don't re-swap and acquire lock (leads to deadlock)
    if (!static_GILHeld) {
        static_GILHeld = true;
        // acquire for global thread
        m_GILState = PyGILState_Ensure();
        m_oldState = PyThreadState_Swap(newState);
    }
}
PyException::SwapIn::~SwapIn()
{
    // ensure we only swap back if this instance was the swapping once
    // must only swap when reached bottom of swap stack
    if (m_oldState) {
        // release from sub interpreter thread and swap back to old thread

        PyThreadState_Swap(m_oldState);
        PyGILState_Release(m_GILState);
        m_oldState = nullptr;

        if (static_GILHeld)
            static_GILHeld = false;
    }
}
bool PyException::SwapIn::static_GILHeld = false;

PyException::PyException():
    Exception(),
    _offset(-1),
    _pyType(nullptr),
    _pyValue(nullptr),
    _pyTraceback(nullptr)
{
    // This should be done in the constructor because when doing
    // in the destructor it's not always clear when it is called
    // and thus may clear a Python exception when it should not.
    PyGILStateLocker locker;

    _pyState = PyThreadState_GET();
    PyErr_Fetch(&_pyType, &_pyValue, &_pyTraceback);
    PyErr_NormalizeException(&_pyType, &_pyValue, &_pyTraceback);

    Py_XINCREF(_pyType);
    Py_XINCREF(_pyTraceback);
    Py_XINCREF(_pyValue);

    _init();
}

PyException::PyException(const Py::Object &obj) :
    Exception(),
    _offset(-1),
    _pyType(nullptr),
    _pyValue(nullptr),
    _pyTraceback(nullptr)
{
    // This should be done in the constructor because when doing
    // in the destructor it's not always clear when it is called
    // and thus may clear a Python exception when it should not.
    PyGILStateLocker locker;

    _pyState = PyThreadState_GET();
    _pyValue = (PyObject*)obj.ptr();
    if (_pyValue)
        _pyType = (PyObject*)_pyValue->ob_type;

    _sErrMsg = obj.as_string();
    // WARNING: we are assuming that python type object will never be
    // destroyed, so we don't keep reference here to save book-keeping in
    // our copy constructor and destructor
    //_exceptionType = (PyObject*)obj.ptr()->ob_type;
    //_errorType = obj.ptr()->ob_type->tp_name;
}

// set exception from tracer function (ie within debugger)
PyException::PyException(PyObject *tracebackArg) :
    Exception(),
    _offset(-1),
    _pyType(nullptr),
    _pyValue(nullptr),
    _pyTraceback(nullptr),
    _pyState(nullptr)
{
    if (!PyTuple_Check(tracebackArg))
        return;

    PyGILStateLocker locker;

    _pyState = PyThreadState_GET();
    _pyType = PyTuple_GET_ITEM(tracebackArg, 0);
    _pyValue = PyTuple_GET_ITEM(tracebackArg, 1);
    _pyTraceback = PyTuple_GET_ITEM(tracebackArg, 2);
    Py_XINCREF(_pyType);
    Py_XINCREF(_pyTraceback);
    Py_XINCREF(_pyValue);

    _init();
}

PyException::PyException(const PyException &other) :
    Exception(),
    _offset(-1),
    _pyType(nullptr),
    _pyValue(nullptr),
    _pyTraceback(nullptr),
    _pyState(nullptr)
{
    _clone(other);
}

// helper function for construtors, should never be called from another method
void PyException::_init()
{    
    // calculate lineNr, functionName, fileName
    if (_pyTraceback && PyTraceBack_Check(_pyTraceback)) { // no traceback when in global space
        PyTracebackObject* tb = (PyTracebackObject*)_pyTraceback;
        while (tb && tb->tb_next) // traverse to find correct linenr
            tb = tb->tb_next;
        _line = PyCode_Addr2Line(tb->tb_frame->f_code, tb->tb_frame->f_lasti);

        // functionName && fileName
        const char *funcname = PY_AS_C_STRING(tb->tb_frame->f_code->co_name);
        const char *filename = PY_AS_C_STRING(tb->tb_frame->f_code->co_filename);
        _function = funcname;
        _file = filename;

    } else if (_pyValue) {
        // in global space, we don't get here when its a traceback
        // base class check setPyObject
        setPyObject(PP_PyDict_Object);

#if PY_MAJOR_VERSION >= 3
        // hack, py3.7 needs to call args before filename?
        PyObject *vlu = PyObject_GetAttrString(_pyValue, "args");
        Py_XDECREF(vlu);
#endif

        // linenr global space
        if (_line == 0) {
            _line = _getAttrAsInt(_pyValue, "lineno");
        }

        // fileName global space
        if (_file.empty()) {
            _file = _getAttrAsString(_pyValue, "filename");
        }
    }

    if (_pyValue) {
//        // we get here regardless of traceback or not
        _offset = _getAttrAsInt(_pyValue, "offset");
    }
    // must restore for PP_Fetch_Error_Text()
    PyErr_Restore(_pyType, _pyValue, _pyTraceback);

    // PP_FetchError_Text Py_DECREFs _pyType, _pyValue and _pyTraceback
    // need to restore these
    Py_ssize_t  refType       = _pyType ? _pyType->ob_refcnt : 0,
                refValue      = _pyValue ? _pyValue->ob_refcnt : 0,
                refTraceback  = _pyTraceback ? _pyTraceback->ob_refcnt : 0;

    // make use of PyTools.c
    PP_Fetch_Error_Text();    /* fetch (and clear) exception */

    // restore refconts
    if (_pyType && _pyType->ob_refcnt < refType)
        _pyType->ob_refcnt = refType;
    if (_pyValue && _pyValue->ob_refcnt < refValue)
        _pyValue->ob_refcnt = refValue;
    if (_pyTraceback && _pyTraceback->ob_refcnt < refTraceback)
        _pyTraceback->ob_refcnt = refTraceback;


    std::string prefix = PP_last_error_type; /* exception name text */
//  prefix += ": ";
    std::string error = PP_last_error_info;            /* exception data text */
#if 0
    // The Python exceptions might be thrown from nested functions, so take
    // into account not to add the same prefix several times
    std::string::size_type pos = error.find(prefix);
    if (pos == std::string::npos)
        _sErrMsg = prefix + error;
    else
        _sErrMsg = error;
#endif
    if (_sErrMsg.empty())
        _sErrMsg = error;
    if (_errorType.empty())
        _errorType = prefix;

    if (_pyType == nullptr)
        _pyType = PP_last_exception_type;

    if (PP_last_exception_type) {
        // WARNING: we are assuming that python type object will never be
        // destroyed, so we don't keep reference here to save book-keeping in
        // our copy constructor and destructor
        Py_DECREF(PP_last_exception_type);
        PP_last_exception_type = 0;

    }

    if (_stackTrace.empty())
        _stackTrace = PP_last_error_trace;     /* exception traceback text */
    PyErr_Clear(); // must be called to keep Python interpreter in a valid state (Werner)
}

int PyException::_getAttrAsInt(PyObject *obj, const char *attr)
{
#if PY_MAJOR_VERSION >= 3
    // hack, py3.7 needs to call args before getattr
    PyObject *vlu = PyObject_GetAttrString(obj, "args");
    Py_XDECREF(vlu);
#endif
    PyObject *longObj = PyObject_GetAttrString(obj, attr); // new ref
    if (longObj) {
        long value = PY_AS_LONG(longObj);
        return static_cast<int>(value);
    } else
        PyErr_Clear();
    return 0;
}

const char *PyException::_getAttrAsString(PyObject *obj, const char *attr)
{
#if PY_MAJOR_VERSION >= 3
    // hack, py3.7 needs to call args before getattr
    PyObject *vlu = PyObject_GetAttrString(obj, "args");
    Py_XDECREF(vlu);
#endif

    PyObject *strObj = PyObject_GetAttrString(obj, attr); // new ref
    if (strObj) {
        if (PyBytes_Check(strObj)) {
            const char *msg = PyBytes_AS_STRING(strObj);
            return  msg;
        } else if (PyUnicode_Check(strObj)) {
#if PY_MAJOR_VERSION >= 3
            const char *msg = PyUnicode_AsUTF8(strObj);
#else
            PyObject *pyBytes = PyUnicode_AsUTF8String(strObj);
            const char *msg = PyBytes_AsString(pyBytes);
            Py_XDECREF(pyBytes);
#endif
            return msg;
        }
        Py_XDECREF(strObj);
    } else
        PyErr_Clear();
    return nullptr;
}

// helper function, should be called from copy constructor or from copy operator
void PyException::_clone(const PyException &other)
{
    // clear possible old state (release memory)
    if (_pyState) {
        SwapIn oldState(_pyState);
        Py_XDECREF(_pyType);
        Py_XDECREF(_pyValue);
        Py_XDECREF(_pyTraceback);
    }

    SwapIn myState(other._pyState);

    _pyType = other._pyType;
    _pyTraceback = other._pyTraceback;
    _pyValue = other._pyValue;
    _pyState =  other._pyState;
    _sErrMsg = other._sErrMsg;
    _errorType = other._errorType;
    _file = other._file;
    _function = other._function;
    _isReported = other._isReported;
    _isTranslatable = other._isTranslatable;
    _line = other._line;
    _stackTrace = other._stackTrace;
    _offset = other._offset;

    // grab a reference, prevent python from deleting these
    Py_XINCREF(_pyType);
    Py_XINCREF(_pyTraceback);
    Py_XINCREF(_pyValue);
}

PyException::~PyException() throw()
{
    // we might get deleted after file has ended
    // and some other python code is running
    SwapIn myState(_pyState);

    Py_XDECREF(_pyType);
    Py_XDECREF(_pyValue);
    Py_XDECREF(_pyTraceback);
}

PyException &PyException::operator =(const PyException &other)
{
    _clone(other);
    return *this;
}

void PyException::ThrowException(void)
{
    PyException myexcp;
    myexcp.ReportException();
    myexcp.raiseException();
}

void PyException::raiseException() {
    PyGILStateLocker locker;

    if (PP_PyDict_Object!=NULL) {
        // delete the Python dict upon destruction of edict
        Py::Dict edict(PP_PyDict_Object, true);
        PP_PyDict_Object = 0;

        std::string exceptionname;
        if (_pyType == Base::BaseExceptionFreeCADAbort)
            edict.setItem("sclassname",
                    Py::String(typeid(Base::AbortException).name()));
        if (_isReported)
            edict.setItem("breported", Py::True());
        Base::ExceptionFactory::Instance().raiseException(edict.ptr());
    }

    if (_pyType == Base::BaseExceptionFreeCADAbort) {
        Base::AbortException e(_sErrMsg.c_str());
        e.setReported(_isReported);
        throw e;
    }

    throw *this;
}

const std::string PyException::getErrorType(bool extractName /*= false*/) const
{
    if (!extractName)
        return _errorType;

    std::size_t pos1 = _errorType.find_first_of("'\""),
                pos2 = _errorType.find_first_of(_errorType[pos1], pos1 +1) -1;
    std::string cleanTypeName = _errorType.substr(pos1 +1, pos2 - pos1);
    return cleanTypeName;
}

void PyException::ReportException (void) const
{
    if (!_isReported) {
        _isReported = true;
        Base::Console().Error("%s%s: %s\n",
            _stackTrace.c_str(), _errorType.c_str(), what());
    }
}

void PyException::setPyException() const
{
    std::stringstream str;
    str << getStackTrace()
        << getErrorType()
        << ": " << what();
    PyErr_SetString(getPyExceptionType(), str.str().c_str());
}

// ---------------------------------------------------------

Base::PyExceptionInfo::PyExceptionInfo() :
    PyException()
{
    _tracebackLevel = tracebackSize() -1;
}

Base::PyExceptionInfo::PyExceptionInfo(const Base::PyException &exc) :
    PyException(exc)
{
    _tracebackLevel = tracebackSize() -1;
}

PyExceptionInfo::PyExceptionInfo(const PyExceptionInfo &other) :
    PyException(other)
{
    _cloneExcInfo(other);
}

PyExceptionInfo::~PyExceptionInfo() noexcept
{
}

void PyExceptionInfo::_cloneExcInfo(const PyExceptionInfo &other)
{
    _tracebackLevel = other._tracebackLevel;
}

PyExceptionInfo &PyExceptionInfo::operator =(const PyExceptionInfo &other)
{
    _clone(other);
    _cloneExcInfo(other);
    return *this;
}

PyException &PyExceptionInfo::operator =(const PyException &other)
{
    _clone(other);
    _tracebackLevel = tracebackSize() -1;
    return *static_cast<PyException*>(this);
}

bool PyExceptionInfo::isValid() const
{
    if (_pyType == nullptr)
        return false;

    PyInterpreterState *interpState = PyInterpreterState_Head();
    if (interpState == nullptr)
        // no currentState, rely on ThreadState swapping
        return _pyState != nullptr;

    if (interpState && _pyState &&
        interpState == _pyState->interp)
    {
        return true;
    }

    return false; // another interpreter
}

int PyExceptionInfo::tracebackSize() const
{
    if (!isValid())
        return -1;

    if (!_pyTraceback || !PyTraceBack_Check(_pyTraceback))
        return 0;

    int count = 0;
    PyTracebackObject* tb = (PyTracebackObject*)_pyTraceback;
    while (tb != nullptr) {
        tb = tb->tb_next;
        ++count;
    }
    return count;
}

void PyExceptionInfo::setTracebackLevel(int level)
{
    if (!isValid())
        return;

    if (level < 0 && level >= tracebackSize())
        return;

    _tracebackLevel = level -1;
}

std::wstring PyExceptionInfo::currentFile() const
{
    if (!isValid())
        return std::wstring();

    SwapIn myState(_pyState);
    const char *filename = nullptr;

    if (_pyTraceback && PyTraceBack_Check(_pyTraceback)) {
        // has a traceback
        // get the current file as selected from traceback level
        PyTracebackObject* tb = getTracebackFrame();
        filename = PY_AS_C_STRING(tb->tb_frame->f_code->co_filename);
    } else {
        // is global space
        filename = _file.c_str(); // string to wstring
    }

    // workaround cstr to std::wstring
    std::string tmp = filename;
    std::wstring ret(tmp.begin(), tmp.end());

    return ret;
}

int PyExceptionInfo::currentLine() const
{
    if (!isValid())
        return -1;

    SwapIn myState(_pyState);

    if (_pyTraceback && PyTraceBack_Check(_pyTraceback)) { // no traceback when in global space
        PyTracebackObject* tb = getTracebackFrame();
        return PyCode_Addr2Line(tb->tb_frame->f_code, tb->tb_frame->f_lasti);
    }

    // else in global space
    return _line;
}

bool PyExceptionInfo::isWarning() const
{
    return compareException(PyExc_Warning);
}

bool PyExceptionInfo::isSyntaxError() const
{
    return compareException(PyExc_SyntaxError);
}

bool PyExceptionInfo::isIndentationError() const
{
    return compareException(PyExc_IndentationError) ||
           compareException(PyExc_TabError);
}

bool PyExceptionInfo::isKeyboardInterupt() const
{
    return compareException(PyExc_KeyboardInterrupt);
}

bool PyExceptionInfo::isSystemExit() const
{
    return compareException(PyExc_SystemExit);
}

bool PyExceptionInfo::compareException(PyObject * pyExcType) const
{
    if (!isValid())
        return false;

    SwapIn myState(_pyState);
    return PyErr_GivenExceptionMatches(_pyType, pyExcType);
}

std::wstring PyExceptionInfo::text() const
{
    if (!isValid())
        return std::wstring();

    SwapIn myState(_pyState);

    PyObject *vlu = getAttr("text"); // new ref
    if (!vlu) {
        vlu = PyObject_Str(_pyValue);
    }
    if (!vlu)
        return std::wstring();

    const char *txt = PY_AS_C_STRING(vlu);
    Py_XDECREF(vlu);

    // workaround cstr to std::wstring
    std::string tmp = txt;
    std::wstring ret(tmp.begin(), tmp.end());

    return ret;
}

const PyThreadState *PyExceptionInfo::threadState() const
{
    return _pyState;
}


PyTracebackObject *PyExceptionInfo::getTracebackFrame() const
{
    SwapIn myState(_pyState);

    int lvl = _tracebackLevel;
    PyTracebackObject* tb = (PyTracebackObject*)_pyTraceback;
    while (lvl > 0) {
        tb = tb->tb_next;
        --lvl;
    }
    return tb;
}

// returns new ref, caller need to Py_DECREF
PyObject *PyExceptionInfo::getAttr(const char *attr) const // new ref
{
    SwapIn myState(_pyState);

    PyObject *vlu = nullptr;
    if (!PyObject_HasAttrString(_pyValue, attr)) {
        vlu = PyObject_GetAttrString(_pyValue, "args"); // hack, py3.7 needs to call args before filename?
        Py_XDECREF(vlu);
    }
    vlu = PyObject_GetAttrString(_pyValue, attr); // new ref
    if (!vlu)
        return nullptr;
    return vlu;
}

/// borrowed ref, caller need to Py_DECREF
PyObject *PyExceptionInfo::getItem(const char *attr) const
{

    SwapIn myState(_pyState);

    PyObject *vlu = nullptr;
    if (PyDict_Check(_pyValue)) {
        vlu = PyDict_GetItemString(_pyValue, attr); // borrowed ref

    } else if (PyList_Check(_pyValue)) {
        int idx = atoi(attr);
        if (PyList_Size(_pyValue) >= idx)
            vlu = PyList_GET_ITEM(_pyValue, idx); // borrowed ref
    }

    Py_XINCREF(vlu); // we decref in caller functions so we need to inref here
    return vlu;
}

// ---------------------------------------------------------

SystemExitException::SystemExitException()
{
    // Set exception message and code based upon the python sys.exit() code and/or message
    // based upon the following sys.exit() call semantics.
    //
    // Invocation       |  _exitCode  |  _sErrMsg
    // ---------------- +  ---------  +  --------
    // sys.exit(int#)   |   int#      |   "System Exit"
    // sys.exit(string) |   1         |   string
    // sys.exit()       |   1         |   "System Exit"

    long int errCode = 1;
    std::string errMsg  = "System exit";
    PyObject  *type, *value, *traceback, *code;

    PyGILStateLocker locker;
    PyErr_Fetch(&type, &value, &traceback);
    PyErr_NormalizeException(&type, &value, &traceback);

    if (value) {
        code = PyObject_GetAttrString(value, "code");
        if (code != NULL && value != Py_None) {
           Py_DECREF(value);
           value = code;
        }

        if (PY_LONG_CHECK(value)) {
            errCode = PY_AS_LONG(value);
        }
        else {
            const char *str = PY_AS_C_STRING(value);
            if (str)
                errMsg = errMsg + ": " + str;
        }
    }

    _sErrMsg  = errMsg;
    _exitCode = errCode;
}

// ---------------------------------------------------------

// Fixes #0000831: python print causes File descriptor error on windows
class PythonStdOutput : public Py::PythonExtension<PythonStdOutput>
{
public:
    static void init_type(void)
    {
        behaviors().name("PythonStdOutput");
        behaviors().doc("Python standard output");
        add_varargs_method("write",&PythonStdOutput::write,"write()");
        add_varargs_method("flush",&PythonStdOutput::flush,"flush()");
    }

    PythonStdOutput()
    {
    }
    ~PythonStdOutput()
    {
    }

    Py::Object write(const Py::Tuple&)
    {
        return Py::None();
    }
    Py::Object flush(const Py::Tuple&)
    {
        return Py::None();
    }
};

// ---------------------------------------------------------

InterpreterSingleton::InterpreterSingleton()
{
    this->_global = 0;
}

InterpreterSingleton::~InterpreterSingleton()
{

}


std::string InterpreterSingleton::runString(const char *sCmd)
{
    PyObject *module, *dict, *presult;          /* "exec code in d, d" */

    PyGILStateLocker locker;
    module = PP_Load_Module("__main__");         /* get module, init python */
    if (module == NULL)
        throw PyException();                         /* not incref'd */
    dict = PyModule_GetDict(module);            /* get dict namespace */
    if (dict == NULL)
        throw PyException();                           /* not incref'd */


    presult = PyRun_String(sCmd, Py_file_input, dict, dict); /* eval direct */
    if (!presult) {
        if (PyErr_ExceptionMatches(PyExc_SystemExit))
            throw SystemExitException();
        else {
            PyException::ThrowException();
            return std::string(); // just to quieten code analyzers
            //throw PyException();
        }
    }

    PyObject* repr = PyObject_Repr(presult);
    Py_DECREF(presult);
    if (repr) {
        std::string ret(PY_AS_C_STRING(repr));

        Py_DECREF(repr);
        return ret;
    }
    else {
        PyErr_Clear();
        return std::string();
    }
}

/** runStringWithKey(psCmd, key, key_initial_value)
 * psCmd is python script to run
 * key is the name of a python string variable the script will have read/write
 * access to during script execution.  It will be our return value.
 * key_initial_value is the initial value c++ will set before calling the script
 * If the script runs successfully it will be able to change the value of key as
 * the return value, but if there is a runtime error key will not be changed even
 * if the error occurs after changing it inside the script.
 */

std::string InterpreterSingleton::runStringWithKey(const char *psCmd, const char *key, const char *key_initial_value)
{
    PyGILStateLocker locker;
    Py::Module module("__main__");
    Py::Dict globalDictionary = module.getDict();
    Py::Dict localDictionary;
    Py::String initial_value(key_initial_value);
    localDictionary.setItem(key, initial_value);

    PyObject* presult = PyRun_String(psCmd, Py_file_input, globalDictionary.ptr(), localDictionary.ptr());
    if (!presult) {
        if (PyErr_ExceptionMatches(PyExc_SystemExit)) {
            throw SystemExitException();
        }
        else {
            PyException::ThrowException();
            return std::string(); // just to quieten code analyzers
        }
    }
    Py_DECREF(presult);

    Py::Object key_return_value = localDictionary.getItem(key);
    if (!key_return_value.isString())
        key_return_value = key_return_value.str();

#if PY_MAJOR_VERSION >= 3
    Py::Bytes str = Py::String(key_return_value).encode("utf-8", "~E~");
    std::string result = static_cast<std::string>(str);
#else
    std::string result = static_cast<std::string>(Py::String(key_return_value));
#endif
    return result;
}

Py::Object InterpreterSingleton::runStringObject(const char *sCmd)
{
    PyObject *module, *dict, *presult;          /* "exec code in d, d" */

    PyGILStateLocker locker;
    module = PP_Load_Module("__main__");         /* get module, init python */
    if (module == NULL)
        throw PyException();                         /* not incref'd */
    dict = PyModule_GetDict(module);            /* get dict namespace */
    if (dict == NULL)
        throw PyException();                           /* not incref'd */


    presult = PyRun_String(sCmd, Py_eval_input, dict, dict); /* eval direct */
    if (!presult) {
        if (PyErr_ExceptionMatches(PyExc_SystemExit))
            throw SystemExitException();
        else
            throw PyException();
    }

    return Py::asObject(presult);
}

void InterpreterSingleton::systemExit(void)
{
    /* This code is taken from the original Python code */
    PyObject *exception, *value, *tb;
    int exitcode = 0;

    PyErr_Fetch(&exception, &value, &tb);
#if PY_MAJOR_VERSION < 3
    if (Py_FlushLine())
        PyErr_Clear();
#endif
    fflush(stdout);
    if (value == NULL || value == Py_None)
        goto done;
    if (PyExceptionInstance_Check(value)) {
        /* The error code should be in the `code' attribute. */
        PyObject *code = PyObject_GetAttrString(value, "code");
        if (code) {
            Py_DECREF(value);
            value = code;
            if (value == Py_None)
                goto done;
        }
        /* If we failed to dig out the 'code' attribute,
           just let the else clause below print the error. */
    }
    if (PY_LONG_CHECK(value))
        exitcode = (int)PY_AS_LONG(value);

    else {
        PyObject_Print(value, stderr, Py_PRINT_RAW);
        PySys_WriteStderr("\n");
        exitcode = 1;
    }
done:
    /* Restore and clear the exception info, in order to properly decref
     * the exception, value, and traceback.  If we just exit instead,
     * these leak, which confuses PYTHONDUMPREFS output, and may prevent
     * some finalizers from running.
     */
    PyErr_Restore(exception, value, tb);
    PyErr_Clear();
    Py_Exit(exitcode);
    /* NOTREACHED */
}

void InterpreterSingleton::runInteractiveString(const char *sCmd)
{
    PyObject *module, *dict, *presult;          /* "exec code in d, d" */

    PyGILStateLocker locker;
    module = PP_Load_Module("__main__");         /* get module, init python */
    if (module == NULL)
        throw PyException();                         /* not incref'd */
    dict = PyModule_GetDict(module);            /* get dict namespace */
    if (dict == NULL)
        throw PyException();                           /* not incref'd */

    presult = PyRun_String(sCmd, Py_single_input, dict, dict); /* eval direct */
    if (!presult) {
        if (PyErr_ExceptionMatches(PyExc_SystemExit)) {
            throw SystemExitException();
        }
        /* get latest python exception information */
        /* and print the error to the error output */
        PyObject *errobj, *errdata, *errtraceback;
        PyErr_Fetch(&errobj, &errdata, &errtraceback);

        RuntimeError exc(""); // do not use PyException since this clears the error indicator
        if (errdata) {
#if PY_MAJOR_VERSION >= 3
            if (PyUnicode_Check(errdata))
                exc.setMessage(PyUnicode_AsUTF8(errdata));
#else
            if (PyString_Check(errdata))
                exc.setMessage(PyString_AsString(errdata));
#endif
        }
        PyErr_Restore(errobj, errdata, errtraceback);
        if (PyErr_Occurred())
            PyErr_Print();
        throw exc;
    }
    else
        Py_DECREF(presult);
}

void InterpreterSingleton::runFile(const char*pxFileName, bool local)
{
#ifdef FC_OS_WIN32
    FileInfo fi(pxFileName);
    FILE *fp = _wfopen(fi.toStdWString().c_str(),L"r");
#else
    FILE *fp = fopen(pxFileName,"r");
#endif
    if (fp) {
        PyGILStateLocker locker;
        //std::string encoding = PyUnicode_GetDefaultEncoding();
        //PyUnicode_SetDefaultEncoding("utf-8");
        //PyUnicode_SetDefaultEncoding(encoding.c_str());
        PyObject *module, *dict;
        module = PyImport_AddModule("__main__");
        dict = PyModule_GetDict(module);
        if (local) {
            dict = PyDict_Copy(dict);
        }
        else {
            Py_INCREF(dict); // avoid to further distinguish between local and global dict
        }

        if (PyDict_GetItemString(dict, "__file__") == NULL) {
#if PY_MAJOR_VERSION >= 3
            PyObject *f = PyUnicode_FromString(pxFileName);
#else
            PyObject *f = PyString_FromString(pxFileName);
#endif
            if (f == NULL) {
                fclose(fp);
                Py_DECREF(dict);
                return;
            }
            if (PyDict_SetItemString(dict, "__file__", f) < 0) {
                Py_DECREF(f);
                fclose(fp);
                Py_DECREF(dict);
                return;
            }
            Py_DECREF(f);
        }

        PyObject *result = PyRun_File(fp, pxFileName, Py_file_input, dict, dict);
        fclose(fp);
        Py_DECREF(dict);

        if (!result) {
            if (PyErr_ExceptionMatches(PyExc_SystemExit))
                throw SystemExitException();
            else
                throw PyException();
        }
        Py_DECREF(result);
    }
    else {
        throw FileException("Unknown file", pxFileName);
    }
}

bool InterpreterSingleton::loadModule(const char* psModName)
{
    // buffer acrobatics
    //PyBuf ModName(psModName);
    PyObject *module;

    PyGILStateLocker locker;
    module = PP_Load_Module(psModName);

    if (!module) {
        if (PyErr_ExceptionMatches(PyExc_SystemExit))
            throw SystemExitException();
        else
            throw PyException();
    }

    return true;
}

void InterpreterSingleton::addType(PyTypeObject* Type,PyObject* Module, const char * Name)
{
    // NOTE: To finish the initialization of our own type objects we must
    // call PyType_Ready, otherwise we run into a segmentation fault, later on.
    // This function is responsible for adding inherited slots from a type's base class.
    if (PyType_Ready(Type) < 0) return;
    union PyType_Object pyType = {Type};
    PyModule_AddObject(Module, Name, pyType.o);
}

void InterpreterSingleton::addPythonPath(const char* Path)
{
    PyGILStateLocker locker;
    Py::List list(PySys_GetObject("path"));
    list.append(Py::String(Path));
}

const char* InterpreterSingleton::init(int argc,char *argv[])
{
    if (!Py_IsInitialized()) {
#if PY_MAJOR_VERSION >= 3
#if PY_MINOR_VERSION >= 5
        wchar_t *wargv = Py_DecodeLocale(argv[0], nullptr);
#else
        wchar_t *wargv = _Py_char2wchar(argv[0], nullptr);
#endif
        Py_SetProgramName(wargv);
        PyMem_RawFree(wargv);
#else
        Py_SetProgramName(argv[0]);
#endif
        // There is a serious bug in VS from 2010 until 2013 where the file descriptor for stdin, stdout or stderr
        // returns a valid value for GUI applications (i.e. subsystem = Windows) where it shouldn't.
        // This causes Python to fail during initialization.
        // A workaround is to use freopen on stdin, stdout and stderr. See the class Redirection inside main()
        // https://bugs.python.org/issue17797#msg197474
        //
        Py_Initialize();
        const char* virtualenv = getenv("VIRTUAL_ENV");
        if (virtualenv) {
            PyRun_SimpleString(
                "# Check for virtualenv, and activate if present.\n"
                "# See https://virtualenv.pypa.io/en/latest/userguide/#using-virtualenv-without-bin-python\n"
                "import os\n"
                "import sys\n"
                "base_path = os.getenv(\"VIRTUAL_ENV\")\n"
                "if not base_path is None:\n"
                "    activate_this = os.path.join(base_path, \"bin\", \"activate_this.py\")\n"
                "    exec(open(activate_this).read(), {'__file__':activate_this})\n"
            );
        }

#if PY_VERSION_HEX < 0x03090000
        PyEval_InitThreads();
#endif

#if PY_MAJOR_VERSION >= 3
        size_t size = static_cast<size_t>(argc);
        wchar_t **_argv = new wchar_t*[size];
        for (int i = 0; i < argc; i++) {
#if PY_MINOR_VERSION >= 5
            _argv[i] = Py_DecodeLocale(argv[i], nullptr);
#else
            _argv[i] = _Py_char2wchar(argv[i], nullptr);
#endif
        }
        PySys_SetArgv(argc, _argv);
        for (int i = 0; i < argc; i++)
            PyMem_RawFree(_argv[i]);
        delete [] _argv;
#else
        PySys_SetArgv(argc, argv);
#endif
        PythonStdOutput::init_type();
        this->_global = PyEval_SaveThread();
    }
#if PY_MAJOR_VERSION >= 3
    PyGILStateLocker lock;
#if PY_MINOR_VERSION >= 5
    return Py_EncodeLocale(Py_GetPath(),NULL);
#else
    return _Py_wchar2char(Py_GetPath(),NULL);
#endif
#else
    return Py_GetPath();
#endif
}

void InterpreterSingleton::replaceStdOutput()
{
    PyGILStateLocker locker;
    PythonStdOutput* out = new PythonStdOutput();
    PySys_SetObject("stdout", out);
    PySys_SetObject("stderr", out);
}

int InterpreterSingleton::cleanup(void (*func)(void))
{
    return Py_AtExit( func );
}

void InterpreterSingleton::finalize()
{
    try {
        PyEval_RestoreThread(this->_global);
        Py_Finalize();
    }
    catch (...) {
    }
}

void InterpreterSingleton::runStringArg(const char * psCom,...)
{
    // va stuff
    va_list namelessVars;
    va_start(namelessVars, psCom);  // Get the "..." vars
    int len = vsnprintf(format2, format2_len, psCom, namelessVars);
    va_end(namelessVars);
    if ( len == -1 ) {
        // argument too long
        assert(false);
    }

    runString(format2);
}


// Singelton:

InterpreterSingleton * InterpreterSingleton::_pcSingelton = 0;

InterpreterSingleton & InterpreterSingleton::Instance(void)
{
    // not initialized!
    if (!_pcSingelton)
        _pcSingelton = new InterpreterSingleton();
    return *_pcSingelton;
}

void InterpreterSingleton::Destruct(void)
{
    // not initialized or double destruct!
    assert(_pcSingelton);
    delete _pcSingelton;
    _pcSingelton = 0;
}

int InterpreterSingleton::runCommandLine(const char *prompt)
{
    PyGILStateLocker locker;
    return PP_Run_Command_Line(prompt);
}

/**
 *  Runs a member method of an object with no parameter and no return value
 *  void (void). There are other methods to run with returns
 */
void InterpreterSingleton::runMethodVoid(PyObject *pobject, const char *method)
{
    PyGILStateLocker locker;
    if (PP_Run_Method(pobject ,    // object
                      method,  // run method
                      0,			   // no return type
                      0,		       // so no return object
                      "()")		   // no arguments
            != 0)
        throw PyException(/*"Error running InterpreterSingleton::RunMethodVoid()"*/);

}

PyObject* InterpreterSingleton::runMethodObject(PyObject *pobject, const char *method)
{
    PyObject *pcO;

    PyGILStateLocker locker;
    if (PP_Run_Method(pobject ,    // object
                      method,  // run method
                      "O",		   // return type
                      &pcO,		   // return object
                      "()")		   // no arguments
            != 0)
        throw PyException();

    return pcO;
}

void InterpreterSingleton::runMethod(PyObject *pobject, const char *method,
                                     const char *resfmt,   void *cresult,        /* convert to c/c++ */
                                     const char *argfmt,   ...  )                /* convert to python */
{
    PyObject *pmeth, *pargs, *presult;
    va_list argslist;                              /* "pobject.method(args)" */
    va_start(argslist, argfmt);

    PyGILStateLocker locker;
    pmeth = PyObject_GetAttrString(pobject, method);
    if (pmeth == NULL) {                            /* get callable object */
        va_end(argslist);
        throw AttributeError("Error running InterpreterSingleton::RunMethod() method not defined");                                 /* bound method? has self */
    }

    pargs = Py_VaBuildValue(argfmt, argslist);     /* args: c->python */
    va_end(argslist);

    if (pargs == NULL) {
        Py_DECREF(pmeth);
        throw TypeError("InterpreterSingleton::RunMethod() wrong arguments");
    }

#if PY_VERSION_HEX < 0x03090000
    presult = PyEval_CallObject(pmeth, pargs);   /* run interpreter */
#else
    presult = PyObject_CallObject(pmeth, pargs);   /* run interpreter */
#endif

    Py_DECREF(pmeth);
    Py_DECREF(pargs);
    if (PP_Convert_Result(presult, resfmt, cresult)!= 0) {
        if ( PyErr_Occurred() )
            PyErr_Print();
        throw RuntimeError("Error running InterpreterSingleton::RunMethod() exception in called method");
    }
}

PyObject * InterpreterSingleton::getValue(const char * key, const char * result_var)
{
    PyObject *module, *dict, *presult;          /* "exec code in d, d" */

    PyGILStateLocker locker;
    module = PP_Load_Module("__main__");         /* get module, init python */
    if (module == NULL)
        throw PyException();                         /* not incref'd */
    dict = PyModule_GetDict(module);            /* get dict namespace */
    if (dict == NULL)
        throw PyException();                           /* not incref'd */


    presult = PyRun_String(key, Py_file_input, dict, dict); /* eval direct */
    if (!presult) {
        throw PyException();
    }
    Py_DECREF(presult);

    return PyObject_GetAttrString(module, result_var);
}

void InterpreterSingleton::dbgObserveFile(const char* sFileName)
{
    if (sFileName)
        _cDebugFileName = sFileName;
    else
        _cDebugFileName = "";
}

void InterpreterSingleton::dbgSetBreakPoint(unsigned int /*uiLineNumber*/)
{

}

void InterpreterSingleton::dbgUnsetBreakPoint(unsigned int /*uiLineNumber*/)
{

}

void InterpreterSingleton::dbgStep(void)
{

}

const std::string InterpreterSingleton::strToPython(const char* Str)
{
    std::string result;
    const char *It=Str;

    while (*It != '\0') {
        switch (*It) {
        case '\\':
            result += "\\\\";
            break;
        case '\"':
            result += "\\\"";
            break;
        case '\'':
            result += "\\\'";
            break;
        default:
            result += *It;
        }
        It++;
    }

    return result;
}

// --------------------------------------------------------------------

int getSWIGVersionFromModule(const std::string& module)
{
    static std::map<std::string, int> moduleMap;
    std::map<std::string, int>::iterator it = moduleMap.find(module);
    if (it != moduleMap.end()) {
        return it->second;
    }
    else {
        try {
            // Get the module and check its __file__ attribute
            Py::Dict dict(PyImport_GetModuleDict());
            if (!dict.hasKey(module))
                return 0;
            Py::Module mod(module);
            Py::String file(mod.getAttr("__file__"));
            std::string filename = (std::string)file;
            // file can have the extension .py or .pyc
            filename = filename.substr(0, filename.rfind('.'));
            filename += ".py";
            boost::regex rx("^# Version ([1-9])\\.([0-9])\\.([0-9]+)");
            boost::cmatch what;

            std::string line;
            Base::FileInfo fi(filename);

            Base::ifstream str(fi, std::ios::in);
            while (str && std::getline(str, line)) {
                if (boost::regex_match(line.c_str(), what, rx)) {
                    int major = std::atoi(what[1].first);
                    int minor = std::atoi(what[2].first);
                    int micro = std::atoi(what[3].first);
                    int version = (major<<16)+(minor<<8)+micro;
                    moduleMap[module] = version;
                    return version;
                }
            }
        }
        catch (Py::Exception& e) {
            e.clear();
        }
    }

#if (defined(HAVE_SWIG) && (HAVE_SWIG == 1))
    moduleMap[module] = 0;
#endif
    return 0;
}

#if (defined(HAVE_SWIG) && (HAVE_SWIG == 1))
namespace Swig_python { extern int createSWIGPointerObj_T(const char* TypeName, void* obj, PyObject** ptr, int own); }
#endif
#if PY_MAJOR_VERSION < 3
namespace Swig_1_3_25 { extern int createSWIGPointerObj_T(const char* TypeName, void* obj, PyObject** ptr, int own); }
namespace Swig_1_3_33 { extern int createSWIGPointerObj_T(const char* TypeName, void* obj, PyObject** ptr, int own); }
namespace Swig_1_3_36 { extern int createSWIGPointerObj_T(const char* TypeName, void* obj, PyObject** ptr, int own); }
namespace Swig_1_3_38 { extern int createSWIGPointerObj_T(const char* TypeName, void* obj, PyObject** ptr, int own); }
namespace Swig_1_3_40 { extern int createSWIGPointerObj_T(const char* TypeName, void* obj, PyObject** ptr, int own); }
#endif

PyObject* InterpreterSingleton::createSWIGPointerObj(const char* Module, const char* TypeName, void* Pointer, int own)
{
    int result = 0;
    PyObject* proxy=0;
    PyGILStateLocker locker;
#if PY_MAJOR_VERSION < 3
    int version = getSWIGVersionFromModule(Module);
    switch (version)
    {
    case 66329:
        result = Swig_1_3_25::createSWIGPointerObj_T(TypeName, Pointer, &proxy, own);
        break;
    case 66337:
        result = Swig_1_3_33::createSWIGPointerObj_T(TypeName, Pointer, &proxy, own);
        break;
    case 66340:
        result = Swig_1_3_36::createSWIGPointerObj_T(TypeName, Pointer, &proxy, own);
        break;
    case 66342:
        result = Swig_1_3_38::createSWIGPointerObj_T(TypeName, Pointer, &proxy, own);
        break;
    case 66344:
        result = Swig_1_3_40::createSWIGPointerObj_T(TypeName, Pointer, &proxy, own);
        break;
    default:
#else
    (void)Module;
#endif
#if (defined(HAVE_SWIG) && (HAVE_SWIG == 1))
    result = Swig_python::createSWIGPointerObj_T(TypeName, Pointer, &proxy, own);
#else
    (void)TypeName;
    (void)Pointer;
    (void)own;
    result = -1; // indicates error
#endif
#if PY_MAJOR_VERSION < 3
    }
#endif

    if (result == 0)
        return proxy;

    // none of the SWIG's succeeded
    throw Base::RuntimeError("No SWIG wrapped library loaded");
}

#if (defined(HAVE_SWIG) && (HAVE_SWIG == 1))
namespace Swig_python { extern int convertSWIGPointerObj_T(const char* TypeName, PyObject* obj, void** ptr, int flags); }
#endif
#if PY_MAJOR_VERSION < 3
namespace Swig_1_3_25 { extern int convertSWIGPointerObj_T(const char* TypeName, PyObject* obj, void** ptr, int flags); }
namespace Swig_1_3_33 { extern int convertSWIGPointerObj_T(const char* TypeName, PyObject* obj, void** ptr, int flags); }
namespace Swig_1_3_36 { extern int convertSWIGPointerObj_T(const char* TypeName, PyObject* obj, void** ptr, int flags); }
namespace Swig_1_3_38 { extern int convertSWIGPointerObj_T(const char* TypeName, PyObject* obj, void** ptr, int flags); }
namespace Swig_1_3_40 { extern int convertSWIGPointerObj_T(const char* TypeName, PyObject* obj, void** ptr, int flags); }
#endif

bool InterpreterSingleton::convertSWIGPointerObj(const char* Module, const char* TypeName, PyObject* obj, void** ptr, int flags)
{
    int result = 0;
    PyGILStateLocker locker;
#if PY_MAJOR_VERSION < 3
    int version = getSWIGVersionFromModule(Module);
    switch (version)
    {
    case 66329:
        result = Swig_1_3_25::convertSWIGPointerObj_T(TypeName, obj, ptr, flags);
        break;
    case 66337:
        result = Swig_1_3_33::convertSWIGPointerObj_T(TypeName, obj, ptr, flags);
        break;
    case 66340:
        result = Swig_1_3_36::convertSWIGPointerObj_T(TypeName, obj, ptr, flags);
        break;
    case 66342:
        result = Swig_1_3_38::convertSWIGPointerObj_T(TypeName, obj, ptr, flags);
        break;
    case 66344:
        result = Swig_1_3_40::convertSWIGPointerObj_T(TypeName, obj, ptr, flags);
        break;
    default:
#else
    (void)Module;
#endif
#if (defined(HAVE_SWIG) && (HAVE_SWIG == 1))
    result = Swig_python::convertSWIGPointerObj_T(TypeName, obj, ptr, flags);
#else
    (void)TypeName;
    (void)obj;
    (void)ptr;
    (void)flags;
    result = -1; // indicates error
#endif
#if PY_MAJOR_VERSION < 3
    }
#endif

    if (result == 0)
        return true;

    // none of the SWIG's succeeded
    throw Base::RuntimeError("No SWIG wrapped library loaded");
}

#if (defined(HAVE_SWIG) && (HAVE_SWIG == 1))
namespace Swig_python { extern void cleanupSWIG_T(const char* TypeName); }
#endif
#if PY_MAJOR_VERSION < 3
namespace Swig_1_3_25 { extern void cleanupSWIG_T(const char* TypeName); }
namespace Swig_1_3_33 { extern void cleanupSWIG_T(const char* TypeName); }
namespace Swig_1_3_36 { extern void cleanupSWIG_T(const char* TypeName); }
namespace Swig_1_3_38 { extern void cleanupSWIG_T(const char* TypeName); }
namespace Swig_1_3_40 { extern void cleanupSWIG_T(const char* TypeName); }
#endif

void InterpreterSingleton::cleanupSWIG(const char* TypeName)
{
    PyGILStateLocker locker;
#if (defined(HAVE_SWIG) && (HAVE_SWIG == 1))
    Swig_python::cleanupSWIG_T(TypeName);
#else
    (void)TypeName;
#endif
#if PY_MAJOR_VERSION < 3
    Swig_1_3_25::cleanupSWIG_T(TypeName);
    Swig_1_3_33::cleanupSWIG_T(TypeName);
    Swig_1_3_36::cleanupSWIG_T(TypeName);
    Swig_1_3_38::cleanupSWIG_T(TypeName);
    Swig_1_3_40::cleanupSWIG_T(TypeName);
#endif
}
