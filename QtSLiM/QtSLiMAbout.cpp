#include "QtSLiMAbout.h"
#include "ui_QtSLiMAbout.h"

#include "slim_globals.h"


QtSLiMAbout::QtSLiMAbout(QWidget *parent) : QDialog(parent), ui(new Ui::QtSLiMAbout)
{
    ui->setupUi(this);
    
    // prevent this window from keeping the app running when all main windows are closed
    setAttribute(Qt::WA_QuitOnClose, false);
    
    // disable resizing
    layout()->setSizeConstraint(QLayout::SetFixedSize);
    setSizeGripEnabled(false);
    
    // fix version number; FIXME would be nice to figure out a way to get the build number on Linux...
    ui->versionLabel->setText("version " + QString(SLIM_VERSION_STRING));
}

QtSLiMAbout::~QtSLiMAbout()
{
    delete ui;
}
