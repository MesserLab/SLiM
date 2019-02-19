//
//  mutation.h
//  SLiM
//
//  Created by Ben Haller on 12/13/14.
//  Copyright (c) 2014-2019 Philipp Messer.  All rights reserved.
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
 
 The class Mutation represents a single mutation, defined by its type, its position on the chromosome, and its selection coefficient.
 Mutations are also tagged with the subpopulation and generation in which they arose.
 
 */

#ifndef __SLiM__mutation__
#define __SLiM__mutation__


#include <iostream>

#include "slim_global.h"
#include "eidos_value.h"
#include "slim_eidos_dictionary.h"

class MutationType;


extern EidosObjectClass *gSLiM_Mutation_Class;

// A global counter used to assign all Mutation objects a unique ID
extern slim_mutationid_t gSLiM_next_mutation_id;

// A MutationIndex is an index into gSLiM_Mutation_Block (see below); it is used as, in effect, a Mutation *, but is 32-bit.
// Note that type int32_t is used instead of uint32_t so that -1 can be used as a "null pointer"; perhaps UINT32_MAX would be
// better, but on the other hand using int32_t has the virtue that if we run out of room we will probably crash hard rather
// than perhaps just silently overrunning gSLiM_Mutation_Block with mysterious memory corruption bugs that are hard to catch.
// For small simulations, defining this as int16_t instead can produce a substantial speedup (as much as 25%), but there are no
// safeguards in the code to check for running out of indices, so doing that is quite dangerous at present.  A way to make
// simulations switch from 16-bit to 32-bit at runtime, to get that speedup when possible, would be nice but in practice is very
// difficult to code since MutationRun's internal buffer of MutationIndex is accessible and used directly by many clients.
typedef int32_t MutationIndex;

// forward declaration of Mutation block allocation; see bottom of header
class Mutation;
extern Mutation *gSLiM_Mutation_Block;


class Mutation : public SLiMEidosDictionary
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.

public:
	
	MutationType *mutation_type_ptr_;					// mutation type identifier
	const slim_position_t position_;					// position on the chromosome
	slim_selcoeff_t selection_coeff_;					// selection coefficient – not const because it may be changed in script
	slim_objectid_t subpop_index_;						// subpopulation in which mutation arose (or a user-defined tag value!)
	const slim_generation_t origin_generation_;			// generation in which mutation arose
	int8_t nucleotide_;									// the nucleotide being kept: A=0, C=1, G=2, T=3.  -1 is used to indicate non-nucleotide-based.
	// NOTE THERE ARE 3 BYTES FREE IN THE CLASS LAYOUT HERE; see Mutation::Mutation() and Mutation layout.graffle
	const slim_mutationid_t mutation_id_;				// a unique id for each mutation, used to track mutations
	slim_usertag_t tag_value_;							// a user-defined tag value
	
#ifdef SLIMGUI
	mutable slim_refcount_t gui_reference_count_;			// a count of the number of occurrences of this mutation within the selected subpopulations in SLiMgui, valid at generation end
	mutable slim_refcount_t gui_scratch_reference_count_;	// an additional refcount used for temporary tallies by SLiMgui, valid only when explicitly updated
#endif
	
	// We cache values used in the fitness calculation code, for speed.  These are the final fitness effects of this mutation
	// when it is homozygous or heterozygous, respectively.  These values are clamped to a minimum of 0.0, so that multiplying
	// by them cannot cause the fitness of the individual to go below 0.0, avoiding slow tests in the core fitness loop.  These
	// values use slim_selcoeff_t for speed; roundoff should not be a concern, since such differences would be inconsequential.
	slim_selcoeff_t cached_one_plus_sel_;				// a cached value for (1 + selection_coeff_), clamped to 0.0 minimum
	slim_selcoeff_t cached_one_plus_dom_sel_;			// a cached value for (1 + dominance_coeff * selection_coeff_), clamped to 0.0 minimum
	
	Mutation(const Mutation&) = delete;					// no copying
	Mutation& operator=(const Mutation&) = delete;		// no copying
	Mutation(void) = delete;							// no null construction; Mutation is an immutable class
	Mutation(MutationType *p_mutation_type_ptr, slim_position_t p_position, double p_selection_coeff, slim_objectid_t p_subpop_index, slim_generation_t p_generation, int8_t p_nucleotide);
	Mutation(slim_mutationid_t p_mutation_id, MutationType *p_mutation_type_ptr, slim_position_t p_position, double p_selection_coeff, slim_objectid_t p_subpop_index, slim_generation_t p_generation, int8_t p_nucleotide);
	
	// a destructor is needed now that we inherit from SLiMEidosDictionary; we want it to be as minimal as possible, though, and inline
#if DEBUG_MUTATIONS
	inline virtual ~Mutation(void)
	{
		SLIM_OUTSTREAM << "Mutation destructed: " << this << std::endl;
	}
#else
	inline virtual ~Mutation(void) { }
#endif
	
	
	inline __attribute__((always_inline)) MutationIndex BlockIndex(void) const			{ return (MutationIndex)(this - gSLiM_Mutation_Block); }
	
	//
	// Eidos support
	//
	virtual const EidosObjectClass *Class(void) const;
	virtual void Print(std::ostream &p_ostream) const;
	
	virtual EidosValue_SP GetProperty(EidosGlobalStringID p_property_id);
	virtual void SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value);
	virtual EidosValue_SP ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_setSelectionCoeff(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_setMutationType(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	
	// Accelerated property access; see class EidosObjectElement for comments on this mechanism
	static EidosValue *GetProperty_Accelerated_id(EidosObjectElement **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_nucleotide(EidosObjectElement **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_nucleotideValue(EidosObjectElement **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_originGeneration(EidosObjectElement **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_position(EidosObjectElement **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_subpopID(EidosObjectElement **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_tag(EidosObjectElement **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_selectionCoeff(EidosObjectElement **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_mutationType(EidosObjectElement **p_values, size_t p_values_size);
	
	// Accelerated property writing; see class EidosObjectElement for comments on this mechanism
	static void SetProperty_Accelerated_subpopID(EidosObjectElement **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size);
	static void SetProperty_Accelerated_tag(EidosObjectElement **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size);
};

// true if M1 has an earlier (smaller) position than M2
inline __attribute__((always_inline)) bool operator<(const Mutation &p_mutation1, const Mutation &p_mutation2)
{
	return (p_mutation1.position_ < p_mutation2.position_);
}

// like operator< but with pointers; used for sort() among other things
// this is kept in terms of Mutation * so callers can cache gSLiM_Mutation_Block locally
inline __attribute__((always_inline)) bool CompareMutations(const Mutation *p_mutation1, const Mutation *p_mutation2)
{
	return (p_mutation1->position_ < p_mutation2->position_);
}

// support stream output of Mutation, for debugging
std::ostream &operator<<(std::ostream &p_outstream, const Mutation &p_mutation);


//
//		Mutation block allocation
//

// All Mutation objects get allocated out of a single shared pool, for speed.  We do not use EidosObjectPool for this
// any more, because we need the allocation to be out of a single contiguous block of memory that we realloc as needed,
// allowing Mutation objects to be referred to using 32-bit indexes into this contiguous block.  So we have a custom
// pool, declared here and implemented in mutation.cpp.  Note that this is a global, to make it easy for users of
// MutationIndex to look up mutations without needing to track down a pointer to the mutation block from the sim.  This
// means that in SLiMgui a single block will be used for all mutations in all simulations; that should be harmless.
class MutationRun;

extern Mutation *gSLiM_Mutation_Block;
extern MutationIndex gSLiM_Mutation_FreeIndex;
extern MutationIndex gSLiM_Mutation_Block_LastUsedIndex;

extern slim_refcount_t *gSLiM_Mutation_Refcounts;	// an auxiliary buffer, parallel to gSLiM_Mutation_Block, to increase memory cache efficiency
													// note that I tried keeping the fitness cache values and positions in separate buffers too, not a win
void SLiM_CreateMutationBlock(void);
void SLiM_IncreaseMutationBlockCapacity(void);
void SLiM_ZeroRefcountBlock(MutationRun &p_mutation_registry);
size_t SLiM_MemoryUsageForMutationBlock(void);
size_t SLiM_MemoryUsageForMutationRefcounts(void);

inline __attribute__((always_inline)) MutationIndex SLiM_NewMutationFromBlock(void)
{
	if (gSLiM_Mutation_FreeIndex == -1)
		SLiM_IncreaseMutationBlockCapacity();
	
	MutationIndex result = gSLiM_Mutation_FreeIndex;
	
	gSLiM_Mutation_FreeIndex = *(MutationIndex *)(gSLiM_Mutation_Block + result);
	
	if (gSLiM_Mutation_Block_LastUsedIndex < result)
		gSLiM_Mutation_Block_LastUsedIndex = result;
	
	return result;	// no need to zero out the memory, we are just an allocater, not a constructor
}

inline __attribute__((always_inline)) void SLiM_DisposeMutationToBlock(MutationIndex p_mutation_index)
{
	void *mut_ptr = gSLiM_Mutation_Block + p_mutation_index;
	
	*(MutationIndex *)mut_ptr = gSLiM_Mutation_FreeIndex;
	gSLiM_Mutation_FreeIndex = p_mutation_index;
}


#endif /* defined(__SLiM__mutation__) */





































































