//
//  substitution.h
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

#ifndef __SLiM__substitution__
#define __SLiM__substitution__


#import "mutation.h"
#import "chromosome.h"


class substitution
{
public:
	
	int   t; // mutation type
	int   x; // position
	float s; // selection coefficient
	int   i; // subpopulation in which mutation arose
	int   g; // generation in which mutation arose  
	int   f; // fixation time
	
	substitution(mutation M, int F);
	
	void print(chromosome& chr);
};


#endif /* defined(__SLiM__substitution__) */
