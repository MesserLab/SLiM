#include "QtSLiMAppDelegate.h"
#include "QtSLiMWindow.h"
#include "QtSLiMPreferences.h"
#include "QtSLiMExtras.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QStyle>
#include <QStyleFactory>
#include <QDebug>

#include <locale>
#include <locale.h>

#include "eidos_globals.h"
#include "interaction_type.h"

#if SLIM_LEAK_CHECKING()
static void clean_up_leak_false_positives(void)
{
	// This does a little cleanup that helps Valgrind to understand that some things have not been leaked.
	// I think perhaps unordered_map keeps values in an unaligned manner that Valgrind doesn't see as pointers.
	InteractionType::DeleteSparseVectorFreeList();
	FreeSymbolTablePool();
	if (gEidos_RNG_Initialized)
		Eidos_FreeRNG();
}

// Note that we still get some leaks reported, many of which are likely spurious.  That seems to be caused by:
// https://stackoverflow.com/a/51553776/2752221
// I'd like to incorporate the fix given there, but I'm not sure where I'm supposed to find <lsan_interface.h>...
#endif


// This function switches the app to a dark theme, regardless of OS settings.  This is not the same as
// "dark mode" on macOS, and should probably never be used on macOS; it's for Linux, where getting
// Qt-based apps to obey the windowing system's preferred theme can be a battle.
// Credit is due to Josip Medved at https://www.medo64.com/2020/08/dark-mode-for-qt-application/
#ifndef __APPLE__
static void linux_ForceDarkMode(void)
{
    QtSLiMPreferencesNotifier &prefs = QtSLiMPreferencesNotifier::instance();
    
    // Josip writes "Just start with a good style (i.e. Fusion) and adjust its palette."  I think he
    // sets the style to Fusion because some styles don't adjust to a changed palette well; they
    // just have their own hard-coded palette.  Commenting out this line on macOS, for example,
    // many UI elements look correctly "dark" but scrollers retain their "light" appearance.  So, it's
    // not ideal to override whatever the default style would be, but it seems necessary to guarantee
    // good results.  I've decided to make this subject to a user pref.
    if (prefs.forceFusionStylePref())
    {
        qApp->setStyle(QStyleFactory::create("Fusion"));
    }
    
    // These numbers are modified from those provided by Josip, to better match the macOS dark mode
    // appearance for consistency, so that our icons, syntax highlighting colors, etc., work well
    if (prefs.forceDarkModePref())
    {
        QPalette newPalette;
        newPalette.setColor(QPalette::Window,          QColor( 49,  50,  51));
        newPalette.setColor(QPalette::WindowText,      QColor(255, 255, 255));
        newPalette.setColor(QPalette::Base,            QColor( 29,  30,  31));
        newPalette.setColor(QPalette::AlternateBase,   QColor(  9,  10,  11));
        newPalette.setColor(QPalette::PlaceholderText, QColor(101, 101, 101));
        newPalette.setColor(QPalette::Text,            QColor(255, 255, 255));
        newPalette.setColor(QPalette::Button,          QColor( 49,  50,  51));
        newPalette.setColor(QPalette::ButtonText,      QColor(255, 255, 255));
        newPalette.setColor(QPalette::BrightText,      QColor(255, 255, 255));
        newPalette.setColor(QPalette::Highlight,       QColor( 22,  86, 114));
        newPalette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
        
        newPalette.setColor(QPalette::Light,           QColor( 75,  75,  75));
        newPalette.setColor(QPalette::Midlight,        QColor( 60,  60,  60));
        //QPalette::Button should fall approximately midway here
        newPalette.setColor(QPalette::Mid,             QColor( 35,  35,  35));
        newPalette.setColor(QPalette::Dark,            QColor( 25,  25,  25));
        newPalette.setColor(QPalette::Shadow,          QColor( 0,    0,   0));
        
        newPalette.setColor(QPalette::Disabled, QPalette::Text,         QColor(101, 101, 101));
        newPalette.setColor(QPalette::Disabled, QPalette::WindowText,   QColor(101, 101, 101));
        newPalette.setColor(QPalette::Disabled, QPalette::ButtonText,   QColor(101, 101, 101));
        
        qApp->setPalette(newPalette);
    }
}
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
    
    // On Linux, force dark mode appearance if the user has chosen that.  This is Linux-only because
    // on macOS we follow the macOS dark mode setting, and Qt largely follows it for us.
#ifndef __APPLE__
    linux_ForceDarkMode();
#endif
    
    // Tell Qt to use high-DPI pixmaps for icons; not needed in Qt6, which is always high-DPI
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
    
    // On macOS, turn off the automatic quit on last window close, for Qt 5.15.2 and later.
    // Builds against older Qt versions will just quit on the last window close, because
    // QTBUG-86874 and QTBUG-86875 prevent this from working.
#ifdef __APPLE__
    app.setQuitOnLastWindowClosed(false);
#endif
    
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
            mainWin = new QtSLiMWindow(QtSLiMWindow::ModelType::WF, /* includeComments */ true);
        }
        else if (prefs.appStartupPref() == 2)
        {
            // run an open panel, which will return a window to show, or nullptr
            mainWin = qtSLiMAppDelegate->open(nullptr);
            
            // if no file was opened, create a new window after all
            if (!mainWin)
                mainWin = new QtSLiMWindow(QtSLiMWindow::ModelType::WF, /* includeComments */ true);
        }
    }
    
    if (mainWin)
    {
        mainWin->show();

        // Ensures the main window is visible and exposed on startup
        QtSLiMMakeWindowVisibleAndExposed(mainWin);
    }
    
    appDelegate.appDidFinishLaunching(mainWin);
    
    // Run the event loop
    int appReturn = app.exec();
    
#if SLIM_LEAK_CHECKING()
    clean_up_leak_false_positives();
#endif
    
    return appReturn;
}


















































