//
//  SLiMWindowController.h
//  SLiM
//
//  Created by Ben Haller on 1/21/15.
//  Copyright (c) 2015-2020 Philipp Messer.  All rights reserved.
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


#import <Cocoa/Cocoa.h>

#include "eidos_rng.h"
#include "slim_sim.h"
#include "slim_gui.h"
#import "ChromosomeView.h"
#import "PopulationView.h"
#import "GraphView.h"
#import "CocoaExtra.h"
#import "EidosTextView.h"
#import "EidosTextViewDelegate.h"
#import "EidosConsoleTextView.h"
#import "EidosConsoleWindowController.h"
#import "EidosConsoleWindowControllerDelegate.h"


@class SLiMHaplotypeGraphView;


@interface SLiMWindowController : NSWindowController <NSTableViewDelegate, NSTableViewDataSource, NSSplitViewDelegate, NSTextViewDelegate, EidosConsoleWindowControllerDelegate, EidosTextViewDelegate>
{
@public
	NSString *scriptString;		// the script string that we are running on right now; not the same as the script textview!
	SLiMSim *sim;				// the simulation instance for this window
	SLiMgui *slimgui;			// the SLiMgui Eidos class instance for this window
	
	// state variables that are globals in Eidos and SLiM; we swap these in and out as needed, to provide each sim with its own context
	Eidos_RNG_State sim_RNG;
	slim_pedigreeid_t sim_next_pedigree_id;
	slim_mutationid_t sim_next_mutation_id;
	bool sim_suppress_warnings;
	std::string sim_working_dir;			// the current working dir that we will return to when executing SLiM/Eidos code
	std::string sim_requested_working_dir;	// the last working dir set by the user with the SLiMgui button/menu; we return to it on recycle
	
	// play-related variables; note that continuousPlayOn covers both profiling and non-profiling runs, whereas profilePlayOn
	// and nonProfilePlayOn cover those cases individually; this is for simplicity in enable bindings in the nib
	BOOL invalidSimulation, continuousPlayOn, profilePlayOn, nonProfilePlayOn, generationPlayOn, reachedSimulationEnd, hasImported;
	slim_generation_t targetGeneration;
	NSDate *continuousPlayStartDate;
	uint64_t continuousPlayGenerationsCompleted;
	int partialUpdateCount;
	SLiMPlaySliderToolTipWindow *playSpeedToolTipWindow;
	
#if (defined(SLIMGUI) && (SLIMPROFILING == 1))
	// profiling-related variables
	NSDate *profileEndDate;
	std::clock_t profileElapsedCPUClock;
	eidos_profile_t profileElapsedWallClock;
	slim_generation_t profileStartGeneration;
#endif
	
	// display-related variables
	double fitnessColorScale, selectionColorScale;
	NSMutableDictionary *genomicElementColorRegistry;
	BOOL zoomedChromosomeShowsRateMaps;
	BOOL zoomedChromosomeShowsGenomicElements;
	BOOL zoomedChromosomeShowsMutations;
	BOOL zoomedChromosomeShowsFixedSubstitutions;
	BOOL reloadingSubpopTableview;
	
	// outlets
	IBOutlet NSButton *buttonForDrawer;
	IBOutlet NSDrawer *drawer;
	
	IBOutlet NSTableView *mutTypeTableView;
	IBOutlet NSTableColumn *mutTypeIDColumn;
	IBOutlet NSTableColumn *mutTypeDominanceColumn;
	IBOutlet NSTableColumn *mutTypeDFETypeColumn;
	IBOutlet NSTableColumn *mutTypeDFEParamsColumn;
	
	IBOutlet NSTableView *genomicElementTypeTableView;
	IBOutlet NSTableColumn *genomicElementTypeIDColumn;
	IBOutlet NSTableColumn *genomicElementTypeColorColumn;
	IBOutlet NSTableColumn *genomicElementTypeMutationTypesColumn;
	
	IBOutlet NSTableView *interactionTypeTableView;
	IBOutlet NSTableColumn *interactionTypeIDColumn;
	IBOutlet NSTableColumn *interactionTypeMaxDistanceColumn;
	IBOutlet NSTableColumn *interactionTypeIFTypeColumn;
	IBOutlet NSTableColumn *interactionTypeIFParamsColumn;
	
	IBOutlet NSTableView *scriptBlocksTableView;
	IBOutlet NSTableColumn *scriptBlocksIDColumn;
	IBOutlet NSTableColumn *scriptBlocksStartColumn;
	IBOutlet NSTableColumn *scriptBlocksEndColumn;
	IBOutlet NSTableColumn *scriptBlocksTypeColumn;
	
	IBOutlet NSSplitView *overallSplitView;
	IBOutlet NSView *overallTopView;
	IBOutlet NSLayoutConstraint *overallTopViewConstraint1;
	IBOutlet NSLayoutConstraint *overallTopViewConstraint2;
	IBOutlet NSLayoutConstraint *overallTopViewConstraint3;
	IBOutlet NSLayoutConstraint *overallTopViewConstraint4;
	
	IBOutlet NSTextField *fitnessTitleTextField;
	IBOutlet SLiMColorStripeView *fitnessColorStripe;
	IBOutlet NSSlider *fitnessColorSlider;
	IBOutlet NSTextField *selectionTitleTextField;
	IBOutlet SLiMColorStripeView *selectionColorStripe;
	IBOutlet NSSlider *selectionColorSlider;
	
	IBOutlet NSButton *playOneStepButton;
	IBOutlet NSButton *playButton;
	IBOutlet NSButton *profileButton;
	IBOutlet NSButton *recycleButton;
	IBOutlet NSSlider *playSpeedSlider;
	IBOutlet NSTextField *generationTextField;
	IBOutlet NSProgressIndicator *generationProgressIndicator;
	
	IBOutlet NSSplitView *bottomSplitView;
	IBOutlet EidosTextView *scriptTextView;
	IBOutlet NSTextField *scriptStatusTextField;
	IBOutlet EidosTextView *outputTextView;
	IBOutlet NSButton *consoleButton;
	IBOutlet NSButton *browserButton;
	
	IBOutlet NSTableView *subpopTableView;
	IBOutlet NSTableColumn *subpopIDColumn;
	IBOutlet NSTableColumn *subpopSizeColumn;
	IBOutlet NSTableColumn *subpopSelfingRateColumn;
	IBOutlet NSTableColumn *subpopMaleCloningRateColumn;
	IBOutlet NSTableColumn *subpopFemaleCloningRateColumn;
	IBOutlet NSTableColumn *subpopSexRatioColumn;
	
	IBOutlet PopulationView *populationView;
	IBOutlet PopulationErrorView *populationErrorView;
	
	IBOutlet ChromosomeView *chromosomeOverview;
	IBOutlet ChromosomeView *chromosomeZoomed;
	IBOutlet NSButton *showRecombinationIntervalsButton;
	IBOutlet NSButton *showGenomicElementsButton;
	IBOutlet NSButton *showMutationsButton;
	IBOutlet NSButton *showFixedSubstitutionsButton;
	
	IBOutlet SLiMMenuButton *outputCommandsButton;
	IBOutlet NSMenu *outputCommandsMenu;
	
	IBOutlet SLiMMenuButton *graphCommandsButton;
	IBOutlet NSMenu *graphCommandsMenu;
	
	IBOutlet SLiMMenuButton *genomeCommandsButton;
	IBOutlet NSMenu *genomeCommandsMenu;
	
	// Graph window ivars
	IBOutlet NSWindow *graphWindow;				// outlet for GraphWindow.xib; note this does not stay wired up, it is just used transiently
	
	NSWindow *graphWindowMutationFreqSpectrum;
	NSWindow *graphWindowMutationFreqTrajectories;
	NSWindow *graphWindowMutationLossTimeHistogram;
	NSWindow *graphWindowMutationFixationTimeHistogram;
	NSWindow *graphWindowFitnessOverTime;
	NSWindow *graphWindowPopulationVisualization;
		// don't forget to add new graph windows in -dealloc, -updateAfterTick, and -windowWillClose:
	
	int openedGraphCount;						// used for new graph window positioning
	
	// Other linked windows, such as the haplotype snapshot
	NSMutableArray *linkedWindows;
	
	// Profile Report window ivars
	IBOutlet NSWindow *profileWindow;		// outlet for ProfileReport.xib; note this does not stay wired up, it is just used transiently
	IBOutlet NSTextView *profileTextView;	// ditto
	
	// Haplotype plot progress panel
	NSLock *haplotypeProgressLock;
	BOOL haplotypeProgressTaskCancelled;
	
	SLiMHaplotypeGraphView *newHaplotypeGraphView;
	
	int haplotypeProgressTaskDistances_Value;
	int haplotypeProgressTaskClustering_Value;
	int haplotypeProgressTaskOptimization_Value;
	
	volatile int haplotypeProgressGreedySortProgressFlag;		// see greedyPeriodicCounterUpdateWithBackgroundController: for comments on this
	
	// Misc
	bool observingKeyPaths;
	
	SLiMFunctionGraphToolTipWindow *functionGraphToolTipWindow;		// for previews of muttype DFEs or interaction type IFs
}

+ (NSColor *)blackContrastingColorForIndex:(int)index;
+ (NSColor *)whiteContrastingColorForIndex:(int)index;

- (instancetype)init;
- (instancetype)initWithWindow:(NSWindow *)window __attribute__((unavailable));
- (instancetype)initWithWindowNibName:(NSString *)windowNibName __attribute__((unavailable));
- (instancetype)initWithWindowNibName:(NSString *)windowNibName owner:(id)owner __attribute__((unavailable));
- (instancetype)initWithWindowNibPath:(NSString *)windowNibPath owner:(id)owner __attribute__((unavailable));
- (instancetype)initWithCoder:(NSCoder *)coder __attribute__((unavailable));

- (void)setScriptStringAndInitializeSimulation:(NSString *)string;

- (std::vector<Subpopulation*>)selectedSubpopulations;
- (void)updatePopulationViewHiding;

- (NSColor *)colorForGenomicElementType:(GenomicElementType *)elementType withID:(slim_objectid_t)elementTypeID;

- (void)addScriptBlockToSimulation:(SLiMEidosBlock *)scriptBlock;

- (void)updateRecycleHighlightForChangeCount:(int)changeCount;


//
//	Properties
//

@property (nonatomic) BOOL invalidSimulation;
@property (nonatomic) BOOL continuousPlayOn;
@property (nonatomic) BOOL profilePlayOn;
@property (nonatomic) BOOL nonProfilePlayOn;
@property (nonatomic) BOOL generationPlayOn;
@property (nonatomic) BOOL reachedSimulationEnd;
@property (nonatomic, readonly) NSColor *colorForWindowLabels;

@property (nonatomic, retain) IBOutlet EidosConsoleWindowController *consoleController;


//
//	Actions
//

- (IBAction)buttonChangeSubpopSize:(id)sender;
- (IBAction)buttonRemoveSubpop:(id)sender;
- (IBAction)buttonAddSubpop:(id)sender;
- (IBAction)buttonSplitSubpop:(id)sender;
- (IBAction)buttonChangeMigrationRates:(id)sender;
- (IBAction)buttonChangeSelfingRates:(id)sender;
- (IBAction)buttonChangeCloningRates:(id)sender;
- (IBAction)buttonChangeSexRatio:(id)sender;

- (IBAction)addMutationType:(id)sender;
- (IBAction)addGenomicElementType:(id)sender;
- (IBAction)addGenomicElementToChromosome:(id)sender;
- (IBAction)addRecombinationInterval:(id)sender;
- (IBAction)addSexConfiguration:(id)sender;

- (IBAction)outputFullPopulationState:(id)sender;
- (IBAction)outputPopulationSample:(id)sender;
- (IBAction)outputFixedMutations:(id)sender;

- (IBAction)graphMutationFrequencySpectrum:(id)sender;
- (IBAction)graphMutationFrequencyTrajectories:(id)sender;
- (IBAction)graphMutationLossTimeHistogram:(id)sender;
- (IBAction)graphMutationFixationTimeHistogram:(id)sender;
- (IBAction)graphFitnessOverTime:(id)sender;
- (IBAction)graphPopulationVisualization:(id)sender;
- (IBAction)graphHaplotypes:(id)sender;

- (IBAction)playOneStep:(id)sender;
- (IBAction)play:(id)sender;
- (IBAction)profile:(id)sender;
- (IBAction)recycle:(id)sender;
- (IBAction)playSpeedChanged:(id)sender;
- (IBAction)generationChanged:(id)sender;

- (IBAction)fitnessColorSliderChanged:(id)sender;
- (IBAction)selectionColorSliderChanged:(id)sender;

- (IBAction)checkScript:(id)sender;
- (IBAction)prettyprintScript:(id)sender;
- (IBAction)showScriptHelp:(id)sender;
- (IBAction)toggleConsoleVisibility:(id)sender;
- (IBAction)toggleBrowserVisibility:(id)sender;
- (IBAction)clearOutput:(id)sender;
- (IBAction)dumpPopulationToOutput:(id)sender;
- (IBAction)changeWorkingDirectory:(id)sender;

- (IBAction)showRecombinationIntervalsButtonToggled:(id)sender;
- (IBAction)showGenomicElementsButtonToggled:(id)sender;
- (IBAction)showMutationsButtonToggled:(id)sender;
- (IBAction)showFixedSubstitutionsButtonToggled:(id)sender;

- (IBAction)drawerButtonToggled:(id)sender;

// This action has been disabled; see comments on the implementation.
//- (IBAction)importPopulation:(id)sender;		// wired through firstResponder because these are menu items

- (IBAction)exportScript:(id)sender;			// wired through firstResponder because these are menu items
- (IBAction)exportOutput:(id)sender;			// wired through firstResponder because these are menu items
- (IBAction)exportPopulation:(id)sender;		// wired through firstResponder because these are menu items


//	Eidos SLiMgui method forwards
- (void)eidos_openDocument:(NSString *)path;
- (void)eidos_pauseExecution;


// Haplotype plot options sheet

// Outlets connected to objects in SLiMHaplotypeOptionsSheet.xib
@property (nonatomic, retain) IBOutlet NSWindow *haplotypeOptionsSheet;
@property (nonatomic, assign) IBOutlet NSTextField *haplotypeSampleTextField;
@property (nonatomic, assign) IBOutlet NSButton *haplotypeOKButton;

@property (nonatomic) int haplotypeSample;		// 0 = all genomes, 1 = sample
@property (nonatomic) int haplotypeClustering;	// 0 = nearest neighbor, 1 = greedy, 2 = greedy + 2-opt
@property (nonatomic) int haplotypeSampleSize;	// from haplotypeSampleTextField

- (void)runHaplotypePlotOptionsSheet;

- (IBAction)changedHaplotypeSample:(id)sender;
- (IBAction)changedHaplotypeClustering:(id)sender;

- (IBAction)validateHaplotypeSheetControls:(id)sender;

- (IBAction)haplotypeSheetCancel:(id)sender;
- (IBAction)haplotypeSheetOK:(id)sender;


// Haplotype plot progress sheet
@property (nonatomic, retain) IBOutlet NSWindow *haplotypeProgressSheet;
@property (nonatomic, assign) IBOutlet NSProgressIndicator *haplotypeProgressDistances;
@property (nonatomic, assign) IBOutlet NSProgressIndicator *haplotypeProgressClustering;
@property (nonatomic, assign) IBOutlet NSProgressIndicator *haplotypeProgressOptimization;
@property (nonatomic, assign) IBOutlet NSTextField *haplotypeProgressOptimizationLabel;
@property (nonatomic, assign) IBOutlet NSLayoutConstraint *haplotypeProgressNoOptConstraint;

- (void)runHaplotypePlotProgressSheetWithGenomeCount:(int)genome_count;

- (IBAction)haplotypeProgressSheetCancel:(id)sender;

- (void)setHaplotypeProgress:(int)progress forStage:(int)stage;		// the background task indicates progress on task
- (BOOL)haplotypeProgressIsCancelled;								// the background task asks: has the user cancelled our task?
- (void)haplotypeProgressTaskStarting;								// the background task tells us it is starting
- (void)haplotypeProgressTaskFinished;								// the background task tells us it has finished

@end



















































