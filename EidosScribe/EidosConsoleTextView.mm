//
//  EidosConsoleTextView.m
//  EidosScribe
//
//  Created by Ben Haller on 4/8/15.
//  Copyright (c) 2015-2020 Philipp Messer.  All rights reserved.
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


#import "EidosConsoleTextView.h"
#import "EidosConsoleTextViewDelegate.h"
#import "EidosCocoaExtra.h"
#include "eidos_globals.h"


@implementation EidosConsoleTextView

- (void)dealloc
{
	[history release];
	history = nil;
	
	[super dealloc];
}

// The method signature is inherited from NSTextView, but we want to check that the delegate follows our delegate protocol
- (void)setDelegate:(id<NSTextViewDelegate>)delegate
{
	if (delegate && ![delegate conformsToProtocol:@protocol(EidosConsoleTextViewDelegate)])
		NSLog(@"Delegate %@ assigned to EidosConsoleTextView %p does not conform to the EidosConsoleTextViewDelegate protocol!", delegate, self);
	
	[super setDelegate:delegate];
}

- (void)insertNewline:(id)sender
{
	id delegate = [self delegate];
	
	if ([delegate respondsToSelector:@selector(eidosConsoleTextViewExecuteInput:)])
		[delegate eidosConsoleTextViewExecuteInput:self];
}

// This is option-return or option-enter; we deliberately let it go, to let people put newlines into their input
//- (void)insertNewlineIgnoringFieldEditor:(id)sender
//{
//}

- (NSString *)currentCommandAtPrompt
{
	NSString *outputString = [self string];
	NSString *commandString = [outputString substringFromIndex:[self promptRangeEnd]];
	
	return commandString;
}

- (void)setCommandAtPrompt:(NSString *)newCommand
{
	NSTextStorage *ts = [self textStorage];
	NSUInteger promptEnd = [self promptRangeEnd];
	NSDictionary *inputAttrs = [NSDictionary eidosInputAttrsWithSize:[self displayFontSize]];
	NSAttributedString *newAttrCommand = [[NSAttributedString alloc] initWithString:newCommand attributes:inputAttrs];
	
	[ts beginEditing];
	[ts replaceCharactersInRange:NSMakeRange(promptEnd, [ts length] - promptEnd) withAttributedString:newAttrCommand];
	[ts endEditing];
	
	[newAttrCommand release];
	
	[self setSelectedRange:NSMakeRange([[self string] length], 0)];
	[self setTypingAttributes:inputAttrs];
	[self scrollRangeToVisible:NSMakeRange([[self string] length], 0)];
}

- (void)moveUp:(id)sender
{
	if (history && (historyIndex > 0))
	{
		// If the user has typed at the current prompt and it is unsaved, save it in the history before going up
		if (historyIndex == [history count])
		{
			NSString *commandString = [self currentCommandAtPrompt];
			
			if ([commandString length] > 0)
			{
				// If there is a provisional item on the top of the stack, get rid of it and replace it
				if (lastHistoryItemIsProvisional)
				{
					[history removeLastObject];
					lastHistoryItemIsProvisional = NO;
					--historyIndex;
				}
				
				[history addObject:commandString];
				lastHistoryItemIsProvisional = YES;
			}
		}
		
		// if the only item on the stack was provisional, and we just replaced it, we may have nowhere up to go
		if (historyIndex > 0)
		{
			--historyIndex;
		
			[self setCommandAtPrompt:[history objectAtIndex:historyIndex]];
		}
		
		//NSLog(@"moveUp: end:\nhistory = %@\nhistoryIndex = %d\nlastHistoryItemIsProvisional = %@", history, historyIndex, lastHistoryItemIsProvisional ? @"YES" : @"NO");
	}
}

- (void)moveDown:(id)sender
{
	if (historyIndex <= [history count])
	{
		// If the user has typed at the current prompt and it is unsaved, save it in the history before going down
		if (historyIndex == [history count])
		{
			NSString *commandString = [self currentCommandAtPrompt];
			
			if ([commandString length] > 0)
			{
				// If there is a provisional item on the top of the stack, get rid of it and replace it
				if (lastHistoryItemIsProvisional)
				{
					[history removeLastObject];
					lastHistoryItemIsProvisional = NO;
					--historyIndex;
				}
				
				if (!history)
					history = [[NSMutableArray alloc] init];
				
				[history addObject:commandString];
				lastHistoryItemIsProvisional = YES;
			}
			else
				return;
		}
		
		++historyIndex;
		
		if (historyIndex == [history count])
			[self setCommandAtPrompt:@""];
		else
			[self setCommandAtPrompt:[history objectAtIndex:historyIndex]];
		
		//NSLog(@"moveDown: end:\nhistory = %@\nhistoryIndex = %d\nlastHistoryItemIsProvisional = %@", history, historyIndex, lastHistoryItemIsProvisional ? @"YES" : @"NO");
	}
}

- (void)registerNewHistoryItem:(NSString *)newItem
{
	if (!history)
		history = [[NSMutableArray alloc] init];
	
	// If there is a provisional item on the top of the stack, get rid of it and replace it
	if (lastHistoryItemIsProvisional)
	{
		[history removeLastObject];
		lastHistoryItemIsProvisional = NO;
	}
	
	[history addObject:newItem];
	historyIndex = (int)[history count];	// a new prompt, one beyond the last history item
	
	//NSLog(@"registerNewHistoryItem: end:\nhistory = %@\nhistoryIndex = %d\nlastHistoryItemIsProvisional = %@", history, historyIndex, lastHistoryItemIsProvisional ? @"YES" : @"NO");
}

- (NSUInteger)promptRangeEnd
{
	return lastPromptRange.location + lastPromptRange.length;
}

- (void)setPromptRangeEnd:(NSUInteger)promptEnd
{
	lastPromptRange.location = promptEnd - lastPromptRange.length;
}

- (NSUInteger)rangeOffsetForCompletionRange
{
	return [self promptRangeEnd];
}

- (void)showWelcomeMessage
{
	NSTextStorage *ts = [self textStorage];
	NSDictionary *outputAttrs = [NSDictionary eidosOutputAttrsWithSize:[self displayFontSize]];
	
	NSString *versionString = [NSString stringWithFormat:@"Eidos version %s\n\nBy Benjamin C. Haller (", EIDOS_VERSION_STRING];
	NSAttributedString *welcomeString1 = [[NSAttributedString alloc] initWithString:versionString attributes:outputAttrs];		// EIDOS VERSION
	NSAttributedString *welcomeString2 = [[NSAttributedString alloc] initWithString:@"http://benhaller.com/" attributes:[NSDictionary eidosBaseAttributes:outputAttrs withHyperlink:@"http://benhaller.com/"]];
	NSAttributedString *welcomeString3 = [[NSAttributedString alloc] initWithString:@").\nCopyright (c) 2016–2020 P. Messer. All rights reserved.\n\nEidos is free software with ABSOLUTELY NO WARRANTY.\nType license() for license and distribution details.\n\nGo to " attributes:outputAttrs];
	NSAttributedString *welcomeString4 = [[NSAttributedString alloc] initWithString:@"https://github.com/MesserLab/SLiM" attributes:[NSDictionary eidosBaseAttributes:outputAttrs withHyperlink:@"https://github.com/MesserLab/SLiM"]];
	NSAttributedString *welcomeString5 = [[NSAttributedString alloc] initWithString:@" for source code,\ndocumentation, examples, and other information.\n\nWelcome to Eidos!\n\n---------------------------------------------------------\n\n" attributes:outputAttrs];
	
	[ts beginEditing];
	[ts replaceCharactersInRange:NSMakeRange([ts length], 0) withAttributedString:welcomeString1];
	[ts replaceCharactersInRange:NSMakeRange([ts length], 0) withAttributedString:welcomeString2];
	[ts replaceCharactersInRange:NSMakeRange([ts length], 0) withAttributedString:welcomeString3];
	[ts replaceCharactersInRange:NSMakeRange([ts length], 0) withAttributedString:welcomeString4];
	[ts replaceCharactersInRange:NSMakeRange([ts length], 0) withAttributedString:welcomeString5];
	[ts endEditing];
	
	[welcomeString1 release];
	[welcomeString2 release];
	[welcomeString3 release];
	[welcomeString4 release];
	[welcomeString5 release];
}

- (void)showPrompt:(unichar)promptChar
{
	NSTextStorage *ts = [self textStorage];
	NSDictionary *promptAttrs = [NSDictionary eidosPromptAttrsWithSize:[self displayFontSize]];
	NSDictionary *inputAttrs = [NSDictionary eidosInputAttrsWithSize:[self displayFontSize]];
	
	// The prompt uses the inputAttrs for the second (space) character, to make sure typing attributes are always correct
	NSString *prompt = [NSString stringWithCharacters:&promptChar length:1];
	NSAttributedString *promptString1 = [[NSAttributedString alloc] initWithString:prompt attributes:promptAttrs];
	NSAttributedString *promptString2 = [[NSAttributedString alloc] initWithString:@" " attributes:inputAttrs];
	
	// We remember the prompt range for various purposes such as uneditability of old content
	lastPromptRange = NSMakeRange([[self string] length], 2);
	
	[ts beginEditing];
	[ts replaceCharactersInRange:NSMakeRange(lastPromptRange.location, 0) withAttributedString:promptString1];
	[ts replaceCharactersInRange:NSMakeRange(lastPromptRange.location + 1, 0) withAttributedString:promptString2];
	[ts endEditing];
	
	[promptString1 release];
	[promptString2 release];
	
	[self setSelectedRange:NSMakeRange(lastPromptRange.location + 2, 0)];
	[self setTypingAttributes:inputAttrs];
	[self scrollRangeToVisible:NSMakeRange([[self string] length], 0)];
}

- (void)showPrompt
{
	[self showPrompt:'>'];
}

- (void)appendSpacer
{
	static NSAttributedString *spacerString = nil;
	
	if (!spacerString)
		spacerString = [[NSAttributedString alloc] initWithString:@"\n" attributes:@{NSForegroundColorAttributeName : [NSColor blackColor], NSFontAttributeName : [NSFont fontWithName:@"Menlo" size:3.0]}];
	
	NSTextStorage *ts = [self textStorage];
	
	// we only add a spacer newline if the current contents already end in a newline; we don't introduce new breaks
	if ([[ts string] characterAtIndex:[ts length] - 1] == '\n')
	{
		[ts beginEditing];
		[ts replaceCharactersInRange:NSMakeRange([ts length], 0) withAttributedString:spacerString];
		[ts endEditing];
	}
}

- (void)clearOutputToPosition:(NSUInteger)clearPosition
{
	NSTextStorage *ts = [self textStorage];
	NSRange selectedRange = [self selectedRange];
	NSRange rangeToClear = NSMakeRange(0, clearPosition);
	
	[ts beginEditing];
	[ts replaceCharactersInRange:rangeToClear withString:@""];
	[ts endEditing];
	
	lastPromptRange.location -= clearPosition;
	
	if (selectedRange.location + selectedRange.length <= rangeToClear.location + rangeToClear.length)
	{
		// the selection was entirely in the deleted range, so reset it to the end of the textview
		selectedRange = NSMakeRange([ts length], 0);
	}
	else if (selectedRange.location <= rangeToClear.location + rangeToClear.length)
	{
		// the selected range started in the deleted range but extended into the undeleted range, so preserve the remaining portion
		unsigned long deletedLength = rangeToClear.location + rangeToClear.length - selectedRange.location;
		
		selectedRange.location = 0;
		selectedRange.length -= deletedLength;
	}
	else
	{
		// the selected range was entirely beyond the deleted range, so preserve the whole thing
		selectedRange.location -= rangeToClear.length;
	}
	
	[self setSelectedRange:selectedRange];
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem
{
	SEL sel = [menuItem action];
	
	if ((sel == @selector(shiftSelectionLeft:)) || (sel == @selector(shiftSelectionRight:)) || (sel == @selector(commentUncommentSelection:)))
		return NO;
	
	return [super validateMenuItem:menuItem];
}

@end



































































