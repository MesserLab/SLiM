#include "QtSLiMWindow.h"
#include "ui_QtSLiMWindow.h"

QtSLiMWindow::QtSLiMWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::QtSLiMWindow)
{
    ui->setupUi(this);
}

QtSLiMWindow::~QtSLiMWindow()
{
    delete ui;
}
