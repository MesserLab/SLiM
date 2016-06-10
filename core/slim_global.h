//
//  slim_global.h
//  SLiM
//
//  Created by Ben Haller on 1/4/15.
//  Copyright (c) 2014-2016 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/slim/
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

#include "eidos_global.h"


// This should be called once at startup to give SLiM an opportunity to initialize static state
void SLiM_WarmUp(void);


// *******************************************************************************************************************
//
//	Output handling for SLiM
//

// Output from SLiM can work in one of two ways.  If gEidosTerminateThrows == 0, ordinary output goes to cout,
// and error output goes to cerr.  The other mode has gEidosTerminateThrows == 1.  In that mode, we use a global
// ostringstream to capture all output to both the output and error streams.  This stream should get emptied out after
// every SLiM operation, so a single stream can be safely used by multiple SLiM instances (as long as we do not
// multithread).  Note that Eidos output goes into its own output stream, which SLiM empties into the SLiM output stream.
// Note also that termination output is handled separately, using EIDOS_TERMINATION.
extern std::ostringstream gSLiMOut;
#define SLIM_OUTSTREAM		(gEidosTerminateThrows ? gSLiMOut : std::cout)
#define SLIM_ERRSTREAM		(gEidosTerminateThrows ? gSLiMOut : std::cerr)


// *******************************************************************************************************************
//
//	Types and maximum values used by SLiM
//

// This is the standard setup for SLiM: 32 bit ints for most values.  This is recommended.  This gives a maximum of
// 1 billion for quantities such as object IDs, generations, population sizes, and chromosome positions.  This is
// comfortably under INT_MAX, which is a bit over 2 billion.  The goal is to try to avoid overflow bugs by keeping
// a large amount of headroom, so that we are not at risk of simple calculations with these quantities overflowing.
// Raising these limits to int64_t is reasonable if you need to run a larger simulation.  Lowering them to int16_t
// is not recommended, and will likely buy you very little, because most of the memory usage in typical simulations
// is in the arrays of pointers kept by Genome objects.  There are, for typical simulations, many pointers to a given
// mutation on average, so the memory used by a Mutation object is insignificant compared to all those 64-bit
// pointers pointing to that single Mutation object.  This means that the only thing that will really reduce SLiM's
// memory footprint significantly is to compile it with 32-bit pointers instead of 64-bit pointers; but that will
// limit you to 4GB of working memory, which is not that much, and SLiM has no checks on its memory usage, so you
// will likely experience random crashes due to running out of memory if you try that.  So, long story short, SLiM's
// memory usage is what it is, and is about as good as it could reasonably be.

typedef int32_t	slim_generation_t;	// generation numbers, generation durations
typedef int32_t	slim_position_t;	// chromosome positions, lengths in base pairs
typedef int32_t	slim_objectid_t;	// identifiers values for objects, like the "5" in p5, g5, m5, s5
typedef int32_t	slim_popsize_t;		// subpopulation sizes and indices, include genome indices
typedef int64_t slim_usertag_t;		// user-provided "tag" values; also used for the "active" property, which is like tag
typedef int32_t slim_refcount_t;	// mutation refcounts, counts of the number of occurrences of a mutation
typedef float slim_selcoeff_t;		// storage of selection coefficients in memory-tight classes; also dominance coefficients

#define SLIM_MAX_GENERATION		(1000000000L)	// generation ranges from 0 (init time) to this
#define SLIM_MAX_BASE_POSITION	(1000000000L)	// base positions in the chromosome can range from 0 to this
#define SLIM_MAX_ID_VALUE		(1000000000L)	// IDs for subpops, genomic elements, etc. can range from 0 to this
#define SLIM_MAX_SUBPOP_SIZE	(1000000000L)	// subpopulations can range in size from 0 to this; genome indexes, up to 2x this

// Functions for casting from Eidos ints (int64_t) to SLiM int types safely; not needed for slim_refcount_t at present
void SLiMRaiseGenerationRangeError(int64_t p_long_value);
void SLiMRaisePositionRangeError(int64_t p_long_value);
void SLiMRaiseObjectidRangeError(int64_t p_long_value);
void SLiMRaisePopsizeRangeError(int64_t p_long_value);
void SLiMRaiseUsertagRangeError(int64_t p_long_value);

inline __attribute__((always_inline)) slim_generation_t SLiMCastToGenerationTypeOrRaise(int64_t p_long_value)
{
	if ((p_long_value < 1) || (p_long_value > SLIM_MAX_GENERATION))
		SLiMRaiseGenerationRangeError(p_long_value);
	
	return static_cast<slim_generation_t>(p_long_value);
}

inline __attribute__((always_inline)) slim_position_t SLiMCastToPositionTypeOrRaise(int64_t p_long_value)
{
	if ((p_long_value < 0) || (p_long_value > SLIM_MAX_BASE_POSITION))
		SLiMRaisePositionRangeError(p_long_value);
	
	return static_cast<slim_position_t>(p_long_value);
}

inline __attribute__((always_inline)) slim_objectid_t SLiMCastToObjectidTypeOrRaise(int64_t p_long_value)
{
	if ((p_long_value < 0) || (p_long_value > SLIM_MAX_ID_VALUE))
		SLiMRaiseObjectidRangeError(p_long_value);
	
	return static_cast<slim_objectid_t>(p_long_value);
}

inline __attribute__((always_inline)) slim_popsize_t SLiMCastToPopsizeTypeOrRaise(int64_t p_long_value)
{
	if ((p_long_value < 0) || (p_long_value > SLIM_MAX_SUBPOP_SIZE))
		SLiMRaisePopsizeRangeError(p_long_value);
	
	return static_cast<slim_popsize_t>(p_long_value);
}

inline __attribute__((always_inline)) slim_usertag_t SLiMCastToUsertagTypeOrRaise(int64_t p_long_value)
{
	// no range check at present since slim_usertag_t is in fact int64_t; it is in range by definition
	// SLiMRaiseUsertagRangeError(long_value);
	
	return static_cast<slim_usertag_t>(p_long_value);
}

inline __attribute__((always_inline)) slim_generation_t SLiMClampToGenerationType(int64_t p_long_value)
{
	if (p_long_value < 1)
		return 1;
	if (p_long_value > SLIM_MAX_GENERATION)
		return SLIM_MAX_GENERATION;
	return static_cast<slim_generation_t>(p_long_value);
}

inline __attribute__((always_inline)) slim_position_t SLiMClampToPositionType(int64_t p_long_value)
{
	if (p_long_value < 0)
		return 0;
	if (p_long_value > SLIM_MAX_BASE_POSITION)
		return SLIM_MAX_BASE_POSITION;
	return static_cast<slim_position_t>(p_long_value);
}

inline __attribute__((always_inline)) slim_objectid_t SLiMClampToObjectidType(int64_t p_long_value)
{
	if (p_long_value < 0)
		return 0;
	if (p_long_value > SLIM_MAX_ID_VALUE)
		return SLIM_MAX_ID_VALUE;
	return static_cast<slim_objectid_t>(p_long_value);
}

inline __attribute__((always_inline)) slim_popsize_t SLiMClampToPopsizeType(int64_t p_long_value)
{
	if (p_long_value < 0)
		return 0;
	if (p_long_value > SLIM_MAX_SUBPOP_SIZE)
		return SLIM_MAX_SUBPOP_SIZE;
	return static_cast<slim_popsize_t>(p_long_value);
}

inline __attribute__((always_inline)) slim_usertag_t SLiMClampToUsertagType(int64_t p_long_value)
{
	// no range check at present since slim_usertag_t is in fact int64_t; it is in range by definition
	return static_cast<slim_usertag_t>(p_long_value);
}


// *******************************************************************************************************************
//
//	Debugging support
//

// Debugging #defines that can be turned on
#define DEBUG_MUTATIONS			0		// turn on logging of mutation construction and destruction
#define DEBUG_MUTATION_ZOMBIES	0		// avoid destroying Mutation objects; keep them as zombies
#define DEBUG_INPUT				1		// additional output for debugging of input file parsing


// *******************************************************************************************************************
//
//	Shared SLiM types and enumerations
//

// This enumeration represents the type of chromosome represented by a genome: autosome, X, or Y.  Note that this is somewhat
// separate from the sex of the individual; one can model sexual individuals but model only an autosome, in which case the sex
// of the individual cannot be determined from its modeled genome.
enum class GenomeType {
	kAutosome = 0,
	kXChromosome,
	kYChromosome
};

std::ostream& operator<<(std::ostream& p_out, GenomeType p_genome_type);


// This enumeration represents the sex of an individual: hermaphrodite, female, or male.  It also includes an "unspecified"
// value that is useful in situations where the code wants to say that it doesn't care what sex is present.
enum class IndividualSex
{
	kUnspecified = -2,
	kHermaphrodite = -1,
	kFemale = 0,
	kMale = 1
};

std::ostream& operator<<(std::ostream& p_out, IndividualSex p_sex);


// *******************************************************************************************************************
//
//	Global strings and IDs
//

//
//	Additional global std::string objects.  See script_globals.h for details.
//

void SLiM_RegisterGlobalStringsAndIDs(void);


extern const std::string gStr_initializeGenomicElement;
extern const std::string gStr_initializeGenomicElementType;
extern const std::string gStr_initializeMutationType;
extern const std::string gStr_initializeGeneConversion;
extern const std::string gStr_initializeMutationRate;
extern const std::string gStr_initializeRecombinationRate;
extern const std::string gStr_initializeSex;

extern const std::string gStr_genomicElements;
extern const std::string gStr_lastPosition;
extern const std::string gStr_overallRecombinationRate;
extern const std::string gStr_recombinationEndPositions;
extern const std::string gStr_recombinationRates;
extern const std::string gStr_geneConversionFraction;
extern const std::string gStr_geneConversionMeanLength;
extern const std::string gStr_mutationRate;
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
extern const std::string gStr_convertToSubstitution;
extern const std::string gStr_distributionType;
extern const std::string gStr_distributionParams;
extern const std::string gStr_dominanceCoeff;
extern const std::string gStr_mutationStackPolicy;
extern const std::string gStr_start;
extern const std::string gStr_end;
extern const std::string gStr_type;
extern const std::string gStr_source;
extern const std::string gStr_active;
extern const std::string gStr_chromosome;
extern const std::string gStr_chromosomeType;
extern const std::string gStr_genomicElementTypes;
extern const std::string gStr_scriptBlocks;
extern const std::string gStr_sexEnabled;
extern const std::string gStr_subpopulations;
extern const std::string gStr_substitutions;
extern const std::string gStr_dominanceCoeffX;
extern const std::string gStr_generation;
extern const std::string gStr_tag;
extern const std::string gStr_firstMaleIndex;
extern const std::string gStr_genomes;
extern const std::string gStr_individuals;
extern const std::string gStr_subpopulation;
extern const std::string gStr_index;
extern const std::string gStr_immigrantSubpopIDs;
extern const std::string gStr_immigrantSubpopFractions;
extern const std::string gStr_selfingRate;
extern const std::string gStr_cloningRate;
extern const std::string gStr_sexRatio;
extern const std::string gStr_individualCount;
extern const std::string gStr_fixationGeneration;

extern const std::string gStr_setRecombinationRate;
extern const std::string gStr_addMutations;
extern const std::string gStr_addNewDrawnMutation;
extern const std::string gStr_addNewMutation;
extern const std::string gStr_containsMutations;
extern const std::string gStr_countOfMutationsOfType;
extern const std::string gStr_mutationsOfType;
extern const std::string gStr_removeMutations;
extern const std::string gStr_setGenomicElementType;
extern const std::string gStr_setMutationFractions;
extern const std::string gStr_setSelectionCoeff;
extern const std::string gStr_setMutationType;
extern const std::string gStr_setDistribution;
extern const std::string gStr_addSubpop;
extern const std::string gStr_addSubpopSplit;
extern const std::string gStr_deregisterScriptBlock;
extern const std::string gStr_mutationFrequencies;
//extern const std::string gStr_mutationsOfType;
//extern const std::string gStr_countOfMutationsOfType;
extern const std::string gStr_outputFixedMutations;
extern const std::string gStr_outputFull;
extern const std::string gStr_outputMutations;
extern const std::string gStr_readFromPopulationFile;
extern const std::string gStr_recalculateFitness;
extern const std::string gStr_registerEarlyEvent;
extern const std::string gStr_registerLateEvent;
extern const std::string gStr_registerFitnessCallback;
extern const std::string gStr_registerMateChoiceCallback;
extern const std::string gStr_registerModifyChildCallback;
extern const std::string gStr_simulationFinished;
extern const std::string gStr_setMigrationRates;
extern const std::string gStr_setCloningRate;
extern const std::string gStr_setSelfingRate;
extern const std::string gStr_setSexRatio;
extern const std::string gStr_setSubpopulationSize;
extern const std::string gStr_cachedFitness;
extern const std::string gStr_outputMSSample;
extern const std::string gStr_outputVCFSample;
extern const std::string gStr_outputSample;

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
extern const std::string gStr_isCloning;
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
extern const std::string gStr_SLiMEidosBlock;
extern const std::string gStr_SLiMSim;
extern const std::string gStr_Subpopulation;
extern const std::string gStr_Individual;
extern const std::string gStr_Substitution;

extern const std::string gStr_A;
extern const std::string gStr_X;
extern const std::string gStr_Y;
extern const std::string gStr_f;
extern const std::string gStr_g;
extern const std::string gStr_e;
extern const std::string gStr_n;
extern const std::string gStr_w;
extern const std::string gStr_l;
extern const std::string gStr_s;
extern const std::string gStr_early;
extern const std::string gStr_late;
extern const std::string gStr_initialize;
extern const std::string gStr_fitness;
extern const std::string gStr_mateChoice;
extern const std::string gStr_modifyChild;


enum _SLiMGlobalStringID : int {
	gID_initializeGenomicElement = gEidosID_LastEntry + 1,
	gID_initializeGenomicElementType,
	gID_initializeMutationType,
	gID_initializeGeneConversion,
	gID_initializeMutationRate,
	gID_initializeRecombinationRate,
	gID_initializeSex,
	
	gID_genomicElements,
	gID_lastPosition,
	gID_overallRecombinationRate,
	gID_recombinationEndPositions,
	gID_recombinationRates,
	gID_geneConversionFraction,
	gID_geneConversionMeanLength,
	gID_mutationRate,
	gID_genomeType,
	gID_isNullGenome,
	gID_mutations,
	gID_genomicElementType,
	gID_startPosition,
	gID_endPosition,
	gID_id,
	gID_mutationTypes,
	gID_mutationFractions,
	gID_mutationType,
	gID_originGeneration,
	gID_position,
	gID_selectionCoeff,
	gID_subpopID,
	gID_convertToSubstitution,
	gID_distributionType,
	gID_distributionParams,
	gID_dominanceCoeff,
	gID_mutationStackPolicy,
	gID_start,
	gID_end,
	gID_type,
	gID_source,
	gID_active,
	gID_chromosome,
	gID_chromosomeType,
	gID_genomicElementTypes,
	gID_scriptBlocks,
	gID_sexEnabled,
	gID_subpopulations,
	gID_substitutions,
	gID_dominanceCoeffX,
	gID_generation,
	gID_tag,
	gID_firstMaleIndex,
	gID_genomes,
	gID_individuals,
	gID_subpopulation,
	gID_index,
	gID_immigrantSubpopIDs,
	gID_immigrantSubpopFractions,
	gID_selfingRate,
	gID_cloningRate,
	gID_sexRatio,
	gID_individualCount,
	gID_fixationGeneration,
	
	gID_setRecombinationRate,
	gID_addMutations,
	gID_addNewDrawnMutation,
	gID_addNewMutation,
	gID_containsMutations,
	gID_countOfMutationsOfType,
	gID_mutationsOfType,
	gID_removeMutations,
	gID_setGenomicElementType,
	gID_setMutationFractions,
	gID_setSelectionCoeff,
	gID_setMutationType,
	gID_setDistribution,
	gID_addSubpop,
	gID_addSubpopSplit,
	gID_deregisterScriptBlock,
	gID_mutationFrequencies,
	//gID_mutationsOfType,
	//gID_countOfMutationsOfType,
	gID_outputFixedMutations,
	gID_outputFull,
	gID_outputMutations,
	gID_readFromPopulationFile,
	gID_recalculateFitness,
	gID_registerEarlyEvent,
	gID_registerLateEvent,
	gID_registerFitnessCallback,
	gID_registerMateChoiceCallback,
	gID_registerModifyChildCallback,
	gID_simulationFinished,
	gID_setMigrationRates,
	gID_setCloningRate,
	gID_setSelfingRate,
	gID_setSexRatio,
	gID_setSubpopulationSize,
	gID_cachedFitness,
	gID_outputMSSample,
	gID_outputVCFSample,
	gID_outputSample,
	
	gID_sim,
	gID_self,
	gID_genome1,
	gID_genome2,
	gID_subpop,
	gID_sourceSubpop,
	gID_weights,
	gID_childGenome1,
	gID_childGenome2,
	gID_childIsFemale,
	gID_parent1Genome1,
	gID_parent1Genome2,
	gID_isCloning,
	gID_isSelfing,
	gID_parent2Genome1,
	gID_parent2Genome2,
	gID_mut,
	gID_relFitness,
	gID_homozygous,
	
	gID_Chromosome,
	gID_Genome,
	gID_GenomicElement,
	gID_GenomicElementType,
	gID_Mutation,
	gID_MutationType,
	gID_SLiMEidosBlock,
	gID_SLiMSim,
	gID_Subpopulation,
	gID_Individual,
	gID_Substitution,
	
	gID_A,
	gID_X,
	gID_Y,
	gID_f,
	gID_g,
	gID_e,
	gID_n,
	gID_w,
	gID_l,
	gID_s,
	gID_early,
	gID_late,
	gID_initialize,
	gID_fitness,
	gID_mateChoice,
	gID_modifyChild,
};


#endif /* defined(__SLiM__slim_global__) */


















































