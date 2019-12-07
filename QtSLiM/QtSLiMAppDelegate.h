#ifndef QTSLIMAPPDELEGATE_H
#define QTSLIMAPPDELEGATE_H

#include <QObject>
#include <string>

class QMenu;
class QAction;
class QtSLiMWindow;
class QtSLiMAppDelegate;
 

extern QtSLiMAppDelegate *qtSLiMAppDelegate;    // global instance

class QtSLiMAppDelegate : public QObject
{
    Q_OBJECT

    std::string app_cwd_;	// the app's current working directory

public:
    explicit QtSLiMAppDelegate(QObject *parent);

    std::string &QtSLiMCurrentWorkingDirectory(void) { return app_cwd_; }
    
    void setUpRecipesMenu(QMenu *openRecipesSubmenu, QAction *findRecipeAction);

public slots:
    void lastWindowClosed(void);
    void aboutToQuit(void);
    
    void findRecipe(void);
    void openRecipe(void);
    
signals:
    void modifiersChanged(Qt::KeyboardModifiers newModifiers);
    
private:
    QtSLiMWindow *activeQtSLiMWindow(void);  
    
    bool eventFilter(QObject *obj, QEvent *event) override;
};


#endif // QTSLIMAPPDELEGATE_H







