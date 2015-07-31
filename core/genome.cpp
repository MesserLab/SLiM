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
#include "eidos_call_signature.h"
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
	EIDOS_TERMINATION << "********* Genome::NullGenomeAccessError() called!" << std::endl << eidos_terminate(true);
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
		EIDOS_ERRSTREAM << "********* Genome::Genome(Genome&) called!" << std::endl;
		eidos_print_stacktrace(stderr);
		EIDOS_ERRSTREAM << "************************************************" << std::endl;
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
		EIDOS_ERRSTREAM << "********* Genome::operator=(Genome&) called!" << std::endl;
		eidos_print_stacktrace(stderr);
		EIDOS_ERRSTREAM << "************************************************" << std::endl;
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
// Eidos support
//

void Genome::GenerateCachedEidosValue(void)
{
	// Note that this cache cannot be invalidated, because we are guaranteeing that this object will
	// live for at least as long as the symbol table it may be placed into!
	self_value_ = (new EidosValue_Object_singleton_const(this))->SetExternalPermanent();
}

const std::string *Genome::ElementType(void) const
{
	return &gStr_Genome;
}

void Genome::Print(std::ostream &p_ostream) const
{
	p_ostream << *ElementType() << "<";
	
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
	std::vector<std::string> constants = EidosObjectElement::ReadOnlyMembers();
	
	constants.push_back(gStr_genomeType);			// genome_type_
	constants.push_back(gStr_isNullGenome);			// (mutations_ == nullptr)
	constants.push_back(gStr_mutations);			// mutations_
	
	return constants;
}

std::vector<std::string> Genome::ReadWriteMembers(void) const
{
	std::vector<std::string> variables = EidosObjectElement::ReadWriteMembers();
	
	variables.push_back(gStr_tag);					// tag_value_
	
	return variables;
}

bool Genome::MemberIsReadOnly(EidosGlobalStringID p_member_id) const
{
	switch (p_member_id)
	{
			// constants
		case gID_genomeType:
		case gID_isNullGenome:
		case gID_mutations:
			return true;
			
			// variables
		case gID_tag:
			return false;
			
			// all others, including gID_none
		default:
			return EidosObjectElement::MemberIsReadOnly(p_member_id);
	}
}

EidosValue *Genome::GetValueForMember(EidosGlobalStringID p_member_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_member_id)
	{
			// constants
		case gID_genomeType:
		{
			switch (genome_type_)
			{
				case GenomeType::kAutosome:		return new EidosValue_String(gStr_Autosome);
				case GenomeType::kXChromosome:	return new EidosValue_String(gStr_X_chromosome);
				case GenomeType::kYChromosome:	return new EidosValue_String(gStr_Y_chromosome);
			}
		}
		case gID_isNullGenome:
			return ((mutations_ == nullptr) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
		case gID_mutations:
		{
			EidosValue_Object_vector *vec = new EidosValue_Object_vector();
			
			for (int mut_index = 0; mut_index < mutation_count_; ++mut_index)
				vec->PushElement(mutations_[mut_index]);
			
			return vec;
		}
			
			// variables
		case gID_tag:
			return new EidosValue_Int_singleton_const(tag_value_);
			
			// all others, including gID_none
		default:
			return EidosObjectElement::GetValueForMember(p_member_id);
	}
}

void Genome::SetValueForMember(EidosGlobalStringID p_member_id, EidosValue *p_value)
{
	switch (p_member_id)
	{
		case gID_tag:
		{
			TypeCheckValue(__func__, p_member_id, p_value, kValueMaskInt);
			
			int64_t value = p_value->IntAtIndex(0);
			
			tag_value_ = value;
			return;
		}
			
		default:
		{
			return EidosObjectElement::SetValueForMember(p_member_id, p_value);
		}
	}
}

std::vector<std::string> Genome::Methods(void) const
{
	std::vector<std::string> methods = EidosObjectElement::Methods();
	
	methods.push_back(gStr_addMutations);
	methods.push_back(gStr_addNewDrawnMutation);
	methods.push_back(gStr_addNewMutation);
	methods.push_back(gStr_removeMutations);
	
	return methods;
}

const EidosMethodSignature *Genome::SignatureForMethod(EidosGlobalStringID p_method_id) const
{
	static EidosInstanceMethodSignature *addMutationsSig = nullptr;
	static EidosInstanceMethodSignature *addNewDrawnMutationSig = nullptr;
	static EidosInstanceMethodSignature *addNewMutationSig = nullptr;
	static EidosInstanceMethodSignature *removeMutationsSig = nullptr;
	
	if (!addMutationsSig)
	{
		addMutationsSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_addMutations, kValueMaskNULL))->AddObject("mutations", &gStr_Mutation);
		addNewDrawnMutationSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_addNewDrawnMutation, kValueMaskObject, &gStr_Mutation))->AddObject_S("mutationType", &gStr_MutationType)->AddInt_S("originGeneration")->AddInt_S("position")->AddInt_S("originSubpopID");
		addNewMutationSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_addNewMutation, kValueMaskObject, &gStr_Mutation))->AddObject_S("mutationType", &gStr_MutationType)->AddInt_S("originGeneration")->AddInt_S("position")->AddNumeric_S("selectionCoeff")->AddInt_S("originSubpopID");
		removeMutationsSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_removeMutations, kValueMaskNULL))->AddObject("mutations", &gStr_Mutation);
	}
	
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_method_id)
	{
		case gID_addMutations:
			return addMutationsSig;
		case gID_addNewDrawnMutation:
			return addNewDrawnMutationSig;
		case gID_addNewMutation:
			return addNewMutationSig;
		case gID_removeMutations:
			return removeMutationsSig;
			
			// all others, including gID_none
		default:
			return EidosObjectElement::SignatureForMethod(p_method_id);
	}
}

EidosValue *Genome::ExecuteMethod(EidosGlobalStringID p_method_id, EidosValue *const *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
	EidosValue *arg0_value = ((p_argument_count >= 1) ? p_arguments[0] : nullptr);
	EidosValue *arg1_value = ((p_argument_count >= 2) ? p_arguments[1] : nullptr);
	EidosValue *arg2_value = ((p_argument_count >= 3) ? p_arguments[2] : nullptr);
	EidosValue *arg3_value = ((p_argument_count >= 4) ? p_arguments[3] : nullptr);
	EidosValue *arg4_value = ((p_argument_count >= 5) ? p_arguments[4] : nullptr);
	

	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_method_id)
	{
			//
			//	*********************	- (void)addMutations(object mutations)
			//
#pragma mark -addMutations()
			
		case gID_addMutations:
		{
			int arg0_count = arg0_value->Count();
			
			if (arg0_count)
			{
				for (int value_index = 0; value_index < arg0_count; ++value_index)
				{
					Mutation *new_mutation = (Mutation *)(arg0_value->ObjectElementAtIndex(value_index));
					
					insert_sorted_mutation_if_unique(new_mutation);
					
					// I think this is not needed; how would the user ever get a Mutation that was not already in the registry?
					//if (!registry.contains_mutation(new_mutation))
					//	registry.push_back(new_mutation);
				}
			}
			
			return gStaticEidosValueNULLInvisible;
		}
			
			
			//
			//	*********************	- (object$)addNewDrawnMutation(object$ mutationType, integer$ originGeneration, integer$ position, integer$ originSubpopID)
			//
#pragma mark -addNewDrawnMutation()
			
		case gID_addNewDrawnMutation:
		{
			EidosObjectElement *mut_type_value = arg0_value->ObjectElementAtIndex(0);
			int origin_generation = (int)arg1_value->IntAtIndex(0);
			int position = (int)arg2_value->IntAtIndex(0);
			int origin_subpop_id = (int)arg3_value->IntAtIndex(0);
			
			MutationType *mut_type = (MutationType *)mut_type_value;
			double selection_coeff = mut_type->DrawSelectionCoefficient();
			Mutation *mutation = new Mutation(mut_type, position, selection_coeff, origin_subpop_id, origin_generation);
			
			// FIXME hack hack hack what is the right way to get up to the population?  should Genome have an up pointer?
			// FIXME this is worse now, because sim might not have been put into the symbol table; this needs to be fixed!
			EidosSymbolTable &symbols = p_interpreter.GetSymbolTable();
			EidosValue *sim_value = symbols.GetValueForSymbol(gStr_sim);
			SLiMSim *sim = (SLiMSim *)(sim_value->ObjectElementAtIndex(0));
			
			insert_sorted_mutation(mutation);
			sim->Population().mutation_registry_.push_back(mutation);
			
			return new EidosValue_Object_singleton_const(mutation);
		}
			
			
			//
			//	*********************	- (object$)addNewMutation(object$ mutationType, integer$ originGeneration, integer$ position, numeric$ selectionCoeff, integer$ originSubpopID)
			//
#pragma mark -addNewMutation()
			
		case gID_addNewMutation:
		{
			EidosObjectElement *mut_type_value = arg0_value->ObjectElementAtIndex(0);
			int origin_generation = (int)arg1_value->IntAtIndex(0);
			int position = (int)arg2_value->IntAtIndex(0);
			double selection_coeff = arg3_value->FloatAtIndex(0);
			int origin_subpop_id = (int)arg4_value->IntAtIndex(0);
			
			MutationType *mut_type = (MutationType *)mut_type_value;
			Mutation *mutation = new Mutation(mut_type, position, selection_coeff, origin_subpop_id, origin_generation);
			
			// FIXME hack hack hack what is the right way to get up to the population?  should Genome have an up pointer?
			// FIXME this is worse now, because sim might not have been put into the symbol table; this needs to be fixed!
			EidosSymbolTable &symbols = p_interpreter.GetSymbolTable();
			EidosValue *sim_value = symbols.GetValueForSymbol(gStr_sim);
			SLiMSim *sim = (SLiMSim *)(sim_value->ObjectElementAtIndex(0));
			
			insert_sorted_mutation(mutation);
			sim->Population().mutation_registry_.push_back(mutation);
			
			return new EidosValue_Object_singleton_const(mutation);
		}
			
			
			//
			//	*********************	- (void)removeMutations(object mutations)
			//
#pragma mark -removeMutations()
			
		case gID_removeMutations:
		{
			int arg0_count = arg0_value->Count();
			
			if (arg0_count)
			{
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
						if (arg0_value->ObjectElementAtIndex(value_index) == candidate_mutation)
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
			
			return gStaticEidosValueNULLInvisible;
		}
			
			
			// all others, including gID_none
		default:
			return EidosObjectElement::ExecuteMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
	}
}



































































