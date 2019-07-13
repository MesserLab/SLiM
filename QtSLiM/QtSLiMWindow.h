#ifndef QTSLIMWINDOW_H
#define QTSLIMWINDOW_H

#include <QMainWindow>
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

public:
    std::string scriptString;	// the script string that we are running on right now; not the same as the script textview!
    SLiMSim *sim;				// the simulation instance for this window
    //SLiMgui *slimgui;			// the SLiMgui Eidos class instance for this window

    // state variables that are globals in Eidos and SLiM; we swap these in and out as needed, to provide each sim with its own context
    Eidos_RNG_State sim_RNG;
    slim_pedigreeid_t sim_next_pedigree_id;
    slim_mutationid_t sim_next_mutation_id;
    bool sim_suppress_warnings;
    std::string sim_working_dir;			// the current working dir that we will return to when executing SLiM/Eidos code
    std::string sim_requested_working_dir;	// the last working dir set by the user with the SLiMgui button/menu; we return to it on recycle

    // play-related variables; note that continuousPlayOn covers both profiling and non-profiling runs, whereas profilePlayOn
    // and nonProfilePlayOn cover those cases individually; this is for simplicity in enable bindings in the nib
    bool invalidSimulation, continuousPlayOn, profilePlayOn, nonProfilePlayOn, generationPlayOn, reachedSimulationEnd, hasImported;
    slim_generation_t targetGeneration;
    QElapsedTimer continuousPlayTimer;
    uint64_t continuousPlayGenerationsCompleted;
    int partialUpdateCount;
    //SLiMPlaySliderToolTipWindow *playSpeedToolTipWindow;

    // profiling-related variables
    //NSDate *profileEndDate;
    //clock_t profileElapsedCPUClock;
    //eidos_profile_t profileElapsedWallClock;
    //slim_generation_t profileStartGeneration;

    // display-related variables
    //double fitnessColorScale, selectionColorScale;
    //NSMutableDictionary *genomicElementColorRegistry;
    bool zoomedChromosomeShowsRateMaps;
    bool zoomedChromosomeShowsGenomicElements;
    bool zoomedChromosomeShowsMutations;
    bool zoomedChromosomeShowsFixedSubstitutions;
    //bool reloadingSubpopTableview;

public:
    explicit QtSLiMWindow(QWidget *parent = nullptr);
    ~QtSLiMWindow();

    void initializeUI(void);

    static QFont &defaultScriptFont(int *p_tabWidth);
    static std::string defaultWFScriptString(void);
    static std::string defaultNonWFScriptString(void);

    void setInvalidSimulation(bool p_invalid);
    void setReachedSimulationEnd(bool p_reachedEnd);
    void checkForSimulationTermination(void);
    void startNewSimulationFromScript(void);
    void setScriptStringAndInitializeSimulation(std::string string);
    void updateAfterTickFull(bool p_fullUpdate);

    void willExecuteScript(void);
    void didExecuteScript(void);
    bool runSimOneGeneration(void);

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
    Ui::QtSLiMWindow *ui;
};

#endif // QTSLIMWINDOW_H
