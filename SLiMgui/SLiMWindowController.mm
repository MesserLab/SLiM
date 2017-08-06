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
#import "SLiMDocument.h"
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
#import "EidosPrettyprinter.h"
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

@synthesize invalidSimulation, continuousPlayOn, profilePlayOn, nonProfilePlayOn, reachedSimulationEnd, generationPlayOn;

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

- (instancetype)init
{
	if (self = [super initWithWindowNibName:@"SLiMWindow"])
	{
		// observe preferences that we care about
		[[NSUserDefaults standardUserDefaults] addObserver:self forKeyPath:defaultsSyntaxHighlightScriptKey options:0 context:NULL];
		[[NSUserDefaults standardUserDefaults] addObserver:self forKeyPath:defaultsSyntaxHighlightOutputKey options:0 context:NULL];
		[[NSUserDefaults standardUserDefaults] addObserver:self forKeyPath:defaultsDisplayFontSizeKey options:0 context:NULL];
		
		observingKeyPaths = YES;
		
		// Observe notifications to keep our variable browser toggle button up to date
		[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(browserWillShow:) name:EidosVariableBrowserWillShowNotification object:nil];
		[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(browserWillHide:) name:EidosVariableBrowserWillHideNotification object:nil];
		
		// set default viewing state; this might come from user defaults on a per-script basis eventually...
		zoomedChromosomeShowsRateMaps = NO;
		zoomedChromosomeShowsGenomicElements = NO;
		zoomedChromosomeShowsMutations = YES;
		zoomedChromosomeShowsFixedSubstitutions = NO;
	}
	
	return self;
}

- (void)cleanup
{
	//NSLog(@"[SLiMWindowController cleanup]");
	
	[[NSNotificationCenter defaultCenter] removeObserver:self];
	
	[NSObject cancelPreviousPerformRequestsWithTarget:self];	// _generationPlay:, _continuousPlay:, _continuousProfile:, etc.
	
	// Disconnect delegate relationships
	[overallSplitView setDelegate:nil];
	overallSplitView = nil;
	
	[bottomSplitView setDelegate:nil];
	bottomSplitView = nil;
	
	[scriptTextView setDelegate:nil];
	scriptTextView = nil;
	
	[outputTextView setDelegate:nil];
	outputTextView = nil;
	
	[_consoleController setDelegate:nil];
	
	// Remove observers
	if (observingKeyPaths)
	{
		NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
	
		[defaults removeObserver:self forKeyPath:defaultsSyntaxHighlightScriptKey context:NULL];
		[defaults removeObserver:self forKeyPath:defaultsSyntaxHighlightOutputKey context:NULL];
		[defaults removeObserver:self forKeyPath:defaultsDisplayFontSizeKey context:NULL];
		
		observingKeyPaths = NO;
	}
	
	[[NSNotificationCenter defaultCenter] removeObserver:self];
	
	[NSObject cancelPreviousPerformRequestsWithTarget:playSpeedToolTipWindow selector:@selector(orderOut:) object:nil];
	[playSpeedToolTipWindow orderOut:nil];
	[playSpeedToolTipWindow release];
	playSpeedToolTipWindow = nil;
	
	// Free resources
	[scriptString release];
	scriptString = nil;
	
	if (sim)
	{
		delete sim;
		sim = nullptr;
	}
	
	if (sim_rng)
	{
		gsl_rng_free(sim_rng);
		sim_rng = nil;
	}
	
	[continuousPlayStartDate release];
	continuousPlayStartDate = nil;
	
	[profileEndDate release];
	profileEndDate = nil;
	
	[genomicElementColorRegistry release];
	genomicElementColorRegistry = nil;
	
	// All graph windows attached to this controller need to be closed, since they refer back to us;
	// closing them will come back via windowWillClose: and make them release and nil themselves
	[self sendAllGraphViewsSelector:@selector(cleanup)];
	[self sendAllGraphWindowsSelector:@selector(close)];
	
	[graphWindowMutationFreqSpectrum autorelease];
	[graphWindowMutationFreqTrajectories autorelease];
	[graphWindowMutationLossTimeHistogram autorelease];
	[graphWindowMutationFixationTimeHistogram autorelease];
	[graphWindowFitnessOverTime autorelease];
	[graphWindowPopulationVisualization autorelease];
	
	graphWindowMutationFreqSpectrum = nil;
	graphWindowMutationFreqTrajectories = nil;
	graphWindowMutationLossTimeHistogram = nil;
	graphWindowMutationFixationTimeHistogram = nil;
	graphWindowFitnessOverTime = nil;
	graphWindowPopulationVisualization = nil;
	
	// We also need to close and release our console window and its associated variable browser window.
	// We don't track the console or var browser in windowWillClose: since we want those windows to
	// continue to exist even when they are hidden, unlike graph windows.
	[[_consoleController browserController] hideWindow];
	[_consoleController hideWindow];
	[self setConsoleController:nil];
}

- (void)dealloc
{
	//NSLog(@"[SLiMWindowController dealloc]");
	
	[self cleanup];
	
	if ([self document])
		[self setDocument:nil];
	
	[super dealloc];
}

- (void)setDocument:(id)document
{
	[super setDocument:document];
	
	// The document currently has our model information, but we keep the model, so we need to pull it over
	if ([self document])
		[self setScriptStringAndInitializeSimulation:[[self document] documentScriptString]];
}

- (NSString *)windowTitleForDocumentDisplayName:(NSString *)displayName
{
	NSString *superString = [super windowTitleForDocumentDisplayName:displayName];
	SLiMDocument *document = [self document];
	NSString *recipeName = [document recipeName];
	NSURL *docURL = [document fileURL];
	
	// If we have a recipe name, *and* we are an unsaved (i.e. untitled) document, then we want to customize the window title
	if (recipeName && !docURL && [displayName hasPrefix:[document defaultDraftName]])
	{
		NSRange spaceRange = [recipeName rangeOfString:@" "];
		
		if (spaceRange.location != NSNotFound)
		{
			NSString *recipeNumber = [recipeName substringToIndex:spaceRange.location];
			
			if ([recipeNumber length])
				return [NSString stringWithFormat:@"%@ [Recipe %@]", superString, recipeNumber];
		}
	}
	
	return superString;
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
	NSDictionary *errorAttrs = [NSDictionary eidosTextAttributesWithColor:[NSColor redColor] size:11.0];
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
	catch (...)
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
		
		if ([keyPath isEqualToString:defaultsDisplayFontSizeKey])
		{
			int fontSize = (int)[defaults integerForKey:defaultsDisplayFontSizeKey];
			
			[scriptTextView setDisplayFontSize:fontSize];
			[outputTextView setDisplayFontSize:fontSize];
			
			[[_consoleController scriptTextView] setDisplayFontSize:fontSize];
			[[_consoleController outputTextView] setDisplayFontSize:fontSize];
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

- (void)updateOutputTextView
{
	std::string &&newOutput = gSLiMOut.str();
	
	if (!newOutput.empty())
	{
		const char *cstr = newOutput.c_str();
		NSString *str = [NSString stringWithUTF8String:cstr];
		
		// So, ideally we would stay pinned at the bottom if the user had scrolled to the bottom, but would stay
		// at the user's chosen scroll position above the bottom if they chose such a position.  Unfortunately,
		// this doesn't seem to work.  I'm not quite sure why.  Particularly when large amounts of output get
		// added quickly, the scroller doesn't seem to catch up, and then it reads here as not being at the
		// bottom, and so we become unpinned even though we used to be pinned.  I'm going to just give up, for
		// now, and always scroll to the bottom when new output comes out.  That's what many other such apps
		// do anyway; it's a little annoying if you're trying to read old output, but so it goes.
		
		//NSScrollView *enclosingScrollView = [outputTextView enclosingScrollView];
		BOOL scrolledToBottom = YES; //(![enclosingScrollView hasVerticalScroller] || [[enclosingScrollView verticalScroller] doubleValue] == 1.0);
		
		[outputTextView replaceCharactersInRange:NSMakeRange([[outputTextView string] length], 0) withString:str];
		[outputTextView setFont:[NSFont fontWithName:@"Menlo" size:[outputTextView displayFontSize]]];
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
	BOOL canDisplayPopulationView = [populationView canDisplaySubpopulations:selectedSubpopulations];
	
	// Swap between populationView and populationErrorView as needed to show text messages in the pop view area
	if (canDisplayPopulationView)
	{
		if ([populationView isHidden])
		{
			[populationView setHidden:NO];
			[populationErrorView setHidden:YES];
		}
	}
	else
	{
		if (![populationView isHidden])
		{
			[populationView setHidden:YES];
			[populationErrorView setHidden:NO];
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
	[overallSplitView eidosRestoreAutosavedPositionsWithName:@"SLiMgui Overall Splitter"];
	[bottomSplitView eidosRestoreAutosavedPositionsWithName:@"SLiMgui Splitter"];
	
	// THEN set the autosave names on the splitviews; this prevents NSSplitView from getting confused
	[overallSplitView setAutosaveName:@"SLiMgui Overall Splitter"];
	[bottomSplitView setAutosaveName:@"SLiMgui Splitter"];
	
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
	[scriptTextView recolorAfterChanges];
	
	// Set up our chromosome views to show the proper stuff
	[chromosomeOverview setReferenceChromosomeView:nil];
	[chromosomeOverview setSelectable:YES];
	[chromosomeOverview setShouldDrawGenomicElements:YES];
	[chromosomeOverview setShouldDrawMutations:NO];
	[chromosomeOverview setShouldDrawFixedSubstitutions:NO];
	[chromosomeOverview setShouldDrawRateMaps:NO];
	
	[chromosomeZoomed setReferenceChromosomeView:chromosomeOverview];
	[chromosomeZoomed setSelectable:NO];
	[chromosomeZoomed setShouldDrawGenomicElements:zoomedChromosomeShowsGenomicElements];
	[chromosomeZoomed setShouldDrawMutations:zoomedChromosomeShowsMutations];
	[chromosomeZoomed setShouldDrawFixedSubstitutions:zoomedChromosomeShowsFixedSubstitutions];
	[chromosomeZoomed setShouldDrawRateMaps:zoomedChromosomeShowsRateMaps];
	
	[showRecombinationIntervalsButton setState:(zoomedChromosomeShowsRateMaps ? NSOnState : NSOffState)];
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

- (NSColor *)colorForGenomicElementType:(GenomicElementType *)elementType withID:(slim_objectid_t)elementTypeID
{
	if (elementType && !elementType->color_.empty())
	{
		return [NSColor colorWithCalibratedRed:elementType->color_red_ green:elementType->color_green_ blue:elementType->color_blue_ alpha:1.0];
	}
	else
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
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem
{
	SEL sel = [menuItem action];
	
	if (sel == @selector(playOneStep:))
		return !(invalidSimulation || continuousPlayOn || generationPlayOn || reachedSimulationEnd);
	if (sel == @selector(play:))
	{
		[menuItem setTitle:(nonProfilePlayOn ? @"Stop" : @"Play")];
		
		return !(invalidSimulation || generationPlayOn || reachedSimulationEnd || profilePlayOn);
	}
	if (sel == @selector(profile:))
	{
		[menuItem setTitle:(profilePlayOn ? @"Stop" : @"Profile")];
		
		return !(invalidSimulation || generationPlayOn || reachedSimulationEnd || nonProfilePlayOn);
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
	if (sel == @selector(prettyprintScript:))
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
		
		sim->AddScriptBlock(scriptBlock, nullptr, nullptr);		// takes ownership from us
		
		[scriptBlocksTableView reloadData];
		[scriptBlocksTableView setNeedsDisplay];
	}
}

- (void)updateRecycleHighlightForChangeCount:(int)changeCount
{
	if (changeCount)
		[recycleButton slimSetTintColor:[NSColor colorWithCalibratedHue:0.33 saturation:0.4 brightness:1.0 alpha:1.0]];
	else
		[recycleButton slimSetTintColor:nil];
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
	[[self document] setTransient:NO]; // Since the user has taken an interest in the window, clear the document's transient status
	
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

#if defined(SLIMGUI) && (SLIMPROFILING == 1)

- (void)displayProfileResults
{
	[[NSBundle mainBundle] loadNibNamed:@"ProfileReport" owner:self topLevelObjects:NULL];
	
	// Set up the report window attributes
	[profileWindow setTitle:[NSString stringWithFormat:@"Profile Report for %@", [[self window] title]]];
	[profileTextView setTextContainerInset:NSMakeSize(5.0, 10.0)];
	
	// Build the report attributed string
	NSMutableAttributedString *content = [[[NSMutableAttributedString alloc] init] autorelease];
	NSFont *optima18b = [NSFont fontWithName:@"Optima-Bold" size:18.0];
	NSFont *optima14b = [NSFont fontWithName:@"Optima-Bold" size:14.0];
	NSFont *optima13 = [NSFont fontWithName:@"Optima" size:13.0];
	NSFont *optima13i = [NSFont fontWithName:@"Optima-Italic" size:13.0];
	NSFont *optima8 = [NSFont fontWithName:@"Optima" size:8.0];
	NSFont *optima3 = [NSFont fontWithName:@"Optima" size:3.0];
	NSFont *menlo11 = [NSFont fontWithName:@"Menlo" size:11.0];
	
	NSDictionary *optima14b_d = @{NSFontAttributeName : optima14b};
	NSDictionary *optima13_d = @{NSFontAttributeName : optima13};
	NSDictionary *optima13i_d = @{NSFontAttributeName : optima13i};
	NSDictionary *optima8_d = @{NSFontAttributeName : optima8};
	NSDictionary *optima3_d = @{NSFontAttributeName : optima3};
	NSDictionary *menlo11_d = @{NSFontAttributeName : menlo11};
	
	NSString *startDateString = [NSDateFormatter localizedStringFromDate:continuousPlayStartDate dateStyle:NSDateFormatterShortStyle timeStyle:NSDateFormatterMediumStyle];
	NSString *endDateString = [NSDateFormatter localizedStringFromDate:profileEndDate dateStyle:NSDateFormatterShortStyle timeStyle:NSDateFormatterMediumStyle];
	NSTimeInterval elapsedWallClockTime = [profileEndDate timeIntervalSinceDate:continuousPlayStartDate];
	double elapsedCPUTimeInSLiM = profileElapsedCPUClock / (double)CLOCKS_PER_SEC;
	double elapsedWallClockTimeInSLiM = Eidos_ElapsedProfileTime(profileElapsedWallClock);
	
	[content eidosAppendString:@"Profile Report\n" attributes:@{NSFontAttributeName : optima18b}];
	[content eidosAppendString:@"\n" attributes:optima3_d];
	
	[content eidosAppendString:[NSString stringWithFormat:@"Model: %@\n", [[self window] title]] attributes:optima13_d];
	[content eidosAppendString:@"\n" attributes:optima8_d];
	
	[content eidosAppendString:[NSString stringWithFormat:@"Run start: %@\n", startDateString] attributes:optima13_d];
	[content eidosAppendString:[NSString stringWithFormat:@"Run end: %@\n", endDateString] attributes:optima13_d];
	[content eidosAppendString:@"\n" attributes:optima8_d];
	
	[content eidosAppendString:[NSString stringWithFormat:@"Elapsed wall clock time: %0.2f s\n", (double)elapsedWallClockTime] attributes:optima13_d];
	[content eidosAppendString:[NSString stringWithFormat:@"Elapsed wall clock time inside SLiM core (corrected): %0.2f s\n", (double)elapsedWallClockTimeInSLiM] attributes:optima13_d];
	[content eidosAppendString:[NSString stringWithFormat:@"Elapsed CPU time inside SLiM core (uncorrected): %0.2f s\n", (double)elapsedCPUTimeInSLiM] attributes:optima13_d];
	[content eidosAppendString:[NSString stringWithFormat:@"Elapsed generations: %d%@\n", (int)continuousPlayGenerationsCompleted, (profileStartGeneration == 0) ? @" (including initialize)" : @""] attributes:optima13_d];
	[content eidosAppendString:@"\n" attributes:optima8_d];
	
	[content eidosAppendString:[NSString stringWithFormat:@"Profile block external overhead: %0.2f ticks (%0.4g s)\n", gEidos_ProfileOverheadTicks, gEidos_ProfileOverheadSeconds] attributes:optima13_d];
	[content eidosAppendString:[NSString stringWithFormat:@"Profile block internal lag: %0.2f ticks (%0.4g s)\n", gEidos_ProfileLagTicks, gEidos_ProfileLagSeconds] attributes:optima13_d];
	
	
	//
	//	Generation stage breakdown
	//
	if (elapsedWallClockTimeInSLiM > 0.0)
	{
		double elapsedStage0Time = Eidos_ElapsedProfileTime(sim->profile_stage_totals_[0]);
		double elapsedStage1Time = Eidos_ElapsedProfileTime(sim->profile_stage_totals_[1]);
		double elapsedStage2Time = Eidos_ElapsedProfileTime(sim->profile_stage_totals_[2]);
		double elapsedStage3Time = Eidos_ElapsedProfileTime(sim->profile_stage_totals_[3]);
		double elapsedStage4Time = Eidos_ElapsedProfileTime(sim->profile_stage_totals_[4]);
		double elapsedStage5Time = Eidos_ElapsedProfileTime(sim->profile_stage_totals_[5]);
		double elapsedStage6Time = Eidos_ElapsedProfileTime(sim->profile_stage_totals_[6]);
		double percentStage0 = (elapsedStage0Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentStage1 = (elapsedStage1Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentStage2 = (elapsedStage2Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentStage3 = (elapsedStage3Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentStage4 = (elapsedStage4Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentStage5 = (elapsedStage5Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentStage6 = (elapsedStage6Time / elapsedWallClockTimeInSLiM) * 100.0;
		int fw = 4;
		
		fw = std::max(fw, 3 + (int)ceil(log10(floor(elapsedStage0Time))));
		fw = std::max(fw, 3 + (int)ceil(log10(floor(elapsedStage1Time))));
		fw = std::max(fw, 3 + (int)ceil(log10(floor(elapsedStage2Time))));
		fw = std::max(fw, 3 + (int)ceil(log10(floor(elapsedStage3Time))));
		fw = std::max(fw, 3 + (int)ceil(log10(floor(elapsedStage4Time))));
		fw = std::max(fw, 3 + (int)ceil(log10(floor(elapsedStage5Time))));
		fw = std::max(fw, 3 + (int)ceil(log10(floor(elapsedStage6Time))));
		
		[content eidosAppendString:@"\n" attributes:optima13_d];
		[content eidosAppendString:@"Generation stage breakdown\n" attributes:optima14b_d];
		[content eidosAppendString:@"\n" attributes:optima3_d];
		
		[content eidosAppendString:[NSString stringWithFormat:@"%*.2f s (%5.2f%%)", fw, elapsedStage0Time, percentStage0] attributes:menlo11_d];
		[content eidosAppendString:@" : initialize() callback execution\n" attributes:optima13_d];
		
		[content eidosAppendString:[NSString stringWithFormat:@"%*.2f s (%5.2f%%)", fw, elapsedStage1Time, percentStage1] attributes:menlo11_d];
		[content eidosAppendString:@" : stage 1 – early() event execution\n" attributes:optima13_d];
		
		[content eidosAppendString:[NSString stringWithFormat:@"%*.2f s (%5.2f%%)", fw, elapsedStage2Time, percentStage2] attributes:menlo11_d];
		[content eidosAppendString:@" : stage 2 – offspring generation\n" attributes:optima13_d];
		
		[content eidosAppendString:[NSString stringWithFormat:@"%*.2f s (%5.2f%%)", fw, elapsedStage3Time, percentStage3] attributes:menlo11_d];
		[content eidosAppendString:@" : stage 3 – bookkeeping (fixed mutation removal, etc.)\n" attributes:optima13_d];
		
		[content eidosAppendString:[NSString stringWithFormat:@"%*.2f s (%5.2f%%)", fw, elapsedStage4Time, percentStage4] attributes:menlo11_d];
		[content eidosAppendString:@" : stage 4 – generation swap\n" attributes:optima13_d];
		
		[content eidosAppendString:[NSString stringWithFormat:@"%*.2f s (%5.2f%%)", fw, elapsedStage5Time, percentStage5] attributes:menlo11_d];
		[content eidosAppendString:@" : stage 5 – late() event execution\n" attributes:optima13_d];
		
		[content eidosAppendString:[NSString stringWithFormat:@"%*.2f s (%5.2f%%)", fw, elapsedStage6Time, percentStage6] attributes:menlo11_d];
		[content eidosAppendString:@" : stage 6 – fitness calculation\n" attributes:optima13_d];
	}
	
	//
	//	Callback type breakdown
	//
	if (elapsedWallClockTimeInSLiM > 0.0)
	{
		double elapsedType0Time = Eidos_ElapsedProfileTime(sim->profile_callback_totals_[0]);
		double elapsedType1Time = Eidos_ElapsedProfileTime(sim->profile_callback_totals_[1]);
		double elapsedType2Time = Eidos_ElapsedProfileTime(sim->profile_callback_totals_[2]);
		double elapsedType3Time = Eidos_ElapsedProfileTime(sim->profile_callback_totals_[3]);
		double elapsedType4Time = Eidos_ElapsedProfileTime(sim->profile_callback_totals_[4]);
		double elapsedType5Time = Eidos_ElapsedProfileTime(sim->profile_callback_totals_[5]);
		double elapsedType6Time = Eidos_ElapsedProfileTime(sim->profile_callback_totals_[6]);
		double elapsedType7Time = Eidos_ElapsedProfileTime(sim->profile_callback_totals_[7]);
		double elapsedType8Time = Eidos_ElapsedProfileTime(sim->profile_callback_totals_[8]);
		double percentType0 = (elapsedType0Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentType1 = (elapsedType1Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentType2 = (elapsedType2Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentType3 = (elapsedType3Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentType4 = (elapsedType4Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentType5 = (elapsedType5Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentType6 = (elapsedType6Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentType7 = (elapsedType7Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentType8 = (elapsedType8Time / elapsedWallClockTimeInSLiM) * 100.0;
		int fw = 4, fw2 = 4;
		
		fw = std::max(fw, 3 + (int)ceil(log10(floor(elapsedType0Time))));
		fw = std::max(fw, 3 + (int)ceil(log10(floor(elapsedType1Time))));
		fw = std::max(fw, 3 + (int)ceil(log10(floor(elapsedType2Time))));
		fw = std::max(fw, 3 + (int)ceil(log10(floor(elapsedType3Time))));
		fw = std::max(fw, 3 + (int)ceil(log10(floor(elapsedType4Time))));
		fw = std::max(fw, 3 + (int)ceil(log10(floor(elapsedType5Time))));
		fw = std::max(fw, 3 + (int)ceil(log10(floor(elapsedType6Time))));
		fw = std::max(fw, 3 + (int)ceil(log10(floor(elapsedType7Time))));
		fw = std::max(fw, 3 + (int)ceil(log10(floor(elapsedType8Time))));
		
		fw2 = std::max(fw2, 3 + (int)ceil(log10(floor(percentType0))));
		fw2 = std::max(fw2, 3 + (int)ceil(log10(floor(percentType1))));
		fw2 = std::max(fw2, 3 + (int)ceil(log10(floor(percentType2))));
		fw2 = std::max(fw2, 3 + (int)ceil(log10(floor(percentType3))));
		fw2 = std::max(fw2, 3 + (int)ceil(log10(floor(percentType4))));
		fw2 = std::max(fw2, 3 + (int)ceil(log10(floor(percentType5))));
		fw2 = std::max(fw2, 3 + (int)ceil(log10(floor(percentType6))));
		fw2 = std::max(fw2, 3 + (int)ceil(log10(floor(percentType7))));
		fw2 = std::max(fw2, 3 + (int)ceil(log10(floor(percentType8))));
		
		[content eidosAppendString:@"\n" attributes:optima13_d];
		[content eidosAppendString:@"Callback type breakdown\n" attributes:optima14b_d];
		[content eidosAppendString:@"\n" attributes:optima3_d];
		
		// Note these are out of numeric order, but in generation-cycle order
		[content eidosAppendString:[NSString stringWithFormat:@"%*.2f s (%*.2f%%)", fw, elapsedType2Time, fw2, percentType2] attributes:menlo11_d];
		[content eidosAppendString:@" : initialize() callbacks\n" attributes:optima13_d];
		
		[content eidosAppendString:[NSString stringWithFormat:@"%*.2f s (%*.2f%%)", fw, elapsedType0Time, fw2, percentType0] attributes:menlo11_d];
		[content eidosAppendString:@" : early() events\n" attributes:optima13_d];
		
		[content eidosAppendString:[NSString stringWithFormat:@"%*.2f s (%*.2f%%)", fw, elapsedType6Time, fw2, percentType6] attributes:menlo11_d];
		[content eidosAppendString:@" : mateChoice() callbacks\n" attributes:optima13_d];
		
		[content eidosAppendString:[NSString stringWithFormat:@"%*.2f s (%*.2f%%)", fw, elapsedType8Time, fw2, percentType8] attributes:menlo11_d];
		[content eidosAppendString:@" : recombination() callbacks\n" attributes:optima13_d];
		
		[content eidosAppendString:[NSString stringWithFormat:@"%*.2f s (%*.2f%%)", fw, elapsedType7Time, fw2, percentType7] attributes:menlo11_d];
		[content eidosAppendString:@" : modifyChild() callbacks\n" attributes:optima13_d];
		
		[content eidosAppendString:[NSString stringWithFormat:@"%*.2f s (%*.2f%%)", fw, elapsedType1Time, fw2, percentType1] attributes:menlo11_d];
		[content eidosAppendString:@" : late() events\n" attributes:optima13_d];
		
		[content eidosAppendString:[NSString stringWithFormat:@"%*.2f s (%*.2f%%)", fw, elapsedType3Time, fw2, percentType3] attributes:menlo11_d];
		[content eidosAppendString:@" : fitness() callbacks\n" attributes:optima13_d];
		
		[content eidosAppendString:[NSString stringWithFormat:@"%*.2f s (%*.2f%%)", fw, elapsedType4Time, fw2, percentType4] attributes:menlo11_d];
		[content eidosAppendString:@" : fitness() callbacks (global)\n" attributes:optima13_d];
		
		[content eidosAppendString:[NSString stringWithFormat:@"%*.2f s (%*.2f%%)", fw, elapsedType5Time, fw2, percentType5] attributes:menlo11_d];
		[content eidosAppendString:@" : interaction() callbacks\n" attributes:optima13_d];
	}
	
	//
	//	Script block profiles
	//
	if (elapsedWallClockTimeInSLiM > 0.0)
	{
		{
			std::vector<SLiMEidosBlock*> &script_blocks = sim->AllScriptBlocks();
			
			// Convert the profile counts in all script blocks into self counts (excluding the counts of nodes below them)
			for (SLiMEidosBlock *script_block : script_blocks)
				script_block->root_node_->ConvertProfileTotalsToSelfCounts();
		}
		{
			[content eidosAppendString:@"\n" attributes:optima13_d];
			[content eidosAppendString:@"Script block profiles (as a fraction of corrected wall clock time)\n" attributes:optima14b_d];
			[content eidosAppendString:@"\n" attributes:optima3_d];
			
			std::vector<SLiMEidosBlock*> &script_blocks = sim->AllScriptBlocks();
			bool firstBlock = true, hiddenInconsequentialBlocks = false;
			
			for (SLiMEidosBlock *script_block : script_blocks)
			{
				const EidosASTNode *profile_root = script_block->root_node_;
				double total_block_time = Eidos_ElapsedProfileTime(profile_root->TotalOfSelfCounts());	// relies on ConvertProfileTotalsToSelfCounts() being called above!
				double percent_block_time = (total_block_time / elapsedWallClockTimeInSLiM) * 100.0;
				
				if ((total_block_time >= 0.01) || (percent_block_time >= 0.01))
				{
					if (!firstBlock)
						[content eidosAppendString:@"\n\n" attributes:menlo11_d];
					firstBlock = false;
					
					const std::string &script_std_string = profile_root->token_->token_string_;
					NSString *script_string = [NSString stringWithUTF8String:script_std_string.c_str()];
					NSMutableAttributedString *scriptAttrString = [[NSMutableAttributedString alloc] initWithString:script_string attributes:menlo11_d];
					
					[self colorScript:scriptAttrString withProfileCountsFromNode:profile_root elapsedTime:elapsedWallClockTimeInSLiM baseIndex:profile_root->token_->token_UTF16_start_];
					
					[content eidosAppendString:[NSString stringWithFormat:@"%0.2f s (%0.2f%%):\n", total_block_time, percent_block_time] attributes:menlo11_d];
					[content eidosAppendString:@"\n" attributes:optima3_d];
					
					[content appendAttributedString:scriptAttrString];
				}
				else
					hiddenInconsequentialBlocks = YES;
			}
			
			if (hiddenInconsequentialBlocks)
			{
				[content eidosAppendString:@"\n" attributes:menlo11_d];
				[content eidosAppendString:@"\n" attributes:optima3_d];
				[content eidosAppendString:@"(blocks using < 0.01 s and < 0.01% of total wall clock time are not shown)" attributes:optima13i_d];
			}
		}
		{
			[content eidosAppendString:@"\n" attributes:menlo11_d];
			[content eidosAppendString:@"\n" attributes:optima13_d];
			[content eidosAppendString:@"Script block profiles (as a fraction of within-block wall clock time)\n" attributes:optima14b_d];
			[content eidosAppendString:@"\n" attributes:optima3_d];
			
			std::vector<SLiMEidosBlock*> &script_blocks = sim->AllScriptBlocks();
			bool firstBlock = true, hiddenInconsequentialBlocks = false;
			
			for (SLiMEidosBlock *script_block : script_blocks)
			{
				const EidosASTNode *profile_root = script_block->root_node_;
				double total_block_time = Eidos_ElapsedProfileTime(profile_root->TotalOfSelfCounts());	// relies on ConvertProfileTotalsToSelfCounts() being called above!
				double percent_block_time = (total_block_time / elapsedWallClockTimeInSLiM) * 100.0;
				
				if ((total_block_time >= 0.01) || (percent_block_time >= 0.01))
				{
					if (!firstBlock)
						[content eidosAppendString:@"\n\n" attributes:menlo11_d];
					firstBlock = false;
					
					const std::string &script_std_string = profile_root->token_->token_string_;
					NSString *script_string = [NSString stringWithUTF8String:script_std_string.c_str()];
					NSMutableAttributedString *scriptAttrString = [[NSMutableAttributedString alloc] initWithString:script_string attributes:menlo11_d];
					
					if (total_block_time > 0.0)
						[self colorScript:scriptAttrString withProfileCountsFromNode:profile_root elapsedTime:total_block_time baseIndex:profile_root->token_->token_UTF16_start_];
					
					[content eidosAppendString:[NSString stringWithFormat:@"%0.2f s (%0.2f%%):\n", total_block_time, percent_block_time] attributes:menlo11_d];
					[content eidosAppendString:@"\n" attributes:optima3_d];
					
					[content appendAttributedString:scriptAttrString];
				}
				else
					hiddenInconsequentialBlocks = YES;
			}
			
			if (hiddenInconsequentialBlocks)
			{
				[content eidosAppendString:@"\n" attributes:menlo11_d];
				[content eidosAppendString:@"\n" attributes:optima3_d];
				[content eidosAppendString:@"(blocks using < 0.01 s and < 0.01% of total wall clock time are not shown)" attributes:optima13i_d];
			}
		}
	}
	
#if SLIM_USE_NONNEUTRAL_CACHES
	//
	//	MutationRun metrics
	//
	{
		int64_t power_tallies[20];	// we only go up to 1024 mutruns right now, but this gives us some headroom
		int64_t power_tallies_total = (int)sim->profile_mutcount_history_.size();
		
		for (int power = 0; power < 20; ++power)
			power_tallies[power] = 0;
		
		for (int32_t count : sim->profile_mutcount_history_)
		{
			int power = (int)round(log2(count));
			
			power_tallies[power]++;
		}
		
		[content eidosAppendString:@"\n" attributes:menlo11_d];
		[content eidosAppendString:@"\n" attributes:optima13_d];
		[content eidosAppendString:@"MutationRun usage\n" attributes:optima14b_d];
		[content eidosAppendString:@"\n" attributes:optima3_d];
		
		for (int power = 0; power < 20; ++power)
		{
			if (power_tallies[power] > 0)
			{
				[content eidosAppendString:[NSString stringWithFormat:@"%6.2f%%", (power_tallies[power] / (double)power_tallies_total) * 100.0] attributes:menlo11_d];
				[content eidosAppendString:[NSString stringWithFormat:@" of generations : %d mutation runs per genome\n", (int)(round(pow(2.0, power)))] attributes:optima13_d];
			}
		}
		
		
		int64_t regime_tallies[3];
		int64_t regime_tallies_total = (int)sim->profile_nonneutral_regime_history_.size();
		
		for (int regime = 0; regime < 3; ++regime)
			regime_tallies[regime] = 0;
		
		for (int32_t regime : sim->profile_nonneutral_regime_history_)
			if ((regime >= 1) && (regime <= 3))
				regime_tallies[regime - 1]++;
			else
				regime_tallies_total--;
		
		[content eidosAppendString:@"\n" attributes:optima13_d];
		
		for (int regime = 0; regime < 3; ++regime)
		{
			[content eidosAppendString:[NSString stringWithFormat:@"%6.2f%%", (regime_tallies[regime] / (double)regime_tallies_total) * 100.0] attributes:menlo11_d];
			[content eidosAppendString:[NSString stringWithFormat:@" of generations : regime %d (%@)\n", regime + 1, (regime == 0 ? @"no fitness callbacks" : (regime == 1 ? @"constant neutral fitness callbacks only" : @"unpredictable fitness callbacks present"))] attributes:optima13_d];
		}
		
		
		[content eidosAppendString:@"\n" attributes:optima13_d];
		
		[content eidosAppendString:[NSString stringWithFormat:@"%lld", sim->profile_mutation_total_usage_] attributes:menlo11_d];
		[content eidosAppendString:@" mutations referenced, summed across all generations\n" attributes:optima13_d];
		
		[content eidosAppendString:[NSString stringWithFormat:@"%lld", sim->profile_nonneutral_mutation_total_] attributes:menlo11_d];
		[content eidosAppendString:@" mutations considered potentially nonneutral\n" attributes:optima13_d];
		
		[content eidosAppendString:[NSString stringWithFormat:@"%0.2f%%", ((sim->profile_mutation_total_usage_ - sim->profile_nonneutral_mutation_total_) / (double)sim->profile_mutation_total_usage_) * 100.0] attributes:menlo11_d];
		[content eidosAppendString:@" of mutations excluded from fitness calculations\n" attributes:optima13_d];
		
		[content eidosAppendString:[NSString stringWithFormat:@"%lld", sim->profile_max_mutation_index_] attributes:menlo11_d];
		[content eidosAppendString:@" maximum simultaneous mutations\n" attributes:optima13_d];
		
		
		[content eidosAppendString:@"\n" attributes:optima13_d];
		
		[content eidosAppendString:[NSString stringWithFormat:@"%lld", sim->profile_mutrun_total_usage_] attributes:menlo11_d];
		[content eidosAppendString:@" mutation runs referenced, summed across all generations\n" attributes:optima13_d];
		
		[content eidosAppendString:[NSString stringWithFormat:@"%lld", sim->profile_unique_mutrun_total_] attributes:menlo11_d];
		[content eidosAppendString:@" unique mutation runs maintained among those\n" attributes:optima13_d];
		
		[content eidosAppendString:[NSString stringWithFormat:@"%6.2f%%", (sim->profile_mutrun_nonneutral_recache_total_ / (double)sim->profile_unique_mutrun_total_) * 100.0] attributes:menlo11_d];
		[content eidosAppendString:@" of mutation run nonneutral caches rebuilt per generation\n" attributes:optima13_d];
		
		[content eidosAppendString:[NSString stringWithFormat:@"%6.2f%%", ((sim->profile_mutrun_total_usage_ - sim->profile_unique_mutrun_total_) / (double)sim->profile_mutrun_total_usage_) * 100.0] attributes:menlo11_d];
		[content eidosAppendString:@" of mutation runs shared among genomes" attributes:optima13_d];
	}
#endif
	
	// Set the attributed string into the profile report text view
	NSTextStorage *ts = [profileTextView textStorage];
	
	[ts beginEditing];
	[ts setAttributedString:content];
	[ts endEditing];
	
	// Show the window and give it ownership over itself
	[profileWindow makeKeyAndOrderFront:nil];
	[profileWindow retain];				// it is set to release itself when closed
	
	// We use one nib for all profile reports, so clear our outlet; the profile now lives as its own independent window
	profileWindow = nil;
	profileTextView = nil;
}

- (void)colorScript:(NSMutableAttributedString *)script withProfileCountsFromNode:(const EidosASTNode *)node elapsedTime:(double)elapsedTime baseIndex:(int32_t)baseIndex
{
	// First color the range for this node
	eidos_profile_t count = node->profile_total_;
	
	if (count > 0)
	{
		int32_t start = 0, end = 0;
		
		node->FullUTF16Range(&start, &end);
		
		start -= baseIndex;
		end -= baseIndex;
		
		NSRange range = NSMakeRange(start, end - start + 1);
		
		NSColor *backgroundColor = [self colorForTimeFraction:Eidos_ElapsedProfileTime(count) / elapsedTime];
		
		[script addAttribute:NSBackgroundColorAttributeName value:backgroundColor range:range];
	}
	
	// Then let child nodes color
	for (const EidosASTNode *child : node->children_)
		[self colorScript:script withProfileCountsFromNode:child elapsedTime:elapsedTime baseIndex:baseIndex];
}

#define SLIM_YELLOW_FRACTION 0.10
#define SLIM_SATURATION 0.75

- (NSColor *)colorForTimeFraction:(double)timeFraction
{
	if (timeFraction < SLIM_YELLOW_FRACTION)
	{
		// small fractions fall on a ramp from white (0.0) to yellow (SLIM_YELLOW_FRACTION)
		return [NSColor colorWithCalibratedHue:(1.0 / 6.0) saturation:(timeFraction / SLIM_YELLOW_FRACTION) * SLIM_SATURATION brightness:1.0 alpha:1.0];
	}
	else
	{
		// larger fractions ramp from yellow (SLIM_YELLOW_FRACTION) to red (1.0)
		return [NSColor colorWithCalibratedHue:(1.0 / 6.0) * (1.0 - (timeFraction - SLIM_YELLOW_FRACTION) / (1.0 - SLIM_YELLOW_FRACTION)) saturation:SLIM_SATURATION brightness:1.0 alpha:1.0];
	}
}

- (void)startProfiling
{
	// prepare for profiling by measuring profile block overhead and lag
	Eidos_PrepareForProfiling();
	
	// initialize counters
	profileElapsedCPUClock = 0;
	profileElapsedWallClock = 0;
	profileStartGeneration = sim->Generation();
	
	// call this first, which has the side effect of emptying out any pending profile counts
	sim->CollectSLiMguiMutationProfileInfo();
	
	// zero out profile counts for generation stages
	sim->profile_stage_totals_[(int)(SLiMGenerationStage::kStage0PreGeneration)] = 0;
	sim->profile_stage_totals_[(int)(SLiMGenerationStage::kStage1ExecuteEarlyScripts)] = 0;
	sim->profile_stage_totals_[(int)(SLiMGenerationStage::kStage2GenerateOffspring)] = 0;
	sim->profile_stage_totals_[(int)(SLiMGenerationStage::kStage3RemoveFixedMutations)] = 0;
	sim->profile_stage_totals_[(int)(SLiMGenerationStage::kStage4SwapGenerations)] = 0;
	sim->profile_stage_totals_[(int)(SLiMGenerationStage::kStage5ExecuteLateScripts)] = 0;
	sim->profile_stage_totals_[(int)(SLiMGenerationStage::kStage6CalculateFitness)] = 0;
	
	// zero out profile counts for callback types
	sim->profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosEventEarly)] = 0;
	sim->profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosEventLate)] = 0;
	sim->profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosInitializeCallback)] = 0;
	sim->profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosFitnessCallback)] = 0;
	sim->profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosFitnessGlobalCallback)] = 0;
	sim->profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosInteractionCallback)] = 0;
	sim->profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosMateChoiceCallback)] = 0;
	sim->profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosModifyChildCallback)] = 0;
	sim->profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosRecombinationCallback)] = 0;
	
	// zero out profile counts for script blocks; dynamic scripts will be zeroed on construction
	std::vector<SLiMEidosBlock*> &script_blocks = sim->AllScriptBlocks();
	
	for (SLiMEidosBlock *script_block : script_blocks)
		script_block->root_node_->ZeroProfileTotals();
	
#if SLIM_USE_NONNEUTRAL_CACHES
	// zero out mutation run metrics
	sim->profile_mutcount_history_.clear();
	sim->profile_nonneutral_regime_history_.clear();
	sim->profile_mutation_total_usage_ = 0;
	sim->profile_nonneutral_mutation_total_ = 0;
	sim->profile_mutrun_total_usage_ = 0;
	sim->profile_unique_mutrun_total_ = 0;
	sim->profile_mutrun_nonneutral_recache_total_ = 0;
	sim->profile_max_mutation_index_ = 0;
#endif
}

- (void)endProfiling
{
	[profileEndDate release];
	profileEndDate = [[NSDate date] retain];
}

#endif	// defined(SLIMGUI) && (SLIMPROFILING == 1)

- (BOOL)runSimOneGeneration
{
	[[self document] setTransient:NO]; // Since the user has taken an interest in the window, clear the document's transient status
	
	// This method should always be used when calling out to run the simulation, because it swaps the correct random number
	// generator stuff in and out bracketing the call to RunOneGeneration().  This bracketing would need to be done around
	// any other call out to the simulation that caused it to use random numbers, too, such as subsample output.
	BOOL stillRunning = YES;
	
	[self eidosConsoleWindowControllerWillExecuteScript:_consoleController];
	
	if (profilePlayOn)
	{
		// We put the wall clock measurements on the inside since we want those to be maximally accurate,
		// as profile report percentages are fractions of the total elapsed wall clock time.
		clock_t startCPUClock = clock();
		SLIM_PROFILE_BLOCK_START();
		
		stillRunning = sim->RunOneGeneration();
		
		SLIM_PROFILE_BLOCK_END(profileElapsedWallClock);
		clock_t endCPUClock = clock();
		
		profileElapsedCPUClock += (endCPUClock - startCPUClock);
	}
	else
	{
		stillRunning = sim->RunOneGeneration();
	}
	
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
	// NOTE this code is parallel to the code in _continuousProfile:
	if (!invalidSimulation)
	{
		NSDate *startDate = [NSDate date];
		double speedSliderValue = [playSpeedSlider doubleValue];
		double intervalSinceStarting = -[continuousPlayStartDate timeIntervalSinceNow];
		
		// Calculate frames per second; this equation must match the equation in playSpeedChanged:
		double maxGenerationsPerSecond = INFINITY;
		
		if (speedSliderValue < 0.99999)
			maxGenerationsPerSecond = (speedSliderValue + 0.06) * (speedSliderValue + 0.06) * (speedSliderValue + 0.06) * 839;
		
		//NSLog(@"speedSliderValue == %f, maxGenerationsPerSecond == %f", speedSliderValue, maxGenerationsPerSecond);
		
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
			
			// bounce our icon; if we are not the active app, to signal that the run is done
			[NSApp requestUserAttention:NSInformationalRequest];
		}
	}
}

- (void)_continuousProfile:(id)sender
{
	// NOTE this code is parallel to the code in _continuousPlay:
	if (!invalidSimulation)
	{
		NSDate *startDate = [NSDate date];
		
		// We keep a local version of reachedSimulationEnd, because calling setReachedSimulationEnd: every generation
		// can actually be a large drag for simulations that run extremely quickly – it can actually exceed the time
		// spent running the simulation itself!  Moral of the story, KVO is wicked slow.
		BOOL reachedEnd = reachedSimulationEnd;
		
		do
		{
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
			[self performSelector:@selector(_continuousProfile:) withObject:nil afterDelay:0 inModes:[NSArray arrayWithObject:NSRunLoopCommonModes]];
		}
		else
		{
			// stop profiling
			[self updateAfterTickFull:YES];
			[profileButton setState:NSOffState];
			[self profile:nil];
			
			// bounce our icon; if we are not the active app, to signal that the run is done
			[NSApp requestUserAttention:NSInformationalRequest];
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

- (void)placeSubview:(NSView *)topView aboveSubview:(NSView *)bottomView
{
	NSView *topSuperview = [topView superview];
	NSView *bottomSuperview = [bottomView superview];
	
	if (topSuperview == bottomSuperview)
	{
		NSMutableArray *subviews = [NSMutableArray arrayWithArray:[topSuperview subviews]];
		
		[topView retain];
		[subviews removeObject:topView];
		
		NSUInteger bottomIndex = [subviews indexOfObjectIdenticalTo:bottomView];
		
		[subviews insertObject:topView atIndex:bottomIndex + 1];
		[topView release];
		
		[topSuperview setSubviews:subviews];
	}
}

- (void)playOrProfile:(BOOL)isPlayAction
{
	BOOL isProfileAction = !isPlayAction;	// to avoid having to think in negatives
	
#ifdef DEBUG
	if (isProfileAction)
	{
		NSAlert *alert = [[NSAlert alloc] init];
		
		[alert setAlertStyle:NSWarningAlertStyle];
		[alert setMessageText:@"Release build required"];
		[alert setInformativeText:@"In order to obtain accurate timing information that is relevant to the actual runtime of a model, profiling requires that you are running a Release build of SLiMgui.  If you are running SLiMgui from within Xcode, please choose the Release build configuration from the Edit Scheme panel."];
		[alert addButtonWithTitle:@"OK"];
		[alert setShowsSuppressionButton:NO];
		
		[alert beginSheetModalForWindow:[self window] completionHandler:^(NSModalResponse returnCode) {
			[alert autorelease];
		}];
		
		return;
	}
#endif
	
#if (SLIMPROFILING == 0)
	if (isProfileAction)
	{
		NSAlert *alert = [[NSAlert alloc] init];
		
		[alert setAlertStyle:NSWarningAlertStyle];
		[alert setMessageText:@"Profiling disabled"];
		[alert setInformativeText:@"Profiling has been disabled in this build of SLiMgui.  If you are running SLiMgui from within Xcode, please change the definition of SLIMPROFILING to 1 in the SLiMgui target."];
		[alert addButtonWithTitle:@"OK"];
		[alert setShowsSuppressionButton:NO];
		
		[alert beginSheetModalForWindow:[self window] completionHandler:^(NSModalResponse returnCode) {
			[alert autorelease];
		}];
		
		return;
	}
#endif
	
	if (!continuousPlayOn)
	{
		// keep the button on; this works for the button itself automatically, but when the menu item is chosen this is needed
		if (isProfileAction)
		{
			[profileButton setState:NSOnState];
			[profileButton slimSetTintColor:[NSColor colorWithCalibratedHue:0.0 saturation:0.5 brightness:1.0 alpha:1.0]];
		}
		else
		{
			[playButton setState:NSOnState];
			[self placeSubview:playButton aboveSubview:profileButton];
		}
		
		// log information needed to track our play speed
		[continuousPlayStartDate release];
		continuousPlayStartDate = [[NSDate date] retain];
		continuousPlayGenerationsCompleted = 0;
		
		[self setContinuousPlayOn:YES];
		isProfileAction ? [self setProfilePlayOn:YES] : [self setNonProfilePlayOn:YES];
		
		// invalidate the console symbols, and don't validate them until we are done
		[_consoleController invalidateSymbolTable];
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// prepare profiling information if necessary
		if (isProfileAction)
		{
			gEidosProfilingClientCount++;
			[self startProfiling];
		}
#endif
		
		// start playing/profiling
		if (isPlayAction)
			[self performSelector:@selector(_continuousPlay:) withObject:nil afterDelay:0 inModes:[NSArray arrayWithObject:NSRunLoopCommonModes]];
		else
			[self performSelector:@selector(_continuousProfile:) withObject:nil afterDelay:0 inModes:[NSArray arrayWithObject:NSRunLoopCommonModes]];
	}
	else
	{
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// close out profiling information if necessary
		if (isProfileAction && sim && !invalidSimulation)
		{
			[self endProfiling];
			gEidosProfilingClientCount--;
		}
#endif
		
		// keep the button off; this works for the button itself automatically, but when the menu item is chosen this is needed
		if (isProfileAction)
		{
			[profileButton setState:NSOffState];
			[profileButton slimSetTintColor:nil];
		}
		else
		{
			[playButton setState:NSOffState];
			[self placeSubview:profileButton aboveSubview:playButton];
		}
		
		// stop our recurring perform request
		if (isPlayAction)
			[NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(_continuousPlay:) object:nil];
		else
			[NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(_continuousProfile:) object:nil];
		
		[self setContinuousPlayOn:NO];
		isProfileAction ? [self setProfilePlayOn:NO] : [self setNonProfilePlayOn:NO];
		
		[_consoleController validateSymbolTable];
		[self updateAfterTickFull:YES];
		
		// Work around a bug that when the simulation ends during menu tracking, menus do not update until menu tracking finishes
		if ([self reachedSimulationEnd])
			[self forceImmediateMenuUpdate];
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// If we just finished profiling, display a report
		if (isProfileAction && sim && !invalidSimulation)
			[self displayProfileResults];
#endif
	}
}

- (IBAction)play:(id)sender
{
	[self playOrProfile:YES];
}

- (IBAction)profile:(id)sender
{
	[self playOrProfile:NO];
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
			
			// bounce our icon; if we are not the active app, to signal that the run is done
			[NSApp requestUserAttention:NSInformationalRequest];
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
	[[self document] setTransient:NO]; // Since the user has taken an interest in the window, clear the document's transient status
	
	[_consoleController invalidateSymbolTable];
	[self clearOutput:nil];
	[self setScriptStringAndInitializeSimulation:[scriptTextView string]];
	[_consoleController validateSymbolTable];
	[self updateAfterTickFull:YES];
	
	// A bit of playing with undo.  We want to break undo coalescing at the point of recycling, so that undo and redo stop
	// at the moment that we recycled.  Then we reset a change counter that we use to know if we have changed relative to
	// the recycle point, so we can highlight the recycle button to show that the executing script is out of date.
	[scriptTextView breakUndoCoalescing];
	[[self document] resetSLiMChangeCount];
	
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
	[[self document] setTransient:NO]; // Since the user has taken an interest in the window, clear the document's transient status
	
	// We want our speed to be from the point when the slider changed, not from when play started
	[continuousPlayStartDate release];
	continuousPlayStartDate = [[NSDate date] retain];
	continuousPlayGenerationsCompleted = 1;		// this prevents a new generation from executing every time the slider moves a pixel
	
	// This method is called whenever playSpeedSlider changes, continuously; we want to show the chosen speed in a tooltip-ish window
	double speedSliderValue = [playSpeedSlider doubleValue];
	
	// Calculate frames per second; this equation must match the equation in _continuousPlay:
	double maxGenerationsPerSecond = INFINITY;
	
	if (speedSliderValue < 0.99999)
		maxGenerationsPerSecond = (speedSliderValue + 0.06) * (speedSliderValue + 0.06) * (speedSliderValue + 0.06) * 839;
	
	// Make a tooltip label string
	NSString *fpsString= @"∞ fps";
	
	if (!isinf(maxGenerationsPerSecond))
	{
		if (maxGenerationsPerSecond < 1.0)
			fpsString = [NSString stringWithFormat:@"%.2f fps", maxGenerationsPerSecond];
		else if (maxGenerationsPerSecond < 10.0)
			fpsString = [NSString stringWithFormat:@"%.1f fps", maxGenerationsPerSecond];
		else
			fpsString = [NSString stringWithFormat:@"%.0f fps", maxGenerationsPerSecond];
		
		//NSLog(@"fps string: %@", fpsString);
	}
	
	// Calculate the tooltip origin; this is adjusted for the metrics on OS X 10.12
	NSRect sliderRect = [playSpeedSlider alignmentRectForFrame:[playSpeedSlider frame]];
	NSPoint tipPoint = sliderRect.origin;
	
	tipPoint.x += 5 + round((sliderRect.size.width - 11.0) * speedSliderValue);
	tipPoint.y += 10;
	
	tipPoint = [[playSpeedSlider superview] convertPoint:tipPoint toView:nil];
	NSRect tipRect = NSMakeRect(tipPoint.x, tipPoint.y, 0, 0);
	tipPoint = [[playSpeedSlider window] convertRectToScreen:tipRect].origin;
	
	// Make the tooltip window, configure it, and display it
	if (!playSpeedToolTipWindow)
		playSpeedToolTipWindow = [SLiMToolTipWindow new];
	
	[playSpeedToolTipWindow setLabel:fpsString];
	[playSpeedToolTipWindow setTipPoint:tipPoint];
	[playSpeedToolTipWindow orderFront:nil];
	
	// Schedule a hide of the tooltip; this runs only once we're out of the tracking loop, conveniently
	[NSObject cancelPreviousPerformRequestsWithTarget:playSpeedToolTipWindow selector:@selector(orderOut:) object:nil];
	[playSpeedToolTipWindow performSelector:@selector(orderOut:) withObject:nil afterDelay:0.01];
}

- (IBAction)fitnessColorSliderChanged:(id)sender
{
	[[self document] setTransient:NO]; // Since the user has taken an interest in the window, clear the document's transient status
	
	fitnessColorScale = [fitnessColorSlider doubleValue];
	fitnessColorScale *= fitnessColorScale;
	
	[fitnessColorStripe setScalingFactor:fitnessColorScale];
	[fitnessColorStripe setNeedsDisplay:YES];
	
	[populationView setNeedsDisplay:YES];
}

- (IBAction)selectionColorSliderChanged:(id)sender
{
	[[self document] setTransient:NO]; // Since the user has taken an interest in the window, clear the document's transient status
	
	selectionColorScale = [selectionColorSlider doubleValue];
	selectionColorScale *= selectionColorScale;
	
	[selectionColorStripe setScalingFactor:selectionColorScale];
	[selectionColorStripe setNeedsDisplay:YES];
	
	[chromosomeZoomed setNeedsDisplayInInterior];
}

- (BOOL)checkScriptSuppressSuccessResponse:(BOOL)suppressSuccessResponse
{
	[[self document] setTransient:NO]; // Since the user has taken an interest in the window, clear the document's transient status
	
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
		catch (...)
		{
			errorDiagnostic = [[NSString stringWithUTF8String:EidosGetTrimmedRaiseMessage().c_str()] retain];
		}
	}
	
	BOOL checkDidSucceed = !errorDiagnostic;
	
	if (!checkDidSucceed || !suppressSuccessResponse)
	{
		// use our ConsoleWindowController delegate method to play the appropriate sound
		[self eidosConsoleWindowController:_consoleController checkScriptDidSucceed:checkDidSucceed];
		
		if (!checkDidSucceed)
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
			NSDictionary *errorAttrs = [NSDictionary eidosTextAttributesWithColor:[NSColor redColor] size:11.0];
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
	}
	
	[errorDiagnostic release];
	
	return checkDidSucceed;
}

- (IBAction)checkScript:(id)sender
{
	[self checkScriptSuppressSuccessResponse:NO];
}

- (IBAction)prettyprintScript:(id)sender
{
	if ([scriptTextView isEditable])
	{
		if ([self checkScriptSuppressSuccessResponse:YES])
		{
			// We know the script is syntactically correct, so we can tokenize and parse it without worries
			NSString *currentScriptString = [scriptTextView string];
			const char *cstr = [currentScriptString UTF8String];
			SLiMEidosScript script(cstr);	// SLiMEidosScript does not override Tokenize(), but it could...
			
			script.Tokenize(false, true);	// get whitespace and comment tokens
			
			// Then generate a new script string that is prettyprinted
			const std::vector<EidosToken> &tokens = script.Tokens();
			NSMutableString *pretty = [NSMutableString string];
			
			if ([EidosPrettyprinter prettyprintTokens:tokens fromScript:script intoString:pretty])
			{
				if ([scriptTextView shouldChangeTextInRange:NSMakeRange(0, [[scriptTextView string] length]) replacementString:pretty])
				{
					[scriptTextView setString:pretty];
					[scriptTextView didChangeText];
				}
				else
				{
					NSBeep();
				}
			}
			else
				NSBeep();
		}
	}
	else
	{
		NSBeep();
	}
}

- (IBAction)showScriptHelp:(id)sender
{
	[_consoleController showScriptHelp:sender];
}

- (IBAction)toggleConsoleVisibility:(id)sender
{
	[[self document] setTransient:NO]; // Since the user has taken an interest in the window, clear the document's transient status
	
	[_consoleController toggleConsoleVisibility:sender];
}

- (IBAction)toggleBrowserVisibility:(id)sender
{
	[[self document] setTransient:NO]; // Since the user has taken an interest in the window, clear the document's transient status
	
	[[_consoleController browserController] toggleBrowserVisibility:self];
}

- (IBAction)clearOutput:(id)sender
{
	[[self document] setTransient:NO]; // Since the user has taken an interest in the window, clear the document's transient status
	
	[outputTextView setString:@""];
	
	// Just in case we have any buffered output, clear the output stream
	gSLiMOut.clear();
	gSLiMOut.str("");
}

- (IBAction)dumpPopulationToOutput:(id)sender
{
	[[self document] setTransient:NO]; // Since the user has taken an interest in the window, clear the document's transient status
	
	try
	{
		// dump the population
		SLIM_OUTSTREAM << "#OUT: " << sim->generation_ << " A" << std::endl;
		sim->population_.PrintAll(SLIM_OUTSTREAM, true);	// output spatial positions if available
		
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
	catch (...)
	{
	}
}

- (IBAction)showRecombinationIntervalsButtonToggled:(id)sender
{
	[[self document] setTransient:NO]; // Since the user has taken an interest in the window, clear the document's transient status
	
	BOOL newValue = ([showRecombinationIntervalsButton state] == NSOnState);
	
	if (newValue != zoomedChromosomeShowsRateMaps)
	{
		zoomedChromosomeShowsRateMaps = newValue;
		[chromosomeZoomed setShouldDrawRateMaps:zoomedChromosomeShowsRateMaps];
		[chromosomeZoomed setNeedsDisplayInInterior];
	}
}

- (IBAction)showGenomicElementsButtonToggled:(id)sender
{
	[[self document] setTransient:NO]; // Since the user has taken an interest in the window, clear the document's transient status
	
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
	[[self document] setTransient:NO]; // Since the user has taken an interest in the window, clear the document's transient status
	
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
	[[self document] setTransient:NO]; // Since the user has taken an interest in the window, clear the document's transient status
	
	BOOL newValue = ([showFixedSubstitutionsButton state] == NSOnState);
	
	if (newValue != zoomedChromosomeShowsFixedSubstitutions)
	{
		zoomedChromosomeShowsFixedSubstitutions = newValue;
		[chromosomeZoomed setShouldDrawFixedSubstitutions:zoomedChromosomeShowsFixedSubstitutions];
		[chromosomeZoomed setNeedsDisplayInInterior];
	}
}

- (IBAction)drawerButtonToggled:(id)sender
{
	[[self document] setTransient:NO]; // Since the user has taken an interest in the window, clear the document's transient status
	
	[drawer toggle:sender];
}

/*
	// BCH 6 April 2017: I am removing the importPopulation: action entirely.  It doesn't work well in the direction SLiM is evolving in.
	// One problem is that SLiMgui allows user actions only at the end of a generation, and that is not an appropriate time point for
	// reading in a population; it comes after fitness values have already been evaluated, and there's no good way to patch that up.
	// It used to be that re-evaluating fitness was a side effect of reading in a population, but we can't do that any more, because
	// evaluating fitness increasingly depends upon other state that needs to be set up – evaluating interactions, pre-caching phenotype
	// values, etc.  Basically, if the user wants to read in a population in SLiMgui, they need to do it in their script to assure that
	// it is done properly.
 
- (IBAction)importPopulation:(id)sender
{
	[[self document] setTransient:NO]; // Since the user has taken an interest in the window, clear the document's transient status
	
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
*/

- (IBAction)exportScript:(id)sender
{
	[[self document] setTransient:NO]; // Since the user has taken an interest in the window, clear the document's transient status
	
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
	[[self document] setTransient:NO]; // Since the user has taken an interest in the window, clear the document's transient status
	
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
	[[self document] setTransient:NO]; // Since the user has taken an interest in the window, clear the document's transient status
	
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
			sim->population_.PrintAll(outstring, true);	// include spatial positions if available
			
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
	NSDictionary *outputAttrs = [NSDictionary eidosOutputAttrsWithSize:[textView displayFontSize]];
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
		std::istringstream infile([[SLiMDocument defaultScriptString] UTF8String]);
		
		SLiMSim signature_sim(infile);
		// note no sim->InitializeRNGFromSeed() here; we don't need the RNG and don't want it to log or have side effects
		
		EidosHelpController *sharedHelp = [EidosHelpController sharedController];
		
		[sharedHelp addTopicsFromRTFFile:@"SLiMHelpFunctions" underHeading:@"6. SLiM Functions" functions:signature_sim.ZeroGenerationFunctionSignatures() methods:nullptr properties:nullptr];
		[sharedHelp addTopicsFromRTFFile:@"SLiMHelpClasses" underHeading:@"7. SLiM Classes" functions:nullptr methods:signature_sim.AllMethodSignatures() properties:signature_sim.AllPropertySignatures()];
		[sharedHelp addTopicsFromRTFFile:@"SLiMHelpCallbacks" underHeading:@"8. SLiM Events and Callbacks" functions:nullptr methods:nullptr properties:nullptr];
		
		// Check for completeness of the help documentation, since it's easy to forget to add new functions/properties/methods to the doc
		[sharedHelp checkDocumentationOfFunctions:signature_sim.ZeroGenerationFunctionSignatures()];
		
		[sharedHelp checkDocumentationOfClass:gSLiM_Chromosome_Class];
		[sharedHelp checkDocumentationOfClass:gSLiM_Genome_Class];
		[sharedHelp checkDocumentationOfClass:gSLiM_GenomicElement_Class];
		[sharedHelp checkDocumentationOfClass:gSLiM_GenomicElementType_Class];
		[sharedHelp checkDocumentationOfClass:gSLiM_Individual_Class];
		[sharedHelp checkDocumentationOfClass:gSLiM_InteractionType_Class];
		[sharedHelp checkDocumentationOfClass:gSLiM_Mutation_Class];
		[sharedHelp checkDocumentationOfClass:gSLiM_MutationType_Class];
		[sharedHelp checkDocumentationOfClass:gSLiM_SLiMEidosBlock_Class];
		[sharedHelp checkDocumentationOfClass:gSLiM_SLiMSim_Class];
		[sharedHelp checkDocumentationOfClass:gSLiM_Subpopulation_Class];
		[sharedHelp checkDocumentationOfClass:gSLiM_Substitution_Class];
		
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
	return SLiMSim::FunctionMapFromBaseMap(baseFunctionMap);
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
		
		if ((first_ch == 'p') || (first_ch == 'g') || (first_ch == 'm') || (first_ch == 's') || (first_ch == 'i'))
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
	if ([clickedText isEqualToString:@"early"])			return @"early()";
	if ([clickedText isEqualToString:@"late"])			return @"late()";
	if ([clickedText isEqualToString:@"fitness"])		return @"fitness() callbacks";
	if ([clickedText isEqualToString:@"interaction"])	return @"interaction() callbacks";
	if ([clickedText isEqualToString:@"mateChoice"])	return @"mateChoice() callbacks";
	if ([clickedText isEqualToString:@"modifyChild"])	return @"modifyChild() callbacks";
	if ([clickedText isEqualToString:@"recombination"])	return @"recombination() callbacks";
	
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

- (BOOL)eidosTextView:(EidosTextView *)eidosTextView completionContextWithScriptString:(NSString *)completionScriptString selection:(NSRange)selection typeTable:(EidosTypeTable **)typeTable functionMap:(EidosFunctionMap **)functionMap callTypeTable:(EidosCallTypeTable **)callTypeTable keywords:(NSMutableArray *)keywords
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
							else if (child_string.compare(gStr_fitness) == 0)		block_type = SLiMEidosBlockType::SLiMEidosFitnessCallback;	// can't distinguish global fitness callbacks, but no need to
							else if (child_string.compare(gStr_interaction) == 0)	block_type = SLiMEidosBlockType::SLiMEidosInteractionCallback;
							else if (child_string.compare(gStr_mateChoice) == 0)	block_type = SLiMEidosBlockType::SLiMEidosMateChoiceCallback;
							else if (child_string.compare(gStr_modifyChild) == 0)	block_type = SLiMEidosBlockType::SLiMEidosModifyChildCallback;
							else if (child_string.compare(gStr_recombination) == 0)	block_type = SLiMEidosBlockType::SLiMEidosRecombinationCallback;
							
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
						
						// Do the same for the zero-generation functions, which should be defined in initialization() blocks and
						// not in other blocks; we add and remove them dynamically so they are defined as appropriate.  We ought
						// to do this for other block-specific stuff as well (like the stuff below), but it is unlikely to matter.
						if (block_type == SLiMEidosBlockType::SLiMEidosInitializeCallback)
							SLiMSim::AddZeroGenerationFunctionsToMap(*functionMap);
						else
							SLiMSim::RemoveZeroGenerationFunctionsFromMap(*functionMap);
						
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
									(*typeTable)->RemoveSymbolsOfClass(gSLiM_Subpopulation_Class);	// subpops defined upstream from us still do not exist for us
									break;
								case SLiMEidosBlockType::SLiMEidosFitnessCallback:
								case SLiMEidosBlockType::SLiMEidosFitnessGlobalCallback:
									(*typeTable)->SetTypeForSymbol(gID_mut,				EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Mutation_Class});
									(*typeTable)->SetTypeForSymbol(gID_homozygous,		EidosTypeSpecifier{kEidosValueMaskLogical, nullptr});
									(*typeTable)->SetTypeForSymbol(gID_relFitness,		EidosTypeSpecifier{kEidosValueMaskFloat, nullptr});
									(*typeTable)->SetTypeForSymbol(gID_individual,		EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Individual_Class});
									(*typeTable)->SetTypeForSymbol(gID_genome1,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Genome_Class});
									(*typeTable)->SetTypeForSymbol(gID_genome2,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Genome_Class});
									(*typeTable)->SetTypeForSymbol(gID_subpop,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Subpopulation_Class});
									break;
								case SLiMEidosBlockType::SLiMEidosInteractionCallback:
									(*typeTable)->SetTypeForSymbol(gID_distance,		EidosTypeSpecifier{kEidosValueMaskFloat, nullptr});
									(*typeTable)->SetTypeForSymbol(gID_strength,		EidosTypeSpecifier{kEidosValueMaskFloat, nullptr});
									(*typeTable)->SetTypeForSymbol(gID_receiver,		EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Individual_Class});
									(*typeTable)->SetTypeForSymbol(gID_exerter,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Individual_Class});
									(*typeTable)->SetTypeForSymbol(gID_subpop,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Subpopulation_Class});
									break;
								case SLiMEidosBlockType::SLiMEidosMateChoiceCallback:
									(*typeTable)->SetTypeForSymbol(gID_individual,		EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Individual_Class});
									(*typeTable)->SetTypeForSymbol(gID_genome1,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Genome_Class});
									(*typeTable)->SetTypeForSymbol(gID_genome2,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Genome_Class});
									(*typeTable)->SetTypeForSymbol(gID_subpop,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Subpopulation_Class});
									(*typeTable)->SetTypeForSymbol(gID_sourceSubpop,	EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Subpopulation_Class});
									(*typeTable)->SetTypeForSymbol(gEidosID_weights,	EidosTypeSpecifier{kEidosValueMaskFloat, nullptr});
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
								case SLiMEidosBlockType::SLiMEidosRecombinationCallback:
									(*typeTable)->SetTypeForSymbol(gID_individual,		EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Individual_Class});
									(*typeTable)->SetTypeForSymbol(gID_genome1,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Genome_Class});
									(*typeTable)->SetTypeForSymbol(gID_genome2,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Genome_Class});
									(*typeTable)->SetTypeForSymbol(gID_subpop,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Subpopulation_Class});
									(*typeTable)->SetTypeForSymbol(gID_breakpoints,		EidosTypeSpecifier{kEidosValueMaskInt, nullptr});
									(*typeTable)->SetTypeForSymbol(gID_gcStarts,		EidosTypeSpecifier{kEidosValueMaskInt, nullptr});
									(*typeTable)->SetTypeForSymbol(gID_gcEnds,			EidosTypeSpecifier{kEidosValueMaskInt, nullptr});
									break;
							}
							
							// Make a type interpreter and add symbols to our type table using it
							// We use SLiMTypeInterpreter because we want to pick up definitions of SLiM constants
							SLiMTypeInterpreter typeInterpreter(block_statement_root, **typeTable, **functionMap, **callTypeTable);
							
							typeInterpreter.TypeEvaluateInterpreterBlock();	// result not used
							
							return YES;
						}
						else
						{
							// This is not the block we're completing in.  We want to add symbols for any constant-defining calls
							// in this block; apart from that, this block cannot affect the completion block, due to scoping.
							
							// Make a type interpreter and add symbols to our type table using it
							// We use SLiMTypeInterpreter because we want to pick up definitions of SLiM constants
							SLiMTypeInterpreter typeInterpreter(block_statement_root, **typeTable, **functionMap, **callTypeTable, true);
							
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
		// keywords like "early", "late", "fitness", "interaction", "mateChoice", "modifyChild", and "recombination" are.
		[keywords removeAllObjects];
		[keywords addObjectsFromArray:@[@"initialize() {\n\n}\n", @"early() {\n\n}\n", @"late() {\n\n}\n", @"fitness() {\n\n}\n", @"interaction() {\n\n}\n", @"mateChoice() {\n\n}\n", @"modifyChild() {\n\n}\n", @"recombination() {\n\n}\n"]];
		
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
		//NSLog(@"SLiMWindowController window closing...");
		[closingWindow setDelegate:nil];
		
		[self cleanup];
		
		// NSWindowController takes care of the rest; we don't need to release ourselves, or ask our document to close, or anything
	}
	else if (closingWindow == graphWindowMutationFreqSpectrum)
	{
		//NSLog(@"graphWindowMutationFreqSpectrum window closing...");
		[graphWindowMutationFreqSpectrum autorelease];
		graphWindowMutationFreqSpectrum = nil;
	}
	else if (closingWindow == graphWindowMutationFreqTrajectories)
	{
		//NSLog(@"graphWindowMutationFreqTrajectories window closing...");
		[graphWindowMutationFreqTrajectories autorelease];
		graphWindowMutationFreqTrajectories = nil;
	}
	else if (closingWindow == graphWindowMutationLossTimeHistogram)
	{
		//NSLog(@"graphWindowMutationLossTimeHistogram window closing...");
		[graphWindowMutationLossTimeHistogram autorelease];
		graphWindowMutationLossTimeHistogram = nil;
	}
	else if (closingWindow == graphWindowMutationFixationTimeHistogram)
	{
		//NSLog(@"graphWindowMutationFixationTimeHistogram window closing...");
		[graphWindowMutationFixationTimeHistogram autorelease];
		graphWindowMutationFixationTimeHistogram = nil;
	}
	else if (closingWindow == graphWindowFitnessOverTime)
	{
		//NSLog(@"graphWindowFitnessOverTime window closing...");
		[graphWindowFitnessOverTime autorelease];
		graphWindowFitnessOverTime = nil;
	}
	else if (closingWindow == graphWindowPopulationVisualization)
	{
		//NSLog(@"graphWindowPopulationVisualization window closing...");
		[graphWindowPopulationVisualization autorelease];
		graphWindowPopulationVisualization = nil;
	}
	
	// If all of our subsidiary graph windows have been closed, we are effectively back at square one regarding window placement
	if (!graphWindowMutationFreqSpectrum && !graphWindowMutationFreqTrajectories && !graphWindowMutationLossTimeHistogram && !graphWindowMutationFixationTimeHistogram && !graphWindowFitnessOverTime && !graphWindowPopulationVisualization)
		openedGraphCount = 0;
}

- (void)windowDidResize:(NSNotification *)notification
{
	[[self document] setTransient:NO]; // Since the user has taken an interest in the window, clear the document's transient status
	
	NSWindow *resizingWindow = [notification object];
	NSView *contentView = [resizingWindow contentView];

	if ([contentView isKindOfClass:[GraphView class]])
		[(GraphView *)contentView graphWindowResized];
	
	if (resizingWindow == [self window])
		[self updatePopulationViewHiding];
}

- (void)windowDidMove:(NSNotification *)notification
{
	[[self document] setTransient:NO]; // Since the user has taken an interest in the window, clear the document's transient status
}


//
//	NSTextView delegate methods
//
#pragma mark -
#pragma mark NSTextView delegate

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
				callbackSig = (new EidosFunctionSignature("initialize", nullptr, kEidosValueMaskNULL));
			
			attributedSignature = [NSAttributedString eidosAttributedStringForCallSignature:callbackSig size:11.0];
		}
		else if ([signatureString hasPrefix:@"early()"])
		{
			static EidosCallSignature *callbackSig = nullptr;
			
			if (!callbackSig)
				callbackSig = (new EidosFunctionSignature("early", nullptr, kEidosValueMaskNULL));
			
			attributedSignature = [NSAttributedString eidosAttributedStringForCallSignature:callbackSig size:11.0];
		}
		else if ([signatureString hasPrefix:@"late()"])
		{
			static EidosCallSignature *callbackSig = nullptr;
			
			if (!callbackSig)
				callbackSig = (new EidosFunctionSignature("late", nullptr, kEidosValueMaskNULL));
			
			attributedSignature = [NSAttributedString eidosAttributedStringForCallSignature:callbackSig size:11.0];
		}
		else if ([signatureString hasPrefix:@"fitness()"])
		{
			static EidosCallSignature *callbackSig = nullptr;
			
			if (!callbackSig)
				callbackSig = (new EidosFunctionSignature("fitness", nullptr, kEidosValueMaskNULL))->AddObject_SN("mutationType", gSLiM_MutationType_Class)->AddObject_OS("subpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULLInvisible);
			
			attributedSignature = [NSAttributedString eidosAttributedStringForCallSignature:callbackSig size:11.0];
		}
		else if ([signatureString hasPrefix:@"interaction()"])
		{
			static EidosCallSignature *callbackSig = nullptr;
			
			if (!callbackSig)
				callbackSig = (new EidosFunctionSignature("interaction", nullptr, kEidosValueMaskNULL))->AddObject_S("interactionType", gSLiM_InteractionType_Class)->AddObject_OS("subpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULLInvisible);
			
			attributedSignature = [NSAttributedString eidosAttributedStringForCallSignature:callbackSig size:11.0];
		}
		else if ([signatureString hasPrefix:@"mateChoice()"])
		{
			static EidosCallSignature *callbackSig = nullptr;
			
			if (!callbackSig)
				callbackSig = (new EidosFunctionSignature("mateChoice", nullptr, kEidosValueMaskNULL))->AddObject_OS("subpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULLInvisible);
			
			attributedSignature = [NSAttributedString eidosAttributedStringForCallSignature:callbackSig size:11.0];
		}
		else if ([signatureString hasPrefix:@"modifyChild()"])
		{
			static EidosCallSignature *callbackSig = nullptr;
			
			if (!callbackSig)
				callbackSig = (new EidosFunctionSignature("modifyChild", nullptr, kEidosValueMaskNULL))->AddObject_OS("subpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULLInvisible);
			
			attributedSignature = [NSAttributedString eidosAttributedStringForCallSignature:callbackSig size:11.0];
		}
		else if ([signatureString hasPrefix:@"recombination()"])
		{
			static EidosCallSignature *callbackSig = nullptr;
			
			if (!callbackSig)
				callbackSig = (new EidosFunctionSignature("recombination", nullptr, kEidosValueMaskNULL))->AddObject_OS("subpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULLInvisible);
			
			attributedSignature = [NSAttributedString eidosAttributedStringForCallSignature:callbackSig size:11.0];
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
			return sim->AllScriptBlocks().size();
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
					return [self colorForGenomicElementType:genomicElementType withID:genomicElementTypeID];
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
			std::vector<SLiMEidosBlock*> &scriptBlocks = sim->AllScriptBlocks();
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
						case SLiMEidosBlockType::SLiMEidosEventEarly:				return @"early()";
						case SLiMEidosBlockType::SLiMEidosEventLate:				return @"late()";
						case SLiMEidosBlockType::SLiMEidosInitializeCallback:		return @"initialize()";
						case SLiMEidosBlockType::SLiMEidosFitnessCallback:			return @"fitness()";
						case SLiMEidosBlockType::SLiMEidosFitnessGlobalCallback:	return @"fitness()";
						case SLiMEidosBlockType::SLiMEidosInteractionCallback:		return @"interaction()";
						case SLiMEidosBlockType::SLiMEidosMateChoiceCallback:		return @"mateChoice()";
						case SLiMEidosBlockType::SLiMEidosModifyChildCallback:		return @"modifyChild()";
						case SLiMEidosBlockType::SLiMEidosRecombinationCallback:	return @"recombination()";
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
			std::vector<SLiMEidosBlock*> &scriptBlocks = sim->AllScriptBlocks();
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

// NSSplitView doesn't like delegates to implement these methods any more; it logs if you do.  We can achieve the same
// effect using constraints in the nib, which is the new way to do things, so that's what we do now.

/*
- (CGFloat)splitView:(NSSplitView *)splitView constrainMaxCoordinate:(CGFloat)proposedMax ofSubviewAt:(NSInteger)dividerIndex
{
	if (splitView == bottomSplitView)
		return proposedMax - 240;
	else if (splitView == overallSplitView)
		return proposedMax - 150;
	
	return proposedMax;
}

- (CGFloat)splitView:(NSSplitView *)splitView constrainMinCoordinate:(CGFloat)proposedMin ofSubviewAt:(NSInteger)dividerIndex
{
	if (splitView == bottomSplitView)
		return proposedMin + 240;
	else if (splitView == overallSplitView)
		return proposedMin + 262;
	
	return proposedMin;
}
*/

- (CGFloat)splitView:(NSSplitView *)splitView constrainSplitPosition:(CGFloat)proposedPosition ofSubviewAt:(NSInteger)dividerIndex
{
	if (splitView == overallSplitView)
	{
		if (fabs(proposedPosition - 262) < 20)
			proposedPosition = 262;
		if (fabs(proposedPosition - 329) < 20)
			proposedPosition = 329;
		if (fabs(proposedPosition - 394) < 20)
			proposedPosition = 394;
	}
	
	//NSLog(@"pos in constrainSplitPosition: %f", proposedPosition);
	
	return proposedPosition;
}

- (void)respondToSizeChangeForSplitView:(NSSplitView *)splitView
{
	if (splitView == overallSplitView)
	{
		NSArray *subviews = [splitView arrangedSubviews];
		
		if ([subviews count] == 2)
		{
			NSView *firstSubview = [subviews objectAtIndex:0];
			CGFloat height = [firstSubview frame].size.height;
			
			// heights here are the same as positions in constrainSplitPosition:, as it turns out
			//NSLog(@"height in splitViewDidResizeSubviews: %f", height);
			//NSLog(@"other height in splitViewDidResizeSubviews: %f", [[subviews objectAtIndex:1] frame].size.height);
			
			{
				bool hideFitnessColorStrip = (height < 329);
				
				[fitnessTitleTextField setHidden:hideFitnessColorStrip];
				[fitnessColorStripe setHidden:hideFitnessColorStrip];
				[fitnessColorSlider setHidden:hideFitnessColorStrip];
			}
			
			{
				bool hideSelCoeffColorStrip = (height < 394);
				
				[selectionTitleTextField setHidden:hideSelCoeffColorStrip];
				[selectionColorStripe setHidden:hideSelCoeffColorStrip];
				[selectionColorSlider setHidden:hideSelCoeffColorStrip];
			}
		}
		
		[self updatePopulationViewHiding];
	}
}

- (void)splitViewDidResizeSubviews:(NSNotification *)notification
{
	NSSplitView *splitView = [notification object];
	
	[[self document] setTransient:NO]; // Since the user has taken an interest in the window, clear the document's transient status
	
	[self respondToSizeChangeForSplitView:splitView];
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

























































