#include "QtSLiMAppDelegate.h"
#include "QtSLiMWindow.h"
#include "QtSLiMPreferences.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>

#include <locale>
#include <locale.h>


int main(int argc, char *argv[])
{
    // Start the application
    QApplication app(argc, argv);
    QtSLiMAppDelegate appDelegate(nullptr);
    
    // Reset the locale to "C" regardless of user locale; see issue #81
    {
        /*{
            qDebug() << "QLocale().name() before:" << QLocale().name();
            
            std::locale loc;
            std::cout << "std::locale name() before : " << loc.name() << std::endl;
            
            char *loc_c = setlocale(LC_ALL, NULL);
            std::cout << "setlocale() name before :" << loc_c << std::endl;
        }*/
        
        setlocale(LC_ALL, "C");          // Might just do LC_NUMERIC, but let's avoid surprises...
        QLocale::setDefault(QLocale("C"));
        
        /*{
            qDebug() << "QLocale().name() after:" << QLocale().name();
            
            std::locale loc;
            std::cout << "std::locale name() after : " << loc.name() << std::endl;
            
            char *loc_c = setlocale(LC_ALL, nullptr);
            std::cout << "setlocale() name after :" << loc_c << std::endl;
        }*/
        
        // Test that the locale is working for us; is the decimal separator a period or a comma?
        double converted_value = strtod("0.5", nullptr);
        
        if (fabs(0.5 - converted_value) > 1e-10)
        {
            std::cout << "Locale issue: strtod() is not translating numbers according to the C locale.";
            exit(EXIT_FAILURE);
        }
    }
    
    // Tell Qt to use high-DPI pixmaps for icons
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    
    // Set the application icon; this fixes in app icon in the dock/toolbar/whatever, even if the right icon is not attached in the desktop
    QIcon appIcon;
    appIcon.addFile(":/icons/AppIcon16.png");
    appIcon.addFile(":/icons/AppIcon32.png");
    appIcon.addFile(":/icons/AppIcon48.png");
    appIcon.addFile(":/icons/AppIcon64.png");
    appIcon.addFile(":/icons/AppIcon128.png");
    appIcon.addFile(":/icons/AppIcon256.png");
    appIcon.addFile(":/icons/AppIcon512.png");
    appIcon.addFile(":/icons/AppIcon1024.png");
    app.setWindowIcon(appIcon);
    
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
