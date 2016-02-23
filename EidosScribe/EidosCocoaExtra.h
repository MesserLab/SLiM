//
//  EidosCocoaExtra.h
//  SLiM
//
//  Created by Ben Haller on 9/11/15.
//  Copyright (c) 2015-2016 Philipp Messer.  All rights reserved.
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


#import <Cocoa/Cocoa.h>

class EidosCallSignature;
class EidosPropertySignature;


/*
 
 Some Cocoa utility categories for working with Eidos.  Note that this header uses Objective-C++, so it can only be imported by
 Objective-C++ compilations (.mm files instead of .m files).  The hope is that the use of Objective-C++ in EidosScribe headers
 can be limited mostly to this file and a few others, so that reuse of main EidosScribe UI classes in a new Context does not
 force the entire project to be Objective-C++.
 
 */


@interface NSAttributedString (EidosAdditions)

// This provides a nicely syntax-colored call signature string for use in the console window status bar and such places.
+ (NSAttributedString *)eidosAttributedStringForCallSignature:(const EidosCallSignature *)signature;
+ (NSAttributedString *)eidosAttributedStringForPropertySignature:(const EidosPropertySignature *)signature;

@end


@interface NSDictionary (EidosAdditions)

// The standard font (Menlo 11 with 3-space tabs) with a given color, used to assemble attributed strings
+ (NSDictionary *)eidosTextAttributesWithColor:(NSColor *)textColor;

// Some standard attribute dictionaries for Eidos syntax coloring
+ (NSDictionary *)eidosPromptAttrs;
+ (NSDictionary *)eidosInputAttrs;
+ (NSDictionary *)eidosOutputAttrs;
+ (NSDictionary *)eidosErrorAttrs;
+ (NSDictionary *)eidosTokensAttrs;
+ (NSDictionary *)eidosParseAttrs;
+ (NSDictionary *)eidosExecutionAttrs;

// Add a hyperlink to an existing attribute dictionary
+ (NSDictionary *)eidosBaseAttributes:(NSDictionary *)baseAttrs withHyperlink:(NSString *)link;

@end


@interface NSSplitView (EidosAdditions)

// A bug fix to make NSSplitView correctly restore its position/layout.
// Borrowed from http://stackoverflow.com/questions/16587058/nssplitview-auto-saving-divider-positions-doesnt-work-with-auto-layout-enable
// Ah, NSSplitView, how I love thee?  Let me count the ways.  OK, I'm done counting.
- (void)eidosRestoreAutosavedPositions;

@end



























































