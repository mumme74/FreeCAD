/***************************************************************************
 *   Copyright (c) 2004 Werner Mayer <wmayer[at]users.sourceforge.net>     *
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


#ifndef GUI_PYTHONEDITOR_H
#define GUI_PYTHONEDITOR_H

#include "Window.h"
#include "TextEdit.h"
#include "SyntaxHighlighter.h"
#include "PythonCode.h"
#include <QDialog>


QT_BEGIN_NAMESPACE
class QSpinBox;
class QLineEdit;
class QCheckBox;
QT_END_NAMESPACE

namespace Py {
class ExceptionInfo;
}

namespace Gui {

class PythonSyntaxHighlighter;
class PythonSyntaxHighlighterP;
class PythonEditorBreakpointDlg;
class PythonDebugger;
class BreakpointLine;

/**
 * Python text editor with syntax highlighting.
 * \author Werner Mayer
 */
class GuiExport PythonEditor : public TextEditor
{
    Q_OBJECT

public:
    PythonEditor(QWidget *parent = 0);
    ~PythonEditor();

public Q_SLOTS:
    void toggleBreakpoint();
    void showDebugMarker(int line);
    void hideDebugMarker();
    // python modules for code completion
    //void importModule(const QString name);
    //void importModuleFrom(const QString from, const QString name);


    /** Inserts a '#' at the beginning of each selected line or the current line if 
     * nothing is selected
     */
    void onComment();
    /**
     * Removes the leading '#' from each selected line or the current line if
     * nothing is selected. In case a line hasn't a leading '#' then
     * this line is skipped.
     */
    void onUncomment();
    /**
     * @brief onAutoIndent
     * Indents selected codeblock
     */
    void onAutoIndent();

    void setFileName(const QString&);
    int  findText(const QString find);
    void startDebug();

    void OnChange(Base::Subject<const char*> &rCaller,const char* rcReason);

    void cut();
    void paste();

protected:
    /** Pops up the context menu with some extensions */
    void contextMenuEvent ( QContextMenuEvent* e );
    void drawMarker(int line, int x, int y, QPainter*);
    void keyPressEvent(QKeyEvent * e);
    bool editorToolTipEvent(QPoint pos, const QString &textUnderPos);
    bool lineMarkerAreaToolTipEvent(QPoint pos, int line);

public Q_SLOTS:
    void clearAllExceptions();
    void clearException(const QString &fn, int line);

private Q_SLOTS:
    void markerAreaContextMenu(int line, QContextMenuEvent *event);
    void breakpointAdded(const BreakpointLine *bpl);
    void breakpointChanged(const BreakpointLine *bpl);
    void breakpointRemoved(int idx, const BreakpointLine *bpl);
    void exception(const Py::ExceptionInfo *exc);


private:
    void breakpointPasteOrCut(bool doCut);
    QString introspect(QString varName);
    void renderExceptionExtraSelections();
    struct PythonEditorP* d;
};

// ---------------------------------------------------------------

class PythonEditorBreakpointDlg : public QDialog
{
    Q_OBJECT
public:
    PythonEditorBreakpointDlg(QWidget *parent, BreakpointLine *bp);
    ~PythonEditorBreakpointDlg();
protected:
    void accept();
 private:
    BreakpointLine *m_bpl;

    QSpinBox  *m_ignoreToHits;
    QSpinBox  *m_ignoreFromHits;
    QLineEdit *m_condition;
    QCheckBox *m_enabled;
};

} // namespace Gui


#endif // GUI_PYTHONEDITOR_H
