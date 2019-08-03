#ifndef QTSLIMABOUT_H
#define QTSLIMABOUT_H

#include <QDialog>

namespace Ui {
class QtSLiMAbout;
}

class QtSLiMAbout : public QDialog
{
    Q_OBJECT
    
public:
    explicit QtSLiMAbout(QWidget *parent = nullptr);
    ~QtSLiMAbout();
    
private:
    Ui::QtSLiMAbout *ui;
};

#endif // QTSLIMABOUT_H
