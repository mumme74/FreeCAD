#ifndef PYTHONSYNTAXHIGHLIGHTER_H
#define PYTHONSYNTAXHIGHLIGHTER_H

#include "PreCompiled.h"
#include "SyntaxHighlighter.h"
#include "PythonToken.h"
#include "TextEditor.h"
#include <Base/Parameter.h>

QT_BEGIN_NAMESPACE
class QPlainTextEdit;
QT_END_NAMESPACE

namespace Base {
class PyException;
class PyExceptionInfo;
}

namespace Gui {
class TextEditBlockScanInfo;
class PythonEditor;
class PythonConsoleTextEdit;

namespace Python {

class TextBlockData;
class SyntaxHighlighterP;

/**
 * Syntax highlighter for Python.
 */
class GuiExport SyntaxHighlighter : public Gui::SyntaxHighlighter,
                                    public Python::Tokenizer
{
    Q_OBJECT
public:
    SyntaxHighlighter(QObject* parent);
    virtual ~SyntaxHighlighter();

    void highlightBlock (const QString & text);


    /// masks for decoding userState()
    enum {
        ParamLineShiftPos = 17,
        ParenCntShiftPos  = 28,
        TokensMASK      = 0x0000FFFF,
        ParamLineMASK   = 0x00010000, // when we are whithin a function parameter line
        ParenCntMASK    = 0xF0000000, // how many parens we have open from previous block
        PreventTokenize = 0x00020000, // prevents this call from running tokenize, used to repaint this block
        Rehighlighted   = 0x00040000, // used to determine if we already have rehighlighted
    };

    /// returns the format to color this token
    QTextCharFormat getFormatToken(const Python::Token *token) const;

    /// update how this token is rendered
    void tokenTypeChanged(const Python::Token *tok) const override;

    /// these formats a line in a predefined maner to to highlight were the error occured
    void setMessage(const Python::Token *tok) const;
    void setIndentError(const Python::Token *tok) const;
    void setSyntaxError(const Python::Token *tok) const;

    /// inserts a new format for token
    void newFormatToken(const Python::Token *tok, QTextCharFormat format) const;

    // used by code analyzer, set by editor
    void setFilePath(QString filePath);
    QString filePath() const;

protected:
    // tokenizes block, called by highlightBlock when text have changed
    //int tokenize(const QString &text, int &parenCnt, bool &isParamLine);

    Python::Token::Type unhandledState(uint &pos, int state, const std::string &text) override;

    /// sets (re-colors) txt contained from token
    void tokenUpdated(const Python::Token *tok) override;

private Q_SLOTS:
    void sourceScanTmrCallback();

private:
    SyntaxHighlighterP* d;

    /*

    //const QString getWord(int pos, const QString &text) const;
    int lastWordCh(int startPos, const std::string &text) const;
    int lastNumberCh(int startPos, const std::string &text) const;
    int lastDblQuoteStringCh(int startAt, const std::string &text) const;
    int lastSglQuoteStringCh(int startAt, const std::string &text) const;
    Python::Token::Type numberType(const std::string &text) const;

    Token *setRestOfLine(int &pos, const std::string &text, Python::Token::Type tokType);
    Token *scanIndentation(int &pos, const std::string &text);

    Token *setWord(int &pos, int len, Python::Token::Type tokType);

    Token *setIdentifier(int &pos, int len, Python::Token::Type tokType);
    Token *setUndeterminedIdentifier(int &pos, int len, Python::Token::Type tokType);

    Token *setNumber(int &pos, int len, Python::Token::Type tokType);

    Token *setOperator(int &pos, int len, Python::Token::Type tokType);

    Token *setDelimiter(int &pos, int len, Python::Token::Type tokType);

    Token *setSyntaxError(int &pos, int len);

    Token *setLiteral(int &pos, int len, Python::Token::Type tokType);

    // this is special can only be one per line
    // T_Indentation(s) token is generated related when looking at lines above as described in
    // https://docs.python.org/3/reference/lexical_analysis.html#indentation
    Token *setIndentation(int &pos, int len, int count);
    */

};


// ------------------------------------------------------------------------

struct MatchingCharInfo
{
    char character;
    int position;
    MatchingCharInfo();
    MatchingCharInfo(const MatchingCharInfo &other);
    MatchingCharInfo(char chr, int pos);
    ~MatchingCharInfo();
    char matchingChar() const;
    static char matchChar(char match);
};

// --------------------------------------------------------------------



/**
 * @brief The PythonMatchingChars highlights the opposite (), [] or {}
 */
class MatchingChars : public QObject
{
    Q_OBJECT

public:
    MatchingChars(PythonEditor *parent);
    MatchingChars(PythonConsoleTextEdit *parent);
    ~MatchingChars();

private Q_SLOTS:
    void cursorPositionChange();

private:
    QPlainTextEdit *m_editor;
};

// -----------------------------------------------------------------------
class TextBlockScanInfo;

class TextBlockData : public Gui::TextEditBlockData,
                      public Python::TokenLine
{
public:
    //typedef QVector<Python::Token*> tokens_t;
    explicit TextBlockData(QTextBlock block, TokenList *tokenList,
                           Token *startTok = nullptr);
    TextBlockData(const TextBlockData &other);
    ~TextBlockData();

    static Python::TextBlockData *pyBlockDataFromCursor(const QTextCursor &cursor);

    Python::TextBlockData *nextBlock() const;
    Python::TextBlockData *previousBlock() const;

    /**
     * @brief tokenAt returns the token defined pointed to by cursor
     * @param cursor that hovers the token to lookup
     * @return pointer to token or nullptr
     */
    static Python::Token* tokenAt(const QTextCursor &cursor);

    Python::Token *tokenAt(int pos) const;


    /**
     * @brief isMatchAt compares token at given pos
     * @param pos position i document
     * @param token match against this token
     * @return true if token are the same
     */
    bool isMatchAt(int pos, Python::Token::Type token) const;


    /**
     * @brief isMatchAt compares multiple tokens at given pos
     * @param pos position i document
     * @param token a list of tokens to match against this token
     * @return true if any of the token are the same
     */
    bool isMatchAt(int pos, const QList<Python::Token::Type> tokens) const;

    // FIXME: Sould factor scanInfo to  be disconnected from TextBlockData
    /**
     * @brief scanInfo contains parse messages set by code analyzer
     * @return nullptr if no parsemessages  or PythonTextBlockScanInfo*
     */
    Python::TextBlockScanInfo *scanInfo() const;

    /**
     * @brief setScanInfo set class with parsemessages
     * @param scanInfo instance of PythonTextBlockScanInfo
     */
    void setScanInfo(Python::TextBlockScanInfo *scanInfo);

protected:
    /**
     * @brief insert should only be used by PythonSyntaxHighlighter
     */
    const Python::Token *setDeterminedToken(Token::Type tokType, int startPos, int len);
    /**
     * @brief insert should only be used by PythonSyntaxHighlighter
     *  signifies that parse tree lookup is needed
     */
    const Python::Token *setUndeterminedToken(Python::Token::Type tokType, int startPos, int len);


    /**
     * @brief setIndentCount should be considered private, is
     * @param count
     */
    void setIndentCount(int count);


private:
   // tokens_t m_tokens;
    QVector<int> m_undeterminedIndexes; // index to m_tokens where a undetermined is at
                                        //  (so context parser can detemine it later)
    int m_indentCharCount; // as spaces NOTE according to python documentation a tab is 8 spaces

#ifdef BUILD_PYTHON_DEBUGTOOLS
    QString m_textDbg;
#endif

    friend class Python::SyntaxHighlighter; // in order to hide some api
};

// -------------------------------------------------------------------

class TextBlockScanInfo : public Gui::TextEditBlockScanInfo
{
public:
    explicit TextBlockScanInfo();
    virtual ~TextBlockScanInfo();


    /// set message for token
    void setParseMessage(const Python::Token *tok, QString message, MsgType type = Message);

    /// get the ParseMsg for tok, filter by type
    /// nullptr if not found
    const ParseMsg *getParseMessage(const Python::Token *tok, MsgType type = AllMsgTypes) const;

    /// get parseMessage for token, filter by type
    QString parseMessage(const Python::Token *tok, MsgType type = AllMsgTypes) const;

    /// clear message
    void clearParseMessage(const Python::Token *tok);
};

// -------------------------------------------------------------------

} // namespace Python


/**
 * @brief helper to determine what icon to use based on exception
 */
class PyExceptionInfoGui
{
public:
    explicit PyExceptionInfoGui(Base::PyExceptionInfo *exc);
    ~PyExceptionInfoGui();
    const char *iconName() const;
private:
    Base::PyExceptionInfo *m_exc;
};

} // namespace Gui


#endif // PYTHONSYNTAXHIGHLIGHTER_H
