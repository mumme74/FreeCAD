#ifndef PYTHONTOKEN_H
#define PYTHONTOKEN_H

#include <vector>
#include <string>
#include <list>
#include <map>
#include <iterator>
#include <memory>

// masks for customType, meaning depends on Token::Type
#define STRING_IS_BYTES_TYPE     (1 << 0)
#define STRING_IS_UNICODE_TYPE   (1 << 1)
#define STRING_IS_FORMAT_TYPE    (1 << 2)
#define STRING_IS_RAW_TYPE       (1 << 3)
#define STRING_IS_MULTILINE_TYPE (1 << 4)

#define NUMBER_IS_IMAGINARY      (1 << 0)

namespace Python {

class SourceListNodeBase; // in code analyzer
class TextBlockData;
class TokenList;
class TokenLine;
class Token;
class Lexer;
class TokenScanInfo;
class TokenWrapperBase;

/// class used to differentiate between versions
/// user can set lexer/parser to generate from python version
class Version
{
public:
    enum versions { Invalid,
        // must add new versions in desceding order
        v2_6, v2_7, EOL,
        v3_0, v3_1, v3_2, v3_3, v3_4, v3_5, v3_6, v3_7, v3_8, v3_9,
        Latest };
    explicit Version(uint8_t major, uint8_t minor);
    explicit Version(versions current);
    Version(const Version &other);
    virtual ~Version();

    /// get/set version
    void setCurrent(versions current);
    versions current() const;

    /// get the version, if ver = Invalid get the current selected version as string
    std::string asString() const;
    static std::string asString(versions current);
    /// get version value from str
    static versions strToVersion(const std::string &versionStr);
    /// get a list of all available versions
    static std::map<versions, const std::string> availableVersions();
    /// get the version as major or minor version
    uint8_t majorVersion() const;
    uint8_t minorVersion() const;
private:
    versions m_version;
};

// ------------------------------------------------------------------------------------

class TokenIterator
{
    Token *m_start;
public:
    explicit TokenIterator(Token *startTok);
    TokenIterator(const TokenIterator &other);
    ~TokenIterator();
    typedef Token                   value_type;
    typedef std::ptrdiff_t          difference_type;
    typedef Token*                  pointer;
    typedef Token&                  reference;
    typedef std::bidirectional_iterator_tag iterator_category;
    Token &operator*() const { return *m_start; }
    Token *operator->() const { return m_start; }
    bool operator==(const TokenIterator& other) const {
        return m_start == other.m_start;
    }
    bool operator!=(const TokenIterator& other) const {
        return !(*this == other);
    }
    bool operator==(const Token &otherTok) const {
        return m_start == &otherTok;
    }
    bool operator!=(const Token *otherTok) const {
        return !(*this == *otherTok);
    }
    TokenIterator &operator++(); // preincrement '++it'
    TokenIterator operator++(int); // postincrement 'it++'
    TokenIterator &operator--(); // predecrement '--it'
    TokenIterator operator--(int); // postdecrement 'it--'
};

// -------------------------------------------------------------------------------------

class Token
{
public:
    enum Type {
        //**
        //* Any token added or removed here must also be added or removed in tokenToCstr
        // all these tokens must be in grouped order ie: All operators grouped, numbers grouped etc
        T_Undetermined     = 0,     // Parser looks tries to figure out next char also Standard text
        // python
        T_Indent,
        T_Dedent,
        T_Comment,     // Comment begins with #
        T_SyntaxError,
        T_IndentError,     // to signify that we have a indent error, set by PythonSourceRoot

        // numbers
        T__NumbersStart,
        T__NumbersIntStart = T__NumbersStart,
        T_NumberHexInt,
        T_NumberBinInt,
        T_NumberOctInt,     // starting with 0 in py2, 0oxx in py3, ie 011 = 9, different color to let it stand out
        T_NumberDecInt,     // Normal number
        T__NumbersIntEnd,
        T_NumberFloat,
        T__NumbersEnd,

        // strings
        T__LiteralsStart = T__NumbersEnd,
        T_LiteralDblQuote,           // String literal beginning with "
        T_LiteralSglQuote,           // Other string literal beginning with '
        T__LiteralsMultilineStart,
        T_LiteralBlockDblQuote,      // Block comments beginning and ending with """
        T_LiteralBlockSglQuote,      // Other block comments beginning and ending with '''
        T__LiteralsMultilineEnd,
        T__LiteralsEnd,

        // Keywords, some may be as operators instead, see is, in etc
        T__KeywordsStart = T__LiteralsEnd,
        T_Keyword,
        T_KeywordClass,
        T_KeywordDef,
        T_KeywordImport,
        T_KeywordFrom,
        T_KeywordAs,
        T_KeywordYield,
        T_KeywordReturn,
        T_KeywordRaise,
        T_KeywordWith,
        T_KeywordGlobal,
        T_KeywordLambda,
        T_KeywordPass,
        T_KeywordAssert,
        T__KeywordIfBlockStart,
        T_KeywordIf,
        T_KeywordElIf,
        T_KeywordElse,
        T__KeywordIfBlockEnd,
        T__KeywordLoopStart = T__KeywordIfBlockEnd,
        T_KeywordFor,
        T_KeywordWhile,
        T_KeywordBreak,
        T_KeywordContinue,
        T__KeywordTryBlockStart,
        T_KeywordTry,
        T_KeywordExcept,
        T_KeywordFinally,
        T__KeywordTryBlockEnd,
        T__KeywordsLoopEnd,
        T__KeywordsEnd = T__KeywordsLoopEnd,
        // leave some room for future keywords

        // operators
        // https://www.w3schools.com/python/python_operators.asp
        // https://docs.python.org/3.7/library/operator.html
        // artihmetic operators
        T__OperatorStart            = T__KeywordsEnd,   // operators start
        T__OperatorArithmeticStart = T__OperatorStart,
        T_OperatorPlus,              // +,
        T_OperatorMinus,             // -,
        T_OperatorMul,               // *,
        T_OperatorExponential,       // **,
        T_OperatorDiv,               // /,
        T_OperatorFloorDiv,          // //,
        T_OperatorModulo,            // %,
        T_OperatorMatrixMul,         // @
        T__OperatorArithmeticEnd,

        // bitwise operators
        T__OperatorBitwiseStart = T__OperatorArithmeticEnd,
        T_OperatorBitShiftLeft,       // <<,
        T_OperatorBitShiftRight,      // >>,
        T_OperatorBitAnd,             // &,
        T_OperatorBitOr,              // |,
        T_OperatorBitXor,             // ^,
        T_OperatorBitNot,             // ~,
        T__OperatorBitwiseEnd,

        // assignment operators
        T__OperatorAssignmentStart   = T__OperatorBitwiseEnd,
        T_OperatorEqual,              // =,
        T_OperatorWalrus,             // := introduced in py 3.8
        T_OperatorPlusEqual,          // +=,
        T_OperatorMinusEqual,         // -=,
        T_OperatorMulEqual,           // *=,
        T_OperatorDivEqual,           // /=,
        T_OperatorModuloEqual,        // %=
        T_OperatorFloorDivEqual,      // //=
        T_OperatorExpoEqual,          // **=
        T_OperatorMatrixMulEqual,     // @= introduced in py 3.5

        // assignment bitwise
        T__OperatorAssignBitwiseStart,
        T_OperatorBitAndEqual,           // &=
        T_OperatorBitOrEqual,            // |=
        T_OperatorBitXorEqual,           // ^=
        T_OperatorBitNotEqual,           // ~=
        T_OperatorBitShiftRightEqual,    // >>=
        T_OperatorBitShiftLeftEqual,     // <<=
        T__OperatorAssignBitwiseEnd,
        T__OperatorAssignmentEnd = T__OperatorAssignBitwiseEnd,

        // compare operators
        T__OperatorCompareStart  = T__OperatorAssignmentEnd,
        T_OperatorCompareEqual,        // ==,
        T_OperatorNotEqual,            // !=,
        T_OperatorLessEqual,           // <=,
        T_OperatorMoreEqual,           // >=,
        T_OperatorLess,                // <,
        T_OperatorMore,                // >,
        T__OperatorCompareKeywordStart,
        T_OperatorAnd,                 // 'and'
        T_OperatorOr,                  // 'or'
        T_OperatorNot,                 // ! or 'not'
        T_OperatorIs,                  // 'is'
        T_OperatorIn,                  // 'in'
        T__OperatorCompareKeywordEnd,
        T__OperatorCompareEnd = T__OperatorCompareKeywordEnd,

        // special meaning
        T__OperatorParamStart = T__OperatorCompareEnd,
        T_OperatorVariableParam,       // * for function parameters ie (*arg1)
        T_OperatorKeyWordParam,        // ** for function parameters ir (**arg1)
        T__OperatorParamEnd,
        T__OperatorEnd = T__OperatorParamEnd,

        // delimiters
        T__DelimiterStart = T__OperatorEnd,
        T_Delimiter,                       // all other non specified
        T_DelimiterOpenParen,              // (
        T_DelimiterCloseParen,             // )
        T_DelimiterOpenBracket,            // [
        T_DelimiterCloseBracket,           // ]
        T_DelimiterOpenBrace,              // {
        T_DelimiterCloseBrace,             // }
        T_DelimiterPeriod,                 // .
        T_DelimiterComma,                  // ,
        T_DelimiterColon,                  // :
        T_DelimiterSemiColon,              // ;
        T_DelimiterEllipsis,               // ...
        // metadata such def funcname(arg: "documentation") ->
        //                            "returntype documentation":
        T_DelimiterArrowR,               // -> might also be ':' inside arguments

        // Text line specific
        T__DelimiterTextLineStart,
        T_DelimiterBackSlash,
                                           // when end of line is escaped like so '\'
        T_DelimiterNewLine,                // each new line
        T__DelimiterTextLineEnd,
        T__DelimiterEnd = T__DelimiterTextLineEnd,

        // identifiers
        T__IdentifierStart = T__DelimiterEnd,
        T__IdentifierVariableStart = T__IdentifierStart,
        T_IdentifierUnknown,                 // name is not identified at this point
        T_IdentifierDefined,                 // variable is in current context
        T_IdentifierSelf,                    // specialcase self
        T_IdentifierBuiltin,                 // its builtin in class methods
        T__IdentifierVariableEnd,

        T__IdentifierImportStart = T__IdentifierVariableEnd,
        T_IdentifierModule,                  // its a module definition
        T_IdentifierModulePackage,           // identifier is a package, ie: root for other modules
        T_IdentifierModuleAlias,             // alias for import. ie: import Something as Alias
        T_IdentifierModuleGlob,              // from mod import * <- glob
        T__IdentifierImportEnd,

        T__IdentifierDeclarationStart = T__IdentifierImportEnd,
        T__IdentifierFrameStartTokenStart = T__IdentifierDeclarationStart,
        T_IdentifierFunction,                // its a function definition
        T_IdentifierMethod,                  // its a method definition
        T_IdentifierClass,                   // its a class definition
        T_IdentifierSuperMethod,             // its a method with name: __**__
        T_IdentifierDefUnknown,              // before system has determined if its a
                                             // method or function yet
        T__IdentifierFrameStartTokenEnd,
        T_IdentifierDecorator,               // member decorator like: @property


        T_IdentifierNone,                    // The None keyword
        T_IdentifierTrue,                    // The bool True
        T_IdentifierFalse,                   // The bool False
        T_IdentifierInvalid,                 // The identifier could not be bound to any other variable
                                             // or it has some error, such as calling a non callable
        T__IdentifierDeclarationEnd,
        T__IdentifierEnd = T__IdentifierDeclarationEnd,

        // metadata such def funcname(arg: "documentaion") -> "returntype documentation":
        T_MetaData = T__IdentifierEnd,

        // these are inserted by PythonSourceRoot
        T_BlockStart, // indicate a new block ie if (a is True):
                      //                                       ^
        T_BlockEnd,   // indicate block end ie:    dosomething
                      //                        dosomethingElse
                      //                       ^
        T_Invalid, // let compiler decide last +1
        T__EndOfTokensMarker,

        // console specific
        T__SpecialTokensStart,
        T_PythonConsoleOutput     = 1000,
        T_PythonConsoleError      = 1001,
        T__SpecialTokensEnd
    };
    /// returns a string representation of token type
    static const char* tokenToCStr(Token::Type tokType);
    /// returns the correct token type from given str
    static Type strToToken(const std::string &tokName);

    explicit Token(Type tokType, uint16_t startPos,
                   uint16_t endPos, uint32_t customMask,
                   TokenLine *ownerLine);
    Token(const Token &other);
    ~Token();
    bool operator==(const Token &rhs) const
    {
        return line() == rhs.line() && m_type == rhs.m_type &&
               m_startPos == rhs.m_startPos && m_endPos == rhs.m_endPos;
    }
    bool operator!=(const Python::Token &rhs) const {
        return !(*this == rhs);
    }
    bool operator > (const Token &rhs) const
    {
        int myLine = line(), rhsLine = rhs.line();
        if (myLine > rhsLine)
            return true;
        if (myLine < rhsLine)
            return false;
        if (m_startPos == rhs.m_startPos)
            return m_previous == &rhs;
        return m_startPos > rhs.m_startPos;
    }
    bool operator < (const Token &rhs) const { return (rhs > *this); }
    bool operator <= (const Token &rhs) const { return !(rhs < *this); }
    bool operator >= (const Token &rhs) const { return !(*this < rhs); }

    /// for traversing tokens in document
    /// returns token or nullptr if at end/begin
    Token *next() const { return m_next; }
    Token *previous() const { return m_previous; }

    // public properties
    Type type() const { return m_type; }
    void changeType(Type tokType);
    uint16_t startPos() const { return m_startPos; }
    int16_t startPosInt() const { return static_cast<int16_t>(m_startPos); }
    uint16_t endPos() const { return m_endPos; }
    int16_t endPosInt() const { return  static_cast<int16_t>(m_endPos); }

    uint16_t textLength() const { return m_endPos - m_startPos; }

    /// token which has textlength has a hash for its text
    /// speeds up compares
    inline std::size_t hash() const { return m_hash; }

    /// pointer to our father textBlockData
    Python::TokenList *ownerList() const;
    Python::TokenLine *ownerLine() const;
    /// get the text as it is in the source
    const std::string text() const;
    /// get the content from token, might differ from text if its a string
    /// extracts the content from string """content""" <- returns content
    const std::string content() const;
    int line() const;

    // tests
    bool isNumber() const;
    bool isInt() const;
    bool isFloat() const;
    bool isString() const;
    /// ie. constructed with r"str"
    bool isStringRaw() const;
    /// ie. constructed with b"str"
    bool isStringBytes() const;
    /// ie constructed with u""
    bool isStringUnicode() const;
    /// ie constructed with f""
    bool isStringFormat() const;
    /// strings spans over many lines
    bool isMultilineString() const;
    bool isBoolean() const;
    bool isKeyword() const;
    bool isOperator() const;
    bool isOperatorArithmetic() const;
    bool isOperatorBitwise() const;
    bool isOperatorAssignment() const;
    bool isOperatorAssignmentBitwise() const;
    bool isOperatorCompare() const;
    bool isOperatorCompareKeyword() const;
    bool isOperatorParam() const;
    bool isDelimiter() const;
    bool isIdentifier() const;
    bool isIdentifierVariable() const;
    bool isIdentifierDeclaration() const;
    bool isIdentifierFrameStart() const;
    bool isNewLine() const; // might be escaped this checks for that
    bool isInValid() const;
    bool isUndetermined() const;
    bool isImport() const;

    /// returns true if token represents code (not just T_Indent, T_NewLine etc)
    /// comment is also ignored
    bool isCode() const;
    /// like isCode but returns true if it is a comment
    bool isText() const;


    /// all references get a call to PythonSourceListNode::tokenDeleted
    void attachReference(Python::TokenWrapperBase *tokWrapper);
    void detachReference(Python::TokenWrapperBase *srcListNode);

    /// get the custom option idx mask
    uint32_t optionMask() const { return m_customMask; }


private:
    Type m_type;
    uint16_t m_startPos, m_endPos;
    uint32_t m_customMask; // to store custom data on this token
    std::size_t m_hash;
    std::list<Python::TokenWrapperBase*> m_wrappers;

    Token *m_next,
          *m_previous;
    TokenLine *m_ownerLine;

#ifdef BUILD_PYTHON_DEBUGTOOLS
    std::string m_nameDbg;
    uint m_lineDbg;
#endif
    friend class Python::TokenList;
    friend class Python::TokenLine;
    friend class Python::Lexer;
};

// ----------------------------------------------------------------------------------------

/// Handles all tokens for a single file
/// implements logic for 2 linked lists, Tokens and TokenLines
/// TokenList is container for each token in each row
class TokenList {
    Python::Token *m_first,
                  *m_last;
    Python::TokenLine *m_firstLine,
                      *m_lastLine;
    uint32_t m_size;
    Python::Lexer *m_lexer;
    explicit TokenList(const TokenList &other);
    TokenList& operator=(const TokenList &other);
public:
    explicit TokenList(Python::Lexer *lexer);
    virtual ~TokenList();

    // owner
    Python::Lexer *lexer() const { return m_lexer; }

    // accessor methods
    Python::Token *front() const { return m_first; }
    Python::Token *back() const { return m_last; }
    TokenIterator begin() const { return TokenIterator(m_first); }
    TokenIterator rbegin() const { return TokenIterator(m_last); }
    TokenIterator end() const { return TokenIterator(nullptr); }
    TokenIterator rend() const { return TokenIterator(nullptr); }
    Python::Token *operator[] (int32_t idx);

    // info
    bool empty() const {
        return m_first == nullptr && m_last == nullptr &&
               m_firstLine == nullptr && m_lastLine == nullptr;
    }
    uint32_t count() const;
    size_t size() const { return static_cast<size_t>(m_size); }
    static uint32_t max_size() { return 20000000u; }

    // modifiers for tokens
    void clear(bool deleteLine = true);

    //lines
    /// get the first line for this list/document
    Python::TokenLine *firstLine() const { return m_firstLine; }
    /// get the last line for this list/document
    Python::TokenLine *lastLine() const { return m_lastLine; }
    /// get the line at lineNr, might be negative for reverse. ie: -1 == lastLine
    /// lineNr is 0 based (ie. first row == 0), lastRow == -1
    Python::TokenLine *lineAt(int32_t lineNr) const;
    /// get total number of lines
    uint32_t lineCount() const;

    /// swap line, old line is deleted with all it containing tokens
    /// lineNr is 0 based (ie. first row == 0), lastRow == -1
    void swapLine(int32_t lineNr, Python::TokenLine *swapIn);

    /// swap line, old line must be deleted by caller
    /// list takes ownership of swapInLine and its tokens
    void swapLine(Python::TokenLine *swapOut, Python::TokenLine *swapIn);

    /// insert line at lineNr, list takes ownership of line and its tokens
    /// lineNr is 0 based (ie. first row == 0), last row == -1
    void insertLine(int32_t lineNr, Python::TokenLine *lineToInsert);
    /// insert line directly after previousLine, list takes ownership of line and its tokens
    void insertLine(Python::TokenLine *previousLine, Python::TokenLine *insertLine);

    /// inserts line at end of lines list
    /// list takes ownership of line and its tokens
    void appendLine(Python::TokenLine *lineToPush);

    /// remove line at lineNr and deletes the line
    /// lineNr is 0 based (ie. first row == 0), last row == -1
    void removeLine(int32_t lineNr, bool deleteLine = true);
    /// remove line from this list and deletes the line
    void removeLine(Python::TokenLine *lineToRemove, bool deleteLine = true);

    // should modify through TokenLine
private:
    void push_back(Python::Token *tok);
    void push_front(Python::Token *tok);
    Python::Token *pop_back();
    Python::Token *pop_front();
    /// inserts token into list, list takes ownership and may delete token
    void insert(Token *previousTok, Python::Token *insertTok);

    /// removes tok from list
    /// if deleteTok is true it also deletes these tokens (mem. free)
    /// else caller must delete tok
    bool remove(Python::Token *tok, bool deleteTok = true);

    /// removes tokens from list
    /// removes from from tok up til endTok, endTok is the first token not removed
    /// if deleteTok is true it also deletes these tokens (mem. free)
    /// else caller must delete tok
    bool remove(Python::Token *tok,
                Python::Token *endTok,
                bool deleteTok = true);

    // when inserting or removing line, change the rests line
    void incLineCount(Python::TokenLine *firstLineToInc) const;
    void decLineCount(Python::TokenLine *firstLineToDec) const;

private:
    friend class Python::TokenLine;
};

// -------------------------------------------------------------------------------------------

class TokenLine {
    Python::TokenList *m_ownerList;
    Python::Token *m_frontTok, *m_backTok;
    Python::TokenLine *m_nextLine, *m_previousLine;
    std::string m_text;
    Token::Type m_endStateOfLastPara; // the lexer state the ended this line
                                     // multiline block strings can span over several lines
    int m_line;

protected:
    // these are protected so that restore from file can subclass and manipulate
    std::shared_ptr<Python::TokenScanInfo> m_tokenScanInfo;

    std::list<int> m_unfinishedTokenIndexes; // index to m_tokens where a undetermined is at
                                            //  (so context parser can determine it later)

    uint16_t m_indentCharCount; // as spaces NOTE according to python documentation a tab is 8 spaces

    int16_t m_parenCnt, m_bracketCnt, m_braceCnt, m_blockStateCnt;
    bool m_isParamLine;
    bool m_isContinuation; // this line is a continuation of previous line
                           // indent is not used on this line

public:
    explicit TokenLine(Python::Token *startTok,
                       const std::string &text);
    TokenLine(const Python::TokenLine &other);
    virtual ~TokenLine();

    // line info methods
    /// returns the container that stores this line
    Python::TokenList *ownerList() const { return m_ownerList; }

    /// returns this lines linenr, 0 based
    uint32_t lineNr() const;
    /// returns this lines text content (without line ending chars)
    const std::string &text() const { return m_text; }

    /// returns the number of tokens
    uint count() const;
    size_t size() const { return static_cast<size_t>(count()); }

    /// true if line is empty
    bool empty() const { return m_frontTok == nullptr; }

    /// returns the number of chars that starts this line
    uint16_t indent() const;

    /// isCodeLine checks if line has any code
    /// lines with only indent or comments return false
    /// return true if line has code
    bool isCodeLine() const;

    /// true if this line is within a prarameter list
    /// ie def func(param1,.....
    ///             param2 <- line continued paramline
    bool isParamLine() const { return m_isParamLine; }

    /// true if this line is a continuation from previous line
    ///  indent is not in effect on this line
    /// ie {prop1: blah, ....
    ///     prop2: baz, <-  line is continued from previous line
    bool isContinuation() const { return m_isContinuation; }

    /// get the lexer state that ended this line
    Token::Type endState() const { return m_endStateOfLastPara; }

    /// number of parens in this line
    /// '(' = parenCnt++
    /// ')' = parenCnt--
    /// Can be negative if this line has more closing parens than opening ones
    int16_t parenCnt() const { return  m_parenCnt; }
    /// number of brackets '[' int this line, see as parenCnt description
    int16_t bracketCnt() const { return m_bracketCnt; }
    /// number of braces '{' in this line, see parenCnt for description
    int16_t braceCnt() const { return m_braceCnt; }

    /// blockState tells if this block starts a new block,
    ///         such as '{' in C like languages or ':' in python
    ///         +1 = blockstart, -1 = blockend, -2 = 2 blockends ie '}}'
    virtual int blockState() const { return m_blockStateCnt; }
    virtual int incBlockState() { return ++m_blockStateCnt; }
    virtual int decBlockState() { return --m_blockStateCnt; }

    bool operator== (const Python::TokenLine &rhs) const;


    // accessor methods
    Python::Token *front() const { return m_frontTok; }
    Python::Token *back() const { return m_backTok; }
    TokenIterator begin() const {
        return TokenIterator(m_frontTok);
    }
    TokenIterator rbegin() const {
        return TokenIterator(m_backTok);
    }
    TokenIterator end() const {
        return TokenIterator(m_backTok ? m_backTok->m_next : nullptr);
    }
    TokenIterator rend() const {
        return TokenIterator(m_frontTok ? m_frontTok->m_previous : nullptr);
    }

    /// gets the nextline sibling
    Python::TokenLine *nextLine() const { return m_nextLine; }
    Python::TokenLine *previousLine() const { return m_previousLine; }
    /// returns the token at idx position in line or nullptr
    Python::Token *operator[] (int idx) const;
    /// returns the token at idx char in line str
    Python::Token *tokenAt(int idx) const;
    /// return what pos tok is at, returns the idx, ...[idx] for tok or -1 if not found
    int tokenPos(const Python::Token *tok) const;

    /// returns a list with all tokens in this line
    const std::vector<Python::Token*> tokens() const;

    /// returns a list with all undetermined or invalid tokens in this line
    std::list<int> &unfinishedTokens();


    /// findToken searches for token in this block
    /// tokType = needle to search for
    /// searchFrom start position in line to start the search
    ///                   if negative start from the back ie -10 searches from pos 10 to 0
    /// returns pointer to token if found, else nullptr
    const Python::Token* findToken(Python::Token::Type tokType,
                                   int searchFrom = 0) const;

    /// firstTextToken lookup first token that is a text (comment or code) token or nullptr
    const Python::Token *firstTextToken() const;
    /// firstCodeToken lookup first token that is a code token or nullptr
    const Python::Token *firstCodeToken() const;



    // line/list mutator methods
    void push_back(Python::Token *tok);
    void push_front(Python::Token *tok);
    /// removes last token in this line
    /// caller taken ownership and must delete token
    Python::Token *pop_back();
    /// removes first token in this line
    /// caller taken ownership and must delete token
    Python::Token *pop_front();
    /// inserts token into list, returns position it is stored at
    void insert(Python::Token *tok);
    /// inserts token into list, returns position it is stored at
    void insert(Python::Token *beforeTok, Python::Token *insertTok);

    /// removes tok from list
    /// if deleteTok is true it also deletes these tokens (mem. free)
    bool remove(Python::Token *tok, bool deleteTok = true);

    /// removes tokens from list
    /// removes from from tok up til endTok, endTok is the first token not removed
    /// if deleteTok is true it also deletes these tokens (mem. free)
    bool remove(Python::Token *tok,
                Python::Token *endTok,
                bool deleteTok = true);

    // these are accessed from Python::Tokenizer
    /// create a token with tokType and append to line
    Python::Token *newDeterminedToken(Python::Token::Type tokType, uint16_t startPos,
                                      uint16_t len, uint32_t customMask);
    /// this insert should only be used by PythonSyntaxHighlighter
    /// stores this token id as needing a parse tree lookup to determine the tokenType
    Python::Token *newUndeterminedToken(Python::Token::Type tokType, uint16_t startPos,
                                        uint16_t len, uint32_t customMask);


    ///tokenScanInfo contains messages for a specific code line/col
    /// if initScanInfo is true it also creates a container if not existing
    std::shared_ptr<TokenScanInfo> tokenScanInfo(bool initScanInfo = false);

    /// set a indent error message
    void setIndentErrorMsg(const Python::Token *tok, const std::string &msg);
    void setLookupErrorMsg(const Python::Token *tok, const std::string &msg  = std::string());
    void setSyntaxErrorMsg(const Python::Token *tok, const std::string &msg);
    void setMessage(const Python::Token *tok, const std::string &msg);

private:
    /// needed because offset when multiple inheritance
    /// returns the real pointer to this instance, not to subclass
    Python::TokenLine *instance() const {
        return static_cast<Python::TokenLine*>(
                    const_cast<Python::TokenLine*>(this));
    }

    friend class Python::TokenList;
    friend class Python::Lexer;
};

// ------------------------------------------------------------------------------------------

class TokenScanInfo
{
public:
    /// types of messages, sorted in priority, higher idx == higher prio
    enum MsgType { Invalid, AllMsgTypes, Message, Warning, Issue,
                   LookupError, IndentError, SyntaxError,
                   };
    struct ParseMsg {
    public:
        explicit ParseMsg(const std::string &message, const Python::Token *tok, MsgType type);
        ~ParseMsg();
        const std::string msgTypeAsString() const;
        static const std::string msgTypeAsString(MsgType msgType);
        static MsgType strToMsgType(const std::string &msgTypeStr);
        const std::string message() const;
        const Python::Token *token() const;
        MsgType type() const;

    private:
        std::string m_message;
        const Python::Token *m_token;
        MsgType m_type;
        Version m_version;
    };
    explicit TokenScanInfo();
    virtual ~TokenScanInfo();
    /// create a new parsemsg attached to token
    void setParseMessage(const Python::Token *tok, const std::string &msg, MsgType type);
    /// get all parseMessage attached to token
    /// filter by type
    const std::list<const ParseMsg*> parseMessages(const Python::Token *tok,
                                                      MsgType type = AllMsgTypes) const;
    /// remove all messages attached to tok, returns number of deleted parseMessages
    /// filter by type
    int clearParseMessages(const Python::Token *tok, MsgType filterType = MsgType::AllMsgTypes);

    std::list<const ParseMsg*> allMessages() const;

private:
    std::list<const ParseMsg*> m_parseMsgs;
};

// ------------------------------------------------------------------------------------------

/// this class attaches to pointer token.
/// When token is deleted, gets notified when token is deleted
/// Base class for TokenWrapper
class TokenWrapperBase {
    friend class Token;
    virtual void tokenDeleted() = 0;
public:
    explicit TokenWrapperBase(Python::Token *tok);
    virtual ~TokenWrapperBase();
    Python::Token *token() const { return m_token; }
    void setToken(Python::Token *tok);
protected:
    Python::Token *m_token;
};

// ------------------------------------------------------------------------------------------

/// this class attaches to pointer token.
/// When token is deleted, gets notified when token is deleted
template<typename TOwner>
class TokenWrapper : public TokenWrapperBase
{
    friend class Token;
    TOwner *m_owner;
public:
    typedef void (TOwner::*tokenDelCallbackPtr)(TokenWrapperBase *wrapper);
    explicit TokenWrapper(Python::Token *token, TOwner *owner, tokenDelCallbackPtr callback) :
             TokenWrapperBase(token),
        m_owner(owner),
        m_callBackPtr(callback)
    { }
    ~TokenWrapper() override { }
private:
    tokenDelCallbackPtr m_callBackPtr;
    void tokenDeleted() override
    {
        if (m_owner)
            (m_owner->*m_callBackPtr)(this);
        m_token = nullptr;
    }
};

// ----------------------------------------------------------------------------------------

// use this class instead of TokenWrapper if you intend to inherit TokenWrapperBase
class TokenWrapperInherit : public TokenWrapperBase
{
    friend class Token;
    void tokenDeleted() override;
public:
    explicit TokenWrapperInherit(Python::Token *token);
    ~TokenWrapperInherit() override;


    /// gets text for token (gets from document)
    const std::string text() const;

    /// gets the hash for this tokens text
    virtual std::size_t hash() const;

    virtual void tokenDeletedCallback() = 0;
};

} // namespace Python

#endif // PYTHONTOKEN_H
