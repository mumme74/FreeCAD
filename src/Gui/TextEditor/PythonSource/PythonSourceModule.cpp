#include "PythonSourceModule.h"
#include "PythonSourceParser.h"
#include "PythonLexer.h"
#include <iostream>

DBG_TOKEN_FILE


Python::SourceModule::SourceModule(Python::SourceRoot *root,
                                   Lexer *lexer) :
    Python::SourceListParentBase(this),
    m_root(root),
    m_rootFrame(this, this),
    m_lexer(lexer)
{
}

Python::SourceModule::~SourceModule()
{
}

void Python::SourceModule::scanFrame(Python::Token *tok)
{
    DEFINE_DBG_VARS
    DBG_TOKEN(tok)

    Python::SourceIndent indent = currentBlockIndent(tok);
    const Python::SourceFrame *frm = getFrameForToken(tok, &m_rootFrame);
    auto parser = Python::SourceParser::instance();
    parser->setActiveModule(this);
    parser->setActiveFrame(const_cast<Python::SourceFrame*>(frm));
    parser->scanFrame(tok, indent);
}

void Python::SourceModule::scanLine(Python::Token *tok)
{
    DEFINE_DBG_VARS
    DBG_TOKEN(tok)

    if (!tok)
        return;

    Python::SourceIndent indent = currentBlockIndent(tok);
    const Python::SourceFrame *frm = getFrameForToken(tok, &m_rootFrame);
    auto parser = Python::SourceParser::instance();
    parser->setActiveModule(this);
    parser->setActiveFrame(const_cast<Python::SourceFrame*>(frm));
    parser->scanLine(tok, indent);
}

Python::SourceIndent Python::SourceModule::currentBlockIndent(const Python::Token *tok) const
{
    DEFINE_DBG_VARS
    DBG_TOKEN(tok)

    Python::SourceIndent ind;
    if (!tok)
        return ind; // is invalid here

    const Python::SourceFrame *frm = getFrameForToken(tok, &m_rootFrame);
    const Python::Token *beginTok = frm->token() ? frm->token() : tok;

    uint frameIndent = 0,
         currentIndent = 0;

    // root frame should always have 0 as indent
    if (frm->parentFrame() == nullptr)
        frameIndent = 0;
    else
        frameIndent = beginTok->ownerLine()->indent();

    //beginTok = tok;
    DBG_TOKEN(beginTok)

    std::list<uint> traversedBlocks;
    // rationale here is to back up until we find a line with less indent
    // def func(arg1):     <- frameIndent
    //     var1 = None     <- previousBlockIndent
    //     if (arg1):      <- triggers change in currentBlockIndent
    //         var1 = arg1 <- currentBlockIndent
    Python::TokenLine *tokLine = tok->ownerLine(),
                      *lastLine = tok->ownerLine();
    uint indent = tokLine->indent();
    int guard = 1000;
    while (tokLine && (guard--) > 0) {
        if (tokLine->isCodeLine()){
            if (indent >= tokLine->indent()) {
                // look for ':' if not found its a syntax error unless its a comment
                beginTok = tokLine->findToken(Python::Token::T_DelimiterColon, -1);
                DBG_TOKEN(beginTok)
                if (beginTok) {
//                    if (beginTok->next() &&
//                        beginTok->next()->type() != Python::Token::T_BlockStart)
//                    {
//                        insertBlockStart(beginTok);
//                    }

                    traversedBlocks.push_back(tokLine->indent());
                    // we found it
                    currentIndent = _currentBlockIndent(beginTok);
                    if (currentIndent >= frameIndent)
                        traversedBlocks.push_back(currentIndent);
                    break;
                } else if (tokLine->indent() < indent) {
                    //lastLine->setSyntaxError(lastLine->front(), "Blockstart without ':'");
                    break;
                }
            }
        }
        lastLine = tokLine;
        tokLine = tokLine->previousLine();
    }

    if (traversedBlocks.empty()) {
        if (guard == 0)
            std::clog << "scanFrame loopguard" << std::endl;
        // we didn't find any ':', must be in root frame
        assert(frm->parentFrame() == nullptr && frameIndent == 0 && "Should be in root frame here!");
    }
    if (frameIndent > 0 || traversedBlocks.size() > 0){
        for(uint indnt : traversedBlocks)
            ind.pushFrameBlock(frameIndent, indnt);
    }
    return ind;
}

uint Python::SourceModule::_currentBlockIndent(const Python::Token *tok) const
{
    DEFINE_DBG_VARS

    const Python::Token *beginTok = tok;
    DBG_TOKEN(beginTok)

    // get indent of first line after newline at frame start
    int newLines = 0;
    while(beginTok && newLines < 2) {
        switch (beginTok->type()){
        case Python::Token::T_DelimiterBackSlash:
            --newLines; // newline escaped
            break;
//        case Python::Token::T_DelimiterColon:
//            insertBlockStart(beginTok);
//            break;
        case Python::Token::T_DelimiterNewLine:
            ++newLines;
            break;
        case Python::Token::T_Comment:
            if (newLines)
                --newLines; // don't count a comment as a indent
            break;
        default:
            if (newLines > 0 && beginTok->isCode())
                return beginTok->ownerLine()->indent();
        }
        NEXT_TOKEN(beginTok)
    }
    // we didn't find any block
    return 0;
}

void Python::SourceModule::tokenTypeChanged(const Python::Token *tok) const
{
    if (m_lexer)
        m_lexer->tokenTypeChanged(tok);
}

int Python::SourceModule::compare(const Python::SourceListNodeBase *left,
                                const Python::SourceListNodeBase *right) const
{
    const Python::SourceModule *l = dynamic_cast<const Python::SourceModule*>(left),
                             *r = dynamic_cast<const Python::SourceModule*>(right);
    assert(l != nullptr && r != nullptr && "Invalid class stored as Python::SourceModule");
    if (l->moduleName() < r->moduleName())
        return +1;
    else if (l->moduleName() > r->moduleName())
        return -1;
    else {
        if (l->filePath() < r->filePath())
            return -1;
        if (l->filePath() > r->filePath())
            return +1;
        assert(false && "Trying to store the same module twice");
    }
}

const Python::SourceFrame *Python::SourceModule::getFrameForToken(const Python::Token *tok,
                                                        const Python::SourceFrame *parentFrame) const
{
    DEFINE_DBG_VARS

    // find the frame associated with this token
    if (parentFrame->empty() || !tok)
        return parentFrame; // we have no children, this is it!

    Python::SourceFrame *childFrm = nullptr;
    Python::SourceListNodeBase *itm = nullptr;

    for (itm = parentFrame->begin();
         itm != parentFrame->end();
         itm = itm->next())
    {
        childFrm = dynamic_cast<Python::SourceFrame*>(itm);
        assert(childFrm != nullptr && "Wrong datatype stored in Python::SourceFrame");
        if (!childFrm->token())
            continue;
        if (!childFrm->lastToken()) {
            // end token have been deleted, re-parse end token
            uint ind = childFrm->token()->ownerLine()->indent();
            Python::TokenLine *tokLine = childFrm->token()->ownerLine();
            while ((tokLine = tokLine->nextLine())) {
                if (tokLine->isCodeLine() && tokLine->indent() <= ind) {
                    while((tokLine = tokLine->previousLine())) {
                        if (tokLine->isCodeLine()) {
                            childFrm->setLastToken(tokLine->back());
                            break;
                        }
                    }
                    break;
                } else if (!tokLine->nextLine()) {
                    // we might be at a row with no tokens
                    while(tokLine->empty())
                        tokLine = tokLine->previousLine();
                    if (tokLine && tokLine->count())
                        childFrm->setLastToken(tokLine->back());
                    break;
                }
            }
        }
        // we might have not found it
        if (!childFrm->lastToken())
            continue;

        if (*tok >= *childFrm->token() && *tok <= *childFrm->lastToken())
        {
            // find recursivly
            if (!childFrm->empty())
                return getFrameForToken(tok, dynamic_cast<const Python::SourceFrame*>(childFrm));
            // no childs, return ourself
            return childFrm;
        }
    }

    return parentFrame;
}

/*
void Python::SourceModule::insertBlockStart(const Python::Token *colonTok) const
{
    if (colonTok->next() && colonTok->next()->type() != Python::Token::T_BlockStart) {
        colonTok->ownerLine()->insert(
                    new Python::Token(Python::Token::T_BlockStart,
                                      colonTok->startPos(), colonTok->startPos(),
                                      colonTok->ownerLine()));
        //colonTok->ownerLine()->incBlockState();
    }
}

Python::Token *Python::SourceModule::insertBlockEnd(const Python::Token *newLineTok) const
{
    if (newLineTok->previous() &&
        newLineTok->previous()->type() != Python::Token::T_BlockEnd)
    {
        int newPos = newLineTok->ownerLine()->insert(newLineTok->previous(),
                                        new Python::Token(Python::Token::T_BlockEnd,
                                                newLineTok->startPos(), newLineTok->endPos(),
                                                          newLineTok->ownerLine()));
        //newLineTok->ownerLine()->decBlockState();
        return newLineTok->ownerLine()->tokenAt(newPos);
    }
    return nullptr;
}
*/

void Python::SourceModule::reparseInvalidTokens()
{
    Python::SourceFrame *frm = &m_rootFrame;
    Python::Token *tok = const_cast<Python::Token*>(frm->token());
    Python::TokenLine *line = tok->ownerLine();
    const Python::TokenLine *endLine = m_rootFrame.lastToken()->ownerLine();

    uint guard = 2000000;
    while (line && line->lineNr() <= endLine->lineNr() && (--guard)) {
        auto it = line->unfinishedTokens().begin();
        while (it != line->unfinishedTokens().end())
        {
            tok = (*line)[*it];
            ++it; // must be done before iterator this item is removed from list
            if (tok && tok->type() == Python::Token::T_IdentifierInvalid) {
                // found the token, look it up
                frm = const_cast<Python::SourceFrame*>(getFrameForToken(tok, &m_rootFrame));
                if (!frm)
                    continue;
                if (Python::SourceParser::instance()->lookupIdentifierReference(tok) &&
                    tok->type() != Python::Token::T_IdentifierInvalid)
                {
                    if (line->tokenScanInfo()) {
                        line->tokenScanInfo()->
                                clearParseMessages(tok, Python::TokenScanInfo::LookupError);
                    }
                }
            }
        }
        line = line->nextLine();
    }
}

/*
void Python::SourceModule::setSyntaxError(const Python::Token *tok, const std::string &parseMessage) const
{
    DEFINE_DBG_VARS

    Python::TokenScanInfo *scanInfo = tok->ownerLine()->tokenScanInfo(true);
    scanInfo->setParseMessage(tok, parseMessage, TokenScanInfo::SyntaxError);

    // create format with default format for syntax error
    const_cast<Python::Token*>(tok)->changeType(Python::Token::T_SyntaxError);
    m_lexer->tokenTypeChanged(tok);

    // set text underline of all text in here
    // move to beginning of text
    const Python::Token *startTok = tok;
    DBG_TOKEN(startTok)

    // first move up to end of code if its a whitespace
    if (!startTok->isCode()) {
        int guard = 100;
        bool hasCode = false;
        while (guard--) {
            NEXT_TOKEN(startTok)
            if (!startTok)
                break;
            if (hasCode) {
                if (!startTok->isCode()) {
                    PREV_TOKEN(startTok)
                    break;
                }
            } else
                hasCode = startTok->isCode();
        }
    }
    // then move down the line til first whitespace
    while(startTok && startTok->isCode()) {
        m_lexer->setSyntaxError(tok);

        // previous token
        if (startTok->previous()) {
            PREV_TOKEN(startTok)
        } else
            break;
    }
}

void Python::SourceModule::setIndentError(const Python::Token *tok) const
{
    DEFINE_DBG_VARS
    DBG_TOKEN(tok)
    Python::TokenScanInfo *scanInfo = tok->ownerLine()->tokenScanInfo(true);

    scanInfo->setParseMessage(tok, "Unexpected indent", TokenScanInfo::IndentError);

    const_cast<Python::Token*>(tok)->changeType(Python::Token::T_SyntaxError);
    tokenTypeChanged(tok);

    m_lexer->setIndentError(tok);
}

void Python::SourceModule::setLookupError(const Python::Token *tok, const std::string &parseMessage) const
{
    DEFINE_DBG_VARS
    DBG_TOKEN(tok)

    Python::TokenScanInfo *scanInfo = tok->ownerLine()->tokenScanInfo(true);

    if (parseMessage.empty())
        scanInfo->setParseMessage(tok, "Can't lookup identifier '" + tok->text() + "'",
                                  TokenScanInfo::LookupError);
    else
        scanInfo->setParseMessage(tok, parseMessage, TokenScanInfo::LookupError);
}

void Python::SourceModule::setMessage(const Python::Token *tok, const std::string &parseMessage) const
{
    DEFINE_DBG_VARS
    DBG_TOKEN(tok)
    Python::TokenScanInfo *scanInfo = tok->ownerLine()->tokenScanInfo(true);

    scanInfo->setParseMessage(tok, parseMessage, TokenScanInfo::Message);

    m_lexer->setMessage(tok);
}
*/


// -----------------------------------------------------------------------

Python::SourceModuleList::SourceModuleList() :
    Python::SourceListParentBase(this),
    m_main(nullptr)
{
}

Python::SourceModuleList::~SourceModuleList()
{
}

void Python::SourceModuleList::setMain(Python::SourceModule *main)
{
    if (!contains(main))
        insert(main);

    m_main = main;
}

bool Python::SourceModuleList::remove(Python::SourceListNodeBase *node, bool deleteNode)
{
    if (node == m_main)
        m_main = nullptr;

    return Python::SourceListParentBase::remove(node, deleteNode);
}
