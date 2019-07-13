#include "QtSLiMAppDelegate.h"
#include <QApplication>

#include "eidos_globals.h"
#include "slim_globals.h"


QtSLiMAppDelegate::QtSLiMAppDelegate(QObject *parent) : QObject(parent)
{
    // Warm up our back ends before anything else happens
    Eidos_WarmUp();
    SLiM_WarmUp();
    // FIXME probably want to enable the SLiMgui class at some point
    //gEidosContextClasses.push_back(gSLiM_SLiMgui_Class);			// available only in SLiMgui
    Eidos_FinishWarmUp();

    // Remember our current working directory, to return to whenever we are not inside SLiM/Eidos
    app_cwd_ = Eidos_CurrentDirectory();

    // FIXME create recipes submenu once we have a document model
    // Create the Open Recipes menu
    //[self setUpRecipesMenu];

    // Connect to the app to find out when we're terminating
    QApplication *app = qApp;

    connect(app, &QApplication::lastWindowClosed, this, &QtSLiMAppDelegate::lastWindowClosed);
    connect(app, &QApplication::aboutToQuit, this, &QtSLiMAppDelegate::aboutToQuit);
}


//
//  public slots
//

void QtSLiMAppDelegate::lastWindowClosed(void)
{
}

void QtSLiMAppDelegate::aboutToQuit(void)
{
}

void QtSLiMAppDelegate::showAboutWindow(void)
{
    // FIXME implement
}

void QtSLiMAppDelegate::showHelp(void)
{
    // FIXME implement
}































