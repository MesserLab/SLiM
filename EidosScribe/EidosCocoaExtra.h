//
//  EidosCocoaExtra.h
//  SLiM
//
//  Created by Ben Haller on 9/11/15.
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


#import <Cocoa/Cocoa.h>
#include <string>

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
+ (NSAttributedString *)eidosAttributedStringForCallSignature:(const EidosCallSignature *)signature size:(double)fontSize;
+ (NSAttributedString *)eidosAttributedStringForPropertySignature:(const EidosPropertySignature *)signature size:(double)fontSize;

@end

@interface NSMutableAttributedString (EidosAdditions)

// A shorthand to avoid having to construct autoreleased temporary attributed strings
- (void)eidosAppendString:(NSString *)str attributes:(NSDictionary<NSString *,id> *)attrs;

@end


@interface NSDictionary (EidosAdditions)

// The standard font (Menlo 11 with 3-space tabs) with a given color, used to assemble attributed strings
+ (NSDictionary *)eidosTextAttributesWithColor:(NSColor *)textColor size:(double)fontSize;

// Some standard attribute dictionaries for Eidos syntax coloring
+ (NSDictionary *)eidosPromptAttrsWithSize:(double)fontSize;
+ (NSDictionary *)eidosInputAttrsWithSize:(double)fontSize;
+ (NSDictionary *)eidosOutputAttrsWithSize:(double)fontSize;
+ (NSDictionary *)eidosErrorAttrsWithSize:(double)fontSize;
+ (NSDictionary *)eidosTokensAttrsWithSize:(double)fontSize;
+ (NSDictionary *)eidosParseAttrsWithSize:(double)fontSize;
+ (NSDictionary *)eidosExecutionAttrsWithSize:(double)fontSize;

// Add a hyperlink to an existing attribute dictionary
+ (NSDictionary *)eidosBaseAttributes:(NSDictionary *)baseAttrs withHyperlink:(NSString *)link;

@end


@interface NSSplitView (EidosAdditions)

// A bug fix to make NSSplitView correctly restore its position/layout.
// Borrowed from http://stackoverflow.com/questions/16587058/nssplitview-auto-saving-divider-positions-doesnt-work-with-auto-layout-enable
// Ah, NSSplitView, how I love thee?  Let me count the ways.  OK, I'm done counting.

// BCH 25 April 2017: added the autosaveName parameter so that we can restore from our autosave before setting
// the autosave name on the splitview, because Apple has broken NSSplitView even more severely than before...
- (void)eidosRestoreAutosavedPositionsWithName:(NSString *)autosaveName;

@end


@interface NSTextField (EidosAdditions)

- (void)eidosSetHyperlink:(NSURL *)url onText:(NSString *)text;

@end

@interface NSString (EidosAdditions)

- (int64_t)eidosScoreAsCompletionOfString:(NSString *)completionBase;

@end


extern std::string Eidos_Beep_MACOS(std::string p_sound_name);























































