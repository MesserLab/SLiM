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
#include <sstream>


// If we're running inside SLiMgui, we use a global ostringstream to capture all output to both the output and error streams.
// This stream gets emptied out after every call out to SLiMSim, so a single stream can be safely used by all of the SLiMSim
// instances running inside SLiMgui (because we do not multithread).  We also have a special stream for termination messages.
#if defined(SLIMGUI) || defined(SLIMSCRIBE)

extern std::ostringstream gSLiMOut;
extern std::ostringstream gSLiMTermination;
#define SLIM_OUTSTREAM		(gSLiMOut)
#define SLIM_ERRSTREAM		(gSLiMOut)
#define SLIM_TERMINATION	(gSLiMTermination)

#else

#define SLIM_OUTSTREAM		(std::cout)
#define SLIM_ERRSTREAM		(std::cerr)
#define SLIM_TERMINATION	(std::cerr)

#endif


// the part of the input file that caused an error; set by CheckInputFile() and used by SLiMgui
extern int gCharacterStartOfParseError, gCharacterEndOfParseError;


// Debugging #defines that can be turned on
#define DEBUG_MUTATIONS			0		// turn on logging of mutation construction and destruction
#define DEBUG_MUTATION_ZOMBIES	0		// avoid destroying Mutation objects; keep them as zombies
#define DEBUG_INPUT				1		// additional output for debugging of input file parsing


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

std::ostream& operator<<(std::ostream& p_out, const slim_terminate &p_terminator);	// note this returns void, not std::ostream&; that is deliberate

// Get the message from the last raise out of gSLiMTermination, optionally with newlines trimmed from the ends
std::string GetTrimmedRaiseMessage(void);
std::string GetUntrimmedRaiseMessage(void);

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


//
//	Global std::string objects.  This is kind of gross, but there are several rationales for it.  First of all, it makes
//	a speed difference; converting a C string to a std::string is done every time it is hit in the code; C++ does not
//	treat that as a constant expression and cache it for you, at least with the current generation of compilers.  The
//	conversion is surprisingly slow; it has shown up repeatedly in profiles I have done.  Second, there is the issue of
//	uniqueness; many of these strings occur in multiple places in the code, and a typo in one of those multiple occurrences
//	would cause a bug that would be very difficult to find.  If multiple places in the code intend to refer to the same
//	conceptual string, and rely on those references being the same, then a shared constant should be used.  So... oh well.
//

extern const std::string gStr_empty_string;
extern const std::string gStr_space_string;

extern const std::string gStr_function;
extern const std::string gStr_method;
extern const std::string gStr_executeLambda;
extern const std::string gStr_globals;

extern const std::string gStr_genomicElements;
extern const std::string gStr_lastPosition;
extern const std::string gStr_overallRecombinationRate;
extern const std::string gStr_recombinationEndPositions;
extern const std::string gStr_recombinationRates;
extern const std::string gStr_geneConversionFraction;
extern const std::string gStr_geneConversionMeanLength;
extern const std::string gStr_overallMutationRate;
extern const std::string gStr_genomeType;
extern const std::string gStr_isNullGenome;
extern const std::string gStr_mutations;
extern const std::string gStr_genomicElementType;
extern const std::string gStr_startPosition;
extern const std::string gStr_endPosition;
extern const std::string gStr_id;
extern const std::string gStr_mutationTypes;
extern const std::string gStr_mutationFractions;
extern const std::string gStr_mutationType;
extern const std::string gStr_originGeneration;
extern const std::string gStr_position;
extern const std::string gStr_selectionCoeff;
extern const std::string gStr_subpopID;
extern const std::string gStr_id;
extern const std::string gStr_distributionType;
extern const std::string gStr_distributionParams;
extern const std::string gStr_dominanceCoeff;
extern const std::string gStr_path;
extern const std::string gStr_id;
extern const std::string gStr_start;
extern const std::string gStr_end;
extern const std::string gStr_type;
extern const std::string gStr_source;
extern const std::string gStr_active;
extern const std::string gStr_chromosome;
extern const std::string gStr_chromosomeType;
extern const std::string gStr_genomicElementTypes;
extern const std::string gStr_mutations;
extern const std::string gStr_mutationTypes;
extern const std::string gStr_scriptBlocks;
extern const std::string gStr_sexEnabled;
extern const std::string gStr_start;
extern const std::string gStr_subpopulations;
extern const std::string gStr_substitutions;
extern const std::string gStr_dominanceCoeffX;
extern const std::string gStr_duration;
extern const std::string gStr_generation;
extern const std::string gStr_randomSeed;
extern const std::string gStr_id;
extern const std::string gStr_firstMaleIndex;
extern const std::string gStr_genomes;
extern const std::string gStr_immigrantSubpopIDs;
extern const std::string gStr_immigrantSubpopFractions;
extern const std::string gStr_selfingFraction;
extern const std::string gStr_sexRatio;
extern const std::string gStr_size;
extern const std::string gStr_mutationType;
extern const std::string gStr_position;
extern const std::string gStr_selectionCoeff;
extern const std::string gStr_subpopID;
extern const std::string gStr_originGeneration;
extern const std::string gStr_fixationTime;

extern const std::string gStr_property;
extern const std::string gStr_str;
extern const std::string gStr_changeRecombinationIntervals;
extern const std::string gStr_addMutations;
extern const std::string gStr_addNewDrawnMutation;
extern const std::string gStr_addNewMutation;
extern const std::string gStr_removeMutations;
extern const std::string gStr_changeGenomicElementType;
extern const std::string gStr_changeMutationFractions;
extern const std::string gStr_setSelectionCoeff;
extern const std::string gStr_changeDistribution;
extern const std::string gStr_files;
extern const std::string gStr_readFile;
extern const std::string gStr_writeFile;
extern const std::string gStr_addSubpop;
extern const std::string gStr_addSubpopSplit;
extern const std::string gStr_deregisterScriptBlock;
extern const std::string gStr_mutationFrequencies;
extern const std::string gStr_outputFixedMutations;
extern const std::string gStr_outputFull;
extern const std::string gStr_outputMutations;
extern const std::string gStr_readFromPopulationFile;
extern const std::string gStr_registerScriptEvent;
extern const std::string gStr_registerScriptFitnessCallback;
extern const std::string gStr_registerScriptMateChoiceCallback;
extern const std::string gStr_registerScriptModifyChildCallback;
extern const std::string gStr_changeMigrationRates;
extern const std::string gStr_changeSelfingRate;
extern const std::string gStr_changeSexRatio;
extern const std::string gStr_changeSubpopulationSize;
extern const std::string gStr_fitness;
extern const std::string gStr_outputMSSample;
extern const std::string gStr_outputSample;

extern const std::string gStr_if;
extern const std::string gStr_else;
extern const std::string gStr_do;
extern const std::string gStr_while;
extern const std::string gStr_for;
extern const std::string gStr_in;
extern const std::string gStr_next;
extern const std::string gStr_break;
extern const std::string gStr_return;

extern const std::string gStr_T;
extern const std::string gStr_F;
extern const std::string gStr_NULL;
extern const std::string gStr_PI;
extern const std::string gStr_E;
extern const std::string gStr_INF;
extern const std::string gStr_NAN;

extern const std::string gStr_void;
extern const std::string gStr_logical;
extern const std::string gStr_string;
extern const std::string gStr_integer;
extern const std::string gStr_float;
extern const std::string gStr_object;
extern const std::string gStr_numeric;

extern const std::string gStr_sim;
extern const std::string gStr_self;
extern const std::string gStr_genome1;
extern const std::string gStr_genome2;
extern const std::string gStr_subpop;
extern const std::string gStr_sourceSubpop;
extern const std::string gStr_weights;
extern const std::string gStr_childGenome1;
extern const std::string gStr_childGenome2;
extern const std::string gStr_childIsFemale;
extern const std::string gStr_parent1Genome1;
extern const std::string gStr_parent1Genome2;
extern const std::string gStr_isSelfing;
extern const std::string gStr_parent2Genome1;
extern const std::string gStr_parent2Genome2;
extern const std::string gStr_mut;
extern const std::string gStr_relFitness;
extern const std::string gStr_homozygous;

extern const std::string gStr_Chromosome;
extern const std::string gStr_Genome;
extern const std::string gStr_GenomicElement;
extern const std::string gStr_GenomicElementType;
extern const std::string gStr_Mutation;
extern const std::string gStr_MutationType;
extern const std::string gStr_Path;
extern const std::string gStr_undefined;
extern const std::string gStr_SLiMScriptBlock;
extern const std::string gStr_SLiMSim;
extern const std::string gStr_Subpopulation;
extern const std::string gStr_Substitution;

extern const std::string gStr_Autosome;
extern const std::string gStr_X_chromosome;
extern const std::string gStr_Y_chromosome;
extern const std::string gStr_event;
extern const std::string gStr_mateChoice;
extern const std::string gStr_modifyChild;
extern const std::string gStr_lessThanSign;
extern const std::string gStr_greaterThanSign;
extern const std::string gStr_GetValueForMemberOfElements;
extern const std::string gStr_ExecuteMethod;


#endif /* defined(__SLiM__slim_global__) */


















































