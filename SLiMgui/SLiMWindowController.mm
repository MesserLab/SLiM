//
//  SLiMWindowController.m
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

#include <iostream>
#include <sstream>
#include <iterator>
#include <stdexcept>


static NSString *defaultScriptString = @"// set up a simple neutral simulation\n"
										"initialize() {\n"
										"	initializeMutationRate(1e-7);\n"
										"	\n"
										"	// m1 mutation type: neutral\n"
										"	initializeMutationType(1, 0.5, \"f\", 0.0);\n"
										"	\n"
										"	// g1 genomic element type: uses m1 for all mutations\n"
										"	initializeGenomicElementType(1, 1, 1.0);\n"
										"	\n"
										"	// uniform chromosome of length 100 kb with uniform recombination\n"
										"	initializeGenomicElement(1, 0, 99999);\n"
										"	initializeRecombinationRate(1e-8);\n"
										"}\n"
										"\n"
										"// create a population of 500 individuals\n"
										"1 {\n"
										"	sim.addSubpop(1, 500);\n"
										"}\n"
										"\n"
										"// output events: samples of 10 genomes periodically, all fixed mutations at end\n"
										"2000 { p1.outputSample(10); }\n"
										"4000 { p1.outputSample(10); }\n"
										"6000 { p1.outputSample(10); }\n"
										"8000 { p1.outputSample(10); }\n"
										"10000 { p1.outputSample(10); }\n"
										"10000 { sim.outputFixedMutations(); }\n";


@implementation SLiMWindowController

//
//	KVC / KVO / properties
//

@synthesize invalidSimulation, continuousPlayOn, reachedSimulationEnd, generationPlayOn;

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


//
//	Core class methods
//
#pragma mark Core class methods

+ (NSColor *)colorForIndex:(int)index
{
	static NSColor **colorArray = NULL;
	
	if (!colorArray)
	{
		colorArray = (NSColor **)malloc(8 * sizeof(NSColor *));
		
		colorArray[0] = [[NSColor colorWithCalibratedHue:0.65 saturation:0.65 brightness:1.0 alpha:1.0] retain];
		colorArray[1] = [[NSColor colorWithCalibratedHue:0.55 saturation:1.0 brightness:1.0 alpha:1.0] retain];
		colorArray[2] = [[NSColor colorWithCalibratedHue:0.40 saturation:1.0 brightness:0.9 alpha:1.0] retain];
		colorArray[3] = [[NSColor colorWithCalibratedHue:0.16 saturation:1.0 brightness:1.0 alpha:1.0] retain];
		colorArray[4] = [[NSColor colorWithCalibratedHue:0.00 saturation:0.65 brightness:1.0 alpha:1.0] retain];
		colorArray[5] = [[NSColor colorWithCalibratedHue:0.08 saturation:0.65 brightness:1.0 alpha:1.0] retain];
		colorArray[6] = [[NSColor colorWithCalibratedHue:0.75 saturation:0.55 brightness:1.0 alpha:1.0] retain];
		colorArray[7] = [[NSColor colorWithCalibratedWhite:0.8 alpha:1.0] retain];
	}
	
	return ((index >= 0) && (index <= 6)) ? colorArray[index] : colorArray[7];
}

- (id)initWithWindow:(NSWindow *)window
{
	if (self = [super initWithWindow:window])
	{
		// observe preferences that we care about
		[[NSUserDefaults standardUserDefaults] addObserver:self forKeyPath:defaultsSyntaxHighlightScriptKey options:0 context:NULL];
		[[NSUserDefaults standardUserDefaults] addObserver:self forKeyPath:defaultsSyntaxHighlightOutputKey options:0 context:NULL];
		
		// set default viewing state; this might come from user defaults on a per-script basis eventually...
		zoomedChromosomeShowsRecombinationIntervals = NO;
		zoomedChromosomeShowsGenomicElements = NO;
		zoomedChromosomeShowsMutations = YES;
		zoomedChromosomeShowsFixedSubstitutions = NO;
	}
	
	return self;
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
		delete sim;
	
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
	[self setScriptStringAndInitializeSimulation:defaultScriptString];
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
	
	if (object == defaults)
	{
		if ([keyPath isEqualToString:defaultsSyntaxHighlightScriptKey])
		{
			if ([defaults boolForKey:defaultsSyntaxHighlightScriptKey])
				[scriptTextView syntaxColorForEidos];
			else
				[scriptTextView clearSyntaxColoring];
		}
		
		if ([keyPath isEqualToString:defaultsSyntaxHighlightOutputKey])
		{
			if ([defaults boolForKey:defaultsSyntaxHighlightOutputKey])
				[outputTextView syntaxColorForOutput];
			else
				[outputTextView clearSyntaxColoring];
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
	
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
	
	[defaults removeObserver:self forKeyPath:defaultsSyntaxHighlightScriptKey context:NULL];
	[defaults removeObserver:self forKeyPath:defaultsSyntaxHighlightOutputKey context:NULL];
	
	[[NSNotificationCenter defaultCenter] removeObserver:self];
	
	[scriptString release];
	scriptString = nil;
	
	delete sim;
	sim = nil;
	
	gsl_rng_free(sim_rng);
	sim_rng = nil;
	
	[continuousPlayStartDate release];
	continuousPlayStartDate = nil;
	
	[genomicElementColorRegistry release];
	genomicElementColorRegistry = nil;
	
	// All graph windows attached to this controller need to be closed, since they refer back to us;
	// closing them will come back via windowWillClose: and make them release and nil themselves
	[self sendAllGraphWindowsSelector:@selector(close)];

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
		BOOL scrolledToBottom = ([enclosingScrollView hasVerticalScroller] && [[enclosingScrollView verticalScroller] doubleValue] == 1.0);
		
		[outputTextView replaceCharactersInRange:NSMakeRange([[outputTextView string] length], 0) withString:str];
		[outputTextView setFont:[NSFont fontWithName:@"Menlo" size:11.0]];
		if ([[NSUserDefaults standardUserDefaults] boolForKey:defaultsSyntaxHighlightOutputKey])
			[outputTextView syntaxColorForOutput];
		
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

- (void)updateAfterTick
{
	// Check whether the simulation has terminated due to an error; if so, show an error message with a delayed perform
	[self checkForSimulationTermination];
	
	// The rest of the code here needs to be careful about the invalid state; we do want to update our controls when invalid, but sim is nil.
	bool invalid = [self invalidSimulation];
	
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
			Subpopulation *subpop = popIter->second;
			BOOL rowSelected = subpop->gui_selected_;
			
			// OK, this is a bit tricky.  Suppose we are just starting a simulation; the subpop tableview is empty.  When a subpop gets added, we want to select
			// it automatically.  If five subpops get added simultaneously, we want to select them all; it would make no sense to select just the first one.  In
			// fact, if five subpops get added sequentially, we want to add each one in to the selection as it gets added.  This is because the default mode
			// should be to show all individuals, for all subpops, in the UI.  But now suppose the user selects just one subpop out of two that exist.  Now they
			// have expressed a deliberate preference to see only that subpop.  As other subpops get added and removed, we should not override that stated
			// preference.  So once the tableview has had a partial selection, we should abandon our behavior of automatically extending the selection to new
			// subpops.  The tricky question is: should we ever go back to the old behavior?  I think we should, if and only if the user selects all (>0)
			// subpops; that indicates a desire to see everything once again (although it could also be interpreted as a specific desire to see only those
			// subpops, even as others get added later; but that interpretation seems strained to me).
			if (!subpopTableviewHasHadPartialSelection)
			{
				rowSelected = YES;
				subpop->gui_selected_ = true;	// since we are now selecting this row, we need to let it know that...
			}
			
			if (rowSelected)
				[indicesToSelect addIndex:i];
			
			popIter++;
		}
		
		[subpopTableView selectRowIndexes:indicesToSelect byExtendingSelection:NO];
	}
	
	reloadingSubpopTableview = NO;
	
	// Now update our other UI, some of which depends upon the state of subpopTableView 
	[subpopTableView setNeedsDisplay];
	[populationView setNeedsDisplay:YES];
	[chromosomeZoomed setNeedsDisplay:YES];
	
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
		[chromosomeOverview setSelectedRange:NSMakeRange(0, 0)];
		[chromosomeOverview setNeedsDisplay:YES];
		
		if (sim)
			sim->chromosome_changed_ = false;
	}
	
	// Update graph windows as well; it is assumed that all graphs need updating every tick
	[self sendAllGraphViewsSelector:@selector(setNeedsDisplay)];
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
	
	// Set the script textview to show its string, with correct formatting
	[scriptTextView setString:scriptString];
	
	if ([[NSUserDefaults standardUserDefaults] boolForKey:defaultsSyntaxHighlightScriptKey])
		[scriptTextView syntaxColorForEidos];
	
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
	[self updateAfterTick];
	
	// Load our console window nib
	[[NSBundle mainBundle] loadNibNamed:@"ConsoleWindow" owner:self topLevelObjects:NULL];
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
				selectedSubpops.push_back(subpop);
			
			popIter++;
		}
	}
	
	return selectedSubpops;
}

- (NSColor *)colorForGenomicElementTypeID:(int)elementTypeID
{
	if (!genomicElementColorRegistry)
		genomicElementColorRegistry = [[NSMutableDictionary alloc] init];
	
	NSNumber *key = [NSNumber numberWithInt:elementTypeID];
	NSColor *elementColor = [genomicElementColorRegistry objectForKey:key];
	
	if (!elementColor)
	{
		int elementCount = (int)[genomicElementColorRegistry count];
		
		elementColor = [SLiMWindowController colorForIndex:elementCount];
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
		return !(invalidSimulation || continuousPlayOn || generationPlayOn || reachedSimulationEnd || hasImported || (sim->generation_ != 1));
	
	if (sel == @selector(exportScript:))
		return !(invalidSimulation || continuousPlayOn || generationPlayOn);
	if (sel == @selector(exportOutput:))
		return !(invalidSimulation || continuousPlayOn || generationPlayOn);
	if (sel == @selector(exportPopulation:))
		return !(invalidSimulation || continuousPlayOn || generationPlayOn);
	
	if (sel == @selector(clearOutput:))
		return !(invalidSimulation);
	if (sel == @selector(dumpPopulationToOutput:))
		return !(invalidSimulation);
	
	return YES;
}

- (void)addScriptBlockToSimulation:(SLiMEidosBlock *)scriptBlock
{
	if (sim)
	{
		sim->script_blocks_.push_back(scriptBlock);		// takes ownership from us
		
		[scriptBlocksTableView reloadData];
		[scriptBlocksTableView setNeedsDisplay];
	}
}


//
//	Actions
//
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
	
	[self willExecuteScript];
	stillRunning = sim->RunOneGeneration();
	[self didExecuteScript];
	
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
		[self updateAfterTick];
	}
}

- (void)_continuousPlay:(id)sender
{
	if (!invalidSimulation)
	{
		NSDate *startDate = [NSDate date];
		double speedSliderValue = [playSpeedSlider doubleValue];
		uint64 maxGenerationsPerSecond = UINT32_MAX;
		double intervalSinceStarting = -[continuousPlayStartDate timeIntervalSinceNow];
		
		if (speedSliderValue < 1.0)
			maxGenerationsPerSecond = (int)floor(speedSliderValue * speedSliderValue * 1000 + 1.0);
		//NSLog(@"speedSliderValue == %f, maxGenerationsPerSecond == %llu", speedSliderValue, maxGenerationsPerSecond);
		
		do
		{
			if (continuousPlayGenerationsCompleted / intervalSinceStarting >= maxGenerationsPerSecond)
				break;
			
			@autoreleasepool {
				[self setReachedSimulationEnd:![self runSimOneGeneration]];
			}
			
			continuousPlayGenerationsCompleted++;
		}
		while (!reachedSimulationEnd && (-[startDate timeIntervalSinceNow] < 0.01));
		
		[self updateAfterTick];
		
		if (!reachedSimulationEnd)
			[self performSelector:@selector(_continuousPlay:) withObject:nil afterDelay:0 inModes:[NSArray arrayWithObject:NSRunLoopCommonModes]];
		else
		{
			// stop playing
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
		[self updateAfterTick];
		
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
		
		do
		{
			if (sim->generation_ >= targetGeneration)
				break;
			
			@autoreleasepool {
				[self setReachedSimulationEnd:![self runSimOneGeneration]];
			}
		}
		while (!reachedSimulationEnd && (-[startDate timeIntervalSinceNow] < 0.01));
		
		[self updateAfterTick];
		
		if (!reachedSimulationEnd && !(sim->generation_ >= targetGeneration))
			[self performSelector:@selector(_generationPlay:) withObject:nil afterDelay:0 inModes:[NSArray arrayWithObject:NSRunLoopCommonModes]];
		else
		{
			// stop playing
			[self generationChanged:nil];
		}
	}
}

- (IBAction)generationChanged:(id)sender
{
	if (!generationPlayOn)
	{
		targetGeneration = (int)[generationTextField intValue];
		
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
	[self updateAfterTick];
	
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
	
	[chromosomeZoomed setNeedsDisplay:YES];
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
		SLiMEidosScript script(cstr, 0);
		
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
	[self checkScriptDidSucceed:!errorDiagnostic];
	
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
	}
	else
	{
		NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
		
		// On success, we optionally show a success alert sheet
		if (![defaults boolForKey:defaultsSuppressScriptCheckSuccessPanelKey])
		{
			NSAlert *alert = [[NSAlert alloc] init];
			
			[alert setAlertStyle:NSInformationalAlertStyle];
			[alert setMessageText:@"No script errors"];
			[alert setInformativeText:@"No errors found."];
			[alert addButtonWithTitle:@"OK"];
			[alert setShowsSuppressionButton:YES];
			
			[alert beginSheetModalForWindow:[self window] completionHandler:^(NSModalResponse returnCode) {
				if ([[alert suppressionButton] state] == NSOnState)
					[defaults setBool:YES forKey:defaultsSuppressScriptCheckSuccessPanelKey];
				[alert autorelease];
			}];
		}
	}
	
	[errorDiagnostic release];
}

- (IBAction)showScriptHelp:(id)sender
{
	// the AppDelegate manages the syntax help window, since it is a singleton, but it needs to know that we're
	// the one requesting it, so it can position the window next to us nicely, so we handle the action for it.
	[(AppDelegate *)[NSApp delegate] showScriptSyntaxHelpForSLiMWindowController:self];
}

- (IBAction)toggleConsoleVisibility:(id)sender
{
	[_consoleController toggleConsoleVisibility:sender];
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
		
		// dump fixed substitutions also; so the dump in SLiMgui is like output events 'A' + 'F'
		SLIM_OUTSTREAM << std::endl;
		SLIM_OUTSTREAM << "#OUT: " << sim->generation_ << " F " << std::endl;
		SLIM_OUTSTREAM << "Mutations:" << std::endl;
		
		for (int i = 0; i < sim->population_.substitutions_.size(); i++)
		{
			SLIM_OUTSTREAM << i;	// used to have a +1; switched to zero-based
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
		[chromosomeZoomed setNeedsDisplay:YES];
	}
}

- (IBAction)showGenomicElementsButtonToggled:(id)sender
{
	BOOL newValue = ([showGenomicElementsButton state] == NSOnState);
	
	if (newValue != zoomedChromosomeShowsGenomicElements)
	{
		zoomedChromosomeShowsGenomicElements = newValue;
		[chromosomeZoomed setShouldDrawGenomicElements:zoomedChromosomeShowsGenomicElements];
		[chromosomeZoomed setNeedsDisplay:YES];
	}
}

- (IBAction)showMutationsButtonToggled:(id)sender
{
	BOOL newValue = ([showMutationsButton state] == NSOnState);
	
	if (newValue != zoomedChromosomeShowsMutations)
	{
		zoomedChromosomeShowsMutations = newValue;
		[chromosomeZoomed setShouldDrawMutations:zoomedChromosomeShowsMutations];
		[chromosomeZoomed setNeedsDisplay:YES];
	}
}

- (IBAction)showFixedSubstitutionsButtonToggled:(id)sender
{
	BOOL newValue = ([showFixedSubstitutionsButton state] == NSOnState);
	
	if (newValue != zoomedChromosomeShowsFixedSubstitutions)
	{
		zoomedChromosomeShowsFixedSubstitutions = newValue;
		[chromosomeZoomed setShouldDrawFixedSubstitutions:zoomedChromosomeShowsFixedSubstitutions];
		[chromosomeZoomed setNeedsDisplay:YES];
	}
}

- (IBAction)importPopulation:(id)sender
{
	NSOpenPanel *op = [[NSOpenPanel openPanel] retain];
	
	[op setTitle:@"Import Population"];
	[op setCanChooseDirectories:NO];
	[op setCanChooseFiles:YES];
	[op setAllowsMultipleSelection:NO];
	[op setExtensionHidden:NO];
	[op setCanSelectHiddenExtension:NO];
	[op setAllowedFileTypes:[NSArray arrayWithObject:@"txt"]];
	
	[op beginSheetModalForWindow:[self window] completionHandler:^(NSInteger result) {
		if (result == NSFileHandlingPanelOKButton)
		{
			NSURL *fileURL = [op URL];
			NSString *popString = [NSString stringWithContentsOfURL:fileURL usedEncoding:NULL error:NULL];
			
			// Get rid of the open panel first, to avoid a weird transition
			[op orderOut:nil];
			
			if (popString)
			{
				BOOL success = NO;
				NSRegularExpression *regex = [NSRegularExpression regularExpressionWithPattern:@"^#OUT: ([0-9]+) A" options:NSRegularExpressionAnchorsMatchLines error:NULL];
				
				if ([regex numberOfMatchesInString:popString options:0 range:NSMakeRange(0, [popString length])] != 1)
				{
					// Show error: pop file did not have a well-formed output line
					NSAlert *alert = [[NSAlert alloc] init];
					
					[alert setAlertStyle:NSCriticalAlertStyle];
					[alert setMessageText:@"Import Population Error"];
					[alert setInformativeText:[NSString stringWithFormat:@"The population file did not have a well-formed #OUT line. Only population files generated by SLiM can be imported."]];
					[alert addButtonWithTitle:@"OK"];
					
					[alert beginSheetModalForWindow:[self window] completionHandler:^(NSModalResponse returnCode) { [alert autorelease]; }];
				}
				else
				{
					NSRange popSeedRange = [popString rangeOfString:@"\n#SEED\n"];
					
					if (popSeedRange.location != NSNotFound)
					{
						NSString *outputString = [outputTextView string];
						NSRange outSeedRange = [outputString rangeOfString:@"\n#SEED\n"];
						
						if (outSeedRange.location != NSNotFound)
						{
							NSString *strippedPopString = [popString substringToIndex:popSeedRange.location];
							NSString *strippedOutString = [outputString substringToIndex:outSeedRange.location];
							
							if ([strippedPopString isEqualToString:strippedOutString])
							{
								// First we need to read in the script, which will set up the chromosome, mutation types, events, etc.
								
								
								// Then import the population file
								const char *filePath = [fileURL fileSystemRepresentation];
								
								sim->InitializePopulationFromFile(filePath);
								
								// Figure out what generation the file says to start at
								NSArray *matches = [regex matchesInString:popString options:0 range:NSMakeRange(0, [popString length])];
								NSTextCheckingResult *match = [matches objectAtIndex:0];	// guaranteed to exist
								NSRange generationRange = [match rangeAtIndex:1];			// first capture of the match
								NSString *generationString = [popString substringWithRange:generationRange];
								int newGeneration = [generationString intValue];
								
								// Mark that we have imported
								SLIM_OUTSTREAM << "// Imported population from: " << filePath << std::endl;
								SLIM_OUTSTREAM << "// Setting generation from import file:\n" << newGeneration << "\n" << std::endl;
								hasImported = YES;
								success = YES;
								
								// Clean up after the import; the order of these steps is quite sensitive, so be careful
								sim->generation_ = newGeneration;
								[self updateAfterTick];							// we need to do this first, to select all our subpopulations so our stats-gathering is correct
								sim->population_.TallyMutationReferences();
								sim->population_.SurveyPopulation();
							}
						}
					}
					
					if (!success)
					{
						// Show error: pop file did not correspond to the current script
						NSAlert *alert = [[NSAlert alloc] init];
						
						[alert setAlertStyle:NSCriticalAlertStyle];
						[alert setMessageText:@"Import Population Error"];
						[alert setInformativeText:@"The population file does not appear to match the current script. The correct script must be loaded before importing. (If you have the correct script, recycle to initialize with it.)"];
						[alert addButtonWithTitle:@"OK"];
						
						[alert beginSheetModalForWindow:[self window] completionHandler:^(NSModalResponse returnCode) { [alert autorelease]; }];
					}
				}
			}
			else
			{
				// Show error: file contents could not be read
				NSAlert *alert = [[NSAlert alloc] init];
				
				[alert setAlertStyle:NSCriticalAlertStyle];
				[alert setMessageText:@"Import Population Error"];
				[alert setInformativeText:@"The population file could not be read."];
				[alert addButtonWithTitle:@"OK"];
				
				[alert beginSheetModalForWindow:[self window] completionHandler:^(NSModalResponse returnCode) { [alert autorelease]; }];
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
	[sp setAllowedFileTypes:[NSArray arrayWithObject:@"txt"]];
	
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
	[sp setAllowedFileTypes:[NSArray arrayWithObject:@"txt"]];
	
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
	[sp setAllowedFileTypes:[NSArray arrayWithObject:@"txt"]];
	
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
//	ConsoleWindowController delegate methods
//
#pragma mark ConsoleWindowController delegate

- (void)appendWelcomeMessageAddendum
{
	EidosConsoleTextView *textView = [_consoleController textView];
	NSTextStorage *ts = [textView textStorage];
	NSDictionary *outputAttrs = [EidosConsoleTextView outputAttrs];
	NSAttributedString *launchString = [[NSAttributedString alloc] initWithString:@"Connected to SLiMgui simulation.\nSLiM version 2.0a3.\n" attributes:outputAttrs];
	NSAttributedString *dividerString = [[NSAttributedString alloc] initWithString:@"\n---------------------------------------------------------\n\n" attributes:outputAttrs];
	
	[ts beginEditing];
	[ts replaceCharactersInRange:NSMakeRange([ts length], 0) withAttributedString:launchString];
	[ts replaceCharactersInRange:NSMakeRange([ts length], 0) withAttributedString:dividerString];
	[ts endEditing];
	
	[launchString release];
	[dividerString release];
}

- (void)injectIntoInterpreter:(EidosInterpreter *)interpreter
{
	if (sim && !invalidSimulation)
		sim->InjectIntoInterpreter(*interpreter, nullptr, false);
}

- (EidosSymbolTable *)globalSymbolTableForCompletion
{
	return [_consoleController symbols];
}

- (NSArray *)languageKeywordsForCompletion
{
	return [NSArray arrayWithObjects:@"initialize", @"fitness", @"mateChoice", @"modifyChild", nil];
}

- (std::vector<EidosFunctionSignature*> *)injectedFunctionSignatures
{
	if (sim && !invalidSimulation)
		return sim->InjectedFunctionSignatures();
	
	return nullptr;
}

- (void)checkScriptDidSucceed:(BOOL)succeeded
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

- (void)willExecuteScript
{
	// Whenever we are about to execute script, we swap in our random number generator; at other times, gEidos_rng is NULL.
	// The goal here is to keep each SLiM window independent in its random number sequence.
	if (gEidos_rng)
		NSLog(@"willExecuteScript: gEidos_rng already set up!");
	
	gEidos_rng = sim_rng;
	gEidos_random_bool_bit_counter = sim_random_bool_bit_counter;
	gEidos_random_bool_bit_buffer = sim_random_bool_bit_buffer;
	gEidos_rng_last_seed = sim_rng_last_seed;
}

- (void)didExecuteScript
{
	// Swap our random number generator back out again; see -willExecuteScript
	sim_rng = gEidos_rng;
	sim_random_bool_bit_counter = gEidos_random_bool_bit_counter;
	sim_random_bool_bit_buffer = gEidos_random_bool_bit_buffer;
	sim_rng_last_seed = gEidos_rng_last_seed;
	
	gEidos_rng = NULL;
}

- (void)consoleWindowWillClose
{
	[consoleButton setState:NSOffState];
}


//
//	NSWindow delegate methods
//
#pragma mark NSWindow delegate

- (void)windowWillClose:(NSNotification *)notification
{
	// We are the delegate of our own window, and of all of our graph windows, too
	NSWindow *closingWindow = [notification object];
	
	if (closingWindow == [self window])
	{
		[closingWindow setDelegate:nil];
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
#pragma mark NSTextView delegate

- (void)textDidChange:(NSNotification *)notification
{
	NSTextView *textView = (NSTextView *)[notification object];
	
	if ([[NSUserDefaults standardUserDefaults] boolForKey:defaultsSyntaxHighlightScriptKey])
	{
		if (textView == scriptTextView)
			[scriptTextView syntaxColorForEidos];
		else if (textView == outputTextView)
			[outputTextView syntaxColorForOutput];
	}
	
	if (textView == scriptTextView)
		[self setDocumentEdited:YES];	// this still doesn't set up the "Edited" marker in the window title bar, because we're not using NSDocument
}

//
//	NSTableView delegate methods
//
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
			int selectedCount = 0;
			
			for (int i = 0; i < subpopCount; ++i)
			{
				Subpopulation *subpop = popIter->second;
				BOOL rowSelected = [subpopTableView isRowSelected:i];
				
				subpop->gui_selected_ = rowSelected;
				
				// remember if we have ever had anything other than a complete selection; see comments in -updateAfterTick
				if (rowSelected)
					selectedCount++;
				else
					subpopTableviewHasHadPartialSelection = YES;
				
				popIter++;
			}
			
			// If every subpop has been selected (and that is more than zero subpops), the user has expressed a desire to go back to showing
			// all of the subpops, not a subset.  See comment in -updateAfterTick for more on subpopTableviewHasHadPartialSelection.
			if ((selectedCount == subpopCount) && (subpopCount > 0))
				subpopTableviewHasHadPartialSelection = NO;
			
			// If the selection has changed, that means that the mutation tallies need to be recomputed
			population.TallyMutationReferences();
			
			// It's a bit hard to tell for sure whether we need to update or not, since a selected subpop might have been removed from the tableview;
			// selection changes should not happen often, so we can just always update, I think.
			[populationView setNeedsDisplay:YES];
			[chromosomeZoomed setNeedsDisplay:YES];
		}
	}
}


//
//	NSTableView datasource methods
//
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
				int subpop_id = popIter->first;
				Subpopulation *subpop = popIter->second;
				
				if (aTableColumn == subpopIDColumn)
				{
					return [NSString stringWithFormat:@"p%d", subpop_id];
				}
				else if (aTableColumn == subpopSizeColumn)
				{
					return [NSString stringWithFormat:@"%d", subpop->child_subpop_size_];
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
			std::map<int,MutationType*> &mutationTypes = sim->mutation_types_;
			int mutationTypeCount = (int)mutationTypes.size();
			
			if (rowIndex < mutationTypeCount)
			{
				auto mutTypeIter = mutationTypes.begin();
				
				std::advance(mutTypeIter, rowIndex);
				int mutTypeID = mutTypeIter->first;
				MutationType *mutationType = mutTypeIter->second;
				
				if (aTableColumn == mutTypeIDColumn)
				{
					return [NSString stringWithFormat:@"m%d", mutTypeID];
				}
				else if (aTableColumn == mutTypeDominanceColumn)
				{
					return [NSString stringWithFormat:@"%.3f", mutationType->dominance_coeff_];
				}
				else if (aTableColumn == mutTypeDFETypeColumn)
				{
					switch (mutationType->dfe_type_)
					{
						case 'f': return @"fixed";
						case 'e': return @"exp";
						case 'g': return @"gamma";
						default: return @"?";
					}
				}
				else if (aTableColumn == mutTypeDFEParamsColumn)
				{
					NSMutableString *paramString = [[NSMutableString alloc] init];
					
					for (int paramIndex = 0; paramIndex < mutationType->dfe_parameters_.size(); ++paramIndex)
					{
						NSString *paramSymbol = @"";
						
						switch (mutationType->dfe_type_)
						{
							case 'f': paramSymbol = @"s"; break;
							case 'e': paramSymbol = @"s̄"; break;
							case 'g': paramSymbol = (paramIndex == 0 ? @"s̄" : @"α"); break;
						}
						
						[paramString appendFormat:@"%@=%.3f", paramSymbol, mutationType->dfe_parameters_[paramIndex]];
						
						if (paramIndex < mutationType->dfe_parameters_.size() - 1)
							[paramString appendString:@", "];
					}
					
					return [paramString autorelease];
				}
			}
		}
		else if (aTableView == genomicElementTypeTableView)
		{
			std::map<int,GenomicElementType*> &genomicElementTypes = sim->genomic_element_types_;
			int genomicElementTypeCount = (int)genomicElementTypes.size();
			
			if (rowIndex < genomicElementTypeCount)
			{
				auto genomicElementTypeIter = genomicElementTypes.begin();
				
				std::advance(genomicElementTypeIter, rowIndex);
				int genomicElementTypeID = genomicElementTypeIter->first;
				GenomicElementType *genomicElementType = genomicElementTypeIter->second;
				
				if (aTableColumn == genomicElementTypeIDColumn)
				{
					return [NSString stringWithFormat:@"g%d", genomicElementTypeID];
				}
				else if (aTableColumn == genomicElementTypeColorColumn)
				{
					return [self colorForGenomicElementTypeID:genomicElementTypeID];
				}
				else if (aTableColumn == genomicElementTypeMutationTypesColumn)
				{
					NSMutableString *paramString = [[NSMutableString alloc] init];
					
					for (int mutTypeIndex = 0; mutTypeIndex < genomicElementType->mutation_fractions_.size(); ++mutTypeIndex)
					{
						MutationType *mutType = genomicElementType->mutation_type_ptrs_[mutTypeIndex];
						double mutTypeFraction = genomicElementType->mutation_fractions_[mutTypeIndex];
						
						[paramString appendFormat:@"m%d=%.3f", mutType->mutation_type_id_, mutTypeFraction];
						
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
					int block_id = scriptBlock->block_id_;
					
					if (block_id == -1)
						return @"—";
					else
						return [NSString stringWithFormat:@"s%d", block_id];
				}
				else if (aTableColumn == scriptBlocksStartColumn)
				{
					if (scriptBlock->start_generation_ == -1)
						return @"MIN";
					else
						return [NSString stringWithFormat:@"%d", scriptBlock->start_generation_];
				}
				else if (aTableColumn == scriptBlocksEndColumn)
				{
					if (scriptBlock->end_generation_ == INT_MAX)
						return @"MAX";
					else
						return [NSString stringWithFormat:@"%d", scriptBlock->end_generation_];
				}
				else if (aTableColumn == scriptBlocksTypeColumn)
				{
					switch (scriptBlock->type_)
					{
						case SLiMEidosBlockType::SLiMEidosEvent:				return @"event";
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
			ns_script_string = [ns_script_string stringByReplacingOccurrencesOfString:@" " withString:@" "];		// second string is an &nbsp;
			ns_script_string = [ns_script_string stringByReplacingOccurrencesOfString:@"\t" withString:@"    "];	// second string is four &nbsp;s
			
			return ns_script_string;
		}
	}
	
	return nil;
}


//
//	NSSplitView delegate methods
//
#pragma mark NSSplitView delegate

- (BOOL)splitView:(NSSplitView *)splitView canCollapseSubview:(NSView *)subview
{
	return YES;
}

- (CGFloat)splitView:(NSSplitView *)splitView constrainMaxCoordinate:(CGFloat)proposedMax ofSubviewAt:(NSInteger)dividerIndex
{
	return proposedMax - 200;
}

- (CGFloat)splitView:(NSSplitView *)splitView constrainMinCoordinate:(CGFloat)proposedMin ofSubviewAt:(NSInteger)dividerIndex
{
	return proposedMin + 200;
}


//
//	NSDrawer delegate methods
//
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

























































