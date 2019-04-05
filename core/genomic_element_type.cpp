//
//  genomic_element_type.cpp
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


#include "genomic_element_type.h"
#include "slim_global.h"
#include "slim_sim.h"
#include "mutation_type.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"

#include <algorithm>
#include <string>


#pragma mark -
#pragma mark GenomicElementType
#pragma mark -

GenomicElementType::GenomicElementType(SLiMSim &p_sim, slim_objectid_t p_genomic_element_type_id, std::vector<MutationType*> p_mutation_type_ptrs, std::vector<double> p_mutation_fractions) :
	sim_(p_sim), genomic_element_type_id_(p_genomic_element_type_id), mutation_type_ptrs_(p_mutation_type_ptrs), mutation_fractions_(p_mutation_fractions), mutation_matrix_(nullptr),
	self_symbol_(Eidos_GlobalStringIDForString(SLiMEidosScript::IDStringWithPrefix('g', p_genomic_element_type_id)), EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(this, gSLiM_GenomicElementType_Class)))
{
	InitializeDraws();
}

GenomicElementType::~GenomicElementType(void)
{
	//EIDOS_ERRSTREAM << "GenomicElementType::~GenomicElementType" << std::endl;
	
	if (lookup_mutation_type_)
	{
		gsl_ran_discrete_free(lookup_mutation_type_);
		lookup_mutation_type_ = nullptr;
	}
	
	if (mm_thresholds)
	{
		free(mm_thresholds);
		mm_thresholds = nullptr;
	}
}

void GenomicElementType::InitializeDraws(void)
{
	size_t mutation_type_count = mutation_type_ptrs_.size();
	
	if (mutation_type_count != mutation_fractions_.size())
		EIDOS_TERMINATION << "ERROR (GenomicElementType::InitializeDraws): mutation types and fractions have different sizes." << EidosTerminate();
	
	if (lookup_mutation_type_)
	{
		gsl_ran_discrete_free(lookup_mutation_type_);
		lookup_mutation_type_ = nullptr;
	}
	
	// We allow an empty mutation type vector initially, because people might want to add mutation types in script.
	// However, if DrawMutationType() is called and our vector is still empty, that will be an error.
	if (mutation_type_count)
	{
		// Prepare to randomly draw mutation types
		std::vector<double> A(mutation_type_count);
		bool nonzero_seen = false;
		
		for (unsigned int i = 0; i < mutation_type_count; i++)
		{
			double fraction = mutation_fractions_[i];
			
			if (fraction > 0.0)
				nonzero_seen = true;
			
			A[i] = fraction;
		}
		
		// A mutation type vector with all zero proportions is treated the same as an empty vector: we allow it
		// on the assumption that it will be fixed later, but if it isn't, that will be an error.
		if (nonzero_seen)
			lookup_mutation_type_ = gsl_ran_discrete_preproc(mutation_type_count, A.data());
	}
}

MutationType *GenomicElementType::DrawMutationType(void) const
{
	if (!lookup_mutation_type_)
		EIDOS_TERMINATION << "ERROR (GenomicElementType::DrawMutationType): empty mutation type vector for genomic element type." << EidosTerminate();
	
	return mutation_type_ptrs_[gsl_ran_discrete(EIDOS_GSL_RNG, lookup_mutation_type_)];
}

void GenomicElementType::SetNucleotideMutationMatrix(EidosValue_Float_vector_SP p_mutation_matrix)
{
	mutation_matrix_ = p_mutation_matrix;
	
	// integrity checks
	if (mutation_matrix_->DimensionCount() != 2)
		EIDOS_TERMINATION << "ERROR (GenomicElementType::SetNucleotideMutationMatrix): initializeGenomicElementType() requires mutationMatrix to be a 4x4 or 64x4 matrix." << EidosTerminate();
	
	const int64_t *dims = mutation_matrix_->Dimensions();
	
	if ((dims[0] == 4) && (dims[1] == 4))
	{
		// This is the 4x4 matrix case, providing rates for each original nucleotide (rows) to each derived nucleotide (cols)
		
		// check for zeros in the necessary positions
		static int required_zeros_4x4[4] = {0, 5, 10, 15};
		
		for (int index = 0; index < 4; ++index)
			if (mutation_matrix_->FloatAtIndex(required_zeros_4x4[index], nullptr) != 0.0)
				EIDOS_TERMINATION << "ERROR (GenomicElementType::SetNucleotideMutationMatrix): the mutationMatrix must contain 0.0 for all entries that correspond to a nucleotide mutating to itself." << EidosTerminate();
		
		// check that each row sums to <= 1.0; in fact this has to be <= 1.0 even when multiplied by the hotspot map, but this is a preliminary sanity check
		// check also for no negative values, and for all values being finite
		for (int row = 0; row < 4; ++row)
		{
			double row_1 = mutation_matrix_->FloatAtIndex(row, nullptr);
			double row_2 = mutation_matrix_->FloatAtIndex(row + 4, nullptr);
			double row_3 = mutation_matrix_->FloatAtIndex(row + 8, nullptr);
			double row_4 = mutation_matrix_->FloatAtIndex(row + 12, nullptr);
			
			if ((row_1 < 0.0) || (row_2 < 0.0) || (row_3 < 0.0) || (row_4 < 0.0) ||
				!std::isfinite(row_1) || !std::isfinite(row_2) || !std::isfinite(row_3) || !std::isfinite(row_4))
				EIDOS_TERMINATION << "ERROR (GenomicElementType::SetNucleotideMutationMatrix): initializeGenomicElementType() requires all mutation matrix values to be finite and >= 0.0." << EidosTerminate();
			if (row_1 + row_2 + row_3 + row_4 > 1.0)
				EIDOS_TERMINATION << "ERROR (GenomicElementType::SetNucleotideMutationMatrix): initializeGenomicElementType() requires the sum of each mutation matrix row (the total probability of mutating for the given nucleotide or trinucleotide) to be <= 1.0." << EidosTerminate();
		}
	}
	else if ((dims[0] == 64) && (dims[1] == 4))
	{
		// This is the 64x4 matrix case, providing rates for each original trinucleotide (rows) to each derived nucleotide (cols)
		
		// check for zeros in the necessary positions
		static int required_zeros_64x4[64] = {0, 1, 2, 3, 16, 17, 18, 19, 32, 33, 34, 35, 48, 49, 50, 51, 68, 69, 70, 71, 84, 85, 86, 87, 100, 101, 102, 103, 116, 117, 118, 119, 136, 137, 138, 139, 152, 153, 154, 155, 168, 169, 170, 171, 184, 185, 186, 187, 204, 205, 206, 207, 220, 221, 222, 223, 236, 237, 238, 239, 252, 253, 254, 255};
		
		for (int index = 0; index < 64; ++index)
			if (mutation_matrix_->FloatAtIndex(required_zeros_64x4[index], nullptr) != 0.0)
				EIDOS_TERMINATION << "ERROR (GenomicElementType::SetNucleotideMutationMatrix): the mutationMatrix must contain 0.0 for all entries that correspond to a nucleotide mutating to itself." << EidosTerminate();
		
		// check that each row sums to <= 1.0; in fact this has to be <= 1.0 even when multiplied by the hotspot map, but this is a preliminary sanity check
		// check also for no negative values
		for (int row = 0; row < 64; ++row)
		{
			double row_1 = mutation_matrix_->FloatAtIndex(row, nullptr);
			double row_2 = mutation_matrix_->FloatAtIndex(row + 64, nullptr);
			double row_3 = mutation_matrix_->FloatAtIndex(row + 128, nullptr);
			double row_4 = mutation_matrix_->FloatAtIndex(row + 192, nullptr);
			
			if ((row_1 < 0.0) || (row_2 < 0.0) || (row_3 < 0.0) || (row_4 < 0.0) ||
				!std::isfinite(row_1) || !std::isfinite(row_2) || !std::isfinite(row_3) || !std::isfinite(row_4))
				EIDOS_TERMINATION << "ERROR (GenomicElementType::SetNucleotideMutationMatrix): initializeGenomicElementType() requires all mutation matrix values to be finite and >= 0.0." << EidosTerminate();
			if (row_1 + row_2 + row_3 + row_4 > 1.0)
				EIDOS_TERMINATION << "ERROR (GenomicElementType::SetNucleotideMutationMatrix): initializeGenomicElementType() requires the sum of each mutation matrix row (the total probability of mutating for the given nucleotide or trinucleotide) to be <= 1.0." << EidosTerminate();
		}
	}
	else
		EIDOS_TERMINATION << "ERROR (GenomicElementType::SetNucleotideMutationMatrix): initializeGenomicElementType() requires mutationMatrix to be a 4x4 or 64x4 matrix." << EidosTerminate();
}

// This is unused except by debugging code and in the debugger itself
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
		
		for (unsigned int i = 0; i < p_genomic_element_type.mutation_type_ptrs_.size(); ++i)
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
		
		for (unsigned int i = 0; i < p_genomic_element_type.mutation_fractions_.size(); ++i)
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
//	Eidos support
//
#pragma mark -
#pragma mark Eidos support
#pragma mark -

const EidosObjectClass *GenomicElementType::Class(void) const
{
	return gSLiM_GenomicElementType_Class;
}

void GenomicElementType::Print(std::ostream &p_ostream) const
{
	p_ostream << Class()->ElementType() << "<g" << genomic_element_type_id_ << ">";
}

EidosValue_SP GenomicElementType::GetProperty(EidosGlobalStringID p_property_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
			// constants
		case gID_id:			// ACCELERATED
		{
			if (!cached_value_getype_id_)
				cached_value_getype_id_ = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(genomic_element_type_id_));
			return cached_value_getype_id_;
		}
		case gID_mutationTypes:
		{
			EidosValue_Object_vector *vec = new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_MutationType_Class);
			EidosValue_SP result_SP = EidosValue_SP(vec);
			
			for (auto mut_type = mutation_type_ptrs_.begin(); mut_type != mutation_type_ptrs_.end(); ++mut_type)
				vec->push_object_element(*mut_type);
			
			return result_SP;
		}
		case gID_mutationFractions:
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector(mutation_fractions_));
		}
		case gID_mutationMatrix:
		{
			if (!mutation_matrix_)
				EIDOS_TERMINATION << "ERROR (GenomicElementType::GetProperty): property mutationMatrix is only defined in nucleotide-based models." << EidosTerminate();
			return mutation_matrix_;
		}
		
			// variables
		case gEidosID_color:
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(color_));
		case gID_tag:			// ACCELERATED
		{
			slim_usertag_t tag_value = tag_value_;
			
			if (tag_value == SLIM_TAG_UNSET_VALUE)
				EIDOS_TERMINATION << "ERROR (GenomicElementType::GetProperty): property tag accessed on genomic element type before being set." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(tag_value));
		}
			
			// all others, including gID_none
		default:
			return EidosObjectElement::GetProperty(p_property_id);
	}
}

EidosValue *GenomicElementType::GetProperty_Accelerated_id(EidosObjectElement **p_values, size_t p_values_size)
{
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		GenomicElementType *value = (GenomicElementType *)(p_values[value_index]);
		
		int_result->set_int_no_check(value->genomic_element_type_id_, value_index);
	}
	
	return int_result;
}

EidosValue *GenomicElementType::GetProperty_Accelerated_tag(EidosObjectElement **p_values, size_t p_values_size)
{
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		GenomicElementType *value = (GenomicElementType *)(p_values[value_index]);
		slim_usertag_t tag_value = value->tag_value_;
		
		if (tag_value == SLIM_TAG_UNSET_VALUE)
			EIDOS_TERMINATION << "ERROR (GenomicElementType::GetProperty): property tag accessed on genomic element type before being set." << EidosTerminate();
		
		int_result->set_int_no_check(tag_value, value_index);
	}
	
	return int_result;
}

void GenomicElementType::SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value)
{
	switch (p_property_id)
	{
		case gEidosID_color:
		{
			color_ = p_value.StringAtIndex(0, nullptr);
			if (!color_.empty())
				Eidos_GetColorComponents(color_, &color_red_, &color_green_, &color_blue_);
			
			// tweak a flag to make SLiMgui update
			sim_.genomic_element_types_changed_ = true;
			
			return;
		}
		case gID_tag:
		{
			slim_usertag_t value = SLiMCastToUsertagTypeOrRaise(p_value.IntAtIndex(0, nullptr));
			
			tag_value_ = value;
			return;
		}
			
		default:
		{
			return EidosObjectElement::SetProperty(p_property_id, p_value);
		}
	}
}

EidosValue_SP GenomicElementType::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
	switch (p_method_id)
	{
		case gID_setMutationFractions:	return ExecuteMethod_setMutationFractions(p_method_id, p_arguments, p_argument_count, p_interpreter);
		case gID_setMutationMatrix:		return ExecuteMethod_setMutationMatrix(p_method_id, p_arguments, p_argument_count, p_interpreter);
		default:						return SLiMEidosDictionary::ExecuteInstanceMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
	}
}

//	*********************	- (void)setMutationFractions(io<MutationType> mutationTypes, numeric proportions)
//
EidosValue_SP GenomicElementType::ExecuteMethod_setMutationFractions(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_argument_count, p_interpreter)
	SLiMSim &sim = SLiM_GetSimFromInterpreter(p_interpreter);
	EidosValue *mutationTypes_value = p_arguments[0].get();
	EidosValue *proportions_value = p_arguments[1].get();
	
	int mut_type_id_count = mutationTypes_value->Count();
	int proportion_count = proportions_value->Count();
	
	if (mut_type_id_count != proportion_count)
		EIDOS_TERMINATION << "ERROR (GenomicElementType::ExecuteMethod_setMutationFractions): setMutationFractions() requires the sizes of mutationTypes and proportions to be equal." << EidosTerminate();
	
	std::vector<MutationType*> mutation_types;
	std::vector<double> mutation_fractions;
	
	for (int mut_type_index = 0; mut_type_index < mut_type_id_count; ++mut_type_index)
	{ 
		MutationType *mutation_type_ptr = SLiM_ExtractMutationTypeFromEidosValue_io(mutationTypes_value, mut_type_index, sim, "setMutationFractions()");
		double proportion = proportions_value->FloatAtIndex(mut_type_index, nullptr);
		
		if ((proportion < 0) || !std::isfinite(proportion))		// == 0 is allowed but must be fixed before the simulation executes; see InitializeDraws()
			EIDOS_TERMINATION << "ERROR (GenomicElementType::ExecuteMethod_setMutationFractions): setMutationFractions() proportions must be greater than or equal to zero (" << EidosStringForFloat(proportion) << " supplied)." << EidosTerminate();
		
		if (std::find(mutation_types.begin(), mutation_types.end(), mutation_type_ptr) != mutation_types.end())
			EIDOS_TERMINATION << "ERROR (GenomicElementType::ExecuteMethod_setMutationFractions): setMutationFractions() mutation type m" << mutation_type_ptr->mutation_type_id_ << " used more than once." << EidosTerminate();
		
		mutation_types.emplace_back(mutation_type_ptr);
		mutation_fractions.emplace_back(proportion);
		
		// check whether we are now using a mutation type that is non-neutral; check and set pure_neutral_
		if ((mutation_type_ptr->dfe_type_ != DFEType::kFixed) || (mutation_type_ptr->dfe_parameters_[0] != 0.0))
			sim.pure_neutral_ = false;
	}
	
	// Everything seems to be in order, so replace our mutation info with the new info
	mutation_type_ptrs_ = mutation_types;
	mutation_fractions_ = mutation_fractions;
	
	// Reinitialize our mutation type lookup based on the new info
	InitializeDraws();
	
	// Notify interested parties of the change
	sim.genomic_element_types_changed_ = true;
	
	return gStaticEidosValueVOID;
}

//	*********************	- (void)setMutationMatrix(float mutationMatrix)
//
EidosValue_SP GenomicElementType::ExecuteMethod_setMutationMatrix(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_argument_count, p_interpreter)
	SLiMSim *sim = (SLiMSim *)p_interpreter.Context();
	
	if (!sim->IsNucleotideBased())
		EIDOS_TERMINATION << "ERROR (GenomicElementType::ExecuteMethod_setMutationMatrix): setMutationMatrix() may only be called in nucleotide-based models." << EidosTerminate();
	
	EidosValue *mutationMatrix_value = p_arguments[0].get();
	
	SetNucleotideMutationMatrix(EidosValue_Float_vector_SP((EidosValue_Float_vector *)(mutationMatrix_value)));
	
	// the change to the mutation matrix means everything downstream has to be recached
	sim_.CacheNucleotideMatrices();
	sim_.CreateNucleotideMutationRateMap();
	sim_.TheChromosome().InitializeDraws();
	
	return gStaticEidosValueVOID;
}


//
//	GenomicElementType_Class
//
#pragma mark -
#pragma mark GenomicElementType_Class
#pragma mark -

class GenomicElementType_Class : public SLiMEidosDictionary_Class
{
public:
	GenomicElementType_Class(const GenomicElementType_Class &p_original) = delete;	// no copy-construct
	GenomicElementType_Class& operator=(const GenomicElementType_Class&) = delete;	// no copying
	inline GenomicElementType_Class(void) { }
	
	virtual const std::string &ElementType(void) const;
	
	virtual const std::vector<const EidosPropertySignature *> *Properties(void) const;
	virtual const std::vector<const EidosMethodSignature *> *Methods(void) const;
};

EidosObjectClass *gSLiM_GenomicElementType_Class = new GenomicElementType_Class();


const std::string &GenomicElementType_Class::ElementType(void) const
{
	return gStr_GenomicElementType;
}

const std::vector<const EidosPropertySignature *> *GenomicElementType_Class::Properties(void) const
{
	static std::vector<const EidosPropertySignature *> *properties = nullptr;
	
	if (!properties)
	{
		properties = new std::vector<const EidosPropertySignature *>(*SLiMEidosDictionary_Class::Properties());
		
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_id,					true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(GenomicElementType::GetProperty_Accelerated_id));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_mutationTypes,		true,	kEidosValueMaskObject, gSLiM_MutationType_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_mutationFractions,	true,	kEidosValueMaskFloat)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_mutationMatrix,		true,	kEidosValueMaskFloat)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_tag,				false,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(GenomicElementType::GetProperty_Accelerated_tag));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gEidosStr_color,			false,	kEidosValueMaskString | kEidosValueMaskSingleton)));
		
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const std::vector<const EidosMethodSignature *> *GenomicElementType_Class::Methods(void) const
{
	static std::vector<const EidosMethodSignature *> *methods = nullptr;
	
	if (!methods)
	{
		methods = new std::vector<const EidosMethodSignature *>(*SLiMEidosDictionary_Class::Methods());
		
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setMutationFractions, kEidosValueMaskVOID))->AddIntObject("mutationTypes", gSLiM_MutationType_Class)->AddNumeric("proportions"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setMutationMatrix, kEidosValueMaskVOID))->AddFloat("mutationMatrix"));
		
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}


































































