#ifndef QTSLIMWINDOW_H
#define QTSLIMWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QElapsedTimer>

#include <string>

#include "eidos_globals.h"
#include "slim_globals.h"
#include "eidos_rng.h"
#include "slim_sim.h"
//#include "slim_gui.h"


namespace Ui {
class QtSLiMWindow;
}

class QtSLiMWindow : public QMainWindow
{
    Q_OBJECT    

private:
    int slimChangeCount = 0;                    // private change count governing the recycle button's highlight
    
    // state variables that are globals in Eidos and SLiM; we swap these in and out as needed, to provide each sim with its own context
    Eidos_RNG_State sim_RNG = {};
    slim_pedigreeid_t sim_next_pedigree_id = 0;
    slim_mutationid_t sim_next_mutation_id = 0;
    bool sim_suppress_warnings = false;
    std::string sim_working_dir;			// the current working dir that we will return to when executing SLiM/Eidos code
    std::string sim_requested_working_dir;	// the last working dir set by the user with the SLiMgui button/menu; we return to it on recycle

    // play-related variables; note that continuousPlayOn covers both profiling and non-profiling runs, whereas profilePlayOn
    // and nonProfilePlayOn cover those cases individually; this is for simplicity in enable bindings in the nib
    bool invalidSimulation_ = false, continuousPlayOn_ = false, profilePlayOn_ = false, nonProfilePlayOn_ = false;
    bool generationPlayOn_ = false, reachedSimulationEnd_ = false, hasImported_ = false;
    slim_generation_t targetGeneration_ = 0;
    QElapsedTimer continuousPlayElapsedTimer_;
    QTimer continuousPlayInvocationTimer_;
    uint64_t continuousPlayGenerationsCompleted_ = 0;
    int partialUpdateCount_ = 0;
    //SLiMPlaySliderToolTipWindow *playSpeedToolTipWindow;

#if (defined(SLIMGUI) && (SLIMPROFILING == 1))
    // profiling-related variables
    //NSDate *profileEndDate = nullptr;
    //clock_t profileElapsedCPUClock = 0;
    //eidos_profile_t profileElapsedWallClock = 0;
    //slim_generation_t profileStartGeneration = 0;
#endif
    
public:
    std::string scriptString;	// the script string that we are running on right now; not the same as the script textview!
    SLiMSim *sim = nullptr;		// the simulation instance for this window
    //SLiMgui *slimgui = nullptr;			// the SLiMgui Eidos class instance for this window

    // display-related variables
    //double fitnessColorScale, selectionColorScale;
    //NSMutableDictionary *genomicElementColorRegistry;
    bool zoomedChromosomeShowsRateMaps = false;
    bool zoomedChromosomeShowsGenomicElements = false;
    bool zoomedChromosomeShowsMutations = true;
    bool zoomedChromosomeShowsFixedSubstitutions = false;
    //bool reloadingSubpopTableview = false;

public:
    explicit QtSLiMWindow(QWidget *parent = nullptr);
    ~QtSLiMWindow();

    void initializeUI(void);

    static QFont &defaultScriptFont(int *p_tabWidth);
    static std::string defaultWFScriptString(void);
    static std::string defaultNonWFScriptString(void);

    void setInvalidSimulation(bool p_invalid);
    void setReachedSimulationEnd(bool p_reachedEnd);
    void setContinuousPlayOn(bool p_flag);
    void setNonProfilePlayOn(bool p_flag);
    
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

    void willExecuteScript(void);
    void didExecuteScript(void);
    bool runSimOneGeneration(void);
    void _continuousPlay(void);
    void playOrProfile(bool isPlayAction);
    
    void updateChangeCount(void);
    bool changedSinceRecycle(void);
    void resetSLiMChangeCount(void);
    void scriptTexteditChanged(void);
    
signals:
    void terminationWithMessage(QString message);
    
public slots:
    void showTerminationMessage(QString terminationMessage);
    
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

    //
    //  UI glue, defined in QtSLiMWindow_glue.cpp
    //
    
private slots:
    void playOneStepPressed(void);
    void playOneStepReleased(void);
    void playPressed(void);
    void playReleased(void);
    void profilePressed(void);
    void profileReleased(void);
    void recyclePressed(void);
    void recycleReleased(void);

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

    void clearOutputPressed(void);
    void clearOutputReleased(void);
    void dumpPopulationPressed(void);
    void dumpPopulationReleased(void);
    void graphPopupButtonPressed(void);
    void graphPopupButtonReleased(void);
    void changeDirectoryPressed(void);
    void changeDirectoryReleased(void);

private:
    void glueUI(void);
    Ui::QtSLiMWindow *ui;
};

#endif // QTSLIMWINDOW_H





















