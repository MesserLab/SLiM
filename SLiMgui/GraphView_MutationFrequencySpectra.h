//
//  GraphView_MutationFrequencySpectra.h
//  SLiM
//
//  Created by Ben Haller on 2/27/15.
//  Copyright (c) 2015-2020 Philipp Messer.  All rights reserved.
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


/*
 
 This graph plots a histogram of the frequencies of the mutations present in the whole population.  In other words, it loops
 through all of the mutations, determines their frequencies in the population, and bins those frequences to show as a histogram.
 Determining the frequences of mutations is actually done automatically by SLiM, stored in reference_count_, so this is pretty
 strightforward.
 
 */


#import "GraphView.h"


@interface GraphView_MutationFrequencySpectra : GraphView
{
}

@end





























































