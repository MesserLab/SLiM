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
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"


GenomicElementType::GenomicElementType(int p_genomic_element_type_id, std::vector<MutationType*> p_mutation_type_ptrs, std::vector<double> p_mutation_fractions) :
	genomic_element_type_id_(p_genomic_element_type_id), mutation_type_ptrs_(p_mutation_type_ptrs), mutation_fractions_(p_mutation_fractions)
{
	if (mutation_type_ptrs_.size() != mutation_fractions_.size())
		EIDOS_TERMINATION << "ERROR (Initialize): mutation types and fractions have different sizes" << eidos_terminate();
	
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
	//EIDOS_ERRSTREAM << "GenomicElementType::~GenomicElementType" << std::endl;
	
	if (lookup_mutation_type)
		gsl_ran_discrete_free(lookup_mutation_type);
	
	if (self_symbol_)
	{
		delete self_symbol_->second;
		delete self_symbol_;
	}
	
	if (cached_value_getype_id_)
		delete cached_value_getype_id_;
}

MutationType *GenomicElementType::DrawMutationType() const
{
	return mutation_type_ptrs_[gsl_ran_discrete(gEidos_rng, lookup_mutation_type)];
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
// Eidos support
//

void GenomicElementType::GenerateCachedSymbolTableEntry(void)
{
	// Note that this cache cannot be invalidated, because we are guaranteeing that this object will
	// live for at least as long as the symbol table it may be placed into!
	std::ostringstream getype_stream;
	
	getype_stream << "g" << genomic_element_type_id_;
	
	self_symbol_ = new EidosSymbolTableEntry(getype_stream.str(), (new EidosValue_Object_singleton_const(this))->SetExternalPermanent());
}

const EidosObjectClass *GenomicElementType::Class(void) const
{
	return gSLiM_GenomicElementType_Class;
}

void GenomicElementType::Print(std::ostream &p_ostream) const
{
	p_ostream << *Class()->ElementType() << "<g" << genomic_element_type_id_ << ">";
}

EidosValue *GenomicElementType::GetProperty(EidosGlobalStringID p_property_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
			// constants
		case gID_id:
		{
			// Note that this cache cannot be invalidated, because we are guaranteeing that this object will
			// live for at least as long as the symbol table it may be placed into!
			if (!cached_value_getype_id_)
				cached_value_getype_id_ = (new EidosValue_Int_singleton_const(genomic_element_type_id_))->SetExternalPermanent();
			return cached_value_getype_id_;
		}
		case gID_mutationTypes:
		{
			EidosValue_Object_vector *vec = new EidosValue_Object_vector();
			
			for (auto mut_type = mutation_type_ptrs_.begin(); mut_type != mutation_type_ptrs_.end(); ++mut_type)
				vec->PushElement(*mut_type);
			
			return vec;
		}
		case gID_mutationFractions:
			return new EidosValue_Float_vector(mutation_fractions_);
			
			// variables
		case gID_tag:
			return new EidosValue_Int_singleton_const(tag_value_);
			
			// all others, including gID_none
		default:
			return EidosObjectElement::GetProperty(p_property_id);
	}
}

void GenomicElementType::SetProperty(EidosGlobalStringID p_property_id, EidosValue *p_value)
{
	switch (p_property_id)
	{
		case gID_tag:
		{
			int64_t value = p_value->IntAtIndex(0);
			
			tag_value_ = value;
			return;
		}
			
		default:
		{
			return EidosObjectElement::SetProperty(p_property_id, p_value);
		}
	}
}

EidosValue *GenomicElementType::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, EidosValue *const *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
	EidosValue *arg0_value = ((p_argument_count >= 1) ? p_arguments[0] : nullptr);
	EidosValue *arg1_value = ((p_argument_count >= 2) ? p_arguments[1] : nullptr);
	
	//
	//	*********************	- (void)setMutationFractions(object mutationTypes, numeric proportions)
	//
#pragma mark -setMutationFractions()
	
	if (p_method_id == gID_setMutationFractions)
	{
		int mut_type_id_count = arg0_value->Count();
		int proportion_count = arg1_value->Count();
		
		if ((mut_type_id_count != proportion_count) || (mut_type_id_count == 0))
			EIDOS_TERMINATION << "ERROR (GenomicElementType::ExecuteInstanceMethod): setMutationFractions() requires the sizes of mutationTypeIDs and proportions to be equal and nonzero." << eidos_terminate();
		
		std::vector<MutationType*> mutation_types;
		std::vector<double> mutation_fractions;
		
		for (int mut_type_index = 0; mut_type_index < mut_type_id_count; ++mut_type_index)
		{ 
			MutationType *mutation_type_ptr = (MutationType *)arg0_value->ObjectElementAtIndex(mut_type_index);
			double proportion = arg1_value->FloatAtIndex(mut_type_index);
			
			if (proportion <= 0)
				EIDOS_TERMINATION << "ERROR (GenomicElementType::ExecuteInstanceMethod): setMutationFractions() proportions must be greater than zero." << eidos_terminate();
			
			mutation_types.push_back(mutation_type_ptr);
			mutation_fractions.push_back(proportion);
		}
		
		// Everything seems to be in order, so replace our mutation info with the new info
		mutation_type_ptrs_ = mutation_types;
		mutation_fractions_ = mutation_fractions;
		
		return gStaticEidosValueNULLInvisible;
	}
	
	
	// all others, including gID_none
	else
		return EidosObjectElement::ExecuteInstanceMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
}


//
//	GenomicElementType_Class
//
#pragma mark GenomicElementType_Class

class GenomicElementType_Class : public EidosObjectClass
{
public:
	GenomicElementType_Class(const GenomicElementType_Class &p_original) = delete;	// no copy-construct
	GenomicElementType_Class& operator=(const GenomicElementType_Class&) = delete;	// no copying
	
	GenomicElementType_Class(void);
	
	virtual const std::string *ElementType(void) const;
	
	virtual const std::vector<const EidosPropertySignature *> *Properties(void) const;
	virtual const EidosPropertySignature *SignatureForProperty(EidosGlobalStringID p_property_id) const;
	
	virtual const std::vector<const EidosMethodSignature *> *Methods(void) const;
	virtual const EidosMethodSignature *SignatureForMethod(EidosGlobalStringID p_method_id) const;
	virtual EidosValue *ExecuteClassMethod(EidosGlobalStringID p_method_id, EidosValue *const *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const;
};

EidosObjectClass *gSLiM_GenomicElementType_Class = new GenomicElementType_Class();


GenomicElementType_Class::GenomicElementType_Class(void)
{
}

const std::string *GenomicElementType_Class::ElementType(void) const
{
	return &gStr_GenomicElementType;
}

const std::vector<const EidosPropertySignature *> *GenomicElementType_Class::Properties(void) const
{
	static std::vector<const EidosPropertySignature *> *properties = nullptr;
	
	if (!properties)
	{
		properties = new std::vector<const EidosPropertySignature *>(*EidosObjectClass::Properties());
		properties->push_back(SignatureForProperty(gID_id));
		properties->push_back(SignatureForProperty(gID_mutationTypes));
		properties->push_back(SignatureForProperty(gID_mutationFractions));
		properties->push_back(SignatureForProperty(gID_tag));
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const EidosPropertySignature *GenomicElementType_Class::SignatureForProperty(EidosGlobalStringID p_property_id) const
{
	// Signatures are all preallocated, for speed
	static EidosPropertySignature *idSig = nullptr;
	static EidosPropertySignature *mutationTypesSig = nullptr;
	static EidosPropertySignature *mutationFractionsSig = nullptr;
	static EidosPropertySignature *tagSig = nullptr;
	
	if (!idSig)
	{
		idSig =					(EidosPropertySignature *)(new EidosPropertySignature(gStr_id,					gID_id,					true,	kEidosValueMaskInt | kEidosValueMaskSingleton));
		mutationTypesSig =		(EidosPropertySignature *)(new EidosPropertySignature(gStr_mutationTypes,		gID_mutationTypes,		true,	kEidosValueMaskObject, gSLiM_MutationType_Class));
		mutationFractionsSig =	(EidosPropertySignature *)(new EidosPropertySignature(gStr_mutationFractions,	gID_mutationFractions,	true,	kEidosValueMaskFloat));
		tagSig =				(EidosPropertySignature *)(new EidosPropertySignature(gStr_tag,					gID_tag,				false,	kEidosValueMaskInt | kEidosValueMaskSingleton));
	}
	
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		case gID_id:				return idSig;
		case gID_mutationTypes:		return mutationTypesSig;
		case gID_mutationFractions:	return mutationFractionsSig;
		case gID_tag:				return tagSig;
			
			// all others, including gID_none
		default:
			return EidosObjectClass::SignatureForProperty(p_property_id);
	}
}

const std::vector<const EidosMethodSignature *> *GenomicElementType_Class::Methods(void) const
{
	std::vector<const EidosMethodSignature *> *methods = nullptr;
	
	if (!methods)
	{
		methods = new std::vector<const EidosMethodSignature *>(*EidosObjectClass::Methods());
		methods->push_back(SignatureForMethod(gID_setMutationFractions));
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}

const EidosMethodSignature *GenomicElementType_Class::SignatureForMethod(EidosGlobalStringID p_method_id) const
{
	static EidosInstanceMethodSignature *setMutationFractionsSig = nullptr;
	
	if (!setMutationFractionsSig)
	{
		setMutationFractionsSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setMutationFractions, kEidosValueMaskNULL))->AddObject("mutationTypes", gSLiM_MutationType_Class)->AddNumeric("proportions");
	}
	
	if (p_method_id == gID_setMutationFractions)
		return setMutationFractionsSig;
	else
		return EidosObjectClass::SignatureForMethod(p_method_id);
}

EidosValue *GenomicElementType_Class::ExecuteClassMethod(EidosGlobalStringID p_method_id, EidosValue *const *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const
{
	return EidosObjectClass::ExecuteClassMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
}


































































