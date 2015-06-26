//
//  substitution.h
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
 
 The class Substitution represents a mutation that has fixed in the population. Fixed mutations are converted to substitutions for
 efficiency, since such mutations no longer need to be tracked in each generation.  This class is not a subclass of Mutation, to
 avoid any possibility of instances of this class getting confused with mutation instances in the code.  It also adds one new
 piece of information, the time to fixation.
 
 */

#ifndef __SLiM__substitution__
#define __SLiM__substitution__


#import "mutation.h"
#import "chromosome.h"
#include "script_value.h"


class Substitution : public ScriptObjectElement
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.

public:
	
	MutationType *mutation_type_ptr_;	// mutation type identifier
	int position_;								// position
	float selection_coeff_;						// selection coefficient
	int subpop_index_;							// subpopulation in which mutation arose
	int generation_;							// generation in which mutation arose  
	int fixation_time_;							// fixation time
#ifdef SLIMGUI
	const uint64_t mutation_id_;				// a unique id for each mutation, used to track mutations in SLiMgui
#endif
	
	Substitution(const Substitution&) = delete;							// no copying
	Substitution& operator=(const Substitution&) = delete;				// no copying
	Substitution(void) = delete;										// no null construction
	Substitution(Mutation &p_mutation, int p_fixation_time);		// construct from the mutation that has fixed, and the generation in which it fixed
	
	void print(std::ostream &p_out) const;
	
	//
	// SLiMscript support
	//
	virtual std::string ElementType(void) const;
	virtual void Print(std::ostream &p_ostream) const;
	
	virtual std::vector<std::string> ReadOnlyMembers(void) const;
	virtual std::vector<std::string> ReadWriteMembers(void) const;
	virtual ScriptValue *GetValueForMember(const std::string &p_member_name);
	virtual void SetValueForMember(const std::string &p_member_name, ScriptValue *p_value);
	
	virtual std::vector<std::string> Methods(void) const;
	virtual const FunctionSignature *SignatureForMethod(std::string const &p_method_name) const;
	virtual ScriptValue *ExecuteMethod(std::string const &p_method_name, ScriptValue *const *const p_arguments, int p_argument_count, ScriptInterpreter &p_interpreter);
};


#endif /* defined(__SLiM__substitution__) */




































































