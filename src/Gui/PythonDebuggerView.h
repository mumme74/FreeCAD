/***************************************************************************
 *   Copyright (c) 2017 Fredrik Johansson github.com/mumme74               *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/

#ifndef PYTHONDEBUGGERVIEW_H
#define PYTHONDEBUGGERVIEW_H


#include <CXX/Extensions.hxx>
#include <QAbstractTableModel>
#include "DockWindow.h"
#include "Window.h"
#include <frameobject.h>
#include <QVariant>
#include <App/PythonDebugger.h>
#include <memory>

QT_BEGIN_NAMESPACE
class QVBoxLayout;
class QTreeView;
class QLineEdit;
QT_END_NAMESPACE

namespace  Base {
    class PyException;
    class PyExceptionInfo;
} // namespace Base

namespace App {
class BreakpointLine;
}

namespace Gui {
namespace DockWnd {

class PythonDebuggerViewP;
class VariableModelP;

using BreakpointPtr = std::shared_ptr<App::Debugging::Python::BrkPnt>;
using PyDebugger = App::Debugging::Python::Debugger;

// -------------------------------------------------------------------------------
/**
 * @brief PythonDebugger dockable widgets
 */
class PythonDebuggerView : public QWidget
{
    Q_OBJECT
public:
    PythonDebuggerView(QWidget *parent = 0);
    ~PythonDebuggerView();
    static void setFileAndScrollToLine(const QString &fn, int line);


protected:
    void changeEvent(QEvent *e);

private Q_SLOTS:
    void startDebug();
    void enableButtons();
    void stackViewCurrentChanged(const QModelIndex & current, const QModelIndex & previous);
    void breakpointViewCurrentChanged(const QModelIndex & current, const QModelIndex & previous);
    void issuesViewCurrentChanged(const QModelIndex & current, const QModelIndex & previous);
    void customBreakpointContextMenu(const QPoint &pos);
    void customIssuesContextMenu(const QPoint &pos);
    void saveExpandedVarViewState();
    void restoreExpandedVarViewState();

private:
    void initButtons(QVBoxLayout *vLayout);
    PythonDebuggerViewP *d;
};

// ------------------------------------------------------------------------------

class WatchWindow : public QWidget
{
    Q_OBJECT
public:
    explicit WatchWindow(QWidget *parent);
    virtual ~WatchWindow();

private Q_SLOTS:
    void returnPressed();
    void onCustomContextMenu(const QPoint &point);

private:
    QLineEdit *m_lineEdit;
    QTreeView *m_treeView;
};


//-------------------------------------------------------------------------------

/**
 * @brief data model for stacktrace
 */
class StackFramesModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    StackFramesModel(QObject *parent = 0);
    ~StackFramesModel();

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;
    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const;

public Q_SLOTS:
    void clear();

private Q_SLOTS:
    void updateFrames();

private:
    PyFrameObject *m_currentFrame;
    static const int colCount = 3;
};

// --------------------------------------------------------------------------------
/**
 * @brief Summary view of all breakpoints
 */
class PythonBreakpointModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    PythonBreakpointModel(QObject *parent = 0);
    ~PythonBreakpointModel();

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const;

private Q_SLOTS:
    void added(size_t uniqueId);
    void changed(size_t uniqueId);
    void removed(size_t uniqueId);

private:
    static const int colCount = 2;
};

// --------------------------------------------------------------------------------
class IssuesModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    IssuesModel(QObject *parent = 0);
    ~IssuesModel();

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const;
    bool removeRows(int row, int count, const QModelIndex & parent = QModelIndex());

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;


private Q_SLOTS:
    void exceptionFilter(Base::Exception* exc);
    void exception(Base::PyExceptionInfo* exc);
    void clear();
    void clearException(const QString &fn, int line);

private:
    static const int colCount = 3;
    QList<Base::PyExceptionInfo*> m_exceptions;
};

// -------------------------------------------------------------------------------

class VariableTreeItem
{
public:
    explicit VariableTreeItem(const QList<QVariant> &data,
                     VariableTreeItem *parent);
    ~VariableTreeItem();

    void addChild(VariableTreeItem *child);
    bool removeChild(int row);
    bool removeChildren(int row, int nrRows);

    VariableTreeItem *child(int row);
    VariableTreeItem *childByName(const QString &name);
    int childRowByName(const QString &name);
    int childCount() const;
    int columnCount() const;
    QVariant data(int column) const;
    int childNumber() const;
    VariableTreeItem *parent();

    const QString name() const;
    const QString value() const;
    void setValue(const QString value);
    const QString type() const;
    void setType(const QString type);

    void setLazyLoadChildren(bool lazy);
    bool lazyLoadChildren() const;

    // traverses up the tree and finds it way down again to find it selfs pointer
    // a workaround as python doesent seem to hold on to its pointers variables
    Py::Object getAttr(const QString attrName) const;
    bool hasAttr(const QString attrName) const;

    // should only be set at locals, global, and builtin level at PyFrameLevel
    void setMeAsRoot(Py::Object root);


private:
    QList<VariableTreeItem*> childItems;
    QList<QVariant> itemData;
    VariableTreeItem *parentItem;
    Py::Object rootObj;
    bool lazyLoad;
};

// ---------------------------------------------------------------------------------
class VarTreeModelBase : public QAbstractItemModel
{
    Q_OBJECT
public:
    VarTreeModelBase(QObject *parent = 0);
    virtual ~VarTreeModelBase();

    void scanObject(PyObject *startObject, VariableTreeItem *parentItem,
                    int depth, bool noBuiltins);
    VariableTreeItem *m_rootItem;

    QVariant data(const QModelIndex &index, int role) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const;
    QModelIndex index(int row, int column,
                      const QModelIndex &parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &index) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

    bool addRows(QList<VariableTreeItem*> added,
                    const QModelIndex &parent = QModelIndex());

    bool removeRows(int firstRow, int nrRows,
                     const QModelIndex & parent = QModelIndex());

    bool hasChildren(const QModelIndex & parent = QModelIndex()) const;

    VariableTreeItem *getItem(const QModelIndex &index) const;

public Q_SLOTS:
    virtual void clear() = 0;

protected Q_SLOTS:
    virtual void updateVariables() = 0;
    void lazyLoad(const QModelIndex &parent);
    void lazyLoad(VariableTreeItem *parentItem);
};

// ---------------------------------------------------------------------------------

class VariableTreeModel : public VarTreeModelBase
{
    Q_OBJECT

public:
    VariableTreeModel(QObject *parent = nullptr);
    ~VariableTreeModel() override;

public Q_SLOTS:
    void clear() override;

protected Q_SLOTS:
    void updateVariables() override;

private:
    //void setupModelData(const QStringList &lines, VariableTreeItem *parent);
    VariableTreeItem *m_localsItem;
    VariableTreeItem *m_globalsItem;
    //VariableTreeItem *m_builtinsItem;
};

// ----------------------------------------------------------------------

class WatchModel : public VarTreeModelBase
{
    Q_OBJECT
public:
    WatchModel(QObject *parent = nullptr);
    ~WatchModel();

    void addItem(QString name);
    void removeItem(QString name);

public Q_SLOTS:
    void clear();

protected Q_SLOTS:
    void updateVariables() override;

private:
    QList<VariableTreeItem*> m_items;
};

} // namespace DockWnd
} // namespace Gui

#endif // PYTHONDEBUGGERVIEW_H
