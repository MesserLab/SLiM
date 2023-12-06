//
//  slim_globals.h
//  SLiM
//
//  Created by Ben Haller on 1/4/15.
//  Copyright (c) 2015-2023 Philipp Messer.  All rights reserved.
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

#ifndef __SLiM__slim_globals__
#define __SLiM__slim_globals__


#include <stdio.h>
#include <cstdint>

#include "eidos_globals.h"
#include "eidos_value.h"

class MutationType;
class Community;
class Species;
class EidosInterpreter;
class Population;
class GenomicElementType;
class Subpopulation;
class SLiMEidosBlock;


// SLiM version: see also Info.plist and QtSLiM.pro
#define SLIM_VERSION_STRING	("4.1")
#define SLIM_VERSION_FLOAT	(4.1)


// This should be called once at startup to give SLiM an opportunity to initialize static state
void SLiM_WarmUp(void);


// *******************************************************************************************************************
//
//	Global optimization flags
//
#pragma mark -
#pragma mark Global optimization flags
#pragma mark -

// If defined, MutationType will keep its own registry of all mutations of that type, under certain circumstances.
// See mutation_type.h for more information on this optimization.
#define SLIM_KEEP_MUTTYPE_REGISTRIES


// *******************************************************************************************************************
//
//	Output handling for SLiM
//
#pragma mark -
#pragma mark Output handling
#pragma mark -

// Output from SLiM can work in one of two ways.  If gEidosTerminateThrows == 0, ordinary output goes to cout,
// and error output goes to cerr.  The other mode has gEidosTerminateThrows == 1.  In that mode, we use global
// ostringstreams to capture all output to both the output and error streams.  These streams should get emptied out after
// every SLiM operation, so a single stream can be safely used by multiple SLiM instances (as long as we do not
// multithread).  Note that Eidos output goes into its own output streams, which SLiM empties into the SLiM output streams.
// Note also that termination output is handled separately, using EIDOS_TERMINATION.
extern std::ostringstream gSLiMOut;
extern std::ostringstream gSLiMError;
#define SLIM_OUTSTREAM		(gEidosTerminateThrows ? gSLiMOut : std::cout)
#define SLIM_ERRSTREAM		(gEidosTerminateThrows ? gSLiMError : std::cerr)

#ifdef SLIMGUI
// When running under SLiMgui, we can have additional output streams that do not exist when running at the command line
extern std::ostringstream gSLiMScheduling;		// information about scheduling in each tick
#endif


// *******************************************************************************************************************
//
//	Types and maximum values used by SLiM
//
#pragma mark -
#pragma mark Types and max values
#pragma mark -

// This is the standard setup for SLiM: 32 bit ints for most values.  This is recommended.  This gives a maximum of
// 1 billion for quantities such as object IDs, ticks, population sizes, and chromosome positions.  This is
// comfortably under INT_MAX, which is a bit over 2 billion.  The goal is to try to avoid overflow bugs by keeping
// a large amount of headroom, so that we are not at risk of simple calculations with these quantities overflowing.
// Raising these limits to int64_t is reasonable if you need to run a larger simulation.  Lowering them to int16_t
// is not recommended, and will likely buy you very little, because most of the memory usage in typical simulations
// is in the arrays of mutation indices kept by Genome objects.
// BCH 11 May 2018: changing slim_position_t to int64_t for SLiM 3; L <= 1e9 was a bit limiting.  We now enforce a
// maximum position of 1e15.  INT64_MAX is almost 1e19, so this may seem arbitrary.  The reason is that we want to
// interact well with code, such as the tree-sequence code, that keeps positions as doubles.  The IEEE standard for
// double mandates 52 bits for the fractional part, which means that the ability to uniquely identify every integer
// position breaks down a little bit shy of 1e16.  Thus we limit to 1e15, as a round limit with lots of headroom.

typedef int32_t	slim_tick_t;			// tick numbers, tick durations
typedef int32_t	slim_age_t;				// individual ages which may be from zero on up
typedef int64_t	slim_position_t;		// chromosome positions, lengths in base pairs
typedef int64_t slim_mutrun_index_t;	// indices of mutation runs within genomes; SLIM_INF_BASE_POSITION leads to very large values, thus 64-bit
typedef int32_t	slim_objectid_t;		// identifiers values for objects, like the "5" in p5, g5, m5, s5
typedef int32_t	slim_popsize_t;			// subpopulation sizes and indices, include genome indices
typedef int64_t slim_usertag_t;			// user-provided "tag" values; also used for the "active" property, which is like tag
typedef int32_t slim_refcount_t;		// mutation refcounts, counts of the number of occurrences of a mutation
typedef int64_t slim_mutationid_t;		// identifiers for mutations, which require 64 bits since there can be so many
typedef int64_t slim_pedigreeid_t;		// identifiers for pedigreed individuals; over many ticks in a large model maybe 64 bits?
typedef int64_t slim_genomeid_t;		// identifiers for pedigreed genomes; not user-visible, used by the tree-recording code, pedigree_id*2 + [0/1]
typedef int32_t slim_polymorphismid_t;	// identifiers for polymorphisms, which need only 32 bits since they are only segregating mutations
typedef float slim_selcoeff_t;			// storage of selection coefficients in memory-tight classes; also dominance coefficients

#define SLIM_MAX_TICK			(1000000000L)	// ticks range from 0 (init time) to this; SLIM_MAX_TICK + 1 is an "infinite" marker value
#define SLIM_MAX_BASE_POSITION	(1000000000000000L)	// base positions in the chromosome can range from 0 to 1e15; see above
#define SLIM_INF_BASE_POSITION	(1100000000000000L)	// used to represent a base position infinitely beyond the end of the chromosome
#define SLIM_MAX_PEDIGREE_ID	(1000000000000000000L)	// pedigree IDs for individuals can range from 0 to 1e18 (~2^60)
#define SLIM_MAX_ID_VALUE		(1000000000L)	// IDs for subpops, genomic elements, etc. can range from 0 to this
#define SLIM_MAX_SUBPOP_SIZE	(1000000000L)	// subpopulations can range in size from 0 to this; genome indexes, up to 2x this
#define SLIM_TAG_UNSET_VALUE	(INT64_MIN)		// for tags of type slim_usertag_t, the flag value for "unset"
#define SLIM_TAGF_UNSET_VALUE	(-DBL_MAX)		// for tags of type double (i.e. tagF), the flag value for "unset"

// Functions for casting from Eidos ints (int64_t) to SLiM int types safely; not needed for slim_refcount_t at present
void SLiM_RaiseTickRangeError(int64_t p_long_value);
void SLiM_RaiseAgeRangeError(int64_t p_long_value);
void SLiM_RaisePositionRangeError(int64_t p_long_value);
void SLiM_RaisePedigreeIDRangeError(int64_t p_long_value);
void SLiM_RaiseObjectidRangeError(int64_t p_long_value);
void SLiM_RaisePopsizeRangeError(int64_t p_long_value);
void SLiM_RaiseUsertagRangeError(int64_t p_long_value);
void SLiM_RaisePolymorphismidRangeError(int64_t p_long_value);

inline __attribute__((always_inline)) slim_tick_t SLiMCastToTickTypeOrRaise(int64_t p_long_value)
{
	if ((p_long_value < 1) || (p_long_value > SLIM_MAX_TICK))
		SLiM_RaiseTickRangeError(p_long_value);
	
	return static_cast<slim_tick_t>(p_long_value);
}

inline __attribute__((always_inline)) slim_age_t SLiMCastToAgeTypeOrRaise(int64_t p_long_value)
{
	if ((p_long_value < 0) || (p_long_value > SLIM_MAX_TICK))
		SLiM_RaiseAgeRangeError(p_long_value);
	
	return static_cast<slim_age_t>(p_long_value);
}

inline __attribute__((always_inline)) slim_position_t SLiMCastToPositionTypeOrRaise(int64_t p_long_value)
{
	if ((p_long_value < 0) || (p_long_value > SLIM_MAX_BASE_POSITION))
		SLiM_RaisePositionRangeError(p_long_value);
	
	return static_cast<slim_position_t>(p_long_value);
}

inline __attribute__((always_inline)) slim_pedigreeid_t SLiMCastToPedigreeIDOrRaise(int64_t p_long_value)
{
	if ((p_long_value < 0) || (p_long_value > SLIM_MAX_PEDIGREE_ID))
		SLiM_RaisePedigreeIDRangeError(p_long_value);
	
	return static_cast<slim_pedigreeid_t>(p_long_value);
}

inline __attribute__((always_inline)) slim_objectid_t SLiMCastToObjectidTypeOrRaise(int64_t p_long_value)
{
	if ((p_long_value < 0) || (p_long_value > SLIM_MAX_ID_VALUE))
		SLiM_RaiseObjectidRangeError(p_long_value);
	
	return static_cast<slim_objectid_t>(p_long_value);
}

inline __attribute__((always_inline)) slim_popsize_t SLiMCastToPopsizeTypeOrRaise(int64_t p_long_value)
{
	if ((p_long_value < 0) || (p_long_value > SLIM_MAX_SUBPOP_SIZE))
		SLiM_RaisePopsizeRangeError(p_long_value);
	
	return static_cast<slim_popsize_t>(p_long_value);
}

inline __attribute__((always_inline)) slim_usertag_t SLiMCastToUsertagTypeOrRaise(int64_t p_long_value)
{
	// no range check at present since slim_usertag_t is in fact int64_t; it is in range by definition
	// SLiM_RaiseUsertagRangeError(long_value);
	
	return static_cast<slim_usertag_t>(p_long_value);
}

inline __attribute__((always_inline)) slim_polymorphismid_t SLiMCastToPolymorphismidTypeOrRaise(int64_t p_long_value)
{
	if ((p_long_value < 0) || (p_long_value > INT32_MAX))
		SLiM_RaisePolymorphismidRangeError(p_long_value);
	
	return static_cast<slim_polymorphismid_t>(p_long_value);
}

inline __attribute__((always_inline)) slim_tick_t SLiMClampToTickType(int64_t p_long_value)
{
	if (p_long_value < 1)
		return 1;
	if (p_long_value > SLIM_MAX_TICK)
		return SLIM_MAX_TICK;
	return static_cast<slim_tick_t>(p_long_value);
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

Community &SLiM_GetCommunityFromInterpreter(EidosInterpreter &p_interpreter);
slim_objectid_t SLiM_ExtractObjectIDFromEidosValue_is(EidosValue *p_value, int p_index, char p_prefix_char);

// These take both a Community and a Species.  If the species is non-nullptr, the lookup is done in that species
// and the community is not used (and may be nullptr).  If the species is nullptr, the lookup is in the community,
// and an object in any species will be found.  So: supply a species if you want a species-specific search.
MutationType *SLiM_ExtractMutationTypeFromEidosValue_io(EidosValue *p_value, int p_index, Community *p_community, Species *p_species, const char *p_method_name);
GenomicElementType *SLiM_ExtractGenomicElementTypeFromEidosValue_io(EidosValue *p_value, int p_index, Community *p_community, Species *p_species, const char *p_method_name);
Subpopulation *SLiM_ExtractSubpopulationFromEidosValue_io(EidosValue *p_value, int p_index, Community *p_community, Species *p_species, const char *p_method_name);
SLiMEidosBlock *SLiM_ExtractSLiMEidosBlockFromEidosValue_io(EidosValue *p_value, int p_index, Community *p_community, Species *p_species, const char *p_method_name);

Species *SLiM_ExtractSpeciesFromEidosValue_No(EidosValue *p_value, int p_index, Community *p_community, const char *p_method_name);		// NULL tries for a single-species default


// *******************************************************************************************************************
//
//	Memory management
//
#pragma mark -
#pragma mark Memory management
#pragma mark -

/*
 
 Memory management in SLiM is a complex topic which I'll try to summarize here.  Classes that are visible in Eidos
 descend from EidosObject.  Most of these have their ownership and lifetime managed by the simulation; whenever an
 Individual dies, for example, the Individual object ceases to exist at that time, and is disposed of.  A subclass
 of EidosObject, EidosDictionaryRetained, provides optional retain/release style memory management for all objects
 visible in Eidos; Chromosome, Mutation, Substitution, and LogFile utilize this facility, and can therefore all be
 held onto long-term in Eidos with defineConstant() or setValue().  In either case, objects might be allocated out
 of several different pools: some objects are allocated with new, some out of EidosObjectPool.  EidosValue objects
 themselves (which can contain pointers to EidosObjects) are always allocated from a single global EidosObjectPool
 that is shared across the process, gEidosValuePool, and EidosASTNodes are similarly allocated from another global
 pool, gEidosASTNodePool.  Here are details on the memory management scheme for various SLiM classes (note that by
 "never deleted", this means not deleted within a run of a simulation; in SLiMgui they can be deleted):
 
 EidosValue : no base class; allocated from a shared pool, held under retain/release with Eidos_intrusive_ptr
 EidosObject : base class for all Eidos classes that are visible as objects in SLiM scripts
 EidosDictionaryUnretained : EidosObject subclass that provides dictionary-style key-value Eidos methods
 EidosDictionaryRetained : EidosDictionaryUnretained subclass that provides retain/release memory management
 
 GenomicElement : EidosObject subclass, allocated with new and never deleted
 SLiMgui : EidosDictionaryUnretained subclass, allocated with new and never deleted
 GenomicElementType : EidosDictionaryUnretained subclass, allocated with new and never deleted
 MutationType : EidosDictionaryUnretained subclass, allocated with new and never deleted
 InteractionType : EidosDictionaryUnretained subclass, allocated with new and never deleted
 Community : EidosDictionaryUnretained subclass, allocated with new and never deleted
 Species : EidosDictionaryUnretained subclass, allocated with new and never deleted
 SLiMEidosBlock : EidosObject subclass, dynamic lifetime with a deferred deletion scheme in Community
 
 MutationRun : no superclass, not visible in Eidos, shared by Genome, private pools for very efficient reuse
 Genome : EidosObject subclass, allocated out of an EidosObjectPool owned by its subpopulation
 Individual : EidosDictionaryUnretained subclass, allocated out of an EidosObjectPool owned by its subpopulation
 Subpopulation : EidosDictionaryUnretained subclass, allocated with new/delete
 
 Chromosome : EidosDictionaryRetained subclass, allocated with new/delete
 Substitution : EidosDictionaryRetained subclass, allocated with new/delete
 Mutation : EidosDictionaryRetained subclass, allocated out of a special global pool, gSLiM_Mutation_Block
 LogFile : EidosDictionaryRetained subclass, allocated with new/delete

 The dynamics of Mutation are unusual and require further discussion.  The global shared gSLiM_Mutation_Block pool
 is used instead of EidosObjectPool because all mutations must be allocated out of a single contiguous memory bloc
 in order to be indexable with MutationIndex (whereas EidosObjectPool dynamically creates new blocs).  This allows
 references to mutations in MutationRun to be done with 32-bit MutationIndexes rather than 64-bit pointers, giving
 better performance.  Mutation, as a subclass of EidosDictionaryRetained, is under retain/release, but the uses of
 Mutation inside SLiM's core do not cause retain/release activity, for efficiency; instead, the population keeps a
 "registry" of mutations that are currently segregating, and the registry holds a retain on all of those mutations
 on behalf of the entire SLiM core.  Normally, when a mutation is lost or fixes and gets removed from the registry
 that retain is released and the mutation is given back to the shared pool.  This only changes if a reference to a
 mutation is kept in an EidosValue, which will apply another retain; if that EidosValue is persistent, as a result
 of being kept by defineConstant() or setValue(), then when the registry releases the mutation the EidosValue will
 still have a retain and the mutation will live on, even though it is no longer referenced by the simulation.  The
 Mutation class now keeps an ivar, state_, that keeps track of whether a mutation is still in the registry, or has
 been lost or fixed.
 
 MutationRun is similarly complex.  It is not visible in Eidos, but a single MutationRun can be shared by multiple
 Genomes, so it keeps a refcount that is updated each tick by tallying usage across genomes.  All MutationRuns are
 allocated out of a single pool per species.  When their refcount goes to zero they do not get destructed; instead
 they are returned to a per-species "freed list" while still in a constructed state, allowing extremely fast reuse
 since they are a central bottleneck in most SLiM models.
 
 In summary, there are two different retain/release schemes in SLiM, one run by Eidos_intrusive_ptr and one run by
 EidosDictionaryRetained.  Eidos_intrusive_ptr is a template-based solution that can be incorporated in class with
 the declaration of a counter and two friend functions, providing a very general solution.  In contrast, the class
 EidosDictionaryRetained provides a retain/release scheme that's tightly integrated into EidosValue's design, with
 explicit calls to retain/release instead of the automatic "shared pointer" design of Eidos_intrusive_ptr.
 
 Other SLiM Eidos objects could conceivably be brought under EidosDictionaryRetained, allowing them to be retained
 long-term like Mutation and Substitution.  However, this is tricky with objects that have a lifetime delimited by
 the simulation itself, like Subpopulation; it would be hard to keep a Subpopulation object alive beyond its usual
 lifetime while remaining useful and functional.  There is also a substantial speed penalty to being under retain/
 release; manipulating large vectors of Mutation objects in Eidos is now ~3x slower than it used to be.  The large
 advantages of being able to keep Mutation objects long-term seemed to justify this; no similar advantages seem to
 exist for the other SLiM classes that would justify this additional overhead.
 
 BCH 3/28/2023: Apropos of the previous paragraph, I looked into having the managed-lifetime objects that live for
 the whole simulation (Community, Species, GenomicElementType, MutationType, InteractionType, SLiMgui) behave like
 they're under retain-release (so they can be put into Dictionaries and defined as global constants and so forth),
 but it proved difficult and fragile.  There are two choices for this.  You could actually put these objects under
 retain-release, and design things so that at the end of the simulation they get a Release() call and that trigger
 causes their deallocation.  The problem with that is that retained references to them - even their self_symbol_ -
 make it so they don't actually get deallocated at the point in time that they should be, and so they leak.  A way
 to break retain cycles would be needed, but that's a very complex thing.  The other possible approach is for them
 to just pretend to be under retain-release, but to get forced to dealloc at the end of the simulation, regardless
 of their retain count.  The problem in this case is that Release() calls will come in for the objects after their
 deallocation; trying to prevent that by chasing down and clearing every retained reference before deallocation is
 a very complex thing too.  What is really needed is a weak-reference scheme where only the simulation's reference
 is strong, and all other references are weak and get cleared when the strong reference is released; but again, it
 is a very complex thing to implement.  In the end I decided it wasn't worth the time, effort, and complexity.
 
 BCH 5/24/2022: Adding a new note regarding memory policy in multispecies SLiM.  The above points still apply, but
 it is worth emphasizing that the shared global pools now apply across species.  Multiple species share a pool for
 Mutation objects, and a pool for MutationRun objects, in other words; they do not share their pools of Individual
 and Genome objects, however, as those are kept by the Population.  This is the simplest design, and seems to work
 fine.  It might provide greater memory locality benefits for each species to have its own pool, but that would be
 more than offset by the added complexity of having to walk up to the species to get the active pool.  This design
 means that mutaton indexes and gSLiM_next_mutation_id are shared across species; a given mutation index is unique
 not only within the species, but across species.  Similarly, the "bulk operation" facilities of MutationRun share
 globals used by all species, so a bulk operation can only be in progress for one species at a time.  Other shared
 globals include s_freed_sparse_vectors_ (which keeps freed SparseVector objects for reuse by InteractionType) and
 gSLiM_next_pedigree_id (which keeps the next pedigree ID to be used).  This implies that pedigree IDs are uniqued
 across species, like mutation IDs, which in turn implies that genome IDs used in tree-sequence recording are too,
 given the invariant relationship between pedigree and genome IDs.
 
 */

#define DEBUG_MUTATIONS				0		// turn on logging of mutation construction and destruction

// Per-species memory usage assessment as done by Species::TabulateSLiMMemoryUsage_Species() is placed into this struct
typedef struct
{
	int64_t chromosomeObjects_count;
	size_t chromosomeObjects;
	size_t chromosomeMutationRateMaps;
	size_t chromosomeRecombinationRateMaps;
	size_t chromosomeAncestralSequence;
	
	int64_t genomeObjects_count;
	size_t genomeObjects;
	size_t genomeExternalBuffers;
	size_t genomeUnusedPoolSpace;				// this pool is kept by Population, per-species
	size_t genomeUnusedPoolBuffers;
	
	int64_t genomicElementObjects_count;
	size_t genomicElementObjects;
	
	int64_t genomicElementTypeObjects_count;
	size_t genomicElementTypeObjects;
	
	int64_t individualObjects_count;
	size_t individualObjects;
	size_t individualUnusedPoolSpace;			// this pool is kept by Population, per-species
	
	int64_t mutationObjects_count;
	size_t mutationObjects;
	
	int64_t mutationRunObjects_count;
	size_t mutationRunObjects;
	size_t mutationRunExternalBuffers;
	size_t mutationRunNonneutralCaches;
	size_t mutationRunUnusedPoolSpace;			// this pool is kept by Species
	size_t mutationRunUnusedPoolBuffers;		// this pool is kept by Species
	
	int64_t mutationTypeObjects_count;
	size_t mutationTypeObjects;
	
	int64_t speciesObjects_count;
	size_t speciesObjects;
	size_t speciesTreeSeqTables;
	
	int64_t subpopulationObjects_count;
	size_t subpopulationObjects;
	size_t subpopulationFitnessCaches;
	size_t subpopulationParentTables;
	size_t subpopulationSpatialMaps;
	size_t subpopulationSpatialMapsDisplay;
	
	int64_t substitutionObjects_count;
	size_t substitutionObjects;
	
	size_t totalMemoryUsage;
} SLiMMemoryUsage_Species;

// Community-wide memory usage assessment as done by Community::TabulateSLiMMemoryUsage_Community() is placed into this struct
typedef struct
{
	int64_t communityObjects_count;
	size_t communityObjects;
	
	size_t mutationRefcountBuffer;			// this pool is kept globally by Mutation
	size_t mutationUnusedPoolSpace;			// this pool is kept globally by Mutation
	
	int64_t interactionTypeObjects_count;	// InteractionType is kept by Community now
	size_t interactionTypeObjects;
	size_t interactionTypeKDTrees;
	size_t interactionTypePositionCaches;
	size_t interactionTypeSparseVectorPool;	// This pool is kept globally by InteractionType
	
	size_t eidosASTNodePool;				// this pool is kept globally by Eidos
	size_t eidosSymbolTablePool;			// this pool is kept globally by EidosSymbolTable
	size_t eidosValuePool;					// this pool is kept globally by Eidos
	size_t fileBuffers;						// these buffers are kept globally by Eidos
	
	size_t totalMemoryUsage;
} SLiMMemoryUsage_Community;

void SumUpMemoryUsage_Species(SLiMMemoryUsage_Species &p_usage);
void SumUpMemoryUsage_Community(SLiMMemoryUsage_Community &p_usage);

void AccumulateMemoryUsageIntoTotal_Species(SLiMMemoryUsage_Species &p_usage, SLiMMemoryUsage_Species &p_total);
void AccumulateMemoryUsageIntoTotal_Community(SLiMMemoryUsage_Community &p_usage, SLiMMemoryUsage_Community &p_total);


// *******************************************************************************************************************
//
//	Debugging support
//
#pragma mark -
#pragma mark Debugging support
#pragma mark -

// Debugging #defines that can be turned on
#define DEBUG_MUTATION_ZOMBIES		0		// avoid destroying Mutation objects; keep them as zombies
#define SLIM_DEBUG_MUTATION_RUNS	0		// turn on to get logging about mutation run uniquing and usage
#define DEBUG_BLOCK_REG_DEREG		0		// turn on to get logging about script block registration/deregistration


// In SLiMgui we want to emit only a reasonably limited number of lines of input debugging; for big models, this output
// can get rather excessive.  Outside of SLiMgui, though, we emit it all, because the user might need it for some reason.
#ifdef SLIMGUI
#define ABBREVIATE_DEBUG_INPUT	1
#else
#define ABBREVIATE_DEBUG_INPUT	0
#endif

// If 1, checks of current memory usage versus maximum allowed memory usage will be done in certain spots
// where we are particularly likely to run out of memory, to provide the user with a better error message.
// Note that even when this is 1, the user can disable some of these checks with -x.
// Disable for Windows until Eidos_GetMaxRSS() issue fixed:
#ifdef _WIN32
#define DO_MEMORY_CHECKS	0
#else
#define DO_MEMORY_CHECKS	1
#endif

// If 1, and SLiM_verbosity_level >= 2, additional output will be generated regarding the mutation run count
// experiments performed by Species.
#define MUTRUN_EXPERIMENT_OUTPUT	0

// Verbosity, from the command-line option -l[ong]; defaults to 1 if -l[ong] is not used
extern int64_t SLiM_verbosity_level;


// *******************************************************************************************************************
//
//	Shared SLiM types and enumerations
//
#pragma mark -
#pragma mark Shared SLiM types and enumerations
#pragma mark -

// This enumeration represents the type of model: presently WF or nonWF
enum class SLiMModelType
{
	kModelTypeWF = 0,			// a Wright-Fisher model: the original model type supported by SLiM
	kModelTypeNonWF				// a non-Wright-Fisher model: a new model type that is more general
};

enum class SLiMCycleStage
{
	kStagePreCycle = 0,
	
	// stages for WF models
	kWFStage0ExecuteFirstScripts = 1,
	kWFStage1ExecuteEarlyScripts,
	kWFStage2GenerateOffspring,
	kWFStage3RemoveFixedMutations,
	kWFStage4SwapGenerations,
	kWFStage5ExecuteLateScripts,
	kWFStage6CalculateFitness,
	kWFStage7AdvanceTickCounter,
	
	// stages for nonWF models
	kNonWFStage0ExecuteFirstScripts = 101,
	kNonWFStage1GenerateOffspring,
	kNonWFStage2ExecuteEarlyScripts,
	kNonWFStage3CalculateFitness,
	kNonWFStage4SurvivalSelection,
	kNonWFStage5RemoveFixedMutations,
	kNonWFStage6ExecuteLateScripts,
	kNonWFStage7AdvanceTickCounter,
	
	// end stage between ticks; things in the Eidos console happen here
	kStagePostCycle = 201,
};

std::string StringForSLiMCycleStage(SLiMCycleStage p_stage);

// This enumeration represents the type of chromosome represented by a genome: autosome, X, or Y.  Note that this is somewhat
// separate from the sex of the individual; one can model sexual individuals but model only an autosome, in which case the sex
// of the individual cannot be determined from its modeled genome.
enum class GenomeType : uint8_t {
	kAutosome = 0,
	kXChromosome,
	kYChromosome
};

std::string StringForGenomeType(GenomeType p_genome_type);
std::ostream& operator<<(std::ostream& p_out, GenomeType p_genome_type);


// This enumeration represents the sex of an individual: hermaphrodite, female, or male.  It also includes an "unspecified"
// value that is useful in situations where the code wants to say that it doesn't care what sex is present.
enum class IndividualSex : int8_t
{
	kUnspecified = -2,
	kHermaphrodite = -1,
	kFemale = 0,
	kMale = 1
};

std::string StringForIndividualSex(IndividualSex p_sex);
std::ostream& operator<<(std::ostream& p_out, IndividualSex p_sex);


// This enumeration represents the policy followed for multiple mutations at the same position.
// Such "stacked" mutations can be allowed (the default), or the first or last mutation at the position can be kept.
enum class MutationStackPolicy : char {
	kStack = 0,
	kKeepFirst,
	kKeepLast,
};

extern EidosValue_String_SP gStaticEidosValue_StringA;
extern EidosValue_String_SP gStaticEidosValue_StringC;
extern EidosValue_String_SP gStaticEidosValue_StringG;
extern EidosValue_String_SP gStaticEidosValue_StringT;

extern const char gSLiM_Nucleotides[4];		// A, C, G, T


// This enumeration represents possible boundary conditions supported by SLiM.  Note that "absorbing"
// is not included here, because there is no SLiM API that supports absorbing boundaries.
enum class BoundaryCondition : char {
	kNone = 0,
	kStopping,
	kReflecting,
	kReprising,
	kPeriodic
};


// *******************************************************************************************************************
//
//	TSKIT/tree sequence tables related
//
#pragma mark -
#pragma mark Tree sequences
#pragma mark -
	
#define SLIM_TSK_INDIVIDUAL_ALIVE       ((tsk_flags_t)(1 << 16))
#define SLIM_TSK_INDIVIDUAL_REMEMBERED  ((tsk_flags_t)(1 << 17))
#define SLIM_TSK_INDIVIDUAL_RETAINED    ((tsk_flags_t)(1 << 18))

extern const std::string gSLiM_tsk_metadata_schema;
extern const std::string gSLiM_tsk_edge_metadata_schema;
extern const std::string gSLiM_tsk_site_metadata_schema;
extern const std::string gSLiM_tsk_mutation_metadata_schema;
extern const std::string gSLiM_tsk_node_metadata_schema;
extern const std::string gSLiM_tsk_individual_metadata_schema;
extern const std::string gSLiM_tsk_population_metadata_schema_PREJSON;		// before SLiM 3.7
extern const std::string gSLiM_tsk_population_metadata_schema;


// *******************************************************************************************************************
//
//	NucleotideArray
//
#pragma mark -
#pragma mark NucleotideArray
#pragma mark -
	
class NucleotideArray
{
	// This is a very quick-and-dirty class for storing a nucleotide sequence compactly.  BCH 14 Feb. 2019
	
private:
	// The length of the array, in nucleotides
	std::size_t length_;
	
	// Nucleotides are stored as 2-bit unsigned quantities in uint64_t, where A=0, C=1, G=2, T=3.
	// The least-significant bits of each uint64_t are filled first.  Each uint64_t holds 32 nucleotides.
	uint64_t *buffer_;
	
public:
	NucleotideArray(const NucleotideArray&) = delete;				// no copying
	NucleotideArray& operator=(const NucleotideArray&) = delete;	// no copying
	NucleotideArray(void) = delete;									// no null construction
	NucleotideArray(std::size_t p_length) : length_(p_length) {
		buffer_ = (uint64_t *)malloc(((length_ + 31) / 32) * sizeof(uint64_t));
		if (!buffer_)
			EIDOS_TERMINATION << "ERROR (NucleotideArray::NucleotideArray): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate();
	}
	~NucleotideArray(void) {
		if (buffer_) { free(buffer_); buffer_ = nullptr; }
	}
	
	// Constructors that take sequences.  These raise a C++ exception if the sequence data is invalid,
	// so that it can be caught and handled without involving the Eidos exception machinery.  These
	// should therefore generally be called from a try/catch block.
	NucleotideArray(std::size_t p_length, const int64_t *p_int_buffer);
	NucleotideArray(std::size_t p_length, const char *p_char_buffer);
	NucleotideArray(std::size_t p_length, const std::vector<std::string> &p_string_vector);
	
	std::size_t size() const { return length_; }
	
	inline int NucleotideAtIndex(std::size_t p_index) const {
		uint64_t chunk = buffer_[p_index / 32];
		return (int)((chunk >> ((p_index % 32) * 2)) & 0x03);
	}
	void SetNucleotideAtIndex(std::size_t p_index, uint64_t p_nuc);
	
	// Write nucleotides to a char buffer; the buffer must be allocated with sufficient length
	// Read nucleotides from a char buffer; the buffer is assumed to be of appropriate length
	void WriteNucleotidesToBuffer(char *buffer) const;
	void ReadNucleotidesFromBuffer(const char *buffer);
	
	// Write compressed nucleotides to an ostream as a binary block, with a leading 64-bit size in nucleotides
	// Read compressed nucleotides from a buffer as a binary block, with a leading size, advancing the pointer
	void WriteCompressedNucleotides(std::ostream &p_out) const;
	void ReadCompressedNucleotides(char **buffer, char *end);
	
	// Write nucleotides into an EidosValue, in any of the supported formats
	EidosValue_SP NucleotidesAsIntegerVector(int64_t start, int64_t end);
	EidosValue_SP NucleotidesAsCodonVector(int64_t start, int64_t end, bool p_force_vector);
	EidosValue_SP NucleotidesAsStringVector(int64_t start, int64_t end);
	EidosValue_SP NucleotidesAsStringSingleton(int64_t start, int64_t end);
	
	// Read/write nucleotides to a stream; here FASTA format is used on output (with no header
	// produced, 70 characters per line), and on input (no header allowed, newlines removed)
	friend std::ostream& operator<<(std::ostream& p_out, const NucleotideArray &p_nuc_array);
	friend std::istream& operator>>(std::istream& p_in, NucleotideArray &p_nuc_array);
	
	// Provides a static lookup table for going from char ('A'/'C'/'G'/'T') to int (0/1/2/3; 4 for errors)
	static uint8_t *NucleotideCharToIntLookup(void);
};

std::ostream& operator<<(std::ostream& p_out, const NucleotideArray &p_nuc_array);


// *******************************************************************************************************************
//
//	Global strings and IDs
//
#pragma mark -
#pragma mark Global strings and IDs
#pragma mark -

//
//	Additional global std::string objects.  See script_globals.h for details.
//

void SLiM_ConfigureContext(void);

extern const std::string &gStr_initializeAncestralNucleotides;
extern const std::string &gStr_initializeGenomicElement;
extern const std::string &gStr_initializeGenomicElementType;
extern const std::string &gStr_initializeMutationType;
extern const std::string &gStr_initializeMutationTypeNuc;
extern const std::string &gStr_initializeGeneConversion;
extern const std::string &gStr_initializeMutationRate;
extern const std::string &gStr_initializeHotspotMap;
extern const std::string &gStr_initializeRecombinationRate;
extern const std::string &gStr_initializeSex;
extern const std::string &gStr_initializeSLiMOptions;
extern const std::string &gStr_initializeSpecies;
extern const std::string &gStr_initializeTreeSeq;
extern const std::string &gStr_initializeSLiMModelType;
extern const std::string &gStr_initializeInteractionType;

extern const std::string &gStr_genomicElements;
extern const std::string &gStr_lastPosition;
extern const std::string &gStr_hotspotEndPositions;
extern const std::string &gStr_hotspotEndPositionsM;
extern const std::string &gStr_hotspotEndPositionsF;
extern const std::string &gStr_hotspotMultipliers;
extern const std::string &gStr_hotspotMultipliersM;
extern const std::string &gStr_hotspotMultipliersF;
extern const std::string &gStr_mutationEndPositions;
extern const std::string &gStr_mutationEndPositionsM;
extern const std::string &gStr_mutationEndPositionsF;
extern const std::string &gStr_mutationRates;
extern const std::string &gStr_mutationRatesM;
extern const std::string &gStr_mutationRatesF;
extern const std::string &gStr_overallMutationRate;
extern const std::string &gStr_overallMutationRateM;
extern const std::string &gStr_overallMutationRateF;
extern const std::string &gStr_overallRecombinationRate;
extern const std::string &gStr_overallRecombinationRateM;
extern const std::string &gStr_overallRecombinationRateF;
extern const std::string &gStr_recombinationEndPositions;
extern const std::string &gStr_recombinationEndPositionsM;
extern const std::string &gStr_recombinationEndPositionsF;
extern const std::string &gStr_recombinationRates;
extern const std::string &gStr_recombinationRatesM;
extern const std::string &gStr_recombinationRatesF;
extern const std::string &gStr_geneConversionEnabled;
extern const std::string &gStr_geneConversionGCBias;
extern const std::string &gStr_geneConversionNonCrossoverFraction;
extern const std::string &gStr_geneConversionMeanLength;
extern const std::string &gStr_geneConversionSimpleConversionFraction;
extern const std::string &gStr_genomeType;
extern const std::string &gStr_isNullGenome;
extern const std::string &gStr_mutations;
extern const std::string &gStr_uniqueMutations;
extern const std::string &gStr_genomicElementType;
extern const std::string &gStr_startPosition;
extern const std::string &gStr_endPosition;
extern const std::string &gStr_id;
extern const std::string &gStr_mutationTypes;
extern const std::string &gStr_mutationFractions;
extern const std::string &gStr_mutationMatrix;
extern const std::string &gStr_isFixed;
extern const std::string &gStr_isSegregating;
extern const std::string &gStr_mutationType;
extern const std::string &gStr_nucleotide;
extern const std::string &gStr_nucleotideValue;
extern const std::string &gStr_originTick;
extern const std::string &gStr_position;
extern const std::string &gStr_selectionCoeff;
extern const std::string &gStr_subpopID;
extern const std::string &gStr_convertToSubstitution;
extern const std::string &gStr_distributionType;
extern const std::string &gStr_distributionParams;
extern const std::string &gStr_dominanceCoeff;
extern const std::string &gStr_haploidDominanceCoeff;
extern const std::string &gStr_mutationStackGroup;
extern const std::string &gStr_mutationStackPolicy;
//extern const std::string &gStr_start;		now gEidosStr_start
//extern const std::string &gStr_end;		now gEidosStr_end
//extern const std::string &gStr_type;		now gEidosStr_type
//extern const std::string &gStr_source;		now gEidosStr_source
extern const std::string &gStr_active;
extern const std::string &gStr_allGenomicElementTypes;
extern const std::string &gStr_allInteractionTypes;
extern const std::string &gStr_allMutationTypes;
extern const std::string &gStr_allScriptBlocks;
extern const std::string &gStr_allSpecies;
extern const std::string &gStr_allSubpopulations;
extern const std::string &gStr_chromosome;
extern const std::string &gStr_chromosomeType;
extern const std::string &gStr_genomicElementTypes;
extern const std::string &gStr_lifetimeReproductiveOutput;
extern const std::string &gStr_lifetimeReproductiveOutputM;
extern const std::string &gStr_lifetimeReproductiveOutputF;
extern const std::string &gStr_modelType;
extern const std::string &gStr_nucleotideBased;
extern const std::string &gStr_scriptBlocks;
extern const std::string &gStr_sexEnabled;
extern const std::string &gStr_subpopulations;
extern const std::string &gStr_substitutions;
extern const std::string &gStr_tick;
extern const std::string &gStr_cycle;
extern const std::string &gStr_cycleStage;
extern const std::string &gStr_colorSubstitution;
extern const std::string &gStr_verbosity;
extern const std::string &gStr_tag;
extern const std::string &gStr_tagF;
extern const std::string &gStr_tagL0;
extern const std::string &gStr_tagL1;
extern const std::string &gStr_tagL2;
extern const std::string &gStr_tagL3;
extern const std::string &gStr_tagL4;
extern const std::string &gStr_migrant;
extern const std::string &gStr_fitnessScaling;
extern const std::string &gStr_firstMaleIndex;
extern const std::string &gStr_genomes;
extern const std::string &gStr_genomesNonNull;
extern const std::string &gStr_sex;
extern const std::string &gStr_individuals;
extern const std::string &gStr_subpopulation;
extern const std::string &gStr_index;
extern const std::string &gStr_immigrantSubpopIDs;
extern const std::string &gStr_immigrantSubpopFractions;
extern const std::string &gStr_avatar;
extern const std::string &gStr_name;
extern const std::string &gStr_description;
extern const std::string &gStr_selfingRate;
extern const std::string &gStr_cloningRate;
extern const std::string &gStr_sexRatio;
extern const std::string &gStr_gridDimensions;
extern const std::string &gStr_interpolate;
extern const std::string &gStr_spatialBounds;
extern const std::string &gStr_spatialMaps;
extern const std::string &gStr_individualCount;
extern const std::string &gStr_fixationTick;
extern const std::string &gStr_age;
extern const std::string &gStr_meanParentAge;
extern const std::string &gStr_pedigreeID;
extern const std::string &gStr_pedigreeParentIDs;
extern const std::string &gStr_pedigreeGrandparentIDs;
extern const std::string &gStr_reproductiveOutput;
extern const std::string &gStr_genomePedigreeID;
extern const std::string &gStr_reciprocal;
extern const std::string &gStr_sexSegregation;
extern const std::string &gStr_dimensionality;
extern const std::string &gStr_periodicity;
extern const std::string &gStr_spatiality;
extern const std::string &gStr_spatialPosition;
extern const std::string &gStr_maxDistance;

extern const std::string &gStr_ancestralNucleotides;
extern const std::string &gStr_nucleotides;
extern const std::string &gStr_setAncestralNucleotides;
extern const std::string &gStr_setGeneConversion;
extern const std::string &gStr_setHotspotMap;
extern const std::string &gStr_setMutationRate;
extern const std::string &gStr_setRecombinationRate;
extern const std::string &gStr_drawBreakpoints;
extern const std::string &gStr_addMutations;
extern const std::string &gStr_addNewDrawnMutation;
extern const std::string &gStr_addNewMutation;
extern const std::string &gStr_containsMutations;
extern const std::string &gStr_countOfMutationsOfType;
extern const std::string &gStr_positionsOfMutationsOfType;
extern const std::string &gStr_containsMarkerMutation;
extern const std::string &gStr_relatedness;
extern const std::string &gStr_sharedParentCount;
extern const std::string &gStr_mutationsOfType;
extern const std::string &gStr_setSpatialPosition;
extern const std::string &gStr_sumOfMutationsOfType;
extern const std::string &gStr_uniqueMutationsOfType;
extern const std::string &gStr_readFromMS;
extern const std::string &gStr_readFromVCF;
extern const std::string &gStr_removeMutations;
extern const std::string &gStr_setGenomicElementType;
extern const std::string &gStr_setMutationFractions;
extern const std::string &gStr_setMutationMatrix;
extern const std::string &gStr_setSelectionCoeff;
extern const std::string &gStr_setMutationType;
extern const std::string &gStr_drawSelectionCoefficient;
extern const std::string &gStr_setDistribution;
extern const std::string &gStr_addSubpop;
extern const std::string &gStr_addSubpopSplit;
extern const std::string &gStr_deregisterScriptBlock;
extern const std::string &gStr_genomicElementTypesWithIDs;
extern const std::string &gStr_interactionTypesWithIDs;
extern const std::string &gStr_mutationTypesWithIDs;
extern const std::string &gStr_scriptBlocksWithIDs;
extern const std::string &gStr_speciesWithIDs;
extern const std::string &gStr_subpopulationsWithIDs;
extern const std::string &gStr_individualsWithPedigreeIDs;
extern const std::string &gStr_killIndividuals;
extern const std::string &gStr_mutationCounts;
extern const std::string &gStr_mutationCountsInGenomes;
extern const std::string &gStr_mutationFrequencies;
extern const std::string &gStr_mutationFrequenciesInGenomes;
//extern const std::string &gStr_mutationsOfType;
//extern const std::string &gStr_countOfMutationsOfType;
extern const std::string &gStr_outputFixedMutations;
extern const std::string &gStr_outputFull;
extern const std::string &gStr_outputMutations;
extern const std::string &gStr_outputUsage;
extern const std::string &gStr_readFromPopulationFile;
extern const std::string &gStr_recalculateFitness;
extern const std::string &gStr_registerFirstEvent;
extern const std::string &gStr_registerEarlyEvent;
extern const std::string &gStr_registerLateEvent;
extern const std::string &gStr_registerFitnessEffectCallback;
extern const std::string &gStr_registerInteractionCallback;
extern const std::string &gStr_registerMateChoiceCallback;
extern const std::string &gStr_registerModifyChildCallback;
extern const std::string &gStr_registerRecombinationCallback;
extern const std::string &gStr_registerMutationCallback;
extern const std::string &gStr_registerMutationEffectCallback;
extern const std::string &gStr_registerSurvivalCallback;
extern const std::string &gStr_registerReproductionCallback;
extern const std::string &gStr_rescheduleScriptBlock;
extern const std::string &gStr_simulationFinished;
extern const std::string &gStr_skipTick;
extern const std::string &gStr_subsetMutations;
extern const std::string &gStr_treeSeqCoalesced;
extern const std::string &gStr_treeSeqSimplify;
extern const std::string &gStr_treeSeqRememberIndividuals;
extern const std::string &gStr_treeSeqOutput;
extern const std::string &gStr_setMigrationRates;
extern const std::string &gStr_pointDeviated;
extern const std::string &gStr_pointInBounds;
extern const std::string &gStr_pointReflected;
extern const std::string &gStr_pointStopped;
extern const std::string &gStr_pointPeriodic;
extern const std::string &gStr_pointUniform;
extern const std::string &gStr_setCloningRate;
extern const std::string &gStr_setSelfingRate;
extern const std::string &gStr_setSexRatio;
extern const std::string &gStr_setSpatialBounds;
extern const std::string &gStr_setSubpopulationSize;
extern const std::string &gStr_addCloned;
extern const std::string &gStr_addCrossed;
extern const std::string &gStr_addEmpty;
extern const std::string &gStr_addRecombinant;
extern const std::string &gStr_addSelfed;
extern const std::string &gStr_takeMigrants;
extern const std::string &gStr_removeSubpopulation;
extern const std::string &gStr_cachedFitness;
extern const std::string &gStr_sampleIndividuals;
extern const std::string &gStr_subsetIndividuals;
extern const std::string &gStr_defineSpatialMap;
extern const std::string &gStr_addSpatialMap;
extern const std::string &gStr_removeSpatialMap;
extern const std::string &gStr_spatialMapColor;
extern const std::string &gStr_spatialMapImage;
extern const std::string &gStr_spatialMapValue;
extern const std::string &gStr_add;
extern const std::string &gStr_blend;
extern const std::string &gStr_multiply;
extern const std::string &gStr_divide;
extern const std::string &gStr_subtract;
extern const std::string &gStr_power;
extern const std::string &gStr_exp;
extern const std::string &gStr_changeColors;
extern const std::string &gStr_changeValues;
extern const std::string &gStr_gridValues;
extern const std::string &gStr_mapColor;
extern const std::string &gStr_mapImage;
extern const std::string &gStr_mapValue;
extern const std::string &gStr_rescale;
extern const std::string &gStr_sampleImprovedNearbyPoint;
extern const std::string &gStr_sampleNearbyPoint;
extern const std::string &gStr_smooth;
extern const std::string &gStr_outputMSSample;
extern const std::string &gStr_outputVCFSample;
extern const std::string &gStr_outputSample;
extern const std::string &gStr_outputMS;
extern const std::string &gStr_outputVCF;
extern const std::string &gStr_output;
extern const std::string &gStr_evaluate;
extern const std::string &gStr_distance;
extern const std::string &gStr_localPopulationDensity;
extern const std::string &gStr_interactionDistance;
extern const std::string &gStr_clippedIntegral;
extern const std::string &gStr_distanceFromPoint;
extern const std::string &gStr_nearestNeighbors;
extern const std::string &gStr_neighborCount;
extern const std::string &gStr_neighborCountOfPoint;
extern const std::string &gStr_nearestInteractingNeighbors;
extern const std::string &gStr_interactingNeighborCount;
extern const std::string &gStr_nearestNeighborsOfPoint;
extern const std::string &gStr_setConstraints;
extern const std::string &gStr_setInteractionFunction;
extern const std::string &gStr_strength;
extern const std::string &gStr_testConstraints;
extern const std::string &gStr_totalOfNeighborStrengths;
extern const std::string &gStr_unevaluate;
extern const std::string &gStr_drawByStrength;

extern const std::string &gStr_community;
extern const std::string &gStr_sim;
extern const std::string &gStr_self;
extern const std::string &gStr_individual;
extern const std::string &gStr_element;
extern const std::string &gStr_genome;
extern const std::string &gStr_genome1;
extern const std::string &gStr_genome2;
extern const std::string &gStr_subpop;
extern const std::string &gStr_sourceSubpop;
//extern const std::string &gStr_weights;		now gEidosStr_weights
extern const std::string &gStr_child;
extern const std::string &gStr_parent;
extern const std::string &gStr_parent1;
extern const std::string &gStr_isCloning;
extern const std::string &gStr_isSelfing;
extern const std::string &gStr_parent2;
extern const std::string &gStr_mut;
extern const std::string &gStr_effect;
extern const std::string &gStr_homozygous;
extern const std::string &gStr_breakpoints;
extern const std::string &gStr_receiver;
extern const std::string &gStr_exerter;
extern const std::string &gStr_originalNuc;
extern const std::string &gStr_fitness;
extern const std::string &gStr_surviving;
extern const std::string &gStr_draw;

extern const std::string &gStr_slimgui;
extern const std::string &gStr_pid;
extern const std::string &gStr_openDocument;
extern const std::string &gStr_pauseExecution;
extern const std::string &gStr_configureDisplay;

extern const std::string &gStr_Chromosome;
//extern const std::string &gStr_Genome;			// in Eidos; see EidosValue_Object::EidosValue_Object()
extern const std::string &gStr_GenomicElement;
extern const std::string &gStr_GenomicElementType;
//extern const std::string &gStr_Mutation;		// in Eidos; see EidosValue_Object::EidosValue_Object()
extern const std::string &gStr_MutationType;
extern const std::string &gStr_SLiMEidosBlock;
extern const std::string &gStr_Community;
extern const std::string &gStr_SpatialMap;
extern const std::string &gStr_Species;
extern const std::string &gStr_Subpopulation;
//extern const std::string &gStr_Individual;		// in Eidos; see EidosValue_Object::EidosValue_Object()
extern const std::string &gStr_Substitution;
extern const std::string &gStr_InteractionType;
extern const std::string &gStr_SLiMgui;

extern const std::string &gStr_createLogFile;
extern const std::string &gStr_logFiles;
extern const std::string &gStr_LogFile;
extern const std::string &gStr_logInterval;
extern const std::string &gStr_precision;
extern const std::string &gStr_addCustomColumn;
extern const std::string &gStr_addCycle;
extern const std::string &gStr_addCycleStage;
extern const std::string &gStr_addMeanSDColumns;
extern const std::string &gStr_addPopulationSexRatio;
extern const std::string &gStr_addPopulationSize;
extern const std::string &gStr_addSubpopulationSexRatio;
extern const std::string &gStr_addSubpopulationSize;
extern const std::string &gStr_addSuppliedColumn;
extern const std::string &gStr_addTick;
extern const std::string &gStr_flush;
extern const std::string &gStr_logRow;
extern const std::string &gStr_setLogInterval;
extern const std::string &gStr_setFilePath;
extern const std::string &gStr_setSuppliedValue;
extern const std::string &gStr_willAutolog;
extern const std::string &gStr_context;

extern const std::string &gStr_A;
extern const std::string gStr_C;	// these nucleotide strings are not registered, no need
extern const std::string gStr_G;
extern const std::string gStr_T;
extern const std::string &gStr_X;
extern const std::string &gStr_Y;
extern const std::string &gStr_f;
extern const std::string &gStr_g;
extern const std::string &gStr_e;
//extern const std::string &gStr_n;		now gEidosStr_n
extern const std::string &gStr_w;
extern const std::string &gStr_l;
extern const std::string &gStr_p;
//extern const std::string &gStr_s;		now gEidosStr_s
extern const std::string &gStr_species;
extern const std::string &gStr_ticks;
extern const std::string &gStr_speciesSpec;
extern const std::string &gStr_ticksSpec;
extern const std::string &gStr_first;
extern const std::string &gStr_early;
extern const std::string &gStr_late;
extern const std::string &gStr_initialize;
extern const std::string &gStr_fitnessEffect;
extern const std::string &gStr_mutationEffect;
extern const std::string &gStr_interaction;
extern const std::string &gStr_mateChoice;
extern const std::string &gStr_modifyChild;
extern const std::string &gStr_mutation;
extern const std::string &gStr_survival;
extern const std::string &gStr_recombination;
extern const std::string &gStr_reproduction;


enum _SLiMGlobalStringID : int {
	gID_initializeAncestralNucleotides = gEidosID_LastEntry + 1,
	gID_initializeGenomicElement,
	gID_initializeGenomicElementType,
	gID_initializeMutationType,
	gID_initializeMutationTypeNuc,
	gID_initializeGeneConversion,
	gID_initializeMutationRate,
	gID_initializeHotspotMap,
	gID_initializeRecombinationRate,
	gID_initializeSex,
	gID_initializeSLiMOptions,
	gID_initializeSpecies,
	gID_initializeTreeSeq,
	gID_initializeSLiMModelType,
	gID_initializeInteractionType,
	
	gID_genomicElements,
	gID_lastPosition,
	gID_hotspotEndPositions,
	gID_hotspotEndPositionsM,
	gID_hotspotEndPositionsF,
	gID_hotspotMultipliers,
	gID_hotspotMultipliersM,
	gID_hotspotMultipliersF,
	gID_mutationEndPositions,
	gID_mutationEndPositionsM,
	gID_mutationEndPositionsF,
	gID_mutationRates,
	gID_mutationRatesM,
	gID_mutationRatesF,
	gID_overallMutationRate,
	gID_overallMutationRateM,
	gID_overallMutationRateF,
	gID_overallRecombinationRate,
	gID_overallRecombinationRateM,
	gID_overallRecombinationRateF,
	gID_recombinationEndPositions,
	gID_recombinationEndPositionsM,
	gID_recombinationEndPositionsF,
	gID_recombinationRates,
	gID_recombinationRatesM,
	gID_recombinationRatesF,
	gID_geneConversionEnabled,
	gID_geneConversionGCBias,
	gID_geneConversionNonCrossoverFraction,
	gID_geneConversionMeanLength,
	gID_geneConversionSimpleConversionFraction,
	gID_genomeType,
	gID_isNullGenome,
	gID_mutations,
	gID_uniqueMutations,
	gID_genomicElementType,
	gID_startPosition,
	gID_endPosition,
	gID_id,
	gID_mutationTypes,
	gID_mutationFractions,
	gID_mutationMatrix,
	gID_isFixed,
	gID_isSegregating,
	gID_mutationType,
	gID_nucleotide,
	gID_nucleotideValue,
	gID_originTick,
	gID_position,
	gID_selectionCoeff,
	gID_subpopID,
	gID_convertToSubstitution,
	gID_distributionType,
	gID_distributionParams,
	gID_dominanceCoeff,
	gID_haploidDominanceCoeff,
	gID_mutationStackGroup,
	gID_mutationStackPolicy,
	//gID_start,	now gEidosID_start
	//gID_end,		now gEidosID_end
	//gID_type,		now gEidosID_type
	//gID_source,	now gEidosID_source
	gID_active,
	gID_allGenomicElementTypes,
	gID_allInteractionTypes,
	gID_allMutationTypes,
	gID_allScriptBlocks,
	gID_allSpecies,
	gID_allSubpopulations,
	gID_chromosome,
	gID_chromosomeType,
	gID_genomicElementTypes,
	gID_lifetimeReproductiveOutput,
	gID_lifetimeReproductiveOutputM,
	gID_lifetimeReproductiveOutputF,
	gID_modelType,
	gID_nucleotideBased,
	gID_scriptBlocks,
	gID_sexEnabled,
	gID_subpopulations,
	gID_substitutions,
	gID_tick,
	gID_cycle,
	gID_cycleStage,
	gID_colorSubstitution,
	gID_verbosity,
	gID_tag,
	gID_tagF,
	gID_tagL0,
	gID_tagL1,
	gID_tagL2,
	gID_tagL3,
	gID_tagL4,
	gID_migrant,
	gID_fitnessScaling,
	gID_firstMaleIndex,
	gID_genomes,
	gID_genomesNonNull,
	gID_sex,
	gID_individuals,
	gID_subpopulation,
	gID_index,
	gID_immigrantSubpopIDs,
	gID_immigrantSubpopFractions,
	gID_avatar,
	gID_name,
	gID_description,
	gID_selfingRate,
	gID_cloningRate,
	gID_sexRatio,
	gID_gridDimensions,
	gID_interpolate,
	gID_spatialBounds,
	gID_spatialMaps,
	gID_individualCount,
	gID_fixationTick,
	gID_age,
	gID_meanParentAge,
	gID_pedigreeID,
	gID_pedigreeParentIDs,
	gID_pedigreeGrandparentIDs,
	gID_reproductiveOutput,
	gID_genomePedigreeID,
	gID_reciprocal,
	gID_sexSegregation,
	gID_dimensionality,
	gID_periodicity,
	gID_spatiality,
	gID_spatialPosition,
	gID_maxDistance,
	
	gID_ancestralNucleotides,
	gID_nucleotides,
	gID_setAncestralNucleotides,
	gID_setGeneConversion,
	gID_setHotspotMap,
	gID_setMutationRate,
	gID_setRecombinationRate,
	gID_drawBreakpoints,
	gID_addMutations,
	gID_addNewDrawnMutation,
	gID_addNewMutation,
	gID_containsMutations,
	gID_countOfMutationsOfType,
	gID_positionsOfMutationsOfType,
	gID_containsMarkerMutation,
	gID_relatedness,
	gID_sharedParentCount,
	gID_mutationsOfType,
	gID_setSpatialPosition,
	gID_sumOfMutationsOfType,
	gID_uniqueMutationsOfType,
	gID_readFromMS,
	gID_readFromVCF,
	gID_removeMutations,
	gID_setGenomicElementType,
	gID_setMutationFractions,
	gID_setMutationMatrix,
	gID_setSelectionCoeff,
	gID_setMutationType,
	gID_drawSelectionCoefficient,
	gID_setDistribution,
	gID_addSubpop,
	gID_addSubpopSplit,
	gID_deregisterScriptBlock,
	gID_genomicElementTypesWithIDs,
	gID_interactionTypesWithIDs,
	gID_mutationTypesWithIDs,
	gID_scriptBlocksWithIDs,
	gID_speciesWithIDs,
	gID_subpopulationsWithIDs,
	gID_individualsWithPedigreeIDs,
	gID_killIndividuals,
	gID_mutationCounts,
	gID_mutationCountsInGenomes,
	gID_mutationFrequencies,
	gID_mutationFrequenciesInGenomes,
	//gID_mutationsOfType,
	//gID_countOfMutationsOfType,
	gID_outputFixedMutations,
	gID_outputFull,
	gID_outputMutations,
	gID_outputUsage,
	gID_readFromPopulationFile,
	gID_recalculateFitness,
	gID_registerFirstEvent,
	gID_registerEarlyEvent,
	gID_registerLateEvent,
	gID_registerFitnessEffectCallback,
	gID_registerInteractionCallback,
	gID_registerMateChoiceCallback,
	gID_registerModifyChildCallback,
	gID_registerRecombinationCallback,
	gID_registerMutationCallback,
	gID_registerMutationEffectCallback,
	gID_registerSurvivalCallback,
	gID_registerReproductionCallback,
	gID_rescheduleScriptBlock,
	gID_simulationFinished,
	gID_skipTick,
	gID_subsetMutations,
	gID_treeSeqCoalesced,
	gID_treeSeqSimplify,
	gID_treeSeqRememberIndividuals,
	gID_treeSeqOutput,
	gID_setMigrationRates,
	gID_pointDeviated,
	gID_pointInBounds,
	gID_pointReflected,
	gID_pointStopped,
	gID_pointPeriodic,
	gID_pointUniform,
	gID_setCloningRate,
	gID_setSelfingRate,
	gID_setSexRatio,
	gID_setSpatialBounds,
	gID_setSubpopulationSize,
	gID_addCloned,
	gID_addCrossed,
	gID_addEmpty,
	gID_addRecombinant,
	gID_addSelfed,
	gID_takeMigrants,
	gID_removeSubpopulation,
	gID_cachedFitness,
	gID_sampleIndividuals,
	gID_subsetIndividuals,
	gID_defineSpatialMap,
	gID_addSpatialMap,
	gID_removeSpatialMap,
	gID_spatialMapColor,
	gID_spatialMapImage,
	gID_spatialMapValue,
	gID_add,
	gID_blend,
	gID_multiply,
	gID_subtract,
	gID_divide,
	gID_power,
	gID_exp,
	gID_changeColors,
	gID_changeValues,
	gID_gridValues,
	gID_mapColor,
	gID_mapImage,
	gID_mapValue,
	gID_rescale,
	gID_sampleImprovedNearbyPoint,
	gID_sampleNearbyPoint,
	gID_smooth,
	gID_outputMSSample,
	gID_outputVCFSample,
	gID_outputSample,
	gID_outputMS,
	gID_outputVCF,
	gID_output,
	gID_evaluate,
	gID_distance,
	gID_localPopulationDensity,
	gID_interactionDistance,
	gID_clippedIntegral,
	gID_distanceFromPoint,
	gID_nearestNeighbors,
	gID_neighborCount,
	gID_neighborCountOfPoint,
	gID_nearestInteractingNeighbors,
	gID_interactingNeighborCount,
	gID_nearestNeighborsOfPoint,
	gID_setConstraints,
	gID_setInteractionFunction,
	gID_strength,
	gID_testConstraints,
	gID_totalOfNeighborStrengths,
	gID_unevaluate,
	gID_drawByStrength,
	
	gID_community,
	gID_sim,
	gID_self,
	gID_individual,
	gID_element,
	gID_genome,
	gID_genome1,
	gID_genome2,
	gID_subpop,
	gID_sourceSubpop,
	//gID_weights,		now gEidosID_weights
	gID_child,
	gID_parent,
	gID_parent1,
	gID_isCloning,
	gID_isSelfing,
	gID_parent2,
	gID_mut,
	gID_effect,
	gID_homozygous,
	gID_breakpoints,
	gID_receiver,
	gID_exerter,
	gID_originalNuc,
	gID_fitness,
	gID_surviving,
	gID_draw,
	
	gID_slimgui,
	gID_pid,
	gID_openDocument,
	gID_pauseExecution,
	gID_configureDisplay,
	
	gID_Chromosome,
	gID_Genome,
	gID_GenomicElement,
	gID_GenomicElementType,
	//gID_Mutation,		// in Eidos; see EidosValue_Object::EidosValue_Object()
	gID_MutationType,
	gID_SLiMEidosBlock,
	gID_Community,
	gID_SpatialMap,
	gID_Species,
	gID_Subpopulation,
	gID_Individual,
	gID_Substitution,
	gID_InteractionType,
	gID_SLiMgui,
	
	gID_createLogFile,
	gID_logFiles,
	gID_LogFile,
	gID_logInterval,
	gID_precision,
	gID_addCustomColumn,
	gID_addCycle,
	gID_addCycleStage,
	gID_addMeanSDColumns,
	gID_addPopulationSexRatio,
	gID_addPopulationSize,
	gID_addSubpopulationSexRatio,
	gID_addSubpopulationSize,
	gID_addSuppliedColumn,
	gID_addTick,
	gID_flush,
	gID_logRow,
	gID_setLogInterval,
	gID_setFilePath,
	gID_setSuppliedValue,
	gID_willAutolog,
	gID_context,
	
	gID_A,
	gID_X,
	gID_Y,
	gID_f,
	gID_g,
	gID_e,
	// gID_n,		now gEidosID_n
	gID_w,
	gID_l,
	gID_p,
	//gID_s,	now gEidosID_s
	gID_species,
	gID_ticks,
	gID_speciesSpec,
	gID_ticksSpec,
	gID_first,
	gID_early,
	gID_late,
	gID_initialize,
	gID_fitnessEffect,
	gID_mutationEffect,
	gID_interaction,
	gID_mateChoice,
	gID_modifyChild,
	gID_recombination,
	gID_mutation,
	gID_survival,
	gID_reproduction,
	
	gID_LastSLiMEntry	// must come last
};

static_assert((int)gID_LastSLiMEntry <= (int)gEidosID_LastContextEntry, "the Context's last EidosGlobalStringID is greater than Eidos's upper limit");


// *******************************************************************************************************************
//
//	Profiling
//
#pragma mark -
#pragma mark Profiling
#pragma mark -

#if (SLIMPROFILING == 1)
void WriteProfileResults(std::string profile_output_path, std::string model_name, Community *community);
#endif


#endif /* defined(__SLiM__slim_globals__) */


















































