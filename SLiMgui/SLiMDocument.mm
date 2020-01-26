//
//  SLiMDocument.m
//  SLiM
//
//  Created by Ben Haller on 1/31/17.
//  Copyright (c) 2017-2020 Philipp Messer.  All rights reserved.
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


#import "SLiMDocument.h"
#import "SLiMWindowController.h"
#import "SLiMDocumentController.h"


@implementation SLiMDocument

+ (NSString *)defaultWFScriptString
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

+ (NSString *)defaultNonWFScriptString
{
	static NSString *str = @"// set up a simple neutral nonWF simulation\n"
	"initialize() {\n"
	"	initializeSLiMModelType(\"nonWF\");\n"
	"	defineConstant(\"K\", 500);	// carrying capacity\n"
	"	\n"
	"	// neutral mutations, which are allowed to fix\n"
	"	initializeMutationType(\"m1\", 0.5, \"f\", 0.0);\n"
	"	m1.convertToSubstitution = T;\n"
	"	\n"
	"	initializeGenomicElementType(\"g1\", m1, 1.0);\n"
	"	initializeGenomicElement(g1, 0, 99999);\n"
	"	initializeMutationRate(1e-7);\n"
	"	initializeRecombinationRate(1e-8);\n"
	"}\n"
	"\n"
	"// each individual reproduces itself once\n"
	"reproduction() {\n"
	"	subpop.addCrossed(individual, subpop.sampleIndividuals(1));\n"
	"}\n"
	"\n"
	"// create an initial population of 10 individuals\n"
	"1 early() {\n"
	"	sim.addSubpop(\"p1\", 10);\n"
	"}\n"
	"\n"
	"// provide density-dependent selection\n"
	"early() {\n"
	"	p1.fitnessScaling = K / p1.individualCount;\n"
	"}\n"
	"\n"
	"// output all fixed mutations at end\n"
	"2000 late() { sim.outputFixedMutations(); }\n";
	
	return str;
}

- (instancetype)init
{
	if (self = [super init])
	{
		//[[self undoManager] disableUndoRegistration];
		
		if ([(SLiMDocumentController *)[NSDocumentController sharedDocumentController] creatingNonWFDocument])
			documentScriptString = [[SLiMDocument defaultNonWFScriptString] retain];
		else
			documentScriptString = [[SLiMDocument defaultWFScriptString] retain];
		
		//[[self undoManager] enableUndoRegistration];
	}
	
	return self;
}

- (void)dealloc
{
	//NSLog(@"[SLiMDocument dealloc]");
	
	[documentScriptString release];
	documentScriptString = nil;
	
	[self setRecipeName:nil];
	
	[super dealloc];
}

- (NSString *)documentScriptString
{
	return documentScriptString;
}

- (SLiMWindowController *)slimWindowController
{
	NSArray *controllers = [self windowControllers];
	
	if ([controllers count] > 0)
	{
		NSWindowController *mainController = [controllers objectAtIndex:0];
		
		if ([mainController isKindOfClass:[SLiMWindowController class]])
			return (SLiMWindowController *)mainController;
	}
	
	return nil;
}

- (void)makeWindowControllers
{
	NSArray *myControllers = [self windowControllers];
	
	// If this document displaced a transient document, it will already have been assigned
	// a window controller. If that is not the case, create one.
	if ([myControllers count] == 0)
	{
		SLiMWindowController *controller = [[[SLiMWindowController alloc] init] autorelease];
		
		[controller setShouldCloseDocument:YES];
		[self addWindowController:controller];
	}
}

- (void)windowControllerDidLoadNib:(NSWindowController *)aController
{
	// Note this method is not called, because we override -makeWindowControllers
    [super windowControllerDidLoadNib:aController];
}

- (NSData *)dataOfType:(NSString *)typeName error:(NSError **)outError
{
	if ([typeName isEqualToString:@"edu.messerlab.slim"])
	{
		SLiMWindowController *controller = [[self windowControllers] objectAtIndex:0];
		NSString *currentScriptString = [controller->scriptTextView string];
		NSData *data = [currentScriptString dataUsingEncoding:NSUTF8StringEncoding];
		
		if (!data && outError)
			*outError = [NSError errorWithDomain:NSCocoaErrorDomain code:NSFileWriteInapplicableStringEncodingError userInfo:@{ NSLocalizedDescriptionKey : @"The script could not be converted to UTF-8 encoding."}];
		
		[controller->scriptTextView breakUndoCoalescing];
		
		return data;
	}
	
    if (outError)
        *outError = [NSError errorWithDomain:NSOSStatusErrorDomain code:unimpErr userInfo:nil];
	
    return nil;
}

- (void)setDocumentScriptString:(NSString *)newString
{
	[newString retain];
	[documentScriptString release];
	documentScriptString = newString;
	
	SLiMWindowController *mainController = [self slimWindowController];
	
	if (mainController)
	{
		[mainController->scriptTextView setString:newString];
		[mainController->scriptTextView recolorAfterChanges];
		[mainController recycle:nil];
	}
}

- (BOOL)readFromData:(NSData *)data ofType:(NSString *)typeName error:(NSError **)outError
{
	if ([typeName isEqualToString:@"edu.messerlab.slim"])
	{
		NSString *scriptString = [[[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] autorelease];
		BOOL lossy = false;
		
		if (!scriptString)
			[NSString stringEncodingForData:data encodingOptions:@{NSStringEncodingDetectionSuggestedEncodingsKey: @[[NSNumber numberWithUnsignedInt:NSUTF8StringEncoding], [NSNumber numberWithUnsignedInt:NSUTF16StringEncoding], [NSNumber numberWithUnsignedInt:NSASCIIStringEncoding], [NSNumber numberWithUnsignedInt:NSISOLatin1StringEncoding]]} convertedString:&scriptString usedLossyConversion:&lossy];
		
		[self setDocumentScriptString:scriptString];
		
		return YES;
	}
	else if ([typeName isEqualToString:@"public.plain-text"])
	{
		NSString *scriptString = nil;
		BOOL lossy = false;
		
		[NSString stringEncodingForData:data encodingOptions:@{NSStringEncodingDetectionSuggestedEncodingsKey: @[[NSNumber numberWithUnsignedInt:NSUTF8StringEncoding], [NSNumber numberWithUnsignedInt:NSUTF16StringEncoding], [NSNumber numberWithUnsignedInt:NSASCIIStringEncoding], [NSNumber numberWithUnsignedInt:NSISOLatin1StringEncoding]]} convertedString:&scriptString usedLossyConversion:&lossy];
		
		[self setDocumentScriptString:scriptString];
		
		return YES;
	}
	
    if (outError) {
        *outError = [NSError errorWithDomain:NSOSStatusErrorDomain code:unimpErr userInfo:nil];
    }
    return NO;
}

- (BOOL)prepareSavePanel:(NSSavePanel *)savePanel
{
	[savePanel setTitle:@"Save Script"];
	[savePanel setMessage:@"Save the simulation script to a file:"];
	[savePanel setExtensionHidden:NO];
	[savePanel setCanSelectHiddenExtension:NO];
	
	return YES;
}

// Do our own tracking of the change count.  We do this so that we know whether the script is in
// the same state it was in when we last recycled, or has been changed.  If it has been changed,
// we add a highlight under the recycle button to suggest to the user that they might want to
// recycle to bring their changes into force.
- (void)updateChangeCount:(NSDocumentChangeType)change
{
	// When a document is changed, it ceases to be transient.
	[self setTransient:NO];
	
	[super updateChangeCount:change];
	
	// Mask off flags in the high bits.  Apple is not explicit about this, but NSChangeDiscardable
	// is 256, and acts as a flag bit, so it seems reasonable to assume this for future compatibility.
	NSDocumentChangeType maskedChange = (NSDocumentChangeType)(change & 0x00FF);
	
	if ((maskedChange == NSChangeDone) || (maskedChange == NSChangeRedone))
		slimChangeCount++;
	else if (maskedChange == NSChangeUndone)
		slimChangeCount--;
	
	[[self slimWindowController] updateRecycleHighlightForChangeCount:slimChangeCount];
}

- (BOOL)changedSinceRecycle
{
	return !(slimChangeCount == 0);
}

- (void)resetSLiMChangeCount
{
	slimChangeCount = 0;
	
	[[self slimWindowController] updateRecycleHighlightForChangeCount:slimChangeCount];
}


// Transient document support: adapted from TextEdit.  A transient document is an untitled document that
// was opened automatically. If a real document is opened before the transient document is edited, the
// real document should replace the transient. If a transient document is edited, it ceases to be transient. 

- (BOOL)isTransient
{
	return transient;
}

- (void)setTransient:(BOOL)flag
{
	transient = flag;
}

- (BOOL)isTransientAndCanBeReplaced
{
	if (![self isTransient])
		return NO;
	
	// We can't replace transient document that have sheets on them.
	for (NSWindowController *controller in [self windowControllers])
		if ([[controller window] attachedSheet])
			return NO;
	
	return YES;
}


// Opt out of all of Apple's autosaving, versioning, etc.  Partly because I dislike those features,
// partly because I don't want a user's model to be saved until they explicitly save since
// modeling often involves a lot of trial and error, partly because I'm not sure how versioning
// in OS X interacts with sharing files with Linux.

+ (BOOL)autosavesInPlace
{
    return NO;
}

+ (BOOL)autosavesDrafts
{
	return NO;
}

+ (BOOL)preservesVersions
{
	return NO;
}

@end









































