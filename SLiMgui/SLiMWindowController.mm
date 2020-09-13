//
//  SLiMWindowController.m
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


#import "SLiMWindowController.h"
#import "SLiMDocument.h"
#import "AppDelegate.h"
#import "GraphView_MutationFrequencySpectra.h"
#import "GraphView_MutationLossTimeHistogram.h"
#import "GraphView_MutationFixationTimeHistogram.h"
#import "GraphView_FitnessOverTime.h"
#import "GraphView_PopulationVisualization.h"
#import "GraphView_MutationFrequencyTrajectory.h"
#import "SLiMHaplotypeGraphView.h"
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
#import "eidos_property_signature.h"
#import "eidos_type_interpreter.h"
#import "slim_test.h"
#import "slim_gui.h"

#include <iostream>
#include <sstream>
#include <iterator>
#include <stdexcept>
#include <sys/stat.h>


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
		colorArray[7] = [[NSColor colorWithCalibratedHue:0.00 saturation:0.0 brightness:0.8 alpha:1.0] retain];
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
		colorArray[6] = [[NSColor colorWithCalibratedHue:0.00 saturation:0.0 brightness:0.5 alpha:1.0] retain];
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
		
		// We set the working directory for new windows to ~/Desktop/, since it makes no sense for them to use the location of the app.
		// Each running simulation will track its own working directory, and the user can set it with a button in the SLiMgui window.
		// BCH 4/2/2020: Per request from PLR, we will now use the Desktop as the default directory only if we were launched by Finder
		// or equivalent; if we were launched by a shell, we will use the working directory given us by that shell.  See issue #76
		bool launchedFromShell = [(AppDelegate *)[NSApp delegate] launchedFromShell];
		
		if (launchedFromShell)
			sim_working_dir = [(AppDelegate *)[NSApp delegate] SLiMguiCurrentWorkingDirectory];
		else
			sim_working_dir = Eidos_ResolvedPath("~/Desktop");
		
		// Check that our chosen working directory actually exists; if not, use ~
		struct stat buffer;
		
		if (stat(sim_working_dir.c_str(), &buffer) != 0)
			sim_working_dir = Eidos_ResolvedPath("~");
		
		sim_requested_working_dir = sim_working_dir;	// return to the working dir on recycle unless the user overrides it
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
	if (slimgui)
	{
		delete slimgui;
		slimgui = nullptr;
	}
	
	Eidos_FreeRNG(sim_RNG);
	
	[self setInvalidSimulation:YES];
	
	[continuousPlayStartDate release];
	continuousPlayStartDate = nil;
	
#if (defined(SLIMGUI) && (SLIMPROFILING == 1))
	[profileEndDate release];
	profileEndDate = nil;
#endif
	
	[genomicElementColorRegistry release];
	genomicElementColorRegistry = nil;
	
	// All graph windows attached to this controller need to be closed, since they refer back to us;
	// closing them will come back via windowWillClose: and make them release and nil themselves
	[self sendAllLinkedViewsSelector:@selector(cleanup)];
	[self sendAllLinkedWindowsSelector:@selector(close)];
	
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
	
	[linkedWindows autorelease];
	linkedWindows = nil;
	
	// We also need to close and release our console window and its associated variable browser window.
	// We don't track the console or var browser in windowWillClose: since we want those windows to
	// continue to exist even when they are hidden, unlike graph windows.
	[[_consoleController browserController] hideWindow];
	[_consoleController hideWindow];
	[self setConsoleController:nil];
	
	// Also close and let go of our mini-graph tooltip window
	[self hideMiniGraphToolTipWindow];
	[functionGraphToolTipWindow autorelease];
	functionGraphToolTipWindow = nil;
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
	if (![[self document] changedSinceRecycle])
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
		sim = nullptr;
		slimgui = nullptr;
		
		Eidos_FreeRNG(sim_RNG);
		
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
	if (slimgui)
	{
		delete slimgui;
		slimgui = nullptr;
	}
	
	// Free the old simulation RNG and let SLiM make one for us
	Eidos_FreeRNG(sim_RNG);
	
	if (EIDOS_GSL_RNG)
		NSLog(@"gEidos_RNG already set up in startNewSimulationFromScript!");
	
	std::istringstream infile([scriptString UTF8String]);
	
	try
	{
		sim = new SLiMSim(infile);
		sim->InitializeRNGFromSeed(nullptr);
		
		// We take over the RNG instance that SLiMSim just made, since each SLiMgui window has its own RNG
		sim_RNG = gEidos_RNG;
		EIDOS_BZERO(&gEidos_RNG, sizeof(Eidos_RNG_State));
		
		// We also reset various Eidos/SLiM instance state; each SLiMgui window is independent
		sim_next_pedigree_id = 0;
		sim_next_mutation_id = 0;
		sim_suppress_warnings = false;
		
		// The current working directory was set up in -init to be ~/Desktop, and should not be reset here; if the
		// user has changed it, that change ought to stick across recycles.  So this bounces us back to the last dir chosen.
		sim_working_dir = sim_requested_working_dir;
		
		[self setReachedSimulationEnd:NO];
		[self setInvalidSimulation:NO];
		hasImported = NO;
	}
	catch (...)
	{
		if (sim)
			sim->simulation_valid_ = false;
		[self setReachedSimulationEnd:YES];
		[self checkForSimulationTermination];
	}
	
	if (sim)
	{
		// make a new SLiMgui instance to represent SLiMgui in Eidos
		slimgui = new SLiMgui(*sim, self);
		
		// set up the "slimgui" symbol for it immediately
		sim->simulation_constants_->InitializeConstantSymbolEntry(slimgui->SymbolTableEntry());
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

- (void)sendAllLinkedViewsSelector:(SEL)selector
{
	[[self graphViewForGraphWindow:graphWindowMutationFreqSpectrum] performSelector:selector];
	[[self graphViewForGraphWindow:graphWindowMutationFreqTrajectories] performSelector:selector];
	[[self graphViewForGraphWindow:graphWindowMutationLossTimeHistogram] performSelector:selector];
	[[self graphViewForGraphWindow:graphWindowMutationFixationTimeHistogram] performSelector:selector];
	[[self graphViewForGraphWindow:graphWindowFitnessOverTime] performSelector:selector];
	[[self graphViewForGraphWindow:graphWindowPopulationVisualization] performSelector:selector];
	
	for (NSWindow *window : linkedWindows)
	{
		NSView *contentView = [window contentView];
		
		if ([contentView respondsToSelector:selector])
			[contentView performSelector:selector];
	}
}

- (void)sendAllLinkedWindowsSelector:(SEL)selector
{
	[graphWindowMutationFreqSpectrum performSelector:selector];
	[graphWindowMutationFreqTrajectories performSelector:selector];
	[graphWindowMutationLossTimeHistogram performSelector:selector];
	[graphWindowMutationFixationTimeHistogram performSelector:selector];
	[graphWindowFitnessOverTime performSelector:selector];
	[graphWindowPopulationVisualization performSelector:selector];
	
	[linkedWindows makeObjectsPerformSelector:selector];
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
		
		NSTextStorage *ts = [outputTextView textStorage];
		NSUInteger tsOriginalLength = [ts length];
		NSUInteger strLength = [str length];
		
		[ts beginEditing];
		[ts replaceCharactersInRange:NSMakeRange(tsOriginalLength, 0) withString:str];
		[ts addAttribute:NSFontAttributeName value:[NSFont fontWithName:@"Menlo" size:[outputTextView displayFontSize]] range:NSMakeRange(tsOriginalLength, strLength)];
		[ts endEditing];
		
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
	BOOL canDisplayPopulationView = [populationView tileSubpopulations:selectedSubpopulations];
	
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

- (bool)modelMightBeNonWF
{
	// We don't have any really solid way to tell what the model will do until it is executed, given the dynamic nature of
	// Eidos, but this method tries to apply some heuristics to the question to provide a guess that will usually be correct.
	if (![self invalidSimulation] && sim)
		if (sim->ModelType() == SLiMModelType::kModelTypeNonWF)
			return YES;
	
	NSString *string = [scriptTextView string];
	
	if ([string containsString:@"initializeSLiMModelType(\"nonWF\")"] || [string containsString:@"initializeSLiMModelType(\'nonWF\')"])
		return YES;
	
	return NO;
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
	
	// Flush any buffered output to files every full update, so that the user sees changes to the files without too much delay
	if (fullUpdate)
		Eidos_FlushFiles();
	
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
		
		if (invalid || !sim)
		{
			[subpopTableView deselectAll:nil];
		}
		else
		{
			Population &population = sim->population_;
			int subpopCount = (int)population.subpops_.size();
			auto popIter = population.subpops_.begin();
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
	
	if (invalid || sim->interaction_types_changed_)
	{
		[interactionTypeTableView reloadData];
		[interactionTypeTableView setNeedsDisplay];
		
		if (sim)
			sim->interaction_types_changed_ = false;
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
		[self sendAllLinkedViewsSelector:@selector(updateAfterTick)];
}

- (void)chromosomeSelectionChanged:(NSNotification *)note
{
	[self sendAllLinkedViewsSelector:@selector(controllerSelectionChanged)];
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
	
	// The splitview sets the size of overallTopView based upon the position of the splitter.  The overallTopView then
	// wants to manually manage the size of its subview, as a way of compensating for half-pixel frame positions that
	// mess up our OpenGL subviews.  (We have the constraints in the nib so that working in the nib has fully defined
	// constraints, even though we don't want them at runtime.)  See SLiMLayoutRoundoffView in CocoaExtra.mm.  Note
	// that a side effect of this is that the window doesn't know its minimum width, because the necessary chain of
	// constraints is broken by overallTopView; we have to set a minimum width for the window in the nib to make that
	// work.  Which seems to work, although it's an unfortunate hack.  The minimum height is governed by constraints
	// on the top-level views under the main splitview, however, and thus the minimum height set on the window in the
	// nib is ignored.
	[overallTopView removeConstraint:overallTopViewConstraint1];
	[overallTopView removeConstraint:overallTopViewConstraint2];
	[overallTopView removeConstraint:overallTopViewConstraint3];
	[overallTopView removeConstraint:overallTopViewConstraint4];
	
	NSView *subviewOfOverallTopView = [[overallTopView subviews] objectAtIndex:0];
	
	[subviewOfOverallTopView setAutoresizingMask:NSViewNotSizable];
	[subviewOfOverallTopView setTranslatesAutoresizingMaskIntoConstraints:YES];
	
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
	
	// Configure our drawer; note that the minimum height here actually forces a minimum height on our window too
	[drawer setMinContentSize:NSMakeSize(280, 450)];
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
		int subpopCount = (int)population.subpops_.size();
		auto popIter = population.subpops_.begin();
		
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
	if (sel == @selector(graphHaplotypes:))
		return (!invalidSimulation && sim && (sim->generation_ > 1) && (sim->population_.subpops_.size() > 0));	// must be past initialize() and have subpops
	
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
	if (sel == @selector(changeWorkingDirectory:))
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

- (void)scriptModNonWFError
{
	NSAlert *alert = [[NSAlert alloc] init];
	
	[alert setAlertStyle:NSCriticalAlertStyle];
	[alert setMessageText:@"Script Modification Error"];
	[alert setInformativeText:@"This type of script modification can only be applied to WF models.  This model appears to be a nonWF model, so this script modification cannot be used."];
	[alert addButtonWithTitle:@"OK"];
	
	[alert beginSheetModalForWindow:[self window] completionHandler:^(NSModalResponse returnCode) { [alert autorelease]; }];
}

- (IBAction)buttonChangeSubpopSize:(id)sender
{
	if ([self modelMightBeNonWF])
		[self scriptModNonWFError];
	else
		[ScriptMod_ChangeSubpopSize runWithController:self];
}

- (IBAction)buttonRemoveSubpop:(id)sender
{
	if ([self modelMightBeNonWF])
		[self scriptModNonWFError];
	else
		[ScriptMod_RemoveSubpop runWithController:self];
}

- (IBAction)buttonAddSubpop:(id)sender
{
	[ScriptMod_AddSubpop runWithController:self];
}

- (IBAction)buttonSplitSubpop:(id)sender
{
	if ([self modelMightBeNonWF])
		[self scriptModNonWFError];
	else
		[ScriptMod_SplitSubpop runWithController:self];
}

- (IBAction)buttonChangeMigrationRates:(id)sender
{
	if ([self modelMightBeNonWF])
		[self scriptModNonWFError];
	else
		[ScriptMod_ChangeMigration runWithController:self];
}

- (IBAction)buttonChangeSelfingRates:(id)sender
{
	if ([self modelMightBeNonWF])
		[self scriptModNonWFError];
	else
		[ScriptMod_ChangeSelfing runWithController:self];
}

- (IBAction)buttonChangeCloningRates:(id)sender
{
	if ([self modelMightBeNonWF])
		[self scriptModNonWFError];
	else
		[ScriptMod_ChangeCloning runWithController:self];
}

- (IBAction)buttonChangeSexRatio:(id)sender
{
	if ([self modelMightBeNonWF])
		[self scriptModNonWFError];
	else
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
	
	// We substitute in a GraphView subclass here, in place in place of the contentView
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

- (IBAction)graphHaplotypes:(id)sender
{
	// Run the options sheet for the haplotype plot.  If OK is pressed there, -createHaplotypePlot does the real work.
	[self runHaplotypePlotOptionsSheet];
	
	// The sequence of events involved in this is actually quite complicated, because the genome clustering work happens in
	// a background thread, which gets launched halfway through the setup of the plot window, and there is a progress panel
	// that is run in SLiMWindowController's window as a sheet even though the clutering is done in SLiMHaplotypeManager,
	// and there is a configuration sheet that runs first, and so forth.  The sequence of events involved here is:
	//
	//	- runHaplotypePlotOptionsSheet is called to get configuration options from the user
	//		* the options sheet nib is loaded and run
	//		* upon completion, -createHaplotypePlot is set up as a delayed perform
	//	- createHaplotypePlot is called
	//		* the plot window nib is loaded, creating the plot view and window
	//		- configureForDisplayWithSlimWindowController: is called on the SLiMHaplotypeGraphView
	//			* the SLiMHaplotypeManager is created by the SLiMHaplotypeGraphView
	//			- initWithClusteringMethod:... is called on the SLiMHaplotypeManager
	//				* the first stage of the haplotype analysis is done; genome references are kept
	//				- runHaplotypePlotProgressSheetWithGenomeCount: is called to start the progress sheet
	//					* the progress sheet nib is loaded
	//					* haplotypeProgressTaskCancelled is set to NO to indicate we are not cancelled
	//					* the progress sheet starts running; it will run until haplotypeProgressSheetOK: below
	//				* a new background thread is started to run finishClusteringAnalysisWithBackgroundController:
	//				* initWithClusteringMethod: then returns
	//		* after configureForDisplayWithSlimWindowController: returns, the graph view and window are remembered
	//	- finishClusteringAnalysisWithBackgroundController: is called on a background thread
	//		- haplotypeProgressTaskStarting is called to lock haplotypeProgressLock for the background thread
	//		* finishClustering... calls the backgroundController periodically to update progress counters and check for cancellation
	//		* IF CANCELLATION OCCURS DURING PROCESSING:
	//			- haplotypeProgressSheetCancel: is called on the main thread
	//				- endSheet:returnCode: is called to trigger the end of the progress sheet
	//					* haplotypeProgressTaskCancelled is set to YES to signal the background task of cancellation
	//					* the sheet completion handler spins until the background task sees haplotypeProgressTaskCancelled and drops out
	//					* the progress sheet is released and goes away
	//		* OTHERWISE (NO CANCELLATION):
	//			* the genomes are clustered
	//			* the display list is created
	//		* IN EITHER CASE:
	//			- haplotypeProgressTaskFinished is called on SLiMWindowController
	//				* haplotypeProgressLock is unlocked
	//				* the main thread is requested to perform haplotypeProgressSheetOK: (unless haplotypeProgressSheetCancel: has been called already)
	//			* the background thread terminates by dropping off the end
	//	- haplotypeProgressSheetOK: is called on the main thread
	//		- endSheet:returnCode: is called to trigger the end of the progress sheet
	//			* the progress sheet is released and goes away
	//			- configurePlotWindowWithSlimWindowController: is called
	//				* the window is sized and titled
	//			* the graph window is ordered front
	//			* the remembered graph view and window are forgotten
	//
	// Gah.  Multithreading with UI sucks.
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
	[content eidosAppendString:@"\n" attributes:optima8_d];
	
	[content eidosAppendString:[NSString stringWithFormat:@"Average generation SLiM memory use: %@\n", [NSString stringForByteCount:sim->profile_total_memory_usage_.totalMemoryUsage / sim->total_memory_tallies_]] attributes:optima13_d];
	[content eidosAppendString:[NSString stringWithFormat:@"Final generation SLiM memory use: %@\n", [NSString stringForByteCount:sim->profile_last_memory_usage_.totalMemoryUsage]] attributes:optima13_d];
	
	
	//
	//	Generation stage breakdown
	//
	if (elapsedWallClockTimeInSLiM > 0.0)
	{
		bool isWF = (sim->ModelType() == SLiMModelType::kModelTypeWF);
		double elapsedStage0Time = Eidos_ElapsedProfileTime(sim->profile_stage_totals_[0]);
		double elapsedStage1Time = Eidos_ElapsedProfileTime(sim->profile_stage_totals_[1]);
		double elapsedStage2Time = Eidos_ElapsedProfileTime(sim->profile_stage_totals_[2]);
		double elapsedStage3Time = Eidos_ElapsedProfileTime(sim->profile_stage_totals_[3]);
		double elapsedStage4Time = Eidos_ElapsedProfileTime(sim->profile_stage_totals_[4]);
		double elapsedStage5Time = Eidos_ElapsedProfileTime(sim->profile_stage_totals_[5]);
		double elapsedStage6Time = Eidos_ElapsedProfileTime(sim->profile_stage_totals_[6]);
		double elapsedStage7Time = Eidos_ElapsedProfileTime(sim->profile_stage_totals_[7]);
		double percentStage0 = (elapsedStage0Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentStage1 = (elapsedStage1Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentStage2 = (elapsedStage2Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentStage3 = (elapsedStage3Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentStage4 = (elapsedStage4Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentStage5 = (elapsedStage5Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentStage6 = (elapsedStage6Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentStage7 = (elapsedStage7Time / elapsedWallClockTimeInSLiM) * 100.0;
		int fw = 4;
		
		fw = std::max(fw, 3 + (int)ceil(log10(floor(elapsedStage0Time))));
		fw = std::max(fw, 3 + (int)ceil(log10(floor(elapsedStage1Time))));
		fw = std::max(fw, 3 + (int)ceil(log10(floor(elapsedStage2Time))));
		fw = std::max(fw, 3 + (int)ceil(log10(floor(elapsedStage3Time))));
		fw = std::max(fw, 3 + (int)ceil(log10(floor(elapsedStage4Time))));
		fw = std::max(fw, 3 + (int)ceil(log10(floor(elapsedStage5Time))));
		fw = std::max(fw, 3 + (int)ceil(log10(floor(elapsedStage6Time))));
		fw = std::max(fw, 3 + (int)ceil(log10(floor(elapsedStage7Time))));
		
		[content eidosAppendString:@"\n" attributes:optima13_d];
		[content eidosAppendString:@"Generation stage breakdown\n" attributes:optima14b_d];
		[content eidosAppendString:@"\n" attributes:optima3_d];
		
		[content eidosAppendString:[NSString stringWithFormat:@"%*.2f s (%5.2f%%)", fw, elapsedStage0Time, percentStage0] attributes:menlo11_d];
		[content eidosAppendString:@" : initialize() callback execution\n" attributes:optima13_d];
		
		[content eidosAppendString:[NSString stringWithFormat:@"%*.2f s (%5.2f%%)", fw, elapsedStage1Time, percentStage1] attributes:menlo11_d];
		[content eidosAppendString:(isWF ? @" : stage 1 – early() event execution\n" : @" : stage 1 – offspring generation\n") attributes:optima13_d];
		
		[content eidosAppendString:[NSString stringWithFormat:@"%*.2f s (%5.2f%%)", fw, elapsedStage2Time, percentStage2] attributes:menlo11_d];
		[content eidosAppendString:(isWF ? @" : stage 2 – offspring generation\n" : @" : stage 2 – early() event execution\n") attributes:optima13_d];
		
		[content eidosAppendString:[NSString stringWithFormat:@"%*.2f s (%5.2f%%)", fw, elapsedStage3Time, percentStage3] attributes:menlo11_d];
		[content eidosAppendString:(isWF ? @" : stage 3 – bookkeeping (fixed mutation removal, etc.)\n" : @" : stage 3 – fitness calculation\n") attributes:optima13_d];
		
		[content eidosAppendString:[NSString stringWithFormat:@"%*.2f s (%5.2f%%)", fw, elapsedStage4Time, percentStage4] attributes:menlo11_d];
		[content eidosAppendString:(isWF ? @" : stage 4 – generation swap\n" : @" : stage 4 – viability/survival selection\n") attributes:optima13_d];
		
		[content eidosAppendString:[NSString stringWithFormat:@"%*.2f s (%5.2f%%)", fw, elapsedStage5Time, percentStage5] attributes:menlo11_d];
		[content eidosAppendString:(isWF ? @" : stage 5 – late() event execution\n" : @" : stage 5 – bookkeeping (fixed mutation removal, etc.)\n") attributes:optima13_d];
		
		[content eidosAppendString:[NSString stringWithFormat:@"%*.2f s (%5.2f%%)", fw, elapsedStage6Time, percentStage6] attributes:menlo11_d];
		[content eidosAppendString:(isWF ? @" : stage 6 – fitness calculation\n" : @" : stage 6 – late() event execution\n") attributes:optima13_d];
		
		[content eidosAppendString:[NSString stringWithFormat:@"%*.2f s (%5.2f%%)", fw, elapsedStage7Time, percentStage7] attributes:menlo11_d];
		[content eidosAppendString:(isWF ? @" : stage 7 – tree sequence auto-simplification\n" : @" : stage 7 – tree sequence auto-simplification\n") attributes:optima13_d];
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
		double elapsedType9Time = Eidos_ElapsedProfileTime(sim->profile_callback_totals_[9]);
		double elapsedType10Time = Eidos_ElapsedProfileTime(sim->profile_callback_totals_[10]);
		double percentType0 = (elapsedType0Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentType1 = (elapsedType1Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentType2 = (elapsedType2Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentType3 = (elapsedType3Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentType4 = (elapsedType4Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentType5 = (elapsedType5Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentType6 = (elapsedType6Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentType7 = (elapsedType7Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentType8 = (elapsedType8Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentType9 = (elapsedType9Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentType10 = (elapsedType10Time / elapsedWallClockTimeInSLiM) * 100.0;
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
		fw = std::max(fw, 3 + (int)ceil(log10(floor(elapsedType9Time))));
		fw = std::max(fw, 3 + (int)ceil(log10(floor(elapsedType10Time))));
		
		fw2 = std::max(fw2, 3 + (int)ceil(log10(floor(percentType0))));
		fw2 = std::max(fw2, 3 + (int)ceil(log10(floor(percentType1))));
		fw2 = std::max(fw2, 3 + (int)ceil(log10(floor(percentType2))));
		fw2 = std::max(fw2, 3 + (int)ceil(log10(floor(percentType3))));
		fw2 = std::max(fw2, 3 + (int)ceil(log10(floor(percentType4))));
		fw2 = std::max(fw2, 3 + (int)ceil(log10(floor(percentType5))));
		fw2 = std::max(fw2, 3 + (int)ceil(log10(floor(percentType6))));
		fw2 = std::max(fw2, 3 + (int)ceil(log10(floor(percentType7))));
		fw2 = std::max(fw2, 3 + (int)ceil(log10(floor(percentType8))));
		fw2 = std::max(fw2, 3 + (int)ceil(log10(floor(percentType9))));
		fw2 = std::max(fw2, 3 + (int)ceil(log10(floor(percentType10))));
		
		[content eidosAppendString:@"\n" attributes:optima13_d];
		[content eidosAppendString:@"Callback type breakdown\n" attributes:optima14b_d];
		[content eidosAppendString:@"\n" attributes:optima3_d];
		
		// Note these are out of numeric order, but in generation-cycle order
		if (sim->ModelType() == SLiMModelType::kModelTypeWF)
		{
			[content eidosAppendString:[NSString stringWithFormat:@"%*.2f s (%*.2f%%)", fw, elapsedType2Time, fw2, percentType2] attributes:menlo11_d];
			[content eidosAppendString:@" : initialize() callbacks\n" attributes:optima13_d];
			
			[content eidosAppendString:[NSString stringWithFormat:@"%*.2f s (%*.2f%%)", fw, elapsedType0Time, fw2, percentType0] attributes:menlo11_d];
			[content eidosAppendString:@" : early() events\n" attributes:optima13_d];
			
			[content eidosAppendString:[NSString stringWithFormat:@"%*.2f s (%*.2f%%)", fw, elapsedType6Time, fw2, percentType6] attributes:menlo11_d];
			[content eidosAppendString:@" : mateChoice() callbacks\n" attributes:optima13_d];
			
			[content eidosAppendString:[NSString stringWithFormat:@"%*.2f s (%*.2f%%)", fw, elapsedType8Time, fw2, percentType8] attributes:menlo11_d];
			[content eidosAppendString:@" : recombination() callbacks\n" attributes:optima13_d];
			
			[content eidosAppendString:[NSString stringWithFormat:@"%*.2f s (%*.2f%%)", fw, elapsedType9Time, fw2, percentType9] attributes:menlo11_d];
			[content eidosAppendString:@" : mutation() callbacks\n" attributes:optima13_d];
			
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
		else
		{
			[content eidosAppendString:[NSString stringWithFormat:@"%*.2f s (%*.2f%%)", fw, elapsedType2Time, fw2, percentType2] attributes:menlo11_d];
			[content eidosAppendString:@" : initialize() callbacks\n" attributes:optima13_d];
			
			[content eidosAppendString:[NSString stringWithFormat:@"%*.2f s (%*.2f%%)", fw, elapsedType10Time, fw2, percentType10] attributes:menlo11_d];
			[content eidosAppendString:@" : reproduction() events\n" attributes:optima13_d];
			
			[content eidosAppendString:[NSString stringWithFormat:@"%*.2f s (%*.2f%%)", fw, elapsedType8Time, fw2, percentType8] attributes:menlo11_d];
			[content eidosAppendString:@" : recombination() callbacks\n" attributes:optima13_d];
			
			[content eidosAppendString:[NSString stringWithFormat:@"%*.2f s (%*.2f%%)", fw, elapsedType9Time, fw2, percentType9] attributes:menlo11_d];
			[content eidosAppendString:@" : mutation() callbacks\n" attributes:optima13_d];
			
			[content eidosAppendString:[NSString stringWithFormat:@"%*.2f s (%*.2f%%)", fw, elapsedType7Time, fw2, percentType7] attributes:menlo11_d];
			[content eidosAppendString:@" : modifyChild() callbacks\n" attributes:optima13_d];
			
			[content eidosAppendString:[NSString stringWithFormat:@"%*.2f s (%*.2f%%)", fw, elapsedType0Time, fw2, percentType0] attributes:menlo11_d];
			[content eidosAppendString:@" : early() events\n" attributes:optima13_d];
			
			[content eidosAppendString:[NSString stringWithFormat:@"%*.2f s (%*.2f%%)", fw, elapsedType3Time, fw2, percentType3] attributes:menlo11_d];
			[content eidosAppendString:@" : fitness() callbacks\n" attributes:optima13_d];
			
			[content eidosAppendString:[NSString stringWithFormat:@"%*.2f s (%*.2f%%)", fw, elapsedType4Time, fw2, percentType4] attributes:menlo11_d];
			[content eidosAppendString:@" : fitness() callbacks (global)\n" attributes:optima13_d];
			
			[content eidosAppendString:[NSString stringWithFormat:@"%*.2f s (%*.2f%%)", fw, elapsedType1Time, fw2, percentType1] attributes:menlo11_d];
			[content eidosAppendString:@" : late() events\n" attributes:optima13_d];
			
			[content eidosAppendString:[NSString stringWithFormat:@"%*.2f s (%*.2f%%)", fw, elapsedType5Time, fw2, percentType5] attributes:menlo11_d];
			[content eidosAppendString:@" : interaction() callbacks\n" attributes:optima13_d];
		}
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
				if (script_block->type_ != SLiMEidosBlockType::SLiMEidosUserDefinedFunction)		// exclude function blocks; not user-visible
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
				if (script_block->type_ == SLiMEidosBlockType::SLiMEidosUserDefinedFunction)
					continue;
				
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
				if (script_block->type_ == SLiMEidosBlockType::SLiMEidosUserDefinedFunction)
					continue;
				
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
	
	//
	//	User-defined functions (if any)
	//
	if (elapsedWallClockTimeInSLiM > 0.0)
	{
		EidosFunctionMap &function_map = sim->FunctionMap();
		std::vector<const EidosFunctionSignature *> userDefinedFunctions;
		
		for (auto functionPairIter = function_map.begin(); functionPairIter != function_map.end(); ++functionPairIter)
		{
			const EidosFunctionSignature *signature = functionPairIter->second.get();
			
			if (signature->body_script_ && signature->user_defined_)
			{
				signature->body_script_->AST()->ConvertProfileTotalsToSelfCounts();
				userDefinedFunctions.push_back(signature);
			}
		}
		
		if (userDefinedFunctions.size())
		{
			[content eidosAppendString:@"\n" attributes:menlo11_d];
			[content eidosAppendString:@"\n" attributes:optima13_d];
			[content eidosAppendString:@"User-defined functions (as a fraction of corrected wall clock time)\n" attributes:optima14b_d];
			[content eidosAppendString:@"\n" attributes:optima3_d];
			
			bool firstBlock = true, hiddenInconsequentialBlocks = false;
			
			for (const EidosFunctionSignature *signature : userDefinedFunctions)
			{
				const EidosASTNode *profile_root = signature->body_script_->AST();
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
					const std::string &&signature_string = signature->SignatureString();
					NSString *signatureString = [NSString stringWithUTF8String:signature_string.c_str()];
					
					[self colorScript:scriptAttrString withProfileCountsFromNode:profile_root elapsedTime:elapsedWallClockTimeInSLiM baseIndex:profile_root->token_->token_UTF16_start_];
					
					[content eidosAppendString:[NSString stringWithFormat:@"%0.2f s (%0.2f%%):\n", total_block_time, percent_block_time] attributes:menlo11_d];
					[content eidosAppendString:@"\n" attributes:optima3_d];
					[content eidosAppendString:[NSString stringWithFormat:@"%@\n", signatureString] attributes:menlo11_d];
					
					[content appendAttributedString:scriptAttrString];
				}
				else
					hiddenInconsequentialBlocks = YES;
			}
			
			if (hiddenInconsequentialBlocks)
			{
				[content eidosAppendString:@"\n" attributes:menlo11_d];
				[content eidosAppendString:@"\n" attributes:optima3_d];
				[content eidosAppendString:@"(functions using < 0.01 s and < 0.01% of total wall clock time are not shown)" attributes:optima13i_d];
			}
		}
		if (userDefinedFunctions.size())
		{
			[content eidosAppendString:@"\n" attributes:menlo11_d];
			[content eidosAppendString:@"\n" attributes:optima13_d];
			[content eidosAppendString:@"User-defined functions (as a fraction of within-block wall clock time)\n" attributes:optima14b_d];
			[content eidosAppendString:@"\n" attributes:optima3_d];
			
			bool firstBlock = true, hiddenInconsequentialBlocks = false;
			
			for (const EidosFunctionSignature *signature : userDefinedFunctions)
			{
				const EidosASTNode *profile_root = signature->body_script_->AST();
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
					const std::string &&signature_string = signature->SignatureString();
					NSString *signatureString = [NSString stringWithUTF8String:signature_string.c_str()];
					
					if (total_block_time > 0.0)
						[self colorScript:scriptAttrString withProfileCountsFromNode:profile_root elapsedTime:total_block_time baseIndex:profile_root->token_->token_UTF16_start_];
					
					[content eidosAppendString:[NSString stringWithFormat:@"%0.2f s (%0.2f%%):\n", total_block_time, percent_block_time] attributes:menlo11_d];
					[content eidosAppendString:@"\n" attributes:optima3_d];
					[content eidosAppendString:[NSString stringWithFormat:@"%@\n", signatureString] attributes:menlo11_d];
					
					[content appendAttributedString:scriptAttrString];
				}
				else
					hiddenInconsequentialBlocks = YES;
			}
			
			if (hiddenInconsequentialBlocks)
			{
				[content eidosAppendString:@"\n" attributes:menlo11_d];
				[content eidosAppendString:@"\n" attributes:optima3_d];
				[content eidosAppendString:@"(functions using < 0.01 s and < 0.01% of total wall clock time are not shown)" attributes:optima13i_d];
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
	
	{
		//
		//	Memory usage metrics
		//
		SLiM_MemoryUsage &mem_tot = sim->profile_total_memory_usage_;
		SLiM_MemoryUsage &mem_last = sim->profile_last_memory_usage_;
		int64_t div = sim->total_memory_tallies_;
		double ddiv = sim->total_memory_tallies_;
		double average_total = mem_tot.totalMemoryUsage / ddiv;
		double final_total = mem_last.totalMemoryUsage;
		
		[content eidosAppendString:@"\n" attributes:menlo11_d];
		[content eidosAppendString:@"\n" attributes:optima13_d];
		[content eidosAppendString:@"SLiM memory usage (average / final generation)\n" attributes:optima14b_d];
		[content eidosAppendString:@"\n" attributes:optima3_d];
		
		// Chromosome
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_tot.chromosomeObjects / div total:average_total attributes:menlo11_d]];
		[content eidosAppendString:@" / " attributes:optima13_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_last.chromosomeObjects total:final_total attributes:menlo11_d]];
		[content eidosAppendString:@" : Chromosome object\n" attributes:optima13_d];
		
		[content eidosAppendString:@"   " attributes:menlo11_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_tot.chromosomeMutationRateMaps / div total:average_total attributes:menlo11_d]];
		[content eidosAppendString:@" / " attributes:optima13_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_last.chromosomeMutationRateMaps total:final_total attributes:menlo11_d]];
		[content eidosAppendString:@" : mutation rate maps\n" attributes:optima13_d];
		
		[content eidosAppendString:@"   " attributes:menlo11_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_tot.chromosomeRecombinationRateMaps / div total:average_total attributes:menlo11_d]];
		[content eidosAppendString:@" / " attributes:optima13_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_last.chromosomeRecombinationRateMaps total:final_total attributes:menlo11_d]];
		[content eidosAppendString:@" : recombination rate maps\n" attributes:optima13_d];

		[content eidosAppendString:@"   " attributes:menlo11_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_tot.chromosomeAncestralSequence / div total:average_total attributes:menlo11_d]];
		[content eidosAppendString:@" / " attributes:optima13_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_last.chromosomeAncestralSequence total:final_total attributes:menlo11_d]];
		[content eidosAppendString:@" : ancestral nucleotides\n" attributes:optima13_d];
		
		// Genome
		[content eidosAppendString:@"\n" attributes:optima8_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_tot.genomeObjects / div total:average_total attributes:menlo11_d]];
		[content eidosAppendString:@" / " attributes:optima13_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_last.genomeObjects total:final_total attributes:menlo11_d]];
		[content eidosAppendString:[NSString stringWithFormat:@" : Genome objects (%0.2f / %lld)\n", mem_tot.genomeObjects_count / ddiv, mem_last.genomeObjects_count] attributes:optima13_d];
		
		[content eidosAppendString:@"   " attributes:menlo11_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_tot.genomeExternalBuffers / div total:average_total attributes:menlo11_d]];
		[content eidosAppendString:@" / " attributes:optima13_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_last.genomeExternalBuffers total:final_total attributes:menlo11_d]];
		[content eidosAppendString:@" : external MutationRun* buffers\n" attributes:optima13_d];
		
		[content eidosAppendString:@"   " attributes:menlo11_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_tot.genomeUnusedPoolSpace / div total:average_total attributes:menlo11_d]];
		[content eidosAppendString:@" / " attributes:optima13_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_last.genomeUnusedPoolSpace total:final_total attributes:menlo11_d]];
		[content eidosAppendString:@" : unused pool space\n" attributes:optima13_d];
		
		[content eidosAppendString:@"   " attributes:menlo11_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_tot.genomeUnusedPoolBuffers / div total:average_total attributes:menlo11_d]];
		[content eidosAppendString:@" / " attributes:optima13_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_last.genomeUnusedPoolBuffers total:final_total attributes:menlo11_d]];
		[content eidosAppendString:@" : unused pool buffers\n" attributes:optima13_d];
		
		// GenomicElement
		[content eidosAppendString:@"\n" attributes:optima8_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_tot.genomicElementObjects / div total:average_total attributes:menlo11_d]];
		[content eidosAppendString:@" / " attributes:optima13_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_last.genomicElementObjects total:final_total attributes:menlo11_d]];
		[content eidosAppendString:[NSString stringWithFormat:@" : GenomicElement objects (%0.2f / %lld)\n", mem_tot.genomicElementObjects_count / ddiv, mem_last.genomicElementObjects_count] attributes:optima13_d];
		
		// GenomicElementType
		[content eidosAppendString:@"\n" attributes:optima8_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_tot.genomicElementTypeObjects / div total:average_total attributes:menlo11_d]];
		[content eidosAppendString:@" / " attributes:optima13_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_last.genomicElementTypeObjects total:final_total attributes:menlo11_d]];
		[content eidosAppendString:[NSString stringWithFormat:@" : GenomicElementType objects (%0.2f / %lld)\n", mem_tot.genomicElementTypeObjects_count / ddiv, mem_last.genomicElementTypeObjects_count] attributes:optima13_d];
		
		// Individual
		[content eidosAppendString:@"\n" attributes:optima8_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_tot.individualObjects / div total:average_total attributes:menlo11_d]];
		[content eidosAppendString:@" / " attributes:optima13_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_last.individualObjects total:final_total attributes:menlo11_d]];
		[content eidosAppendString:[NSString stringWithFormat:@" : Individual objects (%0.2f / %lld)\n", mem_tot.individualObjects_count / ddiv, mem_last.individualObjects_count] attributes:optima13_d];
		
		[content eidosAppendString:@"   " attributes:menlo11_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_tot.individualUnusedPoolSpace / div total:average_total attributes:menlo11_d]];
		[content eidosAppendString:@" / " attributes:optima13_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_last.individualUnusedPoolSpace total:final_total attributes:menlo11_d]];
		[content eidosAppendString:@" : unused pool space\n" attributes:optima13_d];
		
		// InteractionType
		[content eidosAppendString:@"\n" attributes:optima8_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_tot.interactionTypeObjects / div total:average_total attributes:menlo11_d]];
		[content eidosAppendString:@" / " attributes:optima13_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_last.interactionTypeObjects total:final_total attributes:menlo11_d]];
		[content eidosAppendString:[NSString stringWithFormat:@" : InteractionType objects (%0.2f / %lld)\n", mem_tot.interactionTypeObjects_count / ddiv, mem_last.interactionTypeObjects_count] attributes:optima13_d];
		
		if (mem_tot.interactionTypeObjects_count || mem_last.interactionTypeObjects_count)
		{
			[content eidosAppendString:@"   " attributes:menlo11_d];
			[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_tot.interactionTypeKDTrees / div total:average_total attributes:menlo11_d]];
			[content eidosAppendString:@" / " attributes:optima13_d];
			[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_last.interactionTypeKDTrees total:final_total attributes:menlo11_d]];
			[content eidosAppendString:@" : k-d trees\n" attributes:optima13_d];
			
			[content eidosAppendString:@"   " attributes:menlo11_d];
			[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_tot.interactionTypePositionCaches / div total:average_total attributes:menlo11_d]];
			[content eidosAppendString:@" / " attributes:optima13_d];
			[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_last.interactionTypePositionCaches total:final_total attributes:menlo11_d]];
			[content eidosAppendString:@" : position caches\n" attributes:optima13_d];
			
			[content eidosAppendString:@"   " attributes:menlo11_d];
			[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_tot.interactionTypeSparseArrays / div total:average_total attributes:menlo11_d]];
			[content eidosAppendString:@" / " attributes:optima13_d];
			[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_last.interactionTypeSparseArrays total:final_total attributes:menlo11_d]];
			[content eidosAppendString:@" : sparse arrays\n" attributes:optima13_d];
		}
		
		// Mutation
		[content eidosAppendString:@"\n" attributes:optima8_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_tot.mutationObjects / div total:average_total attributes:menlo11_d]];
		[content eidosAppendString:@" / " attributes:optima13_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_last.mutationObjects total:final_total attributes:menlo11_d]];
		[content eidosAppendString:[NSString stringWithFormat:@" : Mutation objects (%0.2f / %lld)\n", mem_tot.mutationObjects_count / ddiv, mem_last.mutationObjects_count] attributes:optima13_d];
		
		[content eidosAppendString:@"   " attributes:menlo11_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_tot.mutationRefcountBuffer / div total:average_total attributes:menlo11_d]];
		[content eidosAppendString:@" / " attributes:optima13_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_last.mutationRefcountBuffer total:final_total attributes:menlo11_d]];
		[content eidosAppendString:@" : refcount buffer\n" attributes:optima13_d];
		
		[content eidosAppendString:@"   " attributes:menlo11_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_tot.mutationUnusedPoolSpace / div total:average_total attributes:menlo11_d]];
		[content eidosAppendString:@" / " attributes:optima13_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_last.mutationUnusedPoolSpace total:final_total attributes:menlo11_d]];
		[content eidosAppendString:@" : unused pool space\n" attributes:optima13_d];
		
		// MutationRun
		[content eidosAppendString:@"\n" attributes:optima8_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_tot.mutationRunObjects / div total:average_total attributes:menlo11_d]];
		[content eidosAppendString:@" / " attributes:optima13_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_last.mutationRunObjects total:final_total attributes:menlo11_d]];
		[content eidosAppendString:[NSString stringWithFormat:@" : MutationRun objects (%0.2f / %lld)\n", mem_tot.mutationRunObjects_count / ddiv, mem_last.mutationRunObjects_count] attributes:optima13_d];
		
		[content eidosAppendString:@"   " attributes:menlo11_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_tot.mutationRunExternalBuffers / div total:average_total attributes:menlo11_d]];
		[content eidosAppendString:@" / " attributes:optima13_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_last.mutationRunExternalBuffers total:final_total attributes:menlo11_d]];
		[content eidosAppendString:@" : external MutationIndex buffers\n" attributes:optima13_d];
		
		[content eidosAppendString:@"   " attributes:menlo11_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_tot.mutationRunNonneutralCaches / div total:average_total attributes:menlo11_d]];
		[content eidosAppendString:@" / " attributes:optima13_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_last.mutationRunNonneutralCaches total:final_total attributes:menlo11_d]];
		[content eidosAppendString:@" : nonneutral mutation caches\n" attributes:optima13_d];
		
		[content eidosAppendString:@"   " attributes:menlo11_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_tot.mutationRunUnusedPoolSpace / div total:average_total attributes:menlo11_d]];
		[content eidosAppendString:@" / " attributes:optima13_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_last.mutationRunUnusedPoolSpace total:final_total attributes:menlo11_d]];
		[content eidosAppendString:@" : unused pool space\n" attributes:optima13_d];
		
		[content eidosAppendString:@"   " attributes:menlo11_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_tot.mutationRunUnusedPoolBuffers / div total:average_total attributes:menlo11_d]];
		[content eidosAppendString:@" / " attributes:optima13_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_last.mutationRunUnusedPoolBuffers total:final_total attributes:menlo11_d]];
		[content eidosAppendString:@" : unused pool buffers\n" attributes:optima13_d];
		
		// MutationType
		[content eidosAppendString:@"\n" attributes:optima8_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_tot.mutationTypeObjects / div total:average_total attributes:menlo11_d]];
		[content eidosAppendString:@" / " attributes:optima13_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_last.mutationTypeObjects total:final_total attributes:menlo11_d]];
		[content eidosAppendString:[NSString stringWithFormat:@" : MutationType objects (%0.2f / %lld)\n", mem_tot.mutationTypeObjects_count / ddiv, mem_last.mutationTypeObjects_count] attributes:optima13_d];
		
		// SLiMSim
		[content eidosAppendString:@"\n" attributes:optima8_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_tot.slimsimObjects / div total:average_total attributes:menlo11_d]];
		[content eidosAppendString:@" / " attributes:optima13_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_last.slimsimObjects total:final_total attributes:menlo11_d]];
		[content eidosAppendString:@" : SLiMSim object\n" attributes:optima13_d];
		
		[content eidosAppendString:@"   " attributes:menlo11_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_tot.slimsimTreeSeqTables / div total:average_total attributes:menlo11_d]];
		[content eidosAppendString:@" / " attributes:optima13_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_last.slimsimTreeSeqTables total:final_total attributes:menlo11_d]];
		[content eidosAppendString:@" : tree-sequence tables\n" attributes:optima13_d];
		
		// Subpopulation
		[content eidosAppendString:@"\n" attributes:optima8_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_tot.subpopulationObjects / div total:average_total attributes:menlo11_d]];
		[content eidosAppendString:@" / " attributes:optima13_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_last.subpopulationObjects total:final_total attributes:menlo11_d]];
		[content eidosAppendString:[NSString stringWithFormat:@" : Subpopulation objects (%0.2f / %lld)\n", mem_tot.subpopulationObjects_count / ddiv, mem_last.subpopulationObjects_count] attributes:optima13_d];
		
		[content eidosAppendString:@"   " attributes:menlo11_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_tot.subpopulationFitnessCaches / div total:average_total attributes:menlo11_d]];
		[content eidosAppendString:@" / " attributes:optima13_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_last.subpopulationFitnessCaches total:final_total attributes:menlo11_d]];
		[content eidosAppendString:@" : fitness caches\n" attributes:optima13_d];
		
		[content eidosAppendString:@"   " attributes:menlo11_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_tot.subpopulationParentTables / div total:average_total attributes:menlo11_d]];
		[content eidosAppendString:@" / " attributes:optima13_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_last.subpopulationParentTables total:final_total attributes:menlo11_d]];
		[content eidosAppendString:@" : parent tables\n" attributes:optima13_d];
		
		[content eidosAppendString:@"   " attributes:menlo11_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_tot.subpopulationSpatialMaps / div total:average_total attributes:menlo11_d]];
		[content eidosAppendString:@" / " attributes:optima13_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_last.subpopulationSpatialMaps total:final_total attributes:menlo11_d]];
		[content eidosAppendString:@" : spatial maps\n" attributes:optima13_d];
		
		if (mem_tot.subpopulationSpatialMapsDisplay || mem_last.subpopulationSpatialMapsDisplay)
		{
			[content eidosAppendString:@"   " attributes:menlo11_d];
			[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_tot.subpopulationSpatialMapsDisplay / div total:average_total attributes:menlo11_d]];
			[content eidosAppendString:@" / " attributes:optima13_d];
			[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_last.subpopulationSpatialMapsDisplay total:final_total attributes:menlo11_d]];
			[content eidosAppendString:@" : spatial map display (SLiMgui only)\n" attributes:optima13_d];
		}
		
		// Substitution
		[content eidosAppendString:@"\n" attributes:optima8_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_tot.substitutionObjects / div total:average_total attributes:menlo11_d]];
		[content eidosAppendString:@" / " attributes:optima13_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_last.substitutionObjects total:final_total attributes:menlo11_d]];
		[content eidosAppendString:[NSString stringWithFormat:@" : Substitution objects (%0.2f / %lld)\n", mem_tot.substitutionObjects_count / ddiv, mem_last.substitutionObjects_count] attributes:optima13_d];
		
		// Eidos
		[content eidosAppendString:@"\n" attributes:optima8_d];
		[content eidosAppendString:@"Eidos:\n" attributes:optima13_d];
		
		[content eidosAppendString:@"   " attributes:menlo11_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_tot.eidosASTNodePool / div total:average_total attributes:menlo11_d]];
		[content eidosAppendString:@" / " attributes:optima13_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_last.eidosASTNodePool total:final_total attributes:menlo11_d]];
		[content eidosAppendString:@" : EidosASTNode pool\n" attributes:optima13_d];
		
		[content eidosAppendString:@"   " attributes:menlo11_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_tot.eidosSymbolTablePool / div total:average_total attributes:menlo11_d]];
		[content eidosAppendString:@" / " attributes:optima13_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_last.eidosSymbolTablePool total:final_total attributes:menlo11_d]];
		[content eidosAppendString:@" : EidosSymbolTable pool\n" attributes:optima13_d];
		
		[content eidosAppendString:@"   " attributes:menlo11_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_tot.eidosValuePool / div total:average_total attributes:menlo11_d]];
		[content eidosAppendString:@" / " attributes:optima13_d];
		[content appendAttributedString:[NSAttributedString attributedStringForByteCount:mem_last.eidosValuePool total:final_total attributes:menlo11_d]];
		[content eidosAppendString:@" : EidosValue pool" attributes:optima13_d];
	}
	
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
		
		NSColor *backgroundColor = [NSColor slimColorForFraction:Eidos_ElapsedProfileTime(count) / elapsedTime];
		
		[script addAttribute:NSBackgroundColorAttributeName value:backgroundColor range:range];
	}
	
	// Then let child nodes color
	for (const EidosASTNode *child : node->children_)
		[self colorScript:script withProfileCountsFromNode:child elapsedTime:elapsedTime baseIndex:baseIndex];
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
	for (int i = 0; i < 8; ++i)
		sim->profile_stage_totals_[i] = 0;
	
	// zero out profile counts for callback types (note SLiMEidosUserDefinedFunction is excluded; that is not a category we profile)
	sim->profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosEventEarly)] = 0;
	sim->profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosEventLate)] = 0;
	sim->profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosInitializeCallback)] = 0;
	sim->profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosFitnessCallback)] = 0;
	sim->profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosFitnessGlobalCallback)] = 0;
	sim->profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosInteractionCallback)] = 0;
	sim->profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosMateChoiceCallback)] = 0;
	sim->profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosModifyChildCallback)] = 0;
	sim->profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosRecombinationCallback)] = 0;
	sim->profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosMutationCallback)] = 0;
	sim->profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosReproductionCallback)] = 0;
	
	// zero out profile counts for script blocks; dynamic scripts will be zeroed on construction
	std::vector<SLiMEidosBlock*> &script_blocks = sim->AllScriptBlocks();
	
	for (SLiMEidosBlock *script_block : script_blocks)
		if (script_block->type_ != SLiMEidosBlockType::SLiMEidosUserDefinedFunction)	// exclude user-defined functions; not user-visible as blocks
			script_block->root_node_->ZeroProfileTotals();
	
	// zero out profile counts for all user-defined functions
	EidosFunctionMap &function_map = sim->FunctionMap();
	
	for (auto functionPairIter = function_map.begin(); functionPairIter != function_map.end(); ++functionPairIter)
	{
		const EidosFunctionSignature *signature = functionPairIter->second.get();
		
		if (signature->body_script_ && signature->user_defined_)
			signature->body_script_->AST()->ZeroProfileTotals();
	}
	
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
	
	// zero out memory usage metrics
	EIDOS_BZERO(&sim->profile_last_memory_usage_, sizeof(SLiM_MemoryUsage));
	EIDOS_BZERO(&sim->profile_total_memory_usage_, sizeof(SLiM_MemoryUsage));
	sim->total_memory_tallies_ = 0;
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
	
#if (defined(SLIMGUI) && (SLIMPROFILING == 1))
	if (profilePlayOn)
	{
		// We put the wall clock measurements on the inside since we want those to be maximally accurate,
		// as profile report percentages are fractions of the total elapsed wall clock time.
		std::clock_t startCPUClock = std::clock();
		SLIM_PROFILE_BLOCK_START();
		
		stillRunning = sim->RunOneGeneration();
		
		SLIM_PROFILE_BLOCK_END(profileElapsedWallClock);
		std::clock_t endCPUClock = std::clock();
		
		profileElapsedCPUClock += (endCPUClock - startCPUClock);
	}
	else
#endif
	{
		stillRunning = sim->RunOneGeneration();
	}
	
	[self eidosConsoleWindowControllerDidExecuteScript:_consoleController];
	
	// We also want to let graphViews know when each generation has finished, in case they need to pull data from the sim.  Note this
	// happens after every generation, not just when we are updating the UI, so drawing and setNeedsDisplay: should not happen here.
	[self sendAllLinkedViewsSelector:@selector(controllerGenerationFinished)];
	
	return stillRunning;
}

- (IBAction)playOneStep:(id)sender
{
	if (!invalidSimulation)
	{
		[_consoleController invalidateSymbolTableAndFunctionMap];
		[self setReachedSimulationEnd:![self runSimOneGeneration]];
		[_consoleController validateSymbolTableAndFunctionMap];
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
		double maxGenerationsPerSecond = 1000000000.0;	// bounded, to allow -eidos_pauseExecution to interrupt us
		
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
		
		if (!reachedEnd)
		{
			do
			{
				@autoreleasepool {
					reachedEnd = ![self runSimOneGeneration];
				}
				
				continuousPlayGenerationsCompleted++;
			}
			while (!reachedEnd && (-[startDate timeIntervalSinceNow] < 0.02));
			
			[self setReachedSimulationEnd:reachedEnd];
		}
		
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
		[profileButton setState:NSOffState];
		
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
		[profileButton setState:NSOffState];
		
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
		[_consoleController invalidateSymbolTableAndFunctionMap];
		
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
		
		[_consoleController validateSymbolTableAndFunctionMap];
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
	// FIXME would be nice to have a way to stop this prematurely, if an inccorect generation is entered or whatever... BCH 2 Nov. 2017
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
		[_consoleController invalidateSymbolTableAndFunctionMap];
		
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
		
		[_consoleController validateSymbolTableAndFunctionMap];
		
		// Work around a bug that when the simulation ends during menu tracking, menus do not update until menu tracking finishes
		if ([self reachedSimulationEnd])
			[self forceImmediateMenuUpdate];
	}
}

- (IBAction)recycle:(id)sender
{
	[[self document] setTransient:NO]; // Since the user has taken an interest in the window, clear the document's transient status
	
	[_consoleController invalidateSymbolTableAndFunctionMap];
	[self clearOutput:nil];
	[self setScriptStringAndInitializeSimulation:[scriptTextView string]];
	[_consoleController validateSymbolTableAndFunctionMap];
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

	[self sendAllLinkedViewsSelector:@selector(controllerRecycled)];
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
		playSpeedToolTipWindow = [SLiMPlaySliderToolTipWindow new];
	
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
			std::string &&error_diagnostic = Eidos_GetTrimmedRaiseMessage();
			errorDiagnostic = [[NSString stringWithUTF8String:error_diagnostic.c_str()] retain];
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
		sim->population_.PrintAll(SLIM_OUTSTREAM, true, true, false, false);	// output spatial positions and ages if available, but not ancestral sequence
		
		// dump fixed substitutions also; so the dump in SLiMgui is like outputFull() + outputFixedMutations()
		SLIM_OUTSTREAM << std::endl;
		SLIM_OUTSTREAM << "#OUT: " << sim->generation_ << " F " << std::endl;
		SLIM_OUTSTREAM << "Mutations:" << std::endl;
		
		for (unsigned int i = 0; i < sim->population_.substitutions_.size(); i++)
		{
			SLIM_OUTSTREAM << i << " ";
			sim->population_.substitutions_[i]->PrintForSLiMOutput(SLIM_OUTSTREAM);
		}
		
		// now send SLIM_OUTSTREAM to the output textview
		[self updateOutputTextView];
	}
	catch (...)
	{
	}
}

- (IBAction)changeWorkingDirectory:(id)sender
{
	[[self document] setTransient:NO]; // Since the user has taken an interest in the window, clear the document's transient status
	
	NSOpenPanel *op = [[NSOpenPanel openPanel] retain];
	
	[op setTitle:@"Choose Working Directory"];
	[op setNameFieldLabel:@"Directory:"];
	[op setMessage:@"Choose a current working directory for this model:"];
	[op setExtensionHidden:NO];
	[op setCanChooseFiles:NO];
	[op setCanChooseDirectories:YES];
	[op setCanCreateDirectories:YES];
	
	// try to make the panel start in the current working directory (not the last requested dir, the actual dir)
	std::string cwd = sim_working_dir;
	NSString *cwd_string = [NSString stringWithUTF8String:cwd.c_str()];
	NSString *expanded_path = [cwd_string stringByStandardizingPath];
	NSURL *url = [NSURL fileURLWithPath:expanded_path];
	[op setDirectoryURL:url];
	
	[op beginSheetModalForWindow:[self window] completionHandler:^(NSInteger result) {
		if (result == NSFileHandlingPanelOKButton)
		{
			NSURL *fileURL = [op URL];
			const char *filePath = [fileURL fileSystemRepresentation];
			
			sim_working_dir = filePath;
			sim_requested_working_dir = sim_working_dir;
		}
	}];
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
	
	if (invalidSimulation || continuousPlayOn || generationPlayOn || reachedSimulationEnd || hasImported || !sim || !sim->simulation_valid_ || (sim->generation_ != 1))
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
				[_consoleController validateSymbolTableAndFunctionMap];	// trigger a reload of the variable browser to show the new symbols
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
			sim->population_.PrintAll(outstring, true, true, true, false);	// include spatial positions, ages, and ancestral sequence, if available
			
			std::string &&population_dump = outstring.str();
			NSString *populationDump = [NSString stringWithUTF8String:population_dump.c_str()];
			
			[populationDump writeToURL:[sp URL] atomically:YES encoding:NSUTF8StringEncoding error:NULL];
			
			[sp autorelease];
		}
	}];
}


//
//	Eidos SLiMgui method forwards
//
#pragma mark -
#pragma mark Eidos SLiMgui method forwards

- (void)finish_eidos_pauseExecution:(id)sender
{
	// this gets called by performSelectorOnMainThread: after _continuousPlay: has broken out of its loop
	// if the simulation has already ended, or is invalid, or is not in continuous play, it does nothing
	if (!invalidSimulation && !reachedSimulationEnd && continuousPlayOn && nonProfilePlayOn && !profilePlayOn && !generationPlayOn)
	{
		[self play:nil];	// this will simulate a press of the play button to stop continuous play
		
		// bounce our icon; if we are not the active app, to signal that the run is done
		[NSApp requestUserAttention:NSInformationalRequest];
	}
}

- (void)eidos_openDocument:(NSString *)path
{
	NSURL *pathURL = [NSURL fileURLWithPath:path];
	
	[[NSDocumentController sharedDocumentController] openDocumentWithContentsOfURL:pathURL display:YES completionHandler:(^ void (NSDocument *typelessDoc, BOOL already_open, NSError *error) { })];
}

- (void)eidos_pauseExecution
{
	if (!invalidSimulation && !reachedSimulationEnd && continuousPlayOn && nonProfilePlayOn && !profilePlayOn && !generationPlayOn)
	{
		continuousPlayGenerationsCompleted = UINT64_MAX - 1;			// this will break us out of the loop in _continuousPlay: at the end of this generation
		[self performSelectorOnMainThread:@selector(finish_eidos_pauseExecution:) withObject:nil waitUntilDone:NO];	// this will actually stop continuous play
	}
}


//
//	Haplotype Plot Options Sheet methods
//
#pragma mark -
#pragma mark Haplotype Plot Options Sheet

- (void)runHaplotypePlotOptionsSheet
{
	// Nil out our outlets for a bit of safety, and then load our sheet nib
	_haplotypeOptionsSheet = nil;
	_haplotypeSampleTextField = nil;
	_haplotypeOKButton = nil;
	
	// these are the default choices in the nib
	_haplotypeSample = 0;
	_haplotypeClustering = 1;
	
	[[NSBundle mainBundle] loadNibNamed:@"SLiMHaplotypeOptionsSheet" owner:self topLevelObjects:NULL];
	
	// Run the sheet in our window
	if (_haplotypeOptionsSheet)
	{
		[_haplotypeSampleTextField setStringValue:@"1000"];
		
		[self validateHaplotypeSheetControls:nil];
		
		NSWindow *window = [self window];
		
		[window beginSheet:_haplotypeOptionsSheet completionHandler:^(NSModalResponse returnCode) {
			if (returnCode == NSAlertFirstButtonReturn)
			{
				// pull values from controls and make the plot
				[self setHaplotypeSampleSize:[_haplotypeSampleTextField intValue]];
				[self performSelector:@selector(createHaplotypePlot) withObject:nil afterDelay:0.001];
			}
			
			[_haplotypeOptionsSheet autorelease];
			_haplotypeOptionsSheet = nil;
		}];
	}
}

- (IBAction)changedHaplotypeSample:(id)sender
{
	NSMenuItem *senderMenuItem = (NSMenuItem *)sender;
	int tag = (int)[senderMenuItem tag];
	
	if (tag != _haplotypeSample)
	{
		_haplotypeSample = tag;
		[self validateHaplotypeSheetControls:nil];
	}
}

- (IBAction)changedHaplotypeClustering:(id)sender
{
	NSMenuItem *senderMenuItem = (NSMenuItem *)sender;
	int tag = (int)[senderMenuItem tag];
	
	if (tag != _haplotypeClustering)
	{
		_haplotypeClustering = tag;
		//[self validateHaplotypeSheetControls:nil];
	}
}

- (IBAction)validateHaplotypeSheetControls:(id)sender
{
	if (_haplotypeSample == 0)
	{
		[_haplotypeSampleTextField setEnabled:NO];
		[_haplotypeOKButton setEnabled:YES];
	}
	else
	{
		BOOL sampleSizeValid = [ScriptMod validIntValueInTextField:_haplotypeSampleTextField withMin:2 max:9999];
		
		[_haplotypeSampleTextField setEnabled:YES];
		[_haplotypeSampleTextField setBackgroundColor:[ScriptMod backgroundColorForValidationState:sampleSizeValid]];
		[_haplotypeOKButton setEnabled:sampleSizeValid];
	}
}

- (IBAction)haplotypeSheetOK:(id)sender
{
	[[self window] endSheet:_haplotypeOptionsSheet returnCode:NSAlertFirstButtonReturn];
}

- (IBAction)haplotypeSheetCancel:(id)sender
{
	[[self window] endSheet:_haplotypeOptionsSheet returnCode:NSAlertSecondButtonReturn];
}

- (void)controlTextDidChange:(NSNotification *)notification
{
	// NSTextField delegate method
	[self validateHaplotypeSheetControls:nil];
}

- (void)createHaplotypePlot
{
	// This roughly follows the outline of -graphWindowWithTitle:viewClass: with minor differences (no positioning,
	// and the graph view is in the nib since NSOpenGLView is much easier to use when it comes out of a nib).
	// How are we not a graph window?  There can be many of us open; we don't update as the sim changes; we don't position
	// the way graph windows do; we set our own title and size; we use an NSOpenGLView subclass (that's why we can't subclass).
	
	[[self document] setTransient:NO]; // Since the user has taken an interest in the window, clear the document's transient status
	
	[[NSBundle mainBundle] loadNibNamed:@"SLiMHaplotypeGraphWindow" owner:self topLevelObjects:NULL];
	
	// We find our SLiMHaplotypeGraphView here, inside the contentView
	NSView *contentView = [graphWindow contentView];
	
	if ([[contentView subviews] count] == 1)
	{
		NSView *subview = [[contentView subviews] objectAtIndex:0];
		
		if ([subview isKindOfClass:[SLiMHaplotypeGraphView class]])
		{
			SLiMHaplotypeGraphView *graphView = (SLiMHaplotypeGraphView *)subview;
			
			[graphView configureForDisplayWithSlimWindowController:self];
			
			// The call above will create a background thread for clustering and return quickly.  As a side
			// effect, however, it will call runHaplotypePlotProgressSheetWithGenomeCount:
			// which will show the plot window once it is ready, using newHaplotypeGraphView.
			newHaplotypeGraphView = [graphView retain];
			
			// We use one nib for all graph types, so we transfer the outlet to a separate ivar
			// This puts a retain on the window, too; if the operation is cancelled it will be removed.
			if (!linkedWindows)
				linkedWindows = [[NSMutableArray alloc] init];
			
			[linkedWindows addObject:graphWindow];
			
			return;
		}
	}
	
	NSLog(@"SLiMHaplotypeGraphWindow.xib not configured correctly!");
	graphWindow = nil;
}

- (void)runHaplotypePlotProgressSheetWithGenomeCount:(int)genome_count
{
	// Nil out our outlets for a bit of safety, and then load our sheet nib
	_haplotypeProgressSheet = nil;
	_haplotypeProgressDistances = nil;
	_haplotypeProgressClustering = nil;
	_haplotypeProgressOptimization = nil;
	_haplotypeProgressOptimizationLabel = nil;
	_haplotypeProgressNoOptConstraint = nil;
	
	[[NSBundle mainBundle] loadNibNamed:@"SLiMHaplotypeProgressSheet" owner:self topLevelObjects:NULL];
	
	// Run the sheet in our window
	if (_haplotypeProgressSheet)
	{
		// Make a lock for use by the background thread to indicate when it is done
		haplotypeProgressTaskCancelled = NO;
		if (!haplotypeProgressLock)
			haplotypeProgressLock = [[NSLock alloc] init];
		
		haplotypeProgressTaskDistances_Value = 0;
		haplotypeProgressTaskClustering_Value = 0;
		haplotypeProgressTaskOptimization_Value = 0;
		
		[_haplotypeProgressDistances setMaxValue:genome_count];
		[_haplotypeProgressClustering setMaxValue:genome_count];
		[_haplotypeProgressOptimization setMaxValue:genome_count];
		
		[_haplotypeProgressDistances setDoubleValue:0.0];
		[_haplotypeProgressClustering setDoubleValue:0.0];
		[_haplotypeProgressOptimization setDoubleValue:0.0];
		
		if (_haplotypeClustering != 2)
		{
			// If we're not doing an optimization step, hide that progress bar.  If we could just dim it,
			// that would work, but there is no setEnabled: method for NSProgressIndicator.  So instead,
			// we hide the progress bar and its label.
			[_haplotypeProgressOptimizationLabel removeFromSuperview];
			_haplotypeProgressOptimizationLabel = nil;
			
			[_haplotypeProgressOptimization removeFromSuperview];
			_haplotypeProgressOptimization = nil;
			
			// We need to adjust constraints to resize the panel.  Just adjusting the layout constraint's
			// constant does not cause the window to resize; I don't understand why, since failing to do
			// so means the window's layout constraints are violated.  Anyway, to address that we tweak
			// the sheet's frame as well.  The mysteries of autolayout.
			double layoutConstant = [_haplotypeProgressNoOptConstraint constant];
			const double newConstant = 26;
			double heightAdjust = layoutConstant - newConstant;
			NSRect frame = [_haplotypeProgressSheet frame];
			
			frame.size.height -= heightAdjust;
			
			[_haplotypeProgressNoOptConstraint setConstant:newConstant];
			[_haplotypeProgressSheet setFrame:frame display:NO];
		}
		
		NSWindow *window = [self window];
		
		[window beginSheet:_haplotypeProgressSheet completionHandler:^(NSModalResponse returnCode) {
			if (returnCode == NSAlertSecondButtonReturn)
			{
				// We got cancelled; spin until the background thread says it is done.  When it
				// unlocks this lock, we will be able to proceed here.
				haplotypeProgressTaskCancelled = YES;
				[haplotypeProgressLock lock];
				[haplotypeProgressLock unlock];
				
				// The new window was added to linkedWindows to retain it, even though it was not
				// yet visible; since the operation has been cancelled, remove it.
				[linkedWindows removeLastObject];
			}
			
			// Dismiss the progress sheet
			[_haplotypeOptionsSheet autorelease];
			_haplotypeOptionsSheet = nil;
			
			if (returnCode == NSAlertFirstButtonReturn)
			{
				// Tell the plot window to finish configuring itself
				[newHaplotypeGraphView configurePlotWindowWithSlimWindowController:self];
				
				// Now we are finally ready to show the window
				[graphWindow orderFront:nil];
				graphWindow = nil;
			}
			
			// We're done with our responsibilities toward the new graph window
			[newHaplotypeGraphView release];
			newHaplotypeGraphView = nil;
			graphWindow = nil;
		}];
	}
}

- (IBAction)haplotypeProgressSheetOK:(id)sender
{
	// We check haplotypeProgressTaskCancelled here again because now we are on the main thread and
	// there could have been a race condition; the sheet might have gotten cancelled on the main thread
	// in the time since the background thread requested that this method be called.  I think the way
	// that haplotypeProgressSheetOK: and haplotypeProgressSheetCancel: both funnel through the main
	// thread should prevent any races between OK and Cancel, with this check.
	if (!haplotypeProgressTaskCancelled)
		if (_haplotypeProgressSheet)
			[[self window] endSheet:_haplotypeProgressSheet returnCode:NSAlertFirstButtonReturn];
}

- (IBAction)haplotypeProgressSheetCancel:(id)sender
{
	if (_haplotypeProgressSheet)
		[[self window] endSheet:_haplotypeProgressSheet returnCode:NSAlertSecondButtonReturn];
}

- (void)updateProgressBars
{
	// This should always be on the main thread
	if (_haplotypeProgressSheet)
	{
		[_haplotypeProgressDistances setDoubleValue:haplotypeProgressTaskDistances_Value];
		[_haplotypeProgressClustering setDoubleValue:haplotypeProgressTaskClustering_Value];
		[_haplotypeProgressOptimization setDoubleValue:haplotypeProgressTaskOptimization_Value];
	}
}

- (void)setHaplotypeProgress:(int)progress forStage:(int)stage
{
	// This can be called on a background thread
	if (_haplotypeProgressSheet)
	{
		switch (stage)
		{
			case 0: haplotypeProgressTaskDistances_Value = progress; break;
			case 1: haplotypeProgressTaskClustering_Value = progress; break;
			case 2: haplotypeProgressTaskOptimization_Value = progress; break;
		}
	}
	
	[self performSelectorOnMainThread:@selector(updateProgressBars) withObject:nil waitUntilDone:NO];
}

- (BOOL)haplotypeProgressIsCancelled
{
	// This can be called on a background thread; this is how the background task checks for cancellation
	return haplotypeProgressTaskCancelled;
}

- (void)haplotypeProgressTaskStarting
{
	// This can be called on a background thread; this is how the background task tells us it is starting
	[haplotypeProgressLock lock];
}

- (void)haplotypeProgressTaskFinished
{
	// This can be called on a background thread; this is how the background task tells us it is done
	[haplotypeProgressLock unlock];
	
	// If the sheet was cancelled, it is already finishing up, and is just waiting for us to release the lock above.
	// If it was not cancelled, though, then we effectively press the OK button on it to dismiss it.
	if (!haplotypeProgressTaskCancelled)
		[self performSelectorOnMainThread:@selector(haplotypeProgressSheetOK:) withObject:nil waitUntilDone:NO];
}


//
//	EidosConsoleWindowControllerDelegate methods
//
#pragma mark -
#pragma mark EidosConsoleWindowControllerDelegate

- (const std::vector<EidosPropertySignature_CSP> *)slimguiAllPropertySignatures
{
	// This adds the properties belonging to the SLiMgui class to those returned by SLiMSim (which does not know about SLiMgui)
	static std::vector<EidosPropertySignature_CSP> *propertySignatures = nullptr;
	
	if (!propertySignatures)
	{
		const std::vector<EidosPropertySignature_CSP> *slimProperties =					SLiMSim::AllPropertySignatures();
		const std::vector<EidosPropertySignature_CSP> *propertiesSLiMgui =				gSLiM_SLiMgui_Class->Properties();
		
		propertySignatures = new std::vector<EidosPropertySignature_CSP>(*slimProperties);
		
		propertySignatures->insert(propertySignatures->end(), propertiesSLiMgui->begin(), propertiesSLiMgui->end());
		
		// *** From here downward this is taken verbatim from SLiMSim::AllPropertySignatures()
		// FIXME should be split into a separate method
		
		// sort by pointer; we want pointer-identical signatures to end up adjacent
		std::sort(propertySignatures->begin(), propertySignatures->end());
		
		// then unique by pointer value to get a list of unique signatures (which may not be unique by name)
		auto unique_end_iter = std::unique(propertySignatures->begin(), propertySignatures->end());
		propertySignatures->resize(static_cast<size_t>(std::distance(propertySignatures->begin(), unique_end_iter)));
		
		// print out any signatures that are identical by name
		std::sort(propertySignatures->begin(), propertySignatures->end(), CompareEidosPropertySignatures);
		
		EidosPropertySignature_CSP previous_sig = nullptr;
		
		for (EidosPropertySignature_CSP &sig : *propertySignatures)
		{
			if (previous_sig && (sig->property_name_.compare(previous_sig->property_name_) == 0))
			{
				// We have a name collision.  That is OK as long as the property signatures are identical.
				if ((sig->property_id_ != previous_sig->property_id_) ||
					(sig->read_only_ != previous_sig->read_only_) ||
					(sig->value_mask_ != previous_sig->value_mask_) ||
					(sig->value_class_ != previous_sig->value_class_))
				std::cout << "Duplicate property name with different signature: " << sig->property_name_ << std::endl;
			}
			
			previous_sig = sig;
		}
	}
	
	return propertySignatures;
}
	
- (const std::vector<EidosMethodSignature_CSP> *)slimguiAllMethodSignatures
{
	// This adds the methods belonging to the SLiMgui class to those returned by SLiMSim (which does not know about SLiMgui)
	static std::vector<EidosMethodSignature_CSP> *methodSignatures = nullptr;
	
	if (!methodSignatures)
	{
		const std::vector<EidosMethodSignature_CSP> *slimMethods =					SLiMSim::AllMethodSignatures();
		const std::vector<EidosMethodSignature_CSP> *methodsSLiMgui =				gSLiM_SLiMgui_Class->Methods();
		
		methodSignatures = new std::vector<EidosMethodSignature_CSP>(*slimMethods);
		
		methodSignatures->insert(methodSignatures->end(), methodsSLiMgui->begin(), methodsSLiMgui->end());
		
		// *** From here downward this is taken verbatim from SLiMSim::AllMethodSignatures()
		// FIXME should be split into a separate method
		
		// sort by pointer; we want pointer-identical signatures to end up adjacent
		std::sort(methodSignatures->begin(), methodSignatures->end());
		
		// then unique by pointer value to get a list of unique signatures (which may not be unique by name)
		auto unique_end_iter = std::unique(methodSignatures->begin(), methodSignatures->end());
		methodSignatures->resize(static_cast<size_t>(std::distance(methodSignatures->begin(), unique_end_iter)));
		
		// print out any signatures that are identical by name
		std::sort(methodSignatures->begin(), methodSignatures->end(), CompareEidosCallSignatures);
		
		EidosMethodSignature_CSP previous_sig = nullptr;
		
		for (EidosMethodSignature_CSP &sig : *methodSignatures)
		{
			if (previous_sig && (sig->call_name_.compare(previous_sig->call_name_) == 0))
			{
				// We have a name collision.  That is OK as long as the method signatures are identical.
				if ((typeid(*sig) != typeid(*previous_sig)) ||
					(sig->is_class_method != previous_sig->is_class_method) ||
					(sig->call_name_ != previous_sig->call_name_) ||
					(sig->return_mask_ != previous_sig->return_mask_) ||
					(sig->return_class_ != previous_sig->return_class_) ||
					(sig->arg_masks_ != previous_sig->arg_masks_) ||
					(sig->arg_names_ != previous_sig->arg_names_) ||
					(sig->arg_classes_ != previous_sig->arg_classes_) ||
					(sig->has_optional_args_ != previous_sig->has_optional_args_) ||
					(sig->has_ellipsis_ != previous_sig->has_ellipsis_))
				std::cout << "Duplicate method name with a different signature: " << sig->call_name_ << std::endl;
			}
			
			previous_sig = sig;
		}
	}
	
	return methodSignatures;
}

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
	NSAttributedString *launchString = [[NSAttributedString alloc] initWithString:[NSString stringWithFormat:@"Connected to SLiMguiLegacy simulation.\nSLiM version %@.\n", versionString] attributes:outputAttrs];	// SLIM VERSION
	NSAttributedString *dividerString = [[NSAttributedString alloc] initWithString:@"\n---------------------------------------------------------\n\n" attributes:outputAttrs];
	
	[ts beginEditing];
	[ts replaceCharactersInRange:NSMakeRange([ts length], 0) withAttributedString:launchString];
	[ts replaceCharactersInRange:NSMakeRange([ts length], 0) withAttributedString:dividerString];
	[ts endEditing];
	
	[launchString release];
	[dividerString release];
	
	// We have some one-time work that we do when the first window opens; this is here instead of
	// applicationWillFinishLaunching: because we don't want to mess up gEidos_RNG
	static BOOL beenHere = NO;
	
	if (!beenHere)
	{
		beenHere = YES;
		
		EidosHelpController *sharedHelp = [EidosHelpController sharedController];
		
		const std::vector<EidosFunctionSignature_CSP> *zg_functions = SLiMSim::ZeroGenerationFunctionSignatures();
		const std::vector<EidosFunctionSignature_CSP> *slim_functions = SLiMSim::SLiMFunctionSignatures();
		std::vector<EidosFunctionSignature_CSP> all_slim_functions;
		
		all_slim_functions.insert(all_slim_functions.end(), zg_functions->begin(), zg_functions->end());
		all_slim_functions.insert(all_slim_functions.end(), slim_functions->begin(), slim_functions->end());
		
		[sharedHelp addTopicsFromRTFFile:@"SLiMHelpFunctions" underHeading:@"6. SLiM Functions" functions:&all_slim_functions methods:nullptr properties:nullptr];
		[sharedHelp addTopicsFromRTFFile:@"SLiMHelpClasses" underHeading:@"7. SLiM Classes" functions:nullptr methods:[self slimguiAllMethodSignatures] properties:[self slimguiAllPropertySignatures]];
		[sharedHelp addTopicsFromRTFFile:@"SLiMHelpCallbacks" underHeading:@"8. SLiM Events and Callbacks" functions:nullptr methods:nullptr properties:nullptr];
		
		// Check for completeness of the help documentation, since it's easy to forget to add new functions/properties/methods to the doc
		[sharedHelp checkDocumentationOfFunctions:&all_slim_functions];
		
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
		
		[sharedHelp checkDocumentationOfClass:gSLiM_SLiMgui_Class];
		
		[sharedHelp checkDocumentationForDuplicatePointers];
		
		// Run startup tests, iff the option key is down; NOTE THAT THIS CAUSES MASSIVE LEAKING DUE TO RAISES INSIDE EIDOS!
		if ([NSEvent modifierFlags] & NSAlternateKeyMask)
		{
			// We will be executing scripts, so bracket that with our delegate method call
			[self eidosConsoleWindowControllerWillExecuteScript:_consoleController];
			
			// Run the tests
			RunSLiMTests();
			
			// Done executing scripts
			[self eidosConsoleWindowControllerDidExecuteScript:_consoleController];
		}
	}
}

- (EidosSymbolTable *)eidosConsoleWindowController:(EidosConsoleWindowController *)eidosConsoleController symbolsFromBaseSymbols:(EidosSymbolTable *)baseSymbols
{
	if (sim && !invalidSimulation)
		return sim->SymbolsFromBaseSymbols(baseSymbols);
	return baseSymbols;
}

- (EidosFunctionMap *)functionMapForEidosConsoleWindowController:(EidosConsoleWindowController *)eidosConsoleController
{
	if (sim && !invalidSimulation)
		return &sim->FunctionMap();
	return nullptr;
}

- (void)eidosConsoleWindowController:(EidosConsoleWindowController *)eidosConsoleController addOptionalFunctionsToMap:(EidosFunctionMap *)functionMap
{
	SLiMSim::AddZeroGenerationFunctionsToMap(*functionMap);
	SLiMSim::AddSLiMFunctionsToMap(*functionMap);
}

- (const std::vector<EidosMethodSignature_CSP> *)eidosConsoleWindowControllerAllMethodSignatures:(EidosConsoleWindowController *)eidosConsoleController
{
	return [self slimguiAllMethodSignatures];
}

- (EidosSyntaxHighlightType)eidosConsoleWindowController:(EidosConsoleWindowController *)eidosConsoleController tokenStringIsSpecialIdentifier:(const std::string &)token_string
{
	if (token_string.compare("sim") == 0)
		return EidosSyntaxHighlightType::kHighlightAsIdentifier;
	if (token_string.compare("slimgui") == 0)
		return EidosSyntaxHighlightType::kHighlightAsIdentifier;
	
	// Request that SLiM's callback keywords be highlighted as "context keywords", which gives them a special color
	// I'm not sure that I'm crazy about this; feels like it makes the color scheme too jumbled.
	// Leaving it out for now.  BCH 2 Nov. 2017
	/*
	if (token_string.compare("initialize") == 0)
		return EidosSyntaxHighlightType::kHighlightAsContextKeyword;
	if (token_string.compare("early") == 0)
		return EidosSyntaxHighlightType::kHighlightAsContextKeyword;
	if (token_string.compare("late") == 0)
		return EidosSyntaxHighlightType::kHighlightAsContextKeyword;
	if (token_string.compare("fitness") == 0)
		return EidosSyntaxHighlightType::kHighlightAsContextKeyword;
	if (token_string.compare("mateChoice") == 0)
		return EidosSyntaxHighlightType::kHighlightAsContextKeyword;
	if (token_string.compare("modifyChild") == 0)
		return EidosSyntaxHighlightType::kHighlightAsContextKeyword;
	if (token_string.compare("interaction") == 0)
		return EidosSyntaxHighlightType::kHighlightAsContextKeyword;
	if (token_string.compare("recombination") == 0)
		return EidosSyntaxHighlightType::kHighlightAsContextKeyword;
	if (token_string.compare("mutation") == 0)
		return EidosSyntaxHighlightType::kHighlightAsContextKeyword;
	if (token_string.compare("reproduction") == 0)
		return EidosSyntaxHighlightType::kHighlightAsContextKeyword;
	*/
	
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
					return EidosSyntaxHighlightType::kNoSyntaxHighlight;
			}
			
			return EidosSyntaxHighlightType::kHighlightAsIdentifier;
		}
	}
	
	return EidosSyntaxHighlightType::kNoSyntaxHighlight;
}

- (NSString *)eidosConsoleWindowController:(EidosConsoleWindowController *)eidosConsoleController helpTextForClickedText:(NSString *)clickedText
{
	// A few strings which, when option-clicked, should result in more targeted searches.
	// @"initialize" is deliberately omitted here so that the initialize...() methods also come up.
	if ([clickedText isEqualToString:@"early"])			return @"Eidos events";
	if ([clickedText isEqualToString:@"late"])			return @"Eidos events";
	if ([clickedText isEqualToString:@"fitness"])		return @"fitness() callbacks";
	if ([clickedText isEqualToString:@"interaction"])	return @"interaction() callbacks";
	if ([clickedText isEqualToString:@"mateChoice"])	return @"mateChoice() callbacks";
	if ([clickedText isEqualToString:@"modifyChild"])	return @"modifyChild() callbacks";
	if ([clickedText isEqualToString:@"recombination"])	return @"recombination() callbacks";
	if ([clickedText isEqualToString:@"mutation"])		return @"mutation() callbacks";
	if ([clickedText isEqualToString:@"reproduction"])	return @"reproduction() callbacks";
	
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
	if (EIDOS_GSL_RNG)
		NSLog(@"eidosConsoleWindowControllerWillExecuteScript: gEidos_rng already set up!");
	
	gEidos_RNG = sim_RNG;
	
	// We also swap in the pedigree id and mutation id counters; each SLiMgui window is independent
	gSLiM_next_pedigree_id = sim_next_pedigree_id;
	gSLiM_next_mutation_id = sim_next_mutation_id;
	gEidosSuppressWarnings = sim_suppress_warnings;
	
	// Set the current directory to its value for this window
	errno = 0;
	int retval = chdir(sim_working_dir.c_str());
	
	if (retval == -1)
		NSLog(@"eidosConsoleWindowControllerWillExecuteScript: Unable to set the working directory to %s (error %d)", sim_working_dir.c_str(), errno);
}

- (void)eidosConsoleWindowControllerDidExecuteScript:(EidosConsoleWindowController *)eidosConsoleController
{
	// Swap our random number generator back out again; see -eidosConsoleWindowControllerWillExecuteScript
	sim_RNG = gEidos_RNG;
	EIDOS_BZERO(&gEidos_RNG, sizeof(Eidos_RNG_State));
	
	// Swap out our pedigree id and mutation id counters; see -eidosConsoleWindowControllerWillExecuteScript
	// Setting to -100000 here is not necessary, but will maybe help find bugs...
	sim_next_pedigree_id = gSLiM_next_pedigree_id;
	gSLiM_next_pedigree_id = -100000;
	
	sim_next_mutation_id = gSLiM_next_mutation_id;
	gSLiM_next_mutation_id = -100000;
	
	sim_suppress_warnings = gEidosSuppressWarnings;
	gEidosSuppressWarnings = false;
	
	// Get the current working directory; each SLiM window has its own cwd, which may have been changed in script since ...WillExecuteScript:
	sim_working_dir = Eidos_CurrentDirectory();
	
	// Return to the app's working directory when not running SLiM/Eidos code
	std::string &app_cwd = [(AppDelegate *)[NSApp delegate] SLiMguiCurrentWorkingDirectory];
	errno = 0;
	int retval = chdir(app_cwd.c_str());
	
	if (retval == -1)
		NSLog(@"eidosConsoleWindowControllerDidExecuteScript: Unable to set the working directory to %s (error %d)", app_cwd.c_str(), errno);
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

- (EidosFunctionMap *)functionMapForEidosTextView:(EidosTextView *)eidosTextView
{
	return [self functionMapForEidosConsoleWindowController:nullptr];
}

- (void)eidosTextView:(EidosTextView *)eidosTextView addOptionalFunctionsToMap:(EidosFunctionMap *)functionMap
{
	[self eidosConsoleWindowController:nullptr addOptionalFunctionsToMap:functionMap];
}

- (const std::vector<EidosMethodSignature_CSP> *)eidosTextViewAllMethodSignatures:(EidosTextView *)eidosTextView;
{
	return [self eidosConsoleWindowControllerAllMethodSignatures:nullptr];
}

- (EidosSyntaxHighlightType)eidosTextView:(EidosTextView *)eidosTextView tokenStringIsSpecialIdentifier:(const std::string &)token_string;
{
	return [self eidosConsoleWindowController:nullptr tokenStringIsSpecialIdentifier:token_string];
}

- (NSString *)eidosTextView:(EidosTextView *)eidosTextView helpTextForClickedText:(NSString *)clickedText
{
	return [self eidosConsoleWindowController:nullptr helpTextForClickedText:clickedText];
}

- (BOOL)eidosTextView:(EidosTextView *)eidosTextView completionContextWithScriptString:(NSString *)completionScriptString selection:(NSRange)selection typeTable:(EidosTypeTable **)typeTable functionMap:(EidosFunctionMap **)functionMap callTypeTable:(EidosCallTypeTable **)callTypeTable keywords:(NSMutableArray *)keywords argumentNameCompletions:(std::vector<std::string> *)argNameCompletions
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
		
		// Parse an "interpreter block" bounded by an EOF rather than a "script block" that requires braces
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
		
		// Use the script text view's facility for using type-interpreting to get a "definitive" function map.  This way
		// all functions that are defined, even if below the completion point, end up in the function map.
		*functionMap = [scriptTextView functionMapForScriptString:[scriptTextView string] includingOptionalFunctions:NO];
		
		SLiMSim::AddSLiMFunctionsToMap(**functionMap);
		
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
			
			if ((int)selection.location > completion_block_end)
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
							else if (child_string.compare(gStr_mutation) == 0)		block_type = SLiMEidosBlockType::SLiMEidosMutationCallback;
							else if (child_string.compare(gStr_reproduction) == 0)	block_type = SLiMEidosBlockType::SLiMEidosReproductionCallback;
							
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
										EidosGlobalStringID constant_id = Eidos_GlobalStringIDForString(child_string);
										
										(*typeTable)->SetTypeForSymbol(constant_id, EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_SLiMEidosBlock_Class});
									}
								}
							}
						}
						else if (child_token->token_type_ == EidosTokenType::kTokenLBrace)
						{
							block_statement_root = block_child;
						}
						else if (child_token->token_type_ == EidosTokenType::kTokenFunction)
						{
							// We handle function blocks a bit differently; see below
							block_type = SLiMEidosBlockType::SLiMEidosUserDefinedFunction;
							
							if (block_child->children_.size() >= 4)
								block_statement_root = block_child->children_[3];
						}
					}
					
					// Now we know the type of the node, and the root node of its compound statement; extract what we want
					if (block_statement_root)
					{
						// The symbol sim is defined in all blocks except initialize() blocks; we need to add and remove it
						// dynamically so that each block has it defined or not defined as necessary.  Since the completion block
						// is last, the sim symbol will be correctly defined at the end of this process.
						if (block_type == SLiMEidosBlockType::SLiMEidosInitializeCallback)
							(*typeTable)->RemoveTypeForSymbol(gID_sim);
						else
							(*typeTable)->SetTypeForSymbol(gID_sim, EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_SLiMSim_Class});
						
						// The slimgui symbol is always available within a block, but not at the top level
						(*typeTable)->SetTypeForSymbol(gID_slimgui, EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_SLiMgui_Class});
						
						// Do the same for the zero-generation functions, which should be defined in initialization() blocks and
						// not in other blocks; we add and remove them dynamically so they are defined as appropriate.  We ought
						// to do this for other block-specific stuff as well (like the stuff below), but it is unlikely to matter.
						// Note that we consider the zero-gen functions to always be defined inside function blocks, since the
						// function might be called from the zero gen (we have no way of knowing definitively).
						if ((block_type == SLiMEidosBlockType::SLiMEidosInitializeCallback) || (block_type == SLiMEidosBlockType::SLiMEidosUserDefinedFunction))
							SLiMSim::AddZeroGenerationFunctionsToMap(**functionMap);
						else
							SLiMSim::RemoveZeroGenerationFunctionsFromMap(**functionMap);
						
						if (script_block_node == completion_block)
						{
							// This is the block we're actually completing in the context of; it is also the last block in the script
							// snippet that we're working with.  We want to first define any callback-associated variables for the block.
							// Note that self is not defined inside functions, even though they are SLiMEidosBlocks; we pretend we are Eidos.
							if (block_type == SLiMEidosBlockType::SLiMEidosUserDefinedFunction)
								(*typeTable)->RemoveTypeForSymbol(gID_self);
							else
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
									break;
								case SLiMEidosBlockType::SLiMEidosMutationCallback:
									(*typeTable)->SetTypeForSymbol(gID_mut,				EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Mutation_Class});
									(*typeTable)->SetTypeForSymbol(gID_parent,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Individual_Class});
									(*typeTable)->SetTypeForSymbol(gID_element,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_GenomicElement_Class});
									(*typeTable)->SetTypeForSymbol(gID_genome,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Genome_Class});
									(*typeTable)->SetTypeForSymbol(gID_subpop,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Subpopulation_Class});
									(*typeTable)->SetTypeForSymbol(gID_originalNuc,		EidosTypeSpecifier{kEidosValueMaskInt, nullptr});
									break;
								case SLiMEidosBlockType::SLiMEidosReproductionCallback:
									(*typeTable)->SetTypeForSymbol(gID_individual,		EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Individual_Class});
									(*typeTable)->SetTypeForSymbol(gID_genome1,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Genome_Class});
									(*typeTable)->SetTypeForSymbol(gID_genome2,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Genome_Class});
									(*typeTable)->SetTypeForSymbol(gID_subpop,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Subpopulation_Class});
									break;
								case SLiMEidosBlockType::SLiMEidosUserDefinedFunction:
								{
									// Similar to the local variables that are defined for callbacks above, here we need to define the parameters to the
									// function, by parsing the relevant AST nodes; this is parallel to EidosTypeInterpreter::TypeEvaluate_FunctionDecl()
									EidosASTNode *function_declaration_node = script_block_node->children_[0];
									const EidosASTNode *param_list_node = function_declaration_node->children_[2];
									const std::vector<EidosASTNode *> &param_nodes = param_list_node->children_;
									std::vector<std::string> used_param_names;
									
									for (EidosASTNode *param_node : param_nodes)
									{
										const std::vector<EidosASTNode *> &param_children = param_node->children_;
										int param_children_count = (int)param_children.size();
										
										if ((param_children_count == 2) || (param_children_count == 3))
										{
											EidosTypeSpecifier &param_type = param_children[0]->typespec_;
											const std::string &param_name = param_children[1]->token_->token_string_;
											
											// Check param_name; it needs to not be used by another parameter
											if (std::find(used_param_names.begin(), used_param_names.end(), param_name) != used_param_names.end())
												continue;
											
											if (param_children_count >= 2)
											{
												// param_node has 2 or 3 children (type, identifier, [default]); we don't care about default values
												(*typeTable)->SetTypeForSymbol(Eidos_GlobalStringIDForString(param_name), param_type);
											}
										}
									}
									break;
								}
								case SLiMEidosBlockType::SLiMEidosNoBlockType: break;	// never hit
							}
						}
						
						if (script_block_node == completion_block)
						{
							// Make a type interpreter and add symbols to our type table using it
							// We use SLiMTypeInterpreter because we want to pick up definitions of SLiM constants
							SLiMTypeInterpreter typeInterpreter(block_statement_root, **typeTable, **functionMap, **callTypeTable);
							
							typeInterpreter.TypeEvaluateInterpreterBlock_AddArgumentCompletions(argNameCompletions, script_string.length());	// result not used
							
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
		// keywords like "early", "late", "fitness", "interaction", "mateChoice", "modifyChild", "recombination", "mutation",
		// and "reproduction" are.
		[keywords removeAllObjects];
		[keywords addObjectsFromArray:@[@"initialize() {\n\n}\n", @"early() {\n\n}\n", @"late() {\n\n}\n", @"fitness() {\n\n}\n", @"interaction() {\n\n}\n", @"mateChoice() {\n\n}\n", @"modifyChild() {\n\n}\n", @"recombination() {\n\n}\n", @"mutation() {\n\n}\n", @"reproduction() {\n\n}\n", @"function (void)name(void) {\n\n}\n"]];
		
		// At the outer level, functions are also not legal
		(*functionMap)->clear();
		
		// And no variables exist except SLiM objects like pX, gX, mX, sX
		std::vector<EidosGlobalStringID> symbol_ids = (*typeTable)->AllSymbolIDs();
		
		for (EidosGlobalStringID symbol_id : symbol_ids)
			if (((*typeTable)->GetTypeForSymbol(symbol_id).type_mask != kEidosValueMaskObject) || (symbol_id == gID_sim) || (symbol_id == gID_slimgui))
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
	else if ([linkedWindows containsObject:closingWindow])
	{
		//NSLog(@"linked window window closing...");
		[linkedWindows removeObject:closingWindow];
	}
	
	// If all of our subsidiary graph windows have been closed, we are effectively back at square one regarding window placement
	if (!graphWindowMutationFreqSpectrum && !graphWindowMutationFreqTrajectories && !graphWindowMutationLossTimeHistogram && !graphWindowMutationFixationTimeHistogram && !graphWindowFitnessOverTime && !graphWindowPopulationVisualization)
		openedGraphCount = 0;
}

- (void)windowDidResize:(NSNotification *)notification
{
	//[[self document] setTransient:NO]; // Since the user has taken an interest in the window, clear the document's transient status
	
	NSWindow *resizingWindow = [notification object];
	NSView *contentView = [resizingWindow contentView];

	if ([contentView isKindOfClass:[GraphView class]])
		[(GraphView *)contentView graphWindowResized];
	
	if (resizingWindow == [self window])
		[self updatePopulationViewHiding];
}

- (void)windowDidMove:(NSNotification *)notification
{
	//[[self document] setTransient:NO]; // Since the user has taken an interest in the window, clear the document's transient status
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
		const EidosCallSignature *sig = nullptr;
		
		if ([signatureString hasPrefix:@"initialize()"])
		{
			static EidosCallSignature_CSP callbackSig = nullptr;
			
			if (!callbackSig)
				callbackSig = EidosCallSignature_CSP((new EidosFunctionSignature("initialize", nullptr, kEidosValueMaskVOID)));
			
			sig = callbackSig.get();
		}
		else if ([signatureString hasPrefix:@"early()"])
		{
			static EidosCallSignature_CSP callbackSig = nullptr;
			
			if (!callbackSig)
				callbackSig = EidosCallSignature_CSP((new EidosFunctionSignature("early", nullptr, kEidosValueMaskVOID)));
			
			sig = callbackSig.get();
		}
		else if ([signatureString hasPrefix:@"late()"])
		{
			static EidosCallSignature_CSP callbackSig = nullptr;
			
			if (!callbackSig)
				callbackSig = EidosCallSignature_CSP((new EidosFunctionSignature("late", nullptr, kEidosValueMaskVOID)));
			
			sig = callbackSig.get();
		}
		else if ([signatureString hasPrefix:@"fitness()"])
		{
			static EidosCallSignature_CSP callbackSig = nullptr;
			
			if (!callbackSig)
				callbackSig = EidosCallSignature_CSP((new EidosFunctionSignature("fitness", nullptr, kEidosValueMaskFloat | kEidosValueMaskSingleton))->AddObject_SN("mutationType", gSLiM_MutationType_Class)->AddObject_OS("subpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULLInvisible));
			
			sig = callbackSig.get();
		}
		else if ([signatureString hasPrefix:@"interaction()"])
		{
			static EidosCallSignature_CSP callbackSig = nullptr;
			
			if (!callbackSig)
				callbackSig = EidosCallSignature_CSP((new EidosFunctionSignature("interaction", nullptr, kEidosValueMaskFloat | kEidosValueMaskSingleton))->AddObject_S("interactionType", gSLiM_InteractionType_Class)->AddObject_OS("subpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULLInvisible));
			
			sig = callbackSig.get();
		}
		else if ([signatureString hasPrefix:@"mateChoice()"])
		{
			static EidosCallSignature_CSP callbackSig = nullptr;
			
			if (!callbackSig)
				callbackSig = EidosCallSignature_CSP((new EidosFunctionSignature("mateChoice", nullptr, kEidosValueMaskNULL | kEidosValueMaskFloat | kEidosValueMaskObject, gSLiM_Individual_Class))->AddObject_OS("subpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULLInvisible));
			
			sig = callbackSig.get();
		}
		else if ([signatureString hasPrefix:@"modifyChild()"])
		{
			static EidosCallSignature_CSP callbackSig = nullptr;
			
			if (!callbackSig)
				callbackSig = EidosCallSignature_CSP((new EidosFunctionSignature("modifyChild", nullptr, kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddObject_OS("subpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULLInvisible));
			
			sig = callbackSig.get();
		}
		else if ([signatureString hasPrefix:@"recombination()"])
		{
			static EidosCallSignature_CSP callbackSig = nullptr;
			
			if (!callbackSig)
				callbackSig = EidosCallSignature_CSP((new EidosFunctionSignature("recombination", nullptr, kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddObject_OS("subpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULLInvisible));
			
			sig = callbackSig.get();
		}
		else if ([signatureString hasPrefix:@"mutation()"])
		{
			static EidosCallSignature_CSP callbackSig = nullptr;
			
			if (!callbackSig)
				callbackSig = EidosCallSignature_CSP((new EidosFunctionSignature("mutation", nullptr, kEidosValueMaskLogical | kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_Mutation_Class))->AddObject_OSN("mutationType", gSLiM_MutationType_Class, gStaticEidosValueNULLInvisible)->AddObject_OSN("subpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULLInvisible));
			
			sig = callbackSig.get();
		}
		else if ([signatureString hasPrefix:@"reproduction()"])
		{
			static EidosCallSignature_CSP callbackSig = nullptr;
			
			if (!callbackSig)
				callbackSig = EidosCallSignature_CSP((new EidosFunctionSignature("reproduction", nullptr, kEidosValueMaskVOID))->AddObject_OSN("subpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULLInvisible)->AddString_OSN("sex", gStaticEidosValueNULLInvisible));
			
			sig = callbackSig.get();
		}
		
		if (sig)
			attributedSignature = [NSAttributedString eidosAttributedStringForCallSignature:sig size:11.0];
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
			int subpopCount = (int)population.subpops_.size();
			auto popIter = population.subpops_.begin();
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
			return sim->population_.subpops_.size();
		}
		else if (aTableView == mutTypeTableView)
		{
			return sim->mutation_types_.size();
		}
		else if (aTableView == genomicElementTypeTableView)
		{
			return sim->genomic_element_types_.size();
		}
		else if (aTableView == interactionTypeTableView)
		{
			return sim->interaction_types_.size();
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
	if (!invalidSimulation && sim)
	{
		if (aTableView == subpopTableView)
		{
			Population &population = sim->population_;
			int subpopCount = (int)population.subpops_.size();
			
			if (rowIndex < subpopCount)
			{
				auto popIter = population.subpops_.begin();
				
				std::advance(popIter, rowIndex);
				slim_objectid_t subpop_id = popIter->first;
				Subpopulation *subpop = popIter->second;
				
				if (aTableColumn == subpopIDColumn)
				{
					return [NSString stringWithFormat:@"p%lld", (int64_t)subpop_id];
				}
				else if (aTableColumn == subpopSizeColumn)
				{
					return [NSString stringWithFormat:@"%lld", (int64_t)subpop->parent_subpop_size_];
				}
#ifdef SLIM_NONWF_ONLY
				else if (sim->ModelType() == SLiMModelType::kModelTypeNonWF)
				{
					// in nonWF models selfing/cloning/sex rates/ratios are emergent, calculated from collected metrics
					double total_offspring = subpop->gui_offspring_cloned_M_ + subpop->gui_offspring_crossed_ + subpop->gui_offspring_empty_ + subpop->gui_offspring_selfed_;
					
					if (subpop->sex_enabled_)
						total_offspring += subpop->gui_offspring_cloned_F_;		// avoid double-counting clones when we are modeling hermaphrodites
					
					if (aTableColumn == subpopSelfingRateColumn)
					{
						if (!subpop->sex_enabled_ && (total_offspring > 0))
							return [NSString stringWithFormat:@"%.2f", subpop->gui_offspring_selfed_ / total_offspring];
					}
					else if (aTableColumn == subpopFemaleCloningRateColumn)
					{
						if (total_offspring > 0)
							return [NSString stringWithFormat:@"%.2f", subpop->gui_offspring_cloned_F_ / total_offspring];
					}
					else if (aTableColumn == subpopMaleCloningRateColumn)
					{
						if (total_offspring > 0)
							return [NSString stringWithFormat:@"%.2f", subpop->gui_offspring_cloned_M_ / total_offspring];
					}
					else if (aTableColumn == subpopSexRatioColumn)
					{
						if (subpop->sex_enabled_ && (subpop->parent_subpop_size_ > 0))
							return [NSString stringWithFormat:@"%.2f", 1.0 - subpop->parent_first_male_index_ / (double)subpop->parent_subpop_size_];
					}
					
					return @"—";
				}
#endif	// SLIM_NONWF_ONLY
#ifdef SLIM_WF_ONLY
				else	// sim->ModelType() == SLiMModelType::kModelTypeWF
				{
					if (aTableColumn == subpopSelfingRateColumn)
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
							return [NSString stringWithFormat:@"%.2f", subpop->parent_sex_ratio_];
						else
							return @"—";
					}
				}
#endif	// SLIM_WF_ONLY
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
						// DFE type 's' has parameters of type string
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
						// All other DFEs have parameters of type double
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
		else if (aTableView == interactionTypeTableView)
		{
			std::map<slim_objectid_t,InteractionType*> &interactionTypes = sim->interaction_types_;
			int interactionTypeCount = (int)interactionTypes.size();
			
			if (rowIndex < interactionTypeCount)
			{
				auto interactionTypeIter = interactionTypes.begin();
				
				std::advance(interactionTypeIter, rowIndex);
				slim_objectid_t interactionTypeID = interactionTypeIter->first;
				InteractionType *interactionType = interactionTypeIter->second;
				
				if (aTableColumn == interactionTypeIDColumn)
				{
					return [NSString stringWithFormat:@"i%lld", (int64_t)interactionTypeID];
				}
				else if (aTableColumn == interactionTypeMaxDistanceColumn)
				{
					return [NSString stringWithFormat:@"%.3f", interactionType->max_distance_];
				}
				else if (aTableColumn == interactionTypeIFTypeColumn)
				{
					switch (interactionType->if_type_)
					{
						case IFType::kFixed:			return @"fixed";
						case IFType::kLinear:			return @"linear";
						case IFType::kExponential:		return @"exp";
						case IFType::kNormal:			return @"normal";
						case IFType::kCauchy:			return @"Cauchy";
					}
				}
				else if (aTableColumn == interactionTypeIFParamsColumn)
				{
					NSMutableString *paramString = [[NSMutableString alloc] init];
					
					// the first parameter is always the maximum interaction strength
					[paramString appendFormat:@"f=%.3f", interactionType->if_param1_];
					
					// append second parameters where applicable
					switch (interactionType->if_type_)
					{
						case IFType::kFixed:
						case IFType::kLinear:
							break;
						case IFType::kExponential:
							[paramString appendFormat:@", β=%.3f", interactionType->if_param2_];
							break;
						case IFType::kNormal:
							[paramString appendFormat:@", σ=%.3f", interactionType->if_param2_];
							break;
						case IFType::kCauchy:
							[paramString appendFormat:@", γ=%.3f", interactionType->if_param2_];
							break;
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
					
					if (scriptBlock->type_ == SLiMEidosBlockType::SLiMEidosUserDefinedFunction)
						return @"—";
					else if (block_id == -1)
						return @"—";
					else
						return [NSString stringWithFormat:@"s%lld", (int64_t)block_id];
				}
				else if (aTableColumn == scriptBlocksStartColumn)
				{
					if (scriptBlock->type_ == SLiMEidosBlockType::SLiMEidosUserDefinedFunction)
						return @"—";
					else if (scriptBlock->start_generation_ == -1)
						return @"MIN";
					else
						return [NSString stringWithFormat:@"%lld", (int64_t)scriptBlock->start_generation_];
				}
				else if (aTableColumn == scriptBlocksEndColumn)
				{
					if (scriptBlock->type_ == SLiMEidosBlockType::SLiMEidosUserDefinedFunction)
						return @"—";
					else if (scriptBlock->end_generation_ == SLIM_MAX_GENERATION + 1)
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
						case SLiMEidosBlockType::SLiMEidosMutationCallback:			return @"mutation()";
						case SLiMEidosBlockType::SLiMEidosReproductionCallback:		return @"reproduction()";
						case SLiMEidosBlockType::SLiMEidosUserDefinedFunction:
						{
							EidosASTNode *function_decl_node = scriptBlock->root_node_->children_[0];
							EidosASTNode *function_name_node = function_decl_node->children_[1];
							const std::string &function_name = function_name_node->token_->token_string_;
							NSString *functionName = [NSString stringWithUTF8String:function_name.c_str()];
							
							return [functionName stringByAppendingString:@"()"];
						}
						case SLiMEidosBlockType::SLiMEidosNoBlockType:				return @"";	// never hit
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
				
				// If we're already showing the tooltip for this muttype, short-circuit; sometimes we get called twice with no exit
				if (functionGraphToolTipWindow && ([functionGraphToolTipWindow mutType] == mutationType))
					return (id _Nonnull)nil;	// get rid of the static analyzer warning
				
				//NSLog(@"show DFE tooltip view here for mut ID %d!", mutationType->mutation_type_id_);
				
				// Make the tooltip window, configure it, and display it
				if (!functionGraphToolTipWindow)
					functionGraphToolTipWindow = [SLiMFunctionGraphToolTipWindow new];
				
				NSPoint tipPoint = NSMakePoint(mouseLocation.x + 0, mouseLocation.y + 65);
				
				tipPoint = [[mutTypeTableView superview] convertPoint:tipPoint toView:nil];
				NSRect tipRect = NSMakeRect(tipPoint.x, tipPoint.y, 0, 0);
				tipPoint = [[mutTypeTableView window] convertRectToScreen:tipRect].origin;
				
				[functionGraphToolTipWindow setMutType:mutationType];	// questionable for it to have a pointer to the mutation type, but I can't think of a way to crash it...
				[functionGraphToolTipWindow setTipPoint:tipPoint];
				[functionGraphToolTipWindow orderFront:nil];
				
				// Wire it to a tracking area so it gets taken down when the mouse moves; see mouseExited: below
				NSTrackingArea *trackingArea = [[NSTrackingArea alloc] initWithRect:*rect
																			options:(NSTrackingMouseEnteredAndExited | NSTrackingActiveAlways)
																			  owner:self
																		   userInfo:@{@"mut_id" : [NSNumber numberWithInteger:mutationType->mutation_type_id_]}];
				
				[mutTypeTableView addTrackingArea:[trackingArea autorelease]];
			}
		}
		else if (aTableView == interactionTypeTableView)
		{
			std::map<slim_objectid_t,InteractionType*> &interactionTypes = sim->interaction_types_;
			int interactionTypeCount = (int)interactionTypes.size();
			
			if (rowIndex < interactionTypeCount)
			{
				auto interactionTypeIter = interactionTypes.begin();
				
				std::advance(interactionTypeIter, rowIndex);
				InteractionType *interactionType = interactionTypeIter->second;
				
				// If we're already showing the tooltip for this muttype, short-circuit; sometimes we get called twice with no exit
				if (functionGraphToolTipWindow && ([functionGraphToolTipWindow interactionType] == interactionType))
					return (id _Nonnull)nil;	// get rid of the static analyzer warning
				
				//NSLog(@"show DFE tooltip view here for interaction ID %d!", interactionType->interaction_type_id_);
				
				// Make the tooltip window, configure it, and display it
				if (!functionGraphToolTipWindow)
					functionGraphToolTipWindow = [SLiMFunctionGraphToolTipWindow new];
				
				NSPoint tipPoint = NSMakePoint(mouseLocation.x + 0, mouseLocation.y + 65);
				
				tipPoint = [[interactionTypeTableView superview] convertPoint:tipPoint toView:nil];
				NSRect tipRect = NSMakeRect(tipPoint.x, tipPoint.y, 0, 0);
				tipPoint = [[interactionTypeTableView window] convertRectToScreen:tipRect].origin;
				
				[functionGraphToolTipWindow setInteractionType:interactionType];	// questionable for it to have a pointer to the interaction type, but I can't think of a way to crash it...
				[functionGraphToolTipWindow setTipPoint:tipPoint];
				[functionGraphToolTipWindow orderFront:nil];
				
				// Wire it to a tracking area so it gets taken down when the mouse moves; see mouseExited: below
				NSTrackingArea *trackingArea = [[NSTrackingArea alloc] initWithRect:*rect
																			options:(NSTrackingMouseEnteredAndExited | NSTrackingActiveAlways)
																			  owner:self
																		   userInfo:@{@"int_id" : [NSNumber numberWithInteger:interactionType->interaction_type_id_]}];
				
				[interactionTypeTableView addTrackingArea:[trackingArea autorelease]];
			}
		}
	}
	
	return (id _Nonnull)nil;	// get rid of the static analyzer warning
}

- (void)hideMiniGraphToolTipWindow
{
	if (functionGraphToolTipWindow)
	{
		[functionGraphToolTipWindow orderOut:nil];
		[functionGraphToolTipWindow setMutType:nullptr];
		[functionGraphToolTipWindow setInteractionType:nullptr];
	}
}

// Used to take down a custom tooltip window that we may have shown above, displaying a mutation type's DFE
- (void)mouseExited:(NSEvent *)event
{
	if (!functionGraphToolTipWindow)
		return;
	
	NSTrackingArea *trackingArea = [event trackingArea];
	NSDictionary *userInfo = [trackingArea userInfo];
	NSObject *mut_id_object = [userInfo objectForKey:@"mut_id"];
	NSObject *int_id_object = [userInfo objectForKey:@"int_id"];
	
	if ((!mut_id_object && !int_id_object) || (mut_id_object && int_id_object))
		return;
	
	if (mut_id_object)
	{
		slim_objectid_t mut_id = (slim_objectid_t)[(NSNumber *)mut_id_object integerValue];
		
		if (mut_id != [functionGraphToolTipWindow mutType]->mutation_type_id_)
			return;
		
		//NSLog(@"   take down DFE tooltip view here for mut ID %d!", mut_id);
		
		[mutTypeTableView removeTrackingArea:trackingArea];
	}
	else if (int_id_object)
	{
		slim_objectid_t int_id = (slim_objectid_t)[(NSNumber *)int_id_object integerValue];
		
		if (int_id != [functionGraphToolTipWindow interactionType]->interaction_type_id_)
			return;
		
		//NSLog(@"   take down DFE tooltip view here for interaction ID %d!", int_id);
		
		[interactionTypeTableView removeTrackingArea:trackingArea];
	}
	
	[self hideMiniGraphToolTipWindow];
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
		NSArray *subviews;
		
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma deploymate push "ignored-api-availability"	// arrangedSubviews: is available on 10.11 and later
		if ([splitView respondsToSelector:@selector(arrangedSubviews)])
			subviews = [splitView arrangedSubviews];
		else
			subviews = [splitView subviews];		// on 10.10 and before, all subviews are arranged
#pragma deploymate pop
#pragma GCC diagnostic pop
		
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

- (void)drawerWillClose:(NSNotification *)notification
{
	[self hideMiniGraphToolTipWindow];
}

- (void)drawerDidOpen:(NSNotification *)notification
{
	[buttonForDrawer setState:NSOnState];
}

- (void)drawerDidClose:(NSNotification *)notification
{
	[buttonForDrawer setState:NSOffState];
}

@end

























































