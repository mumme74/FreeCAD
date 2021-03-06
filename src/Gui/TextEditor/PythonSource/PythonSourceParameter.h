#ifndef PYTHONSOURCEPARAMETER_H
#define PYTHONSOURCEPARAMETER_H

#include "PythonSource.h"
#include "PythonSourceRoot.h"
#include "PythonSourceItemBase.h"

namespace Python {

class SourceFrame;
class SourceIdentifierAssignment;
class SourceParameterList;

// class for function/method parameters ie: def func(arg1, arg2=None, *arg3, **arg4)
//                                                   ^     ^           ^       ^
class SourceParameter : public Python::TokenWrapperInherit
{
public:
    /// type for this parameter, ie (arg1, arg2=0, *arg3, **arg4)
    ///  translates to      (Positional, PositionalDefault, Variable, Keyword)
    enum ParameterType {
        InValid,
        Positional,
        PositionalDefault,
        Variable,
        Keyword
    };
    explicit SourceParameter(Python::SourceFrame *parent,
                             Python::Token *tok);
    virtual ~SourceParameter() override;

    /// get the frame of this parameter
    const Python::SourceFrame *frame() const { return m_frame; }

    /// get the identifierAssignment of this argument
    Python::SourceIdentifierAssignment *identifierAssignment() const;

    /// type for this parameter, ie (arg1, arg2=0, *arg3, **arg4)
    ///  translates to      (Positional, PositionalDefault, Variable, Keyword)
    ParameterType parameterType() const { return m_paramType; }
    void setParameterType(ParameterType paramType) { m_paramType = paramType; }

    const Python::SourceRoot::TypeInfo type() const { return m_type; }
    void setType(Python::SourceRoot::TypeInfo &type) { m_type = type; } // type is implicitly copied

    virtual void tokenDeletedCallback() override;

private:
    Python::SourceRoot::TypeInfo m_type;
    ParameterType m_paramType;
    SourceFrame *m_frame;
};

// ----------------------------------------------------------------------
/*
class SourceParameterList : public Python::SourceListParentBase
{
    Python::SourceFrame *m_frame;
public:
    explicit SourceParameterList(Python::SourceFrame *frame);
    ~SourceParameterList();

    /// get the frame contained for these collections
    const Python::SourceFrame *frame() const { return m_frame; }
    /// get the parameter with name or nullptr if not contained
    const Python::SourceParameter *getParameter(int hash) const;
    const Python::SourceParameter *getParameterByName(const std::string &name) const {
        return getParameter(Python::strToHash(name));
    }
    bool hasParameterByName(const std::string &name) const {
        return getParameterByName(name) != nullptr;
    }
    /// updates param type and or creates parameter if not exists
    Python::SourceParameter *setParameter(Python::Token *tok,
                                         Python::SourceRoot::TypeInfo typeInfo,
                                         Python::SourceParameter::ParameterType paramType);

protected:
    int compare(const Python::SourceListNodeBase *left,
                const Python::SourceListNodeBase *right) const;
};
*/

} // namespace Python

#endif // PYTHONSOURCEPARAMETER_H
