//
//  SLiMWindowController.h
//  SLiM
//
//  Created by Ben Haller on 1/21/15.
//  Copyright (c) 2015 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/software/
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
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>

#include "slim_sim.h"
#include "ChromosomeView.h"
#include "PopulationView.h"
#include "GraphView.h"
#include "CocoaExtra.h"


@interface SLiMWindowController : NSWindowController <NSTableViewDelegate, NSTableViewDataSource, NSSplitViewDelegate, NSTextViewDelegate>
{
@public
	NSString *scriptString;		// the script string that we are running on right now; not the same as the script textview!
	SLiMSim *sim;				// the simulation instance for this window
	
	// random number generator variables that are globals in the back end code; we swap these in and out as needed
	gsl_rng *sim_rng;
	int sim_random_bool_bit_counter;
	unsigned long int sim_random_bool_bit_buffer;

	// play-related variables
	BOOL invalidSimulation, continuousPlayOn, generationPlayOn, reachedSimulationEnd;
	int targetGeneration;
	NSDate *continuousPlayStartDate;
	uint64 continuousPlayGenerationsCompleted;
	
	// display-related variables
	double fitnessColorScale, selectionColorScale;
	NSMutableDictionary *genomicElementColorRegistry;
	BOOL zoomedChromosomeShowsRecombinationIntervals;
	BOOL zoomedChromosomeShowsGenomicElements;
	BOOL zoomedChromosomeShowsMutations;
	BOOL zoomedChromosomeShowsFixedSubstitutions;
	BOOL reloadingSubpopTableview, subpopTableviewHasHadPartialSelection;
	
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
	
	IBOutlet SLiMColorStripeView *fitnessColorStripe;
	IBOutlet NSSlider *fitnessColorSlider;
	IBOutlet SLiMColorStripeView *selectionColorStripe;
	IBOutlet NSSlider *selectionColorSlider;
	
	IBOutlet NSButton *playOneStepButton;
	IBOutlet NSButton *playButton;
	IBOutlet NSButton *recycleButton;
	IBOutlet NSSlider *playSpeedSlider;
	IBOutlet NSTextField *generationTextField;
	IBOutlet NSProgressIndicator *generationProgressIndicator;
	
	IBOutlet NSTextView *scriptTextView;
	IBOutlet NSTextView *outputTextView;
	
	IBOutlet NSTableView *subpopTableView;
	IBOutlet NSTableColumn *subpopIDColumn;
	IBOutlet NSTableColumn *subpopSizeColumn;
	IBOutlet NSTableColumn *subpopSelfingRateColumn;
	IBOutlet NSTableColumn *subpopSexRatioColumn;
	
	IBOutlet PopulationView *populationView;
	
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
}

+ (NSColor *)colorForIndex:(int)index;

+ (void)clearColorFromTextView:(NSTextView *)textView;
+ (void)syntaxColorTextView:(NSTextView *)textView;

- (void)setScriptStringAndInitializeSimulation:(NSString *)string;
- (void)setDefaultScriptStringAndInitializeSimulation;

- (std::vector<Subpopulation*>)selectedSubpopulations;

- (NSColor *)colorForGenomicElementTypeID:(int)elementTypeID;

//
//	Properties
//

@property (nonatomic) BOOL invalidSimulation;
@property (nonatomic) BOOL continuousPlayOn;
@property (nonatomic) BOOL generationPlayOn;
@property (nonatomic) BOOL reachedSimulationEnd;
@property (nonatomic, readonly) NSColor *colorForWindowLabels;

//
//	Actions
//

- (IBAction)buttonChangeSubpopSize:(id)sender;
- (IBAction)buttonRemoveSubpop:(id)sender;
- (IBAction)buttonAddSubpop:(id)sender;
- (IBAction)buttonSplitSubpop:(id)sender;
- (IBAction)buttonChangeMigrationRates:(id)sender;
- (IBAction)buttonChangeSelfingRates:(id)sender;
- (IBAction)buttonChangeSexRatio:(id)sender;

- (IBAction)addMutationType:(id)sender;
- (IBAction)addGenomicElementType:(id)sender;
- (IBAction)addGenomicElementToChromosome:(id)sender;
- (IBAction)addRecombinationInterval:(id)sender;
- (IBAction)addPredeterminedMutation:(id)sender;

- (IBAction)outputFullPopulationState:(id)sender;
- (IBAction)outputPopulationSample:(id)sender;
- (IBAction)outputFixedMutations:(id)sender;
- (IBAction)trackMutationType:(id)sender;

- (IBAction)graphMutationFrequencySpectrum:(id)sender;
- (IBAction)graphMutationFrequencyTrajectories:(id)sender;
- (IBAction)graphMutationLossTimeHistogram:(id)sender;
- (IBAction)graphMutationFixationTimeHistogram:(id)sender;
- (IBAction)graphFitnessOverTime:(id)sender;
- (IBAction)graphPopulationVisualization:(id)sender;

- (IBAction)playOneStep:(id)sender;
- (IBAction)play:(id)sender;
- (IBAction)recycle:(id)sender;
- (IBAction)playSpeedChanged:(id)sender;
- (IBAction)generationChanged:(id)sender;

- (IBAction)fitnessColorSliderChanged:(id)sender;
- (IBAction)selectionColorSliderChanged:(id)sender;

- (IBAction)checkScriptTextView:(id)sender;
- (IBAction)clearOutputTextView:(id)sender;
- (IBAction)dumpPopulationToOutput:(id)sender;

- (IBAction)showRecombinationIntervalsButtonToggled:(id)sender;
- (IBAction)showGenomicElementsButtonToggled:(id)sender;
- (IBAction)showMutationsButtonToggled:(id)sender;
- (IBAction)showFixedSubstitutionsButtonToggled:(id)sender;

- (IBAction)exportScript:(id)sender;			// wired through firstResponder because these are menu items
- (IBAction)exportOutput:(id)sender;			// wired through firstResponder because these are menu items
- (IBAction)exportPopulation:(id)sender;		// wired through firstResponder because these are menu items

@end



















































