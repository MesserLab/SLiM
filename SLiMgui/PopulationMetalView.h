//
//  PopulationMetalView.h
//  SLiMgui
//
//  Created by Ben Haller on 1/30/20.
//  Copyright (c) 2020 Philipp Messer.  All rights reserved.
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


#import "SLiMMetalView.h"

#include "subpopulation.h"
#include <string>
#include <map>


@interface PopulationMetalView : SLiMMetalView
{
}

- (BOOL)tileSubpopulations:(std::vector<Subpopulation*> &)selectedSubpopulations;

@end

// This subclass is for displaying an error message in the population view, which is hard to do in an MTKView
@interface PopulationErrorView : NSView
{
}
@end


































