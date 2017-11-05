//
//  EidosHelpController.h
//  SLiM
//
//  Created by Ben Haller on 9/12/15.
//  Copyright (c) 2015-2016 Philipp Messer.  All rights reserved.
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
#include <vector>

class EidosFunctionSignature;
class EidosMethodSignature;
class EidosPropertySignature;
class EidosObjectClass;


@interface EidosHelpController : NSObject
{
}

+ (EidosHelpController *)sharedController;

- (void)enterSearchForString:(NSString *)searchString titlesOnly:(BOOL)titlesOnly;

- (NSWindow *)window;
- (void)showWindow;

// Add topics and items drawn from a specially formatted RTF file, under a designated top-level heading.
// The signatures found for functions, methods, and prototypes will be checked against the supplied lists,
// if they are not NULL, and if matches are found, colorized versions will be substituted.
- (void)addTopicsFromRTFFile:(NSString *)rtfFile underHeading:(NSString *)topLevelHeading functions:(const std::vector<const EidosFunctionSignature *> *)functionList methods:(const std::vector<const EidosMethodSignature*> *)methodList properties:(const std::vector<const EidosPropertySignature*> *)propertyList;

// Check for complete documentation
- (void)checkDocumentationOfFunctions:(const std::vector<const EidosFunctionSignature*> *)functions;
- (void)checkDocumentationOfClass:(EidosObjectClass *)classObject;
- (void)checkDocumentationForDuplicatePointers;

@end






















































