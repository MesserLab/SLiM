//
//  genome.cpp
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


#include "genome.h"
#include "slim_global.h"


#ifdef DEBUG
bool Genome::s_log_copy_and_assign_ = true;
#endif


// default constructor; gives a non-null genome of type GenomeType::kAutosome
Genome::Genome(void)
{
}

// a constructor for parent/child genomes, particularly in the SEX ONLY case: species type and null/non-null
Genome::Genome(enum GenomeType p_genome_type_, bool p_is_null) : genome_type_(p_genome_type_), is_null_genome_(p_is_null)
{
}

// prints an error message, a stacktrace, and exits; called only for DEBUG
void Genome::NullGenomeAccessError(void) const
{
	SLIM_TERMINATION << "********* Genome::NullGenomeAccessError() called!" << std::endl << slim_terminate(true);
}

// Remove all mutations in p_genome that have a refcount of p_fixed_count, indicating that they have fixed
void Genome::RemoveFixedMutations(int p_fixed_count)
{
#ifdef DEBUG
	if (is_null_genome_)
		NullGenomeAccessError();
#endif
	
	SLIMCONST Mutation **genome_iter = begin_pointer();
	SLIMCONST Mutation **genome_backfill_iter = begin_pointer();
	SLIMCONST Mutation **genome_max = end_pointer();
	
	// genome_iter advances through the mutation list; for each entry it hits, the entry is either fixed (skip it) or not fixed (copy it backward to the backfill pointer)
	while (genome_iter != genome_max)
	{
		if ((*genome_iter)->reference_count_ == p_fixed_count)
		{
			// Fixed mutation; we want to omit it, so we just advance our pointer
			++genome_iter;
		}
		else
		{
			// Unfixed mutation; we want to keep it, so we copy it backward and advance our backfill pointer as well as genome_iter
			if (genome_backfill_iter != genome_iter)
				*genome_backfill_iter = *genome_iter;
			
			++genome_backfill_iter;
			++genome_iter;
		}
	}
	
	// excess mutations at the end have been copied back already; we just adjust mutation_count_ and forget about them
	mutation_count_ -= (genome_iter - genome_backfill_iter);
}


//
//	Methods to enforce limited copying
//

Genome::Genome(const Genome &p_original)
{
#ifdef DEBUG
	if (s_log_copy_and_assign_)
	{
		SLIM_ERRSTREAM << "********* Genome::Genome(Genome&) called!" << std::endl;
		print_stacktrace(stderr);
		SLIM_ERRSTREAM << "************************************************" << std::endl;
	}
#endif
	
	int source_mutation_count = p_original.mutation_count_;
	
	// first we need to ensure that we have sufficient capacity
	if (source_mutation_count > mutation_capacity_)
	{
		mutation_capacity_ = p_original.mutation_capacity_;		// just use the same capacity as the source
		mutations_ = (SLIMCONST Mutation **)realloc(mutations_, mutation_capacity_ * sizeof(Mutation*));
	}
	
	// then copy all pointers from the source to ourselves
	memcpy(mutations_, p_original.mutations_, source_mutation_count * sizeof(Mutation*));
	mutation_count_ = source_mutation_count;
	
	// and copy other state
	genome_type_ = p_original.genome_type_;
	is_null_genome_ = p_original.is_null_genome_;
}

Genome& Genome::operator= (const Genome& p_original)
{
#ifdef DEBUG
	if (s_log_copy_and_assign_)
	{
		SLIM_ERRSTREAM << "********* Genome::operator=(Genome&) called!" << std::endl;
		print_stacktrace(stderr);
		SLIM_ERRSTREAM << "************************************************" << std::endl;
	}
#endif
	
	if (this != &p_original)
	{
		int source_mutation_count = p_original.mutation_count_;
		
		// first we need to ensure that we have sufficient capacity
		if (source_mutation_count > mutation_capacity_)
		{
			mutation_capacity_ = p_original.mutation_capacity_;		// just use the same capacity as the source
			mutations_ = (SLIMCONST Mutation **)realloc(mutations_, mutation_capacity_ * sizeof(Mutation*));
		}
		
		// then copy all pointers from the source to ourselves
		memcpy(mutations_, p_original.mutations_, source_mutation_count * sizeof(Mutation*));
		mutation_count_ = source_mutation_count;
		
		// and copy other state
		genome_type_ = p_original.genome_type_;
		is_null_genome_ = p_original.is_null_genome_;
	}
	
	return *this;
}

#ifdef DEBUG
bool Genome::LogGenomeCopyAndAssign(bool p_log)
{
	bool old_value = s_log_copy_and_assign_;
	
	s_log_copy_and_assign_ = p_log;
	
	return old_value;
}
#endif


#ifndef SLIMCORE
//
// SLiMscript support
//
std::string Genome::ElementType(void) const
{
	return "Genome";
}

void Genome::Print(std::ostream &p_ostream) const
{
	p_ostream << ElementType() << "<";
	
	switch (genome_type_)
	{
		case GenomeType::kAutosome:		p_ostream << "A"; break;
		case GenomeType::kXChromosome:	p_ostream << "X"; break;
		case GenomeType::kYChromosome:	p_ostream << "Y"; break;
	}
	
	if (is_null_genome_)
		p_ostream << ":null>";
	else
		p_ostream << ":" << mutation_count_ << ">";
}

std::vector<std::string> Genome::ReadOnlyMembers(void) const
{
	std::vector<std::string> constants = ScriptObjectElement::ReadOnlyMembers();
	
	constants.push_back("genomeType");			// genome_type_
	constants.push_back("isNullGenome");		// is_null_genome_
	constants.push_back("mutations");			// mutations_
	
	return constants;
}

std::vector<std::string> Genome::ReadWriteMembers(void) const
{
	std::vector<std::string> variables = ScriptObjectElement::ReadWriteMembers();
	
	return variables;
}

ScriptValue *Genome::GetValueForMember(const std::string &p_member_name)
{
	// constants
	if (p_member_name.compare("genomeType") == 0)
	{
		switch (genome_type_)
		{
			case GenomeType::kAutosome:		return new ScriptValue_String("Autosome");
			case GenomeType::kXChromosome:	return new ScriptValue_String("X chromosome");
			case GenomeType::kYChromosome:	return new ScriptValue_String("Y chromosome");
		}
	}
	if (p_member_name.compare("isNullGenome") == 0)
		return new ScriptValue_Logical(is_null_genome_);
	if (p_member_name.compare("mutations") == 0)
	{
		ScriptValue_Object *vec = new ScriptValue_Object();
		
		if (!is_null_genome_)
			for (int mut_index = 0; mut_index < mutation_count_; ++mut_index)
				vec->PushElement(mutations_[mut_index]);
		
		return vec;
	}
	
	return ScriptObjectElement::GetValueForMember(p_member_name);
}

void Genome::SetValueForMember(const std::string &p_member_name, ScriptValue *p_value)
{
	return ScriptObjectElement::SetValueForMember(p_member_name, p_value);
}

std::vector<std::string> Genome::Methods(void) const
{
	std::vector<std::string> methods = ScriptObjectElement::Methods();
	
	return methods;
}

const FunctionSignature *Genome::SignatureForMethod(std::string const &p_method_name) const
{
	return ScriptObjectElement::SignatureForMethod(p_method_name);
}

ScriptValue *Genome::ExecuteMethod(std::string const &p_method_name, std::vector<ScriptValue*> const &p_arguments, std::ostream &p_output_stream, ScriptInterpreter &p_interpreter)
{
	return ScriptObjectElement::ExecuteMethod(p_method_name, p_arguments, p_output_stream, p_interpreter);
}

/*
	GenomeType genome_type_ = GenomeType::kAutosome;	// SEX ONLY: the type of chromosome represented by this genome
	bool is_null_genome_ = false;						// if true, this genome is a meaningless placeholder (often a Y chromosome)
	
	int mutation_count_ = 0;							// the number of entries presently in mutations_
	int mutation_capacity_ = 0;							// the capacity of mutations_
	SLIMCONST Mutation **mutations_ = nullptr;				// OWNED POINTER: a pointer to a malloced array of pointers to const Mutation objects
*/

#endif	// #ifndef SLIMCORE

































































