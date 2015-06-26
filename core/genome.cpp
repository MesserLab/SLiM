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
#include "script_functionsignature.h"
#include "slim_sim.h"	// we need to register mutations in the simulation...


#ifdef DEBUG
bool Genome::s_log_copy_and_assign_ = true;
#endif


// default constructor; gives a non-null genome of type GenomeType::kAutosome
Genome::Genome(void)
{
}

// a constructor for parent/child genomes, particularly in the SEX ONLY case: species type and null/non-null
Genome::Genome(enum GenomeType p_genome_type_, bool p_is_null) : genome_type_(p_genome_type_)
{
	// null genomes are now signalled with a null mutations pointer, rather than a separate flag
	if (p_is_null)
	{
		mutations_ = nullptr;
		mutation_capacity_ = 0;
	}
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
	if (mutations_ == nullptr)
		NullGenomeAccessError();
#endif
	
	Mutation **genome_iter = begin_pointer();
	Mutation **genome_backfill_iter = begin_pointer();
	Mutation **genome_max = end_pointer();
	
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
	
	if (p_original.mutations_ == nullptr)
	{
		// p_original is a null genome, so make ourselves null too
		if (mutations_ != mutations_buffer_)
			free(mutations_);
		
		mutations_ = nullptr;
		mutation_capacity_ = 0;
		mutation_count_ = 0;
	}
	else
	{
		int source_mutation_count = p_original.mutation_count_;
		
		// first we need to ensure that we have sufficient capacity
		if (source_mutation_count > mutation_capacity_)
		{
			mutation_capacity_ = p_original.mutation_capacity_;		// just use the same capacity as the source
			
			// mutations_buffer_ is not malloced and cannot be realloced, so forget that we were using it
			if (mutations_ == mutations_buffer_)
				mutations_ = nullptr;
			
			mutations_ = (Mutation **)realloc(mutations_, mutation_capacity_ * sizeof(Mutation*));
		}
		
		// then copy all pointers from the source to ourselves
		memcpy(mutations_, p_original.mutations_, source_mutation_count * sizeof(Mutation*));
		mutation_count_ = source_mutation_count;
	}
	
	// and copy other state
	genome_type_ = p_original.genome_type_;
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
		if (p_original.mutations_ == nullptr)
		{
			// p_original is a null genome, so make ourselves null too
			if (mutations_ != mutations_buffer_)
				free(mutations_);
			
			mutations_ = nullptr;
			mutation_capacity_ = 0;
			mutation_count_ = 0;
		}
		else
		{
			int source_mutation_count = p_original.mutation_count_;
			
			// first we need to ensure that we have sufficient capacity
			if (source_mutation_count > mutation_capacity_)
			{
				mutation_capacity_ = p_original.mutation_capacity_;		// just use the same capacity as the source
				
				// mutations_buffer_ is not malloced and cannot be realloced, so forget that we were using it
				if (mutations_ == mutations_buffer_)
					mutations_ = nullptr;
				
				mutations_ = (Mutation **)realloc(mutations_, mutation_capacity_ * sizeof(Mutation*));
			}
			
			// then copy all pointers from the source to ourselves
			memcpy(mutations_, p_original.mutations_, source_mutation_count * sizeof(Mutation*));
			mutation_count_ = source_mutation_count;
		}
		
		// and copy other state
		genome_type_ = p_original.genome_type_;
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

Genome::~Genome(void)
{
	// mutations_buffer_ is not malloced and cannot be freed; free only if we have an external buffer
	if (mutations_ != mutations_buffer_)
		free(mutations_);
	
	if (self_value_)
		delete self_value_;
}


//
// SLiMscript support
//

void Genome::GenerateCachedScriptValue(void)
{
	self_value_ = (new ScriptValue_Object_singleton_const(this))->SetExternallyOwned();
}

std::string Genome::ElementType(void) const
{
	return gStr_Genome;
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
	
	if (mutations_ == nullptr)
		p_ostream << ":null>";
	else
		p_ostream << ":" << mutation_count_ << ">";
}

std::vector<std::string> Genome::ReadOnlyMembers(void) const
{
	std::vector<std::string> constants = ScriptObjectElement::ReadOnlyMembers();
	
	constants.push_back(gStr_genomeType);			// genome_type_
	constants.push_back(gStr_isNullGenome);			// (mutations_ == nullptr)
	constants.push_back(gStr_mutations);			// mutations_
	
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
	if (p_member_name.compare(gStr_genomeType) == 0)
	{
		switch (genome_type_)
		{
			case GenomeType::kAutosome:		return new ScriptValue_String(gStr_Autosome);
			case GenomeType::kXChromosome:	return new ScriptValue_String(gStr_X_chromosome);
			case GenomeType::kYChromosome:	return new ScriptValue_String(gStr_Y_chromosome);
		}
	}
	if (p_member_name.compare(gStr_isNullGenome) == 0)
		return ((mutations_ == nullptr) ? gStaticScriptValue_LogicalT : gStaticScriptValue_LogicalF);
	if (p_member_name.compare(gStr_mutations) == 0)
	{
		ScriptValue_Object_vector *vec = new ScriptValue_Object_vector();
		
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
	
	methods.push_back(gStr_addMutations);
	methods.push_back(gStr_addNewDrawnMutation);
	methods.push_back(gStr_addNewMutation);
	methods.push_back(gStr_removeMutations);
	
	return methods;
}

const FunctionSignature *Genome::SignatureForMethod(std::string const &p_method_name) const
{
	static FunctionSignature *addMutationsSig = nullptr;
	static FunctionSignature *addNewDrawnMutationSig = nullptr;
	static FunctionSignature *addNewMutationSig = nullptr;
	static FunctionSignature *removeMutationsSig = nullptr;
	
	if (!addMutationsSig)
	{
		addMutationsSig = (new FunctionSignature(gStr_addMutations, FunctionIdentifier::kNoFunction, kScriptValueMaskNULL))->SetInstanceMethod()->AddObject();
		addNewDrawnMutationSig = (new FunctionSignature(gStr_addNewDrawnMutation, FunctionIdentifier::kNoFunction, kScriptValueMaskObject))->SetInstanceMethod()->AddObject_S()->AddInt_S()->AddInt_S()->AddInt_S();
		addNewMutationSig = (new FunctionSignature(gStr_addNewMutation, FunctionIdentifier::kNoFunction, kScriptValueMaskObject))->SetInstanceMethod()->AddObject_S()->AddInt_S()->AddInt_S()->AddNumeric_S()->AddInt_S();
		removeMutationsSig = (new FunctionSignature(gStr_removeMutations, FunctionIdentifier::kNoFunction, kScriptValueMaskNULL))->SetInstanceMethod()->AddObject();
	}
	
	if (p_method_name.compare(gStr_addMutations) == 0)
		return addMutationsSig;
	else if (p_method_name.compare(gStr_addNewDrawnMutation) == 0)
		return addNewDrawnMutationSig;
	else if (p_method_name.compare(gStr_addNewMutation) == 0)
		return addNewMutationSig;
	else if (p_method_name.compare(gStr_removeMutations) == 0)
		return removeMutationsSig;
	else
		return ScriptObjectElement::SignatureForMethod(p_method_name);
}

ScriptValue *Genome::ExecuteMethod(std::string const &p_method_name, std::vector<ScriptValue*> const &p_arguments, ScriptInterpreter &p_interpreter)
{
	int num_arguments = (int)p_arguments.size();
	ScriptValue *arg0_value = ((num_arguments >= 1) ? p_arguments[0] : nullptr);
	ScriptValue *arg1_value = ((num_arguments >= 2) ? p_arguments[1] : nullptr);
	ScriptValue *arg2_value = ((num_arguments >= 3) ? p_arguments[2] : nullptr);
	ScriptValue *arg3_value = ((num_arguments >= 4) ? p_arguments[3] : nullptr);
	ScriptValue *arg4_value = ((num_arguments >= 5) ? p_arguments[4] : nullptr);
	

	//
	//	*********************	- (void)addMutations(object mutations)
	//
#pragma mark -addMutations()
	
	if (p_method_name.compare(gStr_addMutations) == 0)
	{
		int arg0_count = arg0_value->Count();
		
		if (arg0_count)
		{
			if (((ScriptValue_Object *)arg0_value)->ElementType().compare(gStr_Mutation) != 0)
				SLIM_TERMINATION << "ERROR (Genome::ExecuteMethod): addMutations() requires that mutations has object element type Mutation." << slim_terminate();
			
			for (int value_index = 0; value_index < arg0_count; ++value_index)
			{
				Mutation *new_mutation = (Mutation *)(arg0_value->ElementAtIndex(value_index));
				
				insert_sorted_mutation_if_unique(new_mutation);
				
				// I think this is not needed; how would the user ever get a Mutation that was not already in the registry?
				//if (!registry.contains_mutation(new_mutation))
				//	registry.push_back(new_mutation);
			}
		}
		
		return gStaticScriptValueNULLInvisible;
	}
	
	
	//
	//	*********************	- (object$)addNewDrawnMutation(object$ mutationType, integer$ originGeneration, integer$ position, integer$ originSubpopID)
	//
#pragma mark -addNewDrawnMutation()
	
	if (p_method_name.compare(gStr_addNewDrawnMutation) == 0)
	{
		ScriptObjectElement *mut_type_value = arg0_value->ElementAtIndex(0);
		int origin_generation = (int)arg1_value->IntAtIndex(0);
		int position = (int)arg2_value->IntAtIndex(0);
		int origin_subpop_id = (int)arg3_value->IntAtIndex(0);
		
		if (mut_type_value->ElementType().compare(gStr_MutationType) != 0)
			SLIM_TERMINATION << "ERROR (Genome::ExecuteMethod): addNewMutation() requires that mutationType has object element type MutationType." << slim_terminate();
		
		MutationType *mut_type = (MutationType *)mut_type_value;
		double selection_coeff = mut_type->DrawSelectionCoefficient();
		Mutation *mutation = new Mutation(mut_type, position, selection_coeff, origin_subpop_id, origin_generation);
		
		// FIXME hack hack hack what is the right way to get up to the population?  should Genome have an up pointer?
		// FIXME this is worse now, because sim might not have been put into the symbol table; this needs to be fixed!
		SymbolTable &symbols = p_interpreter.GetSymbolTable();
		ScriptValue *sim_value = symbols.GetValueForSymbol(gStr_sim);
		SLiMSim *sim = (SLiMSim *)(sim_value->ElementAtIndex(0));
		
		insert_sorted_mutation(mutation);
		sim->Population().mutation_registry_.push_back(mutation);
		
		return new ScriptValue_Object_singleton_const(mutation);
	}
	
	
	//
	//	*********************	- (object$)addNewMutation(object$ mutationType, integer$ originGeneration, integer$ position, numeric$ selectionCoeff, integer$ originSubpopID)
	//
#pragma mark -addNewMutation()
	
	if (p_method_name.compare(gStr_addNewMutation) == 0)
	{
		ScriptObjectElement *mut_type_value = arg0_value->ElementAtIndex(0);
		int origin_generation = (int)arg1_value->IntAtIndex(0);
		int position = (int)arg2_value->IntAtIndex(0);
		double selection_coeff = arg3_value->FloatAtIndex(0);
		int origin_subpop_id = (int)arg4_value->IntAtIndex(0);
		
		if (mut_type_value->ElementType().compare(gStr_MutationType) != 0)
			SLIM_TERMINATION << "ERROR (Genome::ExecuteMethod): addNewMutation() requires that mutationType has object element type MutationType." << slim_terminate();
		
		MutationType *mut_type = (MutationType *)mut_type_value;
		Mutation *mutation = new Mutation(mut_type, position, selection_coeff, origin_subpop_id, origin_generation);
		
		// FIXME hack hack hack what is the right way to get up to the population?  should Genome have an up pointer?
		// FIXME this is worse now, because sim might not have been put into the symbol table; this needs to be fixed!
		SymbolTable &symbols = p_interpreter.GetSymbolTable();
		ScriptValue *sim_value = symbols.GetValueForSymbol(gStr_sim);
		SLiMSim *sim = (SLiMSim *)(sim_value->ElementAtIndex(0));
		
		insert_sorted_mutation(mutation);
		sim->Population().mutation_registry_.push_back(mutation);

		return new ScriptValue_Object_singleton_const(mutation);
	}
	
	
	//
	//	*********************	- (void)removeMutations(object mutations)
	//
#pragma mark -removeMutations()
	
	if (p_method_name.compare(gStr_removeMutations) == 0)
	{
		int arg0_count = arg0_value->Count();
		
		if (arg0_count)
		{
			if (((ScriptValue_Object *)arg0_value)->ElementType().compare(gStr_Mutation) != 0)
				SLIM_TERMINATION << "ERROR (Genome::ExecuteMethod): addMutations() requires that mutations has object element type Mutation." << slim_terminate();
			
			if (mutations_ == nullptr)
				NullGenomeAccessError();
			
			// Remove the specified mutations; see RemoveFixedMutations for the origins of this code
			Mutation **genome_iter = begin_pointer();
			Mutation **genome_backfill_iter = begin_pointer();
			Mutation **genome_max = end_pointer();
			
			// genome_iter advances through the mutation list; for each entry it hits, the entry is either removed (skip it) or not removed (copy it backward to the backfill pointer)
			while (genome_iter != genome_max)
			{
				Mutation *candidate_mutation = *genome_iter;
				bool should_remove = false;
				
				for (int value_index = 0; value_index < arg0_count; ++value_index)
					if (arg0_value->ElementAtIndex(value_index) == candidate_mutation)
					{
						should_remove = true;
						break;
					}
				
				if (should_remove)
				{
					// Removed mutation; we want to omit it, so we just advance our pointer
					++genome_iter;
				}
				else
				{
					// Unremoved mutation; we want to keep it, so we copy it backward and advance our backfill pointer as well as genome_iter
					if (genome_backfill_iter != genome_iter)
						*genome_backfill_iter = *genome_iter;
					
					++genome_backfill_iter;
					++genome_iter;
				}
			}
			
			// excess mutations at the end have been copied back already; we just adjust mutation_count_ and forget about them
			mutation_count_ -= (genome_iter - genome_backfill_iter);
		}
		
		return gStaticScriptValueNULLInvisible;
	}
	
	
	else
		return ScriptObjectElement::ExecuteMethod(p_method_name, p_arguments, p_interpreter);
}



































































