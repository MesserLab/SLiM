//
//  mutation_type.h
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

#ifndef __SLiM__mutation_type__
#define __SLiM__mutation_type__


#include <vector>
#include <string>


class mutation_type
{
	// a mutation type is specified by the DFE and the dominance coefficient
	//
	// DFE options: f: fixed (s) 
	//              e: exponential (mean s)
	//              g: gamma distribution (mean s,shape)
	//
	// examples: synonymous, nonsynonymous, adaptive, etc.
	
public:
	
	float  h;				// dominance coefficient 
	char d;					// DFE (f: fixed, g: gamma, e: exponential)
	std::vector<double> p;	// DFE parameters
	
	mutation_type(float H, char D, std::vector<double> P);
	
	float draw_s();
};


#endif /* defined(__SLiM__mutation_type__) */
