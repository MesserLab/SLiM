//
//  SLiMWindowController.m
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


#import "SLiMWindowController.h"
#import "AppDelegate.h"
#import "GraphView_MutationFrequencySpectra.h"
#import "GraphView_MutationLossTimeHistogram.h"
#import "GraphView_MutationFixationTimeHistogram.h"
#import "GraphView_FitnessOverTime.h"
#import "GraphView_PopulationVisualization.h"
#import "GraphView_MutationFrequencyTrajectory.h"
#import "ScriptMod_ChangeSubpopSize.h"
#import "ScriptMod_RemoveSubpop.h"
#import "ScriptMod_AddSubpop.h"
#import "ScriptMod_SplitSubpop.h"
#import "ScriptMod_ChangeMigration.h"
#import "ScriptMod_ChangeSelfing.h"
#import "ScriptMod_ChangeCloning.h"
#import "ScriptMod_ChangeSexRatio.h"
#import "ScriptMod_OutputFullPopulation.h"
#import "ScriptMod_OutputSubpopSample.h"
#import "ScriptMod_OutputFixedMutations.h"
#import "ScriptMod_AddMutationType.h"
#import "ScriptMod_AddGenomicElementType.h"
#import "ScriptMod_AddGenomicElement.h"
#import "ScriptMod_AddRecombinationRate.h"
#import "ScriptMod_AddSexConfiguration.h"
#import "EidosHelpController.h"
#import "EidosCocoaExtra.h"
#import "eidos_call_signature.h"
#import "eidos_type_interpreter.h"
#import "slim_test.h"

#include <iostream>
#include <sstream>
#include <iterator>
#include <stdexcept>


@implementation SLiMWindowController

//
//	KVC / KVO / properties
//

@synthesize invalidSimulation, continuousPlayOn, reachedSimulationEnd, generationPlayOn, scriptVisibleConstraint, scriptHiddenConstraint;

+ (NSSet *)keyPathsForValuesAffectingColorForWindowLabels
{
	return [NSSet setWithObjects:@"invalidSimulation", nil];
}

- (NSColor *)getColorForWindowLabels
{
	if (invalidSimulation)
		return [NSColor colorWithCalibratedWhite:0.5 alpha:1.0];
	else
		return [NSColor blackColor];
}

- (void)setContinuousPlayOn:(BOOL)newFlag
{
	if (continuousPlayOn != newFlag)
	{
		continuousPlayOn = newFlag;
		
		[_consoleController setInterfaceEnabled:!(continuousPlayOn || generationPlayOn)];
	}
}

- (void)setGenerationPlayOn:(BOOL)newFlag
{
	if (generationPlayOn != newFlag)
	{
		generationPlayOn = newFlag;
		
		[_consoleController setInterfaceEnabled:!(continuousPlayOn || generationPlayOn)];
	}
}


//
//	Core class methods
//
#pragma mark -
#pragma mark Core class methods

+ (NSString *)defaultScriptString
{
	static NSString *str = @"// set up a simple neutral simulation\n"
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
							"2000 late() { sim.outputFixedMutations(); }\n";
	
	return str;
}

+ (NSColor *)blackContrastingColorForIndex:(int)index
{
	static NSColor **colorArray = NULL;
	
	if (!colorArray)
	{
		colorArray = (NSColor **)malloc(8 * sizeof(NSColor *));
		
		colorArray[0] = [[NSColor colorWithCalibratedHue:0.65 saturation:0.65 brightness:1.0 alpha:1.0] retain];
		colorArray[1] = [[NSColor colorWithCalibratedHue:0.55 saturation:1.0 brightness:1.0 alpha:1.0] retain];
		colorArray[2] = [[NSColor colorWithCalibratedHue:0.40 saturation:1.0 brightness:0.9 alpha:1.0] retain];
		colorArray[3] = [[NSColor colorWithCalibratedHue:0.16 saturation:1.0 brightness:1.0 alpha:1.0] retain];
		colorArray[4] = [[NSColor colorWithCalibratedHue:0.08 saturation:0.65 brightness:1.0 alpha:1.0] retain];
		colorArray[5] = [[NSColor colorWithCalibratedHue:0.00 saturation:0.65 brightness:1.0 alpha:1.0] retain];
		colorArray[6] = [[NSColor colorWithCalibratedHue:0.80 saturation:0.65 brightness:1.0 alpha:1.0] retain];
		colorArray[7] = [[NSColor colorWithCalibratedWhite:0.8 alpha:1.0] retain];
	}
	
	return ((index >= 0) && (index <= 6)) ? colorArray[index] : colorArray[7];
}

+ (NSColor *)whiteContrastingColorForIndex:(int)index
{
	static NSColor **colorArray = NULL;
	
	if (!colorArray)
	{
		colorArray = (NSColor **)malloc(7 * sizeof(NSColor *));
		
		colorArray[0] = [[NSColor colorWithCalibratedHue:0.65 saturation:0.75 brightness:1.0 alpha:1.0] retain];
		colorArray[1] = [[NSColor colorWithCalibratedHue:0.55 saturation:1.0 brightness:1.0 alpha:1.0] retain];
		colorArray[2] = [[NSColor colorWithCalibratedHue:0.40 saturation:1.0 brightness:0.8 alpha:1.0] retain];
		colorArray[3] = [[NSColor colorWithCalibratedHue:0.08 saturation:0.75 brightness:1.0 alpha:1.0] retain];
		colorArray[4] = [[NSColor colorWithCalibratedHue:0.00 saturation:0.85 brightness:1.0 alpha:1.0] retain];
		colorArray[5] = [[NSColor colorWithCalibratedHue:0.80 saturation:0.85 brightness:1.0 alpha:1.0] retain];
		colorArray[6] = [[NSColor colorWithCalibratedWhite:0.5 alpha:1.0] retain];
	}
	
	return ((index >= 0) && (index <= 5)) ? colorArray[index] : colorArray[6];
}

- (id)initWithWindow:(NSWindow *)window
{
	if (self = [super initWithWindow:window])
	{
		// observe preferences that we care about
		[[NSUserDefaults standardUserDefaults] addObserver:self forKeyPath:defaultsSyntaxHighlightScriptKey options:0 context:NULL];
		[[NSUserDefaults standardUserDefaults] addObserver:self forKeyPath:defaultsSyntaxHighlightOutputKey options:0 context:NULL];
		
		// Observe notifications to keep our variable browser toggle button up to date
		[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(browserWillShow:) name:EidosVariableBrowserWillShowNotification object:nil];
		[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(browserWillHide:) name:EidosVariableBrowserWillHideNotification object:nil];
		
		// set default viewing state; this might come from user defaults on a per-script basis eventually...
		zoomedChromosomeShowsRecombinationIntervals = NO;
		zoomedChromosomeShowsGenomicElements = NO;
		zoomedChromosomeShowsMutations = YES;
		zoomedChromosomeShowsFixedSubstitutions = NO;
	}
	
	return self;
}

- (void)browserWillShow:(NSNotification *)note
{
	if ([note object] == [_consoleController browserController])
		[browserButton setState:NSOnState];
}

- (void)browserWillHide:(NSNotification *)note
{
	if ([note object] == [_consoleController browserController])
		[browserButton setState:NSOffState];
}

- (void)showTerminationMessage:(NSString *)terminationMessage
{
	NSAlert *alert = [[NSAlert alloc] init];
	
	[alert setAlertStyle:NSCriticalAlertStyle];
	[alert setMessageText:@"Simulation Runtime Error"];
	[alert setInformativeText:[NSString stringWithFormat:@"%@\nThis error has invalidated the simulation; it cannot be run further.  Once the script is fixed, you can recycle the simulation and try again.", terminationMessage]];
	[alert addButtonWithTitle:@"OK"];
	
	[alert beginSheetModalForWindow:[self window] completionHandler:^(NSModalResponse returnCode) { [alert autorelease]; [terminationMessage autorelease]; }];
	
	// Depending on the circumstances of the error, we might be able to select a range in our input file to show what caused the error
	[scriptTextView selectErrorRange];
	
	// Show the error in the status bar also
	NSString *trimmedError = [terminationMessage stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
	NSDictionary *errorAttrs = [NSDictionary eidosTextAttributesWithColor:[NSColor redColor]];
	NSMutableAttributedString *errorAttrString = [[[NSMutableAttributedString alloc] initWithString:trimmedError attributes:errorAttrs] autorelease];
	
	[errorAttrString addAttribute:NSBaselineOffsetAttributeName value:[NSNumber numberWithFloat:2.0] range:NSMakeRange(0, [errorAttrString length])];
	[scriptStatusTextField setAttributedStringValue:errorAttrString];
}

- (void)checkForSimulationTermination
{
	std::string &&terminationMessage = gEidosTermination.str();
	
	if (!terminationMessage.empty())
	{
		const char *cstr = terminationMessage.c_str();
		NSString *str = [NSString stringWithUTF8String:cstr];
		
		gEidosTermination.clear();
		gEidosTermination.str("");
		
		[self performSelector:@selector(showTerminationMessage:) withObject:[str retain] afterDelay:0.0];	// showTerminationMessage: will release the string
		
		// Now we need to clean up so we are in a displayable state.  Note that we don't even attempt to dispose
		// of the old simulation object; who knows what state it is in, touching it might crash.
		sim = nil;
		
		if (sim_rng)
		{
			gsl_rng_free(sim_rng);
			sim_rng = NULL;
			
			sim_random_bool_bit_counter = 0;
			sim_random_bool_bit_buffer = 0;
			sim_rng_last_seed = 0;
		}
		
		[self setReachedSimulationEnd:YES];
		[self setInvalidSimulation:YES];
	}
}

- (void)startNewSimulationFromScript
{
	if (sim)
	{
		delete sim;
		sim = nullptr;
	}
	
	if (sim_rng)
	{
		gsl_rng_free(sim_rng);
		sim_rng = NULL;
		
		sim_random_bool_bit_counter = 0;
		sim_random_bool_bit_buffer = 0;
	}
	
	if (gEidos_rng)
		NSLog(@"gEidos_rng already set up in startNewSimulationFromScript!");
	
	std::istringstream infile([scriptString UTF8String]);
	
	try
	{
		sim = new SLiMSim(infile);
		sim->InitializeRNGFromSeed(nullptr);
		
		// We take over the RNG instance that SLiMSim just made, since each SLiMgui window has its own RNG
		sim_rng = gEidos_rng;
		sim_random_bool_bit_counter = gEidos_random_bool_bit_counter;
		sim_random_bool_bit_buffer = gEidos_random_bool_bit_buffer;
		sim_rng_last_seed = gEidos_rng_last_seed;
		
		gEidos_rng = NULL;
		
		[self setReachedSimulationEnd:NO];
		[self setInvalidSimulation:NO];
		hasImported = NO;
	}
	catch (std::runtime_error)
	{
		if (sim)
			sim->simulationValid = false;
		[self setReachedSimulationEnd:YES];
		[self checkForSimulationTermination];
	}
}

- (void)setScriptStringAndInitializeSimulation:(NSString *)string
{
	[scriptString release];
	scriptString = [string retain];
	
	[self startNewSimulationFromScript];
}

- (void)setDefaultScriptStringAndInitializeSimulation
{
	[self setScriptStringAndInitializeSimulation:[SLiMWindowController defaultScriptString]];
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
	
	if (object == defaults)
	{
		if ([keyPath isEqualToString:defaultsSyntaxHighlightScriptKey])
		{
			if ([defaults boolForKey:defaultsSyntaxHighlightScriptKey])
				[scriptTextView setSyntaxColoring:kEidosSyntaxColoringEidos];
			else
				[scriptTextView setSyntaxColoring:kEidosSyntaxColoringNone];
		}
		
		if ([keyPath isEqualToString:defaultsSyntaxHighlightOutputKey])
		{
			if ([defaults boolForKey:defaultsSyntaxHighlightOutputKey])
				[outputTextView setSyntaxColoring:kEidosSyntaxColoringOutput];
			else
				[outputTextView setSyntaxColoring:kEidosSyntaxColoringNone];
		}
	}
}

- (GraphView *)graphViewForGraphWindow:(NSWindow *)window
{
	return (GraphView *)[window contentView];
}

- (void)sendAllGraphViewsSelector:(SEL)selector
{
	[[self graphViewForGraphWindow:graphWindowMutationFreqSpectrum] performSelector:selector];
	[[self graphViewForGraphWindow:graphWindowMutationFreqTrajectories] performSelector:selector];
	[[self graphViewForGraphWindow:graphWindowMutationLossTimeHistogram] performSelector:selector];
	[[self graphViewForGraphWindow:graphWindowMutationFixationTimeHistogram] performSelector:selector];
	[[self graphViewForGraphWindow:graphWindowFitnessOverTime] performSelector:selector];
	[[self graphViewForGraphWindow:graphWindowPopulationVisualization] performSelector:selector];
}

- (void)sendAllGraphWindowsSelector:(SEL)selector
{
	[graphWindowMutationFreqSpectrum performSelector:selector];
	[graphWindowMutationFreqTrajectories performSelector:selector];
	[graphWindowMutationLossTimeHistogram performSelector:selector];
	[graphWindowMutationFixationTimeHistogram performSelector:selector];
	[graphWindowFitnessOverTime performSelector:selector];
	[graphWindowPopulationVisualization performSelector:selector];
}

- (void)dealloc
{
	//NSLog(@"[SLiMWindowController dealloc]");
	
	[[NSNotificationCenter defaultCenter] removeObserver:self];
	
	// Disconnect delegate relationships
	[mainSplitView setDelegate:nil];
	mainSplitView = nil;
	
	[scriptTextView setDelegate:nil];
	scriptTextView = nil;
	
	[outputTextView setDelegate:nil];
	outputTextView = nil;
	
	[_consoleController setDelegate:nil];
	
	// Remove observers
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
	
	[defaults removeObserver:self forKeyPath:defaultsSyntaxHighlightScriptKey context:NULL];
	[defaults removeObserver:self forKeyPath:defaultsSyntaxHighlightOutputKey context:NULL];
	
	[[NSNotificationCenter defaultCenter] removeObserver:self];
	
	// Free resources
	[scriptString release];
	scriptString = nil;
	
	delete sim;
	sim = nullptr;
	
	gsl_rng_free(sim_rng);
	sim_rng = nil;
	
	[continuousPlayStartDate release];
	continuousPlayStartDate = nil;
	
	[genomicElementColorRegistry release];
	genomicElementColorRegistry = nil;
	
	[self setScriptHiddenConstraint:nil];
	[self setScriptVisibleConstraint:nil];
	
	// All graph windows attached to this controller need to be closed, since they refer back to us;
	// closing them will come back via windowWillClose: and make them release and nil themselves
	[self sendAllGraphWindowsSelector:@selector(close)];
	
	// We also need to close and release our console window and its associated variable browser window.
	// We don't track the console or var browser in windowWillClose: since we want those windows to
	// continue to exist even when they are hidden, unlike graph windows.
	[[_consoleController browserController] hideWindow];
	[_consoleController hideWindow];
	[self setConsoleController:nil];
	
	[super dealloc];
}

- (void)updateOutputTextView
{
	std::string &&newOutput = gSLiMOut.str();
	
	if (!newOutput.empty())
	{
		const char *cstr = newOutput.c_str();
		NSString *str = [NSString stringWithUTF8String:cstr];
		NSScrollView *enclosingScrollView = [outputTextView enclosingScrollView];
		BOOL scrolledToBottom = (![enclosingScrollView hasVerticalScroller] || [[enclosingScrollView verticalScroller] doubleValue] == 1.0);
		
		[outputTextView replaceCharactersInRange:NSMakeRange([[outputTextView string] length], 0) withString:str];
		[outputTextView setFont:[NSFont fontWithName:@"Menlo" size:11.0]];
		if ([[NSUserDefaults standardUserDefaults] boolForKey:defaultsSyntaxHighlightOutputKey])
			[outputTextView recolorAfterChanges];
		
		// if the user was scrolled to the bottom, we keep them there; otherwise, we let them stay where they were
		if (scrolledToBottom)
			[outputTextView scrollRangeToVisible:NSMakeRange([[outputTextView string] length], 0)];
		
		// clear any error flags set on the stream and empty out its string so it is ready to receive new output
		gSLiMOut.clear();
		gSLiMOut.str("");
	}
}

- (void)updateGenerationCounter
{
	//NSLog(@"updating after generation %d, generationTextField %p", sim->generation_, generationTextField);
	if (!invalidSimulation)
	{
		if (sim->generation_ == 0)
			[generationTextField setStringValue:@"initialize()"];
		else
			[generationTextField setIntegerValue:sim->generation_];
	}
	else
		[generationTextField setStringValue:@""];
}

- (void)updatePopulationViewHiding
{
	std::vector<Subpopulation*> selectedSubpopulations = [self selectedSubpopulations];
	int selectedSubpopCount = (int)(selectedSubpopulations.size());
	
	// Swap between populationView and populationErrorView as needed to show text messages in the pop view area
	if (selectedSubpopCount > 10)
	{
		if (![populationView isHidden])
		{
			[populationView setHidden:YES];
			[populationErrorView setHidden:NO];
		}
	}
	else
	{
		if ([populationView isHidden])
		{
			[populationView setHidden:NO];
			[populationErrorView setHidden:YES];
		}
	}
}

- (void)updateAfterTickFull:(BOOL)fullUpdate
{
	// fullUpdate is used to suppress some expensive updating to every third update
	if (!fullUpdate)
	{
		if (++partialUpdateCount >= 3)
		{
			partialUpdateCount = 0;
			fullUpdate = YES;
		}
	}
	
	// Check whether the simulation has terminated due to an error; if so, show an error message with a delayed perform
	[self checkForSimulationTermination];
	
	// The rest of the code here needs to be careful about the invalid state; we do want to update our controls when invalid, but sim is nil.
	bool invalid = [self invalidSimulation];
	
	if (fullUpdate)
	{
		// FIXME it would be good for this updating to be minimal; reloading the tableview every time, etc., is quite wasteful...
		[self updateOutputTextView];
		
		// Reloading the subpop tableview is tricky, because we need to preserve the selection across the reload, while also noting that the selection is forced
		// to change when a subpop goes extinct.  The current selection is noted in the gui_selected_ ivar of each subpop.  So what we do here is reload the tableview
		// while suppressing our usual update of our selection state, and then we try to re-impose our selection state on the new tableview content.  If a subpop
		// went extinct, we will fail to notice the selection change; but that is OK, since we force an update of populationView and chromosomeZoomed below anyway.
		reloadingSubpopTableview = YES;
		[subpopTableView reloadData];
		
		if (invalid)
		{
			[subpopTableView deselectAll:nil];
		}
		else
		{
			Population &population = sim->population_;
			int subpopCount = (int)population.size();
			auto popIter = population.begin();
			NSMutableIndexSet *indicesToSelect = [NSMutableIndexSet indexSet];
			
			for (int i = 0; i < subpopCount; ++i)
			{
				if (popIter->second->gui_selected_)
					[indicesToSelect addIndex:i];
				
				popIter++;
			}
			
			[subpopTableView selectRowIndexes:indicesToSelect byExtendingSelection:NO];
		}
		
		reloadingSubpopTableview = NO;
		[subpopTableView setNeedsDisplay];
	}
	
	// Now update our other UI, some of which depends upon the state of subpopTableView 
	[populationView setNeedsDisplay:YES];
	[chromosomeZoomed setNeedsDisplayInInterior];
	
	[self updatePopulationViewHiding];
	
	if (fullUpdate)
		[self updateGenerationCounter];
	
	// Update stuff that only needs updating when the script is re-parsed, not after every tick
	if (invalid || sim->mutation_types_changed_)
	{
		[mutTypeTableView reloadData];
		[mutTypeTableView setNeedsDisplay];
		
		if (sim)
			sim->mutation_types_changed_ = false;
	}
	
	if (invalid || sim->genomic_element_types_changed_)
	{
		[genomicElementTypeTableView reloadData];
		[genomicElementTypeTableView setNeedsDisplay];
		
		if (sim)
			sim->genomic_element_types_changed_ = false;
	}
	
	if (invalid || sim->scripts_changed_)
	{
		[scriptBlocksTableView reloadData];
		[scriptBlocksTableView setNeedsDisplay];
		
		if (sim)
			sim->scripts_changed_ = false;
	}
	
	if (invalid || sim->chromosome_changed_)
	{
		[chromosomeOverview restoreLastSelection];
		[chromosomeOverview setNeedsDisplay:YES];
		
		if (sim)
			sim->chromosome_changed_ = false;
	}
	
	// Update graph windows as well; this will usually trigger a setNeedsDisplay:YES but may do other updating work as well
	if (fullUpdate)
		[self sendAllGraphViewsSelector:@selector(updateAfterTick)];
}

- (void)chromosomeSelectionChanged:(NSNotification *)note
{
	[self sendAllGraphViewsSelector:@selector(controllerSelectionChanged)];
}

- (void)replaceHeaderForColumn:(NSTableColumn *)column withImageNamed:(NSString *)imageName scaledToSize:(int)imageSize withSexSymbol:(IndividualSex)sexSymbol
{
	NSImage *headerImage = [NSImage imageNamed:imageName];
	NSImage *scaledHeaderImage = [headerImage copy];
	NSTextAttachment *ta = [[NSTextAttachment alloc] init];
	
	[scaledHeaderImage setSize:NSMakeSize(imageSize, imageSize)];
	[(NSCell *)[ta attachmentCell] setImage:scaledHeaderImage];
	
	NSMutableAttributedString *attrStr = [[NSAttributedString attributedStringWithAttachment:ta] mutableCopy];
	
	if (sexSymbol != IndividualSex::kUnspecified)
	{
		NSImage *sexSymbolImage = [NSImage imageNamed:(sexSymbol == IndividualSex::kMale ? @"male_symbol" : @"female_symbol")];
		NSImage *scaledSexSymbolImage = [sexSymbolImage copy];
		NSTextAttachment *sexSymbolAttachment = [[NSTextAttachment alloc] init];
		
		[scaledSexSymbolImage setSize:NSMakeSize(14, 14)];
		[(NSCell *)[sexSymbolAttachment attachmentCell] setImage:scaledSexSymbolImage];
		
		NSAttributedString *sexSymbolAttrStr = [NSAttributedString attributedStringWithAttachment:sexSymbolAttachment];
		
		[attrStr appendAttributedString:sexSymbolAttrStr];
		
		[scaledSexSymbolImage release];
		[sexSymbolAttachment release];
	}
	
	NSMutableParagraphStyle *paragraphStyle = [NSMutableParagraphStyle new];
	
	[paragraphStyle setAlignment:NSCenterTextAlignment];
	[attrStr addAttribute:NSParagraphStyleAttributeName value:paragraphStyle range:NSMakeRange(0, [attrStr length])];
	[[column headerCell] setAttributedStringValue:attrStr];
	
	[attrStr release];
	[paragraphStyle release];
	[scaledHeaderImage release];
	[ta release];
}

- (void)windowDidLoad
{
    [super windowDidLoad];
	
	// Tell Cocoa that we can go full-screen
	[[self window] setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];
	
	// Fix our splitview's position restore, which NSSplitView sometimes screws up
	[mainSplitView eidosRestoreAutosavedPositions];
	
	// Change column headers in the subpopulation table to images
	[self replaceHeaderForColumn:subpopSelfingRateColumn withImageNamed:@"change_selfing_ratio" scaledToSize:14 withSexSymbol:IndividualSex::kUnspecified];
	[self replaceHeaderForColumn:subpopFemaleCloningRateColumn withImageNamed:@"change_cloning_rate" scaledToSize:14 withSexSymbol:IndividualSex::kFemale];
	[self replaceHeaderForColumn:subpopMaleCloningRateColumn withImageNamed:@"change_cloning_rate" scaledToSize:14 withSexSymbol:IndividualSex::kMale];
	[self replaceHeaderForColumn:subpopSexRatioColumn withImageNamed:@"change_sex_ratio" scaledToSize:15 withSexSymbol:IndividualSex::kUnspecified];
	
	// Set up our color stripes and sliders
	fitnessColorScale = [fitnessColorSlider doubleValue];
	fitnessColorScale *= fitnessColorScale;
	
	[fitnessColorStripe setMetricToPlot:1];
	[fitnessColorStripe setScalingFactor:fitnessColorScale];
	
	selectionColorScale = [selectionColorSlider doubleValue];
	selectionColorScale *= selectionColorScale;
	
	[selectionColorStripe setMetricToPlot:2];
	[selectionColorStripe setScalingFactor:selectionColorScale];
	
	// Set up syntax coloring based on the user's defaults
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
	
	[scriptTextView setSyntaxColoring:([defaults boolForKey:defaultsSyntaxHighlightScriptKey] ? kEidosSyntaxColoringEidos : kEidosSyntaxColoringNone)];
	[outputTextView setSyntaxColoring:([defaults boolForKey:defaultsSyntaxHighlightOutputKey] ? kEidosSyntaxColoringOutput : kEidosSyntaxColoringNone)];
	
	// Set the script textview to show its string, with correct formatting
	[scriptTextView setString:scriptString];
	
	if ([[NSUserDefaults standardUserDefaults] boolForKey:defaultsSyntaxHighlightScriptKey])
		[scriptTextView setSyntaxColoring:kEidosSyntaxColoringEidos];
	
	// Set up our chromosome views to show the proper stuff
	[chromosomeOverview setReferenceChromosomeView:nil];
	[chromosomeOverview setSelectable:YES];
	[chromosomeOverview setShouldDrawGenomicElements:YES];
	[chromosomeOverview setShouldDrawMutations:NO];
	[chromosomeOverview setShouldDrawFixedSubstitutions:NO];
	[chromosomeOverview setShouldDrawRecombinationIntervals:NO];
	
	[chromosomeZoomed setReferenceChromosomeView:chromosomeOverview];
	[chromosomeZoomed setSelectable:NO];
	[chromosomeZoomed setShouldDrawGenomicElements:zoomedChromosomeShowsGenomicElements];
	[chromosomeZoomed setShouldDrawMutations:zoomedChromosomeShowsMutations];
	[chromosomeZoomed setShouldDrawFixedSubstitutions:zoomedChromosomeShowsFixedSubstitutions];
	[chromosomeZoomed setShouldDrawRecombinationIntervals:zoomedChromosomeShowsRecombinationIntervals];
	
	[showRecombinationIntervalsButton setState:(zoomedChromosomeShowsRecombinationIntervals ? NSOnState : NSOffState)];
	[showGenomicElementsButton setState:(zoomedChromosomeShowsGenomicElements ? NSOnState : NSOffState)];
	[showMutationsButton setState:(zoomedChromosomeShowsMutations ? NSOnState : NSOffState)];
	[showFixedSubstitutionsButton setState:(zoomedChromosomeShowsFixedSubstitutions ? NSOnState : NSOffState)];
	
	[[NSNotificationCenter defaultCenter] removeObserver:self name:SLiMChromosomeSelectionChangedNotification object:nil];
	[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(chromosomeSelectionChanged:) name:SLiMChromosomeSelectionChangedNotification object:chromosomeOverview];
	
	// Set up our menu buttons with their menus
	[outputCommandsButton setSlimMenu:outputCommandsMenu];
	[graphCommandsButton setSlimMenu:graphCommandsMenu];
	[genomeCommandsButton setSlimMenu:genomeCommandsMenu];
	
	// Configure our drawer
	[drawer setMinContentSize:NSMakeSize(280, 570)];
	[drawer setMaxContentSize:NSMakeSize(450, 570)];
	[drawer setContentSize:NSMakeSize(280, 570)];
	[drawer setLeadingOffset:0.0];
	[drawer setTrailingOffset:20.0];
	[drawer openOnEdge:NSMaxXEdge];
	[drawer close];
	
	// Update all our UI to reflect the current state of the simulation
	[self updateAfterTickFull:YES];
	
	// Load our console window nib
	[[NSBundle mainBundle] loadNibNamed:@"EidosConsoleWindow" owner:self topLevelObjects:NULL];
}

- (std::vector<Subpopulation*>)selectedSubpopulations
{
	std::vector<Subpopulation*> selectedSubpops;
	
	if (![self invalidSimulation] && sim)
	{
		Population &population = sim->population_;
		int subpopCount = (int)population.size();
		auto popIter = population.begin();
		
		for (int i = 0; i < subpopCount; ++i)
		{
			Subpopulation *subpop = popIter->second;
			
			if (subpop->gui_selected_)
				selectedSubpops.emplace_back(subpop);
			
			popIter++;
		}
	}
	
	return selectedSubpops;
}

- (NSColor *)colorForGenomicElementTypeID:(slim_objectid_t)elementTypeID
{
	if (!genomicElementColorRegistry)
		genomicElementColorRegistry = [[NSMutableDictionary alloc] init];
	
	NSNumber *key = [NSNumber numberWithInteger:elementTypeID];
	NSColor *elementColor = [genomicElementColorRegistry objectForKey:key];
	
	if (!elementColor)
	{
		int elementCount = (int)[genomicElementColorRegistry count];
		
		elementColor = [SLiMWindowController blackContrastingColorForIndex:elementCount];
		[genomicElementColorRegistry setObject:elementColor forKey:key];
	}
	
	return elementColor;
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem
{
	SEL sel = [menuItem action];
	
	if (sel == @selector(playOneStep:))
		return !(invalidSimulation || continuousPlayOn || generationPlayOn || reachedSimulationEnd);
	if (sel == @selector(play:))
	{
		[menuItem setTitle:(continuousPlayOn ? @"Stop" : @"Play")];
		
		return !(invalidSimulation || generationPlayOn || reachedSimulationEnd);
	}
	if (sel == @selector(recycle:))
		return !(continuousPlayOn || generationPlayOn);
	
	if (sel == @selector(buttonChangeSubpopSize:))
		return !(invalidSimulation || continuousPlayOn || generationPlayOn);
	if (sel == @selector(buttonRemoveSubpop:))
		return !(invalidSimulation || continuousPlayOn || generationPlayOn);
	if (sel == @selector(buttonAddSubpop:))
		return !(invalidSimulation || continuousPlayOn || generationPlayOn);
	if (sel == @selector(buttonSplitSubpop:))
		return !(invalidSimulation || continuousPlayOn || generationPlayOn);
	if (sel == @selector(buttonChangeMigrationRates:))
		return !(invalidSimulation || continuousPlayOn || generationPlayOn);
	if (sel == @selector(buttonChangeSelfingRates:))
		return !(invalidSimulation || continuousPlayOn || generationPlayOn);
	if (sel == @selector(buttonChangeCloningRates:))
		return !(invalidSimulation || continuousPlayOn || generationPlayOn);
	if (sel == @selector(buttonChangeSexRatio:))
		return !(invalidSimulation || continuousPlayOn || generationPlayOn);

	if (sel == @selector(addMutationType:))
		return !(invalidSimulation || continuousPlayOn || generationPlayOn);
	if (sel == @selector(addGenomicElementType:))
		return !(invalidSimulation || continuousPlayOn || generationPlayOn);
	if (sel == @selector(addGenomicElementToChromosome:))
		return !(invalidSimulation || continuousPlayOn || generationPlayOn);
	if (sel == @selector(addRecombinationInterval:))
		return !(invalidSimulation || continuousPlayOn || generationPlayOn);
	if (sel == @selector(addSexConfiguration:))
		return !(invalidSimulation || continuousPlayOn || generationPlayOn);

	if (sel == @selector(outputFullPopulationState:))
		return !(invalidSimulation || continuousPlayOn || generationPlayOn);
	if (sel == @selector(outputPopulationSample:))
		return !(invalidSimulation || continuousPlayOn || generationPlayOn);
	if (sel == @selector(outputFixedMutations:))
		return !(invalidSimulation || continuousPlayOn || generationPlayOn);

	if (sel == @selector(graphMutationFrequencySpectrum:))
		return !(invalidSimulation);
	if (sel == @selector(graphMutationFrequencyTrajectories:))
		return !(invalidSimulation);
	if (sel == @selector(graphMutationLossTimeHistogram:))
		return !(invalidSimulation);
	if (sel == @selector(graphMutationFixationTimeHistogram:))
		return !(invalidSimulation);
	if (sel == @selector(graphFitnessOverTime:))
		return !(invalidSimulation);
	
	if (sel == @selector(checkScript:))
		return !(continuousPlayOn || generationPlayOn);
	
	if (sel == @selector(importPopulation:))
		return !(continuousPlayOn || generationPlayOn);
	
	if (sel == @selector(exportScript:))
		return !(continuousPlayOn || generationPlayOn);
	if (sel == @selector(exportOutput:))
		return !(continuousPlayOn || generationPlayOn);
	if (sel == @selector(exportPopulation:))
		return !(invalidSimulation || continuousPlayOn || generationPlayOn);
	
	if (sel == @selector(clearOutput:))
		return !(invalidSimulation);
	if (sel == @selector(dumpPopulationToOutput:))
		return !(invalidSimulation);
	
	if (sel == @selector(toggleScriptVisibility:))
		[menuItem setTitle:(![mainSplitView isHidden] ? @"Hide Script/Output" : @"Show Script/Output")];
	if (sel == @selector(toggleConsoleVisibility:))
		return [_consoleController validateMenuItem:menuItem];
	if (sel == @selector(toggleBrowserVisibility:))
		return [_consoleController validateMenuItem:menuItem];
	
	return YES;
}

- (void)addScriptBlockToSimulation:(SLiMEidosBlock *)scriptBlock
{
	if (sim)
	{
		// First we tokenize and parse the script, which may raise a C++ exception
		try
		{
			scriptBlock->TokenizeAndParse();
		}
		catch (...)
		{
			delete scriptBlock;
			
			NSBeep();
			NSLog(@"raise in addScriptBlockToSimulation: within script_block->TokenizeAndParse()");
			return;
		}
		
		sim->script_blocks_.emplace_back(scriptBlock);		// takes ownership from us
		
		[scriptBlocksTableView reloadData];
		[scriptBlocksTableView setNeedsDisplay];
	}
}


//
//	Actions
//
#pragma mark -
#pragma mark Actions

- (IBAction)buttonChangeSubpopSize:(id)sender
{
	[ScriptMod_ChangeSubpopSize runWithController:self];
}

- (IBAction)buttonRemoveSubpop:(id)sender
{
	[ScriptMod_RemoveSubpop runWithController:self];
}

- (IBAction)buttonAddSubpop:(id)sender
{
	[ScriptMod_AddSubpop runWithController:self];
}

- (IBAction)buttonSplitSubpop:(id)sender
{
	[ScriptMod_SplitSubpop runWithController:self];
}

- (IBAction)buttonChangeMigrationRates:(id)sender
{
	[ScriptMod_ChangeMigration runWithController:self];
}

- (IBAction)buttonChangeSelfingRates:(id)sender
{
	[ScriptMod_ChangeSelfing runWithController:self];
}

- (IBAction)buttonChangeCloningRates:(id)sender
{
	[ScriptMod_ChangeCloning runWithController:self];
}

- (IBAction)buttonChangeSexRatio:(id)sender
{
	[ScriptMod_ChangeSexRatio runWithController:self];
}

- (IBAction)addMutationType:(id)sender
{
	[ScriptMod_AddMutationType runWithController:self];
}

- (IBAction)addGenomicElementType:(id)sender
{
	[ScriptMod_AddGenomicElementType runWithController:self];
}

- (IBAction)addGenomicElementToChromosome:(id)sender
{
	[ScriptMod_AddGenomicElement runWithController:self];
}

- (IBAction)addRecombinationInterval:(id)sender
{
	[ScriptMod_AddRecombinationRate runWithController:self];
}

- (IBAction)addSexConfiguration:(id)sender
{
	[ScriptMod_AddSexConfiguration runWithController:self];
}

- (IBAction)outputFullPopulationState:(id)sender
{
	[ScriptMod_OutputFullPopulation runWithController:self];
}

- (IBAction)outputPopulationSample:(id)sender
{
	[ScriptMod_OutputSubpopSample runWithController:self];
}

- (IBAction)outputFixedMutations:(id)sender
{
	[ScriptMod_OutputFixedMutations runWithController:self];
}

- (void)positionNewGraphWindow:(NSWindow *)window
{
	NSRect windowFrame = [window frame];
	NSRect mainWindowFrame = [[self window] frame];
	BOOL drawerIsOpen = ([drawer state] == NSDrawerOpenState);
	int oldOpenedGraphCount = openedGraphCount++;
	
	// try along the bottom first
	{
		NSRect candidateFrame = windowFrame;
		
		candidateFrame.origin.x = mainWindowFrame.origin.x + oldOpenedGraphCount * (windowFrame.size.width + 5);
		candidateFrame.origin.y = mainWindowFrame.origin.y - (candidateFrame.size.height + 5);
		
		if ([NSScreen visibleCandidateWindowFrame:candidateFrame])
		{
			[window setFrameOrigin:candidateFrame.origin];
			return;
		}
	}
	
	// try on the left side
	{
		NSRect candidateFrame = windowFrame;
		
		candidateFrame.origin.x = mainWindowFrame.origin.x - (candidateFrame.size.width + 5);
		candidateFrame.origin.y = (mainWindowFrame.origin.y + mainWindowFrame.size.height - candidateFrame.size.height) - oldOpenedGraphCount * (windowFrame.size.height + 5);
		
		if ([NSScreen visibleCandidateWindowFrame:candidateFrame])
		{
			[window setFrameOrigin:candidateFrame.origin];
			return;
		}
	}
	
	// unless the drawer is open, let's try on the right side
	if (!drawerIsOpen)
	{
		NSRect candidateFrame = windowFrame;
		
		candidateFrame.origin.x = mainWindowFrame.origin.x + mainWindowFrame.size.width + 5;
		candidateFrame.origin.y = (mainWindowFrame.origin.y + mainWindowFrame.size.height - candidateFrame.size.height) - oldOpenedGraphCount * (windowFrame.size.height + 5);
		
		if ([NSScreen visibleCandidateWindowFrame:candidateFrame])
		{
			[window setFrameOrigin:candidateFrame.origin];
			return;
		}
	}
	
	// try along the top
	{
		NSRect candidateFrame = windowFrame;
		
		candidateFrame.origin.x = mainWindowFrame.origin.x + oldOpenedGraphCount * (windowFrame.size.width + 5);
		candidateFrame.origin.y = mainWindowFrame.origin.y + mainWindowFrame.size.height + 5;
		
		if ([NSScreen visibleCandidateWindowFrame:candidateFrame])
		{
			[window setFrameOrigin:candidateFrame.origin];
			return;
		}
	}
	
	// if none of those worked, we just leave the window where it got placed out of the nib
}

- (NSWindow *)graphWindowWithTitle:(NSString *)windowTitle viewClass:(Class)viewClass
{
	[[NSBundle mainBundle] loadNibNamed:@"GraphWindow" owner:self topLevelObjects:NULL];
	
	// Set the graph window title
	[graphWindow setTitle:windowTitle];
	
	// We substitute in a GraphView subclass here, in place in place of graphView
	NSView *oldContentView = [graphWindow contentView];
	NSRect contentRect = [oldContentView frame];
	GraphView *graphView = [[viewClass alloc] initWithFrame:contentRect withController:self];
	
	[graphWindow setContentView:graphView];
	[graphView release];
	
	// Position the graph window prior to showing it
	[self positionNewGraphWindow:graphWindow];
	
	// We use one nib for all graph types, so we transfer the outlet to a separate ivar
	NSWindow *returnWindow = graphWindow;
	
	graphWindow = nil;
	return returnWindow;
}

- (IBAction)graphMutationFrequencySpectrum:(id)sender
{
	if (!graphWindowMutationFreqSpectrum)
		graphWindowMutationFreqSpectrum = [[self graphWindowWithTitle:@"Mutation Frequency Spectrum" viewClass:[GraphView_MutationFrequencySpectra class]] retain];
	
	[graphWindowMutationFreqSpectrum orderFront:nil];
}

- (IBAction)graphMutationFrequencyTrajectories:(id)sender
{
	if (!graphWindowMutationFreqTrajectories)
		graphWindowMutationFreqTrajectories = [[self graphWindowWithTitle:@"Mutation Frequency Trajectories" viewClass:[GraphView_MutationFrequencyTrajectory class]] retain];
	
	[graphWindowMutationFreqTrajectories orderFront:nil];
}

- (IBAction)graphMutationLossTimeHistogram:(id)sender
{
	if (!graphWindowMutationLossTimeHistogram)
		graphWindowMutationLossTimeHistogram = [[self graphWindowWithTitle:@"Mutation Loss Time" viewClass:[GraphView_MutationLossTimeHistogram class]] retain];
	
	[graphWindowMutationLossTimeHistogram orderFront:nil];
}

- (IBAction)graphMutationFixationTimeHistogram:(id)sender
{
	if (!graphWindowMutationFixationTimeHistogram)
		graphWindowMutationFixationTimeHistogram = [[self graphWindowWithTitle:@"Mutation Fixation Time" viewClass:[GraphView_MutationFixationTimeHistogram class]] retain];
	
	[graphWindowMutationFixationTimeHistogram orderFront:nil];
}

- (IBAction)graphFitnessOverTime:(id)sender
{
	if (!graphWindowFitnessOverTime)
		graphWindowFitnessOverTime = [[self graphWindowWithTitle:@"Fitness ~ Time" viewClass:[GraphView_FitnessOverTime class]] retain];
	
	[graphWindowFitnessOverTime orderFront:nil];
}

- (IBAction)graphPopulationVisualization:(id)sender
{
	if (!graphWindowPopulationVisualization)
		graphWindowPopulationVisualization = [[self graphWindowWithTitle:@"Population Visualization" viewClass:[GraphView_PopulationVisualization class]] retain];
	
	[graphWindowPopulationVisualization orderFront:nil];
}

- (BOOL)runSimOneGeneration
{
	// This method should always be used when calling out to run the simulation, because it swaps the correct random number
	// generator stuff in and out bracketing the call to RunOneGeneration().  This bracketing would need to be done around
	// any other call out to the simulation that caused it to use random numbers, too, such as subsample output.
	BOOL stillRunning = YES;
	
	[self eidosConsoleWindowControllerWillExecuteScript:_consoleController];
	stillRunning = sim->RunOneGeneration();
	[self eidosConsoleWindowControllerDidExecuteScript:_consoleController];
	
	// We also want to let graphViews know when each generation has finished, in case they need to pull data from the sim.  Note this
	// happens after every generation, not just when we are updating the UI, so drawing and setNeedsDisplay: should not happen here.
	[self sendAllGraphViewsSelector:@selector(controllerGenerationFinished)];
	
	return stillRunning;
}

- (IBAction)playOneStep:(id)sender
{
	if (!invalidSimulation)
	{
		[_consoleController invalidateSymbolTable];
		[self setReachedSimulationEnd:![self runSimOneGeneration]];
		[_consoleController validateSymbolTable];
		[self updateAfterTickFull:YES];
	}
}

- (void)_continuousPlay:(id)sender
{
	if (!invalidSimulation)
	{
		NSDate *startDate = [NSDate date];
		double speedSliderValue = [playSpeedSlider doubleValue];
		uint64_t maxGenerationsPerSecond = UINT32_MAX;
		double intervalSinceStarting = -[continuousPlayStartDate timeIntervalSinceNow];
		
		if (speedSliderValue < 1.0)
			maxGenerationsPerSecond = (uint64_t)floor(speedSliderValue * speedSliderValue * 1000 + 1.0);
		//NSLog(@"speedSliderValue == %f, maxGenerationsPerSecond == %llu", speedSliderValue, maxGenerationsPerSecond);
		
		// We keep a local version of reachedSimulationEnd, because calling setReachedSimulationEnd: every generation
		// can actually be a large drag for simulations that run extremely quickly – it can actually exceed the time
		// spent running the simulation itself!  Moral of the story, KVO is wicked slow.
		BOOL reachedEnd = reachedSimulationEnd;
		
		do
		{
			if (continuousPlayGenerationsCompleted / intervalSinceStarting >= maxGenerationsPerSecond)
				break;
			
			@autoreleasepool {
				reachedEnd = ![self runSimOneGeneration];
			}
			
			continuousPlayGenerationsCompleted++;
		}
		while (!reachedEnd && (-[startDate timeIntervalSinceNow] < 0.02));
		
		[self setReachedSimulationEnd:reachedEnd];
		
		if (!reachedSimulationEnd)
		{
			[self updateAfterTickFull:(-[startDate timeIntervalSinceNow] > 0.04)];	// if too much time has elapsed, do a full update
			[self performSelector:@selector(_continuousPlay:) withObject:nil afterDelay:0 inModes:[NSArray arrayWithObject:NSRunLoopCommonModes]];
		}
		else
		{
			// stop playing
			[self updateAfterTickFull:YES];
			[playButton setState:NSOffState];
			[self play:nil];
		}
	}
}

- (void)forceImmediateMenuUpdate
{
	// So, the situation is that the simulation has stopped playing because the end of the simulation was reached.  If that happens while the user
	// is tracking a menu in the menu bar, the menus do not get updating (enabling, titles) until the user ends menu tracking.  This is sort of a
	// bug in Cocoa, I guess; it assumes that state that influences menu bar items does not change while the user is tracking menus, which is false
	// in our case, but is true for most applications, I suppose, maybe.  Anyway, we need to force an immediate update in this situation.
	NSMenu *mainMenu = [[NSApplication sharedApplication] mainMenu];
	NSInteger numberOfSubmenus = [mainMenu numberOfItems];
	
	for (int itemIndex = 0; itemIndex < numberOfSubmenus; ++itemIndex)
	{
		NSMenuItem *menuItem = [mainMenu itemAtIndex:itemIndex];
		NSMenu *submenu = [menuItem submenu];
		
		[submenu update];
	}
	
	// Same for our context menus
	[outputCommandsMenu update];
	[graphCommandsMenu update];
	[genomeCommandsMenu update];
}

- (IBAction)play:(id)sender
{
	if (!continuousPlayOn)
	{
		// keep the button on; this works for the button itself automatically, but when the menu item is chosen this is needed
		[playButton setState:NSOnState];
		
		// log information needed to track our play speed
		[continuousPlayStartDate release];
		continuousPlayStartDate = [[NSDate date] retain];
		continuousPlayGenerationsCompleted = 0;
		[self setContinuousPlayOn:YES];
		
		// invalidate the console symbols, and don't validate them until we are done
		[_consoleController invalidateSymbolTable];
		
		// start playing
		[self performSelector:@selector(_continuousPlay:) withObject:nil afterDelay:0 inModes:[NSArray arrayWithObject:NSRunLoopCommonModes]];
	}
	else
	{
		// keep the button off; this works for the button itself automatically, but when the menu item is chosen this is needed
		[playButton setState:NSOffState];
		
		// stop our recurring perform request
		[NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(_continuousPlay:) object:nil];
		[self setContinuousPlayOn:NO];
		
		[_consoleController validateSymbolTable];
		[self updateAfterTickFull:YES];
		
		// Work around a bug that when the simulation ends during menu tracking, menus do not update until menu tracking finishes
		if ([self reachedSimulationEnd])
			[self forceImmediateMenuUpdate];
	}
}

- (void)_generationPlay:(id)sender
{
	if (!invalidSimulation)
	{
		NSDate *startDate = [NSDate date];
		
		// We keep a local version of reachedSimulationEnd, because calling setReachedSimulationEnd: every generation
		// can actually be a large drag for simulations that run extremely quickly – it can actually exceed the time
		// spent running the simulation itself!  Moral of the story, KVO is wicked slow.
		BOOL reachedEnd = reachedSimulationEnd;
		
		do
		{
			if (sim->generation_ >= targetGeneration)
				break;
			
			@autoreleasepool {
				reachedEnd = ![self runSimOneGeneration];
			}
		}
		while (!reachedEnd && (-[startDate timeIntervalSinceNow] < 0.02));
		
		[self setReachedSimulationEnd:reachedEnd];
		
		if (!reachedSimulationEnd && !(sim->generation_ >= targetGeneration))
		{
			[self updateAfterTickFull:(-[startDate timeIntervalSinceNow] > 0.04)];	// if too much time has elapsed, do a full update
			[self performSelector:@selector(_generationPlay:) withObject:nil afterDelay:0 inModes:[NSArray arrayWithObject:NSRunLoopCommonModes]];
		}
		else
		{
			// stop playing
			[self updateAfterTickFull:YES];
			[self generationChanged:nil];
		}
	}
}

- (IBAction)generationChanged:(id)sender
{
	if (!generationPlayOn)
	{
		NSString *generationString = [generationTextField stringValue];
		
		// Special-case initialize(); we can never advance to it, since it is first, so we just validate it
		if ([generationString isEqualToString:@"initialize()"])
		{
			if (sim->generation_ != 0)
			{
				NSBeep();
				[self updateGenerationCounter];
			}
			
			return;
		}
		
		// Get the integer value from the textfield, since it is not "initialize()".  I would like the method used here to be
		// -longLongValue, but that method does not presently exist on NSControl.   [generationString longLongValue] does not
		// work properly with the formatter; the formatter adds commas and longLongValue doesn't understand them.  Whatever.
		targetGeneration = SLiMClampToGenerationType([generationTextField integerValue]);
		
		// make sure the requested generation is in range
		if (sim->generation_ >= targetGeneration)
		{
			if (sim->generation_ > targetGeneration)
				NSBeep();
			
			return;
		}
		
		// update UI
		[generationProgressIndicator startAnimation:nil];
		[self setGenerationPlayOn:YES];
		
		// invalidate the console symbols, and don't validate them until we are done
		[_consoleController invalidateSymbolTable];
		
		// get the first responder out of the generation textfield
		[[self window] performSelector:@selector(makeFirstResponder:) withObject:nil afterDelay:0.0];
		
		// start playing
		[self performSelector:@selector(_generationPlay:) withObject:nil afterDelay:0 inModes:[NSArray arrayWithObject:NSRunLoopCommonModes]];
	}
	else
	{
		// stop our recurring perform request
		[NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(_generationPlay:) object:nil];
		
		[self setGenerationPlayOn:NO];
		[generationProgressIndicator stopAnimation:nil];
		
		[_consoleController validateSymbolTable];
		
		// Work around a bug that when the simulation ends during menu tracking, menus do not update until menu tracking finishes
		if ([self reachedSimulationEnd])
			[self forceImmediateMenuUpdate];
	}
}

- (IBAction)recycle:(id)sender
{
	[_consoleController invalidateSymbolTable];
	[self clearOutput:nil];
	[self setScriptStringAndInitializeSimulation:[scriptTextView string]];
	[_consoleController validateSymbolTable];
	[self updateAfterTickFull:YES];
	
	// Update the status field so that if the selection is in an initialize...() function the right signature is shown.  This call would
	// be more technically correct in -updateAfterTick, but I don't want the tokenization overhead there, it's too heavyweight.  The
	// only negative consequence of not having it there is that when the user steps out of initialization time into generation 1, an
	// initialize...() function signature may persist in the status bar that should have been changed to "unrecognized call" - no biggie.
	// BCH 31 May 2016: commenting this out since the status bar is no longer dependent on the simulation state.  We want the
	// initialize() functions to show their prototypes in the status bar whether we are in the zero generation or not.
	//[self updateStatusFieldFromSelection];

	[self sendAllGraphViewsSelector:@selector(controllerRecycled)];
}

- (IBAction)playSpeedChanged:(id)sender
{
	// We want our speed to be from the point when the slider changed, not from when play started
	[continuousPlayStartDate release];
	continuousPlayStartDate = [[NSDate date] retain];
	continuousPlayGenerationsCompleted = 0;
}

- (IBAction)fitnessColorSliderChanged:(id)sender
{
	fitnessColorScale = [fitnessColorSlider doubleValue];
	fitnessColorScale *= fitnessColorScale;
	
	[fitnessColorStripe setScalingFactor:fitnessColorScale];
	[fitnessColorStripe setNeedsDisplay:YES];
	
	[populationView setNeedsDisplay:YES];
}

- (IBAction)selectionColorSliderChanged:(id)sender
{
	selectionColorScale = [selectionColorSlider doubleValue];
	selectionColorScale *= selectionColorScale;
	
	[selectionColorStripe setScalingFactor:selectionColorScale];
	[selectionColorStripe setNeedsDisplay:YES];
	
	[chromosomeZoomed setNeedsDisplayInInterior];
}

- (IBAction)checkScript:(id)sender
{
	// Note this does *not* check out scriptString, which represents the state of the script when the SLiMSim object was created
	// Instead, it checks the current script in the script TextView – which is not used for anything until the recycle button is clicked.
	NSString *currentScriptString = [scriptTextView string];
	const char *cstr = [currentScriptString UTF8String];
	NSString *errorDiagnostic = nil;
	
	if (!cstr)
	{
		errorDiagnostic = [@"The script string could not be read, possibly due to an encoding problem." retain];
	}
	else
	{
		SLiMEidosScript script(cstr);
		
		try {
			script.Tokenize();
			script.ParseSLiMFileToAST();
		}
		catch (std::runtime_error err)
		{
			errorDiagnostic = [[NSString stringWithUTF8String:EidosGetTrimmedRaiseMessage().c_str()] retain];
		}
	}
	
	// use our ConsoleWindowController delegate method to play the appropriate sound
	[self eidosConsoleWindowController:_consoleController checkScriptDidSucceed:!errorDiagnostic];
	
	if (errorDiagnostic)
	{
		// On failure, we show an alert describing the error, and highlight the relevant script line
		NSAlert *alert = [[NSAlert alloc] init];
		
		[alert setAlertStyle:NSWarningAlertStyle];
		[alert setMessageText:@"Script error"];
		[alert setInformativeText:errorDiagnostic];
		[alert addButtonWithTitle:@"OK"];
		
		[alert beginSheetModalForWindow:[self window] completionHandler:^(NSModalResponse returnCode) { [alert autorelease]; }];
		
		[scriptTextView selectErrorRange];
		
		// Show the error in the status bar also
		NSString *trimmedError = [errorDiagnostic stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
		NSDictionary *errorAttrs = [NSDictionary eidosTextAttributesWithColor:[NSColor redColor]];
		NSMutableAttributedString *errorAttrString = [[[NSMutableAttributedString alloc] initWithString:trimmedError attributes:errorAttrs] autorelease];
		
		[errorAttrString addAttribute:NSBaselineOffsetAttributeName value:[NSNumber numberWithFloat:2.0] range:NSMakeRange(0, [errorAttrString length])];
		[scriptStatusTextField setAttributedStringValue:errorAttrString];
	}
	else
	{
		NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
		
		// On success, we optionally show a success alert sheet
		if (![defaults boolForKey:EidosDefaultsSuppressScriptCheckSuccessPanelKey])
		{
			NSAlert *alert = [[NSAlert alloc] init];
			
			[alert setAlertStyle:NSInformationalAlertStyle];
			[alert setMessageText:@"No script errors"];
			[alert setInformativeText:@"No errors found."];
			[alert addButtonWithTitle:@"OK"];
			[alert setShowsSuppressionButton:YES];
			
			[alert beginSheetModalForWindow:[self window] completionHandler:^(NSModalResponse returnCode) {
				if ([[alert suppressionButton] state] == NSOnState)
					[defaults setBool:YES forKey:EidosDefaultsSuppressScriptCheckSuccessPanelKey];
				[alert autorelease];
			}];
		}
	}
	
	[errorDiagnostic release];
}

- (IBAction)showScriptHelp:(id)sender
{
	[_consoleController showScriptHelp:sender];
}

- (IBAction)toggleScriptVisibility:(id)sender
{
	if ([mainSplitView isHidden])
	{
		// View is hidden, so we need to show it
		[mainSplitView setHidden:NO];
		[scriptStatusTextField setHidden:NO];
		
		// We want to autosave the window size under a different name when the script/output are hidden
		[[self window] setFrameAutosaveName:@"SLiMgui"];
		
		// Readjust constraints
		[scriptHiddenConstraint setActive:NO];
		[scriptVisibleConstraint setActive:YES];
	}
	else
	{
		// View is visible, so we need to hide it
		[mainSplitView setHidden:YES];
		[scriptStatusTextField setHidden:YES];
		
		// We want to autosave the window size under a different name when the script/output are hidden
		[[self window] setFrameAutosaveName:@"SLiMgui Restricted"];
		
		// Readjust constraints
		if (!scriptHiddenConstraint)
		{
			NSDictionary *viewsDictionary = @{@"chromosomeZoomed" : chromosomeZoomed};
			NSArray<NSLayoutConstraint *> *constraints = [NSLayoutConstraint constraintsWithVisualFormat:@"V:[chromosomeZoomed]-20-|" options:0 metrics:nil views:viewsDictionary];
			
			if ([constraints count] != 1)
				NSLog(@"Unexpected constraints");
			
			[self setScriptHiddenConstraint:[constraints objectAtIndex:0]];
		}
		
		[scriptVisibleConstraint setActive:NO];
		[scriptHiddenConstraint setActive:YES];
	}
}

- (IBAction)toggleConsoleVisibility:(id)sender
{
	[_consoleController toggleConsoleVisibility:sender];
}

- (IBAction)toggleBrowserVisibility:(id)sender
{
	[[_consoleController browserController] toggleBrowserVisibility:self];
}

- (IBAction)clearOutput:(id)sender
{
	[outputTextView setString:@""];
}

- (IBAction)dumpPopulationToOutput:(id)sender
{
	try
	{
		// dump the population
		SLIM_OUTSTREAM << "#OUT: " << sim->generation_ << " A" << std::endl;
		sim->population_.PrintAll(SLIM_OUTSTREAM);
		
		// dump fixed substitutions also; so the dump in SLiMgui is like outputFull() + outputFixedMutations()
		SLIM_OUTSTREAM << std::endl;
		SLIM_OUTSTREAM << "#OUT: " << sim->generation_ << " F " << std::endl;
		SLIM_OUTSTREAM << "Mutations:" << std::endl;
		
		for (unsigned int i = 0; i < sim->population_.substitutions_.size(); i++)
		{
			SLIM_OUTSTREAM << i << " ";
			sim->population_.substitutions_[i]->print(SLIM_OUTSTREAM);
		}
		
		// now send SLIM_OUTSTREAM to the output textview
		[self updateOutputTextView];
	}
	catch (std::runtime_error err)
	{
	}
}

- (IBAction)showRecombinationIntervalsButtonToggled:(id)sender
{
	BOOL newValue = ([showRecombinationIntervalsButton state] == NSOnState);
	
	if (newValue != zoomedChromosomeShowsRecombinationIntervals)
	{
		zoomedChromosomeShowsRecombinationIntervals = newValue;
		[chromosomeZoomed setShouldDrawRecombinationIntervals:zoomedChromosomeShowsRecombinationIntervals];
		[chromosomeZoomed setNeedsDisplayInInterior];
	}
}

- (IBAction)showGenomicElementsButtonToggled:(id)sender
{
	BOOL newValue = ([showGenomicElementsButton state] == NSOnState);
	
	if (newValue != zoomedChromosomeShowsGenomicElements)
	{
		zoomedChromosomeShowsGenomicElements = newValue;
		[chromosomeZoomed setShouldDrawGenomicElements:zoomedChromosomeShowsGenomicElements];
		[chromosomeZoomed setNeedsDisplayInInterior];
	}
}

- (IBAction)showMutationsButtonToggled:(id)sender
{
	BOOL newValue = ([showMutationsButton state] == NSOnState);
	
	if (newValue != zoomedChromosomeShowsMutations)
	{
		zoomedChromosomeShowsMutations = newValue;
		[chromosomeZoomed setShouldDrawMutations:zoomedChromosomeShowsMutations];
		[chromosomeZoomed setNeedsDisplayInInterior];
	}
}

- (IBAction)showFixedSubstitutionsButtonToggled:(id)sender
{
	BOOL newValue = ([showFixedSubstitutionsButton state] == NSOnState);
	
	if (newValue != zoomedChromosomeShowsFixedSubstitutions)
	{
		zoomedChromosomeShowsFixedSubstitutions = newValue;
		[chromosomeZoomed setShouldDrawFixedSubstitutions:zoomedChromosomeShowsFixedSubstitutions];
		[chromosomeZoomed setNeedsDisplayInInterior];
	}
}

- (IBAction)importPopulation:(id)sender
{
	if (invalidSimulation || continuousPlayOn || generationPlayOn || reachedSimulationEnd || hasImported || !sim || !sim->simulationValid || (sim->generation_ != 1))
	{
		// Can only import when in a very specific state
		NSAlert *alert = [[NSAlert alloc] init];
		
		[alert setAlertStyle:NSCriticalAlertStyle];
		[alert setMessageText:@"Import Population Error"];
		[alert setInformativeText:@"The simulation must be initialized and at the beginning of generation 1 before importing. Recycle and then Step over the initialization stage, and then try importing again."];
		[alert addButtonWithTitle:@"OK"];
		
		[alert beginSheetModalForWindow:[self window] completionHandler:^(NSModalResponse returnCode) { [alert autorelease]; }];
		return;
	}
	
	NSOpenPanel *op = [[NSOpenPanel openPanel] retain];
	
	[op setTitle:@"Import Population"];
	[op setCanChooseDirectories:NO];
	[op setCanChooseFiles:YES];
	[op setAllowsMultipleSelection:NO];
	[op setExtensionHidden:NO];
	[op setCanSelectHiddenExtension:NO];
	//[op setAllowedFileTypes:@[@"txt"]];	// we do not enforce any particular file suffix, at present
	
	[op beginSheetModalForWindow:[self window] completionHandler:^(NSInteger result) {
		if (result == NSFileHandlingPanelOKButton)
		{
			NSURL *fileURL = [op URL];
			const char *filePath = [fileURL fileSystemRepresentation];
			
			// Get rid of the open panel first, to avoid a weird transition
			[op orderOut:nil];
			
			int file_format = sim->FormatOfPopulationFile(filePath);
			
			if (file_format == -1)
			{
				// Show error: file contents could not be read
				NSAlert *alert = [[NSAlert alloc] init];
				
				[alert setAlertStyle:NSCriticalAlertStyle];
				[alert setMessageText:@"Import Population Error"];
				[alert setInformativeText:@"The population file could not be read."];
				[alert addButtonWithTitle:@"OK"];
				
				[alert beginSheetModalForWindow:[self window] completionHandler:^(NSModalResponse returnCode) { [alert autorelease]; }];
			}
			else if (file_format == 0)
			{
				// Show error: pop file did not have a well-formed output line
				NSAlert *alert = [[NSAlert alloc] init];
				
				[alert setAlertStyle:NSCriticalAlertStyle];
				[alert setMessageText:@"Import Population Error"];
				[alert setInformativeText:[NSString stringWithFormat:@"The population file is not well-formed. Only population files generated by SLiM can be imported."]];
				[alert addButtonWithTitle:@"OK"];
				
				[alert beginSheetModalForWindow:[self window] completionHandler:^(NSModalResponse returnCode) { [alert autorelease]; }];
			}
			else
			{
				slim_generation_t newGeneration = sim->InitializePopulationFromFile(filePath, nullptr);
				
				// Mark that we have imported
				SLIM_OUTSTREAM << "// Imported population from: " << filePath << std::endl;
				SLIM_OUTSTREAM << "// Setting generation from import file:\n" << newGeneration << "\n" << std::endl;
				
				// Clean up after the import; the order of these steps may be sensitive, so be careful
				[self updateAfterTickFull:YES];
				[_consoleController validateSymbolTable];	// trigger a reload of the variable browser to show the new symbols
			}
			
			[op autorelease];
		}
	}];
}

- (IBAction)exportScript:(id)sender
{
	NSSavePanel *sp = [[NSSavePanel savePanel] retain];
	
	[sp setTitle:@"Export Script"];
	[sp setNameFieldLabel:@"Export As:"];
	[sp setMessage:@"Export the simulation script to a file:"];
	[sp setExtensionHidden:NO];
	[sp setCanSelectHiddenExtension:NO];
	[sp setAllowedFileTypes:@[@"txt"]];
	
	[sp beginSheetModalForWindow:[self window] completionHandler:^(NSInteger result) {
		if (result == NSFileHandlingPanelOKButton)
		{
			NSString *currentScriptString = [scriptTextView string];
			
			[currentScriptString writeToURL:[sp URL] atomically:YES encoding:NSUTF8StringEncoding error:NULL];
			
			[sp autorelease];
		}
	}];
}

- (IBAction)exportOutput:(id)sender
{
	NSSavePanel *sp = [[NSSavePanel savePanel] retain];
	
	[sp setTitle:@"Export Output"];
	[sp setNameFieldLabel:@"Export As:"];
	[sp setMessage:@"Export the simulation output to a file:"];
	[sp setExtensionHidden:NO];
	[sp setCanSelectHiddenExtension:NO];
	[sp setAllowedFileTypes:@[@"txt"]];
	
	[sp beginSheetModalForWindow:[self window] completionHandler:^(NSInteger result) {
		if (result == NSFileHandlingPanelOKButton)
		{
			NSString *currentOutputString = [outputTextView string];
			
			[currentOutputString writeToURL:[sp URL] atomically:YES encoding:NSUTF8StringEncoding error:NULL];
			
			[sp autorelease];
		}
	}];
}

- (IBAction)exportPopulation:(id)sender
{
	NSSavePanel *sp = [[NSSavePanel savePanel] retain];
	
	[sp setTitle:@"Export Population"];
	[sp setNameFieldLabel:@"Export As:"];
	[sp setMessage:@"Export the simulation state to a file:"];
	[sp setExtensionHidden:NO];
	[sp setCanSelectHiddenExtension:NO];
	[sp setAllowedFileTypes:@[@"txt"]];
	
	[sp beginSheetModalForWindow:[self window] completionHandler:^(NSInteger result) {
		if (result == NSFileHandlingPanelOKButton)
		{
			std::ostringstream outstring;
//			const std::vector<std::string> &input_parameters = sim->InputParameters();
//			
//			for (int i = 0; i < input_parameters.size(); i++)
//				outstring << input_parameters[i] << std::endl;
			
			outstring << "#OUT: " << sim->generation_ << " A " << std::endl;
			sim->population_.PrintAll(outstring);
			
			NSString *populationDump = [NSString stringWithUTF8String:outstring.str().c_str()];
			
			[populationDump writeToURL:[sp URL] atomically:YES encoding:NSUTF8StringEncoding error:NULL];
			
			[sp autorelease];
		}
	}];
}


//
//	EidosConsoleWindowControllerDelegate methods
//
#pragma mark -
#pragma mark EidosConsoleWindowControllerDelegate

- (EidosContext *)eidosConsoleWindowControllerEidosContext:(EidosConsoleWindowController *)eidosConsoleController
{
	return sim;
}

- (void)eidosConsoleWindowControllerAppendWelcomeMessageAddendum:(EidosConsoleWindowController *)eidosConsoleController
{
	EidosConsoleTextView *textView = [_consoleController textView];
	NSTextStorage *ts = [textView textStorage];
	NSDictionary *outputAttrs = [NSDictionary eidosOutputAttrs];
	NSString *bundleVersionString = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleShortVersionString"];
	NSString *bundleVersion = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleVersion"];
	NSString *versionString = [NSString stringWithFormat:@"%@ (build %@)", bundleVersionString, bundleVersion];
	NSAttributedString *launchString = [[NSAttributedString alloc] initWithString:[NSString stringWithFormat:@"Connected to SLiMgui simulation.\nSLiM version %@.\n", versionString] attributes:outputAttrs];	// SLIM VERSION
	NSAttributedString *dividerString = [[NSAttributedString alloc] initWithString:@"\n---------------------------------------------------------\n\n" attributes:outputAttrs];
	
	[ts beginEditing];
	[ts replaceCharactersInRange:NSMakeRange([ts length], 0) withAttributedString:launchString];
	[ts replaceCharactersInRange:NSMakeRange([ts length], 0) withAttributedString:dividerString];
	[ts endEditing];
	
	[launchString release];
	[dividerString release];
	
	// We have some one-time work that we do when the first window opens; this is here instead of
	// applicationWillFinishLaunching: because we don't want to mess up gEidos_rng
	static BOOL beenHere = NO;
	
	if (!beenHere)
	{
		beenHere = YES;
		
		// We will be executing scripts, so bracket that with our delegate method call
		[self eidosConsoleWindowControllerWillExecuteScript:_consoleController];
		
		// Add SLiM help items; we need a SLiMSim instance here to get function prototypes
		std::istringstream infile([[SLiMWindowController defaultScriptString] UTF8String]);
		
		SLiMSim signature_sim(infile);
		// note no sim->InitializeRNGFromSeed() here; we don't need the RNG and don't want it to log or have side effects
		
		[[EidosHelpController sharedController] addTopicsFromRTFFile:@"SLiMHelpFunctions" underHeading:@"6. SLiM Functions" functions:signature_sim.ZeroGenerationFunctionSignatures() methods:nullptr properties:nullptr];
		[[EidosHelpController sharedController] addTopicsFromRTFFile:@"SLiMHelpClasses" underHeading:@"7. SLiM Classes" functions:nullptr methods:signature_sim.AllMethodSignatures() properties:signature_sim.AllPropertySignatures()];
		[[EidosHelpController sharedController] addTopicsFromRTFFile:@"SLiMHelpCallbacks" underHeading:@"8. SLiM Events and Callbacks" functions:nullptr methods:nullptr properties:nullptr];
		
		// Run startup tests, iff the option key is down; NOTE THAT THIS CAUSES MASSIVE LEAKING DUE TO RAISES INSIDE EIDOS!
		if ([NSEvent modifierFlags] & NSAlternateKeyMask)
			RunSLiMTests();
		
		// Done executing scripts
		[self eidosConsoleWindowControllerDidExecuteScript:_consoleController];
	}
}

- (EidosSymbolTable *)eidosConsoleWindowController:(EidosConsoleWindowController *)eidosConsoleController symbolsFromBaseSymbols:(EidosSymbolTable *)baseSymbols
{
	if (sim && !invalidSimulation)
		return sim->SymbolsFromBaseSymbols(baseSymbols);
	return baseSymbols;
}

- (EidosFunctionMap *)eidosConsoleWindowController:(EidosConsoleWindowController *)eidosConsoleController functionMapFromBaseMap:(EidosFunctionMap *)baseFunctionMap
{
	if (sim && !invalidSimulation)
		return sim->FunctionMapFromBaseMap(baseFunctionMap);
	return baseFunctionMap;
}

- (const std::vector<const EidosMethodSignature*> *)eidosConsoleWindowControllerAllMethodSignatures:(EidosConsoleWindowController *)eidosConsoleController
{
	return SLiMSim::AllMethodSignatures();
}

- (BOOL)eidosConsoleWindowController:(EidosConsoleWindowController *)eidosConsoleController tokenStringIsSpecialIdentifier:(const std::string &)token_string
{
	if (token_string.compare("sim") == 0)
		return YES;
	
	int len = (int)token_string.length();
	
	if (len >= 2)
	{
		unichar first_ch = token_string[0];
		
		if ((first_ch == 'p') || (first_ch == 'g') || (first_ch == 'm') || (first_ch == 's'))
		{
			for (int ch_index = 1; ch_index < len; ++ch_index)
			{
				unichar idx_ch = token_string[ch_index];
				
				if ((idx_ch < '0') || (idx_ch > '9'))
					return NO;
			}
			
			return YES;
		}
	}
	
	return NO;
}

- (NSString *)eidosConsoleWindowController:(EidosConsoleWindowController *)eidosConsoleController helpTextForClickedText:(NSString *)clickedText
{
	// A few strings which, when option-clicked, should result in more targeted searches.
	// @"initialize" is deliberately omitted here so that the initialize...() methods also come up.
	if ([clickedText isEqualToString:@"early"])		return @"early()";
	if ([clickedText isEqualToString:@"late"])		return @"late()";
	if ([clickedText isEqualToString:@"fitness"])		return @"fitness() callbacks";
	if ([clickedText isEqualToString:@"mateChoice"])	return @"mateChoice() callbacks";
	if ([clickedText isEqualToString:@"modifyChild"])	return @"modifyChild() callbacks";
	
	return nil;
}

- (void)eidosConsoleWindowController:(EidosConsoleWindowController *)eidosConsoleController checkScriptDidSucceed:(BOOL)succeeded
{
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
	
	if (succeeded)
	{
		if ([defaults boolForKey:defaultsPlaySoundParseSuccessKey])
			[[NSSound soundNamed:@"Bottle"] play];
	}
	else
	{
		if ([defaults boolForKey:defaultsPlaySoundParseFailureKey])
			[[NSSound soundNamed:@"Ping"] play];
	}
}

- (void)eidosConsoleWindowControllerWillExecuteScript:(EidosConsoleWindowController *)eidosConsoleController
{
	// Whenever we are about to execute script, we swap in our random number generator; at other times, gEidos_rng is NULL.
	// The goal here is to keep each SLiM window independent in its random number sequence.
	if (gEidos_rng)
		NSLog(@"eidosConsoleWindowControllerWillExecuteScript: gEidos_rng already set up!");
	
	gEidos_rng = sim_rng;
	gEidos_random_bool_bit_counter = sim_random_bool_bit_counter;
	gEidos_random_bool_bit_buffer = sim_random_bool_bit_buffer;
	gEidos_rng_last_seed = sim_rng_last_seed;
}

- (void)eidosConsoleWindowControllerDidExecuteScript:(EidosConsoleWindowController *)eidosConsoleController
{
	// Swap our random number generator back out again; see -eidosConsoleWindowControllerWillExecuteScript
	sim_rng = gEidos_rng;
	sim_random_bool_bit_counter = gEidos_random_bool_bit_counter;
	sim_random_bool_bit_buffer = gEidos_random_bool_bit_buffer;
	sim_rng_last_seed = gEidos_rng_last_seed;
	
	gEidos_rng = NULL;
}

- (void)eidosConsoleWindowControllerConsoleWindowWillClose:(EidosConsoleWindowController *)eidosConsoleController
{
	[consoleButton setState:NSOffState];
}


//
//	EidosTextViewDelegate methods
//
#pragma mark -
#pragma mark EidosTextViewDelegate

// This is necessary because we are both a EidosTextViewDelegate (for the views we directly contain) and an
// EidosConsoleWindowControllerDelegate (for the console window we own), and the delegate protocols are similar
// but not identical.  This protocol just forwards on to the EidosConsoleWindowControllerDelegate methods.

- (EidosSymbolTable *)eidosTextView:(EidosTextView *)eidosTextView symbolsFromBaseSymbols:(EidosSymbolTable *)baseSymbols
{
	// Here we use the symbol table from the console window, rather than calling the console window controller delegate
	// method, which would derive a new symbol table – not what we want here
	return [_consoleController symbols];
}

- (EidosFunctionMap *)eidosTextView:(EidosTextView *)eidosTextView functionMapFromBaseMap:(EidosFunctionMap *)baseFunctionMap
{
	// When called for our script text view, we do not forward to eidosConsoleWindowController:functionMapFromBaseMap:,
	// because it adds functions only when in the zero generation.  Instead, we want to add the functions always.
	if (eidosTextView == scriptTextView)
	{
		if (sim && !invalidSimulation)
			return sim->FunctionMapFromBaseMap(baseFunctionMap, true);	// true forces the add
	}
	
	return [self eidosConsoleWindowController:nullptr functionMapFromBaseMap:baseFunctionMap];
}

- (const std::vector<const EidosMethodSignature*> *)eidosTextViewAllMethodSignatures:(EidosTextView *)eidosTextView;
{
	return [self eidosConsoleWindowControllerAllMethodSignatures:nullptr];
}

- (BOOL)eidosTextView:(EidosTextView *)eidosTextView tokenStringIsSpecialIdentifier:(const std::string &)token_string;
{
	return [self eidosConsoleWindowController:nullptr tokenStringIsSpecialIdentifier:token_string];
}

- (NSString *)eidosTextView:(EidosTextView *)eidosTextView helpTextForClickedText:(NSString *)clickedText
{
	return [self eidosConsoleWindowController:nullptr helpTextForClickedText:clickedText];
}

- (BOOL)eidosTextView:(EidosTextView *)eidosTextView completionContextWithScriptString:(NSString *)completionScriptString selection:(NSRange)selection typeTable:(EidosTypeTable **)typeTable functionMap:(EidosFunctionMap **)functionMap keywords:(NSMutableArray *)keywords
{
	// Code completion in the console window and other ancillary EidosTextViews should use the standard code completion
	// machinery in EidosTextView.  In the script view, however, we want things to behave somewhat differently.  In
	// other contexts, we want the variables and functions available to depend solely upon the current state of the
	// simulation; whatever is actually available is what code completion provides.  In the script view, however, we
	// want to be smarter than that.  Initialization functions should be available when the user is completing
	// inside an initialize() callback, and not available otherwise, regardless of the current simulation state.
	// Similarly, variables associated with particular types of callbacks should always be available within those
	// callbacks; variables defined in script blocks other than the focal block should not be visible in code
	// completion; defined constants should be available everywhere; and it should be assumed that variables with
	// names like pX, mX, gX, and sX have their usual types even if they are not presently defined.  This delegate
	// method accomplishes all of those things, by replacing the standard EidosTextView completion handling.
	if (eidosTextView == scriptTextView)
	{
		std::string script_string([completionScriptString UTF8String]);
		SLiMEidosScript script(script_string);
		
#if EIDOS_DEBUG_COMPLETION
		std::cout << "SLiM script:\n" << script_string << std::endl << std::endl;
#endif
		
		// Parse, an "interpreter block" bounded by an EOF rather than a "script block" that requires braces
		script.Tokenize(true, false);				// make bad tokens as needed, do not keep nonsignificant tokens
		script.ParseSLiMFileToAST(true);			// make bad nodes as needed (i.e. never raise, and produce a correct tree)
		
#if EIDOS_DEBUG_COMPLETION
		std::ostringstream parse_stream;
		script.PrintAST(parse_stream);
		std::cout << "SLiM AST:\n" << parse_stream.str() << std::endl << std::endl;
#endif
		
		// Substitute a type table of class SLiMTypeTable and add any defined symbols to it.  We use SLiMTypeTable so that
		// variables like pX, gX, mX, and sX have a known object type even if they are not presently defined in the simulation.
		*typeTable = new SLiMTypeTable();
		EidosSymbolTable *symbols = [_consoleController symbols];
		
		if (symbols)
			symbols->AddSymbolsToTypeTable(*typeTable);
		
		// Now we scan through the children of the root node, each of which is the root of a SLiM script block.  The last
		// script block is the one we are actually completing inside, but we also want to do a quick scan of any other
		// blocks we find, solely to add entries for any defineConstant() calls we can decode.
		const EidosASTNode *script_root = script.AST();
		
		if (script_root && (script_root->children_.size() > 0))
		{
			EidosASTNode *completion_block = script_root->children_.back();
			
			// If the last script block has a range that ends before the start of the selection, then we are completing after the end
			// of that block, at the outer level of the script.  Detect that case and fall through to the handler for it at the end.
			int32_t completion_block_end = completion_block->token_->token_end_;
			
			if ((int)(selection.location + selection.length) > completion_block_end)
			{
				 // Selection is after end of completion_block
				completion_block = nullptr;
			}
			
			if (completion_block)
			{
				for (EidosASTNode *script_block_node : script_root->children_)
				{
					// script_block_node can have various children, such as an sX identifier, start and end generations, a block type
					// identifier like late(), and then the root node of the compound statement for the script block.  We want to
					// decode the parts that are important to us, without the complication of making SLiMEidosBlock objects.
					EidosASTNode *block_statement_root = nullptr;
					SLiMEidosBlockType block_type = SLiMEidosBlockType::SLiMEidosEventEarly;
					
					for (EidosASTNode *block_child : script_block_node->children_)
					{
						EidosToken *child_token = block_child->token_;
						
						if (child_token->token_type_ == EidosTokenType::kTokenIdentifier)
						{
							const std::string &child_string = child_token->token_string_;
							
							if (child_string.compare(gStr_early) == 0)				block_type = SLiMEidosBlockType::SLiMEidosEventEarly;
							else if (child_string.compare(gStr_late) == 0)			block_type = SLiMEidosBlockType::SLiMEidosEventLate;
							else if (child_string.compare(gStr_initialize) == 0)	block_type = SLiMEidosBlockType::SLiMEidosInitializeCallback;
							else if (child_string.compare(gStr_fitness) == 0)		block_type = SLiMEidosBlockType::SLiMEidosFitnessCallback;
							else if (child_string.compare(gStr_mateChoice) == 0)	block_type = SLiMEidosBlockType::SLiMEidosMateChoiceCallback;
							else if (child_string.compare(gStr_modifyChild) == 0)	block_type = SLiMEidosBlockType::SLiMEidosModifyChildCallback;
							
							// Check for an sX designation on a script block and, if found, add a symbol for it
							else if ((block_child == script_block_node->children_[0]) && (child_string.length() >= 2))
							{
								if (child_string[0] == 's')
								{
									bool all_numeric = true;
									
									for (size_t idx = 1; idx < child_string.length(); ++idx)
										if (!isdigit(child_string[idx]))
											all_numeric = false;
									
									if (all_numeric)
									{
										EidosGlobalStringID constant_id = EidosGlobalStringIDForString(child_string);
										
										(*typeTable)->SetTypeForSymbol(constant_id, EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_SLiMEidosBlock_Class});
									}
								}
							}
						}
						else if (child_token->token_type_ == EidosTokenType::kTokenLBrace)
						{
							block_statement_root = block_child;
						}
					}
					
					// Now we know the type of the node, and the root node of its compound statement; extract what we want
					if (block_statement_root)
					{
						// The symbol sim is defined in initialize() blocks and not in other blocks; we need to add and remove it
						// dynamically so that each block has it defined or not defined as necessary.  Since the completion block
						// is last, the sim symbol will be correctly defined at the end of this process.
						if (block_type == SLiMEidosBlockType::SLiMEidosInitializeCallback)
							(*typeTable)->RemoveTypeForSymbol(gID_sim);
						else
							(*typeTable)->SetTypeForSymbol(gID_sim, EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_SLiMSim_Class});
						
						if (script_block_node == completion_block)
						{
							// This is the block we're actually completing in the context of; it is also the last block in the script
							// snippet that we're working with.  We want to first define any callback-associated variables for the block.
							(*typeTable)->SetTypeForSymbol(gID_self, EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_SLiMEidosBlock_Class});
							
							switch (block_type)
							{
								case SLiMEidosBlockType::SLiMEidosEventEarly:
									break;
								case SLiMEidosBlockType::SLiMEidosEventLate:
									break;
								case SLiMEidosBlockType::SLiMEidosInitializeCallback:
									break;
								case SLiMEidosBlockType::SLiMEidosFitnessCallback:
									(*typeTable)->SetTypeForSymbol(gID_mut,				EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_SLiMEidosBlock_Class});
									(*typeTable)->SetTypeForSymbol(gID_homozygous,		EidosTypeSpecifier{kEidosValueMaskLogical, nullptr});
									(*typeTable)->SetTypeForSymbol(gID_relFitness,		EidosTypeSpecifier{kEidosValueMaskFloat, nullptr});
									(*typeTable)->SetTypeForSymbol(gID_individual,		EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Individual_Class});
									(*typeTable)->SetTypeForSymbol(gID_genome1,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Genome_Class});
									(*typeTable)->SetTypeForSymbol(gID_genome2,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Genome_Class});
									(*typeTable)->SetTypeForSymbol(gID_subpop,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Subpopulation_Class});
									break;
								case SLiMEidosBlockType::SLiMEidosMateChoiceCallback:
									(*typeTable)->SetTypeForSymbol(gID_individual,		EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Individual_Class});
									(*typeTable)->SetTypeForSymbol(gID_genome1,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Genome_Class});
									(*typeTable)->SetTypeForSymbol(gID_genome2,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Genome_Class});
									(*typeTable)->SetTypeForSymbol(gID_subpop,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Subpopulation_Class});
									(*typeTable)->SetTypeForSymbol(gID_sourceSubpop,	EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Subpopulation_Class});
									(*typeTable)->SetTypeForSymbol(gID_weights,			EidosTypeSpecifier{kEidosValueMaskFloat, nullptr});
									break;
								case SLiMEidosBlockType::SLiMEidosModifyChildCallback:
									(*typeTable)->SetTypeForSymbol(gID_child,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Individual_Class});
									(*typeTable)->SetTypeForSymbol(gID_childGenome1,	EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Genome_Class});
									(*typeTable)->SetTypeForSymbol(gID_childGenome2,	EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Genome_Class});
									(*typeTable)->SetTypeForSymbol(gID_childIsFemale,	EidosTypeSpecifier{kEidosValueMaskLogical, nullptr});
									(*typeTable)->SetTypeForSymbol(gID_parent1,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Individual_Class});
									(*typeTable)->SetTypeForSymbol(gID_parent1Genome1,	EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Genome_Class});
									(*typeTable)->SetTypeForSymbol(gID_parent1Genome2,	EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Genome_Class});
									(*typeTable)->SetTypeForSymbol(gID_isCloning,		EidosTypeSpecifier{kEidosValueMaskLogical, nullptr});
									(*typeTable)->SetTypeForSymbol(gID_isSelfing,		EidosTypeSpecifier{kEidosValueMaskLogical, nullptr});
									(*typeTable)->SetTypeForSymbol(gID_parent2,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Individual_Class});
									(*typeTable)->SetTypeForSymbol(gID_parent2Genome1,	EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Genome_Class});
									(*typeTable)->SetTypeForSymbol(gID_parent2Genome2,	EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Genome_Class});
									(*typeTable)->SetTypeForSymbol(gID_subpop,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Subpopulation_Class});
									(*typeTable)->SetTypeForSymbol(gID_sourceSubpop,	EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Subpopulation_Class});
									break;
							}
							
							// Get a function map and add initialization functions to it if we're completing inside an initialize() callback
							if (block_type == SLiMEidosBlockType::SLiMEidosInitializeCallback)
								if (sim)
									sim->AddZeroGenerationFunctionsToMap(*functionMap);
							
							// Make a type interpreter and add symbols to our type table using it
							// We use SLiMTypeInterpreter because we want to pick up definitions of SLiM constants
							SLiMTypeInterpreter typeInterpreter(block_statement_root, **typeTable, **functionMap);
							
							typeInterpreter.TypeEvaluateInterpreterBlock();	// result not used
							
							return YES;
						}
						else
						{
							// This is not the block we're completing in.  We want to add symbols for any constant-defining calls
							// in this block; apart from that, this block cannot affect the completion block, due to scoping.
							
							// Make a type interpreter and add symbols to our type table using it
							// We use SLiMTypeInterpreter because we want to pick up definitions of SLiM constants
							SLiMTypeInterpreter typeInterpreter(block_statement_root, **typeTable, **functionMap, true);
							
							typeInterpreter.TypeEvaluateInterpreterBlock();	// result not used
						}
					}
				}
			}
		}
		
		// We drop through to here if we have a bad or empty script root, or if the final script block (completion_block) didn't
		// have a compound statement (meaning its starting brace has not yet been typed), or if we're completing outside of any
		// existing script block.  In these sorts of cases, we want to return completions for the outer level of a SLiM script.
		// This means that standard Eidos language keywords like "while", "next", etc. are not legal, but SLiM script block
		// keywords like "fitness" and "modifyChild" are.
		[keywords removeAllObjects];
		[keywords addObjectsFromArray:@[@"initialize() {\n\n}\n", @"early() {\n\n}\n", @"late() {\n\n}\n", @"fitness() {\n\n}\n", @"mateChoice() {\n\n}\n", @"modifyChild() {\n\n}\n"]];
		
		// At the outer level, functions are also not legal
		(*functionMap)->clear();
		
		// And no variables exist except SLiM objects like pX, gX, mX, sX
		std::vector<EidosGlobalStringID> symbol_ids = (*typeTable)->AllSymbolIDs();
		
		for (EidosGlobalStringID symbol_id : symbol_ids)
			if ((*typeTable)->GetTypeForSymbol(symbol_id).type_mask != kEidosValueMaskObject)
				(*typeTable)->RemoveTypeForSymbol(symbol_id);
		
		return YES;
	}
	
	return NO;
}


//
//	NSWindow delegate methods
//
#pragma mark -
#pragma mark NSWindow delegate

- (void)windowWillClose:(NSNotification *)notification
{
	// We are the delegate of our own window, and of all of our graph windows, too
	NSWindow *closingWindow = [notification object];
	
	if (closingWindow == [self window])
	{
		[closingWindow setDelegate:nil];
		
		[_consoleController finalize];
		[self setConsoleController:nil];
		
		//[self setWindow:nil];
		[self autorelease];
	}
	else if (closingWindow == graphWindowMutationFreqSpectrum)
	{
		[graphWindowMutationFreqSpectrum autorelease];
		graphWindowMutationFreqSpectrum = nil;
	}
	else if (closingWindow == graphWindowMutationFreqTrajectories)
	{
		[graphWindowMutationFreqTrajectories autorelease];
		graphWindowMutationFreqTrajectories = nil;
	}
	else if (closingWindow == graphWindowMutationLossTimeHistogram)
	{
		[graphWindowMutationLossTimeHistogram autorelease];
		graphWindowMutationLossTimeHistogram = nil;
	}
	else if (closingWindow == graphWindowMutationFixationTimeHistogram)
	{
		[graphWindowMutationFixationTimeHistogram autorelease];
		graphWindowMutationFixationTimeHistogram = nil;
	}
	else if (closingWindow == graphWindowFitnessOverTime)
	{
		[graphWindowFitnessOverTime autorelease];
		graphWindowFitnessOverTime = nil;
	}
	else if (closingWindow == graphWindowPopulationVisualization)
	{
		[graphWindowPopulationVisualization autorelease];
		graphWindowPopulationVisualization = nil;
	}
	
	// If all of our subsidiary graph windows have been closed, we are effectively back at square one regarding window placement
	if (!graphWindowMutationFreqSpectrum && !graphWindowMutationFreqTrajectories && !graphWindowMutationLossTimeHistogram && !graphWindowMutationFixationTimeHistogram && !graphWindowFitnessOverTime && !graphWindowPopulationVisualization)
		openedGraphCount = 0;
}

- (void)windowDidResize:(NSNotification *)notification
{
	NSWindow *resizingWindow = [notification object];
	NSView *contentView = [resizingWindow contentView];

	if ([contentView isKindOfClass:[GraphView class]])
		[(GraphView *)contentView graphWindowResized];
}

//
//	NSTextView delegate methods
//
#pragma mark -
#pragma mark NSTextView delegate

- (void)textDidChange:(NSNotification *)notification
{
	NSTextView *textView = (NSTextView *)[notification object];
	
	if (textView == scriptTextView)
		[self setDocumentEdited:YES];	// this still doesn't set up the "Edited" marker in the window title bar, because we're not using NSDocument
}

- (void)updateStatusFieldFromSelection
{
	NSAttributedString *attributedSignature = [scriptTextView attributedSignatureForScriptString:[scriptTextView string] selection:[scriptTextView selectedRange]];
	NSString *signatureString = [attributedSignature string];
	
	// Here we do a little quick-and-dirty patching in order to show signatures when inside callback definitions
	if ([signatureString hasSuffix:@"unrecognized call"])
	{
		if ([signatureString hasPrefix:@"initialize()"])
		{
			static EidosCallSignature *callbackSig = nullptr;
			
			if (!callbackSig)
				callbackSig = (new EidosFunctionSignature("initialize", EidosFunctionIdentifier::kNoFunction, kEidosValueMaskNULL));
			
			attributedSignature = [NSAttributedString eidosAttributedStringForCallSignature:callbackSig];
		}
		else if ([signatureString hasPrefix:@"early()"])
		{
			static EidosCallSignature *callbackSig = nullptr;
			
			if (!callbackSig)
				callbackSig = (new EidosFunctionSignature("early", EidosFunctionIdentifier::kNoFunction, kEidosValueMaskNULL));
			
			attributedSignature = [NSAttributedString eidosAttributedStringForCallSignature:callbackSig];
		}
		else if ([signatureString hasPrefix:@"late()"])
		{
			static EidosCallSignature *callbackSig = nullptr;
			
			if (!callbackSig)
				callbackSig = (new EidosFunctionSignature("late", EidosFunctionIdentifier::kNoFunction, kEidosValueMaskNULL));
			
			attributedSignature = [NSAttributedString eidosAttributedStringForCallSignature:callbackSig];
		}
		else if ([signatureString hasPrefix:@"fitness()"])
		{
			static EidosCallSignature *callbackSig = nullptr;
			
			if (!callbackSig)
				callbackSig = (new EidosFunctionSignature("fitness", EidosFunctionIdentifier::kNoFunction, kEidosValueMaskNULL))->AddObject_S("mutationType", gSLiM_MutationType_Class)->AddObject_OS("subpop", gSLiM_Subpopulation_Class);
			
			attributedSignature = [NSAttributedString eidosAttributedStringForCallSignature:callbackSig];
		}
		else if ([signatureString hasPrefix:@"mateChoice()"])
		{
			static EidosCallSignature *callbackSig = nullptr;
			
			if (!callbackSig)
				callbackSig = (new EidosFunctionSignature("mateChoice", EidosFunctionIdentifier::kNoFunction, kEidosValueMaskNULL))->AddObject_OS("subpop", gSLiM_Subpopulation_Class);
			
			attributedSignature = [NSAttributedString eidosAttributedStringForCallSignature:callbackSig];
		}
		else if ([signatureString hasPrefix:@"modifyChild()"])
		{
			static EidosCallSignature *callbackSig = nullptr;
			
			if (!callbackSig)
				callbackSig = (new EidosFunctionSignature("modifyChild", EidosFunctionIdentifier::kNoFunction, kEidosValueMaskNULL))->AddObject_OS("subpop", gSLiM_Subpopulation_Class);
			
			attributedSignature = [NSAttributedString eidosAttributedStringForCallSignature:callbackSig];
		}
	}
	
	[scriptStatusTextField setAttributedStringValue:attributedSignature];
}

- (void)textViewDidChangeSelection:(NSNotification *)notification
{
	NSTextView *textView = (NSTextView *)[notification object];
	
	if (textView == scriptTextView)
	{
		[self updateStatusFieldFromSelection];
	}
}


//
//	NSTableView delegate methods
//
#pragma mark -
#pragma mark NSTableView delegate

- (BOOL)tableView:(NSTableView *)tableView shouldTrackCell:(NSCell *)cell forTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
	return NO;
}

- (void)tableViewSelectionDidChange:(NSNotification *)aNotification
{
	if (!invalidSimulation)
	{
		NSTableView *aTableView = [aNotification object];
		
		if (aTableView == subpopTableView && !reloadingSubpopTableview)		// see comment in -updateAfterTick after reloadingSubpopTableview
		{
			Population &population = sim->population_;
			int subpopCount = (int)population.size();
			auto popIter = population.begin();
			bool all_selected = true;
			
			for (int i = 0; i < subpopCount; ++i)
			{
				popIter->second->gui_selected_ = [subpopTableView isRowSelected:i];
				
				if (!popIter->second->gui_selected_)
					all_selected = false;
				
				popIter++;
			}
			
			population.gui_all_selected_ = all_selected;
			
			// If the selection has changed, that means that the mutation tallies need to be recomputed
			population.TallyMutationReferences(nullptr, true);
			
			// It's a bit hard to tell for sure whether we need to update or not, since a selected subpop might have been removed from the tableview;
			// selection changes should not happen often, so we can just always update, I think.
			[populationView setNeedsDisplay:YES];
			[self updatePopulationViewHiding];
			
			[chromosomeZoomed setNeedsDisplayInInterior];
		}
	}
}


//
//	NSTableView datasource methods
//
#pragma mark -
#pragma mark NSTableView datasource

- (NSInteger)numberOfRowsInTableView:(NSTableView *)aTableView
{
	if (!invalidSimulation)
	{
		if (aTableView == subpopTableView)
		{
			return sim->population_.size();
		}
		else if (aTableView == mutTypeTableView)
		{
			return sim->mutation_types_.size();
		}
		else if (aTableView == genomicElementTypeTableView)
		{
			return sim->genomic_element_types_.size();
		}
		else if (aTableView == scriptBlocksTableView)
		{
			return sim->script_blocks_.size();
		}
	}
	
	return 0;
}

- (id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(NSInteger)rowIndex
{
	if (!invalidSimulation)
	{
		if (aTableView == subpopTableView)
		{
			Population &population = sim->population_;
			int subpopCount = (int)population.size();
			
			if (rowIndex < subpopCount)
			{
				auto popIter = population.begin();
				
				std::advance(popIter, rowIndex);
				slim_objectid_t subpop_id = popIter->first;
				Subpopulation *subpop = popIter->second;
				
				if (aTableColumn == subpopIDColumn)
				{
					return [NSString stringWithFormat:@"p%lld", (int64_t)subpop_id];
				}
				else if (aTableColumn == subpopSizeColumn)
				{
					return [NSString stringWithFormat:@"%lld", (int64_t)subpop->child_subpop_size_];
				}
				else if (aTableColumn == subpopSelfingRateColumn)
				{
					if (subpop->sex_enabled_)
						return @"—";
					else
						return [NSString stringWithFormat:@"%.2f", subpop->selfing_fraction_];
				}
				else if (aTableColumn == subpopFemaleCloningRateColumn)
				{
					return [NSString stringWithFormat:@"%.2f", subpop->female_clone_fraction_];
				}
				else if (aTableColumn == subpopMaleCloningRateColumn)
				{
					return [NSString stringWithFormat:@"%.2f", subpop->male_clone_fraction_];
				}
				else if (aTableColumn == subpopSexRatioColumn)
				{
					if (subpop->sex_enabled_)
						return [NSString stringWithFormat:@"%.2f", subpop->child_sex_ratio_];
					else
						return @"—";
				}
			}
		}
		else if (aTableView == mutTypeTableView)
		{
			std::map<slim_objectid_t,MutationType*> &mutationTypes = sim->mutation_types_;
			int mutationTypeCount = (int)mutationTypes.size();
			
			if (rowIndex < mutationTypeCount)
			{
				auto mutTypeIter = mutationTypes.begin();
				
				std::advance(mutTypeIter, rowIndex);
				slim_objectid_t mutTypeID = mutTypeIter->first;
				MutationType *mutationType = mutTypeIter->second;
				
				if (aTableColumn == mutTypeIDColumn)
				{
					return [NSString stringWithFormat:@"m%lld", (int64_t)mutTypeID];
				}
				else if (aTableColumn == mutTypeDominanceColumn)
				{
					return [NSString stringWithFormat:@"%.3f", mutationType->dominance_coeff_];
				}
				else if (aTableColumn == mutTypeDFETypeColumn)
				{
					switch (mutationType->dfe_type_)
					{
						case DFEType::kFixed:			return @"fixed";
						case DFEType::kGamma:			return @"gamma";
						case DFEType::kExponential:		return @"exp";
						case DFEType::kNormal:			return @"normal";
						case DFEType::kWeibull:			return @"Weibull";
						case DFEType::kScript:			return @"script";
					}
				}
				else if (aTableColumn == mutTypeDFEParamsColumn)
				{
					NSMutableString *paramString = [[NSMutableString alloc] init];
					
					if (mutationType->dfe_type_ == DFEType::kScript)
					{
						// DFE type 's' has string parameters
						for (unsigned int paramIndex = 0; paramIndex < mutationType->dfe_strings_.size(); ++paramIndex)
						{
							const char *dfe_string = mutationType->dfe_strings_[paramIndex].c_str();
							NSString *ns_dfe_string = [NSString stringWithUTF8String:dfe_string];
							
							[paramString appendFormat:@"\"%@\"", ns_dfe_string];
							
							if (paramIndex < mutationType->dfe_strings_.size() - 1)
								[paramString appendString:@", "];
						}
					}
					else
					{
						// All other DFEs have double parameters
						for (unsigned int paramIndex = 0; paramIndex < mutationType->dfe_parameters_.size(); ++paramIndex)
						{
							NSString *paramSymbol = @"";
							
							switch (mutationType->dfe_type_)
							{
								case DFEType::kFixed:			paramSymbol = @"s"; break;
								case DFEType::kGamma:			paramSymbol = (paramIndex == 0 ? @"s̄" : @"α"); break;
								case DFEType::kExponential:		paramSymbol = @"s̄"; break;
								case DFEType::kNormal:			paramSymbol = (paramIndex == 0 ? @"s̄" : @"σ"); break;
								case DFEType::kWeibull:			paramSymbol = (paramIndex == 0 ? @"λ" : @"k"); break;
								case DFEType::kScript:			break;
							}
							
							[paramString appendFormat:@"%@=%.3f", paramSymbol, mutationType->dfe_parameters_[paramIndex]];
							
							if (paramIndex < mutationType->dfe_parameters_.size() - 1)
								[paramString appendString:@", "];
						}
					}
					
					return [paramString autorelease];
				}
			}
		}
		else if (aTableView == genomicElementTypeTableView)
		{
			std::map<slim_objectid_t,GenomicElementType*> &genomicElementTypes = sim->genomic_element_types_;
			int genomicElementTypeCount = (int)genomicElementTypes.size();
			
			if (rowIndex < genomicElementTypeCount)
			{
				auto genomicElementTypeIter = genomicElementTypes.begin();
				
				std::advance(genomicElementTypeIter, rowIndex);
				slim_objectid_t genomicElementTypeID = genomicElementTypeIter->first;
				GenomicElementType *genomicElementType = genomicElementTypeIter->second;
				
				if (aTableColumn == genomicElementTypeIDColumn)
				{
					return [NSString stringWithFormat:@"g%lld", (int64_t)genomicElementTypeID];
				}
				else if (aTableColumn == genomicElementTypeColorColumn)
				{
					return [self colorForGenomicElementTypeID:genomicElementTypeID];
				}
				else if (aTableColumn == genomicElementTypeMutationTypesColumn)
				{
					NSMutableString *paramString = [[NSMutableString alloc] init];
					
					for (unsigned int mutTypeIndex = 0; mutTypeIndex < genomicElementType->mutation_fractions_.size(); ++mutTypeIndex)
					{
						MutationType *mutType = genomicElementType->mutation_type_ptrs_[mutTypeIndex];
						double mutTypeFraction = genomicElementType->mutation_fractions_[mutTypeIndex];
						
						[paramString appendFormat:@"m%lld=%.3f", (int64_t)mutType->mutation_type_id_, mutTypeFraction];
						
						if (mutTypeIndex < genomicElementType->mutation_fractions_.size() - 1)
							[paramString appendString:@", "];
					}
					
					return [paramString autorelease];
				}
			}
		}
		else if (aTableView == scriptBlocksTableView)
		{
			std::vector<SLiMEidosBlock*> &scriptBlocks = sim->script_blocks_;
			int scriptBlockCount = (int)scriptBlocks.size();
			
			if (rowIndex < scriptBlockCount)
			{
				SLiMEidosBlock *scriptBlock = scriptBlocks[rowIndex];
				
				if (aTableColumn == scriptBlocksIDColumn)
				{
					slim_objectid_t block_id = scriptBlock->block_id_;
					
					if (block_id == -1)
						return @"—";
					else
						return [NSString stringWithFormat:@"s%lld", (int64_t)block_id];
				}
				else if (aTableColumn == scriptBlocksStartColumn)
				{
					if (scriptBlock->start_generation_ == -1)
						return @"MIN";
					else
						return [NSString stringWithFormat:@"%lld", (int64_t)scriptBlock->start_generation_];
				}
				else if (aTableColumn == scriptBlocksEndColumn)
				{
					if (scriptBlock->end_generation_ == SLIM_MAX_GENERATION)
						return @"MAX";
					else
						return [NSString stringWithFormat:@"%lld", (int64_t)scriptBlock->end_generation_];
				}
				else if (aTableColumn == scriptBlocksTypeColumn)
				{
					switch (scriptBlock->type_)
					{
						case SLiMEidosBlockType::SLiMEidosEventEarly:			return @"early()";
						case SLiMEidosBlockType::SLiMEidosEventLate:			return @"late()";
						case SLiMEidosBlockType::SLiMEidosInitializeCallback:	return @"initialize()";
						case SLiMEidosBlockType::SLiMEidosFitnessCallback:		return @"fitness()";
						case SLiMEidosBlockType::SLiMEidosMateChoiceCallback:	return @"mateChoice()";
						case SLiMEidosBlockType::SLiMEidosModifyChildCallback:	return @"modifyChild()";
					}
				}
			}
		}
	}
	
	return @"";
}

- (NSString *)tableView:(NSTableView *)aTableView toolTipForCell:(NSCell *)aCell rect:(NSRectPointer)rect tableColumn:(NSTableColumn *)aTableColumn row:(NSInteger)rowIndex mouseLocation:(NSPoint)mouseLocation
{
	if (!invalidSimulation)
	{
		if (aTableView == scriptBlocksTableView)
		{
			std::vector<SLiMEidosBlock*> &scriptBlocks = sim->script_blocks_;
			int scriptBlockCount = (int)scriptBlocks.size();
			
			if (rowIndex < scriptBlockCount)
			{
				SLiMEidosBlock *scriptBlock = scriptBlocks[rowIndex];
				const char *script_string = scriptBlock->compound_statement_node_->token_->token_string_.c_str();
				NSString *ns_script_string = [NSString stringWithUTF8String:script_string];
				
				// change whitespace to non-breaking spaces; we want to force AppKit not to wrap code
				// note this doesn't really prevent AppKit from wrapping our tooltip, and I'd also like to use Monaco 9; I think I need a custom popup to do that...
				ns_script_string = [ns_script_string stringByReplacingOccurrencesOfString:@" " withString:@" "];		// second string is an &nbsp;
				ns_script_string = [ns_script_string stringByReplacingOccurrencesOfString:@"\t" withString:@"   "];		// second string is three &nbsp;s
				
				return ns_script_string;
			}
		}
		else if (aTableView == mutTypeTableView)
		{
			std::map<slim_objectid_t,MutationType*> &mutationTypes = sim->mutation_types_;
			int mutationTypeCount = (int)mutationTypes.size();
			
			if (rowIndex < mutationTypeCount)
			{
				auto mutTypeIter = mutationTypes.begin();
				
				std::advance(mutTypeIter, rowIndex);
				MutationType *mutationType = mutTypeIter->second;
				
				if (mutationType->dfe_type_ == DFEType::kScript)
				{
					const char *dfe_string = mutationType->dfe_strings_[0].c_str();
					NSString *ns_dfe_string = [NSString stringWithUTF8String:dfe_string];
					
					// change whitespace to non-breaking spaces; we want to force AppKit not to wrap code
					// note this doesn't really prevent AppKit from wrapping our tooltip, and I'd also like to use Monaco 9; I think I need a custom popup to do that...
					ns_dfe_string = [ns_dfe_string stringByReplacingOccurrencesOfString:@" " withString:@" "];		// second string is an &nbsp;
					ns_dfe_string = [ns_dfe_string stringByReplacingOccurrencesOfString:@"\t" withString:@"   "];		// second string is three &nbsp;s
					
					return ns_dfe_string;
				}
			}
		}
	}
	
	return nil;
}


//
//	NSSplitView delegate methods
//
#pragma mark -
#pragma mark NSSplitView delegate

- (BOOL)splitView:(NSSplitView *)splitView canCollapseSubview:(NSView *)subview
{
	return YES;
}

- (CGFloat)splitView:(NSSplitView *)splitView constrainMaxCoordinate:(CGFloat)proposedMax ofSubviewAt:(NSInteger)dividerIndex
{
	return proposedMax - 240;
}

- (CGFloat)splitView:(NSSplitView *)splitView constrainMinCoordinate:(CGFloat)proposedMin ofSubviewAt:(NSInteger)dividerIndex
{
	return proposedMin + 240;
}


//
//	NSDrawer delegate methods
//
#pragma mark -
#pragma mark NSDrawer delegate

- (void)drawerDidOpen:(NSNotification *)notification
{
	[buttonForDrawer setState:NSOnState];
}

- (void)drawerDidClose:(NSNotification *)notification
{
	[buttonForDrawer setState:NSOffState];
}

@end

























































