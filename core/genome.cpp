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
#include "eidos_property_signature.h"
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
	EIDOS_TERMINATION << "ERROR (Genome::NullGenomeAccessError): (internal error) a null genome was accessed." << eidos_terminate();
}

// Remove all mutations in p_genome that have a refcount of p_fixed_count, indicating that they have fixed
void Genome::RemoveFixedMutations(slim_refcount_t p_fixed_count)
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
		eidos_print_stacktrace(stderr);
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
		eidos_print_stacktrace(stderr);
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
//	Eidos support
//
#pragma mark -
#pragma mark Eidos support

void Genome::GenerateCachedEidosValue(void)
{
	// Note that this cache cannot be invalidated, because we are guaranteeing that this object will
	// live for at least as long as the symbol table it may be placed into!
	self_value_ = (new EidosValue_Object_singleton(this))->SetExternalPermanent();
}

const EidosObjectClass *Genome::Class(void) const
{
	return gSLiM_Genome_Class;
}

void Genome::Print(std::ostream &p_ostream) const
{
	p_ostream << Class()->ElementType() << "<";
	
	switch (genome_type_)
	{
		case GenomeType::kAutosome:		p_ostream << gStr_A; break;
		case GenomeType::kXChromosome:	p_ostream << gStr_X; break;
		case GenomeType::kYChromosome:	p_ostream << gStr_Y; break;
	}
	
	if (mutations_ == nullptr)
		p_ostream << ":null>";
	else
		p_ostream << ":" << mutation_count_ << ">";
}

EidosValue *Genome::GetProperty(EidosGlobalStringID p_property_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
			// constants
		case gID_genomeType:
		{
			switch (genome_type_)
			{
				case GenomeType::kAutosome:		return new EidosValue_String_singleton(gStr_A);
				case GenomeType::kXChromosome:	return new EidosValue_String_singleton(gStr_X);
				case GenomeType::kYChromosome:	return new EidosValue_String_singleton(gStr_Y);
			}
		}
		case gID_isNullGenome:
			return ((mutations_ == nullptr) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
		case gID_mutations:
		{
			EidosValue_Object_vector *vec = (new EidosValue_Object_vector())->Reserve(mutation_count_);
			
			for (int mut_index = 0; mut_index < mutation_count_; ++mut_index)
				vec->PushObjectElement(mutations_[mut_index]);
			
			return vec;
		}
			
			// variables
		case gID_tag:
			return new EidosValue_Int_singleton(tag_value_);
			
			// all others, including gID_none
		default:
			return EidosObjectElement::GetProperty(p_property_id);
	}
}

void Genome::SetProperty(EidosGlobalStringID p_property_id, EidosValue *p_value)
{
	switch (p_property_id)
	{
		case gID_tag:
		{
			slim_usertag_t value = SLiMCastToUsertagTypeOrRaise(p_value->IntAtIndex(0, nullptr));
			
			tag_value_ = value;
			return;
		}
			
		default:
		{
			return EidosObjectElement::SetProperty(p_property_id, p_value);
		}
	}
}

EidosValue *Genome::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, EidosValue *const *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
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
					Mutation *new_mutation = (Mutation *)(arg0_value->ObjectElementAtIndex(value_index, nullptr));
					
					insert_sorted_mutation_if_unique(new_mutation);
					
					// I think this is not needed; how would the user ever get a Mutation that was not already in the registry?
					//if (!registry.contains_mutation(new_mutation))
					//	registry.push_back(new_mutation);
				}
			}
			
			return gStaticEidosValueNULLInvisible;
		}
			
			
			//
			//	*********************	- (object<Mutation>)addNewDrawnMutation(io<MutationType>$ mutationType, Ni$ originGeneration, integer$ position, io<Subpopulation>$ originSubpop)
			//
#pragma mark -addNewDrawnMutation()
			
		case gID_addNewDrawnMutation:
		{
			SLiMSim *sim = dynamic_cast<SLiMSim *>(p_interpreter.GetEidosContext());
			
			if (!sim)
				EIDOS_TERMINATION << "ERROR (Genome::ExecuteInstanceMethod): (internal error) the sim is not registered as the context pointer." << eidos_terminate();
			
			MutationType *mutation_type_ptr = nullptr;
			
			if (arg0_value->Type() == EidosValueType::kValueInt)
			{
				slim_objectid_t mutation_type_id = SLiMCastToObjectidTypeOrRaise(arg0_value->IntAtIndex(0, nullptr));
				auto found_muttype_pair = sim->MutationTypes().find(mutation_type_id);
				
				if (found_muttype_pair != sim->MutationTypes().end())
					mutation_type_ptr = found_muttype_pair->second;
				
				if (!mutation_type_ptr)
					EIDOS_TERMINATION << "ERROR (Genome::ExecuteInstanceMethod): addNewDrawnMutation() mutation type m" << mutation_type_id << " not defined." << eidos_terminate();
			}
			else
			{
				mutation_type_ptr = dynamic_cast<MutationType *>(arg0_value->ObjectElementAtIndex(0, nullptr));
			}
			
			slim_generation_t origin_generation;
			
			if (arg1_value->Type() == EidosValueType::kValueNULL)
				origin_generation = sim->Generation();
			else
				origin_generation = SLiMCastToGenerationTypeOrRaise(arg1_value->IntAtIndex(0, nullptr));
			
			slim_position_t position = SLiMCastToPositionTypeOrRaise(arg2_value->IntAtIndex(0, nullptr));
			
			if (position > sim->Chromosome().last_position_)
				EIDOS_TERMINATION << "ERROR (Genome::ExecuteInstanceMethod): addNewDrawnMutation() position " << position << " is past the end of the chromosome." << eidos_terminate();
			
			slim_objectid_t origin_subpop_id;
			
			if (arg3_value->Type() == EidosValueType::kValueInt)
				origin_subpop_id = SLiMCastToObjectidTypeOrRaise(arg3_value->IntAtIndex(0, nullptr));
			else
				origin_subpop_id = dynamic_cast<Subpopulation *>(arg3_value->ObjectElementAtIndex(0, nullptr))->subpopulation_id_;
			
			double selection_coeff = mutation_type_ptr->DrawSelectionCoefficient();
			Mutation *mutation = new Mutation(mutation_type_ptr, position, selection_coeff, origin_subpop_id, origin_generation);
			
			insert_sorted_mutation(mutation);
			sim->Population().mutation_registry_.push_back(mutation);
			
			return new EidosValue_Object_singleton(mutation);
		}
			
			
			//
			//	*********************	- (object<Mutation>)addNewMutation(io<MutationType>$ mutationType, Ni$ originGeneration, integer$ position, numeric$ selectionCoeff, io<Subpopulation>$ originSubpop)
			//
#pragma mark -addNewMutation()
			
		case gID_addNewMutation:
		{
			SLiMSim *sim = dynamic_cast<SLiMSim *>(p_interpreter.GetEidosContext());
			
			if (!sim)
				EIDOS_TERMINATION << "ERROR (Genome::ExecuteInstanceMethod): (internal error) the sim is not registered as the context pointer." << eidos_terminate();
			
			MutationType *mutation_type_ptr = nullptr;
			
			if (arg0_value->Type() == EidosValueType::kValueInt)
			{
				slim_objectid_t mutation_type_id = SLiMCastToObjectidTypeOrRaise(arg0_value->IntAtIndex(0, nullptr));
				auto found_muttype_pair = sim->MutationTypes().find(mutation_type_id);
				
				if (found_muttype_pair != sim->MutationTypes().end())
					mutation_type_ptr = found_muttype_pair->second;
				
				if (!mutation_type_ptr)
					EIDOS_TERMINATION << "ERROR (Genome::ExecuteInstanceMethod): addNewMutation() mutation type m" << mutation_type_id << " not defined." << eidos_terminate();
			}
			else
			{
				mutation_type_ptr = dynamic_cast<MutationType *>(arg0_value->ObjectElementAtIndex(0, nullptr));
			}
			
			slim_generation_t origin_generation;
			
			if (arg1_value->Type() == EidosValueType::kValueNULL)
				origin_generation = sim->Generation();
			else
				origin_generation = SLiMCastToGenerationTypeOrRaise(arg1_value->IntAtIndex(0, nullptr));
			
			slim_position_t position = SLiMCastToPositionTypeOrRaise(arg2_value->IntAtIndex(0, nullptr));
			
			if (position > sim->Chromosome().last_position_)
				EIDOS_TERMINATION << "ERROR (Genome::ExecuteInstanceMethod): addNewMutation() position " << position << " is past the end of the chromosome." << eidos_terminate();
			
			double selection_coeff = arg3_value->FloatAtIndex(0, nullptr);
			slim_objectid_t origin_subpop_id;
			
			if (arg4_value->Type() == EidosValueType::kValueInt)
				origin_subpop_id = SLiMCastToObjectidTypeOrRaise(arg4_value->IntAtIndex(0, nullptr));
			else
				origin_subpop_id = dynamic_cast<Subpopulation *>(arg4_value->ObjectElementAtIndex(0, nullptr))->subpopulation_id_;
			
			Mutation *mutation = new Mutation(mutation_type_ptr, position, selection_coeff, origin_subpop_id, origin_generation);
			
			insert_sorted_mutation(mutation);
			sim->Population().mutation_registry_.push_back(mutation);
			
			return new EidosValue_Object_singleton(mutation);
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
						if (arg0_value->ObjectElementAtIndex(value_index, nullptr) == candidate_mutation)
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
			return EidosObjectElement::ExecuteInstanceMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
	}
}


//
//	Genome_Class
//
#pragma mark -
#pragma mark Genome_Class

class Genome_Class : public EidosObjectClass
{
public:
	Genome_Class(const Genome_Class &p_original) = delete;	// no copy-construct
	Genome_Class& operator=(const Genome_Class&) = delete;	// no copying
	
	Genome_Class(void);
	
	virtual const std::string &ElementType(void) const;
	
	virtual const std::vector<const EidosPropertySignature *> *Properties(void) const;
	virtual const EidosPropertySignature *SignatureForProperty(EidosGlobalStringID p_property_id) const;
	
	virtual const std::vector<const EidosMethodSignature *> *Methods(void) const;
	virtual const EidosMethodSignature *SignatureForMethod(EidosGlobalStringID p_method_id) const;
	virtual EidosValue *ExecuteClassMethod(EidosGlobalStringID p_method_id, EidosValue *const *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const;
};

EidosObjectClass *gSLiM_Genome_Class = new Genome_Class();


Genome_Class::Genome_Class(void)
{
}

const std::string &Genome_Class::ElementType(void) const
{
	return gStr_Genome;
}

const std::vector<const EidosPropertySignature *> *Genome_Class::Properties(void) const
{
	static std::vector<const EidosPropertySignature *> *properties = nullptr;
	
	if (!properties)
	{
		properties = new std::vector<const EidosPropertySignature *>(*EidosObjectClass::Properties());
		properties->push_back(SignatureForPropertyOrRaise(gID_genomeType));
		properties->push_back(SignatureForPropertyOrRaise(gID_isNullGenome));
		properties->push_back(SignatureForPropertyOrRaise(gID_mutations));
		properties->push_back(SignatureForPropertyOrRaise(gID_tag));
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const EidosPropertySignature *Genome_Class::SignatureForProperty(EidosGlobalStringID p_property_id) const
{
	// Signatures are all preallocated, for speed
	static EidosPropertySignature *genomeTypeSig = nullptr;
	static EidosPropertySignature *isNullGenomeSig = nullptr;
	static EidosPropertySignature *mutationsSig = nullptr;
	static EidosPropertySignature *tagSig = nullptr;
	
	if (!genomeTypeSig)
	{
		genomeTypeSig =		(EidosPropertySignature *)(new EidosPropertySignature(gStr_genomeType,		gID_genomeType,		true,	kEidosValueMaskString | kEidosValueMaskSingleton));
		isNullGenomeSig =	(EidosPropertySignature *)(new EidosPropertySignature(gStr_isNullGenome,	gID_isNullGenome,	true,	kEidosValueMaskLogical | kEidosValueMaskSingleton));
		mutationsSig =		(EidosPropertySignature *)(new EidosPropertySignature(gStr_mutations,		gID_mutations,		true,	kEidosValueMaskObject, gSLiM_Mutation_Class));
		tagSig =			(EidosPropertySignature *)(new EidosPropertySignature(gStr_tag,				gID_tag,			false,	kEidosValueMaskInt | kEidosValueMaskSingleton));
	}
	
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		case gID_genomeType:	return genomeTypeSig;
		case gID_isNullGenome:	return isNullGenomeSig;
		case gID_mutations:		return mutationsSig;
		case gID_tag:			return tagSig;
			
			// all others, including gID_none
		default:
			return EidosObjectClass::SignatureForProperty(p_property_id);
	}
}

const std::vector<const EidosMethodSignature *> *Genome_Class::Methods(void) const
{
	static std::vector<const EidosMethodSignature *> *methods = nullptr;
	
	if (!methods)
	{
		methods = new std::vector<const EidosMethodSignature *>(*EidosObjectClass::Methods());
		methods->push_back(SignatureForMethodOrRaise(gID_addMutations));
		methods->push_back(SignatureForMethodOrRaise(gID_addNewDrawnMutation));
		methods->push_back(SignatureForMethodOrRaise(gID_addNewMutation));
		methods->push_back(SignatureForMethodOrRaise(gID_removeMutations));
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}

const EidosMethodSignature *Genome_Class::SignatureForMethod(EidosGlobalStringID p_method_id) const
{
	static EidosInstanceMethodSignature *addMutationsSig = nullptr;
	static EidosInstanceMethodSignature *addNewDrawnMutationSig = nullptr;
	static EidosInstanceMethodSignature *addNewMutationSig = nullptr;
	static EidosInstanceMethodSignature *removeMutationsSig = nullptr;
	
	if (!addMutationsSig)
	{
		addMutationsSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_addMutations, kEidosValueMaskNULL))->AddObject("mutations", gSLiM_Mutation_Class);
		addNewDrawnMutationSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_addNewDrawnMutation, kEidosValueMaskObject, gSLiM_Mutation_Class))->AddIntObject_S("mutationType", gSLiM_MutationType_Class)->AddInt_SN("originGeneration")->AddInt_S("position")->AddIntObject_S("originSubpop", gSLiM_Subpopulation_Class);
		addNewMutationSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_addNewMutation, kEidosValueMaskObject, gSLiM_Mutation_Class))->AddIntObject_S("mutationType", gSLiM_MutationType_Class)->AddInt_SN("originGeneration")->AddInt_S("position")->AddNumeric_S("selectionCoeff")->AddIntObject_S("originSubpop", gSLiM_Subpopulation_Class);
		removeMutationsSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_removeMutations, kEidosValueMaskNULL))->AddObject("mutations", gSLiM_Mutation_Class);
	}
	
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_method_id)
	{
		case gID_addMutations:			return addMutationsSig;
		case gID_addNewDrawnMutation:	return addNewDrawnMutationSig;
		case gID_addNewMutation:		return addNewMutationSig;
		case gID_removeMutations:		return removeMutationsSig;
			
			// all others, including gID_none
		default:
			return EidosObjectClass::SignatureForMethod(p_method_id);
	}
}

EidosValue *Genome_Class::ExecuteClassMethod(EidosGlobalStringID p_method_id, EidosValue *const *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const
{
	return EidosObjectClass::ExecuteClassMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
}



































































