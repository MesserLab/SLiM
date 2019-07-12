#ifndef QTSLIMWINDOW_H
#define QTSLIMWINDOW_H

#include <QMainWindow>

namespace Ui {
class QtSLiMWindow;
}

class QtSLiMWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit QtSLiMWindow(QWidget *parent = nullptr);
    ~QtSLiMWindow();

    static QString defaultWFScriptString(void);
    static QString defaultNonWFScriptString(void);

public slots:
    void playOneStepClicked(void);
    void playClicked(void);
    void profileClicked(void);
    void recycleClicked(void);

    void showMutationsToggled(void);
    void showFixedSubstitutionsToggled(void);
    void showChromosomeMapsToggled(void);
    void showGenomicElementsToggled(void);

    void checkScriptClicked(void);
    void prettyprintClicked(void);
    void scriptHelpClicked(void);
    void showConsoleClicked(void);
    void showBrowserClicked(void);

    void clearOutputClicked(void);
    void dumpPopulationClicked(void);
    void graphPopupButtonClicked(void);
    void changeDirectoryClicked(void);

private slots:
    void playPressed(void);
    void playReleased(void);

private:
    Ui::QtSLiMWindow *ui;
};

#endif // QTSLIMWINDOW_H
