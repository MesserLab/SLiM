//
//  QtSLiMSyntaxHighlighting.cpp
//  SLiM
//
//  Created by Ben Haller on 8/4/2019.
//  Copyright (c) 2019-2020 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/slim/
//

//	This file is part of SLiM.
//
//	SLiM is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
//
//	SLiM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License along with SLiM.  If not, see <http://www.gnu.org/licenses/>.


#include "QtSLiMSyntaxHighlighting.h"
#include "QtSLiMExtras.h"

#include <QTextDocument>
#include <QString>
#include <QTextCharFormat>
#include <QDebug>

#include "eidos_script.h"


//
//  QtSLiMOutputHighlighter
//

QtSLiMOutputHighlighter::QtSLiMOutputHighlighter(QTextDocument *parent) :
    QSyntaxHighlighter(parent),
    poundRegex(QString("^\\s*#[^\\n]*")),
    commentRegex(QString("//[^\\n]*")),
    globalRegex(QString("\\b[pgm][0-9]+\\b"))
{
    poundDirectiveFormat.setForeground(QColor(196, 26, 22));
    commentFormat.setForeground(QColor(0, 116, 0));
    subpopFormat.setForeground(QColor(28, 0, 207));
    genomicElementFormat.setForeground(QColor(63, 110, 116));
    mutationTypeFormat.setForeground(QColor(170, 13, 145));
}

void QtSLiMOutputHighlighter::highlightBlock(const QString &text)
{
    if (text.length())
    {
        // highlight globals first; if they occur inside pound or comment regions, their format will be overwritten
        {
            QRegularExpressionMatchIterator matchIterator = globalRegex.globalMatch(text);
            while (matchIterator.hasNext()) {
                QRegularExpressionMatch match = matchIterator.next();
                QString matchString = match.captured();
                
                if (matchString.length() > 0)
                {
                    if (matchString[0] == 'p')
                        setFormat(match.capturedStart(), match.capturedLength(), subpopFormat);
                    else if (matchString[0] == 'g')
                        setFormat(match.capturedStart(), match.capturedLength(), genomicElementFormat);
                    else if (matchString[0] == 'm')
                        setFormat(match.capturedStart(), match.capturedLength(), mutationTypeFormat);
                }
            }
        }
        
        // highlight pound lines next, since that overrides the previous coloring rules
        {
            QRegularExpressionMatchIterator matchIterator = poundRegex.globalMatch(text);
            while (matchIterator.hasNext()) {
                QRegularExpressionMatch match = matchIterator.next();
                setFormat(match.capturedStart(), match.capturedLength(), poundDirectiveFormat);
            }
        }
        
        // highlight comments last, since there is no syntax coloring inside them
        {
            QRegularExpressionMatchIterator matchIterator = commentRegex.globalMatch(text);
            while (matchIterator.hasNext()) {
                QRegularExpressionMatch match = matchIterator.next();
                setFormat(match.capturedStart(), match.capturedLength(), commentFormat);
            }
        }
    }
}


//
//  QtSLiMScriptHighlighter
//

QtSLiMScriptHighlighter::QtSLiMScriptHighlighter(QTextDocument *parent) : QSyntaxHighlighter(parent)
{
    numberLiteralFormat.setForeground(QColor(28, 0, 207));
    stringLiteralFormat.setForeground(QColor(196, 26, 22));
    commentFormat.setForeground(QColor(0, 116, 0));
    identifierFormat.setForeground(QColor(63, 110, 116));
    keywordFormat.setForeground(QColor(170, 13, 145));
    contextKeywordFormat.setForeground(QColor(80, 13, 145));
    
    // listen for changes to our document's contents
    // FIXME technically we need to recache and stuff if setDocument() is called, but we never do that in QtSLiM
    connect(parent, &QTextDocument::contentsChanged, this, &QtSLiMScriptHighlighter::documentContentsChanged);
}

QtSLiMScriptHighlighter::~QtSLiMScriptHighlighter()
{
    // throw out our cached information about the document
    documentContentsChanged();
}

void QtSLiMScriptHighlighter::documentContentsChanged(void)
{
    // Note that this is called by highlightBlock() below, as well as by the QTextDocument::contentsChanged signal
    if (script)
    {
        delete script;
        script = nullptr;
        
        lastProcessedTokenIndex = -1;
    }
}

void QtSLiMScriptHighlighter::highlightBlock(__attribute__((__unused__)) const QString &text)
{
    //qDebug() << "highlightBlock : " << text;
    
    const QTextBlock &block = currentBlock();
    int pos = block.position(), len = block.length();
    
    // Unfortunately, when setPlainText() gets called on the document's textedit, it does not send us
    // a contentsChanged signal until *after* it has asked us to do all of the syntax highlighting
    // for the new script.  So that signal is useless to us, and we have to look for a change ourselves
    // instead, by comparing the script string we have cached to the current script string.  This is
    // not great, since it requires a comparison of the entire script string, which will usually be
    // unchanged.  We optimize by doing this check only when we've been asked to highlight the very
    // first block; when setPlainText() is called, highlighting will proceed from the beginning.
    if (script && (pos == 0))
    {
        const std::string &cached_script_string = script->String();
        const std::string real_script_string = document()->toPlainText().toUtf8().constData();
        
        if (cached_script_string.compare(real_script_string) != 0)
        {
            //qDebug() << "cached script is wrong!";
            documentContentsChanged();
        }
    }
    
    // set up a new cached script if we don't have one
    if (!script)
    {
        script = new EidosScript(document()->toPlainText().toUtf8().constData());
        
        script->Tokenize(true, true);	// make bad tokens as needed, keep nonsignificant tokens
        
        //qDebug() << "   tokenized...";
    }
    
    const std::vector<EidosToken> &tokens = script->Tokens();
    int64_t token_count = static_cast<int64_t>(tokens.size());
    int64_t token_index = 0;
    
    if ((lastProcessedTokenIndex != -1) && (lastProcessedTokenIndex < token_count))
    {
        // check whether we can skip tokens processed by earlier calls to highlightBlock,
        // avoiding having to do an O(N) scan for each block, which would be O(N^2) overall
        const EidosToken &lastProcessedToken = tokens[static_cast<size_t>(lastProcessedTokenIndex)];
        
        if (lastProcessedToken.token_UTF16_end_ < pos)
            token_index = lastProcessedTokenIndex;
    }
    
    //qDebug() << "   token_count == " << token_count << ", initial token_index == " << token_index;
    
    for (; token_index < token_count; ++token_index)
    {
        const EidosToken &token = tokens[static_cast<size_t>(token_index)];
        
        // a token that starts after the end of the current block means we're done
        int token_start = token.token_UTF16_start_;
        
        if (token_start >= pos + len)
            break;
        
        // a token that ends before the start of the current block means we haven't reached our work yet
        int token_end = token.token_UTF16_end_;
        
        if (token_end < pos)
        {
            lastProcessedTokenIndex = token_index;
            continue;
        }
        
        // remember that we processed this token, unless it extends beyond the end of this block (as whitespace and comments can, among others)
        if (token_end < pos + len)
            lastProcessedTokenIndex = token_index;
        
        // otherwise, the token is in this block and should be colored; from here on, token_start and token_end are within-block positions
        // note that a token might start before this block and extend into it, or extend past the end of this block, so we clip
        token_start -= pos;
        token_end -= pos;
        
        if (token_start < 0)
            token_start = 0;
        if (token_end >= len)
            token_end = len - 1;
        
        switch (token.token_type_)
        {
        case EidosTokenType::kTokenNumber:
            setFormat(token_start, token_end - token_start + 1, numberLiteralFormat);
            break;
        case EidosTokenType::kTokenString:
            setFormat(token_start, token_end - token_start + 1, stringLiteralFormat);
            break;
        case EidosTokenType::kTokenComment:
        case EidosTokenType::kTokenCommentLong:
            setFormat(token_start, token_end - token_start + 1, commentFormat);
            break;
        case EidosTokenType::kFirstIdentifierLikeToken:
            setFormat(token_start, token_end - token_start + 1, keywordFormat);
            break;
        case EidosTokenType::kTokenIdentifier:
        {
            // most identifiers are left as black; only special ones get colored
            const std::string &token_string = token.token_string_;
            
            if ((token_string.compare("T") == 0) ||
                    (token_string.compare("F") == 0) ||
                    (token_string.compare("E") == 0) ||
                    (token_string.compare("PI") == 0) ||
                    (token_string.compare("INF") == 0) ||
                    (token_string.compare("NAN") == 0) ||
                    (token_string.compare("NULL") == 0))
            {
                setFormat(token_start, token_end - token_start + 1, identifierFormat);
            }
            else
            {
                // Here we handle SLiM-specific syntax coloring, beyond the Eidos coloring done above
                // This is from -[SLiMWindowController eidosConsoleWindowController:tokenStringIsSpecialIdentifier:]
                if (token_string.compare("sim") == 0)
                    setFormat(token_start, token_end - token_start + 1, identifierFormat);
                else if (token_string.compare("slimgui") == 0)
                    setFormat(token_start, token_end - token_start + 1, identifierFormat);
                // -[SLiMWindowController eidosConsoleWindowController:tokenStringIsSpecialIdentifier:] has code
                // here to give a special color (contextKeywordFormat) to the various keywords for callbacks, like
                // "fitness", "initialize", etc.; it is commented out and I don't think we want it
                else
                {
                    int token_length = static_cast<int>(token_string.length());
                    
                    if (token_length >= 2)
                    {
                        char first_ch = token_string[0];
                        
                        if ((first_ch == 'p') || (first_ch == 'g') || (first_ch == 'm') || (first_ch == 's') || (first_ch == 'i'))
                        {
                            bool is_slim_identifier = true;
                            
                            for (int ch_index = 1; ch_index < token_length; ++ch_index)
                            {
                                char idx_ch = token_string[static_cast<size_t>(ch_index)];
                                
                                if ((idx_ch < '0') || (idx_ch > '9'))
                                {
                                    is_slim_identifier = false;
                                    break;
                                }
                            }
                            
                            if (is_slim_identifier)
                                setFormat(token_start, token_end - token_start + 1, identifierFormat);
                        }
                    }
                    
                }
            }
            break;
        }
        default:
            break;
        }
    }
    
    // Here we deliberately break an optimization in QSyntaxHighlighter.  It uses these block states to
    // determine whether a rehighlight of one block needs to cascade to the next block; for example, a
    // new '/*' inserted in one block might cause the next block to become a comment.  We are not set
    // up to represent such states explicitly for QSyntaxHighlighter's benefit, so we just always poke
    // the block state so that QSyntaxHighlighter always recolors the following blocks, all the way to
    // the end of the script.  This is a bit unfortunate, but in practice it doesn't seem to produce
    // noticeable performance issues, and if it does the user can always turn off syntax coloring.
    setCurrentBlockState(currentBlockState() + 1);
}
































