#include "QtSLiMWindow.h"
#include "ui_QtSLiMWindow.h"


void QtSLiMWindow::glueUI(void)
{
    // connect all QtSLiMWindow slots
    connect(ui->playOneStepButton, &QPushButton::clicked, this, &QtSLiMWindow::playOneStepClicked);
    connect(ui->playButton, &QPushButton::clicked, this, &QtSLiMWindow::playClicked);
    connect(ui->profileButton, &QPushButton::clicked, this, &QtSLiMWindow::profileClicked);
    connect(ui->recycleButton, &QPushButton::clicked, this, &QtSLiMWindow::recycleClicked);

    connect(ui->showMutationsButton, &QPushButton::clicked, this, &QtSLiMWindow::showMutationsToggled);
    connect(ui->showFixedSubstitutionsButton, &QPushButton::clicked, this, &QtSLiMWindow::showFixedSubstitutionsToggled);
    connect(ui->showChromosomeMapsButton, &QPushButton::clicked, this, &QtSLiMWindow::showChromosomeMapsToggled);
    connect(ui->showGenomicElementsButton, &QPushButton::clicked, this, &QtSLiMWindow::showGenomicElementsToggled);

    connect(ui->checkScriptButton, &QPushButton::clicked, this, &QtSLiMWindow::checkScriptClicked);
    connect(ui->prettyprintButton, &QPushButton::clicked, this, &QtSLiMWindow::prettyprintClicked);
    connect(ui->scriptHelpButton, &QPushButton::clicked, this, &QtSLiMWindow::scriptHelpClicked);
    connect(ui->consoleButton, &QPushButton::clicked, this, &QtSLiMWindow::showConsoleClicked);
    connect(ui->browserButton, &QPushButton::clicked, this, &QtSLiMWindow::showBrowserClicked);

    connect(ui->clearOutputButton, &QPushButton::clicked, this, &QtSLiMWindow::clearOutputClicked);
    connect(ui->dumpPopulationButton, &QPushButton::clicked, this, &QtSLiMWindow::dumpPopulationClicked);
    connect(ui->graphPopupButton, &QPushButton::clicked, this, &QtSLiMWindow::graphPopupButtonClicked);
    connect(ui->changeDirectoryButton, &QPushButton::clicked, this, &QtSLiMWindow::changeDirectoryClicked);

    // set up all icon-based QPushButtons to change their icon as they track
    connect(ui->playOneStepButton, &QPushButton::pressed, this, &QtSLiMWindow::playOneStepPressed);
    connect(ui->playOneStepButton, &QPushButton::released, this, &QtSLiMWindow::playOneStepReleased);
    connect(ui->playButton, &QPushButton::pressed, this, &QtSLiMWindow::playPressed);
    connect(ui->playButton, &QPushButton::released, this, &QtSLiMWindow::playReleased);
    connect(ui->profileButton, &QPushButton::pressed, this, &QtSLiMWindow::profilePressed);
    connect(ui->profileButton, &QPushButton::released, this, &QtSLiMWindow::profileReleased);
    connect(ui->recycleButton, &QPushButton::pressed, this, &QtSLiMWindow::recyclePressed);
    connect(ui->recycleButton, &QPushButton::released, this, &QtSLiMWindow::recycleReleased);
    connect(ui->showMutationsButton, &QPushButton::pressed, this, &QtSLiMWindow::showMutationsPressed);
    connect(ui->showMutationsButton, &QPushButton::released, this, &QtSLiMWindow::showMutationsReleased);
    connect(ui->showFixedSubstitutionsButton, &QPushButton::pressed, this, &QtSLiMWindow::showFixedSubstitutionsPressed);
    connect(ui->showFixedSubstitutionsButton, &QPushButton::released, this, &QtSLiMWindow::showFixedSubstitutionsReleased);
    connect(ui->showChromosomeMapsButton, &QPushButton::pressed, this, &QtSLiMWindow::showChromosomeMapsPressed);
    connect(ui->showChromosomeMapsButton, &QPushButton::released, this, &QtSLiMWindow::showChromosomeMapsReleased);
    connect(ui->showGenomicElementsButton, &QPushButton::pressed, this, &QtSLiMWindow::showGenomicElementsPressed);
    connect(ui->showGenomicElementsButton, &QPushButton::released, this, &QtSLiMWindow::showGenomicElementsReleased);
    connect(ui->checkScriptButton, &QPushButton::pressed, this, &QtSLiMWindow::checkScriptPressed);
    connect(ui->checkScriptButton, &QPushButton::released, this, &QtSLiMWindow::checkScriptReleased);
    connect(ui->prettyprintButton, &QPushButton::pressed, this, &QtSLiMWindow::prettyprintPressed);
    connect(ui->prettyprintButton, &QPushButton::released, this, &QtSLiMWindow::prettyprintReleased);
    connect(ui->scriptHelpButton, &QPushButton::pressed, this, &QtSLiMWindow::scriptHelpPressed);
    connect(ui->scriptHelpButton, &QPushButton::released, this, &QtSLiMWindow::scriptHelpReleased);
    connect(ui->consoleButton, &QPushButton::pressed, this, &QtSLiMWindow::showConsolePressed);
    connect(ui->consoleButton, &QPushButton::released, this, &QtSLiMWindow::showConsoleReleased);
    connect(ui->browserButton, &QPushButton::pressed, this, &QtSLiMWindow::showBrowserPressed);
    connect(ui->browserButton, &QPushButton::released, this, &QtSLiMWindow::showBrowserReleased);
    connect(ui->clearOutputButton, &QPushButton::pressed, this, &QtSLiMWindow::clearOutputPressed);
    connect(ui->clearOutputButton, &QPushButton::released, this, &QtSLiMWindow::clearOutputReleased);
    connect(ui->dumpPopulationButton, &QPushButton::pressed, this, &QtSLiMWindow::dumpPopulationPressed);
    connect(ui->dumpPopulationButton, &QPushButton::released, this, &QtSLiMWindow::dumpPopulationReleased);
    connect(ui->graphPopupButton, &QPushButton::pressed, this, &QtSLiMWindow::graphPopupButtonPressed);
    connect(ui->graphPopupButton, &QPushButton::released, this, &QtSLiMWindow::graphPopupButtonReleased);
    connect(ui->changeDirectoryButton, &QPushButton::pressed, this, &QtSLiMWindow::changeDirectoryPressed);
    connect(ui->changeDirectoryButton, &QPushButton::released, this, &QtSLiMWindow::changeDirectoryReleased);
}


//
//  private slots
//

void QtSLiMWindow::playOneStepPressed(void)
{
    ui->playOneStepButton->setIcon(QIcon(":/buttons/play_step_H.png"));
}
void QtSLiMWindow::playOneStepReleased(void)
{
    ui->playOneStepButton->setIcon(QIcon(":/buttons/play_step.png"));
}
void QtSLiMWindow::playPressed(void)
{
    ui->playButton->setIcon(QIcon(ui->playButton->isChecked() ? ":/buttons/play.png" : ":/buttons/play_H.png"));
}
void QtSLiMWindow::playReleased(void)
{
    ui->playButton->setIcon(QIcon(ui->playButton->isChecked() ? ":/buttons/play_H.png" : ":/buttons/play.png"));
}
void QtSLiMWindow::profilePressed(void)
{
    ui->profileButton->setIcon(QIcon(ui->profileButton->isChecked() ? ":/buttons/profile.png" : ":/buttons/profile_H.png"));
}
void QtSLiMWindow::profileReleased(void)
{
    ui->profileButton->setIcon(QIcon(ui->profileButton->isChecked() ? ":/buttons/profile_H.png" : ":/buttons/profile.png"));
}
void QtSLiMWindow::recyclePressed(void)
{
    updateRecycleButtonIcon(true);
}
void QtSLiMWindow::recycleReleased(void)
{
    updateRecycleButtonIcon(false);
}
void QtSLiMWindow::showMutationsPressed(void)
{
    ui->showMutationsButton->setIcon(QIcon(ui->showMutationsButton->isChecked() ? ":/buttons/show_mutations.png" : ":/buttons/show_mutations_H.png"));
}
void QtSLiMWindow::showMutationsReleased(void)
{
    ui->showMutationsButton->setIcon(QIcon(ui->showMutationsButton->isChecked() ? ":/buttons/show_mutations_H.png" : ":/buttons/show_mutations.png"));
}
void QtSLiMWindow::showFixedSubstitutionsPressed(void)
{
    ui->showFixedSubstitutionsButton->setIcon(QIcon(ui->showFixedSubstitutionsButton->isChecked() ? ":/buttons/show_fixed.png" : ":/buttons/show_fixed_H.png"));
}
void QtSLiMWindow::showFixedSubstitutionsReleased(void)
{
    ui->showFixedSubstitutionsButton->setIcon(QIcon(ui->showFixedSubstitutionsButton->isChecked() ? ":/buttons/show_fixed_H.png" : ":/buttons/show_fixed.png"));
}
void QtSLiMWindow::showChromosomeMapsPressed(void)
{
    ui->showChromosomeMapsButton->setIcon(QIcon(ui->showChromosomeMapsButton->isChecked() ? ":/buttons/show_recombination.png" : ":/buttons/show_recombination_H.png"));
}
void QtSLiMWindow::showChromosomeMapsReleased(void)
{
    ui->showChromosomeMapsButton->setIcon(QIcon(ui->showChromosomeMapsButton->isChecked() ? ":/buttons/show_recombination_H.png" : ":/buttons/show_recombination.png"));
}
void QtSLiMWindow::showGenomicElementsPressed(void)
{
    ui->showGenomicElementsButton->setIcon(QIcon(ui->showGenomicElementsButton->isChecked() ? ":/buttons/show_genomicelements.png" : ":/buttons/show_genomicelements_H.png"));
}
void QtSLiMWindow::showGenomicElementsReleased(void)
{
    ui->showGenomicElementsButton->setIcon(QIcon(ui->showGenomicElementsButton->isChecked() ? ":/buttons/show_genomicelements_H.png" : ":/buttons/show_genomicelements.png"));
}
void QtSLiMWindow::checkScriptPressed(void)
{
    ui->checkScriptButton->setIcon(QIcon(":/buttons/check_H.png"));
}
void QtSLiMWindow::checkScriptReleased(void)
{
    ui->checkScriptButton->setIcon(QIcon(":/buttons/check.png"));
}
void QtSLiMWindow::prettyprintPressed(void)
{
    ui->prettyprintButton->setIcon(QIcon(":/buttons/prettyprint_H.png"));
}
void QtSLiMWindow::prettyprintReleased(void)
{
    ui->prettyprintButton->setIcon(QIcon(":/buttons/prettyprint.png"));
}
void QtSLiMWindow::scriptHelpPressed(void)
{
    ui->scriptHelpButton->setIcon(QIcon(":/buttons/syntax_help_H.png"));
}
void QtSLiMWindow::scriptHelpReleased(void)
{
    ui->scriptHelpButton->setIcon(QIcon(":/buttons/syntax_help.png"));
}
void QtSLiMWindow::showConsolePressed(void)
{
    ui->consoleButton->setIcon(QIcon(ui->consoleButton->isChecked() ? ":/buttons/show_console.png" : ":/buttons/show_console_H.png"));
    ui->consoleButton->setIcon(QIcon(":/buttons/show_console_H.png"));
}
void QtSLiMWindow::showConsoleReleased(void)
{
    ui->consoleButton->setIcon(QIcon(ui->consoleButton->isChecked() ? ":/buttons/show_console_H.png" : ":/buttons/show_console.png"));
    ui->consoleButton->setIcon(QIcon(":/buttons/show_console.png"));
}
void QtSLiMWindow::showBrowserPressed(void)
{
    ui->browserButton->setIcon(QIcon(ui->browserButton->isChecked() ? ":/buttons/show_browser.png" : ":/buttons/show_browser_H.png"));
}
void QtSLiMWindow::showBrowserReleased(void)
{
    ui->browserButton->setIcon(QIcon(ui->browserButton->isChecked() ? ":/buttons/show_browser_H.png" : ":/buttons/show_browser.png"));
}
void QtSLiMWindow::clearOutputPressed(void)
{
    ui->clearOutputButton->setIcon(QIcon(":/buttons/delete_H.png"));
}
void QtSLiMWindow::clearOutputReleased(void)
{
    ui->clearOutputButton->setIcon(QIcon(":/buttons/delete.png"));
}
void QtSLiMWindow::dumpPopulationPressed(void)
{
    ui->dumpPopulationButton->setIcon(QIcon(":/buttons/dump_output_H.png"));
}
void QtSLiMWindow::dumpPopulationReleased(void)
{
    ui->dumpPopulationButton->setIcon(QIcon(":/buttons/dump_output.png"));
}
void QtSLiMWindow::graphPopupButtonPressed(void)
{
    ui->graphPopupButton->setIcon(QIcon(":/buttons/graph_submenu_H.png"));
}
void QtSLiMWindow::graphPopupButtonReleased(void)
{
    ui->graphPopupButton->setIcon(QIcon(":/buttons/graph_submenu.png"));
}
void QtSLiMWindow::changeDirectoryPressed(void)
{
    ui->changeDirectoryButton->setIcon(QIcon(":/buttons/change_folder_H.png"));
}
void QtSLiMWindow::changeDirectoryReleased(void)
{
    ui->changeDirectoryButton->setIcon(QIcon(":/buttons/change_folder.png"));
}
