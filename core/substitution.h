//
//  substitution.h
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
 
 The class Substitution represents a mutation that has fixed in the population. Fixed mutations are converted to substitutions for
 efficiency, since such mutations no longer need to be tracked in each cycle.  This class is not a subclass of Mutation, to
 avoid any possibility of instances of this class getting confused with mutation instances in the code.  It also adds one new
 piece of information, the time to fixation.
 
 */

#ifndef __SLiM__substitution__
#define __SLiM__substitution__


#include "mutation.h"
#include "chromosome.h"
#include "eidos_value.h"

class Trait;


extern EidosClass *gSLiM_Substitution_Class;

// This structure contains all of the information about how a substitution influenced a particular trait: in particular, its
// effect size and dominance coefficient.  Each substitution keeps this information for each trait in its species, and since
// the number of traits is determined at runtime, the size of this data -- the number of SubstitutionTraitInfo records kept
// by each substitution -- is also determined at runtime.  This is parallel to the MutationTraitInfo struct for mutations,
// but keeps less information since it is not used during fitness evaluation.  Also unlike Mutation, which keeps all this
// in a block maintained by MutationBlock, we simply make a malloced block for each substitution; substitution is relatively
// rare and substitutions don't go away once created, so there is no need to overcomplicate this design.
// BCH 12/27/2025: Note that dominance_coeff_UNSAFE_ is marked "UNSAFE" because it can be NAN, representing independent
// dominance.  For this reason, it should not be used directly; instead, use RealizedDominanceForTrait().
typedef struct _SubstitutionTraitInfo
{
	slim_effect_t effect_size_;					// selection coefficient (s) or additive effect (a)
	slim_effect_t dominance_coeff_UNSAFE_;		// dominance coefficient (h), inherited from MutationType by default; CAN BE NAN
	slim_effect_t hemizygous_dominance_coeff_;	// hemizygous dominance coefficient (h_hemi), inherited from MutationType by default
} SubstitutionTraitInfo;

class Substitution : public EidosDictionaryRetained
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.

private:
	typedef EidosDictionaryRetained super;

public:
	
	MutationType *mutation_type_ptr_;			// mutation type identifier
	slim_position_t position_;					// position
	slim_objectid_t subpop_index_;				// subpopulation in which mutation arose
	slim_tick_t origin_tick_;					// tick in which mutation arose
	slim_tick_t fixation_tick_;					// tick in which mutation fixed
	slim_chromosome_index_t chromosome_index_;	// the (uint8_t) index of this mutation's chromosome
	
	unsigned int is_neutral_ : 1;				// all effects are 0.0; see mutation.h
	unsigned int is_independent_dominance_ : 1;	// configured for "independent dominance"; see mutation.h
	
	int8_t nucleotide_;							// the nucleotide being kept: A=0, C=1, G=2, T=3.  -1 is used to indicate non-nucleotide-based.
	const slim_mutationid_t mutation_id_;		// a unique id for each mutation, used to track mutations
	slim_usertag_t tag_value_;					// a user-defined tag value
	
	// Per-trait information
	SubstitutionTraitInfo *trait_info_;			// OWNED: a malloced block of per-trait information
	
	Substitution(const Substitution&) = delete;							// no copying
	Substitution& operator=(const Substitution&) = delete;				// no copying
	Substitution(void) = delete;										// no null construction
	Substitution(Mutation &p_mutation, slim_tick_t p_fixation_tick);	// construct from the mutation that has fixed, and the tick in which it fixed
	Substitution(slim_mutationid_t p_mutation_id, MutationType *p_mutation_type_ptr, slim_chromosome_index_t p_chromosome_index, slim_position_t p_position, slim_effect_t p_selection_coeff, slim_effect_t p_dominance_coeff, slim_objectid_t p_subpop_index, slim_tick_t p_tick, slim_tick_t p_fixation_tick, int8_t p_nucleotide);
	
	inline virtual ~Substitution(void) override { free(trait_info_); trait_info_ = nullptr; }
	
	// Check that our internal state all makes sense
	void SelfConsistencyCheck(const std::string &p_message_end);
	
	// This handles the possibility that a dominance coefficient is NAN, representing independent dominance, and returns the correct value
	slim_effect_t RealizedDominanceForTrait(Trait *p_trait);
	
	void PrintForSLiMOutput(std::ostream &p_out) const;
	void PrintForSLiMOutput_Tag(std::ostream &p_out) const;
	
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
	
	// Accelerated property access; see class EidosObject for comments on this mechanism
	static EidosValue *GetProperty_Accelerated_id(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_isIndependentDominance(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_isNeutral(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_nucleotide(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_nucleotideValue(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_originTick(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_fixationTick(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_position(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_subpopID(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_tag(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_mutationType(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size);
};

class Substitution_Class : public EidosDictionaryRetained_Class
{
private:
	typedef EidosDictionaryRetained_Class super;

public:
	Substitution_Class(const Substitution_Class &p_original) = delete;	// no copy-construct
	Substitution_Class& operator=(const Substitution_Class&) = delete;	// no copying
	inline Substitution_Class(const std::string &p_class_name, EidosClass *p_superclass) : super(p_class_name, p_superclass) { }
	
	virtual const std::vector<EidosPropertySignature_CSP> *Properties(void) const override;
	virtual const std::vector<EidosMethodSignature_CSP> *Methods(void) const override;
};


#endif /* defined(__SLiM__substitution__) */




































































