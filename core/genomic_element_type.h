//
//  genomic_element_type.h
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

#ifndef __SLiM__genomic_element_type__
#define __SLiM__genomic_element_type__


#include <vector>

#include "g_rng.h"


class genomic_element_type
{
	// a genomic element type is specified by a vector of the mutation type identifiers off all 
	// mutation types than can occur in such elements and a vector of their relative fractions.
	// examples: exon, intron, utr, intergenic, etc.
	
private:
	
	gsl_ran_discrete_t* LT;
	
public:
	
	std::vector<int>    m; // mutation types identifiers in this element
	std::vector<double> g; // relative fractions of each mutation type
	
	genomic_element_type(std::vector<int> M, std::vector<double> G);
	
	int draw_mutation_type();
};


#endif /* defined(__SLiM__genomic_element_type__) */
