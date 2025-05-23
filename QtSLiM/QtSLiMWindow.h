//
//  QtSLiMWindow.h
//  SLiM
//
//  Created by Ben Haller on 7/11/2019.
//  Copyright (c) 2019-2025 Benjamin C. Haller.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/slim/
//

//	This file is part of SLiM.
//
//	SLiM is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
//
//	SLiM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License along with SLiM.  If not, see <http://www.gnu.org/licenses/>.

#ifndef QTSLIMWINDOW_H
#define QTSLIMWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QElapsedTimer>
#include <QColor>
#include <QDateTime>
#include <QApplication>

#include <string>
#include <vector>
#include <unordered_map>
#include <ctime>

#include "eidos_globals.h"
#include "slim_globals.h"
#include "eidos_rng.h"
#include "community.h"
#include "species.h"
#include "QtSLiMExtras.h"
#include "QtSLiMPopulationTable.h"

class Subpopulation;
class QCloseEvent;
class QTextCursor;
class QtSLiMEidosConsole;
class QtSLiMTablesDrawer;
class QItemSelection;
class SLiMgui;
class QtSLiMGraphView;
class QtSLiMGraphView_CustomPlot;
class Plot;
class QtSLiMScriptTextEdit;
class QtSLiMTextEdit;
class QtSLiMDebugOutputWindow;
class QtSLiMChromosomeWidget;
class QtSLiMChromosomeWidgetController;
class LogFile;


namespace Ui {
class QtSLiMWindow;
}

class QtSLiMWindow : public QMainWindow
{
    Q_OBJECT    

private:
    // basic file i/o and change count management
    void init(void);
    void initializeUI(void);
    bool maybeSave(void);
    bool saveFile(const QString &fileName);
    void setCurrentFile(const QString &fileName);
    
    int slimChangeCount = 0;                    // private change count governing the recycle button's highlight
    
    QString lastSavedString;                    // the last string saved to disk, or initial script string
    QDateTime lastSavedDate;                    // the date when we last saved, to detect external changes
    bool scriptChangeObserved = false;          // has a change to the script been observed since last saved?
    bool isScriptModified(void);                // uses scriptChangeObserved / lastSavedString to determine modified status
    
    // tracking our interaction with the user about the file on disk, mod dates, external editing, etc.
    // the goal here is to avoid warning more than once; see https://github.com/MesserLab/SLiM/issues/476
    bool warnedAboutUnreadabilityOnDisk = false;    // avoid repeating this unless it gets fixed
    bool warnedAboutNotExistingOnDisk = false;      // avoid repeating this unless it gets fixed
    bool warnedAboutExternalEditing = false;        // avoid repeating this unless it gets fixed or changes
    QString lastExternalChangeString;               // the string we last observed on disk and warned about
    bool currentlyWarningAboutDiskFile = false;     // a flag to avoid re-entrancy for this window
    
    // state variables that are globals in Eidos and SLiM; we swap these in and out as needed, to provide each sim with its own context
	bool sim_RNG_initialized = false;
    Eidos_RNG_State sim_RNG;                // QtSLiM never runs multithreaded, so we do not need the _PERTHREAD variant here
    slim_pedigreeid_t sim_next_pedigree_id = 0;
    slim_mutationid_t sim_next_mutation_id = 0;
    bool sim_suppress_warnings = false;
    std::string sim_working_dir;			// the current working dir that we will return to when executing SLiM/Eidos code
    std::string sim_requested_working_dir;	// the last working dir set by the user with the SLiMgui button/menu; we return to it on recycle

    // play-related variables; note that continuousPlayOn covers profiling play, tick play, and normal play, whereas profilePlayOn,
    // tickPlayOn_, and nonProfilePlayOn_ cover those cases individually; this is for simplicity in enable bindings in the nib
    bool invalidSimulation_ = true, continuousPlayOn_ = false, profilePlayOn_ = false, nonProfilePlayOn_ = false;
    bool tickPlayOn_ = false, reachedSimulationEnd_ = false, hasImported_ = false;
    slim_tick_t targetTick_ = 0;
    QElapsedTimer continuousPlayElapsedTimer_;
    QTimer continuousPlayInvocationTimer_;
    uint64_t continuousPlayTicksCompleted_ = 0;
    QTimer continuousProfileInvocationTimer_;
    QTimer playOneStepInvocationTimer_;
    int partialUpdateCount_ = 0;
    std::clock_t elapsedCPUClock_ = 0;      // kept even when not profiling, for status bar updates

    QtSLiMPopulationTableModel *populationTableModel_ = nullptr;
    QtSLiMEidosConsole *consoleController = nullptr;
    QtSLiMTablesDrawer *tablesDrawerController = nullptr;
    
    QtSLiMDebugOutputWindow *debugOutputWindow_ = nullptr;
    QTimer debugButtonFlashTimer_;
    int debugButtonFlashCount_ = 0;
    
    int openedGraphCount_left = 0;      // used for new graph window positioning
    int openedGraphCount_right = 0;
    int openedGraphCount_top = 0;
    int openedGraphCount_bottom = 0;
    
public:
    bool isZombieWindow_ = false;   // set when the UI is invalidated, to avoid various issues
    bool isUntitled = false, isRecipe = false, isTransient = false;
    QString currentFile;
    
    std::string scriptString;	// the script string that we are running on right now; not the same as the script textview!
    Community *community = nullptr;		// the simulation instance for this window
    Species *focalSpecies = nullptr;    // NOT OWNED: a pointer to the focal species in community; do not use, call focalDisplaySpecies()
    std::string focalSpeciesName;       // the name of the focal species (or "all"), for persistence across recycles
    SLiMgui *slimgui = nullptr;			// the SLiMgui Eidos class instance for this window

    // display-related variables
    std::unordered_map<slim_objectid_t, QColor> genomicElementColorRegistry;
    bool reloadingSubpopTableview = false;
    bool reloadingSpeciesBar = false;
    
    // chromosome view configuration, applied to all chromosome views in multispecies models
    QtSLiMChromosomeWidgetController *chromosomeConfig = nullptr;
    
public:
    typedef enum {
        WF = 0,
        nonWF
    } ModelType;
    
    QtSLiMWindow(QtSLiMWindow::ModelType modelType, bool includeComments);  // untitled window
    explicit QtSLiMWindow(const QString &fileName);                         // window from a file
    QtSLiMWindow(const QString &recipeName, const QString &recipeScript);   // window from a recipe
    virtual ~QtSLiMWindow() override;
    
    void tile(const QMainWindow *previous);
    void displayStartupMessage(void);
    void loadFile(const QString &fileName);                                     // loads a file into an existing window
    void loadRecipe(const QString &recipeName, const QString &recipeScript);    // loads a recipe into an existing window
    QWidget *imageWindowWithPath(const QString &path);                          // creates an image window subsidiary to the receiver
    
    static const QColor &blackContrastingColorForIndex(int index);
    static const QColor &whiteContrastingColorForIndex(int index);
    void colorForGenomicElementType(GenomicElementType *elementType, slim_objectid_t elementTypeID, float *p_red, float *p_green, float *p_blue, float *p_alpha);
    void colorForSpecies(Species *species, float *p_red, float *p_green, float *p_blue, float *p_alpha);
    QColor qcolorForSpecies(Species *species);
    
    std::vector<Subpopulation *> listedSubpopulations(void);
    std::vector<Subpopulation*> selectedSubpopulations(void);
    
    inline bool invalidSimulation(void) { return invalidSimulation_; }
    void setInvalidSimulation(bool p_invalid);
    inline bool reachedSimulationEnd(void) { return reachedSimulationEnd_; }
    void setReachedSimulationEnd(bool p_reachedEnd);
    inline bool isPlaying(void) { return continuousPlayOn_; }
    void setContinuousPlayOn(bool p_flag);
    void setTickPlayOn(bool p_flag);
    void setProfilePlayOn(bool p_flag);
    void setNonProfilePlayOn(bool p_flag);
    QtSLiMScriptTextEdit *scriptTextEdit(void);
    QtSLiMTextEdit *outputTextEdit(void);
    QtSLiMEidosConsole *ConsoleController(void) { return consoleController; }
    QtSLiMTablesDrawer *TablesDrawerController(void) { return tablesDrawerController; }
    
    QtSLiMDebugOutputWindow *debugOutputWindow(void) { return debugOutputWindow_; }
    void flashDebugButton(void);
    void stopDebugButtonFlash(void);
    
    void checkForSimulationTermination(void);
    void startNewSimulationFromScript(void);
    void setScriptStringAndInitializeSimulation(std::string string);
    
    Species *focalDisplaySpecies(void);
    Chromosome *focalChromosome(void);
    
    void updateOutputViews(void);
    void updateTickCounter(void);
    void updateSpeciesBar(void);
    void updateChromosomeViewSetup(void);
    void updateAfterTickFull(bool p_fullUpdate);
    void updatePlayButtonIcon(bool pressed);
    void updateProfileButtonIcon(bool pressed);
    void updateRecycleButtonIcon(bool pressed);
    void updateUIEnabling(void);
    void updateMenuEnablingACTIVE(QWidget *focusWidget);
    void updateMenuEnablingINACTIVE(QWidget *focusWidget, QWidget *focusWindow);
    void updateMenuEnablingSHARED(QWidget *focusWidget);
    void updateWindowMenu(void);

    void colorScriptWithProfileCountsFromNode(const EidosASTNode *node, double elapsedTime, int32_t baseIndex, QTextDocument *doc, QTextCharFormat &baseFormat);
    void displayProfileResults(void);
    
    void willExecuteScript(void);
    void didExecuteScript(void);
    bool runSimOneTick(void);
    void _continuousPlay(void);
    void _continuousProfile(void);
    void _playOneStep(void);
    
    enum PlayType {
        kNormalPlay = 0,
        kProfilePlay,
        kTickPlay,
    };
    void playOrProfile(PlayType playType);
    
    bool windowIsReuseable(void);   // requires isUntitled, !isRecipe, isTransient, and other conditions
    void updateChangeCount(void);
    bool changedSinceRecycle(void);
    void resetSLiMChangeCount(void);
    void scriptTexteditChanged(void);
    void setScriptBlockLabelTextFromSelection(void);
    
    bool checkScriptSuppressSuccessResponse(bool suppressSuccessResponse);   
    
    bool offerAndExecuteAutofix(QTextCursor target, QString replacement, QString explanation, QString terminationMessage);
    bool checkTerminationForAutofix(QString terminationMessage);
    
    //	Eidos SLiMgui method forwards
    EidosValue_SP eidos_logFileData(LogFile *logFile, EidosValue *column_value);
    void eidos_openDocument(QString path);
    void eidos_pauseExecution(void);
    QtSLiMGraphView_CustomPlot *eidos_createPlot(QString title, double *x_range, double *y_range, QString x_label, QString y_label, double width, double height, bool horizontalGrid, bool verticalGrid, bool fullBox, double axisLabelSize, double tickLabelSize);
    QtSLiMGraphView_CustomPlot *eidos_plotWithTitle(QString title);
    
    void plotLogFileData_1D(QString title, QString y_title, double *y_values, int data_count);
    void plotLogFileData_2D(QString title, QString x_title, QString y_title, double *x_values, double *y_values, int data_count, bool makeScatterPlot);
    
signals:
    void terminationWithMessage(QString message, EidosErrorContext errorContext);
    void playStateChanged(void);
    void controllerChangeCountChanged(int changeCount);
    
    void controllerPartialUpdateAfterTick(void);
    void controllerFullUpdateAfterTick(void);
    void controllerTickFinished(void);
    void controllerRecycled(void);
    
public slots:
    void showTerminationMessage(QString terminationMessage, EidosErrorContext errorContext);
    
    void playOneStepClicked(void);
    void tickChanged(void);
    void recycleClicked(void);
    void playSpeedChanged(void);

    void showDrawerClicked(void);
    void chromosomeDisplayPopupButtonRunMenu(void);
    void showConsoleClicked(void);
    void showBrowserClicked(void);
    void jumpToPopupButtonRunMenu(void);

    void clearOutputClicked(void);
    void clearDebugPointsClicked(void);
    void dumpPopulationClicked(void);
    void debugOutputClicked(void);
    void graphPopupButtonRunMenu(void);
    void changeDirectoryClicked(void);
    void displayGraphClicked(void);

    void selectedSpeciesChanged(void);
    void subpopSelectionDidChange(const QItemSelection &selected, const QItemSelection &deselected);
    
    //
    //  UI glue, defined in QtSLiMWindow_glue.cpp
    //
    
private slots:
    void displayFontPrefChanged(void);
    void applicationPaletteChanged(void);
    
    bool save(void);
    bool saveAs(void);
    void revert(void);
    void documentWasModified(void);
    void appStateChanged(Qt::ApplicationState state);
    
    void playOneStepPressed(void);
    void playOneStepReleased(void);
    void playPressed(void);
    void playReleased(void);
    void profilePressed(void);
    void profileReleased(void);
    void recyclePressed(void);
    void recycleReleased(void);

    void toggleDrawerPressed(void);
    void toggleDrawerReleased(void);
    void chromosomeActionPressed(void);
    void chromosomeActionReleased(void);
    void chromosomeDisplayPressed(void);
    void chromosomeDisplayReleased(void);

    void clearDebugPressed(void);
    void clearDebugReleased(void);
    void checkScriptPressed(void);
    void checkScriptReleased(void);
    void prettyprintPressed(void);
    void prettyprintReleased(void);
    void scriptHelpPressed(void);
    void scriptHelpReleased(void);
    void showConsolePressed(void);
    void showConsoleReleased(void);
    void showBrowserPressed(void);
    void showBrowserReleased(void);
    void jumpToPopupButtonPressed(void);
    void jumpToPopupButtonReleased(void);

    void clearOutputPressed(void);
    void clearOutputReleased(void);
    void dumpPopulationPressed(void);
    void dumpPopulationReleased(void);
    void debugOutputPressed(void);
    void debugOutputReleased(void);
    void graphPopupButtonPressed(void);
    void graphPopupButtonReleased(void);
    void changeDirectoryPressed(void);
    void changeDirectoryReleased(void);
    
    void handleDebugButtonFlash(void);
    
    void finish_eidos_pauseExecution(void);
    
protected:
    virtual void closeEvent(QCloseEvent *p_event) override;
    virtual void moveEvent(QMoveEvent *p_event) override;
    virtual void resizeEvent(QResizeEvent *p_event) override;
    virtual void showEvent(QShowEvent *p_event) override;
    void positionNewSubsidiaryWindow(QWidget *window);
    QWidget *graphWindowWithView(QtSLiMGraphView *graphView, double windowWidth=300, double windowHeight=300);
    QtSLiMGraphView *graphViewForGraphWindow(QWidget *window);
    QWidget *newChromosomeDisplay(std::string chromosome_symbol, QString windowTitle);  // pass "" for all chromosomes, or a symbol for one chromosome
    
    // used to suppress saving of resize/position info until we are fully constructed
    bool donePositioning_ = false;
    
    // splitter support
    void interpolateVerticalSplitter(void);
    QWidget *overallTopWidget = nullptr;
    QWidget *overallBottomWidget = nullptr;
    QSplitter *overallSplitter = nullptr;
    
    void interpolateHorizontalSplitter(void);
    QWidget *scriptWidget = nullptr;
    QWidget *outputWidget = nullptr;
    QSplitter *bottomSplitter = nullptr;
    
    void interpolateSplitters(void);
    
    // multispecies chromosome view support
    std::vector<QVBoxLayout *> chromosomeWidgetLayouts;
    std::vector<QtSLiMChromosomeWidget *> chromosomeOverviewWidgets;
    std::vector<QtSLiMChromosomeWidget *> chromosomeZoomedWidgets;
    
    void removeExtraChromosomeViews(void);
    void addChromosomeWidgets(QVBoxLayout *chromosomeLayout, QtSLiMChromosomeWidget *overviewWidget, QtSLiMChromosomeWidget *zoomedWidget);
    void runChromosomeContextMenuAtPoint(QPoint p_globalPoint);
    
private:
    void glueUI(void);
    void invalidateUI(void);
    QtSLiMGraphView *graphViewWithTitle(QString title);
    int graphViewCount(void);
    
    Ui::QtSLiMWindow *ui;
};

#endif // QTSLIMWINDOW_H





















