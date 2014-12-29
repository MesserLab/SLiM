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

/*
 
 The class Event represents an event defined by the input file, such as a change in the population structure, the introduction of
 a new mutation, or a request for output to be generated.  The type of event is specified by a character tag, and additional
 parameters are kept as a vector of strings.
 
 */

#ifndef __SLiM__event__
#define __SLiM__event__


#include <vector>
#include <string>
#include <iostream>


class Event
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.

public:
	
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
	
	char event_type_;							// event type (see above)
	std::vector<std::string> parameters_;		// vector of strings with parameters of event
	
	Event(const Event&) = delete;										// no copying
	Event& operator=(const Event&) = delete;							// no copying
	Event(char p_event_type, std::vector<std::string> p_parameters);	// construct an event with a given type and parameters
};

std::ostream &operator<<(std::ostream &p_outstream, const Event &p_event);


#endif /* defined(__SLiM__event__) */




































































