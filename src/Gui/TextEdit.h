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


#ifndef GUI_TEXTEDIT_H
#define GUI_TEXTEDIT_H

#include <QListWidget>
#include <QPlainTextEdit>
#include <QScrollBar>
#include "View.h"
#include "Window.h"

QT_BEGIN_NAMESPACE
class QCompleter;
QT_END_NAMESPACE

namespace Gui {
class CompletionBox;
class SyntaxHighlighter;

/**
 * Completion is a means by which an editor automatically completes words that the user is typing. 
 * For example, in a code editor, a programmer might type "sur", then Tab, and the editor will complete 
 * the word the programmer was typing so that "sur" is replaced by "surnameLineEdit". This is very 
 * useful for text that contains long words or variable names. The completion mechanism usually works 
 * by looking at the existing text to see if any words begin with what the user has typed, and in most 
 * editors completion is invoked by a special key sequence.
 *
 * TextEdit can detect a special key sequence to invoke the completion mechanism, and can handle three 
 * different situations:
 * \li There are no possible completions.
 * \li There is a single possible completion.
 * \li There are two or more possible completions.
 * 
 * \remark The original sources are taken from Qt Quarterly (Customizing for Completion).
 * @author Werner Mayer
 */
class CompletionList;
class GuiExport TextEdit : public QPlainTextEdit
{
    Q_OBJECT

public:
    TextEdit(QWidget *parent = 0);
    virtual ~TextEdit();
    virtual void highlightText(int pos, int len, const QTextCharFormat format);

private Q_SLOTS:
    void complete();

protected:
    void keyPressEvent(QKeyEvent *);

private:
    void createListBox();

private:
    QString wordPrefix;
    int cursorPosition;
    CompletionList *listBox;
};

class LineMarkerArea;
class SyntaxHighlighter;
class GuiExport TextEditor : public TextEdit, public WindowParameter
{
    Q_OBJECT

public:
    TextEditor(QWidget *parent = 0);
    ~TextEditor();
    void setSyntaxHighlighter(SyntaxHighlighter*);
    SyntaxHighlighter *syntaxHighlighter();

    void OnChange(Base::Subject<const char*> &rCaller,const char* rcReason);

    //void lineNumberAreaPaintEvent(QPaintEvent* );
    int lineNumberAreaWidth();
    int findAndHighlight(const QString needle, QTextDocument::FindFlags flags = 0);
    // highlights text such as search for
    void setTextMarkers(QString key, QList<QTextEdit::ExtraSelection> selections);

    void setCompleter(QCompleter *completer) const;
    QCompleter *completer() const;

private Q_SLOTS:
    void updateLineNumberAreaWidth(int newBlockCount);
    void updateLineNumberArea(const QRect &, int);
    void highlightSelections();

protected:
    void keyPressEvent (QKeyEvent * e);
    /** Draw a beam in the line where the cursor is. */
    void paintEvent (QPaintEvent * e);
    void resizeEvent(QResizeEvent* e);
    LineMarkerArea* lineMarkerArea() const
    { return lineNumberArea; }
    virtual void drawMarker(int line, int x, int y, QPainter*);
    bool event(QEvent *event);
    virtual bool editorToolTipEvent(QPoint pos, const QString &textUnderPos);
    virtual bool lineMarkerAreaToolTipEvent(QPoint pos, int line);

private:
    SyntaxHighlighter* highlighter;
    LineMarkerArea* lineNumberArea;
    struct TextEditorP* d;

    friend class SyntaxHighlighter;
    friend class LineMarkerArea;
};


class LineMarkerAreaP;
class LineMarkerArea : public QWidget
{
    Q_OBJECT

public:
    LineMarkerArea(TextEditor* editor);
    virtual ~LineMarkerArea();

    QSize sizeHint() const;
    void setLineNumbersVisible(bool active);
    bool lineNumbersVisible() const;

Q_SIGNALS:
    void clickedOnLine(int line, QMouseEvent *event);
    void contextMenuOnLine(int line, QContextMenuEvent * event);

protected:
    void paintEvent (QPaintEvent *event);
    void mouseReleaseEvent(QMouseEvent * event);
    void wheelEvent(QWheelEvent * event);
    void contextMenuEvent(QContextMenuEvent * event);

private:
    LineMarkerAreaP *d;
};


/**
 * @breif A custom scrollbar that can show markers at different lines
 *      such as markers when searching for a word
 */
class AnnotatedScrollBar : public QScrollBar
{
    Q_OBJECT
public:
    AnnotatedScrollBar(TextEditor *parent = 0);
    ~AnnotatedScrollBar();

    /**
     * @brief setMarker at a given line in document
     * @param line: the line
     * @param color: the color to render with
     */
    void setMarker(int line, QColor color);

    /**
     * @brief resetMarkers clears markers och color and sets new ones found in color
     * @param color
     */
    void resetMarkers(QList<int> newMarkers, QColor color);
    /**
     * @brief Clear all markers
     */
    void clearMarkers();

    /**
     * @brief Clear all Markers of the given color
     * @param color
     */
    void clearMarkers(QColor color);

    void clearMarker(int line, QColor color);

protected:
    void paintEvent(QPaintEvent *e);

private:
    QMultiHash<int, QColor> m_markers;
    TextEditor *m_editor;
};

/**
 * The CompletionList class provides a list box that pops up in a text edit if the user has pressed
 * an accelerator to complete the current word he is typing in.
 * @author Werner Mayer
 */
class CompletionList : public QListWidget
{
    Q_OBJECT

public:
    /// Construction
    CompletionList(QPlainTextEdit* parent);
    /// Destruction
    ~CompletionList();

    void findCurrentWord(const QString&);

protected:
    bool eventFilter(QObject *, QEvent *);

private Q_SLOTS:
    void completionItem(QListWidgetItem *item);

private:
    QPlainTextEdit* textEdit;
};

} // namespace Gui

#endif // GUI_TEXTEDIT_H
