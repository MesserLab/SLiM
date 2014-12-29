//
//  chromosome.h
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
 
 The class Chromosome represents an entire chromosome.  Only the portions of the chromosome that are relevant to the simulation are
 explicitly modeled, so in practice, a chromosome is a vector of genomic elements defined by the input file.  A chromosome also has
 a length, an overall mutation rate, an overall recombination rate, and parameters related to gene conversion.
 
 */

#ifndef __SLiM__chromosome__
#define __SLiM__chromosome__


#include <vector>
#include <map>

#include "mutation.h"
#include "mutation_type.h"
#include "genomic_element.h"
#include "genomic_element_type.h"
#include "g_rng.h"


class Chromosome : public std::vector<GenomicElement>
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.

private:
	
	gsl_ran_discrete_t *lookup_mutation = nullptr;			// OWNED POINTER: lookup table for drawing mutations
	gsl_ran_discrete_t *lookup_recombination = nullptr;		// OWNED POINTER: lookup table for drawing recombination breakpoints
	
public:
	
	vector<int> recombination_end_positions_;				// end positions of each defined recombination region
	vector<double> recombination_rates_;					// recombination rates, in events per base pair
	
	int    length_;											// length of the chromosome
	double overall_mutation_rate_;							// overall mutation rate
	double overall_recombination_rate_;						// overall recombination rate
	double gene_conversion_fraction_;						// gene conversion fraction
	double gene_conversion_avg_length_;						// average gene conversion stretch length
	
	Chromosome(const Chromosome&) = delete;									// no copying
	Chromosome& operator=(const Chromosome&) = delete;						// no copying
	Chromosome(void) = default;												// default constructor
	~Chromosome(void);														// destructor
	
	void InitializeDraws();													// initialize the random lookup tables used by Chromosome to draw mutation and recombination events
	int DrawMutationCount() const;											// draw the number of mutations that occur, based on the overall mutation rate
	Mutation *DrawNewMutation(int p_subpop_index, int p_generation) const;	// draw a new mutation, based on the genomic element types present and their mutational proclivities
	vector<int> DrawBreakpoints() const;									// choose a set of recombination breakpoints, based on recomb. intervals, overall recomb. rate, and gene conversion probability
};


#endif /* defined(__SLiM__chromosome__) */




































































