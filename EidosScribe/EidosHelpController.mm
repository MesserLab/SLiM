//
//  EidosHelpController.m
//  SLiM
//
//  Created by Ben Haller on 9/12/15.
//  Copyright (c) 2015 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/software/
//

//	This file is part of Eidos.
//
//	Eidos is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
//
//	Eidos is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License along with Eidos.  If not, see <http://www.gnu.org/licenses/>.


#import "EidosHelpController.h"
#import "EidosCocoaExtra.h"

#include "eidos_interpreter.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"


//	EidosHelpTextStorage – a little subclass to make line wrapping in the help textview work the way it should, defined below
@interface EidosHelpTextStorage : NSTextStorage
- (id)init;
- (id)initWithAttributedString:(NSAttributedString *)attrStr;
@end

//	EidosDividerTextAttachmentCell – a little subclass to provide a divider between multiple help items, defined below
@interface EidosDividerTextAttachmentCell : NSTextAttachmentCell
@end

// EidosLockingClipView – a little subclass to work around an AppKit bug, see setDescription: below
@interface EidosLockingClipView : NSClipView
@property (atomic, getter=isEidosScrollingLocked) BOOL eidosScrollingLocked;
@end


//
//	EidosHelpController
//
#pragma mark -
#pragma mark EidosHelpController

@interface EidosHelpController () <NSOutlineViewDataSource, NSOutlineViewDelegate>
{
	NSMutableDictionary *topicRoot;
	
	int searchType;		// 0 == Title, 1 == Content; equal to the tags on the search type menu items
}

@property (nonatomic, retain) IBOutlet NSWindow *helpWindow;

@property (nonatomic, retain) IBOutlet NSSearchField *searchField;
@property (nonatomic, retain) IBOutlet NSOutlineView *topicOutlineView;
@property (nonatomic, retain) IBOutlet NSTextView *descriptionTextView;

- (IBAction)searchTypeChanged:(id)sender;
- (IBAction)searchFieldChanged:(id)sender;

@end


@implementation EidosHelpController

+ (EidosHelpController *)sharedController
{
	static EidosHelpController *sharedInstance = nil;
	
	if (!sharedInstance)
		sharedInstance = [[EidosHelpController alloc] init];
	
	return sharedInstance;
}

- (instancetype)init
{
	if (self = [super init])
	{
		topicRoot = [NSMutableDictionary new];
		
		// Add Eidos topics; the only methods defined by Eidos come from EidosObjectClass
		[self addTopicsFromRTFFile:@"EidosHelpFunctions" underHeading:@"1. Eidos Functions" functions:&EidosInterpreter::BuiltInFunctions() methods:nullptr properties:nullptr];
		[self addTopicsFromRTFFile:@"EidosHelpMethods" underHeading:@"2. Eidos Methods" functions:nullptr methods:gEidos_UndefinedClassObject->Methods() properties:nullptr];
		
		//NSLog(@"topicRoot == %@", topicRoot);
	}
	
	return self;
}

// The attributed strings straight out of the RTF file need a little reformatting, since they have paragraph indents and small font sizes appropriate for print/PDF.
// This function corrects the strings by scanning over effective attribute ranges and patching in new paragraph styles and fonts.
- (void)reformatTopicItemString:(NSMutableAttributedString *)topicItemAttrString
{
	NSUInteger topicItemAttrStringLength = [topicItemAttrString length];
	NSRange topicItemAttrStringRange = NSMakeRange(0, topicItemAttrStringLength);
	NSRange effectiveRange;
	
	// Fix the paragraph styles to have no head indent
	for (NSUInteger fixIndex = 0; fixIndex < topicItemAttrStringLength; fixIndex = effectiveRange.location + effectiveRange.length)
	{
		NSParagraphStyle *pStyle = [topicItemAttrString attribute:NSParagraphStyleAttributeName atIndex:fixIndex longestEffectiveRange:&effectiveRange inRange:topicItemAttrStringRange];
		
		if (pStyle)
		{
			CGFloat oldFirstLineHeadIndent = [pStyle firstLineHeadIndent];
			
			if (oldFirstLineHeadIndent > 0)
			{
				NSMutableParagraphStyle *newPStyle = [pStyle mutableCopy];
				CGFloat oldHeadIndent = [pStyle headIndent];
				
				[newPStyle setHeadIndent:oldHeadIndent - oldFirstLineHeadIndent];
				[newPStyle setFirstLineHeadIndent:0];
				[newPStyle setTailIndent:0];
				
				[topicItemAttrString addAttribute:NSParagraphStyleAttributeName value:newPStyle range:effectiveRange];
			}
		}
	}
	
	// Fix the fonts to be a bit larger
	NSFontManager *fm = [NSFontManager sharedFontManager];
	
	for (NSUInteger fixIndex = 0; fixIndex < topicItemAttrStringLength; fixIndex = effectiveRange.location + effectiveRange.length)
	{
		NSFont *font = [topicItemAttrString attribute:NSFontAttributeName atIndex:fixIndex longestEffectiveRange:&effectiveRange inRange:topicItemAttrStringRange];
		
		if (font)
		{
			NSFont *newFont = [fm convertFont:font toSize:[font pointSize] + 2];
			
			[topicItemAttrString addAttribute:NSFontAttributeName value:newFont range:effectiveRange];
		}
	}
	
	//NSLog(@"attributed string for key %@ == %@", topicItemKey, topicItemAttrString);
}

// This is a helper method for addTopicsFromRTFFile:... that creates a new "topic dictionary" under which items will be placed, and finds the right parent
// dictionary to insert it under.  This method makes a lot of assumptions about the layout of the RTF file, such as that section number proceeds in sorted order.
- (NSMutableDictionary *)topicDictForSection:(NSString *)sectionString title:(NSString *)title currentTopicDicts:(NSMutableDictionary *)topics topDict:(NSMutableDictionary *)topLevelDict
{
	NSArray *sectionComponents = [sectionString componentsSeparatedByString:@"."];
	NSUInteger sectionCount = [sectionComponents count];
	NSMutableDictionary *newTopicDict = [NSMutableDictionary dictionary];
	
	if ([title hasSuffix:@" functions"])
		title = [title substringToIndex:[title length] - [@" functions" length]];
	
	NSString *numberedTitle = [NSString stringWithFormat:@"%@. %@", [sectionComponents lastObject], title];
	
	if (sectionCount <= 1)
	{
		// With an empty section string, or a whole-number section like "3", we always make a new topic dict at the top level; we have no parent
		[topLevelDict setObject:newTopicDict forKey:numberedTitle];
		[topics setObject:newTopicDict forKey:sectionString];
	}
	else if (sectionCount > 1)
	{
		// We have a section string like "3.1" or "3.1.2"; we want to look for a parent to add it to – like "3" or "3.1", respectively
		NSArray *parentSectionComponents = [sectionComponents subarrayWithRange:NSMakeRange(0, [sectionComponents count] - 1)];
		NSString *parentSectionString = [parentSectionComponents componentsJoinedByString:@"."];
		NSMutableDictionary *parentTopicDict = [topics objectForKey:parentSectionString];
		
		if (parentTopicDict)
		{
			// Found a parent to add to, so add there
			[parentTopicDict setObject:newTopicDict forKey:numberedTitle];
			[topics setObject:newTopicDict forKey:sectionString];
		}
		else
		{
			// Couldn't find a parent to add to, so we add to the top level; this is not an error, because the doc RTF files are just inner subsections of the manual
			[topLevelDict setObject:newTopicDict forKey:numberedTitle];
			[topics setObject:newTopicDict forKey:sectionString];
		}
	}
	
	return newTopicDict;
}

- (NSMutableDictionary *)effectiveTopicRoot
{
	NSMutableDictionary *effectiveTopicRoot = topicRoot;
	
	// If the topic root contains only a single key, collapse that top level and consider the object for that key to be the searchDict
	if ([effectiveTopicRoot count] == 1)
	{
		id topValue = [[effectiveTopicRoot allValues] objectAtIndex:0];
		
		if ([topValue isKindOfClass:[NSDictionary class]])
			effectiveTopicRoot = topValue;
	}
	
	return effectiveTopicRoot;
}

// This is the main RTF doc file reading method; it finds an RTF file of a given name in the main bundle, reads it into an attributed string, and then scans
// that string for topic headings, function/method/property signature lines, etc., and creates a hierarchy of help topics from the results.  This process
// assumes that the RTF doc file is laid out in a standard way that fits the regex patterns used here; it is designed to work directly with content copied
// and pasted out of our Word documentation files into RTF in TextEdit.
- (void)addTopicsFromRTFFile:(NSString *)rtfFile underHeading:(NSString *)topLevelHeading functions:(const std::vector<const EidosFunctionSignature *> *)functionList methods:(const std::vector<const EidosMethodSignature*> *)methodList properties:(const std::vector<const EidosPropertySignature*> *)propertyList
{
	NSString *topicFilePath = [[NSBundle mainBundle] pathForResource:rtfFile ofType:@"rtf"];
	NSData *topicFileData = [NSData dataWithContentsOfFile:topicFilePath];
	NSAttributedString *topicFileAttrString = [[[NSAttributedString alloc] initWithRTF:topicFileData documentAttributes:NULL] autorelease];
	
	// Set up the top-level dictionary that we will place items under
	NSMutableDictionary *topLevelDict = [NSMutableDictionary dictionary];
	NSMutableDictionary *topics = [NSMutableDictionary dictionary];			// keys are strings like 3.1 or 3.1.2 or whatever
	NSMutableDictionary *currentTopicDict = topLevelDict;					// start out putting new topics in the top level dict
	
	[topicRoot setObject:topLevelDict forKey:topLevelHeading];
	
	// Set up the current topic item that we are appending content into
	NSString *topicItemKey = nil;
	NSMutableAttributedString *topicItemAttrString = nil;
	
	// Make regular expressions that we will use below
	NSRegularExpression *topicHeaderRegex = [NSRegularExpression regularExpressionWithPattern:@"^((?:[0-9]+\\.)*[0-9]+)\\.?  (.+)$" options:NSRegularExpressionCaseInsensitive error:NULL];									// 2 captures
	NSRegularExpression *topicFunctionRegex = [NSRegularExpression regularExpressionWithPattern:@"^\\([a-zA-Z<>\\*+$]+\\)([a-zA-Z_0-9]+)\\(.+$" options:NSRegularExpressionCaseInsensitive error:NULL];						// 1 capture
	NSRegularExpression *topicMethodRegex = [NSRegularExpression regularExpressionWithPattern:@"^([-–+])[  ]\\([a-zA-Z<>\\*+$]+\\)([a-zA-Z_0-9]+)\\(.+$" options:NSRegularExpressionCaseInsensitive error:NULL];			// 2 captures
	NSRegularExpression *topicPropertyRegex = [NSRegularExpression regularExpressionWithPattern:@"^([a-zA-Z_0-9]+)[  ]((?:<[-–]>)|(?:=>)) \\([a-zA-Z<>\\*+$]+\\)$" options:NSRegularExpressionCaseInsensitive error:NULL];		// 2 captures
	
	// Scan through the file one line at a time, parsing out topic headers
	NSString *topicFileString = [topicFileAttrString string];
	NSArray *topicFileLineArray = [topicFileString componentsSeparatedByString:@"\n"];
	NSUInteger lineCount = [topicFileLineArray count];
	NSUInteger lineStartIndex = 0;		// the character index of the current line in topicFileAttrString
	
	for (int lineIndex = 0; lineIndex < lineCount; ++lineIndex)
	{
		NSString *line = [topicFileLineArray objectAtIndex:lineIndex];
		NSUInteger lineLength = [line length];
		NSRange lineRange = NSMakeRange(0, lineLength);
		NSRange lineRangeInAttrString = NSMakeRange(lineStartIndex, lineLength);
		NSAttributedString *lineAttrString = [topicFileAttrString attributedSubstringFromRange:lineRangeInAttrString];
		
		// on the assumption (usually true) that we will need the corrected attributed string for this line, we reformat it now
		{
			NSMutableAttributedString *correctedLineAttrString = [[lineAttrString mutableCopy] autorelease];
			
			[self reformatTopicItemString:correctedLineAttrString];
			lineAttrString = correctedLineAttrString;
		}
		
		// figure out what kind of line we have and handle it
		lineStartIndex += (lineLength + 1);		// +1 to jump over the newline
		
		NSUInteger isTopicHeaderLine = ([topicHeaderRegex numberOfMatchesInString:line options:(NSMatchingOptions)0 range:lineRange] > 0);
		NSUInteger isTopicFunctionLine = ([topicFunctionRegex numberOfMatchesInString:line options:(NSMatchingOptions)0 range:lineRange] > 0);
		NSUInteger isTopicMethodLine = ([topicMethodRegex numberOfMatchesInString:line options:(NSMatchingOptions)0 range:lineRange] > 0);
		NSUInteger isTopicPropertyLine = ([topicPropertyRegex numberOfMatchesInString:line options:(NSMatchingOptions)0 range:lineRange] > 0);
		NSUInteger matchCount = isTopicHeaderLine + isTopicFunctionLine + isTopicMethodLine + isTopicPropertyLine;
		
		if (matchCount > 1)
		{
			NSLog(@"*** line matched more than one regex type: %@", line);
			return;
		}
		
		if (matchCount == 0)
		{
			if ([line length])
			{
				// This is a content line, to be appended to the current topic item's content
				[topicItemAttrString replaceCharactersInRange:NSMakeRange([topicItemAttrString length], 0) withString:@"\n"];
				[topicItemAttrString replaceCharactersInRange:NSMakeRange([topicItemAttrString length], 0) withAttributedString:lineAttrString];
			}
		}
		
		if ((matchCount == 1) || ((matchCount == 0) && (lineIndex == lineCount - 1)))
		{
			// This line starts a new header or item or ends the file, so we need to terminate the current item
			if (topicItemAttrString && topicItemKey)
			{
				[currentTopicDict setObject:topicItemAttrString forKey:topicItemKey];
				
				topicItemAttrString= nil;
				topicItemKey = nil;
			}
		}
		
		if (isTopicHeaderLine)
		{
			// We have hit a new topic header.  This might be a subtopic of the current topic, or a sibling, or a sibling of one of our ancestors
			NSTextCheckingResult *match = [topicHeaderRegex firstMatchInString:line options:(NSMatchingOptions)0 range:lineRange];
			NSRange sectionRange = [match rangeAtIndex:1];
			NSString *sectionString = [line substringWithRange:sectionRange];
			NSRange titleRange = [match rangeAtIndex:2];
			NSString *titleString = [line substringWithRange:titleRange];
			
			//NSLog(@"topic header section %@, title %@, line: %@", sectionString, titleString, line);
			
			currentTopicDict = [self topicDictForSection:sectionString title:titleString currentTopicDicts:topics topDict:topLevelDict];
		}
		else if (isTopicFunctionLine)
		{
			// We have hit a new topic item line.  This will become a key in the current topic dictionary.
			NSTextCheckingResult *match = [topicFunctionRegex firstMatchInString:line options:(NSMatchingOptions)0 range:lineRange];
			NSRange callNameRange = [match rangeAtIndex:1];
			NSString *callName = [line substringWithRange:callNameRange];
			
			//NSLog(@"topic function name: %@, line: %@", callName, line);
			
			// check for a built-in function signature that matches and substitute it in
			if (functionList)
			{
				std::string function_name([callName UTF8String]);
				const EidosFunctionSignature *function_signature = nullptr;
				
				for (auto signature_iter = functionList->begin(); signature_iter != functionList->end(); signature_iter++)
					if ((*signature_iter)->function_name_.compare(function_name) == 0)
					{
						function_signature = *signature_iter;
						break;
					}
				
				if (function_signature)
				{
					NSAttributedString *attrSig = [NSAttributedString eidosAttributedStringForCallSignature:function_signature];
					NSString *oldSignatureString = [lineAttrString string];
					NSString *newSignatureString = [attrSig string];
					
					if ([oldSignatureString isEqualToString:newSignatureString])
					{
						//NSLog(@"signature match for function %@", callName);
						
						// Replace the signature line from the RTF file with the syntax-colored version
						lineAttrString = attrSig;
					}
					else
					{
						NSLog(@"*** function signature mismatch:\nold: %@\nnew: %@", oldSignatureString, newSignatureString);
					}
				}
				else
				{
					NSLog(@"*** no function signature found for function name %@", callName);
				}
			}
			
			topicItemKey = [callName stringByAppendingString:@"()"];
			topicItemAttrString = [[[NSMutableAttributedString alloc] initWithAttributedString:lineAttrString] autorelease];
		}
		else if (isTopicMethodLine)
		{
			// We have hit a new topic item line.  This will become a key in the current topic dictionary.
			NSTextCheckingResult *match = [topicMethodRegex firstMatchInString:line options:(NSMatchingOptions)0 range:lineRange];
			NSRange classMethodRange = [match rangeAtIndex:1];
			NSString *classMethodString = [line substringWithRange:classMethodRange];
			NSRange callNameRange = [match rangeAtIndex:2];
			NSString *callName = [line substringWithRange:callNameRange];
			
			//NSLog(@"topic method name: %@, line: %@", callName, line);
			
			// check for a built-in method signature that matches and substitute it in
			if (methodList)
			{
				std::string method_name([callName UTF8String]);
				const EidosMethodSignature *method_signature = nullptr;
				
				for (auto signature_iter = methodList->begin(); signature_iter != methodList->end(); signature_iter++)
					if ((*signature_iter)->function_name_.compare(method_name) == 0)
					{
						method_signature = *signature_iter;
						break;
					}
				
				if (method_signature)
				{
					NSAttributedString *attrSig = [NSAttributedString eidosAttributedStringForCallSignature:method_signature];
					NSString *oldSignatureString = [lineAttrString string];
					NSString *newSignatureString = [attrSig string];
					
					if ([oldSignatureString isEqualToString:newSignatureString])
					{
						//NSLog(@"signature match for method %@", callName);
						
						// Replace the signature line from the RTF file with the syntax-colored version
						lineAttrString = attrSig;
					}
					else
					{
						NSLog(@"*** method signature mismatch:\nold: %@\nnew: %@", oldSignatureString, newSignatureString);
					}
				}
				else
				{
					NSLog(@"*** no method signature found for method name %@", callName);
				}
			}
			
			topicItemKey = [NSString stringWithFormat:@"%@ %@()", classMethodString, callName];
			topicItemAttrString = [[[NSMutableAttributedString alloc] initWithAttributedString:lineAttrString] autorelease];
		}
		else if (isTopicPropertyLine)
		{
			// We have hit a new topic item line.  This will become a key in the current topic dictionary.
			NSTextCheckingResult *match = [topicPropertyRegex firstMatchInString:line options:(NSMatchingOptions)0 range:lineRange];
			NSRange callNameRange = [match rangeAtIndex:1];
			NSString *callName = [line substringWithRange:callNameRange];
			NSRange readOnlyRange = [match rangeAtIndex:2];
			NSString *readOnlyName = [line substringWithRange:readOnlyRange];
			
			//NSLog(@"topic property name: %@, line: %@", callName, line);
			
			// check for a built-in method signature that matches and substitute it in
			if (propertyList)
			{
				std::string property_name([callName UTF8String]);
				const EidosPropertySignature *property_signature = nullptr;
				
				for (auto signature_iter = propertyList->begin(); signature_iter != propertyList->end(); signature_iter++)
					if ((*signature_iter)->property_name_.compare(property_name) == 0)
					{
						property_signature = *signature_iter;
						break;
					}
				
				if (property_signature)
				{
					NSAttributedString *attrSig = [NSAttributedString eidosAttributedStringForPropertySignature:property_signature];
					NSString *oldSignatureString = [lineAttrString string];
					NSString *newSignatureString = [attrSig string];
					
					if ([oldSignatureString isEqualToString:newSignatureString])
					{
						//NSLog(@"signature match for method %@", callName);
						
						// Replace the signature line from the RTF file with the syntax-colored version
						lineAttrString = attrSig;
					}
					else
					{
						NSLog(@"*** property signature mismatch:\nold: %@\nnew: %@", oldSignatureString, newSignatureString);
					}
				}
				else
				{
					NSLog(@"*** no property signature found for property name %@", callName);
				}
			}
			
			topicItemKey = [NSString stringWithFormat:@"%@ %@", callName, readOnlyName];
			topicItemAttrString = [[[NSMutableAttributedString alloc] initWithAttributedString:lineAttrString] autorelease];
		}
	}
}

- (NSWindow *)window
{
	if (!_helpWindow)
	{
		[[NSBundle mainBundle] loadNibNamed:@"EidosHelpWindow" owner:self topLevelObjects:NULL];
		
		// Replace the text storage of the description textview with our custom subclass
		[[_descriptionTextView layoutManager] replaceTextStorage:[[[EidosHelpTextStorage alloc] init] autorelease]];
		
		// Fix text container insets to look a bit nicer; {0,0} by default
		[_descriptionTextView setTextContainerInset:NSMakeSize(0.0, 5.0)];
		
		// Fix outline view to expand/collapse with a click anywhere in a topic row
		[_topicOutlineView setTarget:self];
		[_topicOutlineView setAction:@selector(outlineViewClicked:)];
	}
	
	return _helpWindow;
}

- (void)showWindow
{
	[[self window] makeKeyAndOrderFront:nil];
}

- (void)outlineViewClicked:(id)sender
{
	if (sender == _topicOutlineView)
	{
		id clickItem = [sender itemAtRow:[_topicOutlineView clickedRow]];
		
		if (clickItem)
		{
			BOOL optionPressed = (([[NSApp currentEvent] modifierFlags] & NSAlternateKeyMask) == NSAlternateKeyMask);
			
			if ([_topicOutlineView isItemExpanded:clickItem])
				[_topicOutlineView.animator collapseItem:clickItem collapseChildren:optionPressed];
			else
				[_topicOutlineView.animator expandItem:clickItem expandChildren:optionPressed];
		}
	}
}

- (IBAction)searchTypeChanged:(id)sender
{
	NSMenuItem *senderMenuItem = (NSMenuItem *)sender;
	int newSearchType = (int)[senderMenuItem tag];
	
	if (newSearchType != searchType)
	{
		searchType = newSearchType;
		
		[self searchFieldChanged:_searchField];
	}
}

- (BOOL)findItemsUnderRoot:(NSDictionary *)root withKey:(NSString *)rootKey matchingSearchString:(NSString *)searchString titlesOnly:(BOOL)titlesOnly addingToMatchKeys:(NSMutableArray *)matchKeys andItemsToExpand:(NSMutableArray *)expandItems
{
	__block BOOL anyChildMatches = NO;
	
	[root enumerateKeysAndObjectsUsingBlock:^(id key, id obj, BOOL *stop) {
		if ([obj isKindOfClass:[NSDictionary class]])
		{
			// If the object for the key is a dictionary, the child is an expandable item, so we need to recurse
			BOOL result = [self findItemsUnderRoot:obj withKey:key matchingSearchString:searchString titlesOnly:titlesOnly addingToMatchKeys:matchKeys andItemsToExpand:expandItems];
			
			if (result)
				anyChildMatches = YES;
		}
		else
		{
			// If the item is not a dictionary, then the child is a leaf, so we need to test for a match
			BOOL isMatch = NO;
			
			if ([key rangeOfString:searchString options:NSCaseInsensitiveSearch].location != NSNotFound)
				isMatch = YES;
			
			if (!titlesOnly)
			{
				if ([obj isKindOfClass:[NSString class]])
				{
					if ([obj rangeOfString:searchString options:NSCaseInsensitiveSearch].location != NSNotFound)
						isMatch = YES;
				}
				else if ([obj isKindOfClass:[NSAttributedString class]])
				{
					if ([[obj string] rangeOfString:searchString options:NSCaseInsensitiveSearch].location != NSNotFound)
						isMatch = YES;
				}
			}
			
			if (isMatch)
			{
				[matchKeys addObject:key];
				anyChildMatches = YES;
			}
		}
	}];
	
	if (anyChildMatches && rootKey)
		[expandItems addObject:rootKey];
	
	return anyChildMatches;
}

- (IBAction)searchFieldChanged:(id)sender
{
	NSString *searchString = [_searchField stringValue];
	
	// Do a depth-first search under the topic root that matches the search pattern, and gather tasks to perform
	NSMutableArray *matchKeys = [NSMutableArray array];
	NSMutableArray *expandItems = [NSMutableArray array];
	
	[self findItemsUnderRoot:[self effectiveTopicRoot] withKey:nil matchingSearchString:searchString titlesOnly:(searchType == 0) addingToMatchKeys:matchKeys andItemsToExpand:expandItems];
	
	if ([matchKeys count])
	{
		// First collapse everything, as an initial state
		[_topicOutlineView collapseItem:nil collapseChildren:YES];
		
		// Expand all nodes that have a search hit; reverse order so parents expand before their children
		[expandItems enumerateObjectsWithOptions:NSEnumerationReverse usingBlock:^(id obj, NSUInteger idx, BOOL *stop) {
			[_topicOutlineView expandItem:obj];
		}];
		
		// Select all of the items that matched
		[matchKeys enumerateObjectsUsingBlock:^(id obj, NSUInteger idx, BOOL *stop) {
			[_topicOutlineView selectRowIndexes:[NSIndexSet indexSetWithIndex:[_topicOutlineView rowForItem:obj]] byExtendingSelection:YES];
		}];
	}
	else
	{
		NSBeep();
	}
}

- (void)enterSearchForString:(NSString *)searchString titlesOnly:(BOOL)titlesOnly
{
	// Load our nib if it is not already loaded
	[self window];
	
	// Set the search string per the request
	[_searchField setStringValue:searchString];
	
	// Set the search type per the request
	searchType = titlesOnly ? 0 : 1;
	
	// Then execute the search by firing the search field's action
	[self searchFieldChanged:_searchField];
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem
{
	SEL selector = [menuItem action];
	
	if (selector == @selector(searchTypeChanged:))
	{
		NSInteger tag = [menuItem tag];
		
		[menuItem setState:(searchType == tag) ? NSOnState : NSOffState];
		return YES;
	}
	
	return YES;	// no super
}

- (id)findObjectForKey:(NSString *)searchKey withinDictionary:(NSDictionary *)searchDict
{
	__block id value = nullptr;
	
	if (value)
		return value;
	
	[searchDict enumerateKeysAndObjectsUsingBlock:^(id key, id obj, BOOL *stop) {
		// Search by pointer equality, not by hash/isEqual:, so that items with identical (but not pointer-equal) keys
		// can refer to different values in our topic tree; the "subpopID" property, for example, has the same name
		// in both Mutation and Substitution, but has different descriptions.
		if (key == searchKey)
			value = obj;
		else if ([obj isKindOfClass:[NSDictionary class]])
			value = [self findObjectForKey:searchKey withinDictionary:obj];
		
		if (value)
			*stop = YES;
	}];
	
	return value;
}

- (id)findObjectForKey:(NSString *)searchKey
{
	// The datasource methods below deal in keys; the child returned at a given index is an NSString key.  We often need
	// to find the corresponding value for a given key, but this is not simple since we don't have a reference to the
	// dictionary that it is a key within.  So we do a depth-first search through our topic tree to find it.  This is
	// going to be a bit slow, but the topic tree should be small so it shouldn't matter.  The alternative would be to
	// develop a better data structure for storing our topics, rather than using NSDictionary.
	
	// Start at the topic root
	NSDictionary *searchDict = [self effectiveTopicRoot];
	
	// If the search key is nil, return the searchDict; this makes the searchDict correspond to the root object of the outline view
		if (!searchKey)
			return searchDict;
	
	// Otherwise, search within the searchDict
	return [self findObjectForKey:searchKey withinDictionary:searchDict];
}


//
//	NSOutlineView datasource / delegate methods
//
#pragma mark NSOutlineView delegate

- (NSInteger)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item
{
	// According to our design, outline items are always NSStrings, and we have to look up their values in our topic tree
	id itemValue = [self findObjectForKey:item];
	
	if ([itemValue isKindOfClass:[NSDictionary class]])
		return [itemValue count];
	
	return 0;
}

- (id)outlineView:(NSOutlineView *)outlineView child:(NSInteger)index ofItem:(id)item
{
	// According to our design, outline items are always NSStrings, and we have to look up their values in our topic tree
	id itemValue = [self findObjectForKey:item];
	
	if ([itemValue isKindOfClass:[NSDictionary class]])
		return [[[itemValue allKeys] sortedArrayUsingSelector:@selector(localizedStandardCompare:)] objectAtIndex:index];
	
	return nil;
}

- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item
{
	// According to our design, outline items are always NSStrings, and we have to look up their values in our topic tree
	id itemValue = [self findObjectForKey:item];
	
	if ([itemValue isKindOfClass:[NSDictionary class]])
		return YES;
	
	return NO;
}

- (id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item
{
	if (item)
	{
		static NSRegularExpression *sortOrderStripRegex = nullptr;
		BOOL isString = [item isKindOfClass:[NSString class]];
		NSString *itemString = isString ? item : [item string];
		
		if (!sortOrderStripRegex)
			sortOrderStripRegex = [[NSRegularExpression regularExpressionWithPattern:@"^(?:[0-9]+\\. )(.+)$" options:NSRegularExpressionCaseInsensitive error:NULL] retain];
		
		NSTextCheckingResult *match = [sortOrderStripRegex firstMatchInString:itemString options:(NSMatchingOptions)0 range:NSMakeRange(0, [itemString length])];
		
		if (match)
		{
			// If the item starts with a sort-order prefix, strip it off for display
			NSRange captureRange = [match rangeAtIndex:1];
			
			if (isString)
				return [item substringWithRange:captureRange];
			else
				return [item attributedSubstringFromRange:captureRange];
		}
		
		return item;
	}
	
	return @"";
}

- (BOOL)outlineView:(NSOutlineView *)outlineView isGroupItem:(id)item
{
	// We want items under the true topic root to look like groups; if we have collapsed the top level, we have no group items
	if ([topicRoot objectForKey:item])
		return YES;
	
	return NO;
}

- (BOOL)outlineView:(NSOutlineView *)outlineView shouldSelectItem:(id)item
{
	return ![self outlineView:outlineView isItemExpandable:item];
}

- (void)lockClipView:(id)sender
{
	// See comment in setDescription: below
	NSView *superview = [_descriptionTextView superview];
	
	if ([superview isKindOfClass:[EidosLockingClipView class]])
		[(EidosLockingClipView *)superview setEidosScrollingLocked:YES];
}

- (void)unlockClipView:(id)sender
{
	// See comment in setDescription: below
	NSView *superview = [_descriptionTextView superview];
	
	if ([superview isKindOfClass:[EidosLockingClipView class]])
		[(EidosLockingClipView *)superview setEidosScrollingLocked:NO];
}

- (void)setDescription:(NSAttributedString *)newDescription
{
	NSTextStorage *ts = [_descriptionTextView textStorage];
	
	[ts beginEditing];
	[ts setAttributedString:newDescription];
	[ts endEditing];
	
	// scroll to where we actually want to be and then lock it down
	[_descriptionTextView scrollPoint:NSZeroPoint];
	[self lockClipView:nil];
	
	// NSTextView will scroll the content down just slightly, annoyingly, if the content is too long to fit in the view;
	// to prevent this, we have to subclass NSClipView, as far as I can tell (forcing layout here does not work because
	// more work is still done by background layout later that moves the scroll point, and doing a delayed perform to
	// fix the scroll position is both unreliable and causes a visible jump).  Ugh.
	[self performSelector:@selector(unlockClipView:) withObject:nil afterDelay:0.1];
}

- (void)outlineViewSelectionDidChange:(NSNotification *)notification
{
	NSOutlineView *outlineView = [notification object];
	
	if (outlineView == _topicOutlineView)
	{
		NSIndexSet *selectedIndices = [_topicOutlineView selectedRowIndexes];
		NSUInteger indexCount = [selectedIndices count];
		
		if (indexCount == 0)
		{
			// Empty selection, so clear the description textview
			[_descriptionTextView setString:@""];
		}
		else if (indexCount == 1)
		{
			// Single-topic selection, so show the topic's description
			NSString *selectedKey = [_topicOutlineView itemAtRow:[selectedIndices firstIndex]];
			id selectedValue = [self findObjectForKey:selectedKey];
			
			if ([selectedValue isKindOfClass:[NSString class]])
			{
				[_descriptionTextView setString:selectedValue];
			}
			else if ([selectedValue isKindOfClass:[NSAttributedString class]])
			{
				[self setDescription:(NSAttributedString *)selectedValue];
			}
		}
		else
		{
			// Multiple items are selected, so we want to put together an attributed string that shows them all, with dividers
			__block NSMutableAttributedString *conglomerate = [[[NSMutableAttributedString alloc] initWithString:@""] autorelease];
			__block BOOL firstIndex = YES;
			
			[selectedIndices enumerateIndexesUsingBlock:^(NSUInteger idx, BOOL *stop) {
				NSString *selectedKey = [_topicOutlineView itemAtRow:idx];
				id selectedValue = [self findObjectForKey:selectedKey];
				
				if ([selectedValue isKindOfClass:[NSAttributedString class]])
				{
					if (!firstIndex)
					{
						NSTextAttachment *attachment = [[[NSTextAttachment alloc] init] autorelease];
						EidosDividerTextAttachmentCell *dividerCell = [[[EidosDividerTextAttachmentCell alloc] init] autorelease];
						
						[attachment setAttachmentCell:dividerCell];
						
						NSAttributedString *attachmentString = [NSAttributedString attributedStringWithAttachment:attachment];
						
						[conglomerate replaceCharactersInRange:NSMakeRange([conglomerate length], 0) withString:@"\n"];
						[conglomerate replaceCharactersInRange:NSMakeRange([conglomerate length], 0) withAttributedString:attachmentString];
						[conglomerate replaceCharactersInRange:NSMakeRange([conglomerate length], 0) withString:@"\n"];
					}
					
					firstIndex = NO;
					
					[conglomerate replaceCharactersInRange:NSMakeRange([conglomerate length], 0) withAttributedString:selectedValue];
				}
			}];
			
			[self setDescription:conglomerate];
		}
	}
}

@end


//
//	EidosHelpTextStorage
//
#pragma mark -
#pragma mark EidosHelpTextStorage

@interface EidosHelpTextStorage ()
{
	NSMutableAttributedString *contents;
}
@end

@implementation EidosHelpTextStorage

- (id)initWithAttributedString:(NSAttributedString *)attrStr
{
	if (self = [super init])
	{
		contents = attrStr ? [attrStr mutableCopy] : [[NSMutableAttributedString alloc] init];
	}
	return self;
}

- init
{
	return [self initWithAttributedString:nil];
}

- (void)dealloc
{
	[contents release];
	[super dealloc];
}

// The next set of methods are the primitives for attributed and mutable attributed string...

- (NSString *)string
{
	return [contents string];
}

- (NSDictionary *)attributesAtIndex:(NSUInteger)location effectiveRange:(NSRange *)range
{
	return [contents attributesAtIndex:location effectiveRange:range];
}

- (void)replaceCharactersInRange:(NSRange)range withString:(NSString *)str
{
	NSUInteger origLen = [self length];
	[contents replaceCharactersInRange:range withString:str];
	[self edited:NSTextStorageEditedCharacters range:range changeInLength:[self length] - origLen];
}

- (void)setAttributes:(NSDictionary *)attrs range:(NSRange)range
{
	[contents setAttributes:attrs range:range];
	[self edited:NSTextStorageEditedCharacters range:range changeInLength:0];
}

// And now the actual reason for this subclass: to provide code-aware line break points

- (NSUInteger)lineBreakBeforeIndex:(NSUInteger)index withinRange:(NSRange)aRange
{
	NSUInteger breakpoint = [super lineBreakBeforeIndex:index withinRange:aRange];
	NSString *string = [self string];
	
	// until we hit the beginning of the string, demand earlier breakpoints if the breakpoint we're given splits in the middle of a type identifier
	while ((breakpoint != NSNotFound) && (breakpoint > aRange.location) && (breakpoint < aRange.location + aRange.length))
	{
		unichar uch1 = [string characterAtIndex:breakpoint - 1];
		unichar uch2 = [string characterAtIndex:breakpoint];
		
		if (uch1 != ' ')
		{
			static BOOL beenHere = NO;
			static unichar nbs = 'x';
			
			if (!beenHere)
			{
				beenHere = YES;
				nbs = [@" " characterAtIndex:0];		// " " is an NBS; no platform-independent way to represent it as a unichar literal, apparently, due to endianness etc.
			}
			
			if ((uch2 == '$') || (uch2 == nbs))
			{
				breakpoint = [super lineBreakBeforeIndex:breakpoint withinRange:aRange];
				continue;
			}
		}
		
		break;
	}
	
	// if we failed to find a breakpoint, allow breaking after a non-breaking space; this is better than forcing a mid-word break
	if (((breakpoint == NSNotFound) || (breakpoint == aRange.location)) && (index > aRange.location))
	{
		NSRange nbsSearchRange;
		
		if (index - aRange.location < aRange.length)
			nbsSearchRange = NSMakeRange(aRange.location, index - aRange.location);	// don't include index
		else
			nbsSearchRange = NSMakeRange(aRange.location, aRange.length - 1);		// don't include last character
		
		// break after a non-breaking space if necessary, but not right at the start; avoid "- ("
		NSRange nbsRange = [string rangeOfString:@" " options:(NSStringCompareOptions)(NSLiteralSearch | NSBackwardsSearch) range:nbsSearchRange];
		
		if ((nbsRange.location != NSNotFound) && (nbsRange.location > aRange.location + 2))
			breakpoint = nbsRange.location + 1;
		else
		{
			// If that didn't work, break after an opening parenthesis if necessary, but not right at the start; avoid "- ("
			nbsRange = [string rangeOfString:@"(" options:(NSStringCompareOptions)(NSLiteralSearch | NSBackwardsSearch) range:nbsSearchRange];
			
			if ((nbsRange.location != NSNotFound) && (nbsRange.location > aRange.location + 3))
				breakpoint = nbsRange.location + 1;
			else
			{
				// If that didn't work, break after a closing parenthesis if necessary
				nbsRange = [string rangeOfString:@")" options:(NSStringCompareOptions)(NSLiteralSearch | NSBackwardsSearch) range:nbsSearchRange];
				
				if ((nbsRange.location != NSNotFound) && (nbsRange.location != aRange.location))
					breakpoint = nbsRange.location + 1;
			}
		}
	}
	
	return breakpoint;
}

@end


//
//	EidosDividerTextAttachmentCell
//
#pragma mark -
#pragma mark EidosDividerTextAttachmentCell

@interface EidosDividerTextAttachmentCell ()
@end

@implementation EidosDividerTextAttachmentCell

- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView *)controlView
{
	const CGFloat widthFraction = 0.85;
	const CGFloat spaceFraction = 1.0 - widthFraction;
	CGFloat cellLeft = cellFrame.origin.x;
	CGFloat cellRight = cellFrame.origin.x + cellFrame.size.width;
	CGFloat weightedLeft = round(cellLeft * widthFraction + cellRight * spaceFraction);
	CGFloat weightedRight = round(cellLeft * spaceFraction + cellRight * widthFraction);
	CGFloat middleHeight = round(cellFrame.origin.y + cellFrame.size.height * 0.5 + 1.0);
	NSRect dividerRect = NSMakeRect(weightedLeft, middleHeight, weightedRight - weightedLeft, 2);
	
	[[NSColor colorWithCalibratedWhite:0.4 alpha:1.0] set];
	NSRectFill(dividerRect);
	
	[[NSColor colorWithCalibratedWhite:0.7 alpha:1.0] set];
	NSRectFill(NSMakeRect(dividerRect.origin.x + 1, dividerRect.origin.y + 1, dividerRect.size.width - 1, 1));
}

- (void)highlight:(BOOL)flag withFrame:(NSRect)cellFrame inView:(NSView *)controlView
{
	[self drawWithFrame:cellFrame inView:controlView];
}

- (NSRect)cellFrameForTextContainer:(NSTextContainer *)textContainer proposedLineFragment:(NSRect)lineFrag glyphPosition:(NSPoint)position characterIndex:(NSUInteger)charIndex
{
	// the -10.0 appears to be necessary because the text container inset is not subtracted out of the lineFrag rect; is this an AppKit bug?
	return NSMakeRect(0, 0, lineFrag.size.width - 10.0, lineFrag.size.height);
}

- (BOOL)wantsToTrackMouse
{
	return NO;
}

@end


//
//	EidosLockingClipView
//
#pragma mark -
#pragma mark EidosLockingClipView

@implementation EidosLockingClipView

- (NSRect)constrainBoundsRect:(NSRect)proposedBounds
{
	NSRect modifiedBounds = [super constrainBoundsRect:proposedBounds];
	
	if (_eidosScrollingLocked)
		modifiedBounds.origin.y = 0.0;
	
	//NSLog(@"proposedBounds == %@, modifiedBounds == %@, locking is %@", NSStringFromRect(proposedBounds), NSStringFromRect(modifiedBounds), _eidosScrollingLocked ? @"ON" : @"OFF");
	
	return modifiedBounds;
}

@end











































