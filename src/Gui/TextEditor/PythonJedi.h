#ifndef PYTHONJEDI_H
#define PYTHONJEDI_H

#include "PreCompiled.h"
#include <Base/Parameter.h>

#include <memory>

#include "Base/Interpreter.h"
#include <frameobject.h> // python
#include <QStringList>
#include <QString>
#include <QObject>
//#include <qglobal.h>

QT_BEGIN_NAMESPACE;
class QEventLoop;
QT_END_NAMESPACE;

namespace Gui {
class JediDefinitionObj;
class JediCompletionObj;
class JediCallSignatureObj;
class JediBaseDefinitionObj;
class JediScriptObj;


class PythonJedi
{
public:
    PythonJedi();
};

//
typedef std::shared_ptr<JediCompletionObj> JediCompletion_ptr_t;
typedef QList<JediCompletion_ptr_t> JediCompletion_list_t;
typedef std::shared_ptr<JediDefinitionObj> JediDefinition_ptr_t;
typedef QList<JediDefinition_ptr_t> JediDefinition_list_t;
typedef std::shared_ptr<JediCallSignatureObj> JediCallSignature_ptr_t;
typedef QList<JediCallSignature_ptr_t> JediCallSignature_list_t;
typedef std::shared_ptr<JediScriptObj> JediScript_ptr_t;
typedef std::shared_ptr<JediBaseDefinitionObj> JediBaseDefinition_ptr_t;
typedef QList<JediBaseDefinition_ptr_t> JediBaseDefinition_list_t;


/**
 * @brief The JediBaseDefinition class
 * wrapper for python class BaseDefinition
 */

class JediBaseDefinitionObj
{
public:
    JediBaseDefinitionObj(Py::Object obj);
    virtual ~JediBaseDefinitionObj();

    enum Types {
        Base, Definition, Completion, CallSignature
    };

    /*
        Name of variable/function/class/module.

        For example, for ``x = None`` it returns ``'x'``.

        :rtype: str or None
        @property
     */
    QString name() const;

    // The type of the definition.
    //     @property
    QString type() const;

    // The module name.
    //     @property
    QString module_name() const;

    QString module_path() const;

    // Whether this is a builtin module.
    bool in_builtin_module() const;

    // The line where the definition occurs (starting with 1).
    //     @property
    int line() const;

    // The column where the definition occurs (starting with 0).
    //     @property
    int column() const;

    // Return a document string for this completion object.
    //
    // Notice that useful extra information is added to the actual
    // docstring.  For function, it is call signature.  If you need
    // actual docstring, use ``raw=True`` instead.

    // :param fast: Don't follow imports that are only one level deep like
    //    ``import foo``, but follow ``from foo import bar``. This makes
    //    sense for speed reasons. Completing `import a` is slow if you use
    //    the ``foo.docstring(fast=False)`` on every object, because it
    //    parses all libraries starting with ``a``.
    QString docstring(bool raw = false, bool fast = true) const;

    // A textual description of the object.
    //     @property
    QString description() const;

    // Dot-separated path of this object.
    //     @property
    QString full_name() const;

    // return a list with assignments
    JediDefinition_list_t goto_assignments() const;

    // return a list with definitions
    JediDefinition_list_t goto_definitions() const;

    // Return the original definitions. use with care see doumentation for jedi
    //     @property
    JediDefinition_list_t params() const;

    // return parent Definition or nullptr   -> in c++ = empty list
    JediDefinition_ptr_t parent() const;

    // Returns the line of code where this object was defined.
    // :param before: Add n lines before the current line to the output.
    // :param after: Add n lines after the current line to the output.
    // :return str: Returns the line(s) of code or an empty string if it's a
    //              builtin.
    int get_line_code(int before = 0, int after = 0) const;


    Types cppType() const { return m_type; }
    bool isValid() const { return m_obj.ptr() != nullptr; }
    const Py::Object pyObj() const { return m_obj; }
    JediDefinitionObj *toDefinitionObj();
    JediCompletionObj *toCompletionObj();
    JediCallSignatureObj *toCallSignatureObj();
    bool isDefinitionObj();
    bool isCompletionObj();
    bool isCallSignatureObj();

protected:

    Py::Object m_obj;
    Types m_type;
};

// ------------------------------------------------------------------------


class JediCompletionObj : public JediBaseDefinitionObj
{
    /*
    """
    `Completion` objects are returned from :meth:`api.Script.completions`. They
    provide additional information about a completion.
    """
     */
public:
    JediCompletionObj(Py::Object obj);
    ~JediCompletionObj();

    //  Return the rest of the word,
    //     @property
    QString complete() const;

    //  Similar to :attr:`name`, but like :attr:`name` returns also the
    //          symbols, for example assuming the following function definition::
    //              def foo(param=0):
    //                    pass
    //          completing ``foo(`` would give a ``Completion`` which
    //          ``name_with_symbols`` would be "param=".
    //     @property
    QString name_with_symbols() const;
};

// ---------------------------------------------------------------------------

class JediDefinitionObj : public JediBaseDefinitionObj
{
    /*
    """
    *Definition* objects are returned from :meth:`api.Script.goto_assignments`
    or :meth:`api.Script.goto_definitions`.
    """
     */
public:
    JediDefinitionObj(Py::Object obj);
    ~JediDefinitionObj();

    /*
        List sub-definitions (e.g., methods in class).
        :rtype: list of Definition
     */
    JediDefinition_list_t defined_names() const;

    /*
        Returns True, if defined as a name in a statement, function or class.
        Returns False, if it's a reference to such a definition.
     */
    bool is_definition() const;
};

// --------------------------------------------------------------------------

class JediCallSignatureObj : public JediDefinitionObj
{
    /*
    `CallSignature` objects is the return value of `Script.function_definition`.
    It knows what functions you are currently in. e.g. `isinstance(` would
    return the `isinstance` function. without `(` it would return nothing.
     */
public:
    JediCallSignatureObj(Py::Object obj);
    ~JediCallSignatureObj();

    /*
        The Param index of the current call.
        Returns nullptr if the index cannot be found in the curent call.
            @property
     */
    JediDefinition_ptr_t index() const;

    /*
        The indent of the bracket that is responsible for the last function
        call.
            @property
     */
    int bracket_start() const;
};

// --------------------------------------------------------------------------

/**
 * @brief The JediScriptObj class is used for code completion, and analysis
 * Like painting known variables and find usages
 * Uses the python library Jedi, intall via 'pip install jedi'
 */
class JediCommonP;
class JediScriptObj
{
    Py::Object m_obj;

public:
    /*
     *     :param source: The source code of the current file, separated by newlines.
    :type source: str
    :param line: The line to perform actions on (starting with 1).
    :type line: int
    :param column: The column of the cursor (starting with 0).
    :type column: int
    :param path: The path of the file in the file system, or ``''`` if
        it hasn't been saved yet.
    :type path: str or None
    :param encoding: The encoding of ``source``, if it is not a
        ``unicode`` object (default ``'utf-8'``).
    :type encoding: str
    :param sys_path: ``sys.path`` to use during analysis of the script
    :type sys_path: list
     */
    // source=None, line=None, column=None, path=None, encoding='utf-8'
    JediScriptObj(const QString source = QString(), int line = -1, int column = -1,
                  const QString path = QString(),
                  const QString encoding = QLatin1String("utf-8"));
    virtual ~JediScriptObj();

    /*
        Return :class:`classes.Completion` objects. Those objects contain
        information about the completions, more than just names.

        :return: Completion objects, sorted by name and __ comes last.
        :rtype: list of :class:`classes.Completion`
     */
    JediCompletion_list_t completions() const;

    /*
        Return the definitions of a the path under the cursor.  goto function!
        This follows complicated paths and returns the end, not the first
        definition. The big difference between :meth:`goto_assignments` and
        :meth:`goto_definitions` is that :meth:`goto_assignments` doesn't
        follow imports and statements. Multiple objects may be returned,
        because Python itself is a dynamic language, which means depending on
        an option you can have two different versions of a function.

        :rtype: list of :class:`classes.Definition`
     */
    JediDefinition_list_t goto_definitions() const;

    /*
        Return the first definition found, while optionally following imports.
        Multiple objects may be returned, because Python itself is a
        dynamic language, which means depending on an option you can have two
        different versions of a function.

        :rtype: list of :class:`classes.Definition`
     */
    JediDefinition_list_t goto_assignments(bool follow_imports = false) const;

    /*
        Return :class:`classes.Definition` objects, which contain all
        names that point to the definition of the name under the cursor. This
        is very useful for refactoring (renaming), or to show all usages of a
        variable.

        .. todo:: Implement additional_module_paths

        :rtype: list of :class:`classes.Definition`
     */
    JediDefinition_list_t usages(QStringList additional_module_paths = QStringList()) const;

    /*
        Return the function object of the call you're currently in.

        E.g. if the cursor is here::

            abs(# <-- cursor is here

        This would return the ``abs`` function. On the other hand::

            abs()# <-- cursor is here

        This would return an empty list..

        :rtype: list of :class:`classes.CallSignature`
     */
    JediCallSignature_list_t call_signatures() const;


    bool isValid() const;
};

// -----------------------------------------------------------------------

// a callback module that proxies jedi debug info to JediInterpreter
extern "C" {
class JediDebugProxy : public Py::ExtensionModule<JediDebugProxy>
{
public:
    JediDebugProxy();

    virtual ~JediDebugProxy();
    static const char *ModuleName;
private:
    Py::Object proxy(const Py::Tuple &args);
};
}

// -----------------------------------------------------------------------

/**
 * @brief The JediInterpreter class
 * we need a separate interpreter to isolate
 * code analysis from freecad built in interpreter
 */
class JediInterpreter : public QObject
{
    Q_OBJECT
public:
    JediInterpreter();
    ~JediInterpreter();

    // scope guard to do work with this interpreter
    class SwapIn {
        PyThreadState *m_oldState;
        PyGILState_STATE m_GILState;
        static bool static_GILHeld;
    public:
        SwapIn();
        ~SwapIn();
    };

    static JediInterpreter *instance();
    static int haltMainInterpreter(PyObject *obj, PyFrameObject *frame,
                                   int what, PyObject *arg);

    bool isValid() const;

    /**
     * @brief createScriptObj
     * @return a new
     */
    Py::Object runString(const QString &src);

    Py::Module &jedi() const;
    Py::Module &api() const;
    PyThreadState *threadState() const;

    /*

        Returns a list of `Definition` objects, containing name parts.
        This means you can call ``Definition.goto_assignments()`` and get the
        reference of a name.
        The parameters are the same as in :py:class:`Script`, except or the
        following ones:

        :param all_scopes: If True lists the names of all scopes instead of only
            the module namespace.
        :param definitions: If True lists the names that have been defined by a
            class, function or a statement (``a = b`` returns ``a``).
        :param references: If True lists all the names that are not listed by
            ``definitions=True``. E.g. ``a = b`` returns ``b``.
    */
    JediDefinition_list_t  names(const QString source = QString(), const QString path = QString(),
                                 const QString encoding = QLatin1String("utf-8"), bool all_scopes = false,
                                 bool definitions = true, bool references = true) const;

    /*
    Preloading modules tells Jedi to load a module now, instead of lazy parsing
    of modules. Usful for IDEs, to control which modules to load on startup.

    :param modules: different module names, list of string.
     */
    bool preload_module(const QStringList modules) const;

    /*
     * Turn on Jedi set_debug_function, although it transforms those functions calls
     * to a Qt signal
     */
    bool setDebugging(bool on, bool warnings = true,
                            bool notices = true, bool speed = true) const;


Q_SIGNALS:
    void onDebugInfo(const QString &color, const QString &str);


private:
    void destroy();
    PyThreadState *m_threadState;
    PyThreadState *m_interpState;
    Py::Module *m_jedi;
    Py::Module *m_api;
    static JediInterpreter *m_instance;
    static QEventLoop *m_evtLoop;

    // so the proxy can emit as if originating from this class
    friend class JediDebugProxy;
};

} // namespace Gui
#endif // PYTHONJEDI_H
