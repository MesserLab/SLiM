//
//  trait.cpp
//  SLiM
//
//  Created by Ben Haller on 6/25/25.
//  Copyright © 2025 Messer Lab, http://messerlab.org/software/. All rights reserved.
//

#include "trait.h"
#include "community.h"
#include "species.h"


Trait::Trait(Species &p_species, const std::string &p_name, TraitType p_type, double p_baselineOffset, double p_individualOffsetMean, double p_individualOffsetSD, bool p_directFitnessEffect) :
	index_(-1), name_(p_name), type_(p_type), baselineOffset_(p_baselineOffset),
	individualOffsetMean_(p_individualOffsetMean), individualOffsetSD_(p_individualOffsetSD),
	directFitnessEffect_(p_directFitnessEffect), community_(p_species.community_), species_(p_species)
{
}

Trait::~Trait(void)
{
	//EIDOS_ERRSTREAM << "Trait::~Trait" << std::endl;
}

const EidosClass *Trait::Class(void) const
{
	return gSLiM_Trait_Class;
}

void Trait::Print(std::ostream &p_ostream) const
{
	p_ostream << Class()->ClassNameForDisplay() << "<" << name_ << ">";
}

EidosValue_SP Trait::GetProperty(EidosGlobalStringID p_property_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
			// constants
		case gID_index:
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(index_));
		}
		case gID_name:
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String(name_));
		}
		case gID_species:
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(&species_, gSLiM_Species_Class));
		}
		case gEidosID_type:
		{
			static EidosValue_SP static_type_string_multiplicative;
			static EidosValue_SP static_type_string_additive;
			
#pragma omp critical (GetProperty_trait_type)
			{
				if (!static_type_string_multiplicative)
				{
					static_type_string_multiplicative = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("multiplicative"));
					static_type_string_additive = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("additive"));
				}
			}
			
			switch (type_)
			{
				case TraitType::kMultiplicative:		return static_type_string_multiplicative;
				case TraitType::kAdditive:				return static_type_string_additive;
				default:	return gStaticEidosValueNULL;	// never hit; here to make the compiler happy
			}
		}
			
			// variables
		case gID_baselineOffset:
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(baselineOffset_));
		}
		case gID_directFitnessEffect:
		{
			return (directFitnessEffect_ ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
		}
		case gID_individualOffsetMean:
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(individualOffsetMean_));
		}
		case gID_individualOffsetSD:
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(individualOffsetSD_));
		}
		case gID_tag:
		{
			slim_usertag_t tag_value = tag_value_;
			
			if (tag_value == SLIM_TAG_UNSET_VALUE)
				EIDOS_TERMINATION << "ERROR (Trait::GetProperty): property tag accessed on trait before being set." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(tag_value));
		}
			
			// all others, including gID_none
		default:
			return super::GetProperty(p_property_id);
	}
}

void Trait::SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		case gID_baselineOffset:
		{
			double value = p_value.FloatAtIndex_NOCAST(0, nullptr);
			
			if (!std::isfinite(value))
				EIDOS_TERMINATION << "ERROR (Trait::SetProperty): property baselineOffset requires a finite value (not NAN or INF)." << EidosTerminate();
			
			baselineOffset_ = value;
			return;
		}
		case gID_directFitnessEffect:
		{
			bool value = p_value.LogicalAtIndex_NOCAST(0, nullptr);
			
			directFitnessEffect_ = value;
			return;
		}
		case gID_individualOffsetMean:
		{
			double value = p_value.FloatAtIndex_NOCAST(0, nullptr);
			
			if (!std::isfinite(value))
				EIDOS_TERMINATION << "ERROR (Trait::SetProperty): property individualOffsetMean requires a finite value (not NAN or INF)." << EidosTerminate();
			
			individualOffsetMean_ = value;
			return;
		}
		case gID_individualOffsetSD:
		{
			double value = p_value.FloatAtIndex_NOCAST(0, nullptr);
			
			if (!std::isfinite(value))
				EIDOS_TERMINATION << "ERROR (Trait::SetProperty): property individualOffsetSD requires a finite value (not NAN or INF)." << EidosTerminate();
			
			individualOffsetSD_ = value;
			return;
		}
		case gID_tag:
		{
			slim_usertag_t value = SLiMCastToUsertagTypeOrRaise(p_value.IntAtIndex_NOCAST(0, nullptr));
			
			tag_value_ = value;
			return;
		}
			
			// all others, including gID_none
		default:
			return super::SetProperty(p_property_id, p_value);
	}
}

EidosValue_SP Trait::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	switch (p_method_id)
	{
		default:								return super::ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
	}
}


//
//	Trait_Class
//
#pragma mark -
#pragma mark Trait_Class
#pragma mark -

EidosClass *gSLiM_Trait_Class = nullptr;

const std::vector<EidosPropertySignature_CSP> *Trait_Class::Properties(void) const
{
	static std::vector<EidosPropertySignature_CSP> *properties = nullptr;
	
	if (!properties)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("Trait_Class::Properties(): not warmed up");
		
		properties = new std::vector<EidosPropertySignature_CSP>(*super::Properties());
		
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_baselineOffset,							false,	kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_directFitnessEffect,					false,	kEidosValueMaskLogical | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_index,									true,	kEidosValueMaskInt | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_individualOffsetMean,					false,	kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_individualOffsetSD,						false,	kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_name,									true,	kEidosValueMaskString | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_species,								true,	kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_Species_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_tag,									false,	kEidosValueMaskInt | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gEidosStr_type,								true,	kEidosValueMaskString | kEidosValueMaskSingleton)));
		
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const std::vector<EidosMethodSignature_CSP> *Trait_Class::Methods(void) const
{
	static std::vector<EidosMethodSignature_CSP> *methods = nullptr;
	
	if (!methods)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("Trait_Class::Methods(): not warmed up");
		
		methods = new std::vector<EidosMethodSignature_CSP>(*super::Methods());
		
		
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}






























