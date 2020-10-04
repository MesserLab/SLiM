//
//  QtSLiMWindow.h
//  SLiM
//
//  Created by Ben Haller on 7/11/2019.
//  Copyright (c) 2019-2020 Philipp Messer.  All rights reserved.
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

#include <string>
#include <vector>
#include <unordered_map>

#include "eidos_globals.h"
#include "slim_globals.h"
#include "eidos_rng.h"
#include "slim_sim.h"
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
class QtSLiMScriptTextEdit;


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
    
    // state variables that are globals in Eidos and SLiM; we swap these in and out as needed, to provide each sim with its own context
    Eidos_RNG_State sim_RNG = {};
    slim_pedigreeid_t sim_next_pedigree_id = 0;
    slim_mutationid_t sim_next_mutation_id = 0;
    bool sim_suppress_warnings = false;
    std::string sim_working_dir;			// the current working dir that we will return to when executing SLiM/Eidos code
    std::string sim_requested_working_dir;	// the last working dir set by the user with the SLiMgui button/menu; we return to it on recycle

    // play-related variables; note that continuousPlayOn covers profiling play, generation play, and normal play, whereas profilePlayOn,
    // generationPlayOn_, and nonProfilePlayOn_ cover those cases individually; this is for simplicity in enable bindings in the nib
    bool invalidSimulation_ = true, continuousPlayOn_ = false, profilePlayOn_ = false, nonProfilePlayOn_ = false;
    bool generationPlayOn_ = false, reachedSimulationEnd_ = false, hasImported_ = false;
    slim_generation_t targetGeneration_ = 0;
    QElapsedTimer continuousPlayElapsedTimer_;
    QTimer continuousPlayInvocationTimer_;
    uint64_t continuousPlayGenerationsCompleted_ = 0;
    QTimer continuousProfileInvocationTimer_;
    QTimer playOneStepInvocationTimer_;
    int partialUpdateCount_ = 0;
    std::clock_t elapsedCPUClock_ = 0;      // kept even when not profiling, for status bar updates

#if (defined(SLIMGUI) && (SLIMPROFILING == 1))
    // profiling-related variables
    QDateTime profileStartDate_;
    QDateTime profileEndDate_;
    std::clock_t profileElapsedCPUClock = 0;
    eidos_profile_t profileElapsedWallClock = 0;
    slim_generation_t profileStartGeneration = 0;
#endif
    
    QtSLiMPopulationTableModel *populationTableModel_ = nullptr;
    QtSLiMEidosConsole *consoleController = nullptr;
    QtSLiMTablesDrawer *tablesDrawerController = nullptr;
    
    int openedGraphCount_left = 0;      // used for new graph window positioning
    int openedGraphCount_right = 0;
    int openedGraphCount_top = 0;
    int openedGraphCount_bottom = 0;
    
public:
    bool isUntitled = false, isRecipe = false, isTransient = false;
    QString currentFile;
    
    std::string scriptString;	// the script string that we are running on right now; not the same as the script textview!
    SLiMSim *sim = nullptr;		// the simulation instance for this window
    SLiMgui *slimgui = nullptr;			// the SLiMgui Eidos class instance for this window

    // display-related variables
    //double fitnessColorScale, selectionColorScale;
    std::unordered_map<slim_objectid_t, QColor> genomicElementColorRegistry;
    bool zoomedChromosomeShowsRateMaps = false;
    bool zoomedChromosomeShowsGenomicElements = false;
    bool zoomedChromosomeShowsMutations = true;
    bool zoomedChromosomeShowsFixedSubstitutions = false;
    bool reloadingSubpopTableview = false;

public:
    typedef enum {
        WF = 0,
        nonWF
    } ModelType;
    
    QtSLiMWindow(QtSLiMWindow::ModelType modelType);                        // untitled window
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
    
    std::vector<Subpopulation*> selectedSubpopulations(void);
    void chromosomeSelection(bool *p_hasSelection, slim_position_t *p_selectionFirstBase, slim_position_t *p_selectionLastBase);
    const std::vector<slim_objectid_t> &chromosomeDisplayMuttypes(void);
    
    inline bool invalidSimulation(void) { return invalidSimulation_; }
    void setInvalidSimulation(bool p_invalid);
    inline bool reachedSimulationEnd(void) { return reachedSimulationEnd_; }
    void setReachedSimulationEnd(bool p_reachedEnd);
    inline bool isPlaying(void) { return continuousPlayOn_; }
    void setContinuousPlayOn(bool p_flag);
    void setGenerationPlayOn(bool p_flag);
    void setProfilePlayOn(bool p_flag);
    void setNonProfilePlayOn(bool p_flag);
    QtSLiMScriptTextEdit *scriptTextEdit(void);
    QtSLiMEidosConsole *ConsoleController(void) { return consoleController; }
    QtSLiMTablesDrawer *TablesDrawerController(void) { return tablesDrawerController; }
    
    void selectErrorRange(void);
    void checkForSimulationTermination(void);
    void startNewSimulationFromScript(void);
    void setScriptStringAndInitializeSimulation(std::string string);
    
    void updateOutputTextView(void);
    void updateGenerationCounter(void);
    void updateAfterTickFull(bool p_fullUpdate);
    void updatePlayButtonIcon(bool pressed);
    void updateProfileButtonIcon(bool pressed);
    void updateRecycleButtonIcon(bool pressed);
    void updateUIEnabling(void);
    void updateMenuEnablingACTIVE(QWidget *focusWidget);
    void updateMenuEnablingINACTIVE(QWidget *focusWidget, QWidget *focusWindow);
    void updateMenuEnablingSHARED(QWidget *focusWidget);
    void updateVariableBrowserButtonState(bool visible);
    void updateWindowMenu(void);

    void colorScriptWithProfileCountsFromNode(const EidosASTNode *node, double elapsedTime, int32_t baseIndex, QTextDocument *doc, QTextCharFormat &baseFormat);
    void displayProfileResults(void);
    void startProfiling(void);
    void endProfiling(void);
    
    void willExecuteScript(void);
    void didExecuteScript(void);
    bool runSimOneGeneration(void);
    void _continuousPlay(void);
    void _continuousProfile(void);
    void _playOneStep(void);
    
    enum PlayType {
        kNormalPlay = 0,
        kProfilePlay,
        kGenerationPlay,
    };
    void playOrProfile(PlayType playType);
    
    bool windowIsReuseable(void);   // requires isUntitled, !isRecipe, isTransient, and other conditions
    void updateChangeCount(void);
    bool changedSinceRecycle(void);
    void resetSLiMChangeCount(void);
    void scriptTexteditChanged(void);
    
    bool checkScriptSuppressSuccessResponse(bool suppressSuccessResponse);    
    
    //	Eidos SLiMgui method forwards
    void eidos_openDocument(QString path);
    void eidos_pauseExecution(void);
    
signals:
    void terminationWithMessage(QString message);
    void playStateChanged(void);
    
    void controllerUpdatedAfterTick(void);
    void controllerSelectionChanged(void);
    void controllerGenerationFinished(void);
    void controllerRecycled(void);
    
public slots:
    void showTerminationMessage(QString terminationMessage);
    
    void playOneStepClicked(void);
    void generationChanged(void);
    void recycleClicked(void);
    void playSpeedChanged(void);

    void toggleDrawerToggled(void);
    void showMutationsToggled(void);
    void showFixedSubstitutionsToggled(void);
    void showChromosomeMapsToggled(void);
    void showGenomicElementsToggled(void);

    void showConsoleClicked(void);
    void showBrowserClicked(void);
    void jumpToPopupButtonRunMenu(void);

    void clearOutputClicked(void);
    void dumpPopulationClicked(void);
    void graphPopupButtonRunMenu(void);
    void changeDirectoryClicked(void);

    void subpopSelectionDidChange(const QItemSelection &selected, const QItemSelection &deselected);
    
    //
    //  UI glue, defined in QtSLiMWindow_glue.cpp
    //
    
private slots:
    bool save(void);
    bool saveAs(void);
    void revert(void);
    void documentWasModified(void);
    
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
    void showMutationsPressed(void);
    void showMutationsReleased(void);
    void showFixedSubstitutionsPressed(void);
    void showFixedSubstitutionsReleased(void);
    void showChromosomeMapsPressed(void);
    void showChromosomeMapsReleased(void);
    void showGenomicElementsPressed(void);
    void showGenomicElementsReleased(void);

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
    void graphPopupButtonPressed(void);
    void graphPopupButtonReleased(void);
    void changeDirectoryPressed(void);
    void changeDirectoryReleased(void);
    
    void finish_eidos_pauseExecution(void);
    
protected:
    virtual void closeEvent(QCloseEvent *event) override;
    virtual void moveEvent(QMoveEvent *event) override;
    virtual void resizeEvent(QResizeEvent *event) override;
    virtual void showEvent(QShowEvent *event) override;
    void positionNewSubsidiaryWindow(QWidget *window);
    QWidget *graphWindowWithView(QtSLiMGraphView *graphView);
    QtSLiMGraphView *graphViewForGraphWindow(QWidget *window);
    
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
    
private:
    void glueUI(void);
    Ui::QtSLiMWindow *ui;
};

#endif // QTSLIMWINDOW_H





















