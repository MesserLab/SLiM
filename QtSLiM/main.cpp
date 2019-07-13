#include "QtSLiMAppDelegate.h"
#include "QtSLiMWindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QtSLiMAppDelegate appDelegate(nullptr);

    QtSLiMWindow w;
    w.show();

    return a.exec();
}
