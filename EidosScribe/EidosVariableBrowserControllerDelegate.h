//
//  EidosVariableBrowserControllerDelegate.h
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

@class EidosVariableBrowserController;
class EidosSymbolTable;


/*
 
 This is an Objective-C++ header, and so can only be included by Objective-C++ compilations (.mm files instead of .m files).
 You should not need to include this header in your .h files, since you can declare protocol conformance in a class-continuation
 category in your .m file, so only classes that conform to this protocol should need to be Objective-C++.
 
 EidosVariableBrowserControllerDelegate is a protocol for the delegate of an EidosVariableBrowserController.  The variable
 browser controller gets the symbols to display from its delegate; if you are using EidosConsoleWindowController it typically
 acts as the delegate for the variable browser, but if you are building your own Eidos user interface you will need to provide
 this delegate method.
 
 */


@protocol EidosVariableBrowserControllerDelegate
@required

- (EidosSymbolTable *)symbolTableForEidosVariableBrowserController:(EidosVariableBrowserController *)browserController;

@end






















































