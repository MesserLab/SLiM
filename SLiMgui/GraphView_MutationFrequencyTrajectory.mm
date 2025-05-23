//
//  GraphView_MutationFrequencyTrajectory.m
//  SLiM
//
//  Created by Ben Haller on 3/11/15.
//  Copyright (c) 2015-2025 Benjamin C. Haller.  All rights reserved.
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


#import "GraphView_MutationFrequencyTrajectory.h"
#import "SLiMWindowController.h"

#include "community.h"


@implementation GraphView_MutationFrequencyTrajectory

- (id)initWithFrame:(NSRect)frameRect withController:(SLiMWindowController *)controller
{
	if (self = [super initWithFrame:frameRect withController:controller])
	{
		//[self setXAxisRangeFromTick];	// the end tick is not yet known
		
		[self setXAxisLabelString:@"Tick"];
		[self setYAxisLabelString:@"Frequency"];
		
		[self setAllowXAxisUserRescale:YES];
		[self setAllowYAxisUserRescale:YES];
		
		[self setShowHorizontalGridLines:YES];
		[self setTweakXAxisTickLabelAlignment:YES];
		
		// Start with no selected subpop or mutation-type; these will be set to the first menu item added when menus are constructed
		_selectedSubpopulationID = -1;
		_selectedMutationTypeIndex = -1;
		
		// Start plotting lost mutations, by default
		_plotLostMutations = YES;
		_plotFixedMutations = YES;
		_plotActiveMutations = YES;
		_useColorsForPlotting = YES;
		
		// Make our pop-up menu buttons
		subpopulationButton = [self addPopUpWithAction:@selector(subpopulationPopupChanged:)];
		mutationTypeButton = [self addPopUpWithAction:@selector(mutationTypePopupChanged:)];
		
		[self addSubpopulationsToMenu];
		[self addMutationTypesToMenu];
		[self setConstraintsForPopups];
	}
	
	return self;
}

- (void)dealloc
{
	[self invalidateCachedData];
	
	[super dealloc];
}

- (NSPopUpButton *)addPopUpWithAction:(SEL)action
{
	NSPopUpButton *popupButton = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(10, 10, 100, 47) pullsDown:NO];
	[popupButton setFont:[NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:NSControlSizeMini]]];
	[popupButton setControlSize:NSControlSizeMini];
	[popupButton setAutoenablesItems:NO];
	[popupButton setTarget:self];
	[popupButton setAction:action];
	
	[popupButton addItemWithTitle:@"foo"];
	
	// Resize the popup using its intrinsic size
	[popupButton sizeToFit];
	
	NSSize intrinsicSize = [popupButton intrinsicContentSize];
	[popupButton setFrame:NSMakeRect(10, 10, intrinsicSize.width, intrinsicSize.height)];
	[popupButton setTranslatesAutoresizingMaskIntoConstraints:NO];
	
	// Add the popup as a subview
	[self addSubview:[popupButton autorelease]];
	
	return popupButton;
}

- (void)setConstraintsForPopups
{
	NSDictionary *viewsDictionary = @{@"subpopulationButton" : subpopulationButton, @"mutationTypeButton" : mutationTypeButton};
	
	[self addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"|-6-[subpopulationButton]-6-[mutationTypeButton]" options:0 metrics:nil views:viewsDictionary]];
	[self addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"V:[subpopulationButton]-6-|" options:0 metrics:nil views:viewsDictionary]];
	[self addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"V:[mutationTypeButton]-6-|" options:0 metrics:nil views:viewsDictionary]];
}

- (BOOL)addSubpopulationsToMenu
{
	Species *displaySpecies = [self focalDisplaySpecies];
	NSMenuItem *lastItem;
	slim_objectid_t firstTag = -1;
	
	// Depopulate and populate the menu
	[subpopulationButton removeAllItems];

	if (displaySpecies)
	{
		Population &population = displaySpecies->population_;
		
		for (auto popIter = population.subpops_.begin(); popIter != population.subpops_.end(); ++popIter)
		{
			slim_objectid_t subpopID = popIter->first;
			//Subpopulation *subpop = popIter->second;
			NSString *subpopString = [NSString stringWithFormat:@"p%lld", (long long int)subpopID];
			
			[subpopulationButton addItemWithTitle:subpopString];
			lastItem = [subpopulationButton lastItem];
			[lastItem setTag:subpopID];
			
			// Remember the first item we add; we will use this item's tag to make a selection if needed
			if (firstTag == -1)
				firstTag = subpopID;
		}
	}
	
	[subpopulationButton slimSortMenuItemsByTag];
	
	// If it is empty, disable it
	BOOL hasItems = ([subpopulationButton numberOfItems] >= 1);
	
	[subpopulationButton setEnabled:hasItems];
	
	// Fix the selection and then select the chosen subpopulation
	if (hasItems)
	{
		NSInteger indexOfTag = [subpopulationButton indexOfItemWithTag:[self selectedSubpopulationID]];
		
		if (indexOfTag == -1)
			[self setSelectedSubpopulationID:-1];
		if ([self selectedSubpopulationID] == -1)
			[self setSelectedSubpopulationID:firstTag];
		
		[subpopulationButton selectItemWithTag:[self selectedSubpopulationID]];
		[subpopulationButton synchronizeTitleAndSelectedItem];
	}
	
	return hasItems;	// YES if we found at least one subpop to add to the menu, NO otherwise
}

- (BOOL)addMutationTypesToMenu
{
	Species *displaySpecies = [self focalDisplaySpecies];
	NSMenuItem *lastItem;
	int firstTag = -1;
	
	// Depopulate and populate the menu
	[mutationTypeButton removeAllItems];
	
	if (displaySpecies)
	{
		std::map<slim_objectid_t,MutationType*> &mutationTypes = displaySpecies->mutation_types_;
		
		for (auto mutTypeIter = mutationTypes.begin(); mutTypeIter != mutationTypes.end(); ++mutTypeIter)
		{
			MutationType *mutationType = mutTypeIter->second;
			slim_objectid_t mutationTypeID = mutationType->mutation_type_id_;
			int mutationTypeIndex = mutationType->mutation_type_index_;
			NSString *mutationTypeString = [NSString stringWithFormat:@"m%lld", (long long int)mutationTypeID];
			
			[mutationTypeButton addItemWithTitle:mutationTypeString];
			lastItem = [mutationTypeButton lastItem];
			[lastItem setTag:mutationTypeIndex];
			
			// Remember the first item we add; we will use this item's tag to make a selection if needed
			if (firstTag == -1)
				firstTag = mutationTypeIndex;
		}
	}
	
	[mutationTypeButton slimSortMenuItemsByTag];
	
	// If it is empty, disable it
	BOOL hasItems = ([mutationTypeButton numberOfItems] >= 1);
	
	[mutationTypeButton setEnabled:hasItems];
	
	// Fix the selection and then select the chosen mutation type
	if (hasItems)
	{
		NSInteger indexOfTag = [mutationTypeButton indexOfItemWithTag:[self selectedMutationTypeIndex]];
		
		if (indexOfTag == -1)
			[self setSelectedMutationTypeIndex:-1];
		if ([self selectedMutationTypeIndex] == -1)
			[self setSelectedMutationTypeIndex:firstTag];
		
		[mutationTypeButton selectItemWithTag:[self selectedMutationTypeIndex]];
		[mutationTypeButton synchronizeTitleAndSelectedItem];
	}
	
	return hasItems;	// YES if we found at least one muttype to add to the menu, NO otherwise
}

- (void)invalidateCachedData
{
//	SLiMWindowController *controller = [self slimWindowController];
//	Species &species = controller->community->single_species_;
//	slim_tick_t tick = community->Tick();
//	
//	NSLog(@"-invalidateCachedData called at tick %d", tick);
	
	if (frequencyHistoryDict)
	{
		[frequencyHistoryDict release];
		frequencyHistoryDict = nil;
	}
	
	if (frequencyHistoryColdStorageLost)
	{
		[frequencyHistoryColdStorageLost release];
		frequencyHistoryColdStorageLost = nil;
	}
	
	if (frequencyHistoryColdStorageFixed)
	{
		[frequencyHistoryColdStorageFixed release];
		frequencyHistoryColdStorageFixed = nil;
	}
}

- (void)fetchDataForFinishedTick
{
	SLiMWindowController *controller = [self slimWindowController];
	Community &community = *controller->community;
	Species *displaySpecies = [self focalDisplaySpecies];
	
	if (!displaySpecies)
		return;
	
	Population &population = displaySpecies->population_;
	int registry_size;
	const MutationIndex *registry = population.MutationRegistry(&registry_size);
	static BOOL alreadyHere = NO;
	
	// Check that the subpop we're supposed to be surveying exists; if not, bail.
	BOOL foundSelectedSubpop = NO;
	BOOL foundSelectedMutType = NO;
	
	for (const std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population.subpops_)
		if (subpop_pair.first == _selectedSubpopulationID)	// find our chosen subpop
			foundSelectedSubpop = YES;
	
	for (const std::pair<const slim_objectid_t,MutationType*> &subpop_pair : displaySpecies->mutation_types_)
		if (subpop_pair.second->mutation_type_index_ == _selectedMutationTypeIndex)	// find our chosen muttype
			foundSelectedMutType = YES;
	
	// Make sure we have a selected subpop if possible.  Our menu might not have been loaded, or our chosen subpop might have vanished.
	if ((_selectedSubpopulationID == -1) || !foundSelectedSubpop)
	{
		// Call -addSubpopulationsToMenu to reload our subpop menu and choose a subpop.  This will call us, so we use a static flag to prevent re-entrancy.
		alreadyHere = YES;
		foundSelectedSubpop = [self addSubpopulationsToMenu];
		alreadyHere = NO;
	}
	
	// Make sure we have a selected muttype if possible.  Our menu might not have been loaded, or our chosen muttype might have vanished.
	if ((_selectedMutationTypeIndex == -1) || !foundSelectedMutType)
	{
		// Call -addMutationTypesToMenu to reload our muttype menu and choose a muttype.  This will call us, so we use a static flag to prevent re-entrancy.
		alreadyHere = YES;
		foundSelectedMutType = [self addMutationTypesToMenu];
		alreadyHere = NO;
	}
	
	if (!foundSelectedSubpop || !foundSelectedMutType)
		return;
	
	if (!frequencyHistoryDict)
		frequencyHistoryDict = [[NSMutableDictionary alloc] init];
	if (!frequencyHistoryColdStorageLost)
		frequencyHistoryColdStorageLost = [[NSMutableArray alloc] init];
	if (!frequencyHistoryColdStorageFixed)
		frequencyHistoryColdStorageFixed = [[NSMutableArray alloc] init];

	// Start by zeroing out the "updated" flags; this is how we find dead mutations
	[frequencyHistoryDict enumerateKeysAndObjectsUsingBlock:^(NSNumber *key, MutationFrequencyHistory *history, BOOL *stop) {
		history->updated = NO;
	}];
	
	//
	// this code is a slightly modified clone of the code in Population::TallyMutationReferences; here we scan only the
	// subpopulation that is being displayed in this graph, and tally into gui_scratch_reference_count only
	// BCH 4/21/2023: This could use mutrun use counts to run faster...
	//
	int haplosome_count_per_individual = displaySpecies->HaplosomeCountPerIndividual();
	int subpop_total_haplosome_count = 0;
	
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	const MutationIndex *registry_iter = registry;
	const MutationIndex *registry_iter_end = registry + registry_size;
	
	for (; registry_iter != registry_iter_end; ++registry_iter)
		(mut_block_ptr + *registry_iter)->gui_scratch_reference_count_ = 0;
	
	for (const std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population.subpops_)
	{
		if (subpop_pair.first == _selectedSubpopulationID)	// tally only within our chosen subpop
		{
			Subpopulation *subpop = subpop_pair.second;
			
			for (Individual *ind : subpop->parent_individuals_)
			{
				Haplosome **haplosomes = ind->haplosomes_;
				
				for (int haplosome_index = 0; haplosome_index < haplosome_count_per_individual; haplosome_index++)
				{
					Haplosome *haplosome = haplosomes[haplosome_index];
					
					if (!haplosome->IsNull())
					{
						int mutrun_count = haplosome->mutrun_count_;
						
						for (int run_index = 0; run_index < mutrun_count; ++run_index)
						{
							const MutationRun *mutrun = haplosome->mutruns_[run_index];
							const MutationIndex *haplosome_iter = mutrun->begin_pointer_const();
							const MutationIndex *haplosome_end_iter = mutrun->end_pointer_const();
							
							for (; haplosome_iter != haplosome_end_iter; ++haplosome_iter)
							{
								const Mutation *mutation = mut_block_ptr + *haplosome_iter;
								
								if (mutation->mutation_type_ptr_->mutation_type_index_ == _selectedMutationTypeIndex)
									(mutation->gui_scratch_reference_count_)++;
							}
						}
						
						subpop_total_haplosome_count++;
					}
				}
			}
		}
	}
	
	// Now we can run through the mutations and use the tallies in gui_scratch_reference_count to update our histories
	for (registry_iter = registry; registry_iter != registry_iter_end; ++registry_iter)
	{
		const Mutation *mutation = mut_block_ptr + *registry_iter;
		slim_refcount_t refcount = mutation->gui_scratch_reference_count_;
		
		if (refcount)
		{
			uint16_t value = (uint16_t)((refcount * (unsigned long long)UINT16_MAX) / subpop_total_haplosome_count);	// FIXME static analyzer says potential divide by zero here
			NSNumber *mutationIDNumber = [[NSNumber alloc] initWithLongLong:mutation->mutation_id_];
			MutationFrequencyHistory *history = [frequencyHistoryDict objectForKey:mutationIDNumber];
			
			//NSLog(@"mutation refcount %d has uint16_t value %d, found history %p for id %lld", refcount, value, history, (long long int)(mutation->mutation_id_));
			
			if (history)
			{
				// We have a history for this mutation, so we just need to add an entry; this sets the updated flag
				[history addEntry:value];
			}
			else
			{
				// No history, so we make one starting at this tick; this also sets the updated flag
				// Note we use Tick() - 1, because the tick counter has already been advanced to the next tick
				history = [[MutationFrequencyHistory alloc] initWithEntry:value forMutation:mutation atBaseTick:community.Tick() - 1];
				
				[frequencyHistoryDict setObject:history forKey:mutationIDNumber];
				[history release];
			}
			
			[mutationIDNumber release];
		}
	}
	
	// OK, now every mutation that has frequency >0 in our subpop has got a current entry.  But what about mutations that used to circulate,
	// but don't any more?  These could still be active in a different subpop, or they might be gone – lost or fixed.  For the former case,
	// we need to add an entry with frequency zero.  For the latter case, we need to put their history into "cold storage" for efficiency.
	__block NSMutableDictionary *historiesToAddToColdStorage = nil;
	
	[frequencyHistoryDict enumerateKeysAndObjectsUsingBlock:^(NSNumber *key, MutationFrequencyHistory *history, BOOL *stop) {
		if (!history->updated)
		{
			slim_mutationid_t historyID = history->mutationID;
			const MutationIndex *mutation_iter = registry;
			const MutationIndex *mutation_iter_end = registry + registry_size;
			BOOL mutationStillExists = NO;
			
			for ( ; mutation_iter != mutation_iter_end; ++mutation_iter)
			{
				const Mutation *mutation = mut_block_ptr + *mutation_iter;
				slim_mutationid_t mutationID = mutation->mutation_id_;
				
				if (historyID == mutationID)
				{
					mutationStillExists = YES;
					break;
				}
			}
			
			if (mutationStillExists)
			{
				// The mutation is still around, so just add a zero entry for it
				[history addEntry:0];
			}
			else
			{
				// The mutation is gone, so we need to put its history into cold storage, but we can't modify
				// our dictionary since we are enumerating it, so we just make a record and do it below
				if (historiesToAddToColdStorage)
					[historiesToAddToColdStorage setObject:history forKey:key];
				else
					historiesToAddToColdStorage = [[NSMutableDictionary alloc] initWithObjectsAndKeys:history, key, nil];
			}
		}
	}];
	
	// Now, if historiesToAddToColdStorage is non-nil, we have histories to put into cold storage; do it now
	if (historiesToAddToColdStorage)
	{
		[historiesToAddToColdStorage enumerateKeysAndObjectsUsingBlock:^(NSNumber *key, MutationFrequencyHistory *history, BOOL *stop) {
			// The remaining tricky bit is that we have to figure out whether the vanished mutation was fixed or lost; we do this by
			// scanning through all our Substitution objects, which use the same unique IDs as Mutations use.  We need to know this
			// for two reasons: to add the final entry for the mutation, and to put it into the correct cold storage array.
			slim_mutationid_t mutationID = history->mutationID;
			BOOL wasFixed = NO;
			
			std::vector<Substitution*> &substitutions = population.substitutions_;
			
			for (const Substitution *substitution : substitutions)
			{
				if (substitution->mutation_id_ == mutationID)
				{
					wasFixed = YES;
					break;
				}
			}
			
			if (wasFixed)
			{
				[history addEntry:UINT16_MAX];
				[frequencyHistoryColdStorageFixed addObject:history];
			}
			else
			{
				[history addEntry:0];
				[frequencyHistoryColdStorageLost addObject:history];
			}
			
			[frequencyHistoryDict removeObjectForKey:key];
		}];
		[historiesToAddToColdStorage release];
	}
	
	//NSLog(@"frequencyHistoryDict has %lld entries, frequencyHistoryColdStorageLost has %lld entries, frequencyHistoryColdStorageFixed has %lld entries", (long long int)[frequencyHistoryDict count], (long long int)[frequencyHistoryColdStorageLost count], (long long int)[frequencyHistoryColdStorageFixed count]);
	
	lastTick = community.Tick();
}

- (void)setSelectedSubpopulationID:(slim_objectid_t)newID
{
	if (_selectedSubpopulationID != newID)
	{
		_selectedSubpopulationID = newID;
		
		[self invalidateCachedData];
		[self fetchDataForFinishedTick];
		[self setNeedsDisplay:YES];
	}
}

- (void)setSelectedMutationTypeIndex:(int)newIndex
{
	if (_selectedMutationTypeIndex != newIndex)
	{
		_selectedMutationTypeIndex = newIndex;
		
		[self invalidateCachedData];
		[self fetchDataForFinishedTick];
		[self setNeedsDisplay:YES];
	}
}

- (IBAction)subpopulationPopupChanged:(id)sender
{
	[self setSelectedSubpopulationID:SLiMClampToObjectidType([subpopulationButton selectedTag])];
}

- (IBAction)mutationTypePopupChanged:(id)sender
{
	[self setSelectedMutationTypeIndex:(int)[mutationTypeButton selectedTag]];
}

- (void)controllerRecycled
{
	SLiMWindowController *controller = [self slimWindowController];
	
	if (![controller invalidSimulation])
	{
		//if (![self xAxisIsUserRescaled])
		//	[self setXAxisRangeFromTick];	// the end tick is not yet known
		
		[self setNeedsDisplay:YES];
	}
	
	// Remake our popups, whether or not the controller is valid
	[self invalidateCachedData];
	[self addSubpopulationsToMenu];
	[self addMutationTypesToMenu];
	
	[super controllerRecycled];
}

- (void)controllerTickFinished
{
	[super controllerTickFinished];
	
	// Check for an unexpected change in tick, in which case we invalidate all our histories and start over
	SLiMWindowController *controller = [self slimWindowController];
	Community *community = controller->community;
	
	if (lastTick != community->Tick() - 1)
	{
		[self invalidateCachedData];
		[self setNeedsDisplay:YES];
	}
	
	// Fetch and store the frequencies for all mutations of the selected mutation type(s), within the subpopulation selected
	[self fetchDataForFinishedTick];
}

- (void)updateAfterTick
{
	// BCH 3/20/2024: We set the x axis range each tick, because the end tick is now invalid until after initialize() callbacks
	if (![self xAxisIsUserRescaled])
		[self setXAxisRangeFromTick];
	
	// Rebuild the subpop and muttype menus; this has the side effect of checking and fixing our selections, and that,
	// in turn, will have the side effect of invaliding our cache and fetching new data if needed
	[self addSubpopulationsToMenu];
	[self addMutationTypesToMenu];
	
	[super updateAfterTick];
}

- (void)drawHistory:(MutationFrequencyHistory *)history inInteriorRect:(NSRect)interiorRect
{
	int entryCount = history->entryCount;
	
	if (entryCount > 1)		// one entry just generates a moveto
	{
		uint16_t *entries = history->entries;
		NSBezierPath *linePath = [NSBezierPath bezierPath];
		uint16_t firstValue = *entries;
		double firstFrequency = ((double)firstValue) / UINT16_MAX;
		slim_tick_t tick = history->baseTick;
		NSPoint firstPoint = NSMakePoint([self plotToDeviceX:tick withInteriorRect:interiorRect], [self plotToDeviceY:firstFrequency withInteriorRect:interiorRect]);
		
		[linePath moveToPoint:firstPoint];
		
		for (int entryIndex = 1; entryIndex < entryCount; ++entryIndex)
		{
			uint16_t value = entries[entryIndex];
			double frequency = ((double)value) / UINT16_MAX;
			NSPoint nextPoint = NSMakePoint([self plotToDeviceX:++tick withInteriorRect:interiorRect], [self plotToDeviceY:frequency withInteriorRect:interiorRect]);
			
			[linePath lineToPoint:nextPoint];
		}
		
		[linePath setLineWidth:1.0];
		[linePath stroke];
	}
}

- (void)drawGraphInInteriorRect:(NSRect)interiorRect withController:(SLiMWindowController *)controller
{
	BOOL plotInColor = [self useColorsForPlotting];
	
	if (!plotInColor)
		[[NSColor blackColor] set];
	
	// Go through all our history entries and draw a line for each.  First we draw the ones in cold storage, then the active ones.
	if ([self plotLostMutations])
	{
		if (plotInColor)
			[[NSColor redColor] set];
		
		[frequencyHistoryColdStorageLost enumerateObjectsUsingBlock:^(MutationFrequencyHistory *history, NSUInteger idx, BOOL *stop) {
			[self drawHistory:history inInteriorRect:interiorRect];
		}];
	}
	
	if ([self plotFixedMutations])
	{
		if (plotInColor)
			[[NSColor colorWithCalibratedRed:0.4 green:0.4 blue:1.0 alpha:1.0] set];
		
		[frequencyHistoryColdStorageFixed enumerateObjectsUsingBlock:^(MutationFrequencyHistory *history, NSUInteger idx, BOOL *stop) {
			[self drawHistory:history inInteriorRect:interiorRect];
		}];
	}
	
	if ([self plotActiveMutations])
	{
		if (plotInColor)
			[[NSColor blackColor] set];
		
		[frequencyHistoryDict enumerateKeysAndObjectsUsingBlock:^(NSNumber *key, MutationFrequencyHistory *history, BOOL *stop) {
			[self drawHistory:history inInteriorRect:interiorRect];
		}];
	}
}

- (IBAction)toggleShowLostMutations:(id)sender
{
	[self setPlotLostMutations:![self plotLostMutations]];
	[self setNeedsDisplay:YES];
}

- (IBAction)toggleShowFixedMutations:(id)sender
{
	[self setPlotFixedMutations:![self plotFixedMutations]];
	[self setNeedsDisplay:YES];
}

- (IBAction)toggleShowActiveMutations:(id)sender
{
	[self setPlotActiveMutations:![self plotActiveMutations]];
	[self setNeedsDisplay:YES];
}

- (IBAction)toggleUseColorsForPlotting:(id)sender
{
	[self setUseColorsForPlotting:![self useColorsForPlotting]];
	[self setNeedsDisplay:YES];
}

- (IBAction)copy:(id)sender
{
	// Hide our popups around the call to super, so they don't appear in the snapshot
	[subpopulationButton setHidden:YES];
	[mutationTypeButton setHidden:YES];
	
	[super copy:sender];
	
	[subpopulationButton setHidden:NO];
	[mutationTypeButton setHidden:NO];
}

- (IBAction)saveGraph:(id)sender
{
	// Hide our popups around the call to super, so they don't appear in the snapshot
	[subpopulationButton setHidden:YES];
	[mutationTypeButton setHidden:YES];
	
	[super saveGraph:sender];
	
	[subpopulationButton setHidden:NO];
	[mutationTypeButton setHidden:NO];
}

- (void)subclassAddItemsToMenu:(NSMenu *)menu forEvent:(NSEvent *)theEvent
{
	if (menu)
	{
		{
			NSMenuItem *menuItem = [menu addItemWithTitle:([self plotLostMutations] ? @"Hide Lost Mutations" : @"Show Lost Mutations") action:@selector(toggleShowLostMutations:) keyEquivalent:@""];
			
			[menuItem setTarget:self];
		}
		
		{
			NSMenuItem *menuItem = [menu addItemWithTitle:([self plotFixedMutations] ? @"Hide Fixed Mutations" : @"Show Fixed Mutations") action:@selector(toggleShowFixedMutations:) keyEquivalent:@""];
			
			[menuItem setTarget:self];
		}
		
		{
			NSMenuItem *menuItem = [menu addItemWithTitle:([self plotActiveMutations] ? @"Hide Active Mutations" : @"Show Active Mutations") action:@selector(toggleShowActiveMutations:) keyEquivalent:@""];
			
			[menuItem setTarget:self];
		}
		
		[menu addItem:[NSMenuItem separatorItem]];
		
		{
			NSMenuItem *menuItem = [menu addItemWithTitle:([self useColorsForPlotting] ? @"Black Plot Lines" : @"Colored Plot Lines") action:@selector(toggleUseColorsForPlotting:) keyEquivalent:@""];
			
			[menuItem setTarget:self];
		}
	}
}

- (NSArray *)legendKey
{
	if ([self useColorsForPlotting])
	{
		NSMutableArray *legendKey = [NSMutableArray array];
		
		if ([self plotLostMutations])
			[legendKey addObject:@[@"lost", [NSColor redColor]]];
		
		if ([self plotFixedMutations])
			[legendKey addObject:@[@"fixed", [NSColor colorWithCalibratedRed:0.4 green:0.4 blue:1.0 alpha:1.0]]];
		
		if ([self plotActiveMutations])
			[legendKey addObject:@[@"active", [NSColor blackColor]]];
		
		return legendKey;
	}
	
	return nil;
}

- (void)appendEntriesFromArray:(NSArray *)array toString:(NSMutableString *)string completedTicks:(slim_tick_t)completedTicks
{
	[array enumerateObjectsUsingBlock:^(MutationFrequencyHistory *history, NSUInteger idx, BOOL *stop) {
		int entryCount = history->entryCount;
		slim_tick_t baseTick = history->baseTick;
		
		for (slim_tick_t tick = 1; tick <= completedTicks; ++tick)
		{
			if (tick < baseTick)
				[string appendString:@"NA, "];
			else if (tick - baseTick < entryCount)
				[string appendFormat:@"%.4f, ", ((double)history->entries[tick - baseTick]) / UINT16_MAX];
			else
				[string appendString:@"NA, "];
		}
		
		[string appendString:@"\n"];
	}];
}

- (NSString *)stringForDataWithController:(SLiMWindowController *)controller
{
	NSMutableString *string = [NSMutableString stringWithString:@"# Graph data: fitness trajectories\n"];
	slim_tick_t completedTicks = controller->community->Tick() - 1;
	
	[string appendString:[self dateline]];
	
	if ([self plotLostMutations])
	{
		[string appendString:@"\n\n# Lost mutations:\n"];
		[self appendEntriesFromArray:frequencyHistoryColdStorageLost toString:string completedTicks:completedTicks];
	}
	
	if ([self plotFixedMutations])
	{
		[string appendString:@"\n\n# Fixed mutations:\n"];
		[self appendEntriesFromArray:frequencyHistoryColdStorageFixed toString:string completedTicks:completedTicks];
	}
	
	if ([self plotActiveMutations])
	{
		[string appendString:@"\n\n# Active mutations:\n"];
		[self appendEntriesFromArray:[frequencyHistoryDict allValues] toString:string completedTicks:completedTicks];
	}
	
	// Get rid of extra commas
	[string replaceOccurrencesOfString:@", \n" withString:@"\n" options:0 range:NSMakeRange(0, [string length])];
	
	return string;
}

@end


@implementation MutationFrequencyHistory

- (instancetype)initWithEntry:(uint16_t)value forMutation:(const Mutation *)mutation atBaseTick:(slim_tick_t)tick
{
	if (self = [super init])
	{
		mutationID = mutation->mutation_id_;
		mutationType = mutation->mutation_type_ptr_;
		baseTick = tick;
		
		bufferSize = 0;
		entryCount = 0;
		[self addEntry:value];
	}
	
	return self;
}

- (void)dealloc
{
	if (entries)
	{
		free(entries);
		entries = NULL;
		
		bufferSize = 0;
		entryCount = 0;
	}
	
	[super dealloc];
}

- (void)addEntry:(uint16_t)value
{
	if (entryCount == bufferSize)
	{
		// We need to expand
		bufferSize += 1024;
		entries = (uint16_t *)realloc(entries, bufferSize * sizeof(uint16_t));
	}
	
	entries[entryCount++] = value;
	updated = YES;
}

@end



























































