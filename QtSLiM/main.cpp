#include "QtSLiMAppDelegate.h"
#include "QtSLiMWindow.h"
#include "QtSLiMPreferences.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>

#include <locale>


int main(int argc, char *argv[])
{
    // Reset the locale to "C" regardless of user locale; see issue #81
    {
        //qDebug() << "QLocale().name() before:" << QLocale().name();
        //
        //std::locale loc;
        //std::cout << "loc.name() : " << loc.name() << std::endl;
        
        QLocale::setDefault(QLocale("C"));
        
        //qDebug() << "QLocale().name() after:" << QLocale().name();
    }
    
    // Start the application
    QApplication app(argc, argv);
    QtSLiMAppDelegate appDelegate(nullptr);
    
    // Parse the command line
    QCommandLineParser parser;
    
    parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
    parser.setApplicationDescription(QCoreApplication::applicationName());
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("file", "The file(s) to open.");
    parser.process(app);
    
    // Open windows
    QtSLiMWindow *mainWin = nullptr;
    const QStringList posArgs = parser.positionalArguments();
    
    for (const QString &file : posArgs)
    {
        QtSLiMWindow *newWin = new QtSLiMWindow(file);
        newWin->tile(mainWin);
        newWin->show();
        mainWin = newWin;
    }
    
    if (!mainWin)
    {
        QtSLiMPreferencesNotifier &prefs = QtSLiMPreferencesNotifier::instance();
        
        if (prefs.appStartupPref() == 1)
        {
            // create a new window
            mainWin = new QtSLiMWindow(QtSLiMWindow::ModelType::WF);
        }
        else if (prefs.appStartupPref() == 2)
        {
            // run an open panel, which will return a window to show, or nullptr
            mainWin = QtSLiMWindow::runInitialOpenPanel();
            
            // if no file was opened, create a new window after all
            if (!mainWin)
                mainWin = new QtSLiMWindow(QtSLiMWindow::ModelType::WF);
        }
    }
    
    if (mainWin)
        mainWin->show();
    
    // Run the event loop
    return app.exec();
}
