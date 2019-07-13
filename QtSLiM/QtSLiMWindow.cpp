#include "QtSLiMWindow.h"
#include "ui_QtSLiMWindow.h"

#include <QCoreApplication>
#include <QtDebug>

// TO DO:
//
// custom layout for play/profile buttons: https://doc.qt.io/qt-5/layout.html
// splitviews for the window: https://stackoverflow.com/questions/28309376/how-to-manage-qsplitter-in-qt-designer
// set up the app icon correctly: this seems to be very complicated, and didn't work on macOS, sigh...
// get the tableview columns configured correctly
// make the chromosome view
// make the population view
// syntax coloring in the script and output textedits
// implement pop-up menu for graph pop-up button
// create a simulation object and hook up recycle, step, play

QtSLiMWindow::QtSLiMWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::QtSLiMWindow)
{
    ui->setupUi(this);

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

    // fix the layout of the window
    ui->scriptHeaderLayout->setSpacing(4);
    ui->scriptHeaderLayout->setMargin(0);
    ui->scriptHeaderLabel->setContentsMargins(8, 0, 15, 0);

    ui->outputHeaderLayout->setSpacing(4);
    ui->outputHeaderLayout->setMargin(0);
    ui->outputHeaderLabel->setContentsMargins(8, 0, 15, 0);

    ui->chromosomeButtonsLayout->setSpacing(4);
    ui->chromosomeButtonsLayout->setMargin(0);

    ui->playControlsLayout->setSpacing(4);
    ui->playControlsLayout->setMargin(0);

    // set up the script and output textedits
    ui->scriptTextEdit->setFontFamily("Menlo");
    ui->scriptTextEdit->setFontPointSize(11);
    ui->scriptTextEdit->setTabStopWidth(20);    // should use setTabStopDistance(), which requires Qt 5.10; see https://stackoverflow.com/a/54605709/2752221
    ui->scriptTextEdit->setText(QtSLiMWindow::defaultWFScriptString());

    ui->outputTextEdit->setFontFamily("Menlo");
    ui->outputTextEdit->setFontPointSize(11);
    ui->scriptTextEdit->setTabStopWidth(20);    // should use setTabStopDistance(), which requires Qt 5.10; see https://stackoverflow.com/a/54605709/2752221

    // remove the profile button, for the time being
    QPushButton *profileButton = ui->profileButton;

    ui->playControlsLayout->removeWidget(profileButton);
    delete profileButton;
}

QtSLiMWindow::~QtSLiMWindow()
{
    delete ui;
}

QString QtSLiMWindow::defaultWFScriptString(void)
{
    return QString(
                "// set up a simple neutral simulation\n"
                "initialize() {\n"
                "	initializeMutationRate(1e-7);\n"
                "	\n"
                "	// m1 mutation type: neutral\n"
                "	initializeMutationType(\"m1\", 0.5, \"f\", 0.0);\n"
                "	\n"
                "	// g1 genomic element type: uses m1 for all mutations\n"
                "	initializeGenomicElementType(\"g1\", m1, 1.0);\n"
                "	\n"
                "	// uniform chromosome of length 100 kb with uniform recombination\n"
                "	initializeGenomicElement(g1, 0, 99999);\n"
                "	initializeRecombinationRate(1e-8);\n"
                "}\n"
                "\n"
                "// create a population of 500 individuals\n"
                "1 {\n"
                "	sim.addSubpop(\"p1\", 500);\n"
                "}\n"
                "\n"
                "// output samples of 10 genomes periodically, all fixed mutations at end\n"
                "1000 late() { p1.outputSample(10); }\n"
                "2000 late() { p1.outputSample(10); }\n"
                "2000 late() { sim.outputFixedMutations(); }\n");
}

QString QtSLiMWindow::defaultNonWFScriptString(void)
{
    return QString(
                "// set up a simple neutral nonWF simulation\n"
                "initialize() {\n"
                "	initializeSLiMModelType(\"nonWF\");\n"
                "	defineConstant(\"K\", 500);	// carrying capacity\n"
                "	\n"
                "	// neutral mutations, which are allowed to fix\n"
                "	initializeMutationType(\"m1\", 0.5, \"f\", 0.0);\n"
                "	m1.convertToSubstitution = T;\n"
                "	\n"
                "	initializeGenomicElementType(\"g1\", m1, 1.0);\n"
                "	initializeGenomicElement(g1, 0, 99999);\n"
                "	initializeMutationRate(1e-7);\n"
                "	initializeRecombinationRate(1e-8);\n"
                "}\n"
                "\n"
                "// each individual reproduces itself once\n"
                "reproduction() {\n"
                "	subpop.addCrossed(individual, p1.sampleIndividuals(1));\n"
                "}\n"
                "\n"
                "// create an initial population of 10 individuals\n"
                "1 early() {\n"
                "	sim.addSubpop(\"p1\", 10);\n"
                "}\n"
                "\n"
                "// provide density-dependent selection\n"
                "early() {\n"
                "	p1.fitnessScaling = K / p1.individualCount;\n"
                "}\n"
                "\n"
                "// output all fixed mutations at end\n"
                "2000 late() { sim.outputFixedMutations(); }\n");
}


//
//  public slots
//

void QtSLiMWindow::playOneStepClicked(void)
{
    qDebug() << "playOneStepClicked";
}

void QtSLiMWindow::playClicked(void)
{
    ui->playButton->setIcon(QIcon(ui->playButton->isChecked() ? ":/buttons/play_H.png" : ":/buttons/play.png"));

    qDebug() << "playClicked: isChecked() == " << ui->playButton->isChecked();
}

void QtSLiMWindow::profileClicked(void)
{
    ui->profileButton->setIcon(QIcon(ui->profileButton->isChecked() ? ":/buttons/profile_H.png" : ":/buttons/profile.png"));

    qDebug() << "profileClicked: isChecked() == " << ui->profileButton->isChecked();
}

void QtSLiMWindow::recycleClicked(void)
{
    qDebug() << "recycleClicked";
}

void QtSLiMWindow::showMutationsToggled(void)
{
    ui->showMutationsButton->setIcon(QIcon(ui->showMutationsButton->isChecked() ? ":/buttons/show_mutations_H.png" : ":/buttons/show_mutations.png"));

    qDebug() << "showMutationsToggled: isChecked() == " << ui->showMutationsButton->isChecked();
}

void QtSLiMWindow::showFixedSubstitutionsToggled(void)
{
    ui->showFixedSubstitutionsButton->setIcon(QIcon(ui->showFixedSubstitutionsButton->isChecked() ? ":/buttons/show_fixed_H.png" : ":/buttons/show_fixed.png"));

    qDebug() << "showFixedSubstitutionsToggled: isChecked() == " << ui->showFixedSubstitutionsButton->isChecked();
}

void QtSLiMWindow::showChromosomeMapsToggled(void)
{
    ui->showChromosomeMapsButton->setIcon(QIcon(ui->showChromosomeMapsButton->isChecked() ? ":/buttons/show_recombination_H.png" : ":/buttons/show_recombination.png"));

    qDebug() << "showRecombinationIntervalsToggled: isChecked() == " << ui->showChromosomeMapsButton->isChecked();
}

void QtSLiMWindow::showGenomicElementsToggled(void)
{
    ui->showGenomicElementsButton->setIcon(QIcon(ui->showGenomicElementsButton->isChecked() ? ":/buttons/show_genomicelements_H.png" : ":/buttons/show_genomicelements.png"));

    qDebug() << "showGenomicElementsToggled: isChecked() == " << ui->showGenomicElementsButton->isChecked();
}

void QtSLiMWindow::checkScriptClicked(void)
{
    qDebug() << "checkScriptClicked";
}

void QtSLiMWindow::prettyprintClicked(void)
{
    qDebug() << "prettyprintClicked";
}

void QtSLiMWindow::scriptHelpClicked(void)
{
    qDebug() << "showHelpClicked";
}

void QtSLiMWindow::showConsoleClicked(void)
{
    ui->consoleButton->setIcon(QIcon(ui->consoleButton->isChecked() ? ":/buttons/show_console_H.png" : ":/buttons/show_console.png"));

    qDebug() << "showConsoleClicked: isChecked() == " << ui->consoleButton->isChecked();
}

void QtSLiMWindow::showBrowserClicked(void)
{
    ui->browserButton->setIcon(QIcon(ui->browserButton->isChecked() ? ":/buttons/show_browser_H.png" : ":/buttons/show_browser.png"));

    qDebug() << "showBrowserClicked: isChecked() == " << ui->browserButton->isChecked();
}

void QtSLiMWindow::clearOutputClicked(void)
{
    qDebug() << "clearOutputClicked";
}

void QtSLiMWindow::dumpPopulationClicked(void)
{
    qDebug() << "dumpPopulationClicked";
}

void QtSLiMWindow::graphPopupButtonClicked(void)
{
    qDebug() << "graphButtonClicked";
}

void QtSLiMWindow::changeDirectoryClicked(void)
{
    qDebug() << "changeDirectoryClicked";
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
    ui->recycleButton->setIcon(QIcon(":/buttons/recycle_H.png"));
}
void QtSLiMWindow::recycleReleased(void)
{
    ui->recycleButton->setIcon(QIcon(":/buttons/recycle.png"));
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



















