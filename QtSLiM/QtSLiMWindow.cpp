#include "QtSLiMWindow.h"
#include "ui_QtSLiMWindow.h"

#include <QCoreApplication>
#include <QtDebug>

// TO DO:
//
// custom layout for play/profile buttons: https://doc.qt.io/qt-5/layout.html
// splitviews for the window: https://stackoverflow.com/questions/28309376/how-to-manage-qsplitter-in-qt-designer
// make the button highlighting/tracking/icons work properly:
// set up the app icon correctly

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

    connect(ui->playButton, &QPushButton::pressed, this, &QtSLiMWindow::playPressed);
    connect(ui->playButton, &QPushButton::released, this, &QtSLiMWindow::playReleased);

    // fix the layout of the window
    ui->scriptHeaderLayout->setSpacing(4);
    ui->scriptHeaderLayout->setMargin(0);
    ui->scriptHeaderLabel->setContentsMargins(8, 0, 15, 0);

    ui->outputHeaderLayout->setSpacing(4);
    ui->outputHeaderLayout->setMargin(0);
    ui->outputHeaderLabel->setContentsMargins(8, 0, 15, 0);

    ui->chromosomeButtonsLayout->setSpacing(4);

    // set up the script and output textedits
    ui->scriptTextEdit->setFontFamily("Menlo");
    ui->scriptTextEdit->setFontPointSize(11);
    ui->scriptTextEdit->setTabStopWidth(20);    // should use setTabStopDistance(), see https://stackoverflow.com/a/54605709/2752221
    ui->scriptTextEdit->setText(QtSLiMWindow::defaultWFScriptString());

    ui->outputTextEdit->setFontFamily("Menlo");
    ui->outputTextEdit->setFontPointSize(11);
    ui->scriptTextEdit->setTabStopWidth(20);    // should use setTabStopDistance(), see https://stackoverflow.com/a/54605709/2752221
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
    qDebug() << "playClicked";
}

void QtSLiMWindow::profileClicked(void)
{
    qDebug() << "profileClicked";
}

void QtSLiMWindow::recycleClicked(void)
{
    qDebug() << "recycleClicked";
}

void QtSLiMWindow::showMutationsToggled(void)
{
    qDebug() << "showMutationsToggled";
}

void QtSLiMWindow::showFixedSubstitutionsToggled(void)
{
    qDebug() << "showFixedSubstitutionsToggled";
}

void QtSLiMWindow::showChromosomeMapsToggled(void)
{
    qDebug() << "showRecombinationIntervalsToggled";
}

void QtSLiMWindow::showGenomicElementsToggled(void)
{
    qDebug() << "showGenomicElementsToggled";
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
    qDebug() << "showConsoleClicked";
}

void QtSLiMWindow::showBrowserClicked(void)
{
    qDebug() << "showBrowserClicked";
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

void QtSLiMWindow::playPressed(void)
{
    ui->playButton->setIcon(QIcon(":/buttons/play_H.png"));
}

void QtSLiMWindow::playReleased(void)
{
    ui->playButton->setIcon(QIcon(":/buttons/play.png"));
}




















