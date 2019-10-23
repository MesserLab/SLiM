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
    void highlightBlock(const QString &text) override;

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
    void highlightBlock(const QString &text) override;
    
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





























