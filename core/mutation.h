//
//  mutation.h
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

#ifndef __SLiM__mutation__
#define __SLiM__mutation__


class mutation
{
public:
	
	int   t; // mutation type identifier
	int   x; // position
	float s; // selection coefficient
	int   i; // subpopulation in which mutation arose
	int   g; // generation in which mutation arose  
	
	// null constructor
	mutation(void);
	
	// standard constructor, supplying values for all ivars
	mutation(int T, int X, float S, int I, int G);
};

// true if M1 has an earlier (smaller) position than M2
bool operator< (const mutation &M1,const mutation &M2);

// true if M1 and M2 have the same position, type, and selection coefficient
bool operator== (const mutation &M1,const mutation &M2);


#endif /* defined(__SLiM__mutation__) */
