//
//  PopulationView.h
//  SLiM
//
//  Created by Ben Haller on 1/21/15.
//  Copyright (c) 2015-2023 Philipp Messer.  All rights reserved.
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
#include <map>


typedef struct {
	int backgroundType;				// 0 == black, 1 == gray, 2 == white, 3 == named spatial map; if no preference has been set, no entry will exist
	std::string spatialMapName;		// the name of the spatial map chosen, for backgroundType == 3
} PopulationViewBackgroundSettings;

@interface PopulationView : NSOpenGLView
{
	// display mode: 0 == individuals (non-spatial), 1 == individuals (spatial)
	int displayMode;
	
	// display background preferences, kept indexed by subpopulation id
	std::map<slim_objectid_t, PopulationViewBackgroundSettings> backgroundSettings;
	slim_objectid_t lastContextMenuSubpopID;
	
	// subview tiling, kept indexed by subpopulation id
	std::map<slim_objectid_t, NSRect> subpopTiles;
}

// Outlets connected to objects in PopulationViewOptionsSheet.xib
@property (nonatomic, retain) IBOutlet NSWindow *displayOptionsSheet;
@property (nonatomic, assign) IBOutlet NSTextField *binCountTextField;
@property (nonatomic, assign) IBOutlet NSTextField *fitnessMinTextField;
@property (nonatomic, assign) IBOutlet NSTextField *fitnessMaxTextField;
@property (nonatomic, assign) IBOutlet NSButton *okButton;

- (BOOL)tileSubpopulations:(std::vector<Subpopulation*> &)selectedSubpopulations;

@end


// This subclass is for displaying an error message in the population view, which is hard to do in an NSOpenGLView
@interface PopulationErrorView : NSView
{
}
@end













































