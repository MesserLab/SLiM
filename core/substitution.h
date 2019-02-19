//
//  substitution.h
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
 
 The class Substitution represents a mutation that has fixed in the population. Fixed mutations are converted to substitutions for
 efficiency, since such mutations no longer need to be tracked in each generation.  This class is not a subclass of Mutation, to
 avoid any possibility of instances of this class getting confused with mutation instances in the code.  It also adds one new
 piece of information, the time to fixation.
 
 */

#ifndef __SLiM__substitution__
#define __SLiM__substitution__


#include "mutation.h"
#include "chromosome.h"
#include "eidos_value.h"


extern EidosObjectClass *gSLiM_Substitution_Class;


class Substitution : public SLiMEidosDictionary
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.

public:
	
	MutationType *mutation_type_ptr_;			// mutation type identifier
	slim_position_t position_;					// position
	slim_selcoeff_t selection_coeff_;			// selection coefficient
	slim_objectid_t subpop_index_;				// subpopulation in which mutation arose
	slim_generation_t origin_generation_;		// generation in which mutation arose
	slim_generation_t fixation_generation_;		// generation in which mutation fixed
	int8_t nucleotide_;							// the nucleotide being kept: A=0, C=1, G=2, T=3.  -1 is used to indicate non-nucleotide-based.
	const slim_mutationid_t mutation_id_;		// a unique id for each mutation, used to track mutations
	slim_usertag_t tag_value_;					// a user-defined tag value
	
	Substitution(const Substitution&) = delete;							// no copying
	Substitution& operator=(const Substitution&) = delete;				// no copying
	Substitution(void) = delete;										// no null construction
	Substitution(Mutation &p_mutation, slim_generation_t p_fixation_generation);		// construct from the mutation that has fixed, and the generation in which it fixed
	Substitution(slim_mutationid_t p_mutation_id, MutationType *p_mutation_type_ptr, slim_position_t p_position, double p_selection_coeff, slim_objectid_t p_subpop_index, slim_generation_t p_generation, slim_generation_t p_fixation_generation, int8_t p_nucleotide);
	
	// a destructor is needed now that we inherit from SLiMEidosDictionary; we want it to be as minimal as possible, though, and inline
	inline virtual ~Substitution(void) { }
	
	void PrintForSLiMOutput(std::ostream &p_out) const;
	
	//
	// Eidos support
	//
	virtual const EidosObjectClass *Class(void) const;
	virtual void Print(std::ostream &p_ostream) const;
	
	virtual EidosValue_SP GetProperty(EidosGlobalStringID p_property_id);
	virtual void SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value);
	virtual EidosValue_SP ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	
	// Accelerated property access; see class EidosObjectElement for comments on this mechanism
	static EidosValue *GetProperty_Accelerated_id(EidosObjectElement **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_nucleotide(EidosObjectElement **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_nucleotideValue(EidosObjectElement **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_originGeneration(EidosObjectElement **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_fixationGeneration(EidosObjectElement **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_position(EidosObjectElement **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_subpopID(EidosObjectElement **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_tag(EidosObjectElement **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_selectionCoeff(EidosObjectElement **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_mutationType(EidosObjectElement **p_values, size_t p_values_size);
};


#endif /* defined(__SLiM__substitution__) */




































































