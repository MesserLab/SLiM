#ifndef QTSLIMWINDOW_H
#define QTSLIMWINDOW_H

#include <QMainWindow>

namespace Ui {
class QtSLiMWindow;
}

class QtSLiMWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit QtSLiMWindow(QWidget *parent = nullptr);
    ~QtSLiMWindow();

private:
    Ui::QtSLiMWindow *ui;
};

#endif // QTSLIMWINDOW_H
