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

#ifdef __APPLE__
#include <objc/runtime.h>
#include  <objc/message.h>
#endif

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


// Force to light mode on macOS
// To avoid having to make this a .mm file on macOS, we use the Obj-C runtime directly
// See https://www.mikeash.com/pyblog/objc_msgsends-new-prototype.html for some background
// This is pretty gross, but this is also why Objective-C is cool!  :->
// Note that we also have a custom Info.plist file that forces light mode; we try to
// force light mode in two different ways.  This function is necessary for cmake builds,
// which do not use the custom Info.plist at this time.  The custom Info.plist is
// necessary because this function, for some reason, does not succeed in forcing light
// mode when QtSLiM is built with qmake at the command line.
#ifdef __APPLE__
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
    
    // On macOS, force light mode appearance
#ifdef __APPLE__
    macos_ForceLightMode();
#endif
    
    // Tell Qt to use high-DPI pixmaps for icons
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    
    // On macOS, turn off the automatic quit on last window close
    // BCH 9/23/2020: I am forced not to do this by a crash on quit, so we continue to delete on close for
    // now (and we continue to quit when the last window closes).  See QTBUG-86874 and QTBUG-86875.  If a
    // fix or workaround for either of those issues is found, the code is otherwise ready to transition to
    // having QtSLiM stay open after the last window closes, on macOS.  Search for those bug numbers to find
    // the other spots in the code related to this mess.
    // BCH 9/24/2020: Note that QTBUG-86875 is fixed in 5.15.1, but we don't want to require that.
#ifdef __APPLE__
    app.setQuitOnLastWindowClosed(true);
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
            mainWin = new QtSLiMWindow(QtSLiMWindow::ModelType::WF);
        }
        else if (prefs.appStartupPref() == 2)
        {
            // run an open panel, which will return a window to show, or nullptr
            mainWin = qtSLiMAppDelegate->open(nullptr);
            
            // if no file was opened, create a new window after all
            if (!mainWin)
                mainWin = new QtSLiMWindow(QtSLiMWindow::ModelType::WF);
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


















































