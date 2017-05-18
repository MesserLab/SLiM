//
//  SLiMWindowController.h
//  SLiM
//
//  Created by Ben Haller on 1/21/15.
//  Copyright (c) 2015-2016 Philipp Messer.  All rights reserved.
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
#import "ChromosomeView.h"
#import "PopulationView.h"
#import "GraphView.h"
#import "CocoaExtra.h"
#import "EidosTextView.h"
#import "EidosTextViewDelegate.h"
#import "EidosConsoleTextView.h"
#import "EidosConsoleWindowController.h"
#import "EidosConsoleWindowControllerDelegate.h"


@interface SLiMWindowController : NSWindowController <NSTableViewDelegate, NSTableViewDataSource, NSSplitViewDelegate, NSTextViewDelegate, EidosConsoleWindowControllerDelegate, EidosTextViewDelegate>
{
@public
	NSString *scriptString;		// the script string that we are running on right now; not the same as the script textview!
	SLiMSim *sim;				// the simulation instance for this window
	
	// random number generator variables that are globals in Eidos; we swap these in and out as needed
	gsl_rng *sim_rng;
	int sim_random_bool_bit_counter;
	uint32_t sim_random_bool_bit_buffer;
	unsigned long int sim_rng_last_seed;			// unsigned long int is the type used by gsl_rng_set()

	// play-related variables
	BOOL invalidSimulation, continuousPlayOn, profileOn, generationPlayOn, reachedSimulationEnd, hasImported;
	slim_generation_t targetGeneration;
	NSDate *continuousPlayStartDate;
	uint64_t continuousPlayGenerationsCompleted;
	int partialUpdateCount;
	SLiMToolTipWindow *playSpeedToolTipWindow;
	
	// profiling-related variables
	NSDate *profileEndDate;
	clock_t profileElapsedClock;
	slim_generation_t profileStartGeneration;
	
	// display-related variables
	double fitnessColorScale, selectionColorScale;
	NSMutableDictionary *genomicElementColorRegistry;
	BOOL zoomedChromosomeShowsRecombinationIntervals;
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
	
	IBOutlet NSTableView *scriptBlocksTableView;
	IBOutlet NSTableColumn *scriptBlocksIDColumn;
	IBOutlet NSTableColumn *scriptBlocksStartColumn;
	IBOutlet NSTableColumn *scriptBlocksEndColumn;
	IBOutlet NSTableColumn *scriptBlocksTypeColumn;
	
	IBOutlet NSSplitView *overallSplitView;
	
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
	
	// Profile Report window ivars
	IBOutlet NSWindow *profileWindow;		// outlet for ProfileReport.xib; note this does not stay wired up, it is just used transiently
	IBOutlet NSTextView *profileTextView;	// ditto
	
	// Misc
	bool observingKeyPaths;
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

- (NSColor *)colorForGenomicElementType:(GenomicElementType *)elementType withID:(slim_objectid_t)elementTypeID;

- (void)addScriptBlockToSimulation:(SLiMEidosBlock *)scriptBlock;

- (void)updateRecycleHighlightForChangeCount:(int)changeCount;


//
//	Properties
//

@property (nonatomic) BOOL invalidSimulation;
@property (nonatomic) BOOL continuousPlayOn;
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

- (IBAction)playOneStep:(id)sender;
- (IBAction)play:(id)sender;
- (IBAction)profile:(id)sender;
- (IBAction)recycle:(id)sender;
- (IBAction)playSpeedChanged:(id)sender;
- (IBAction)generationChanged:(id)sender;

- (IBAction)fitnessColorSliderChanged:(id)sender;
- (IBAction)selectionColorSliderChanged:(id)sender;

- (IBAction)checkScript:(id)sender;
- (IBAction)showScriptHelp:(id)sender;
- (IBAction)toggleConsoleVisibility:(id)sender;
- (IBAction)toggleBrowserVisibility:(id)sender;
- (IBAction)clearOutput:(id)sender;
- (IBAction)dumpPopulationToOutput:(id)sender;

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

@end



















































