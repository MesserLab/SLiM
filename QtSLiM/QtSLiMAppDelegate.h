#ifndef QTSLIMAPPDELEGATE_H
#define QTSLIMAPPDELEGATE_H

#include <QObject>
#include <string>

class QtSLiMAppDelegate : public QObject
{
    Q_OBJECT

    std::string app_cwd_;	// the app's current working directory

public:
    explicit QtSLiMAppDelegate(QObject *parent);

    std::string &QtSLiMCurrentWorkingDirectory(void) { return app_cwd_; }

public slots:
    void lastWindowClosed(void);
    void aboutToQuit(void);

    void showAboutWindow(void);
    void showHelp(void);
    // FIXME pull in other actions from AppDelegate
};

#endif // QTSLIMAPPDELEGATE_H







