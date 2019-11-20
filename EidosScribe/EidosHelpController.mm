//
//  EidosHelpController.m
//  SLiM
//
//  Created by Ben Haller on 9/12/15.
//  Copyright (c) 2015-2019 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/slim/
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

#include <vector>
#include <algorithm>
#include <string>
#include <cctype>


// BCH 4/7/2016: So we can build against the OS X 10.9 SDK
#ifndef NS_DESIGNATED_INITIALIZER
#define NS_DESIGNATED_INITIALIZER
#endif


// EidosHelpOutlineView – this lets us colorize rows in the outline, and search for all rows matching an item
@interface EidosHelpOutlineView : NSOutlineView
- (NSIndexSet *)eidosRowIndicesForItem:(id)item;
@end

// EidosHelpTextStorage – a little subclass to make line wrapping in the help textview work the way it should, defined below
@interface EidosHelpTextStorage : NSTextStorage
- (id)init;
- (id)initWithAttributedString:(NSAttributedString *)attrStr NS_DESIGNATED_INITIALIZER;
@end

// EidosDividerTextAttachmentCell – a little subclass to provide a divider between multiple help items, defined below
@interface EidosDividerTextAttachmentCell : NSTextAttachmentCell
@end

// EidosLockingClipView – a little subclass to work around an AppKit bug, see setDescription: below
@interface EidosLockingClipView : NSClipView
@property (atomic, getter=isEidosScrollingLocked) BOOL eidosScrollingLocked;
@end

// EidosNSString – an NSString subclass to get around the non-uniqueness of NSTaggedPointerStrings; see guardAgainstNSTaggedPointerString
@interface EidosNSString : NSString <NSCopying>
{
	NSString *internal_string_;
}
+ (NSString *)stringWithString:(NSString *)aString;
- (instancetype)init;
- (instancetype)initWithString:(NSString *)aString;
- (NSUInteger)length;
- (unichar)characterAtIndex:(NSUInteger)index;
- (void)getCharacters:(unichar *)buffer range:(NSRange)range;
- (id)copyWithZone:(nullable NSZone *)zone;
@end


//
//	EidosHelpController
//
#pragma mark -
#pragma mark EidosHelpController

// This is a tree data structure class to hold the documentation in a hierarchical form
// Each node has a set of children that are key-value pairs, kept in a std::map
// The value for a given key can be either a pointer to a child tree node, or an attributed string
// A child is only a leaf, with displayable text, if the attributed string pointer is non-null
// The value struct also contains a unique object that is used in the outline view; on macOS this is an NSString *
// This design used to be much simpler: an NSDictionary using NSStrings as both keys and outline view items.  But
// supporting QtSLiM as well required getting rid of the NSDictionary and separating the key function from the
// outline view function, since those are entirely different things in the Qt world.  Thus the current mess.
// The declaration here is a bit tricky to get right; see https://stackoverflow.com/a/58957891/2752221
struct HelpTreeValue;

using HelpTreeNode = std::map<std::string, HelpTreeValue>;
typedef std::pair<std::string, HelpTreeValue> HelpTreeKVPair;
typedef NSString HelpTreeOutlineItem;

struct HelpTreeValue
{
	HelpTreeOutlineItem *outline_item = nil;		// In SLiMgui, an NSString
	NSAttributedString *doc_attr_string = nullptr;
	HelpTreeNode *child = nullptr;
	
	HelpTreeValue(HelpTreeOutlineItem *p_outline_item, NSAttributedString *p_doc_attr_string, HelpTreeNode *p_child)
		: outline_item(p_outline_item), doc_attr_string(p_doc_attr_string), child(p_child)
	{};
};

@interface EidosHelpController () <NSOutlineViewDataSource, NSOutlineViewDelegate>
{
	HelpTreeValue *topicRoot;
	
	int searchType;		// 0 == Title, 1 == Content; equal to the tags on the search type menu items
}

@property (nonatomic, retain) IBOutlet NSWindow *helpWindow;

@property (nonatomic, assign) IBOutlet NSSearchField *searchField;
@property (nonatomic, assign) IBOutlet NSOutlineView *topicOutlineView;
@property (nonatomic, assign) IBOutlet NSTextView *descriptionTextView;

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
		topicRoot = new HelpTreeValue(nil, nil, new HelpTreeNode);
		
		// Add Eidos topics; the only methods defined by Eidos come from EidosObjectClass
		[self addTopicsFromRTFFile:@"EidosHelpFunctions" underHeading:"1. Eidos Functions" functions:&EidosInterpreter::BuiltInFunctions() methods:nullptr properties:nullptr];
		[self addTopicsFromRTFFile:@"EidosHelpMethods" underHeading:"2. Eidos Methods" functions:nullptr methods:gEidos_UndefinedClassObject->Methods() properties:nullptr];
		[self addTopicsFromRTFFile:@"EidosHelpOperators" underHeading:"3. Eidos Operators" functions:nullptr methods:nullptr properties:nullptr];
		[self addTopicsFromRTFFile:@"EidosHelpStatements" underHeading:"4. Eidos Statements" functions:nullptr methods:nullptr properties:nullptr];
		[self addTopicsFromRTFFile:@"EidosHelpTypes" underHeading:"5. Eidos Types" functions:nullptr methods:nullptr properties:nullptr];
		
		// Check for completeness of the help documentation, since it's easy to forget to add new functions/properties/methods to the doc
		[self checkDocumentationOfFunctions:&EidosInterpreter::BuiltInFunctions()];
		[self checkDocumentationOfClass:gEidos_UndefinedClassObject];
		[self checkDocumentationForDuplicatePointers];
	}
	
	return self;
}
	
- (void)dealloc
{
	[self setHelpWindow:nil];
	
	[super dealloc];
}

// So, we have a little problem involving a private NSString subclass called NSTaggedPointerString:
//
// https://www.mikeash.com/pyblog/friday-qa-2015-07-31-tagged-pointer-strings.html
//
// The problem is that for short strings, NSTaggedPointerString effectively means that all copies of a given string are the
// same exact object (the same pointer value), which means that using pointer equality to test whether two strings are
// different objects, even though they contain the same characters, does not work – such tests are always true if the strings
// are represented with NSTaggedPointerString.  We depend upon pointer equality to look up the right topics, due to the
// crappy NSDictionary-based design of this class that I should probably rip out.  So we need to prevent NSTaggedPointerString
// from getting into our topic tree.  As it turns out, at least for now +[NSString stringWithString:] will return a clean
// string that is not an NSTaggedPointerString.  This is surprising, actually, since there is really no reason for it to make
// a copy, but it does.  This could break at any point, but for now it works, and if it breaks I have a check elsewhere that
// will detect the duplicate topic pointers and log (see -checkDocumentationForDuplicatePointers).  BCH 11/5/2017
// Well, that stopped working in OS X 10.13; +[NSString stringWithString:] now makes an NSTaggedPointerString, foiling our
// scheme.  So my new hack scheme is to use a custom NSString subclass, EidosNSString, for keys in our topic tree.  This method
// now substitutes an EidosNSString for the original NSString.  That EidosNSString responds to copyWithZone: by making a copy
// of itself, preventing NSTaggedPointerString from getting in.  This seems to work, and I think it ought to be fairly safe
// and airtight.  We shall see.  BCH 2/26/2018
- (NSString *)guardAgainstNSTaggedPointerString:(NSString *)oldString
{
	//return oldString;		// can be used to test the efficacy of checkDocumentationForDuplicatePointers; in OS X 10.12.6 we're good
	return [EidosNSString stringWithString:oldString];
}

- (HelpTreeOutlineItem *)outlineItemForString:(const std::string &)string
{
	return [[self guardAgainstNSTaggedPointerString:[NSString stringWithUTF8String:string.c_str()]] retain];
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
				[newPStyle release];
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

// This is a helper method for addTopicsFromRTFFile:... that finds the right parent dictionary to insert a given section index under.
// This method makes a lot of assumptions about the layout of the RTF file, such as that section number proceeds in sorted order.
- (HelpTreeNode *)parentDictForSection:(const std::string &)sectionString currentTopicDicts:(std::unordered_map<std::string, HelpTreeNode *> &)topics topDict:(HelpTreeNode *)topLevelDict
{
	std::vector<std::string> sectionComponents = Eidos_string_split(sectionString, ".");
	size_t sectionCount = sectionComponents.size();
	
	if (sectionCount <= 1)
	{
		// With an empty section string, or a whole-number section like "3", the parent is always the top level dict
		return topLevelDict;
	}
	else
	{
		// We have a section string like "3.1" or "3.1.2"; we want to look for a parent to add it to – like "3" or "3.1", respectively
		sectionComponents.pop_back();
		
		std::string parentSectionString = Eidos_string_join(sectionComponents, ".");
		auto parentTopicDictIter = topics.find(parentSectionString);
		
		if (parentTopicDictIter != topics.end())
		{
			// Found a parent to add to
			return parentTopicDictIter->second;
		}
		else
		{
			// Couldn't find a parent to add to, so the parent is the top level
			return topLevelDict;
		}
	}
}

// This is a helper method for addTopicsFromRTFFile:... that creates a new "topic dictionary" under which items will be placed, and finds the right parent
// dictionary to insert it under.  This method makes a lot of assumptions about the layout of the RTF file, such as that section number proceeds in sorted order.
- (HelpTreeNode *)topicDictForSection:(const std::string &)sectionString title:(std::string &)title currentTopicDicts:(std::unordered_map<std::string, HelpTreeNode *> &)topics topDict:(HelpTreeNode *)topLevelDict
{
	std::vector<std::string> sectionComponents = Eidos_string_split(sectionString, ".");
	HelpTreeNode *newTopicDict = new HelpTreeNode();
	
	static const std::string functions(" functions");
	
	if (Eidos_string_hasSuffix(title, functions))
		title.erase(title.length() - functions.length(), std::string::npos);
	
	std::string numberedTitle = sectionComponents.back().append(". ").append(title);
	HelpTreeNode *parentTopicDict = [self parentDictForSection:sectionString currentTopicDicts:topics topDict:topLevelDict];
	HelpTreeOutlineItem *topic_outlineItem = [self outlineItemForString:numberedTitle];
	
	parentTopicDict->emplace(HelpTreeKVPair(numberedTitle, HelpTreeValue(topic_outlineItem, nil, newTopicDict)));
	topics.emplace(std::pair<std::string, HelpTreeNode *>(sectionString, newTopicDict));
	
	return newTopicDict;
}

// This is the main RTF doc file reading method; it finds an RTF file of a given name in the main bundle, reads it into an attributed string, and then scans
// that string for topic headings, function/method/property signature lines, etc., and creates a hierarchy of help topics from the results.  This process
// assumes that the RTF doc file is laid out in a standard way that fits the regex patterns used here; it is designed to work directly with content copied
// and pasted out of our Word documentation files into RTF in TextEdit.
- (void)addTopicsFromRTFFile:(NSString *)rtfFile underHeading:(const std::string &)topLevelHeading functions:(const std::vector<EidosFunctionSignature_SP> *)functionList methods:(const std::vector<const EidosMethodSignature*> *)methodList properties:(const std::vector<const EidosPropertySignature*> *)propertyList
{
	NSString *topicFilePath = [[NSBundle mainBundle] pathForResource:rtfFile ofType:@"rtf"];
	NSData *topicFileData = [NSData dataWithContentsOfFile:topicFilePath];
	NSAttributedString *topicFileAttrString = [[[NSAttributedString alloc] initWithRTF:topicFileData documentAttributes:NULL] autorelease];
	
	// Set up the top-level dictionary that we will place items under
	HelpTreeNode *topLevelDict = new HelpTreeNode();
	std::unordered_map<std::string, HelpTreeNode *> topics;				// keys are strings like 3.1 or 3.1.2 or whatever
	HelpTreeNode *currentTopicDict = topLevelDict;					// start out putting new topics in the top level dict
	HelpTreeOutlineItem *top_outlineItem = [self outlineItemForString:topLevelHeading];
	
	topicRoot->child->emplace(HelpTreeKVPair(topLevelHeading, HelpTreeValue(top_outlineItem, nil, topLevelDict)));
	
	// Set up the current topic item that we are appending content into
	std::string topicItemKey;
	NSMutableAttributedString *topicItemAttrString = nil;
	
	// Make regular expressions that we will use below
	NSRegularExpression *topicHeaderRegex = [NSRegularExpression regularExpressionWithPattern:@"^((?:[0-9]+\\.)*[0-9]+)\\.?  (.+)$" options:NSRegularExpressionCaseInsensitive error:NULL];									// 2 captures
	NSRegularExpression *topicGenericItemRegex = [NSRegularExpression regularExpressionWithPattern:@"^((?:[0-9]+\\.)*[0-9]+)\\.?  ITEM: ((?:[0-9]+\\.? )?)(.+)$" options:NSRegularExpressionCaseInsensitive error:NULL];	// 3 captures
	NSRegularExpression *topicFunctionRegex = [NSRegularExpression regularExpressionWithPattern:@"^\\([a-zA-Z<>\\*+$]+\\)([a-zA-Z_0-9]+)\\(.+$" options:NSRegularExpressionCaseInsensitive error:NULL];						// 1 capture
	NSRegularExpression *topicMethodRegex = [NSRegularExpression regularExpressionWithPattern:@"^([-–+])[  ]\\([a-zA-Z<>\\*+$]+\\)([a-zA-Z_0-9]+)\\(.+$" options:NSRegularExpressionCaseInsensitive error:NULL];			// 2 captures
	NSRegularExpression *topicPropertyRegex = [NSRegularExpression regularExpressionWithPattern:@"^([a-zA-Z_0-9]+)[  ]((?:<[-–]>)|(?:=>)) \\([a-zA-Z<>\\*+$]+\\)$" options:NSRegularExpressionCaseInsensitive error:NULL];	// 2 captures
	
	// Scan through the file one line at a time, parsing out topic headers
	NSString *topicFileString = [topicFileAttrString string];
	NSArray *topicFileLineArray = [topicFileString componentsSeparatedByString:@"\n"];
	NSUInteger lineCount = [topicFileLineArray count];
	NSUInteger lineStartIndex = 0;		// the character index of the current line in topicFileAttrString
	
	for (unsigned int lineIndex = 0; lineIndex < lineCount; ++lineIndex)
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
		NSUInteger isTopicGenericItemLine = ([topicGenericItemRegex numberOfMatchesInString:line options:(NSMatchingOptions)0 range:lineRange] > 0);
		NSUInteger isTopicFunctionLine = ([topicFunctionRegex numberOfMatchesInString:line options:(NSMatchingOptions)0 range:lineRange] > 0);
		NSUInteger isTopicMethodLine = ([topicMethodRegex numberOfMatchesInString:line options:(NSMatchingOptions)0 range:lineRange] > 0);
		NSUInteger isTopicPropertyLine = ([topicPropertyRegex numberOfMatchesInString:line options:(NSMatchingOptions)0 range:lineRange] > 0);
		NSUInteger matchCount = isTopicHeaderLine + isTopicFunctionLine + isTopicMethodLine + isTopicPropertyLine;	// excludes isTopicGenericItemLine, which is a subtype of isTopicHeaderLine
		
		if (matchCount > 1)
		{
			NSLog(@"*** line matched more than one regex type: %@", line);
			return;
		}
		
		if (matchCount == 0)
		{
			if ([line length])
			{
				if (topicItemAttrString)
				{
					// We have a topic, so this is a content line, to be appended to the current topic item's content
					[topicItemAttrString replaceCharactersInRange:NSMakeRange([topicItemAttrString length], 0) withString:@"\n"];
					[topicItemAttrString replaceCharactersInRange:NSMakeRange([topicItemAttrString length], 0) withAttributedString:lineAttrString];
				}
				else
				{
					NSLog(@"orphan line while reading for top level heading %s", topLevelHeading.c_str());
				}
			}
		}
		
		if ((matchCount == 1) || ((matchCount == 0) && (lineIndex == lineCount - 1)))
		{
			// This line starts a new header or item or ends the file, so we need to terminate the current item
			if (topicItemAttrString && topicItemKey.length())
			{
				HelpTreeOutlineItem *topic_outlineItem = [self outlineItemForString:topicItemKey];
				
				currentTopicDict->emplace(HelpTreeKVPair(topicItemKey, HelpTreeValue(topic_outlineItem, [topicItemAttrString retain], nullptr)));
				
				topicItemAttrString = nil;
				topicItemKey.clear();
			}
		}
		
		if (isTopicHeaderLine)
		{
			// We have hit a new topic header.  This might be a subtopic of the current topic, or a sibling, or a sibling of one of our ancestors
			NSTextCheckingResult *match = [topicHeaderRegex firstMatchInString:line options:(NSMatchingOptions)0 range:lineRange];
			NSRange sectionRange = [match rangeAtIndex:1];
			std::string sectionString([[line substringWithRange:sectionRange] UTF8String]);
			NSRange titleRange = [match rangeAtIndex:2];
			std::string titleString([[line substringWithRange:titleRange] UTF8String]);
			
			//NSLog(@"topic header section %@, title %@, line: %@", sectionString, titleString, line);
			
			if (isTopicGenericItemLine)
			{
				// This line plays two roles: it is both a header (with a period-separated section index at the beginning) and a
				// topic item starter like function/method/property lines, with item content following it immediately.  First we
				// use the header-line section string to find the right parent section to place it.
				currentTopicDict = [self parentDictForSection:sectionString currentTopicDicts:topics topDict:topLevelDict];
				
				// Then we extract the item name and create a new item under the parent dict.
				NSTextCheckingResult *itemMatch = [topicGenericItemRegex firstMatchInString:line options:(NSMatchingOptions)0 range:lineRange];
				NSRange itemOrderRange = [itemMatch rangeAtIndex:2];
				NSString *itemOrder = [line substringWithRange:itemOrderRange];
				NSRange itemNameRange = [itemMatch rangeAtIndex:3];
				NSString *itemName = [line substringWithRange:itemNameRange];
				
				topicItemKey = std::string([[itemOrder stringByAppendingString:itemName] UTF8String]);
				topicItemAttrString = [[[lineAttrString attributedSubstringFromRange:itemNameRange] mutableCopy] autorelease];
				
				//NSLog(@"   parsed topic header item with currentTopicDict %p, itemName %@, itemNameRange %@", currentTopicDict, itemName, NSStringFromRange(itemNameRange));
			}
			else
			{
				// This header line is not also an item line, so we want to create a new expandable category and await items
				currentTopicDict = [self topicDictForSection:sectionString title:titleString currentTopicDicts:topics topDict:topLevelDict];
			}
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
					if ((*signature_iter)->call_name_.compare(function_name) == 0)
					{
						function_signature = signature_iter->get();
						break;
					}
				
				if (function_signature)
				{
					NSAttributedString *attrSig = [NSAttributedString eidosAttributedStringForCallSignature:function_signature size:11.0];
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
			
			topicItemKey = std::string([[callName stringByAppendingString:@"()"] UTF8String]);
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
					if ((*signature_iter)->call_name_.compare(method_name) == 0)
					{
						method_signature = *signature_iter;
						break;
					}
				
				if (method_signature)
				{
					NSAttributedString *attrSig = [NSAttributedString eidosAttributedStringForCallSignature:method_signature size:11.0];
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
			
			topicItemKey = std::string([[NSString stringWithFormat:@"%@ %@()", classMethodString, callName] UTF8String]);
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
			
			// check for a built-in property signature that matches and substitute it in
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
					NSAttributedString *attrSig = [NSAttributedString eidosAttributedStringForPropertySignature:property_signature size:11.0];
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
			
			topicItemKey = std::string([[NSString stringWithFormat:@"%@ %@", callName, readOnlyName] UTF8String]);
			topicItemAttrString = [[[NSMutableAttributedString alloc] initWithAttributedString:lineAttrString] autorelease];
		}
	}
}

- (void)checkDocumentationOfFunctions:(const std::vector<EidosFunctionSignature_SP> *)functions
{
	for (EidosFunctionSignature_SP functionSignature : *functions)
	{
		const std::string &functionNameString = functionSignature->call_name_;
		
		if (!Eidos_string_hasPrefix(functionNameString, "_"))
		{
			std::string functionString = std::string(functionNameString).append("()");
			
			HelpTreeValue *functionDocumentation = [self findObjectForKeyEqualTo:functionString withinDictionary:topicRoot->child];
			
			if (!functionDocumentation)
				std::cerr << "*** no documentation found for function " << functionString << std::endl;
		}
	}
}

- (void)checkDocumentationOfClass:(EidosObjectClass *)classObject
{
	bool classIsUndefinedClass = (classObject == gEidos_UndefinedClassObject);
	const std::string &className = classObject->ElementType();
	const std::string classKey = (classIsUndefinedClass ? "Eidos Methods" : std::string("Class ").append(className));
	HelpTreeValue *classDocumentation = [self findObjectWithKeySuffix:classKey withinDictionary:topicRoot->child];
	
	if (classDocumentation->doc_attr_string == nullptr)
	{
		HelpTreeNode *classDocDict = classDocumentation->child;
		std::string propertiesKey = std::string("1. ").append(className).append(" properties");
		std::string methodsKey = std::string("2. ").append(className).append(" methods");
		
		if (classIsUndefinedClass ||
			((classDocDict->size() == 2) && (classDocDict->find(propertiesKey) != classDocDict->end()) && (classDocDict->find(methodsKey) != classDocDict->end())))
		{
			// Check for complete documentation of all properties defined by the class
			if (!classIsUndefinedClass)
			{
				HelpTreeNode *propertyDict = classDocDict->find(propertiesKey)->second.child;
				const std::vector<const EidosPropertySignature *> *classProperties = classObject->Properties();
				std::vector<std::string> docProperties;
				
				for (auto& kv_iter : *propertyDict)
					docProperties.push_back(kv_iter.first);
				
				for (const EidosPropertySignature *propertySignature : *classProperties)
				{
					const std::string &&connector_string = propertySignature->PropertySymbol();
					const std::string &property_name_string = propertySignature->property_name_;
					std::string property_string = std::string(property_name_string).append(" ").append(connector_string);
					auto docIter = std::find(docProperties.begin(), docProperties.end(), property_string);
					
					if (docIter != docProperties.end())
					{
						docProperties.erase(docIter);
					}
					else
					{
						std::cerr << "*** no documentation found for class " << className << " property " << property_string << std::endl;
					}
				}
				
				if (docProperties.size())
				{
					std::cerr << "*** excess documentation found for class " << className << " properties ";
					for (auto& v_iter : docProperties)
						std::cerr << v_iter << " ";
					std::cerr << std::endl;
				}
			}
			
			// Check for complete documentation of all methods defined by the class
			{
				HelpTreeNode *methodDict = (classIsUndefinedClass ? classDocDict : classDocDict->find(methodsKey)->second.child);
				const std::vector<const EidosMethodSignature *> *classMethods = classObject->Methods();
				const std::vector<const EidosMethodSignature *> *baseMethods = gEidos_UndefinedClassObject->Methods();
				std::vector<std::string> docMethods;
				
				for (auto& kv_iter : *methodDict)
					docMethods.push_back(kv_iter.first);
				
				for (const EidosMethodSignature *methodSignature : *classMethods)
				{
					bool isBaseMethod = (std::find(baseMethods->begin(), baseMethods->end(), methodSignature) != baseMethods->end());
					
					if (!isBaseMethod || classIsUndefinedClass)
					{
						const std::string &&prefix_string = methodSignature->CallPrefix();
						const std::string &method_name_string = methodSignature->call_name_;
						std::string method_string = std::string(prefix_string).append(method_name_string).append("()");
						auto docIter = std::find(docMethods.begin(), docMethods.end(), method_string);
						
						if (docIter != docMethods.end())
						{
							docMethods.erase(docIter);
						}
						else
						{
							std::cerr << "*** no documentation found for class " << className << " method " << method_string << std::endl;
						}
					}
				}
				
				if (docMethods.size())
				{
					std::cerr << "*** excess documentation found for class " << className << " methods ";
					for (auto& v_iter : docMethods)
						std::cerr << v_iter << " ";
					std::cerr << std::endl;
				}
			}
		}
		else
		{
			std::cerr << "*** documentation for class " << className << " in unexpected format" << std::endl;
		}
	}
	else
	{
		std::cerr << "*** no documentation found for class " << className << std::endl;
	}
}

- (void)_gatherKeysWithinDictionary:(HelpTreeNode *)searchDict intoVector:(std::vector<pointer_t> &)vec
{
	for (auto& kv_iter : *searchDict) {
		HelpTreeValue &node_value = kv_iter.second;
		
		vec.push_back((pointer_t)node_value.outline_item);
		
		if (node_value.child)
			[self _gatherKeysWithinDictionary:node_value.child intoVector:vec];
	}
}

- (void)checkDocumentationForDuplicatePointers
{
	// The goal here is to check that no two topics in the tree have the same pointer for their key.  This can happen if
	// NSStrings are uniqued behind out back, which it turns out Apple actually does now using NSTaggedPointerString; see
	// guardAgainstNSTaggedPointerString: above.  We have a workaround for that problem, for now, but need to be vigilant
	// in case our workaround breaks, as might happen if Apple makes +[NSString stringWithString:] smart enough to return
	// an NSTaggedPointerString back to us.
	std::vector<pointer_t> topic_keys;
	
	[self _gatherKeysWithinDictionary:topicRoot->child intoVector:topic_keys];
	
	// Now that we've got all the keys, sort, and then find and print duplicates
	std::sort(topic_keys.begin(), topic_keys.end());
	
	std::vector<pointer_t>::iterator topic_iter = topic_keys.begin();
	
	while (YES)
	{
		topic_iter = std::adjacent_find(topic_iter, topic_keys.end());
		
		if (topic_iter == topic_keys.end())
			break;
		
#if 1
		NSLog(@"*** duplicate topic keys in help tree: %@ (%@)", (NSString *)*topic_iter, [(id)(*topic_iter) class]);
#else
		std::cerr << "*** duplicate topic keys in help tree: " << *(std::string *)*topic_iter;
#endif
		++topic_iter;
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
	NSOutlineView *senderOutlineView = (NSOutlineView *)sender;
	
	if (senderOutlineView == _topicOutlineView)
	{
		id clickItem = [_topicOutlineView itemAtRow:[_topicOutlineView clickedRow]];
		
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

- (bool)findItemsUnderRoot:(HelpTreeNode *)root withKey:(HelpTreeOutlineItem *)rootKey matchingSearchString:(const std::string &)searchString titlesOnly:(bool)titlesOnly addingToMatchKeys:(std::vector<HelpTreeOutlineItem *> &)matchKeys andItemsToExpand:(std::vector<HelpTreeOutlineItem *> &)expandItems
{
	bool anyChildMatches = false;
	
	for (auto& kv_iter : *root)
	{
		const std::string &key = kv_iter.first;
		HelpTreeOutlineItem *outlineItem = kv_iter.second.outline_item;
		
		if (kv_iter.second.child)
		{
			// If the object for the key is a dictionary, the child is an expandable item, so we need to recurse
			HelpTreeNode *obj = kv_iter.second.child;
			
			bool result = [self findItemsUnderRoot:obj withKey:outlineItem matchingSearchString:searchString titlesOnly:titlesOnly addingToMatchKeys:matchKeys andItemsToExpand:expandItems];
			
			if (result)
				anyChildMatches = true;
		}
		else
		{
			// If the item is not a dictionary, then the child is a leaf, so we need to test for a match
			bool isMatch = false;
			
			if (Eidos_string_containsCaseInsensitive(key, searchString))
				isMatch = true;
			
			if (!titlesOnly)
			{
				NSAttributedString *obj = kv_iter.second.doc_attr_string;
				std::string obj_string([[obj string] UTF8String]);
				
				if (Eidos_string_containsCaseInsensitive(obj_string, searchString))
					isMatch = true;
			}
			
			if (isMatch)
			{
				matchKeys.push_back(outlineItem);
				anyChildMatches = true;
			}
		}
	}
	
	if (anyChildMatches && rootKey)
		expandItems.push_back(rootKey);
	
	return anyChildMatches;
}

- (IBAction)searchFieldChanged:(id)sender
{
	NSString *searchString = [_searchField stringValue];
	std::string search_string([searchString UTF8String]);
	
	// Do a depth-first search under the topic root that matches the search pattern, and gather tasks to perform
	std::vector<HelpTreeOutlineItem *> matchKeys;
	std::vector<HelpTreeOutlineItem *> expandItems;
	
	[self findItemsUnderRoot:topicRoot->child withKey:nil matchingSearchString:search_string titlesOnly:(searchType == 0) addingToMatchKeys:matchKeys andItemsToExpand:expandItems];
	
	if (matchKeys.size())
	{
		// First collapse everything, as an initial state
		[_topicOutlineView collapseItem:nil collapseChildren:YES];
		
		// Expand all nodes that have a search hit; reverse order so parents expand before their children
		for (auto iter = expandItems.rbegin(); iter != expandItems.rend(); ++iter)
			[_topicOutlineView expandItem:*iter];
		
		// Select all of the items that matched; rowForItem: only returns the first hit, so we have a custom method that gets all hits
		for (auto item : matchKeys)
			[_topicOutlineView selectRowIndexes:[(EidosHelpOutlineView *)_topicOutlineView eidosRowIndicesForItem:item] byExtendingSelection:YES];
		
		// The outline view occasionally seems to mis-update; I think it is an AppKit bug but it's hard to be sure.
		// In any case, telling it to completely redraw here seems to fix the problem.
		[_topicOutlineView setNeedsDisplay:YES];
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

- (HelpTreeValue *)findObjectWithKeySuffix:(const std::string &)searchKeySuffix withinDictionary:(HelpTreeNode *)searchDict
{
	HelpTreeValue *value = nullptr;
	
	for (auto& keyval_iter : *searchDict)
	{
		// Search by substring matching; we have to be careful to use this method to search only for unique keys
		const std::string &key = keyval_iter.first;
		
		if (Eidos_string_hasSuffix(key, searchKeySuffix))
			value = &keyval_iter.second;
		else if (keyval_iter.second.child)
			value = [self findObjectWithKeySuffix:searchKeySuffix withinDictionary:keyval_iter.second.child];
		
		if (value)
			break;
	}
	
	return value;
}

- (HelpTreeValue *)findObjectForKeyEqualTo:(const std::string &)searchKey withinDictionary:(HelpTreeNode *)searchDict
{
	HelpTreeValue *value = nullptr;
	
	for (auto& keyval_iter : *searchDict)
	{
		// Search using isEqualToString:; we have to be careful to use this method to search only for unique keys
		const std::string &key = keyval_iter.first;
		
		if (key == searchKey)
			value = &keyval_iter.second;
		else if (keyval_iter.second.child)
			value = [self findObjectForKeyEqualTo:searchKey withinDictionary:keyval_iter.second.child];
		
		if (value)
			break;
	}
	
	return value;
}

- (HelpTreeValue *)findObjectForOutlineItem:(HelpTreeOutlineItem *)searchOutlineItem withinDictionary:(HelpTreeNode *)searchDict
{
	HelpTreeValue *value = nullptr;
	
	for (auto& keyval_iter : *searchDict)
	{
		// Search by pointer equality, not by hash/isEqual:, so that items with identical (but not pointer-equal) keys
		// can refer to different values in our topic tree; the "subpopID" property, for example, has the same name
		// in both Mutation and Substitution, but has different descriptions.
		HelpTreeOutlineItem *outlineItem = keyval_iter.second.outline_item;
		
		if (outlineItem == searchOutlineItem)
			value = &keyval_iter.second;
		else if (keyval_iter.second.child)
			value = [self findObjectForOutlineItem:searchOutlineItem withinDictionary:keyval_iter.second.child];
		
		if (value)
			break;
	}
	
	return value;
}

- (HelpTreeValue *)findObjectForOutlineItem:(HelpTreeOutlineItem *)searchOutlineItem
{
	// The datasource methods below deal in keys; the child returned at a given index is an NSString key.  We often need
	// to find the corresponding value for a given key, but this is not simple since we don't have a reference to the
	// dictionary that it is a key within.  So we do a depth-first search through our topic tree to find it.  This is
	// going to be a bit slow, but the topic tree should be small so it shouldn't matter.  The alternative would be to
	// develop a better data structure for storing our topics.
	
	// If the search key is nil, return topicRoot; this makes topicRoot correspond to the root object of the outline view
	if (!searchOutlineItem)
		return topicRoot;
	
	// Otherwise, search within topicRoot's children
	return [self findObjectForOutlineItem:searchOutlineItem withinDictionary:topicRoot->child];
}


//
//	NSOutlineView datasource / delegate methods
//
#pragma mark NSOutlineView delegate

- (NSInteger)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item
{
	// According to our design, outline items are always NSStrings, and we have to look up their values in our topic tree
	HelpTreeOutlineItem *help_item = (HelpTreeOutlineItem *)item;
	
	HelpTreeValue *itemValue = [self findObjectForOutlineItem:help_item];
	
	if (itemValue->child)
		return itemValue->child->size();
	
	return 0;
}

- (id)outlineView:(NSOutlineView *)outlineView child:(NSInteger)index ofItem:(id)item
{
	// According to our design, outline items are always NSStrings, and we have to look up their values in our topic tree
	HelpTreeOutlineItem *help_item = (HelpTreeOutlineItem *)item;
	
	HelpTreeValue *itemValue = [self findObjectForOutlineItem:help_item];
	
	if (itemValue->child)
	{
		std::vector<HelpTreeOutlineItem *> keys;
		
		for (auto kv_iter : *itemValue->child)
			keys.push_back(kv_iter.second.outline_item);
		
		return keys[index];
	}
	
	return (id _Nonnull)nil;	// get rid of the static analyzer warning
}

- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item
{
	// According to our design, outline items are always NSStrings, and we have to look up their values in our topic tree
	HelpTreeOutlineItem *help_item = (HelpTreeOutlineItem *)item;
	
	HelpTreeValue *itemValue = [self findObjectForOutlineItem:help_item];
	
	if (itemValue->child)
		return YES;
	
	return NO;
}

- (id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item
{
	if (item)
	{
		HelpTreeOutlineItem *help_item = (HelpTreeOutlineItem *)item;
		static NSRegularExpression *sortOrderStripRegex = nullptr;
		NSString *itemString = item;
		
		if (!sortOrderStripRegex)
			sortOrderStripRegex = [[NSRegularExpression regularExpressionWithPattern:@"^(?:[0-9]+\\. )(.+)$" options:NSRegularExpressionCaseInsensitive error:NULL] retain];
		
		NSTextCheckingResult *match = [sortOrderStripRegex firstMatchInString:itemString options:(NSMatchingOptions)0 range:NSMakeRange(0, [itemString length])];
		
		if (match)
		{
			// If the item starts with a sort-order prefix, strip it off for display
			NSRange captureRange = [match rangeAtIndex:1];
			
			return [help_item substringWithRange:captureRange];
		}
		
		return help_item;
	}
	
	return @"";
}

- (BOOL)outlineView:(NSOutlineView *)outlineView isGroupItem:(id)item
{
	// We want items under the true topic root to look like groups; if we have collapsed the top level, we have no group items
	HelpTreeOutlineItem *help_item = (HelpTreeOutlineItem *)item;
	
	for (auto& keyval_iter : *topicRoot->child)
		if (keyval_iter.second.outline_item == help_item)
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
			HelpTreeOutlineItem *selectedKey = [_topicOutlineView itemAtRow:[selectedIndices firstIndex]];
			HelpTreeValue *selectedValue = [self findObjectForOutlineItem:selectedKey];
			
			if (selectedValue->doc_attr_string)
				[self setDescription:selectedValue->doc_attr_string];
		}
		else
		{
			// Multiple items are selected, so we want to put together an attributed string that shows them all, with dividers
			__block NSMutableAttributedString *conglomerate = [[[NSMutableAttributedString alloc] initWithString:@""] autorelease];
			__block BOOL firstIndex = YES;
			
			[selectedIndices enumerateIndexesUsingBlock:^(NSUInteger idx, BOOL *stop) {
				HelpTreeOutlineItem *selectedKey = [_topicOutlineView itemAtRow:idx];
				HelpTreeValue *selectedValue = [self findObjectForOutlineItem:selectedKey];
				
				if (selectedValue->doc_attr_string)
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
					
					[conglomerate replaceCharactersInRange:NSMakeRange([conglomerate length], 0) withAttributedString:selectedValue->doc_attr_string];
				}
			}];
			
			[self setDescription:conglomerate];
		}
	}
}

@end


//
//	EidosHelpOutlineView
//
#pragma mark -
#pragma mark EidosHelpOutlineView

@implementation EidosHelpOutlineView

- (void)drawRow:(NSInteger)rowIndex clipRect:(NSRect)clipRect
{
	[super drawRow:rowIndex clipRect:clipRect];
	
	NSRect rowRect = [self rectOfRow:rowIndex];
	id item = [self itemAtRow:rowIndex];
	id delegate = [self delegate];
	
	// We want to colorize group rows green if they are from the Eidos doc, blue otherwise (Context doc)
	if ([delegate outlineView:self isGroupItem:item])
	{
		if (item && ([item rangeOfString:@"Eidos"].location != NSNotFound))	// BCH 4/7/2016: containsString: added in 10.10
		{
			[[NSColor colorWithCalibratedRed:0 green:1.0 blue:0 alpha:0.04] set];
			NSRectFillUsingOperation(rowRect, NSCompositeSourceOver);
		}
		else
		{
			[[NSColor colorWithCalibratedRed:0 green:0 blue:1.0 alpha:0.04] set];
			NSRectFillUsingOperation(rowRect, NSCompositeSourceOver);
		}
	}
	
	// Add little colored boxes to the WF-only and nonWF-only API; this is pretty inefficient, but we
	// don't have very many strings to check so it should be fine, and is simpler than the alternatives...
	BOOL draw_WF_box = NO, draw_nonWF_box = NO, draw_nucmut_box = NO;
	static NSArray *stringsWF = nullptr;
	static NSArray *stringsNonWF = nullptr;
	static NSArray *stringsNucmut = nullptr;
	
	if (!stringsWF)
		stringsWF = [@[@"– addSubpopSplit()",
					   @"– registerMateChoiceCallback()",
					   @"cloningRate =>",
					   @"immigrantSubpopFractions =>",
					   @"immigrantSubpopIDs =>",
					   @"selfingRate =>",
					   @"sexRatio =>",
					   @"– setCloningRate()",
					   @"– setMigrationRates()",
					   @"– setSelfingRate()",
					   @"– setSexRatio()",
					   @"– setSubpopulationSize()",
					   @"4. mateChoice() callbacks"
					   ] retain];
	
	if (!stringsNonWF)
		stringsNonWF = [@[@"initializeSLiMModelType()",
						  @"age =>",
						  @"modelType =>",
						  @"– registerReproductionCallback()",
						  @"– addCloned()",
						  @"– addCrossed()",
						  @"– addEmpty()",
						  @"– addRecombinant()",
						  @"– addSelfed()",
						  @"– removeSubpopulation()",
						  @"– takeMigrants()",
						  @"8. reproduction() callbacks"
						  ] retain];
	
	if (!stringsNucmut)
		stringsNucmut = [@[@"initializeAncestralNucleotides()",
						   @"initializeMutationTypeNuc()",
						   @"initializeHotspotMap()",
						   @"codonsToAminoAcids()",
						   @"randomNucleotides()",
						   @"mm16To256()",
						   @"mmJukesCantor()",
						   @"mmKimura()",
						   @"nucleotideCounts()",
						   @"nucleotideFrequencies()",
						   @"nucleotidesToCodons()",
						   @"codonsToNucleotides()",
						   @"nucleotideBased =>",
						   @"nucleotide <–>",
						   @"nucleotideValue <–>",
						   @"mutationMatrix =>",
						   @"– setMutationMatrix()",
						   @"– ancestralNucleotides()",
						   @"– setAncestralNucleotides()",
						   @"– nucleotides()",
						   @"hotspotEndPositions =>",
						   @"hotspotEndPositionsF =>",
						   @"hotspotEndPositionsM =>",
						   @"hotspotMultipliers =>",
						   @"hotspotMultipliersF =>",
						   @"hotspotMultipliersM =>",
						   @"– setHotspotMap()"
						   ] retain];
	
	if ([stringsWF containsObject:item])
		draw_WF_box = YES;
	if ([stringsNonWF containsObject:item])
		draw_nonWF_box = YES;
	if ([stringsNucmut containsObject:item])
		draw_nucmut_box = YES;
	
	if (draw_WF_box || draw_nonWF_box || draw_nucmut_box)
	{
		NSRect boxRect = NSMakeRect(NSMaxX(rowRect) - 13, rowRect.origin.y + 6, 8, 8);
		
		if (draw_WF_box)
			[[NSColor colorWithCalibratedRed:66/255.0 green:255/255.0 blue:53/255.0 alpha:1.0] set];	// WF-only color (green)
		else if (draw_nonWF_box)
			[[NSColor colorWithCalibratedRed:88/255.0 green:148/255.0 blue:255/255.0 alpha:1.0] set];	// nonWF-only color (blue)
		else //if (draw_nucmut_box)
			[[NSColor colorWithCalibratedRed:228/255.0 green:118/255.0 blue:255/255.0 alpha:1.0] set];	// nucmut color (purple)
		
		NSRectFill(boxRect);
		[[NSColor blackColor] set];
		NSFrameRect(boxRect);
	}
}
	
- (NSIndexSet *)eidosRowIndicesForItem:(id)item
{
	NSMutableIndexSet *indexSet = [NSMutableIndexSet indexSet];
	NSInteger rowCount = [self numberOfRows];
	
	for (NSInteger rowIndex = 0; rowIndex < rowCount; ++rowIndex)
	{
		id rowItem = [self itemAtRow:rowIndex];
		
		if ([item isEqual:rowItem])
		[indexSet addIndex:rowIndex];
	}
	
	return indexSet;
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
	[self edited:NSTextStorageEditedAttributes range:range changeInLength:0];
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


//
//	EidosNSString
//
#pragma mark -
#pragma mark EidosNSString

@implementation EidosNSString
+ (NSString *)stringWithString:(NSString *)aString
{
	return [[[EidosNSString alloc] initWithString:aString] autorelease];
}
- (instancetype)init
{
	if (self = [super init])
	{
		internal_string_ = [[NSString alloc] init];
	}
	
	return self;
}
- (instancetype)initWithString:(NSString *)aString
{
	if (self = [super init])
	{
		internal_string_ = [[NSString alloc] initWithString:aString];
	}
	
	return self;
}
- (void)dealloc
{
	if (internal_string_)
	{
		[internal_string_ release];
		internal_string_ = nil;
	}
	
	[super dealloc];
}
- (NSUInteger)length
{
	return [internal_string_ length];
}
- (unichar)characterAtIndex:(NSUInteger)index
{
	return [internal_string_ characterAtIndex:index];
}
- (void)getCharacters:(unichar *)buffer range:(NSRange)range
{
	[internal_string_ getCharacters:buffer range:range];
}
- (id)copyWithZone:(nullable NSZone *)zone
{
	NSString *copiedString = [internal_string_ copyWithZone:zone];
	EidosNSString *copy = [EidosNSString stringWithString:copiedString];
	[copiedString release];
	return [copy retain];
}
@end










































