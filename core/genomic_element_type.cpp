//
//  genomic_element_type.cpp
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


#include "genomic_element_type.h"

#include "slim_global.h"
#include "mutation_type.h"
#include "script_functionsignature.h"


GenomicElementType::GenomicElementType(int p_genomic_element_type_id, std::vector<MutationType*> p_mutation_type_ptrs, std::vector<double> p_mutation_fractions) :
	genomic_element_type_id_(p_genomic_element_type_id), mutation_type_ptrs_(p_mutation_type_ptrs), mutation_fractions_(p_mutation_fractions)
{
	if (mutation_type_ptrs_.size() != mutation_fractions_.size())
		SLIM_TERMINATION << "ERROR (Initialize): mutation types and fractions have different sizes" << slim_terminate();
	
	// Prepare to randomly draw mutation types
	double A[mutation_type_ptrs_.size()];
	
	for (int i = 0; i < mutation_type_ptrs_.size(); i++)
		A[i] = mutation_fractions_[i];
	
	if (lookup_mutation_type)
		gsl_ran_discrete_free(lookup_mutation_type);
	
	lookup_mutation_type = gsl_ran_discrete_preproc(p_mutation_fractions.size(), A);
}

GenomicElementType::~GenomicElementType(void)
{
	//SLIM_ERRSTREAM << "GenomicElementType::~GenomicElementType" << std::endl;
	
	if (lookup_mutation_type)
		gsl_ran_discrete_free(lookup_mutation_type);
	
	if (self_symbol_)
		delete self_symbol_;
}

MutationType *GenomicElementType::DrawMutationType() const
{
	return mutation_type_ptrs_[gsl_ran_discrete(g_rng, lookup_mutation_type)];
}

std::ostream &operator<<(std::ostream &p_outstream, const GenomicElementType &p_genomic_element_type)
{
	p_outstream << "GenomicElementType{mutation_types_ ";
	
	if (p_genomic_element_type.mutation_type_ptrs_.size() == 0)
	{
		p_outstream << "*";
	}
	else
	{
		p_outstream << "<";
		
		for (int i = 0; i < p_genomic_element_type.mutation_type_ptrs_.size(); ++i)
		{
			p_outstream << p_genomic_element_type.mutation_type_ptrs_[i]->mutation_type_id_;
			
			if (i < p_genomic_element_type.mutation_type_ptrs_.size() - 1)
				p_outstream << " ";
		}
		
		p_outstream << ">";
	}
	
	p_outstream << ", mutation_fractions_ ";
	
	if (p_genomic_element_type.mutation_fractions_.size() == 0)
	{
		p_outstream << "*";
	}
	else
	{
		p_outstream << "<";
		
		for (int i = 0; i < p_genomic_element_type.mutation_fractions_.size(); ++i)
		{
			p_outstream << p_genomic_element_type.mutation_fractions_[i];
			
			if (i < p_genomic_element_type.mutation_fractions_.size() - 1)
				p_outstream << " ";
		}
		
		p_outstream << ">";
	}

	p_outstream << "}";
	
	return p_outstream;
}

//
// SLiMscript support
//

void GenomicElementType::GenerateCachedSymbolTableEntry(void)
{
	std::ostringstream getype_stream;
	
	getype_stream << "g" << genomic_element_type_id_;
	
	self_symbol_ = new SymbolTableEntry(getype_stream.str(), (new ScriptValue_Object(this))->SetExternallyOwned(true)->SetInSymbolTable(true));
}

std::string GenomicElementType::ElementType(void) const
{
	return "GenomicElementType";
}

void GenomicElementType::Print(std::ostream &p_ostream) const
{
	p_ostream << ElementType() << "<g" << genomic_element_type_id_ << ">";
}

std::vector<std::string> GenomicElementType::ReadOnlyMembers(void) const
{
	std::vector<std::string> constants = ScriptObjectElement::ReadOnlyMembers();
	
	constants.push_back("id");						// genomic_element_type_id_
	constants.push_back("mutationTypes");			// mutation_type_ptrs_
	constants.push_back("mutationFractions");		// mutation_fractions_
	
	return constants;
}

std::vector<std::string> GenomicElementType::ReadWriteMembers(void) const
{
	std::vector<std::string> variables = ScriptObjectElement::ReadWriteMembers();
	
	return variables;
}

ScriptValue *GenomicElementType::GetValueForMember(const std::string &p_member_name)
{
	// constants
	if (p_member_name.compare("id") == 0)
		return new ScriptValue_Int(genomic_element_type_id_);
	if (p_member_name.compare("mutationTypes") == 0)
	{
		ScriptValue_Object *vec = new ScriptValue_Object();
		
		for (auto mut_type = mutation_type_ptrs_.begin(); mut_type != mutation_type_ptrs_.end(); ++mut_type)
			vec->PushElement(*mut_type);
		
		return vec;
	}
	if (p_member_name.compare("mutationFractions") == 0)
		return new ScriptValue_Float(mutation_fractions_);
	
	return ScriptObjectElement::GetValueForMember(p_member_name);
}

void GenomicElementType::SetValueForMember(const std::string &p_member_name, ScriptValue *p_value)
{
	return ScriptObjectElement::SetValueForMember(p_member_name, p_value);
}

std::vector<std::string> GenomicElementType::Methods(void) const
{
	std::vector<std::string> methods = ScriptObjectElement::Methods();
	
	methods.push_back("changeMutationFractions");
	
	return methods;
}

const FunctionSignature *GenomicElementType::SignatureForMethod(std::string const &p_method_name) const
{
	static FunctionSignature *changeMutationFractionsSig = nullptr;
	
	if (!changeMutationFractionsSig)
	{
		changeMutationFractionsSig = (new FunctionSignature("changeMutationFractions", FunctionIdentifier::kNoFunction, kScriptValueMaskNULL))->SetInstanceMethod()->AddObject()->AddNumeric();
	}
	
	if (p_method_name.compare("changeMutationFractions") == 0)
		return changeMutationFractionsSig;
	else
		return ScriptObjectElement::SignatureForMethod(p_method_name);
}

ScriptValue *GenomicElementType::ExecuteMethod(std::string const &p_method_name, std::vector<ScriptValue*> const &p_arguments, ScriptInterpreter &p_interpreter)
{
	int num_arguments = (int)p_arguments.size();
	ScriptValue *arg0_value = ((num_arguments >= 1) ? p_arguments[0] : nullptr);
	ScriptValue *arg1_value = ((num_arguments >= 2) ? p_arguments[1] : nullptr);
	
	//
	//	*********************	- (void)changeMutationFractions(object mutationTypes, numeric proportions)
	//
#pragma mark -changeMutationFractions()
	
	if (p_method_name.compare("changeMutationFractions") == 0)
	{
		int mut_type_id_count = arg0_value->Count();
		int proportion_count = arg1_value->Count();
		
		if ((mut_type_id_count != proportion_count) || (mut_type_id_count == 0))
			SLIM_TERMINATION << "ERROR (GenomicElementType::ExecuteMethod): changeMutationFractions() requires the sizes of mutationTypeIDs and proportions to be equal and nonzero." << slim_terminate();
		
		std::vector<MutationType*> mutation_types;
		std::vector<double> mutation_fractions;
		
		for (int mut_type_index = 0; mut_type_index < mut_type_id_count; ++mut_type_index)
		{ 
			MutationType *mutation_type_ptr = (MutationType *)arg0_value->ElementAtIndex(mut_type_index);
			double proportion = arg1_value->FloatAtIndex(mut_type_index);
			
			if (proportion <= 0)
				SLIM_TERMINATION << "ERROR (GenomicElementType::ExecuteMethod): changeMutationFractions() proportions must be greater than zero." << slim_terminate();
			
			mutation_types.push_back(mutation_type_ptr);
			mutation_fractions.push_back(proportion);
		}
		
		// Everything seems to be in order, so replace our mutation info with the new info
		mutation_type_ptrs_ = mutation_types;
		mutation_fractions_ = mutation_fractions;
		
		return ScriptValue_NULL::Static_ScriptValue_NULL_Invisible();
	}
	
	
	else
		return ScriptObjectElement::ExecuteMethod(p_method_name, p_arguments, p_interpreter);
}


































































