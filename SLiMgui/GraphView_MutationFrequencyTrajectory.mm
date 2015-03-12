//
//  GraphView_MutationFrequencyTrajectory.m
//  SLiM
//
//  Created by Ben Haller on 3/11/15.
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


#import "GraphView_MutationFrequencyTrajectory.h"
#import "SLiMWindowController.h"


@implementation GraphView_MutationFrequencyTrajectory

- (id)initWithFrame:(NSRect)frameRect withController:(SLiMWindowController *)controller
{
	if (self = [super initWithFrame:frameRect withController:controller])
	{
		[self setXAxisMax:[self slimWindowController]->sim->time_duration_];
		[self setXAxisMajorTickInterval:10000];
		[self setXAxisMinorTickInterval:1000];
		[self setXAxisMajorTickModulus:10];
		[self setXAxisTickValuePrecision:0];
		
		[self setXAxisLabelString:@"Generation"];
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
	[popupButton setFont:[NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:NSMiniControlSize]]];
	[popupButton setControlSize:NSMiniControlSize];
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
	NSDictionary *viewsDictionary = [NSDictionary dictionaryWithObjectsAndKeys:subpopulationButton, @"subpopulationButton", mutationTypeButton, @"mutationTypeButton", nil];
	
	[self addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"|-6-[subpopulationButton]-6-[mutationTypeButton]" options:0 metrics:nil views:viewsDictionary]];
	[self addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"V:[subpopulationButton]-6-|" options:0 metrics:nil views:viewsDictionary]];
	[self addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"V:[mutationTypeButton]-6-|" options:0 metrics:nil views:viewsDictionary]];
}

- (void)sortMenuItemsByTagInPopUpButton:(NSPopUpButton *)popup
{
	NSMenu *menu = [popup menu];
	int nItems = (int)[menu numberOfItems];
	
	// completely dumb bubble sort; not worth worrying about
	do
	{
		BOOL foundSwap = NO;
		
		for (int i = 0; i < nItems - 1; ++i)
		{
			NSMenuItem *firstItem = [menu itemAtIndex:i];
			NSMenuItem *secondItem = [menu itemAtIndex:i + 1];
			NSInteger firstTag = [firstItem tag];
			NSInteger secondTag = [secondItem tag];
			
			if (firstTag > secondTag)
			{
				[secondItem retain];
				[menu removeItemAtIndex:i + 1];
				[menu insertItem:secondItem atIndex:i];
				[secondItem release];
				
				foundSwap = YES;
			}
		}
		
		if (!foundSwap)
			break;
	}
	while (YES);
}

- (void)addSubpopulationsToMenu
{
	SLiMWindowController *controller = [self slimWindowController];
	NSMenuItem *lastItem;
	int firstTag = -1;
	
	// Depopulate and populate the menu
	[subpopulationButton removeAllItems];

	if (![controller invalidSimulation])
	{
		Population &population = controller->sim->population_;
		
		for (auto popIter = population.begin(); popIter != population.end(); ++popIter)
		{
			int subpopID = popIter->first;
			//Subpopulation *subpop = popIter->second;
			NSString *subpopString = [NSString stringWithFormat:@"p%d", subpopID];
			
			[subpopulationButton addItemWithTitle:subpopString];
			lastItem = [subpopulationButton lastItem];
			[lastItem setTag:subpopID];
			
			// Remember the first item we add; we will use this item's tag to make a selection if needed
			if (firstTag == -1)
				firstTag = subpopID;
		}
	}
	
	[self sortMenuItemsByTagInPopUpButton:subpopulationButton];
	
	// If it is empty, disable it
	[subpopulationButton setEnabled:([subpopulationButton numberOfItems] >= 1)];
	
	// Fix the selection and then select the chosen subpopulation
	NSInteger indexOfTag = [subpopulationButton indexOfItemWithTag:[self selectedSubpopulationID]];
	
	if (indexOfTag == -1)
		[self setSelectedSubpopulationID:-1];
	if ([self selectedSubpopulationID] == -1)
		[self setSelectedSubpopulationID:firstTag];
	
	[subpopulationButton selectItemWithTag:[self selectedSubpopulationID]];
	[subpopulationButton synchronizeTitleAndSelectedItem];
}

- (void)addMutationTypesToMenu
{
	SLiMWindowController *controller = [self slimWindowController];
	NSMenuItem *lastItem;
	int firstTag = -1;
	
	// Depopulate and populate the menu
	[mutationTypeButton removeAllItems];
	
	if (![controller invalidSimulation])
	{
		std::map<int,MutationType*> &mutationTypes = controller->sim->mutation_types_;
		
		for (auto mutTypeIter = mutationTypes.begin(); mutTypeIter != mutationTypes.end(); ++mutTypeIter)
		{
			MutationType *mutationType = mutTypeIter->second;
			int mutationTypeID = mutationType->mutation_type_id_;
			int mutationTypeIndex = mutationType->mutation_type_index_;
			NSString *mutationTypeString = [NSString stringWithFormat:@"m%d", mutationTypeID];
			
			[mutationTypeButton addItemWithTitle:mutationTypeString];
			lastItem = [mutationTypeButton lastItem];
			[lastItem setTag:mutationTypeIndex];
			
			// Remember the first item we add; we will use this item's tag to make a selection if needed
			if (firstTag == -1)
				firstTag = mutationTypeIndex;
		}
	}
	
	[self sortMenuItemsByTagInPopUpButton:mutationTypeButton];
	
	// If it is empty, disable it
	[mutationTypeButton setEnabled:([mutationTypeButton numberOfItems] >= 1)];
	
	// Fix the selection and then select the chosen mutation type
	NSInteger indexOfTag = [mutationTypeButton indexOfItemWithTag:[self selectedMutationTypeIndex]];
	
	if (indexOfTag == -1)
		[self setSelectedMutationTypeIndex:-1];
	if ([self selectedMutationTypeIndex] == -1)
		[self setSelectedMutationTypeIndex:firstTag];
	
	[mutationTypeButton selectItemWithTag:[self selectedMutationTypeIndex]];
	[mutationTypeButton synchronizeTitleAndSelectedItem];
}

- (void)invalidateCachedData
{
	//NSLog(@"-invalidateCachedData");
	
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

- (void)fetchDataForFinishedGeneration
{
	SLiMWindowController *controller = [self slimWindowController];
	SLiMSim *sim = controller->sim;
	Population &population = sim->population_;
	Genome &mutationRegistry = population.mutation_registry_;
	
	if (population.child_generation_valid)
	{
		NSLog(@"child_generation_valid set in fetchDataForFinishedGeneration");
		return;
	}
	
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
	//
	int subpop_total_genome_count = 0;
	
	const Mutation **registry_iter = mutationRegistry.begin_pointer();
	const Mutation **registry_iter_end = mutationRegistry.end_pointer();
	
	for (; registry_iter != registry_iter_end; ++registry_iter)
		(*registry_iter)->gui_scratch_reference_count = 0;
	
	for (const std::pair<const int,Subpopulation*> &subpop_pair : population)
	{
		if (subpop_pair.first == _selectedSubpopulationID)	// tally only within our chosen subpop
		{
			Subpopulation *subpop = subpop_pair.second;
			
			int subpop_genome_count = 2 * subpop->parent_subpop_size_;
			std::vector<Genome> &subpop_genomes = subpop->parent_genomes_;
			
			for (int i = 0; i < subpop_genome_count; i++)
			{
				Genome &genome = subpop_genomes[i];
				
				if (!genome.IsNull())
				{
					const Mutation **genome_iter = genome.begin_pointer();
					const Mutation **genome_end_iter = genome.end_pointer();
					
					for (; genome_iter != genome_end_iter; ++genome_iter)
					{
						const Mutation *mutation = *genome_iter;
						
						if (mutation->mutation_type_ptr_->mutation_type_index_ == _selectedMutationTypeIndex)
							(mutation->gui_scratch_reference_count)++;
					}
					
					subpop_total_genome_count++;
				}
			}
		}
	}
	
	// Now we can run through the mutations and use the tallies in gui_scratch_reference_count to update our histories
	for (registry_iter = mutationRegistry.begin_pointer(); registry_iter != registry_iter_end; ++registry_iter)
	{
		const Mutation *mutation = *registry_iter;
		int32_t refcount = mutation->gui_scratch_reference_count;
		
		if (refcount)
		{
			uint16 value = (uint16)((refcount * (unsigned long long)UINT16_MAX) / subpop_total_genome_count);
			NSNumber *mutationIDNumber = [[NSNumber alloc] initWithLongLong:mutation->mutation_id_];
			MutationFrequencyHistory *history = [frequencyHistoryDict objectForKey:mutationIDNumber];
			
			//NSLog(@"mutation refcount %d has uint16 value %d, found history %p for id %llu", refcount, value, history, mutation->mutation_id_);
			
			if (history)
			{
				// We have a history for this mutation, so we just need to add an entry; this sets the updated flag
				[history addEntry:value];
			}
			else
			{
				// No history, so we make one starting at this generation; this also sets the updated flag
				// Note we use sim->generation_ - 1, because the generation counter has already been advanced to the next generation
				history = [[MutationFrequencyHistory alloc] initWithEntry:value forMutation:mutation atBaseGeneration:sim->generation_ - 1];
				
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
			uint64 historyID = history->mutationID;
			const Mutation **mutation_iter = mutationRegistry.begin_pointer();
			const Mutation **mutation_iter_end = mutationRegistry.end_pointer();
			BOOL mutationStillExists = NO;
			
			for (mutation_iter = mutationRegistry.begin_pointer(); mutation_iter != mutation_iter_end; ++mutation_iter)
			{
				const Mutation *mutation = *mutation_iter;
				uint64 mutationID = mutation->mutation_id_;
				
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
			uint64 mutationID = history->mutationID;
			BOOL wasFixed = NO;
			
			std::vector<const Substitution*> &substitutions = population.substitutions_;
			
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
	
	//NSLog(@"frequencyHistoryDict has %lu entries, frequencyHistoryColdStorageLost has %lu entries, frequencyHistoryColdStorageFixed has %lu entries", (unsigned long)[frequencyHistoryDict count], (unsigned long)[frequencyHistoryColdStorageLost count], (unsigned long)[frequencyHistoryColdStorageFixed count]);
}

- (void)setSelectedSubpopulationID:(int)newID
{
	if (_selectedSubpopulationID != newID)
	{
		_selectedSubpopulationID = newID;
		
		[self invalidateCachedData];
		[self fetchDataForFinishedGeneration];
		[self setNeedsDisplay:YES];
	}
}

- (void)setSelectedMutationTypeIndex:(int)newIndex
{
	if (_selectedMutationTypeIndex != newIndex)
	{
		_selectedMutationTypeIndex = newIndex;
		
		[self invalidateCachedData];
		[self fetchDataForFinishedGeneration];
		[self setNeedsDisplay:YES];
	}
}

- (IBAction)subpopulationPopupChanged:(id)sender
{
	[self setSelectedSubpopulationID:(int)[subpopulationButton selectedTag]];
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
		if (![self xAxisIsUserRescaled])
			[self setXAxisMax:controller->sim->time_duration_];
		
		[self setNeedsDisplay:YES];
	}
	
	// Remake our popups, whether or not the controller is valid
	[self addSubpopulationsToMenu];
	[self addMutationTypesToMenu];
	
	[super controllerRecycled];
}

- (void)controllerGenerationFinished
{
	[super controllerGenerationFinished];
	
	// Fetch and store the frequencies for all mutations of the selected mutation type(s), within the subpopulation selected
	[self fetchDataForFinishedGeneration];
}

- (void)willDrawWithInteriorRect:(NSRect)interiorRect andController:(SLiMWindowController *)controller
{
	// Rebuild the subpop menu; this has the side effect of checking and fixing our subpop selection, and that,
	// in turn, will have the side effect of invaliding our cache and fetching new data if needed
	[self addSubpopulationsToMenu];
}

- (void)drawHistory:(MutationFrequencyHistory *)history inInteriorRect:(NSRect)interiorRect
{
	int entryCount = history->entryCount;
	
	if (entryCount > 1)		// one entry just generates a moveto
	{
		uint16 *entries = history->entries;
		NSBezierPath *linePath = [NSBezierPath bezierPath];
		uint16 firstValue = *entries;
		double firstFrequency = ((double)firstValue) / UINT16_MAX;
		int generation = history->baseGeneration;
		NSPoint firstPoint = NSMakePoint([self plotToDeviceX:generation withInteriorRect:interiorRect], [self plotToDeviceY:firstFrequency withInteriorRect:interiorRect]);
		
		[linePath moveToPoint:firstPoint];
		
		for (int entryIndex = 1; entryIndex < entryCount; ++entryIndex)
		{
			uint16 value = entries[entryIndex];
			double frequency = ((double)value) / UINT16_MAX;
			NSPoint nextPoint = NSMakePoint([self plotToDeviceX:++generation withInteriorRect:interiorRect], [self plotToDeviceY:frequency withInteriorRect:interiorRect]);
			
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
			[legendKey addObject:[NSArray arrayWithObjects:@"lost", [NSColor redColor], nil]];
		
		if ([self plotFixedMutations])
			[legendKey addObject:[NSArray arrayWithObjects:@"fixed", [NSColor colorWithCalibratedRed:0.4 green:0.4 blue:1.0 alpha:1.0], nil]];
		
		if ([self plotActiveMutations])
			[legendKey addObject:[NSArray arrayWithObjects:@"active", [NSColor blackColor], nil]];
		
		return legendKey;
	}
	
	return nil;
}

- (void)appendEntriesFromArray:(NSArray *)array toString:(NSMutableString *)string completedGenerations:(int)completedGenerations
{
	[array enumerateObjectsUsingBlock:^(MutationFrequencyHistory *history, NSUInteger idx, BOOL *stop) {
		int entryCount = history->entryCount;
		int baseGeneration = history->baseGeneration;
		
		for (int gen = 1; gen <= completedGenerations; ++gen)
		{
			if (gen < baseGeneration)
				[string appendString:@"NA, "];
			else if (gen - baseGeneration < entryCount)
				[string appendFormat:@"%.4f, ", ((double)history->entries[gen - baseGeneration]) / UINT16_MAX];
			else
				[string appendString:@"NA, "];
		}
		
		[string appendString:@"\n"];
	}];
}

- (NSString *)stringForDataWithController:(SLiMWindowController *)controller
{
	NSMutableString *string = [NSMutableString stringWithString:@"# Graph data: fitness trajectories\n"];
	SLiMSim *sim = controller->sim;
	int completedGenerations = sim->generation_ - 1;
	
	[string appendString:[self dateline]];
	
	if ([self plotLostMutations])
	{
		[string appendString:@"\n\n# Lost mutations:\n"];
		[self appendEntriesFromArray:frequencyHistoryColdStorageLost toString:string completedGenerations:completedGenerations];
	}
	
	if ([self plotFixedMutations])
	{
		[string appendString:@"\n\n# Fixed mutations:\n"];
		[self appendEntriesFromArray:frequencyHistoryColdStorageFixed toString:string completedGenerations:completedGenerations];
	}
	
	if ([self plotActiveMutations])
	{
		[string appendString:@"\n\n# Active mutations:\n"];
		[self appendEntriesFromArray:[frequencyHistoryDict allValues] toString:string completedGenerations:completedGenerations];
	}
	
	// Get rid of extra commas
	[string replaceOccurrencesOfString:@", \n" withString:@"\n" options:0 range:NSMakeRange(0, [string length])];
	
	return string;
}

@end


@implementation MutationFrequencyHistory

- (instancetype)initWithEntry:(uint16)value forMutation:(const Mutation *)mutation atBaseGeneration:(int)generation
{
	if (self = [super init])
	{
		mutationID = mutation->mutation_id_;
		mutationType = mutation->mutation_type_ptr_;
		baseGeneration = generation;
		
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

- (void)addEntry:(uint16)value
{
	if (entryCount == bufferSize)
	{
		// We need to expand
		bufferSize += 1024;
		entries = (uint16 *)realloc(entries, bufferSize * sizeof(uint16));
	}
	
	entries[entryCount++] = value;
	updated = YES;
}

@end



























































