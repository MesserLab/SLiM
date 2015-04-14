//
//  ConsoleTextView.h
//  SLiM
//
//  Created by Ben Haller on 4/8/15.
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


#import <Cocoa/Cocoa.h>


@interface ConsoleTextView : NSTextView
{
@public
	NSRange lastPromptRange;
	
	NSMutableArray *history;
	int historyIndex;
	BOOL lastHistoryItemIsProvisional;	// got added to the history by a moveUp: event but is not an executed command
}

+ (NSDictionary *)promptAttrs;
+ (NSDictionary *)inputAttrs;
+ (NSDictionary *)outputAttrs;
+ (NSDictionary *)errorAttrs;
+ (NSDictionary *)tokensAttrs;
+ (NSDictionary *)parseAttrs;
+ (NSDictionary *)executionAttrs;

- (NSUInteger)promptRangeEnd;

- (void)showWelcomeMessage;
- (void)showPrompt;
- (void)appendSpacer;

- (void)clearOutput;

- (void)registerNewHistoryItem:(NSString *)newItem;

@end

// A protocol that the delegate should respond to, to be notified when the user presses return/enter
@protocol ConsoleTextViewDelegate <NSObject>
- (void)executeConsoleInput:(ConsoleTextView *)textView;
@end



































































