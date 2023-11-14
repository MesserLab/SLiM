#include "QtSLiMAppDelegate.h"
#include "QtSLiMWindow.h"
#include "QtSLiMPreferences.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QStyle>
#include <QStyleFactory>
#include <QDebug>

#include <locale>
#include <locale.h>

#include "eidos_globals.h"
#include "interaction_type.h"

#if SLIM_LEAK_CHECKING
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


// Force to light mode on macOS
// To avoid having to make this a .mm file on macOS, we use the Obj-C runtime directly
// See https://www.mikeash.com/pyblog/objc_msgsends-new-prototype.html for some background
// This is pretty gross, but this is also why Objective-C is cool!  :->
// Note that we also have a custom Info.plist file that forces light mode; we try to
// force light mode in two different ways.  This function is necessary for cmake builds,
// which do not use the custom Info.plist at this time.  The custom Info.plist is
// necessary because this function, for some reason, does not succeed in forcing light
// mode when QtSLiM is built with qmake at the command line.
// BCH 1/17/2021: We now try to force light mode only when compiled against a Qt version
// earlier than 5.15.2 (since Qt did not support dark mode well prior to that version).
// Since the double-click install version is built against 5.15.2, this should mean that
// most macOS users get dark mode support.  The NSRequiresAquaSystemAppearance key in our
// QtSLiM_Info.plist file, which was set to <true/> to force light mode, has been
// removed.  As per the above comment, this may mean that macOS builds against a Qt version
// earlier than 5.15.2 will not succeed in forcing light mode.  For this reason and others,
// we now require Qt 5.15.2 when building on macOS (see QtSLiMAppDelegate.cpp).
// BCH 4/13/2022: Disabling this macos_ForceLightMode() function altogether.  Building against
// Qt versions prior to 5.15.2 on macOS is no longer supported at all, and linking against
// libobjc.dylib on macOS 12 has gotten complicated for security reasons (trampolines).
#ifdef __APPLE__
#if (QT_VERSION < QT_VERSION_CHECK(5, 15, 2))
#if 0
// Include objc headers
#include <objc/runtime.h>
#include <objc/message.h>

static void macos_ForceLightMode(void)
{
    // First we need to make an NSString with value @"NSAppearanceNameAqua"; 1 is NSASCIIStringEncoding
    Class nsstring_class = objc_lookUpClass("NSString");
    SEL selector_stringWithCString_encoding = sel_registerName("stringWithCString:encoding:");
    
    if ((!nsstring_class) || (!selector_stringWithCString_encoding))
        return;
    
    //std::cout << "nsstring_class == " << nsstring_class << std::endl;
    //std::cout << "class_getName(nsstring_class) == " << class_getName(nsstring_class) << std::endl;
    //std::cout << "selector_stringWithCString_encoding == " << sel_getName(selector_stringWithCString_encoding) << std::endl;
    
    id aquaString = ((id (*)(id, SEL, const char *, int))objc_msgSend)((id)nsstring_class, selector_stringWithCString_encoding, "NSAppearanceNameAqua", 1);
    
    if (!aquaString)
        return;
    
    //std::cout << "aquaString == " << aquaString << std::endl;
    //std::cout << std::endl;
    
    // Next we need to get the named appearance from NSAppearance
    Class nsappearance_class = objc_lookUpClass("NSAppearance");
    SEL selector_appearanceNamed = sel_registerName("appearanceNamed:");
    
    if ((!nsappearance_class) || (!selector_appearanceNamed))
        return;
    
    //std::cout << "nsappearance_class == " << nsappearance_class << std::endl;
    //std::cout << "class_getName(nsappearance_class) == " << class_getName(nsappearance_class) << std::endl;
    //std::cout << "selector_appearanceNamed == " << sel_getName(selector_appearanceNamed) << std::endl;
    
    id aquaAppearance = ((id (*)(id, SEL, id))objc_msgSend)((id)nsappearance_class, selector_appearanceNamed, aquaString);
    
    if (!aquaAppearance)
        return;
    
    //std::cout << "aquaAppearance == " << aquaAppearance << std::endl;
    //std::cout << std::endl;
    
    // Then get the shared NSApp object
    Class nsapp_class = objc_lookUpClass("NSApplication");
    SEL selector_sharedApplication = sel_registerName("sharedApplication");
    
    if ((!nsapp_class) || (!selector_sharedApplication))
        return;
    
    //std::cout << "nsapp_class == " << nsapp_class << std::endl;
    //std::cout << "class_getName(nsapp_class) == " << class_getName(nsapp_class) << std::endl;
    //std::cout << "selector_sharedApplication == " << sel_getName(selector_sharedApplication) << std::endl;
    
    id sharedApplication = ((id (*)(id, SEL))objc_msgSend)((id)nsapp_class, selector_sharedApplication);
    
    if (!sharedApplication)
        return;
    
    //std::cout << "sharedApplication == " << sharedApplication << std::endl;
    //std::cout << std::endl;
    
    // Then call setAppearance: on NSApplication to force light mode
    SEL selector_setAppearance = sel_registerName("setAppearance:");
    
    if (!selector_setAppearance)
        return;
    
    //std::cout << "selector_setAppearance == " << sel_getName(selector_setAppearance) << std::endl;
    //std::cout << std::endl;
    
    // -[NSApplication setAppearance:] was added in 10.14, so we need to check availability first
    if (class_respondsToSelector(nsapp_class, selector_setAppearance))
        ((void (*)(id, SEL, id))objc_msgSend)(sharedApplication, selector_setAppearance, aquaAppearance);
}
#endif
#endif
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
#if (QT_VERSION >= QT_VERSION_CHECK(5, 12, 0))
        newPalette.setColor(QPalette::PlaceholderText, QColor(101, 101, 101));
#endif
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
    
    // On macOS, force light mode appearance.  BCH 1/17/2021: We now try to force light mode only when
    // compiled against a Qt version earlier than 5.15.2 (since Qt did not support dark mode well prior
    // to that version).  Since the double-click install version is built against 5.15.2, this should
    // mean that most macOS users get dark mode support.
#ifdef __APPLE__
#if (QT_VERSION < QT_VERSION_CHECK(5, 15, 2))
#warning Building on macOS with a Qt version less than 5.15.2; dark/light mode may not work correctly
    //macos_ForceLightMode();
#endif
#endif
    
    // On Linux, force dark mode appearance if the user has chosen that.  This is Linux-only because
    // on macOS we follow the macOS dark mode setting, and Qt largely follows it for us.
#ifndef __APPLE__
    linux_ForceDarkMode();
#endif
    
    // Tell Qt to use high-DPI pixmaps for icons
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    
    // On macOS, turn off the automatic quit on last window close, for Qt 5.15.2.
    // Builds against older Qt versions will just quit on the last window close, because
    // QTBUG-86874 and QTBUG-86875 prevent this from working.
#ifdef __APPLE__
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 2))
    app.setQuitOnLastWindowClosed(false);
#endif
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
        mainWin->show();
    
    appDelegate.appDidFinishLaunching(mainWin);
    
    // Run the event loop
    int appReturn = app.exec();
    
#if SLIM_LEAK_CHECKING
    clean_up_leak_false_positives();
#endif
    
    return appReturn;
}


















































