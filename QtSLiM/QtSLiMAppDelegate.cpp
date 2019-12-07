#include "QtSLiMAppDelegate.h"
#include "QtSLiMWindow.h"
#include "QtSLiMFindRecipe.h"

#include <QApplication>
#include <QOpenGLWidget>
#include <QSurfaceFormat>
#include <QMenu>
#include <QAction>
#include <QDebug>
#include <QDir>
#include <QCollator>
#include <QKeyEvent>

#include "eidos_globals.h"
#include "eidos_beep.h"
#include "slim_globals.h"


static std::string Eidos_Beep_QT(std::string p_sound_name);


QtSLiMAppDelegate *qtSLiMAppDelegate = nullptr;

QtSLiMAppDelegate::QtSLiMAppDelegate(QObject *parent) : QObject(parent)
{
    // Install our custom beep handler
    Eidos_Beep = &Eidos_Beep_QT;
    
    // Let Qt know who we are, for QSettings configuration
    QCoreApplication::setOrganizationName("MesserLab");
    QCoreApplication::setOrganizationDomain("MesserLab.edu");   // Qt expects the domain in the standard order, and reverses it to form "edu.messerlab.QtSLiM.plist" as per Apple's usage
    QCoreApplication::setApplicationName("QtSLiM");
    QCoreApplication::setApplicationVersion(SLIM_VERSION_STRING);
    
    // Warm up our back ends before anything else happens
    Eidos_WarmUp();
    SLiM_WarmUp();
    // FIXME probably want to enable the SLiMgui class at some point
    //gEidosContextClasses.push_back(gSLiM_SLiMgui_Class);			// available only in SLiMgui
    Eidos_FinishWarmUp();

    // Remember our current working directory, to return to whenever we are not inside SLiM/Eidos
    app_cwd_ = Eidos_CurrentDirectory();

    // Set up the format for OpenGL buffers globally, so that it applies to all windows and contexts
    // This defaults to OpenGL 2.0, which is what we want, so right now we don't customize
    QSurfaceFormat format;
    //format.setDepthBufferSize(24);
    //format.setStencilBufferSize(8);
    //format.setVersion(3, 2);
    //format.setProfile(QSurfaceFormat::CompatibilityProfile);
    QSurfaceFormat::setDefaultFormat(format);
    
    // Connect to the app to find out when we're terminating
    QApplication *app = qApp;

    connect(app, &QApplication::lastWindowClosed, this, &QtSLiMAppDelegate::lastWindowClosed);
    connect(app, &QApplication::aboutToQuit, this, &QtSLiMAppDelegate::aboutToQuit);

    // Install our event filter to detect modifier key changes
    app->installEventFilter(this);
    
    // We assume we are the global instance; FIXME singleton pattern would be good
    qtSLiMAppDelegate = this;
}

void QtSLiMAppDelegate::setUpRecipesMenu(QMenu *openRecipesMenu, QAction *findRecipeAction)
{
    // This is called by QtSLiMWindow::initializeUI().  We are to take control of the
    // recipes submenu, filling it in and implementing the Find Recipe panel.
    connect(findRecipeAction, &QAction::triggered, this, &QtSLiMAppDelegate::findRecipe);
    
    // Find recipes in our resources and sort by numeric order
    QDir recipesDir(":/recipes/", "Recipe *.*", QDir::NoSort, QDir::Files | QDir::NoSymLinks);
    QStringList entryList = recipesDir.entryList(QStringList("Recipe *.*"));   // the previous name filter seems to be ignored
    QCollator collator;
    
    collator.setNumericMode(true);
    std::sort(entryList.begin(), entryList.end(), collator);
    //qDebug() << entryList;
    
    // Append an action for each, organized into submenus
    QString previousItemChapter;
    QMenu *chapterSubmenu = nullptr;
    
    for (QString &entryName : entryList)
    {
        QString recipeName;
        
        // Determine the menu item text for the recipe, removing "Recipe " and ".txt", and adding pythons
        if (entryName.endsWith(".txt"))
            recipeName = entryName.mid(7, entryName.length() - 11);
        else if (entryName.endsWith(".py"))
            recipeName = entryName.mid(7, entryName.length() - 7) + QString(" 🐍");
        
        int firstDotIndex = recipeName.indexOf('.');
        
        if (firstDotIndex != -1)
        {
            QString recipeChapter = recipeName.left(firstDotIndex);
            
            // Create a submenu for each chapter
            if (recipeChapter != previousItemChapter)
            {
                int recipeChapterValue = recipeChapter.toInt();
                QString chapterName;
                
                switch (recipeChapterValue)
                {
                    case 4: chapterName = "Getting started: Neutral evolution in a panmictic population";		break;
                    case 5: chapterName = "Demography and population structure";								break;
                    case 6: chapterName = "Sexual reproduction";												break;
                    case 7: chapterName = "Mutation types, genomic elements, and chromosome structure";         break;
                    case 8: chapterName = "SLiMgui visualizations for polymorphism patterns";					break;
                    case 9:	chapterName = "Selective sweeps";													break;
                    case 10:chapterName = "Context-dependent selection using fitness() callbacks";				break;
                    case 11:chapterName = "Complex mating schemes using mateChoice() callbacks";				break;
                    case 12:chapterName = "Direct child modifications using modifyChild() callbacks";			break;
                    case 13:chapterName = "Phenotypes, fitness functions, quantitative traits, and QTLs";		break;
                    case 14:chapterName = "Advanced models";													break;
                    case 15:chapterName = "Continuous-space models and interactions";							break;
                    case 16:chapterName = "Going beyond Wright-Fisher models: nonWF model recipes";             break;
                    case 17:chapterName = "Tree-sequence recording: tracking population history";				break;
                    case 18:chapterName = "Modeling explicit nucleotides";										break;
                    default: break;
                }
                
                if (chapterName.length())
                {
                    QString fullChapterName = QString("%1 – %2").arg(QString::number(recipeChapterValue), chapterName);
                    QAction *mainItem = openRecipesMenu->addAction(fullChapterName);
                    
                    chapterSubmenu = new QMenu(fullChapterName);
                    mainItem->setMenu(chapterSubmenu);
                }
                else
                {
                    qDebug() << "unrecognized chapter value " << recipeChapterValue;
                    qApp->beep();
                    break;
                }
            }
            
            // Move on to the current chapter
            previousItemChapter = recipeChapter;
            
            // And now add the menu item for the recipe
            QAction *menuItem = chapterSubmenu->addAction(recipeName);
            
            connect(menuItem, &QAction::triggered, this, &QtSLiMAppDelegate::openRecipe);
            menuItem->setData(QVariant(entryName));
        }
    }
}

bool QtSLiMAppDelegate::eventFilter(QObject *obj, QEvent *event)
{
    QEvent::Type type = event->type();
    
    if ((type == QEvent::KeyPress) || (type == QEvent::KeyRelease))
    {
        QKeyEvent *keyEvent = dynamic_cast<QKeyEvent *>(event);
        
        if (keyEvent)
        {
            Qt::Key key = static_cast<Qt::Key>(keyEvent->key());     // why does this return int?
            
            if ((key == Qt::Key_Shift) ||
                    (key == Qt::Key_Control) ||
                    (key == Qt::Key_Meta) ||
                    (key == Qt::Key_Alt) ||
                    (key == Qt::Key_AltGr) ||
                    (key == Qt::Key_CapsLock) ||
                    (key == Qt::Key_NumLock) ||
                    (key == Qt::Key_ScrollLock))
                emit modifiersChanged(keyEvent->modifiers());
        }
    }
    
    // standard event processing
    return QObject::eventFilter(obj, event);
}


//
//  public slots
//

void QtSLiMAppDelegate::lastWindowClosed(void)
{
    //qDebug() << "QtSLiMAppDelegate::lastWindowClosed";
}

void QtSLiMAppDelegate::aboutToQuit(void)
{
    //qDebug() << "QtSLiMAppDelegate::aboutToQuit";
}

QtSLiMWindow *QtSLiMAppDelegate::activeQtSLiMWindow(void)
{
    QWidget *activeWindow = qApp->activeWindow();
    QtSLiMWindow *activeQtSLiMWindow = qobject_cast<QtSLiMWindow *>(activeWindow);
    
    if (!activeQtSLiMWindow)
    {
        const QWidgetList topLevelWidgets = QApplication::topLevelWidgets();
        
        for (QWidget *widget : topLevelWidgets)
        {
            if (!widget->isHidden())
            {
                activeQtSLiMWindow = qobject_cast<QtSLiMWindow *>(widget);
                if (activeQtSLiMWindow) break;
            }
        }
    }
    
    return activeQtSLiMWindow;
}

void QtSLiMAppDelegate::findRecipe(void)
{
    // We delegate the opening itself to the active window, so that it can tile
    QtSLiMWindow *activeWindow = activeQtSLiMWindow();
    
    if (activeWindow)
    {
        QtSLiMFindRecipe findRecipePanel(nullptr);
        
        int result = findRecipePanel.exec();
        
        if (result == QDialog::Accepted)
        {
            QString resourceName = findRecipePanel.selectedRecipeFilename();
            QString recipeScript = findRecipePanel.selectedRecipeScript();
            QString trimmedName = resourceName;
            
            if (trimmedName.endsWith(".txt"))
                trimmedName.chop(4);
            
            activeWindow->openRecipe(trimmedName, recipeScript);
        }
    }
    else
    {
        // beep if there is no QtSLiMWindow to handle the action, but this should never happen
        qApp->beep();
    }
}

void QtSLiMAppDelegate::openRecipe(void)
{
    QAction *action = qobject_cast<QAction *>(sender());
    
    if (action)
    {
        const QVariant &data = action->data();
        QString resourceName = data.toString();
        
        if (resourceName.length())
        {
            QString resourcePath = ":/recipes/" + resourceName;
            QFile recipeFile(resourcePath);
            
            if (recipeFile.open(QFile::ReadOnly | QFile::Text))
            {
                QTextStream recipeTextStream(&recipeFile);
                QString recipeScript = recipeTextStream.readAll();
                
                // We delegate the opening itself to the active window, so that it can tile
                QtSLiMWindow *activeWindow = activeQtSLiMWindow();
                
                if (activeWindow)
                {
                    QString trimmedName = resourceName;
                    
                    if (trimmedName.endsWith(".txt"))
                        trimmedName.chop(4);
                    
                    activeWindow->openRecipe(trimmedName, recipeScript);
                }
                else
                {
                    // beep if there is no QtSLiMWindow to handle the action, but this should never happen
                    qApp->beep();
                }
            }
        }
     }
}


// This is declared in eidos_beep.h, but in QtSLiM it is actually defined here,
// so that we can produce the beep sound with Qt
std::string Eidos_Beep_QT(std::string __attribute__((__unused__)) p_sound_name)
{
    std::string return_string;
    
    qApp->beep();
    
    return return_string;
}




























