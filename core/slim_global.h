//
//  slim_global.h
//  SLiM
//
//  Created by Ben Haller on 1/4/15.
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
 
 This file contains various enumerations and small helper classes that are used across SLiM.
 
 */

#ifndef __SLiM__slim_global__
#define __SLiM__slim_global__

#include <stdio.h>
#include <iostream>


// Debugging #defines that can be turned on
#define DEBUG_MUTATIONS			0		// turn on logging of mutation construction and destruction
#define DEBUG_MUTATION_ZOMBIES	0		// avoid destroying Mutation objects; keep them as zombies
#define DEBUG_INPUT				0		// additional output for debugging of input file parsing


// Print a demangled stack backtrace of the caller function to FILE* out; see slim_global.cpp for credits and comments.
void print_stacktrace(FILE *out = stderr, unsigned int max_frames = 63);


// This little class is used as a stream manipulator that causes SLiM to terminate with EXIT_FAILURE, optionally
// with a backtrace.  This is nice since it lets us log and terminate in a single line of code.  It also allows
// the GUI to intercept the exit() call and do something more graceful with it.
class slim_terminate
{
public:
	bool print_backtrace_ = false;
	
	slim_terminate(void) = default;														// default constructor, no backtrace
	slim_terminate(bool p_print_backtrace) : print_backtrace_(p_print_backtrace) {};	// optionally request a backtrace
};

std::ostream& operator<<(std::ostream& p_out, const slim_terminate &p_terminator);


// This enumeration represents the type of chromosome represented by a genome: autosome, X, or Y.  Note that this is somewhat
// separate from the sex of the individual; one can model sexual individuals but model only an autosome, in which case the sex
// of the individual cannot be determined from its modeled genome.
enum class GenomeType {
	kAutosome = 0,
	kXChromosome,
	kYChromosome
};

inline std::ostream& operator<<(std::ostream& p_out, GenomeType p_genome_type)
{
	switch (p_genome_type)
	{
		case GenomeType::kAutosome:		p_out << "A"; break;
		case GenomeType::kXChromosome:	p_out << "X"; break;	// SEX ONLY
		case GenomeType::kYChromosome:	p_out << "Y"; break;	// SEX ONLY
	}
	
	return p_out;
}


// This enumeration represents the sex of an individual: hermaphrodite, female, or male.  It also includes an "unspecified"
// value that is useful in situations where the code wants to say that it doesn't care what sex is present.
enum class IndividualSex
{
	kUnspecified = -2,
	kHermaphrodite = -1,
	kFemale = 0,
	kMale = 1
};

inline std::ostream& operator<<(std::ostream& p_out, IndividualSex p_sex)
{
	switch (p_sex)
	{
		case IndividualSex::kUnspecified:		p_out << "*"; break;
		case IndividualSex::kHermaphrodite:		p_out << "H"; break;
		case IndividualSex::kFemale:			p_out << "F"; break;	// SEX ONLY
		case IndividualSex::kMale:				p_out << "M"; break;	// SEX ONLY
	}
	
	return p_out;
}



#endif /* defined(__SLiM__slim_global__) */


















































