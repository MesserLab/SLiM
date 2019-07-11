#include "QtSLiMWindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QtSLiMWindow w;
    w.show();

    return a.exec();
}
