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

namespace Gui {

class PythonSyntaxHighlighter;
class PythonSyntaxHighlighterP;

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

    void toggleBreakpoint();
    void showDebugMarker(int line);
    void hideDebugMarker();
    // python modules for code completion
    void importModule(const QString name);
    void importModuleFrom(const QString from, const QString name);

public Q_SLOTS:
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
     * @brief onIndent
     * Indents selected codeblock
     */
    void onAutoIndent();

    void setFileName(const QString&);
    void startDebug();

protected:
    /** Pops up the context menu with some extensions */
    void contextMenuEvent ( QContextMenuEvent* e );
    void drawMarker(int line, int x, int y, QPainter*);
    void keyPressEvent(QKeyEvent * e);
    QTextCursor inputBegin( void ) const;

private:
    //PythonSyntaxHighlighter* pythonSyntax;
    struct PythonEditorP* d;
};

/**
 * Syntax highlighter for Python.
 * \author Werner Mayer
 */
class GuiExport PythonSyntaxHighlighter : public SyntaxHighlighter
{
public:
    PythonSyntaxHighlighter(QObject* parent);
    virtual ~PythonSyntaxHighlighter();

    void highlightBlock (const QString & text);

    enum States {
        Standard         = 0,     // Standard text
        Digit            = 1,     // Digits
        Comment          = 2,     // Comment begins with #
        Literal1         = 3,     // String literal beginning with "
        Literal2         = 4,     // Other string literal beginning with '
        Blockcomment1    = 5,     // Block comments beginning and ending with """
        Blockcomment2    = 6,     // Other block comments beginning and ending with '''
        ClassName        = 7,     // Text after the keyword class
        DefineName       = 8,     // Text after the keyword def
        ImportName       = 9,    // Text after import statement
        FromName         = 10,    // Text after from statement before import statement
    };

private:
    PythonSyntaxHighlighterP* d;

    inline void setComment(int pos, int len);
    inline void setSingleQuotString(int pos, int len);
    inline void setDoubleQuotString(int pos, int len);
    inline void setSingleQuotBlockComment(int pos, int len);
    inline void setDoubleQuotBlockComment(int pos, int len);
    inline void setOperator(int pos, int len);
    inline void setKeyword(int pos, int len);
    inline void setText(int pos, int len);
    inline void setNumber(int pos, int len);
};

} // namespace Gui

#endif // GUI_PYTHONEDITOR_H
