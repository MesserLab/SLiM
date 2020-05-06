#include "QtSLiMAppDelegate.h"
#include "QtSLiMWindow.h"
#include "QtSLiMPreferences.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>

#include <locale>
#include <locale.h>

#include "eidos_globals.h"
#include "eidos_test_element.h"
#include "eidos_symbol_table.h"


#if SLIM_LEAK_CHECKING
static void clean_up_leak_false_positives(void)
{
	// This does a little cleanup that helps Valgrind to understand that some things have not been leaked.
	// I think perhaps unordered_map keeps values in an unaligned manner that Valgrind doesn't see as pointers.
	Eidos_FreeGlobalStrings();
	EidosTestElement::FreeThunks();
	MutationRun::DeleteMutationRunFreeList();
    FreeSymbolTablePool();
	Eidos_FreeRNG(gEidos_RNG);
}

// Note that we still get some leaks reported, many of which are likely spurious.  That seems to be caused by:
// https://stackoverflow.com/a/51553776/2752221
// I'd like to incorporate the fix given there, but I'm not sure where I'm supposed to find <lsan_interface.h>...
#endif


int main(int argc, char *argv[])
{
    // Check that the run-time Qt version matches the compile-time Qt version
    if (strcmp(qVersion(), QT_VERSION_STR) != 0)
    {
        std::cout << "Run-time Qt version " << qVersion() << " does not match compile-time Qt version " << QT_VERSION_STR << std::endl;
        exit(EXIT_FAILURE);
    }
    
    // Check for running under ASAN and log to confirm it is enabled; see SLiM.pro to enable it
#if defined(__has_feature)
#  if __has_feature(address_sanitizer)
    std::cout << "***** ASAN enabled *****" << std::endl;
#  endif
#endif
    
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
    int appReturn = app.exec();
    
#if SLIM_LEAK_CHECKING
    clean_up_leak_false_positives();
#endif
    
    return appReturn;
}


















































