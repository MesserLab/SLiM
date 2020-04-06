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
    // Reset the locale to "C" regardless of user locale; see issue #81
    {
        {
            qDebug() << "QLocale().name() before:" << QLocale().name();
            
            std::locale loc;
            std::cout << "std::locale name() before : " << loc.name() << std::endl;
            
            char *loc_c = setlocale(LC_ALL, NULL);
            std::cout << "setlocale() name before :" << loc_c << std::endl;
        }
        
        QLocale::setDefault(QLocale("C"));
        
        {
            qDebug() << "QLocale().name() after:" << QLocale().name();
            
            std::locale loc;
            std::cout << "std::locale name() after : " << loc.name() << std::endl;
            
            char *loc_c = setlocale(LC_ALL, nullptr);
            std::cout << "setlocale() name after :" << loc_c << std::endl;
        }
        
        // Test that the locale is working for us; is the decimal separator a period or a comma?
        double converted_value = strtod("1.0e-8", nullptr);
        
        if (fabs(1e-8 - converted_value) > 1e-10)
        {
            std::cout << "Locale issue: strtod() is not translating numbers according to the C locale.";
            exit(EXIT_FAILURE);
        }
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
