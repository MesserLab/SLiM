#ifndef QTSLIMSCRIPTTEXTEDIT_H
#define QTSLIMSCRIPTTEXTEDIT_H

#include <QTextEdit>


class QtSLiMScriptTextEdit : public QTextEdit
{
    Q_OBJECT
    
public:
    QtSLiMScriptTextEdit(const QString &text, QWidget *parent = nullptr) : QTextEdit(text, parent) {}
    QtSLiMScriptTextEdit(QWidget *parent = nullptr) : QTextEdit(parent) {}
    ~QtSLiMScriptTextEdit() override;
    
signals:
    void optionClickOnSymbol(QString symbol);
    
protected:
    bool optionClickIntercepted = false;    // used to track that we are intercepting a mouse event
    
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
};


#endif // QTSLIMSCRIPTTEXTEDIT_H






























