//
//  PopulationView.h
//  SLiM
//
//  Created by Ben Haller on 1/21/15.
//  Copyright (c) 2015-2017 Philipp Messer.  All rights reserved.
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


#import <Cocoa/Cocoa.h>

#include "subpopulation.h"


@interface PopulationView : NSOpenGLView
{
	int displayMode;	// 0 == individuals, 1 == fitness distribution line plots, 2 == fitness distribution barplot
	
	// used in displayMode 1 and 2, set by PopulationViewOptionsSheet.xib
	int binCount;
	double fitnessMin;
	double fitnessMax;
}

// Outlets connected to objects in PopulationViewOptionsSheet.xib
@property (nonatomic, retain) IBOutlet NSWindow *displayOptionsSheet;
@property (nonatomic, assign) IBOutlet NSTextField *binCountTextField;
@property (nonatomic, assign) IBOutlet NSTextField *fitnessMinTextField;
@property (nonatomic, assign) IBOutlet NSTextField *fitnessMaxTextField;
@property (nonatomic, assign) IBOutlet NSButton *okButton;

- (IBAction)validateSheetControls:(id)sender;						// can be wired to controls that need to trigger validation

- (BOOL)canDisplaySubpopulations:(std::vector<Subpopulation*> &)selectedSubpopulations;

@end


// This subclass is for displaying an error message in the population view, which is hard to do in an NSOpenGLView
@interface PopulationErrorView : NSView
{
}
@end













































