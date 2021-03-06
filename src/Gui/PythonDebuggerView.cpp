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


#include "PreCompiled.h"
#ifndef _PreComp_
# include <QGridLayout>
# include <QApplication>
# include <QMenu>
# include <QContextMenuEvent>
# include <QTextCursor>
# include <QTextStream>
#endif

#include "PythonDebuggerView.h"
#include "BitmapFactory.h"
#include "Window.h"
#include "MainWindow.h"
#include "Macro.h"


#include <App/PythonDebugger.h>
#include <TextEditor/EditorView.h>
#include <TextEditor/PythonEditor.h>
#include <CXX/Extensions.hxx>
#include <frameobject.h>
#include <Base/Interpreter.h>
#include <Application.h>

#include <QVariant>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QPushButton>
#include <QTableView>
#include <QTreeView>
#include <QTabWidget>
#include <QHeaderView>
#include <QFileDialog>
#include <QSplitter>
#include <QMessageBox>
#include <QMenu>
#include <QDebug>



/* TRANSLATOR Gui::DockWnd::PythonDebuggerView */

static ParameterGrp::handle _GetParam() {
    static ParameterGrp::handle hGrp;
    if(!hGrp) {
        hGrp = App::GetApplication().GetParameterGroupByPath(
                "User parameter:BaseApp/Preferences/PythonDebuggerView");
    }
    return hGrp;
}


namespace Gui {
namespace DockWnd {


class PythonDebuggerViewP
{
public:
    QPushButton *m_startDebugBtn;
    QPushButton *m_stopDebugBtn;
    QPushButton *m_stepIntoBtn;
    QPushButton *m_stepOverBtn;
    QPushButton *m_stepOutBtn;
    QPushButton *m_haltOnNextBtn;
    QPushButton *m_continueBtn;
    QLabel      *m_varLabel;
    QTreeView   *m_varView;
    QTableView  *m_stackView;
    QTableView  *m_breakpointView;
    QTableView  *m_issuesView;
    QTabWidget  *m_stackTabWgt;
    QTabWidget  *m_varTabWgt;
    QSplitter   *m_splitter;
    PythonDebuggerViewP() { }
    ~PythonDebuggerViewP() { }
};

} // namespace DockWnd
} //namespace Gui

using namespace Gui;
using namespace Gui::DockWnd;

/**
 *  Constructs a PythonDebuggerView which is a child of 'parent', with the
 *  name 'name' and widget flags set to 'f'
 */
PythonDebuggerView::PythonDebuggerView(QWidget *parent)
   : QWidget(parent), d(new PythonDebuggerViewP())
{
    setObjectName(QLatin1String("DebuggerView"));

    QVBoxLayout *vLayout = new QVBoxLayout(this);

    initButtons(vLayout);

    d->m_splitter = new QSplitter(this);
    d->m_splitter->setOrientation(Qt::Vertical);
    vLayout->addWidget(d->m_splitter);

    // variables explorer view
    d->m_varTabWgt = new QTabWidget(d->m_splitter);
    d->m_splitter->addWidget(d->m_varTabWgt);

    d->m_varView = new QTreeView(this);
    d->m_varTabWgt->addTab(d->m_varView, tr("Variables"));
    VariableTreeModel *varModel = new VariableTreeModel(this);
    d->m_varView->setModel(varModel);
    d->m_varView->setIndentation(10);

    WatchWindow *watch = new WatchWindow(this);
    d->m_varTabWgt->addTab(watch, tr("Watch window"));

    connect(d->m_varView, SIGNAL(expanded(const QModelIndex)),
            varModel, SLOT(lazyLoad(const QModelIndex)));


    // stack and breakpoints tabwidget
    d->m_stackTabWgt = new QTabWidget(d->m_splitter);
    d->m_splitter->addWidget(d->m_stackTabWgt);

    // stack view
    d->m_stackView = new QTableView(this);
    StackFramesModel *stackModel = new StackFramesModel(this);
    d->m_stackView->setModel(stackModel);
    d->m_stackView->verticalHeader()->hide();
    d->m_stackView->setShowGrid(false);
    d->m_stackView->setTextElideMode(Qt::ElideLeft);
    d->m_stackView->setSelectionBehavior(QAbstractItemView::SelectRows);
    d->m_stackTabWgt->addTab(d->m_stackView, tr("Stack"));

    connect(d->m_stackView->selectionModel(), SIGNAL(currentChanged(const QModelIndex &, const QModelIndex&)),
            this, SLOT(stackViewCurrentChanged(const QModelIndex&, const QModelIndex&)));

    // breakpoints view
    d->m_breakpointView = new QTableView(this);
    PythonBreakpointModel *bpModel = new PythonBreakpointModel(this);
    d->m_breakpointView->setModel(bpModel);
    d->m_breakpointView->verticalHeader()->hide();
    d->m_breakpointView->setShowGrid(false);
    d->m_breakpointView->setTextElideMode(Qt::ElideLeft);
    d->m_breakpointView->setSelectionBehavior(QAbstractItemView::SelectRows);
    d->m_breakpointView->setContextMenuPolicy(Qt::CustomContextMenu);
    d->m_stackTabWgt->addTab(d->m_breakpointView, tr("Breakpoints"));

    connect(d->m_breakpointView, SIGNAL(customContextMenuRequested(const QPoint&)),
            this, SLOT(customBreakpointContextMenu(const QPoint&)));
    connect(d->m_breakpointView->selectionModel(), SIGNAL(currentChanged(const QModelIndex &, const QModelIndex&)),
            this, SLOT(breakpointViewCurrentChanged(const QModelIndex&, const QModelIndex&)));

    // issues view
    d->m_issuesView = new QTableView(this);
    IssuesModel *issueModel = new IssuesModel(this);
    d->m_issuesView->setModel(issueModel);
    d->m_issuesView->verticalHeader()->hide();
    d->m_issuesView->setShowGrid(false);
    d->m_issuesView->setTextElideMode(Qt::ElideLeft);
    d->m_issuesView->setSelectionBehavior(QAbstractItemView::SelectRows);
    d->m_issuesView->setContextMenuPolicy(Qt::CustomContextMenu);
    d->m_issuesView->setStyleSheet(QLatin1String("QToolTip {font-size:12pt; font-family:'DejaVu Sans Mono', Courier; }"));
    d->m_stackTabWgt->addTab(d->m_issuesView, tr("Issues"));

    connect(d->m_issuesView->selectionModel(), SIGNAL(currentChanged(const QModelIndex &, const QModelIndex&)),
            this, SLOT(issuesViewCurrentChanged(const QModelIndex&, const QModelIndex&)));
    connect(d->m_issuesView, &QTableView::customContextMenuRequested,
            this, &PythonDebuggerView::customIssuesContextMenu);
    connect(issueModel, &IssuesModel::rowsInserted, [&](const QModelIndex&, int, int last){
        d->m_stackTabWgt->setCurrentWidget(d->m_issuesView);
        auto line = d->m_issuesView->model()->index(last, 0).data().toInt();
        auto file = d->m_issuesView->model()->index(last, 1).data().toString();
        PyDebugger::instance()->setActiveLine(file, line);
    });

    setLayout(vLayout);

    // raise the tab page set in the preferences
    ParameterGrp::handle hGrp = _GetParam()->GetGroup("DebugView");
    d->m_stackTabWgt->setCurrentIndex(static_cast<int>(hGrp->GetInt("AutoloadStackViewTab", 0)));
    d->m_varTabWgt->setCurrentIndex(static_cast<int>(hGrp->GetInt("AutoloadVarViewTab", 0)));


    // restore header data such column width etc
    std::string state = hGrp->GetASCII("BreakpointHeaderState", "");
    d->m_breakpointView->horizontalHeader()->restoreState(QByteArray::fromBase64(state.c_str()));

    // restore header data such column width etc
    state = hGrp->GetASCII("StackHeaderState", "");
    d->m_stackView->horizontalHeader()->restoreState(QByteArray::fromBase64(state.c_str()));

    // restore header data such column width etc
    state = hGrp->GetASCII("VarViewHeaderState", "");
    d->m_varView->header()->restoreState(QByteArray::fromBase64(state.c_str()));

    // restore header data such column width etc
    state = hGrp->GetASCII("IssuesHeaderState", "");
    d->m_issuesView->horizontalHeader()->restoreState(QByteArray::fromBase64(state.c_str()));

    // splitter setting
    state = hGrp->GetASCII("SplitterState", "");
    d->m_splitter->restoreState(QByteArray::fromBase64(state.c_str()));
}

PythonDebuggerView::~PythonDebuggerView()
{
    // save currently viewed tab
    ParameterGrp::handle hGrp = _GetParam()->GetGroup("DebugView");
    hGrp->SetInt("AutoloadStackViewTab", d->m_stackTabWgt->currentIndex());
    hGrp->SetInt("AutoloadVarViewTab", d->m_varTabWgt->currentIndex());

    // save column width for breakpointView
    QByteArray state = d->m_breakpointView->horizontalHeader()->saveState();
    hGrp->SetASCII("BreakpointHeaderState", state.toBase64().data());

    // save column width for stackView
    state = d->m_stackView->horizontalHeader()->saveState();
    hGrp->SetASCII("StackHeaderState", state.toBase64().data());

    // save column width for varView
    state = d->m_varView->header()->saveState();
    hGrp->SetASCII("VarViewHeaderState", state.toBase64().data());

    // save column width for stackView
    state = d->m_issuesView->horizontalHeader()->saveState();
    hGrp->SetASCII("IssuesHeaderState", state.toBase64().data());

    // splitter setting
    state = d->m_splitter->saveState();
    hGrp->SetASCII("SplitterState", state.toBase64().data());

    delete d;
}

void PythonDebuggerView::changeEvent(QEvent *e)
{
    QWidget::changeEvent(e);
    if (e->type() == QEvent::LanguageChange) {
        d->m_stackTabWgt->setTabText(0, tr("Stack"));
        d->m_stackTabWgt->setTabText(1, tr("Breakpoints"));
        d->m_stackTabWgt->setTabText(2, tr("Issues"));
        d->m_varTabWgt->setTabText(0, tr("Variables"));
    }
}

void PythonDebuggerView::startDebug()
{

    auto view = EditorViewSingleton::instance()->activeView();
    if (!view)
        return;

    if (view->editor()) {
        QFileInfo fi(view->filename());
        if (fi.suffix().toLower() != QLatin1String("py") &&
            fi.suffix().toLower() != QLatin1String("fcmacro"))
        {
            QMessageBox::information(this, tr("Not a Python file!"),
                                     tr("Active editor does not seem to have a python filename"));
            return;
        }

    }

    view->setFocus();
    view->save();

    auto debugger = App::Debugging::Python::Debugger::instance();
    debugger->stop();

    if (debugger->start())
        debugger->runFile(view->filename());
}

void PythonDebuggerView::enableButtons()
{
    auto debugger = PyDebugger::instance();
    bool running = debugger->isRunning();
    bool halted = debugger->isHalted();
    d->m_startDebugBtn->setEnabled(!running && !halted);
    d->m_continueBtn->setEnabled(running || halted);
    d->m_stopDebugBtn->setEnabled(running || halted);
    d->m_stepIntoBtn->setEnabled(halted);
    d->m_stepOverBtn->setEnabled(halted);
    d->m_stepOutBtn->setEnabled(halted);
    d->m_haltOnNextBtn->setEnabled(running && !debugger->isHaltOnNext() && !halted);
}

void PythonDebuggerView::stackViewCurrentChanged(const QModelIndex &current,
                                                 const QModelIndex &previous)
{
    Q_UNUSED(previous)

    QAbstractItemModel *viewModel = d->m_stackView->model();
    QModelIndex idxLeft = viewModel->index(current.row(), 0, QModelIndex());
    QModelIndex idxRight = viewModel->index(current.row(),
                                            viewModel->columnCount(),
                                            QModelIndex());

    QItemSelection sel(idxLeft, idxRight);
    QItemSelectionModel *selModel = d->m_stackView->selectionModel();
    selModel->select(sel, QItemSelectionModel::Rows);

    QModelIndex stackIdx = current.sibling(current.row(), 0);
    StackFramesModel *model = reinterpret_cast<StackFramesModel*>(d->m_stackView->model());
    if (!model)
        return;
    QVariant idx = model->data(stackIdx, Qt::DisplayRole);

    auto debugger = App::Debugging::Python::Debugger::instance();
    debugger->setStackLevel(idx.toInt());
}

void PythonDebuggerView::breakpointViewCurrentChanged(const QModelIndex &current,
                                                      const QModelIndex &previous)
{
    Q_UNUSED(previous)
    auto debugger = App::Debugging::Python::Debugger::instance();
    auto bp = debugger->getBreakpointFromIdx(current.row());
    if (!bp)
        return;

    setFileAndScrollToLine(bp->bpFile()->fileName(), bp->lineNr());
}

void PythonDebuggerView::issuesViewCurrentChanged(const QModelIndex &current, const QModelIndex &previous)
{
    Q_UNUSED(previous)
    QAbstractItemModel *model = d->m_issuesView->model();
    QModelIndex idxLine = model->index(current.row(), 0);
    QModelIndex idxFile = model->index(current.row(), 1);
    QVariant vLine = model->data(idxLine);
    QVariant vFile = model->data(idxFile);
    if (vLine.isValid() && vFile.isValid())
        setFileAndScrollToLine(vFile.toString(), vLine.toInt());
}

void PythonDebuggerView::customBreakpointContextMenu(const QPoint &pos)
{
    QModelIndex currentItem = d->m_breakpointView->indexAt(pos);
    if (!currentItem.isValid())
        return;

    auto debugger = App::Debugging::Python::Debugger::instance();
    auto bpl = debugger->getBreakpointFromIdx(currentItem.row());
    if (!bpl)
        return;

    QMenu menu;
    QAction disable(BitmapFactory().iconFromTheme("breakpoint-disabled"),
                    tr("Disable breakpoint"), &menu);
    QAction enable(BitmapFactory().iconFromTheme("breakpoint"),
                   tr("Enable breakpoint"), &menu);

    if (bpl->disabled())
        menu.addAction(&enable);
    else
        menu.addAction(&disable);

    QAction edit(BitmapFactory().iconFromTheme("preferences-general"),
                    tr("Edit breakpoint"), &menu);
    menu.addAction(&edit);
    QAction del(BitmapFactory().iconFromTheme("delete"),
                   tr("Delete breakpoint"), &menu);
    menu.addAction(&del);
    menu.addSeparator();
    QAction clear(BitmapFactory().iconFromTheme("process-stop"),
                  tr("Clear all breakpoints"), &menu);
    menu.addAction(&clear);

    QAction showLine(BitmapFactory().iconFromTheme("document-python.svg"),
                  tr("Show line"), &menu);
    menu.addSeparator();
    menu.addAction(&showLine);

    QAction *res = menu.exec(d->m_breakpointView->mapToGlobal(pos));
    if (res == &disable) {
        bpl->setDisabled(true);
    } else if(res == &enable) {
        bpl->setDisabled(false);
    } else if (res == &edit) {
        PythonEditorBreakpointDlg dlg(this, bpl);
        dlg.exec();
    } else if (res == &del) {
        debugger->removeBreakpoint(bpl);
    } else if (res == &clear) {
        debugger->clearAllBreakPoints();
    } else if (res == &showLine) {
        EditorViewSingleton::instance()->activeView()->
                open(bpl->bpFile()->fileName());
    }
}

void PythonDebuggerView::customIssuesContextMenu(const QPoint &pos)
{
    QAbstractItemModel *model = d->m_issuesView->model();
    if (model->rowCount() < 1)
        return;

    int line = 0;
    QMenu menu;
    QAction *clearCurrent = nullptr;
    QModelIndex currentItem = d->m_issuesView->indexAt(pos);
    QString fn;
    if (currentItem.isValid()) {
        fn = model->data(currentItem.sibling(currentItem.row(), 1),
                         Qt::DisplayRole).toString();
        line = model->data(currentItem.sibling(currentItem.row(), 0),
                         Qt::DisplayRole).toInt();
        QVariant vIcon = model->data(currentItem.sibling(currentItem.row(), 0),
                                     Qt::DecorationRole);

        if (vIcon.isValid())
            clearCurrent = new QAction(vIcon.value<QIcon>(), tr("Clear issue"), &menu);
        else
            clearCurrent = new QAction(tr("Clear issue"), &menu);
        menu.addAction(clearCurrent);
    }

    QAction clearAll(BitmapFactory().iconFromTheme("process-stop"),
                          tr("Clear all Issues"), &menu);
    menu.addAction(&clearAll);

    // run context menu
    QAction *res = menu.exec(d->m_issuesView->mapToGlobal(pos));

    auto debugger = App::Debugging::Python::Debugger::instance();
    // rely on signals between our model and PythonDebugger
    if (res == clearCurrent)
        debugger->sendClearException(fn, line);
    else
        debugger->sendClearAllExceptions();

    if (clearCurrent)
        delete clearCurrent;
}

void PythonDebuggerView::saveExpandedVarViewState()
{

}

void PythonDebuggerView::restoreExpandedVarViewState()
{

}

void PythonDebuggerView::initButtons(QVBoxLayout *vLayout)
{
    // debug buttons
    d->m_startDebugBtn = new QPushButton(this);
    d->m_continueBtn   = new QPushButton(this);
    d->m_stopDebugBtn  = new QPushButton(this);
    d->m_stepIntoBtn   = new QPushButton(this);
    d->m_stepOverBtn   = new QPushButton(this);
    d->m_stepOutBtn    = new QPushButton(this);
    d->m_haltOnNextBtn = new QPushButton(this);

    QHBoxLayout *btnLayout = new QHBoxLayout;
    btnLayout->addWidget(d->m_startDebugBtn);
    btnLayout->addWidget(d->m_stopDebugBtn);
    btnLayout->addWidget(d->m_haltOnNextBtn);
    btnLayout->addWidget(d->m_continueBtn);
    btnLayout->addWidget(d->m_stepIntoBtn);
    btnLayout->addWidget(d->m_stepOverBtn);
    btnLayout->addWidget(d->m_stepOutBtn);
    vLayout->addLayout(btnLayout);

    d->m_startDebugBtn->setIcon(BitmapFactory().iconFromTheme("debug-start").pixmap(16,16));
    d->m_continueBtn->setIcon(BitmapFactory().iconFromTheme("debug-continue").pixmap(16,16));
    d->m_stopDebugBtn->setIcon(BitmapFactory().iconFromTheme("debug-stop").pixmap(16,16));
    d->m_stepOverBtn->setIcon(BitmapFactory().iconFromTheme("debug-step-over").pixmap(16,16));
    d->m_stepOutBtn->setIcon(BitmapFactory().iconFromTheme("debug-step-out").pixmap(16,16));
    d->m_stepIntoBtn->setIcon(BitmapFactory().iconFromTheme("debug-step-into").pixmap(16,16));
    d->m_haltOnNextBtn->setIcon(BitmapFactory().iconFromTheme("debug-halt").pixmap(16,16));
    d->m_startDebugBtn->setToolTip(tr("Start debugging"));
    d->m_stopDebugBtn->setToolTip(tr("Stop debugger"));
    d->m_continueBtn->setToolTip(tr("Continue running"));
    d->m_stepIntoBtn->setToolTip(tr("Next instruction, steps into functions"));
    d->m_stepOverBtn->setToolTip(tr("Next instruction, don't step into functions"));
    d->m_stepOutBtn->setToolTip(tr("Continue until current function ends"));
    d->m_haltOnNextBtn->setToolTip(tr("Halt on any python code"));
    d->m_continueBtn->setAutoRepeat(true);
    d->m_stepIntoBtn->setAutoRepeat(true);
    d->m_stepOverBtn->setAutoRepeat(true);
    d->m_stepOutBtn->setAutoRepeat(true);
    enableButtons();

    auto debugger = App::Debugging::Python::Debugger::instance();

    connect(d->m_startDebugBtn, &QPushButton::clicked,
            this, &PythonDebuggerView::startDebug);
    connect(d->m_stopDebugBtn, &QPushButton::clicked,
            debugger, &PyDebugger::stop);
    connect(d->m_haltOnNextBtn, &QPushButton::clicked,
            debugger, &PyDebugger::haltOnNext);
    connect(d->m_stepIntoBtn, &QPushButton::clicked,
            debugger, &PyDebugger::stepInto);
    connect(d->m_stepOverBtn, &QPushButton::clicked,
            debugger, &PyDebugger::stepOver);
    connect(d->m_stepOutBtn, &QPushButton::clicked,
            debugger, &PyDebugger::stepOut);
    connect(d->m_continueBtn, &QPushButton::clicked,
            debugger, &PyDebugger::stepContinue);
    connect(debugger, SIGNAL(haltAt(QString,int)), this, SLOT(enableButtons()));
    connect(debugger, SIGNAL(releaseAt(QString,int)), this, SLOT(enableButtons()));
    connect(debugger, &PyDebugger::started,
            this, &PythonDebuggerView::enableButtons);
    connect(debugger, &PyDebugger::stopped,
            this, &PythonDebuggerView::enableButtons);

}

/* static */
void PythonDebuggerView::setFileAndScrollToLine(const QString &fn, int line)
{
    // switch file in editor so we can view line
    auto editView = EditorViewSingleton::instance()->activeView();
    editView->open(fn);

    // scroll to view
    QTextCursor cursor(editView->editor()->document()->
                       findBlockByLineNumber(line - 1)); // ln-1 because line number starts from 0
    editView->editor()->setTextCursor(cursor);
}

// ---------------------------------------------------------------------------

WatchWindow::WatchWindow(QWidget *parent):
    QWidget(parent),
    m_lineEdit(new QLineEdit(this)),
    m_treeView(new QTreeView(this))
{
    QVBoxLayout *layout = new QVBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_treeView);
    layout->addWidget(m_lineEdit);
    setLayout(layout);

    WatchModel *wModel = new WatchModel(this);
    m_treeView->setModel(wModel);

    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_treeView, SIGNAL(customContextMenuRequested(const QPoint &)),
            this, SLOT(onCustomContextMenu(const QPoint &)));


    connect(m_lineEdit, SIGNAL(returnPressed()), this, SLOT(returnPressed()));
}

WatchWindow::~WatchWindow()
{
}

void WatchWindow::returnPressed()
{
    if (!m_lineEdit->text().isEmpty()) {
        WatchModel *wModel = qobject_cast<WatchModel*>(m_treeView->model());
        if (wModel) {
            wModel->addItem(m_lineEdit->text().trimmed());
        }
    }

    m_lineEdit->clear();
}

void WatchWindow::onCustomContextMenu(const QPoint &point)
{
    QModelIndex index = m_treeView->indexAt(point);
    if (index.isValid()) {
        WatchModel *mdl = qobject_cast<WatchModel*>(m_treeView->model());
        if (!mdl)
            return;
        QString name = mdl->data(index, Qt::DisplayRole).toString();
        QMenu menu;
        QAction remove(tr("Remove '%1'").arg(name));
        menu.addAction(&remove);

        QAction *res = menu.exec(m_treeView->viewport()->mapToGlobal(point));
        if (res == &remove)
            mdl->removeItem(name);
    }
}

// ---------------------------------------------------------------------------

StackFramesModel::StackFramesModel(QObject *parent) :
    QAbstractTableModel(parent),
    m_currentFrame(nullptr)
{
    auto debugger = App::Debugging::Python::Debugger::instance();

    connect(debugger, &PyDebugger::functionCalled,
               this, &StackFramesModel::updateFrames);
    connect(debugger, &PyDebugger::functionExited,
               this, &StackFramesModel::updateFrames);
    connect(debugger, &PyDebugger::stopped, this, &StackFramesModel::clear);
}

StackFramesModel::~StackFramesModel()
{
}

int StackFramesModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    auto debugger = App::Debugging::Python::Debugger::instance();
    return debugger->callDepth(m_currentFrame);
}

int StackFramesModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return colCount +1;
}

QVariant StackFramesModel::data(const QModelIndex &index, int role) const
{
    if (m_currentFrame == nullptr)
        return QVariant();

    if (!index.isValid() || (role != Qt::DisplayRole && role != Qt::ToolTipRole))
        return QVariant();

    if (index.column() > colCount)
        return QVariant();

    auto debugger = App::Debugging::Python::Debugger::instance();

    Base::PyGILStateLocker locker;
    PyFrameObject *frame = m_currentFrame;

    int i = 0,
        j = debugger->callDepth(m_currentFrame);

    while (nullptr != frame) {
        if (i == index.row()) {
            switch(index.column()) {
            case 0:
                return QString::number(j);
            case 1: { // function
#if PY_MAJOR_VERSION >= 3
                const char *funcname = PyUnicode_AsUTF8(frame->f_code->co_name);
#else
                const char *funcname = PyBytes_AsString(frame->f_code->co_name);
#endif
                return QString(QLatin1String(funcname));
            }
            case 2: {// file
#if PY_MAJOR_VERSION >= 3
                const char *filename = PyUnicode_AsUTF8(frame->f_code->co_filename);
#else
                const char *filename = PyBytes_AsString(frame->f_code->co_filename);
#endif
                return QString(QLatin1String(filename));
            }
            case 3: {// line
                int line = PyCode_Addr2Line(frame->f_code, frame->f_lasti);
                return QString::number(line);
            }
            default:
                return QVariant();
            }
        }

        frame = frame->f_back;
        ++i;
        --j; // need to reversed, display current on top
    }


    return QVariant();
}

QVariant StackFramesModel::headerData(int section,
                                      Qt::Orientation orientation,
                                      int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();

    switch (section) {
    case 0:
        return QString(tr("Level"));
    case 1:
        return QString(tr("Function"));
    case 2:
        return QString(tr("File"));
    case 3:
        return QString(tr("Line"));
    default:
        return QVariant();
    }
}

void StackFramesModel::clear()
{
    int i = 0;
    Base::PyGILStateLocker locker;
    Py_XDECREF(m_currentFrame);
    while (nullptr != m_currentFrame) {
        m_currentFrame = m_currentFrame->f_back;
        ++i;
    }

    beginRemoveRows(QModelIndex(), 0, i);
    endRemoveRows();
}

void StackFramesModel::updateFrames()
{
    Base::PyGILStateLocker locker;
    PyFrameObject* frame = PyDebugger::instance()->currentFrame();
    if (m_currentFrame != frame) {
        clear();
        m_currentFrame = frame;
        Py_XINCREF(frame);

        beginInsertRows(QModelIndex(), 0, rowCount()-1);
        endInsertRows();
    }
}

// --------------------------------------------------------------------


PythonBreakpointModel::PythonBreakpointModel(QObject *parent) :
    QAbstractTableModel(parent)
{
    auto debugger = PyDebugger::instance();
    connect(debugger, &PyDebugger::breakpointAdded,
            this, &PythonBreakpointModel::added);
    connect(debugger, &PyDebugger::breakpointChanged,
            this, &PythonBreakpointModel::changed);
    connect(debugger, &PyDebugger::breakpointRemoved,
            this, &PythonBreakpointModel::removed);
}

PythonBreakpointModel::~PythonBreakpointModel()
{
}

int PythonBreakpointModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return PyDebugger::instance()->breakpointCount();
}

int PythonBreakpointModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return colCount +1;
}

QVariant PythonBreakpointModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || (role != Qt::DisplayRole &&
                             role != Qt::ToolTipRole &&
                             role != Qt::DecorationRole))
    {
        return QVariant();
    }

    if (index.column() > colCount)
        return QVariant();

    auto debugger = PyDebugger::instance();
    auto bpl = debugger->getBreakpointFromIdx(index.row());
    if (!bpl)
        return QVariant();

    if (role == Qt::DecorationRole) {
        if (index.column() != 0)
            return QVariant();
        const char *icon = bpl->disabled() ? "breakpoint-disabled" : "breakpoint" ;

        return QVariant(BitmapFactory().iconFromTheme(icon));

    } else if (role == Qt::ToolTipRole) {
        QString enabledStr(QLatin1String("enabled"));
        if (bpl->disabled())
            enabledStr = QLatin1String("disabled");

        QString txt = QString(QLatin1String("line %1 [%2] number: %3\n%4"))
                    .arg(bpl->lineNr()).arg(enabledStr)
                    .arg(bpl->uniqueId()).arg(bpl->bpFile()->fileName());
        return QVariant(txt);
    }

    switch(index.column()) {
    case 0:
        return QString::number(bpl->uniqueId());
    case 1: {// file
        return bpl->bpFile()->fileName();
    }
    case 2: {// line
        return QString::number(bpl->lineNr());
    }
    default:
        return QVariant();
    }
}

QVariant PythonBreakpointModel::headerData(int section,
                                           Qt::Orientation orientation,
                                           int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();

    switch (section) {
    case 0:
        return QString(tr("Number"));
    case 1:
        return QString(tr("File"));
    case 2:
        return QString(tr("Line"));
    default:
        return QVariant();
    }
}

void PythonBreakpointModel::added(size_t uniqueId)
{
    Q_UNUSED(uniqueId)
    int count = rowCount();
    beginInsertRows(QModelIndex(), count, count);
    endInsertRows();
}

void PythonBreakpointModel::changed(size_t uniqueId)
{
    auto debugger = PyDebugger::instance();
    auto bp = debugger->getBreakpointFromUniqueId(uniqueId);
    int idx = debugger->getIdxFromBreakpoint(bp);
    if (idx > -1) {
        Q_EMIT dataChanged(index(idx, 0), index(idx, colCount));
    }
}

void PythonBreakpointModel::removed(size_t uniqueId)
{
    Q_UNUSED(uniqueId)
    auto bp = PyDebugger::instance()->getBreakpointFromUniqueId(uniqueId);
    if (bp) {
        int idx = static_cast<int>(bp->uniqueId());
        beginRemoveRows(QModelIndex(), idx, idx);
        endRemoveRows();
    }
}

// --------------------------------------------------------------------


IssuesModel::IssuesModel(QObject *parent) :
    QAbstractTableModel(parent)
{
    auto debugger = PyDebugger::instance();
    connect(debugger, &PyDebugger::exceptionOccured,
            this, &IssuesModel::exceptionFilter);

    connect(debugger, &PyDebugger::exceptionFatal,
            this, &IssuesModel::exception);

    connect(debugger, &PyDebugger::started, this, &IssuesModel::clear);
    connect(debugger, &PyDebugger::clearAllExceptions, this, &IssuesModel::clear);
    connect(debugger, &PyDebugger::clearException,
            this, &IssuesModel::clearException);


    MacroManager *macroMgr = Application::Instance->macroManager();
    connect(macroMgr, &MacroManager::exceptionFatal,
            this, &IssuesModel::exception);
}

IssuesModel::~IssuesModel()
{
    //qDeleteAll(m_exceptions); // not applicable to smart pointers
}

int IssuesModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_exceptions.size();
}

int IssuesModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return colCount;
}

QVariant IssuesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || (role != Qt::DisplayRole &&
                             role != Qt::ToolTipRole &&
                             role != Qt::DecorationRole))
    {
        return QVariant();
    }

    if (index.column() > colCount)
        return QVariant();

    auto exc = m_exceptions.at(index.row());
    if (!exc || !exc->isValid())
        return QVariant();

    if (role == Qt::DecorationRole) {
        if (index.column() != 0)
            return QVariant();
        PyExceptionInfoGui excGui(exc);
        return QVariant(BitmapFactory().iconFromTheme(excGui.iconName()));

    } else if (role == Qt::ToolTipRole) {

        QString srcText = QString::fromStdWString(exc->text());
        int offset = exc->getOffset();
        if (offset > 0) { // syntax error and such that give a column where it occurred
            if (!srcText.endsWith(QLatin1String("\n")))
                srcText += QLatin1String("\n");
            for (int i = 0; i < offset -1; ++i) {
                srcText += QLatin1Char('-');
            }
            srcText += QLatin1Char('^');
        }

        return  QString(tr("%1 on line %2 in file %3\nreason: '%4'\n\n%5"))
                                .arg(QString::fromStdString(exc->getErrorType(true)))
                                .arg(QString::number(exc->getLine()))
                                .arg(QString::fromStdString(exc->getFile()))
                                .arg(QString::fromStdString(exc->getMessage()))
                                .arg(srcText);
    }

    switch(index.column()) {
    case 0:
        return QString::number(exc->getLine());
    case 1: // file
        return QString::fromStdString(exc->getFile());
    case 2: // msg
        return QString::fromStdString(exc->getMessage());
    case 3: // function
        return QString::fromStdString(exc->getFunction());
    default:
        return QVariant();
    }
}

QVariant IssuesModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();

    switch (section) {
    case 0:
        return QString(tr("Line"));
    case 1:
        return QString(tr("File"));
    case 2:
        return QString(tr("Msg"));
    case 3:
        return QString(tr("Function"));
    default:
        return QVariant();
    }
}

bool IssuesModel::removeRows(int row, int count, const QModelIndex &parent)
{
    Q_UNUSED(parent)
    beginRemoveRows(QModelIndex(), row, count -1);
    for (int i = row; i < row + count; ++i) {
        m_exceptions.removeAt(i);
    }
    endRemoveRows();
    return true;
}

QModelIndex IssuesModel::index(int row, int column, const QModelIndex &parent) const
{
    (void)parent;
    return createIndex(row, column);
}

void IssuesModel::exceptionFilter(Base::Exception* exc)
{
    auto excPy = dynamic_cast<Base::PyExceptionInfo*>(exc);
    if (!excPy)
        return;

    // on runtime, we don't want to catch runtime exceptions before they are fatal
    // but we do want to catch warnings
    //if (exc->isWarning())
        exception(excPy);

}

void IssuesModel::exception(Base::PyExceptionInfo* exception)
{
    // already set?
    for (const auto &exc : m_exceptions) {
        if (exc->getFile() == exception->getFile() &&
            exc->getLine() == exception->getLine())
            return;
    }

    // copy exception and store it
    beginInsertRows(QModelIndex(), m_exceptions.size(), m_exceptions.size());
    m_exceptions.append(exception);
    endInsertRows();
}

void IssuesModel::clear()
{
    beginRemoveRows(QModelIndex(), 0, m_exceptions.size() -1);
    m_exceptions.clear();
    endRemoveRows();
}

void IssuesModel::clearException(const QString &fn, int line)
{
    for (int i = 0; i < m_exceptions.size(); ++i) {
        if (m_exceptions[i]->getFile() == fn.toStdString() &&
            m_exceptions[i]->getLine() == line)
        {
            beginRemoveRows(QModelIndex(), i, 1);
            m_exceptions.removeAt(i);
            endRemoveRows();
            break;
        }
    }
}


// --------------------------------------------------------------------


VariableTreeItem::VariableTreeItem(const QList<QVariant> &data,
                                   VariableTreeItem *parent):
    parentItem(parent),
    lazyLoad(false)
{
    itemData = data;
}

VariableTreeItem::~VariableTreeItem()
{
    Base::PyGILStateLocker lock;(void)lock;
    qDeleteAll(childItems);
}

void VariableTreeItem::addChild(VariableTreeItem *item)
{
    QRegExp re(QLatin1String("^__\\w[\\w\\n]*__$"));
    bool insertedIsSuper = re.indexIn(item->name()) != -1;

    // insert sorted
    for (int i = 0; i < childItems.size(); ++i) {
        bool itemIsSuper = re.indexIn(childItems[i]->name()) != -1;

        if (insertedIsSuper && itemIsSuper &&
            item->name() < childItems[i]->name()) {
            // both super sort alphabetically
            childItems.insert(i, item);
            return;
        } else if (!insertedIsSuper && itemIsSuper) {
            // new item not super but this child item is
            // insert before this one
            childItems.insert(i, item);
            return;
        } else if (item->name() < childItems[i]->name()) {
            // no super, just sort aplhabetically
            childItems.insert(i, item);
            return;
        }
    }

    // if we get here are definitely last
    childItems.append(item);
}

bool VariableTreeItem::removeChild(int row)
{
    Base::PyGILStateLocker lock;(void)lock;
    if (row > -1 && row < childItems.size()) {
        VariableTreeItem *item = childItems.takeAt(row);
        item->parentItem = nullptr;
        delete item;
        return true;
    }
    return false;
}

bool VariableTreeItem::removeChildren(int row, int nrRows)
{
    if (row < 0 || row >= childItems.size())
        return false;
    if (row + nrRows > childItems.size())
        return false;

    Base::PyGILStateLocker lock;(void)lock;
    for (int i = row; i < row + nrRows; ++i) {
        VariableTreeItem *item = childItems.takeAt(row);
        delete item;
    }
    return true;
}

VariableTreeItem *VariableTreeItem::child(int row)
{
    return childItems.value(row);
}

int VariableTreeItem::childRowByName(const QString &name)
{
    for (int i = 0; i < childItems.size(); ++i) {
        VariableTreeItem *item = childItems[i];
        if (item->name() == name)
            return i;
    }
    return -1;
}

VariableTreeItem *VariableTreeItem::childByName(const QString &name)
{
    for (int i = 0; i < childItems.size(); ++i) {
        VariableTreeItem *item = childItems.at(i);
        if (item->name() == name)
            return item;
    }

    return nullptr;
}

int VariableTreeItem::childCount() const
{
    return childItems.count();
}

int VariableTreeItem::columnCount() const
{
    return itemData.count();
}

QVariant VariableTreeItem::data(int column) const
{
    return itemData.value(column);
}

VariableTreeItem *VariableTreeItem::parent()
{
    return parentItem;
}

const QString VariableTreeItem::name() const
{
    return itemData.value(0).toString();
}

const QString VariableTreeItem::value() const
{
    return itemData.value(1).toString();
}

void VariableTreeItem::setValue(const QString value)
{
    itemData[1] = value;
}

const QString VariableTreeItem::type() const
{
    return itemData.value(2).toString();
}

void VariableTreeItem::setType(const QString type)
{
    itemData[2] = type;
}

void VariableTreeItem::setLazyLoadChildren(bool lazy)
{
    lazyLoad = lazy;
}

Py::Object VariableTreeItem::getAttr(const QString attrName) const
{
    Py::Object me;
    if (rootObj.isNull())
        me = parentItem->getAttr(itemData[0].toString());
    else
        me = rootObj;

    if (me.isNull())
        return me; // return nullobj

    Py::String attr(attrName.toStdString());

    if (PyModule_Check(me.ptr())) {
        if (me.hasAttr(attr))
            return me.getAttr(attr);

        me = PyModule_GetDict(me.ptr());
    }
    if (me.isDict() || me.isList())
        return me.getItem(attr);
    if (me.hasAttr(attr))
        return me.getAttr(attr);
    return Py::None();
}

bool VariableTreeItem::hasAttr(const QString attrName) const
{
    return getAttr(attrName).isNull();
}

void VariableTreeItem::setMeAsRoot(Py::Object root)
{
    rootObj = root;
}

bool VariableTreeItem::lazyLoadChildren() const
{
    return lazyLoad;
}

int VariableTreeItem::childNumber() const
{
    if (parentItem)
        return parentItem->childItems.indexOf(const_cast<VariableTreeItem*>(this));

    return 0;
}

// --------------------------------------------------------------------------


VarTreeModelBase::VarTreeModelBase(QObject *parent) :
    QAbstractItemModel(parent)
{
    QList<QVariant> rootData;
    rootData << tr("Name") << tr("Value") << tr("Type");
    m_rootItem = new VariableTreeItem(rootData, nullptr);

    auto debugger = PyDebugger::instance();

    connect(debugger, &PyDebugger::nextInstruction,
               this, &VarTreeModelBase::updateVariables);

    connect(debugger, &PyDebugger::functionExited,
               this, &VarTreeModelBase::clear);

    connect(debugger, &PyDebugger::stopped, this, &VarTreeModelBase::clear);
}

VarTreeModelBase::~VarTreeModelBase()
{
    Base::PyGILStateLocker lock;(void)lock;
    delete m_rootItem; // root item handles delete of children
}

int VarTreeModelBase::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        VariableTreeItem *item =  getItem(parent);
        if (item)
            return item->columnCount();

    }

    return m_rootItem->columnCount();
}

bool VarTreeModelBase::addRows(QList<VariableTreeItem*> added, const QModelIndex &parent)
{
    VariableTreeItem *parentItem = getItem(parent);
    if (!parentItem)
        return false;

    int childCount =  parentItem->childCount();

    beginInsertRows(parent, childCount, childCount + added.size() -1);
    for (VariableTreeItem *item : added) {
        parentItem->addChild(item);
    }
    endInsertRows();
    Q_EMIT layoutChanged();

    return true;
}

bool VarTreeModelBase::removeRows(int firstRow, int nrRows, const QModelIndex &parent)
{
    VariableTreeItem *item = getItem(parent);
    if (!item)
        return false;

    if (firstRow < 0 || firstRow >= item->childCount())
        return false;
    if (nrRows < 0 || firstRow + nrRows > item->childCount())
        nrRows = item->childCount() - firstRow;

    beginRemoveRows(parent, firstRow, firstRow + nrRows -1);
    for (int i = firstRow; i < firstRow + nrRows; ++i) {
        item->removeChild(firstRow);
    }
    endRemoveRows();

    // invalidate abstractmodels internal index, otherwise we crash
    for(QModelIndex &idx : persistentIndexList()) {
        changePersistentIndex(idx, QModelIndex());
    }

    return true;
}

bool VarTreeModelBase::hasChildren(const QModelIndex &parent) const
{
    VariableTreeItem *parentItem;
    if (parent.column() > 0)
        return 0;

    if (!parent.isValid())
        parentItem = m_rootItem;
    else
        parentItem = getItem(parent);

    if (!parentItem)
        return 0;

    return parentItem->lazyLoadChildren() || parentItem->childCount() > 0;
}

VariableTreeItem *VarTreeModelBase::getItem(const QModelIndex &index) const
{
    if (index.isValid()) {
        VariableTreeItem *item = static_cast<VariableTreeItem*>(index.internalPointer());
        if (item)
            return item;
    }
    return m_rootItem;
}


QVariant VarTreeModelBase::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (role != Qt::DisplayRole && role != Qt::ToolTipRole)
        return QVariant();

    VariableTreeItem *item = getItem(index);

    if (!item)
        return QVariant();

    return item->data(index.column());
}

Qt::ItemFlags VarTreeModelBase::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return nullptr;

    return QAbstractItemModel::flags(index); // Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QVariant VarTreeModelBase::headerData(int section, Qt::Orientation orientation,
                                       int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
        return m_rootItem->data(section);

    return QVariant();
}

QModelIndex VarTreeModelBase::index(int row, int column, const QModelIndex &parent)
            const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    VariableTreeItem *parentItem;

    if (!parent.isValid())
        parentItem = m_rootItem;
    else
        parentItem = getItem(parent);

    if (!parentItem)
        return QModelIndex();

    VariableTreeItem *childItem = parentItem->child(row);
    if (childItem)
        return createIndex(row, column, childItem);

    return QModelIndex();
}

QModelIndex VarTreeModelBase::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    VariableTreeItem *childItem = getItem(index);
    if (!childItem)
        return QModelIndex();

    VariableTreeItem *parentItem = childItem->parent();

    if (parentItem == m_rootItem || parentItem == nullptr)
        return QModelIndex();

    return createIndex(parentItem->childNumber(), 0, parentItem);
}

int VarTreeModelBase::rowCount(const QModelIndex &parent) const
{
    VariableTreeItem *item = getItem(parent);
    if (!item)
        return 0;
    return item->childCount();
}

void VarTreeModelBase::lazyLoad(const QModelIndex &parent)
{
    VariableTreeItem *parentItem = getItem(parent);
    if (parentItem && parentItem->lazyLoadChildren())
        lazyLoad(parentItem);
}

void VarTreeModelBase::lazyLoad(VariableTreeItem *parentItem)
{
    Py::Dict dict;
    // workaround to not being able to store pointers to variables
    QString myName = parentItem->name();
    Py::Object me = parentItem->parent()->getAttr(myName);
    if (me.isNull())
        return;

    try {

        Py::List lst (PyObject_Dir(me.ptr()));

        for (uint i = 0; i < lst.length(); ++i) {
            Py::Object attr(PyObject_GetAttr(me.ptr(), lst[i].str().ptr()));
            dict.setItem(lst[i], attr);
        }

        scanObject(dict.ptr(), parentItem, 15, true);

    } catch (Py::Exception e) {
        PyErr_Print();
        e.clear();
    }
}

void VarTreeModelBase::scanObject(PyObject *startObject, VariableTreeItem *parentItem,
                                   int depth, bool noBuiltins)
{
    Base::PyGILStateLocker lock;(void)lock;

    // avoid cyclic recursion
    static const int maxDepth = 15;
    if (depth > maxDepth)
        return;

    QList<VariableTreeItem*> added;
    QList<QString> visited, updated, deleted;

    Py::List keys;
    Py::Dict object;

    // only scan object which has keys
    try {
        if (PyDict_Check(startObject))
            object = Py::Dict(startObject);
        else if (PyList_Check(startObject)) {
            Py::List lst(startObject);
            for (int i = 0; i < static_cast<int>(lst.size()); ++i) {
                Py::Int key(i);
                object[key.str()] = lst[i];
            }
        } else if (PyTuple_Check(startObject)) {
            Py::Tuple tpl(startObject);
            for (int i = 0; i < static_cast<int>(tpl.size()); ++i) {
                Py::Int key(i);
                object[key.str()] = tpl[i];
            }
        } else if (PyFrame_Check(startObject)){
            PyFrameObject *frame = (PyFrameObject*)startObject;
            object = Py::Dict(frame->f_locals);
        } else if (PyModule_Check(startObject)) {
            object = Py::Dict(PyModule_GetDict(startObject));
        } else {
            // all objects should be able to use dir(o) on?
            keys = Py::List(PyObject_Dir(startObject));
            for (Py::Object key : keys) {
                object[key] = Py::Object(PyObject_GetAttr(startObject, key.ptr()));
            }
        }
        // let other types fail and return here

        keys = object.keys();

    } catch(Py::Exception e) {
        //Get error message
        PyObject *ptype, *pvalue, *ptraceback;
        PyErr_Fetch(&ptype, &pvalue, &ptraceback);
        Py::String err(pvalue);
        qDebug() << "Not supported type in debugger:" << QString::fromStdString(err) << endl;
        e.clear();
        return;
    }


    // first we traverse and compare before we do anything so
    // view doesn't get updated too many times

    // traverse and compare
    for (Py::List::iterator it = keys.begin(); it != keys.end(); ++it) {
        if ((*it).ptr() == nullptr)
            continue;
        Py::String key(*it);
        QString name = QString::fromStdString(key);
        // PyCXX methods throws here on type objects from class self type?
        // use Python C instead
        QString newValue, newType;
        PyObject *vl = nullptr,
                 *itm = PyDict_GetItem(object.ptr(), key.ptr());
        if (itm) {
            Py_XINCREF(itm);
            if (PyCallable_Check(itm))
                continue; // don't want to crowd explorer with functions
            vl = PyObject_Str(itm);
            if (vl) {
#if PY_MAJOR_VERSION >= 3
                const char *vlu = PyUnicode_AsUTF8(vl);
#else
                const char *vlu = PyBytes_AS_STRING(vl);
#endif
                newValue = QLatin1String(vlu);

                if (!PyBytes_Check(itm)) {
                    // extract memory address if needed, but not on ordinary strings
                    QRegExp re(QLatin1String("^<\\w+[\\w\\s'\"\\.\\-]* at (?:0x)?([0-9a-fA-F]+)>$"));
                    if (re.indexIn(newValue) != -1) {
                        newValue = re.cap(1);
                    }
                } else {
                    // it's a string, wrap in quotes ('')
                    newValue = QString(QLatin1String("'%1'")).arg(newValue);
                }

                newType = QString::fromLatin1(Py_TYPE(itm)->tp_name);
            }
            Py_DECREF(itm);
            Py_XDECREF(vl);
            PyErr_Clear();
        }

        VariableTreeItem *currentChild = parentItem->childByName(name);
        if (currentChild == nullptr) {
            // not found, should add to model
            QRegExp reBuiltin(QLatin1String(".*built-*in.*"));
            if (noBuiltins) {
                if (reBuiltin.indexIn(newType) != -1)
                    continue;
            } else if (reBuiltin.indexIn(newType) == -1)
                continue;

            QList<QVariant> data;
            data << name << newValue << newType;
            VariableTreeItem *createdItem = new VariableTreeItem(data, parentItem);
            added.append(createdItem);

            if (PyDict_Check(itm) || PyList_Check(itm) || PyTuple_Check(itm)) {
                // check for subobject recursively
                scanObject(itm, createdItem, depth +1, true);

            } else if (Py_TYPE(itm)->tp_dict && PyDict_Size(Py_TYPE(itm)->tp_dict)) // && // members check
                      // !createdItem->parent()->hasAttr(name)) // avoid cyclic child inclusions
            {
                // set lazy load for these
                createdItem->setLazyLoadChildren(true);
            }
        } else {
            visited.append(name);

            if (newType != currentChild->type() ||
                newValue != currentChild->value())
            {
                currentChild->setType(newType);
                currentChild->setValue(newValue);
                updated.append(name);
            }

            // check its children
            if (currentChild->lazyLoadChildren() && currentChild->childCount() > 0)
                lazyLoad(currentChild);
        }
    }

    // maybe some values have gotten out of scope
    for (int i = 0; i < parentItem->childCount(); ++i) {
        VariableTreeItem *item = parentItem->child(i);
        if (!visited.contains(item->name()))
            deleted.append(item->name());
    }

    // finished compare, now do the changes


    // handle updates
    // this tries to find out how many consecutive rows that've changed
    int row = 0;
    for (const QString &item : updated) {
        row = parentItem->childRowByName(item);
        if (row < 0) continue;

        QModelIndex topLeftIdx = createIndex(row, 1, parentItem);
        QModelIndex bottomRightIdx = createIndex(row, 2, parentItem);
        Q_EMIT dataChanged(topLeftIdx, bottomRightIdx);
    }

    // handle deletes
    for (const QString &name : deleted) {
        row = parentItem->childRowByName(name);
        if (row < 0) continue;
        QModelIndex parentIdx = createIndex(row, 0, parentItem);
        removeRows(row, 1, parentIdx);
    }

    // handle inserts
    // these are already created during compare phase
    // we insert last so variables don't jump around in the treeview
    if (added.size()) {
        QModelIndex parentIdx = createIndex(row, 0, parentItem);
        addRows(added, parentIdx);
    }

    // notify view that parentItem might have children now
    if (updated.size() || added.size() || deleted.size()) {
        //Q_EMIT layoutChanged();
    }
}

// -------------------------------------------------------------------------


VariableTreeModel::VariableTreeModel(QObject *parent)
    : VarTreeModelBase(parent)
{
    QList<QVariant> locals, globals; //, builtins;

    locals << QLatin1String("locals") << QLatin1String("") << QLatin1String("");
    globals << QLatin1String("globals") << QLatin1String("") << QLatin1String("");
    //builtins << QLatin1String("builtins") << QLatin1String("") << QLatin1String("");
    m_localsItem = new VariableTreeItem(locals, m_rootItem);
    m_globalsItem = new VariableTreeItem(globals, m_rootItem);
    //m_builtinsItem = new VariableTreeItem(builtins, m_rootItem);
    m_rootItem->addChild(m_localsItem);
    m_rootItem->addChild(m_globalsItem);
    //m_rootItem->appendChild(m_builtinsItem);

}

VariableTreeModel::~VariableTreeModel()
{
}

void VariableTreeModel::clear()
{
    // locals
    if (m_localsItem->childCount() > 0) {
        QModelIndex localsIdx = createIndex(0, 0, m_localsItem);
        removeRows(0, m_localsItem->childCount(), localsIdx);
    }

    // globals
    if (m_globalsItem->childCount()) {
        QModelIndex globalsIdx = createIndex(0, 0, m_globalsItem);
        removeRows(0, m_globalsItem->childCount(), globalsIdx);
    }
}

void VariableTreeModel::updateVariables()
{
    Base::PyGILStateLocker lock;

    PyFrameObject *frame = PyDebugger::instance()->currentFrame();
    PyFrame_FastToLocals(frame);

    // first locals
    VariableTreeItem *parentItem = m_localsItem;
    PyObject *rootObject = static_cast<PyObject*>(frame->f_locals);
    if (rootObject) {
        m_localsItem->setMeAsRoot(Py::Object(rootObject));
        scanObject(rootObject, parentItem, 0, true);
    }

    // then globals
    parentItem = m_globalsItem;
    PyObject *globalsDict = frame->f_globals;
    if (globalsDict) {
        m_globalsItem->setMeAsRoot(Py::Object(globalsDict));
        scanObject(globalsDict, parentItem, 0, true);
    }

    /*
    // and the builtins
    parentItem = m_builtinsItem;
    rootObject = (PyObject*)frame->f_builtins;
    if (rootObject) {
        m_builtinsItem->setMeAsRoot(Py::Object(rootObject));
        scanObject(rootObject, parentItem, 0, false);
    }*/
}

// -----------------------------------------------------------------

WatchModel::WatchModel(QObject *parent) :
    VarTreeModelBase(parent)
{
}

WatchModel::~WatchModel()
{
}

void WatchModel::addItem(QString name)
{
    if (m_rootItem->childByName(name))
        return;

    beginInsertRows(createIndex(m_rootItem->childCount(), 0, m_rootItem), m_rootItem->childCount(), m_rootItem->childCount());
    QList<QVariant> item;
    item << name << QLatin1String("") << QLatin1String("");
    auto *child = new VariableTreeItem(item, m_rootItem);
    m_rootItem->addChild(child);
    endInsertRows();
    Q_EMIT layoutChanged();

    // update variables with newly added
    auto debugger = App::Debugging::Python::Debugger::instance();
    if (debugger->currentFrame())
        updateVariables();
}

void WatchModel::removeItem(QString name)
{
    VariableTreeItem *itm = m_rootItem->childByName(name);
    if (!itm)
        return;

    int childNr = itm->childNumber();
    beginRemoveRows(createIndex(0, 0, m_rootItem), childNr, childNr);
    m_rootItem->removeChild(childNr);
    endInsertRows();
}

void WatchModel::clear()
{
    for (int i = 0; i < m_rootItem->childCount(); ++i) {
        VariableTreeItem *itm = m_rootItem->child(i);
        if (itm->childCount() > 0) {
            QModelIndex idx = createIndex(0, 0, itm);
            removeRows(0, itm->childCount(), idx);
        }
    }
}

void WatchModel::updateVariables()
{
    Base::PyGILStateLocker lock;

    PyFrameObject *frame = PyDebugger::instance()->currentFrame();
    PyFrame_FastToLocals(frame);

    // stored variables
    for (int i = 0; i < m_rootItem->childCount(); ++i) {
        VariableTreeItem *itm = m_rootItem->child(i);
        PyObject *rootObject = static_cast<PyObject*>(frame->f_locals);
        if (rootObject && PyDict_Check(rootObject)) {
            Py::Dict locals(rootObject);
            if (locals.hasKey(itm->data(0).toString().toStdString())) {
                Py::Object obj = locals.getItem(itm->data(0).toString().toStdString());

                itm->setMeAsRoot(obj);
                scanObject(obj.ptr(), itm, 0, true);
            } else {
                // clear variable
                if (itm->childCount() > 0) {
                    QModelIndex idx = createIndex(i, 1, itm);
                    removeRows(0, itm->childCount(), idx);
                }
            }
        }
    }
}


#include "moc_PythonDebuggerView.cpp"
