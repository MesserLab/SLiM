//
//  QtSLiMSyntaxHighlighting.h
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

#ifndef QTSLIMSYNTAXHIGHLIGHTING_H
#define QTSLIMSYNTAXHIGHLIGHTING_H

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>

class QString;
class QTextDocument;
class EidosScript;


// This one is for the output pane, and is regex-driven
class QtSLiMOutputHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    QtSLiMOutputHighlighter(QTextDocument *parent = nullptr);

protected:
    virtual void highlightBlock(const QString &text) override;

private:
    QRegularExpression poundRegex;
    QTextCharFormat poundDirectiveFormat;
    
    QRegularExpression commentRegex;
    QTextCharFormat commentFormat;
    
    QRegularExpression globalRegex;
    QTextCharFormat subpopFormat;
    QTextCharFormat genomicElementFormat;
    QTextCharFormat mutationTypeFormat;
};

// This one is for the scripting pane, and is AST-driven
class QtSLiMScriptHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    QtSLiMScriptHighlighter(QTextDocument *parent = nullptr);
    virtual ~QtSLiMScriptHighlighter() override;

protected:
    virtual void highlightBlock(const QString &text) override;
    
protected slots:
    void documentContentsChanged(void);
    
private:
    QTextCharFormat numberLiteralFormat;
    QTextCharFormat stringLiteralFormat;
    QTextCharFormat commentFormat;
    QTextCharFormat identifierFormat;
    QTextCharFormat keywordFormat;
    QTextCharFormat contextKeywordFormat;
    
    EidosScript *script = nullptr;
    int64_t lastProcessedTokenIndex = -1;
};


#endif // QTSLIMSYNTAXHIGHLIGHTING_H





























