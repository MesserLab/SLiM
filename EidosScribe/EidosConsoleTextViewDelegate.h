//
//  EidosConsoleTextViewDelegate.h
//  SLiM
//
//  Created by Ben Haller on 9/11/15.
//  Copyright (c) 2015-2025 Benjamin C. Haller.  All rights reserved.
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

@class EidosConsoleTextView;


// A protocol that the delegate should respond to, to be notified when the user presses return/enter; this should
// trigger execution of the current line of input (i.e., all text after -promptRangeEnd).
@protocol EidosConsoleTextViewDelegate <EidosTextViewDelegate>
@required

- (void)eidosConsoleTextViewExecuteInput:(EidosConsoleTextView *)textView;

@end


















































