//
//  event.h
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

#ifndef __SLiM__event__
#define __SLiM__event__


#include <vector>
#include <string>


class event
{
	// type of events:
	//
	// t P i n [j]:  add subpopulation i of size n [drawn from j]
	// t N i n:      set size of subpopulation i to n
	// t M i j x:    set fraction x of subpopulation i that originates as migrants from j
	// t S i s;      set selfing fraction of subpopulation i to s
	//
	// t R i n:      output sample of n randomly drawn genomes from subpopulation i
	// t F:          output list of all mutations that have become fixed so far
	// t A [file]:   output state of entire population [into file]
	// t T m:        follow trajectory of mutation m (specified by mutation type) from generation t on
	
public:
	
	char t;						// event type
	std::vector<std::string> s;	// vector of strings with parameters of event
	int np;						// number of parameters
	
	// construct an event with a given type and parameters
	event(char T, std::vector<std::string> S);  
};


#endif /* defined(__SLiM__event__) */
