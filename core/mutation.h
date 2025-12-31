//
//  mutation.h
//  SLiM
//
//  Created by Ben Haller on 12/13/14.
//  Copyright (c) 2014-2025 Benjamin C. Haller.  All rights reserved.
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
 Mutations are also tagged with the subpopulation and tick in which they arose.
 
 */

#ifndef __SLiM__mutation__
#define __SLiM__mutation__


#include <iostream>

#include "slim_globals.h"
#include "eidos_value.h"

class MutationType;
class Trait;


class Mutation_Class;
extern Mutation_Class *gSLiM_Mutation_Class;


// A global counter used to assign all Mutation objects a unique ID
extern slim_mutationid_t gSLiM_next_mutation_id;

// A MutationIndex is an index into a MutationBlock (see mutation_block.h); it is used as, in effect, a Mutation *, but is 32-bit.
// Note that type int32_t is used instead of uint32_t so that -1 can be used as a "null pointer"; perhaps UINT32_MAX would be
// better, but on the other hand using int32_t has the virtue that if we run out of room we will probably crash hard rather
// than perhaps just silently overrunning gSLiM_Mutation_Block with mysterious memory corruption bugs that are hard to catch.
// For small simulations, defining this as int16_t instead can produce a substantial speedup (as much as 25%), but there are no
// safeguards in the code to check for running out of indices, so doing that is quite dangerous at present.  A way to make
// simulations switch from 16-bit to 32-bit at runtime, to get that speedup when possible, would be nice but in practice is very
// difficult to code since MutationRun's internal buffer of MutationIndex is accessible and used directly by many clients.
typedef int32_t MutationIndex;

// This structure contains all of the information about how a mutation influences a particular trait: in particular, its
// effect size and dominance coefficient.  Each mutation keeps this information for each trait in its species, and since
// the number of traits is determined at runtime, the size of this data -- the number of MutationTraitInfo records kept
// by each mutation -- is also determined at runtime.  We don't want to make a separate malloced block for each mutation;
// that would be far too expensive.  Instead, MutationBlock keeps a block of MutationTraitInfo records for the species,
// with a number of records per mutation that is determined when it is constructed.
// BCH 12/27/2025: Note that dominance_coeff_UNSAFE_ is marked "UNSAFE" because it can be NAN, representing independent
// dominance.  For this reason, it should not be used directly; instead, use RealizedDominanceForTrait().
typedef struct _MutationTraitInfo
{
	slim_effect_t effect_size_;					// selection coefficient (s) or additive effect (a)
	slim_effect_t dominance_coeff_UNSAFE_;		// dominance coefficient (h), inherited from MutationType by default; CAN BE NAN
	slim_effect_t hemizygous_dominance_coeff_;	// hemizygous dominance coefficient (h_hemi), inherited from MutationType by default
	
	// We cache values used in the fitness calculation code, for speed.  These are the final fitness effects of this mutation
	// when it is homozygous or heterozygous, respectively.  These values are clamped to a minimum of 0.0, so that multiplying
	// by them cannot cause the fitness of the individual to go below 0.0, avoiding slow tests in the core fitness loop.  These
	// values use slim_effect_t for speed; roundoff should not be a concern, since such differences would be inconsequential.
	slim_effect_t homozygous_effect_;		// a cached value for 1 + s, clamped to 0.0 minimum;  OR for 2a
	slim_effect_t heterozygous_effect_;		// a cached value for 1 + hs, clamped to 0.0 minimum; OR for 2ha
	slim_effect_t hemizygous_effect_;		// a cached value for 1 + hs, clamped to 0.0 minimum; OR for 2ha (h = h_hemi)
} MutationTraitInfo;

typedef enum {
	kNewMutation = 0,			// the state after new Mutation()
	kInRegistry,				// segregating in the simulation
	kRemovedWithSubstitution,	// removed by removeMutations(substitute=T); transitional from kInRegistry to kFixedAndSubstituted
	kFixedAndSubstituted,		// fixed and turned into a Substitution object
	kLostAndRemoved				// lost and removed from the simulation
} MutationState;

class Mutation : public EidosDictionaryRetained
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.

private:
	typedef EidosDictionaryRetained super;

public:
	
	MutationType *mutation_type_ptr_;					// mutation type identifier
	const slim_position_t position_;					// position on the chromosome
	slim_objectid_t subpop_index_;						// subpopulation in which mutation arose (or a user-defined tag value!)
	const slim_tick_t origin_tick_;						// tick in which the mutation arose
	slim_chromosome_index_t chromosome_index_;			// the (uint8_t) index of this mutation's chromosome
	int state_ : 4;										// see MutationState above; 4 bits so we can represent -1
	
	// is_neutral_ is true if all mutation effects are 0.0 (note this might be overridden by a callback).
	// The state of is_neutral_ is updated to reflect the current state of the mutation whenever it changes.
	// This is used to make constructing non-neutral caches for trait evaluation fast with multiple traits.
	unsigned int is_neutral_ : 1;
	// FIXME MULTITRAIT: it occurs to me that the present is_neutral_ flag on mutations is ambiguous.  One meaning
	// is "this mutation has neutral effects for all traits"; such mutations can be disregarded in all phenotype
	// calculations.  The other is "this mutation has neutral effects for all traits *that have a direct fitness
	// effect*"; such mutations can be disregarded for all calculations leading to a fitness value.  The former
	// is the meaning that needs to determine whether a given mutation is placed into a non-neutral cache, since
	// the non-neutral caches will be used for all phenotype calculations (I THINK?).  The latter is the meaning
	// that should be used to determine whether a given trait with a direct fitness effect is considered to be
	// neutral or not; if any mutation has a non-neutral effect on that given trait, then that trait needs to be
	// demanded and factored in to fitness calculations.
	
	// is_independent_dominance_ is true if the mutation has been configured to exhibit "independent dominance",
	// meaning that two heterozygous effects equal one homozygous effect, allowing the effects from haplosomes
	// to be calculated separately with no regard for zygosity; this is configured by using NAN as the default
	// dominance coefficient for MutationType.  It is updated if the state of the mutation's dominance changes,
	// but only based upon the special NAN dominance value in setDominanceForTrait(); setting dominance values
	// that happen to produce independent dominance does not cause this flag to be set, only the special NAN
	// value.  This is used to construct independent-dominance caches for fast trait evaluation.  Note that this
	// flag can be true when is_neutral_ is also true, recording that independent dominance was configured.
	unsigned int is_independent_dominance_ : 1;
	
	int8_t nucleotide_;									// the nucleotide being kept: A=0, C=1, G=2, T=3.  -1 is used to indicate non-nucleotide-based.
	int8_t scratch_;									// temporary scratch space for use by algorithms; regard as volatile outside your own code block
	const slim_mutationid_t mutation_id_;				// a unique id for each mutation, used to track mutations
	slim_usertag_t tag_value_;							// a user-defined tag value
	
#ifdef SLIMGUI
	mutable slim_refcount_t gui_reference_count_;			// a count of the number of occurrences of this mutation within the selected subpopulations in SLiMgui, valid at cycle end
	mutable slim_refcount_t gui_scratch_reference_count_;	// an additional refcount used for temporary tallies by SLiMgui, valid only when explicitly updated
#endif
	
#if DEBUG
	mutable slim_refcount_t refcount_CHECK_;					// scratch space for checking of parallel refcounting
#endif
	
	Mutation(const Mutation&) = delete;					// no copying
	Mutation& operator=(const Mutation&) = delete;		// no copying
	Mutation(void) = delete;							// no null construction; Mutation is an immutable class
	
	// This constructor is used when making a new mutation with effects DRAWN from each trait's DES, and dominance taken from each trait's default dominance coefficient, both from the given mutation type
	Mutation(MutationType *p_mutation_type_ptr, slim_chromosome_index_t p_chromosome_index, slim_position_t p_position, slim_objectid_t p_subpop_index, slim_tick_t p_tick, int8_t p_nucleotide);
	
	// This constructor is used when making a new mutation with effects and dominances PROVIDED by the caller
	// FIXME MULTITRAIT: needs to take a whole vector of each, per trait!
	Mutation(MutationType *p_mutation_type_ptr, slim_chromosome_index_t p_chromosome_index, slim_position_t p_position, slim_effect_t p_selection_coeff, slim_effect_t p_dominance_coeff, slim_objectid_t p_subpop_index, slim_tick_t p_tick, int8_t p_nucleotide);
	
	// This constructor is used when making a new mutation with effects and dominances PROVIDED by the caller, AND a mutation id provided by the caller
	// FIXME MULTITRAIT: needs to take a whole vector of each, per trait!
	Mutation(slim_mutationid_t p_mutation_id, MutationType *p_mutation_type_ptr, slim_chromosome_index_t p_chromosome_index, slim_position_t p_position, slim_effect_t p_selection_coeff, slim_effect_t p_dominance_coeff, slim_objectid_t p_subpop_index, slim_tick_t p_tick, int8_t p_nucleotide);
	
	// a destructor is needed now that we inherit from EidosDictionaryRetained; we want it to be as minimal as possible, though, and inline
#if DEBUG_MUTATIONS
	inline virtual ~Mutation(void) override
	{
		std::cout << "Mutation destructed: " << this << std::endl;
	}
#else
	inline virtual ~Mutation(void) override { }
#endif
	
	virtual void SelfDelete(void) override;
	
	// Check that our internal state all makes sense
	void SelfConsistencyCheck(const std::string &p_message_end);
	
	// This handles the possibility that a dominance coefficient is NAN, representing independent dominance, and returns the correct value
	slim_effect_t RealizedDominanceForTrait(Trait *p_trait);
	
	// These should be called whenever a mutation effect/dominance is changed; they handle the necessary recaching
	void SetEffect(Trait *p_trait, MutationTraitInfo *traitInfoRec, slim_effect_t p_new_effect);
	void SetDominance(Trait *p_trait, MutationTraitInfo *traitInfoRec, slim_effect_t p_new_dominance);
	void SetHemizygousDominance(Trait *p_trait, MutationTraitInfo *traitInfoRec, slim_effect_t p_new_dominance);
	
	//
	// Eidos support
	//
	virtual const EidosClass *Class(void) const override;
	virtual void Print(std::ostream &p_ostream) const override;
	
	virtual EidosValue_SP GetProperty(EidosGlobalStringID p_property_id) override;
	virtual void SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value) override;
	virtual EidosValue_SP ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) override;
	EidosValue_SP ExecuteMethod_effectForTrait(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_dominanceForTrait(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_hemizygousDominanceForTrait(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_setMutationType(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	
	// Accelerated property access; see class EidosObject for comments on this mechanism
	static EidosValue *GetProperty_Accelerated_id(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_isFixed(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_isIndependentDominance(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_isNeutral(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_isSegregating(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_nucleotide(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_nucleotideValue(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_originTick(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_position(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_subpopID(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_tag(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_mutationType(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size);
	
	// Accelerated property writing; see class EidosObject for comments on this mechanism
	static void SetProperty_Accelerated_subpopID(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size);
	static void SetProperty_Accelerated_tag(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size);
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

class Mutation_Class : public EidosDictionaryRetained_Class
{
private:
	typedef EidosDictionaryRetained_Class super;

public:
	Mutation_Class(const Mutation_Class &p_original) = delete;	// no copy-construct
	Mutation_Class& operator=(const Mutation_Class&) = delete;	// no copying
	inline Mutation_Class(const std::string &p_class_name, EidosClass *p_superclass) : super(p_class_name, p_superclass) { }
	
	virtual const std::vector<EidosPropertySignature_CSP> *Properties(void) const override;
	virtual const std::vector<EidosMethodSignature_CSP> *Methods(void) const override;
	
	virtual EidosValue_SP ExecuteClassMethod(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) const override;
	EidosValue_SP ExecuteMethod_setEffectForTrait(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) const;
	EidosValue_SP ExecuteMethod_setDominanceForTrait(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) const;
	EidosValue_SP ExecuteMethod_setHemizygousDominanceForTrait(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) const;
};

#endif /* defined(__SLiM__mutation__) */





































































