//
//  introduced_mutation.h
//  SLiM
//
//  Created by Ben Haller on 12/13/14.
//  Copyright (c) 2014 Philipp Messer.  All rights reserved.
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

#ifndef __SLiM__introduced_mutation__
#define __SLiM__introduced_mutation__


#import "mutation.h"


class introduced_mutation : public mutation
{
public:
	
	int i;   // subpopulation into which mutation is introduced
	int g;   // generation in which mutation is introduced
	int nAA; // number of homozygotes
	int nAa; // number of heterozygotes
	
	introduced_mutation(int T, int X, int I, int G, int NAA, int NAa);
};


#endif /* defined(__SLiM__introduced_mutation__) */
