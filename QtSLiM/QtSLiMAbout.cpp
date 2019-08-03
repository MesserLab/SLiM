#include "QtSLiMAbout.h"
#include "ui_QtSLiMAbout.h"

QtSLiMAbout::QtSLiMAbout(QWidget *parent) : QDialog(parent), ui(new Ui::QtSLiMAbout)
{
    ui->setupUi(this);
    
    // prevent this window from keeping the app running when all main windows are closed
    setAttribute(Qt::WA_QuitOnClose, false);
    
    // disable resizing
    layout()->setSizeConstraint(QLayout::SetFixedSize);
    setSizeGripEnabled(false);
}

QtSLiMAbout::~QtSLiMAbout()
{
    delete ui;
}
