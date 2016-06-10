//
//  individual.cpp
//  SLiM
//
//  Created by Ben Haller on 6/10/16.
//  Copyright (c) 2016 Philipp Messer.  All rights reserved.
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


#include "individual.h"
#include "subpopulation.h"
#include "slim_sim.h"
#include "eidos_property_signature.h"
#include "eidos_call_signature.h"


#ifdef DEBUG
bool Individual::s_log_copy_and_assign_ = true;
#endif


Individual::Individual(const Individual &p_original) : subpopulation_(p_original.subpopulation_), index_(p_original.index_), tag_value_(p_original.tag_value_)
{
#ifdef DEBUG
	if (s_log_copy_and_assign_)
	{
		SLIM_ERRSTREAM << "********* Individual::Individual(Individual&) called!" << std::endl;
		eidos_print_stacktrace(stderr);
		SLIM_ERRSTREAM << "************************************************" << std::endl;
	}
#endif
}

#ifdef DEBUG
bool Individual::LogIndividualCopyAndAssign(bool p_log)
{
	bool old_value = s_log_copy_and_assign_;
	
	s_log_copy_and_assign_ = p_log;
	
	return old_value;
}
#endif

Individual::Individual(Subpopulation &p_subpopulation, slim_popsize_t p_individual_index) : subpopulation_(p_subpopulation), index_(p_individual_index)
{
}

Individual::~Individual(void)
{
}


//
// Eidos support
//
#pragma mark -
#pragma mark Eidos support

void Individual::GenerateCachedEidosValue(void)
{
	// Note that this cache cannot be invalidated, because we are guaranteeing that this object will
	// live for at least as long as the symbol table it may be placed into!
	self_value_ = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(this, gSLiM_Individual_Class));
}

const EidosObjectClass *Individual::Class(void) const
{
	return gSLiM_Individual_Class;
}

void Individual::Print(std::ostream &p_ostream) const
{
	p_ostream << Class()->ElementType() << "<p" << subpopulation_.subpopulation_id_ << ":i" << index_ << ">";
}

EidosValue_SP Individual::GetProperty(EidosGlobalStringID p_property_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
			// constants
		case gID_subpopulation:
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(&subpopulation_, gSLiM_Subpopulation_Class));
		}
		case gID_index:
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(index_));
		}
		case gID_genomes:
		{
			std::vector<Genome> &genomes = (subpopulation_.child_generation_valid_ ? subpopulation_.child_genomes_ : subpopulation_.parent_genomes_);
			int genome_count = (int)genomes.size();
			slim_popsize_t genome_index = index_ * 2;
			
			if (genome_index + 1 < genome_count)
			{
				EidosValue_Object_vector *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Genome_Class))->Reserve(2);
				
				vec->PushObjectElement(&genomes[genome_index]);
				vec->PushObjectElement(&genomes[genome_index + 1]);
				
				return EidosValue_SP(vec);
			}
			else
			{
				return gStaticEidosValueNULL;
			}
		}
			
			// variables
		case gID_tag:
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(tag_value_));
			
			// all others, including gID_none
		default:
			return EidosObjectElement::GetProperty(p_property_id);
	}
}

void Individual::SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		case gID_tag:
		{
			slim_usertag_t value = SLiMCastToUsertagTypeOrRaise(p_value.IntAtIndex(0, nullptr));
			
			tag_value_ = value;
			return;
		}
			
			// all others, including gID_none
		default:
			return EidosObjectElement::SetProperty(p_property_id, p_value);
	}
}

EidosValue_SP Individual::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
	EidosValue *arg0_value = ((p_argument_count >= 1) ? p_arguments[0].get() : nullptr);
	
	
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_method_id)
	{
			//
			//	*********************	- (logical)containsMutations(object<Mutation> mutations)
			//
#pragma mark -containsMutations()
			
		case gID_containsMutations:
		{
			std::vector<Genome> &genomes = (subpopulation_.child_generation_valid_ ? subpopulation_.child_genomes_ : subpopulation_.parent_genomes_);
			int genome_count = (int)genomes.size();
			slim_popsize_t genome_index = index_ * 2;
			
			if (genome_index + 1 < genome_count)
			{
				Genome *genome1 = &genomes[genome_index];
				Genome *genome2 = &genomes[genome_index + 1];
				int arg0_count = arg0_value->Count();
				
				if (arg0_count == 1)
				{
					Mutation *mut = (Mutation *)(arg0_value->ObjectElementAtIndex(0, nullptr));
					
					if (genome1->contains_mutation(mut) || genome2->contains_mutation(mut))
						return gStaticEidosValue_LogicalT;
					else
						return gStaticEidosValue_LogicalF;
				}
				else
				{
					EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->Reserve(arg0_count);
					std::vector<eidos_logical_t> &logical_result_vec = *logical_result->LogicalVector_Mutable();
					
					for (int value_index = 0; value_index < arg0_count; ++value_index)
					{
						Mutation *mut = (Mutation *)(arg0_value->ObjectElementAtIndex(value_index, nullptr));
						bool contains_mut = (genome1->contains_mutation(mut) || genome2->contains_mutation(mut));
						
						logical_result_vec.emplace_back(contains_mut);
					}
					
					return EidosValue_SP(logical_result);
				}
			}
			
			return gStaticEidosValueNULL;
		}
			
			
			//
			//	*********************	- (integer$)countOfMutationsOfType(io<MutationType>$ mutType)
			//
#pragma mark -countOfMutationsOfType()
			
		case gID_countOfMutationsOfType:
		{
			std::vector<Genome> &genomes = (subpopulation_.child_generation_valid_ ? subpopulation_.child_genomes_ : subpopulation_.parent_genomes_);
			int genome_count = (int)genomes.size();
			slim_popsize_t genome_index = index_ * 2;
			
			if (genome_index + 1 < genome_count)
			{
				Genome *genome1 = &genomes[genome_index];
				Genome *genome2 = &genomes[genome_index + 1];
				MutationType *mutation_type_ptr = nullptr;
				
				if (arg0_value->Type() == EidosValueType::kValueInt)
				{
					SLiMSim *sim = dynamic_cast<SLiMSim *>(p_interpreter.Context());
					
					if (!sim)
						EIDOS_TERMINATION << "ERROR (Individual::ExecuteInstanceMethod): (internal error) the sim is not registered as the context pointer." << eidos_terminate();
					
					slim_objectid_t mutation_type_id = SLiMCastToObjectidTypeOrRaise(arg0_value->IntAtIndex(0, nullptr));
					auto found_muttype_pair = sim->MutationTypes().find(mutation_type_id);
					
					if (found_muttype_pair == sim->MutationTypes().end())
						EIDOS_TERMINATION << "ERROR (Individual::ExecuteInstanceMethod): countOfMutationsOfType() mutation type m" << mutation_type_id << " not defined." << eidos_terminate();
					
					mutation_type_ptr = found_muttype_pair->second;
				}
				else
				{
					mutation_type_ptr = (MutationType *)(arg0_value->ObjectElementAtIndex(0, nullptr));
				}
				
				// Count the number of mutations of the given type
				int match_count = 0;
				
				{
					int genome1_count = genome1->size();
					Mutation **genome1_ptr = genome1->begin_pointer();
					
					for (int mut_index = 0; mut_index < genome1_count; ++mut_index)
						if (genome1_ptr[mut_index]->mutation_type_ptr_ == mutation_type_ptr)
							++match_count;
				}
				{
					int genome2_count = genome2->size();
					Mutation **genome2_ptr = genome2->begin_pointer();
					
					for (int mut_index = 0; mut_index < genome2_count; ++mut_index)
						if (genome2_ptr[mut_index]->mutation_type_ptr_ == mutation_type_ptr)
							++match_count;
				}
				
				return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(match_count));
			}
			
			return gStaticEidosValueNULL;
		}
			
			
			// all others, including gID_none
		default:
			return EidosObjectElement::ExecuteInstanceMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
	}
}


//
//	Individual_Class
//
#pragma mark -
#pragma mark Individual_Class

class Individual_Class : public EidosObjectClass
{
public:
	Individual_Class(const Individual_Class &p_original) = delete;	// no copy-construct
	Individual_Class& operator=(const Individual_Class&) = delete;	// no copying
	
	Individual_Class(void);
	
	virtual const std::string &ElementType(void) const;
	
	virtual const std::vector<const EidosPropertySignature *> *Properties(void) const;
	virtual const EidosPropertySignature *SignatureForProperty(EidosGlobalStringID p_property_id) const;
	
	virtual const std::vector<const EidosMethodSignature *> *Methods(void) const;
	virtual const EidosMethodSignature *SignatureForMethod(EidosGlobalStringID p_method_id) const;
	virtual EidosValue_SP ExecuteClassMethod(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const;
};

EidosObjectClass *gSLiM_Individual_Class = new Individual_Class();


Individual_Class::Individual_Class(void)
{
}

const std::string &Individual_Class::ElementType(void) const
{
	return gStr_Individual;
}

const std::vector<const EidosPropertySignature *> *Individual_Class::Properties(void) const
{
	static std::vector<const EidosPropertySignature *> *properties = nullptr;
	
	if (!properties)
	{
		properties = new std::vector<const EidosPropertySignature *>(*EidosObjectClass::Properties());
		properties->emplace_back(SignatureForPropertyOrRaise(gID_subpopulation));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_index));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_genomes));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_tag));
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const EidosPropertySignature *Individual_Class::SignatureForProperty(EidosGlobalStringID p_property_id) const
{
	// Signatures are all preallocated, for speed
	static EidosPropertySignature *subpopulationSig = nullptr;
	static EidosPropertySignature *indexSig = nullptr;
	static EidosPropertySignature *genomesSig = nullptr;
	static EidosPropertySignature *tagSig = nullptr;
	
	if (!subpopulationSig)
	{
		subpopulationSig =	(EidosPropertySignature *)(new EidosPropertySignature(gStr_subpopulation,	gID_subpopulation,	true,	kEidosValueMaskObject, gSLiM_Subpopulation_Class));
		indexSig =			(EidosPropertySignature *)(new EidosPropertySignature(gStr_index,			gID_index,			true,	kEidosValueMaskInt | kEidosValueMaskSingleton));
		genomesSig =		(EidosPropertySignature *)(new EidosPropertySignature(gStr_genomes,			gID_genomes,		true,	kEidosValueMaskObject, gSLiM_Genome_Class));
		tagSig =			(EidosPropertySignature *)(new EidosPropertySignature(gStr_tag,				gID_tag,			false,	kEidosValueMaskInt | kEidosValueMaskSingleton));
	}
	
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		case gID_subpopulation:		return subpopulationSig;
		case gID_index:				return indexSig;
		case gID_genomes:			return genomesSig;
		case gID_tag:				return tagSig;
			
			// all others, including gID_none
		default:
			return EidosObjectClass::SignatureForProperty(p_property_id);
	}
}

const std::vector<const EidosMethodSignature *> *Individual_Class::Methods(void) const
{
	static std::vector<const EidosMethodSignature *> *methods = nullptr;
	
	if (!methods)
	{
		methods = new std::vector<const EidosMethodSignature *>(*EidosObjectClass::Methods());
		methods->emplace_back(SignatureForMethodOrRaise(gID_containsMutations));
		methods->emplace_back(SignatureForMethodOrRaise(gID_countOfMutationsOfType));
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}

const EidosMethodSignature *Individual_Class::SignatureForMethod(EidosGlobalStringID p_method_id) const
{
	static EidosInstanceMethodSignature *containsMutationsSig = nullptr;
	static EidosInstanceMethodSignature *countOfMutationsOfTypeSig = nullptr;
	
	if (!containsMutationsSig)
	{
		containsMutationsSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_containsMutations, kEidosValueMaskLogical))->AddObject("mutations", gSLiM_Mutation_Class);
		countOfMutationsOfTypeSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_countOfMutationsOfType, kEidosValueMaskInt | kEidosValueMaskSingleton))->AddIntObject_S("mutType", gSLiM_MutationType_Class);
	}
	
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_method_id)
	{
		case gID_containsMutations:			return containsMutationsSig;
		case gID_countOfMutationsOfType:	return countOfMutationsOfTypeSig;
			
			// all others, including gID_none
		default:
			return EidosObjectClass::SignatureForMethod(p_method_id);
	}
}

EidosValue_SP Individual_Class::ExecuteClassMethod(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const
{
	return EidosObjectClass::ExecuteClassMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
}












































