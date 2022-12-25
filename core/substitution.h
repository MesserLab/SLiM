//
//  substitution.h
//  SLiM
//
//  Created by Ben Haller on 12/13/14.
//  Copyright (c) 2014-2023 Philipp Messer.  All rights reserved.
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


extern EidosClass *gSLiM_Substitution_Class;


class Substitution : public EidosDictionaryRetained
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.

private:
	typedef EidosDictionaryRetained super;

public:
	
	MutationType *mutation_type_ptr_;			// mutation type identifier
	slim_position_t position_;					// position
	slim_selcoeff_t selection_coeff_;			// selection coefficient
	slim_objectid_t subpop_index_;				// subpopulation in which mutation arose
	slim_tick_t origin_tick_;					// tick in which mutation arose
	slim_tick_t fixation_tick_;					// tick in which mutation fixed
	int8_t nucleotide_;							// the nucleotide being kept: A=0, C=1, G=2, T=3.  -1 is used to indicate non-nucleotide-based.
	const slim_mutationid_t mutation_id_;		// a unique id for each mutation, used to track mutations
	slim_usertag_t tag_value_;					// a user-defined tag value
	
	Substitution(const Substitution&) = delete;							// no copying
	Substitution& operator=(const Substitution&) = delete;				// no copying
	Substitution(void) = delete;										// no null construction
	Substitution(Mutation &p_mutation, slim_tick_t p_fixation_tick);	// construct from the mutation that has fixed, and the tick in which it fixed
	Substitution(slim_mutationid_t p_mutation_id, MutationType *p_mutation_type_ptr, slim_position_t p_position, double p_selection_coeff, slim_objectid_t p_subpop_index, slim_tick_t p_tick, slim_tick_t p_fixation_tick, int8_t p_nucleotide);
	
	// a destructor is needed now that we inherit from EidosDictionaryRetained; we want it to be as minimal as possible, though, and inline
	inline virtual ~Substitution(void) override { }
	
	void PrintForSLiMOutput(std::ostream &p_out) const;
	
	//
	// Eidos support
	//
	virtual const EidosClass *Class(void) const override;
	virtual void Print(std::ostream &p_ostream) const override;
	
	virtual EidosValue_SP GetProperty(EidosGlobalStringID p_property_id) override;
	virtual void SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value) override;
	virtual EidosValue_SP ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) override;
	
	// Accelerated property access; see class EidosObject for comments on this mechanism
	static EidosValue *GetProperty_Accelerated_id(EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_nucleotide(EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_nucleotideValue(EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_originTick(EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_fixationTick(EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_position(EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_subpopID(EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_tag(EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_selectionCoeff(EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_mutationType(EidosObject **p_values, size_t p_values_size);
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




































































