//
//  trait.cpp
//  SLiM
//
//  Created by Ben Haller on 6/25/25.
//  Copyright (c) 2025-2026 Benjamin C. Haller.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/slim/
//

#include "trait.h"
#include "community.h"
#include "species.h"


Trait::Trait(Species &p_species, const std::string &p_name, TraitType p_type, bool p_logistic_post, slim_trait_offset_t p_baselineOffset, double p_individualOffsetDistributionMean, double p_individualOffsetDistributionSD, bool p_directFitnessEffect, bool p_baselineAccumulation) :
	index_(-1), name_(p_name), type_(p_type), logistic_post_(p_logistic_post),
	individualOffsetDistributionMean_(p_individualOffsetDistributionMean), individualOffsetDistributionSD_(p_individualOffsetDistributionSD),
	directFitnessEffect_(p_directFitnessEffect), baselineAccumulation_(p_baselineAccumulation),
	community_(p_species.community_), species_(p_species)
{
	// offsets must always be finite
	if (!std::isfinite(p_baselineOffset))
		EIDOS_TERMINATION << "ERROR (Trait::Trait): (internal error) p_baselineOffset requires a finite value (not NAN or INF)." << EidosTerminate();
	if (!std::isfinite(individualOffsetDistributionMean_))
		EIDOS_TERMINATION << "ERROR (Trait::Trait): (internal error) individualOffsetDistributionMean_ requires a finite value (not NAN or INF)." << EidosTerminate();
	if (!std::isfinite(individualOffsetDistributionSD_) || (individualOffsetDistributionSD_ < 0.0))
		EIDOS_TERMINATION << "ERROR (Trait::Trait): (internal error) individualOffsetDistributionSD_ requires a nonnegative finite value (not NAN or INF)." << EidosTerminate();
	
	if (p_logistic_post && (type_ != TraitType::kAdditive))
		EIDOS_TERMINATION << "ERROR (Trait::Trait): (internal error) p_logistic_post is only supported for additive traits." << EidosTerminate();
	
	// effects for multiplicative traits clip at 0.0
	if ((type_ == TraitType::kMultiplicative) && (p_baselineOffset < (slim_trait_offset_t)0.0))
		baselineOffset_ = 0.0;
	else
		baselineOffset_ = p_baselineOffset;
	
	_RecacheIndividualOffsetDistribution();
}

void Trait::_RecacheIndividualOffsetDistribution(void)
{
	// cache for the fast case of an individual-offset SD of 0.0
	if (individualOffsetDistributionSD_ == 0.0)
	{
		individualOffsetDistributionFixed_ = true;
		
		if (type_ == TraitType::kMultiplicative)
		{
			// multiplicative traits use an exp() transformation to get a lognormal distribution
			// (effects for multiplicative traits also clip at 0.0, but exp() guarantees that anyway)
			individualOffsetDistributionFixedValue_ = static_cast<slim_trait_offset_t>(std::exp(individualOffsetDistributionMean_));
		}
		else
		{
			// additive and logistic traits use a normal distribution, so the mean is the mean
			individualOffsetDistributionFixedValue_ = static_cast<slim_trait_offset_t>(individualOffsetDistributionMean_);
		}
	}
	else
	{
		individualOffsetDistributionFixed_ = false;
	}
}

Trait::~Trait(void)
{
	//EIDOS_ERRSTREAM << "Trait::~Trait" << std::endl;
}

std::string Trait::UserVisibleType(void) const
{
	if (HasLogisticPostTransform())
		return "logistic";
	else if (Type() == TraitType::kAdditive)
		return "additive";
	else
		return "multiplicative";
}

const EidosClass *Trait::Class(void) const
{
	return gSLiM_Trait_Class;
}

void Trait::Print(std::ostream &p_ostream) const
{
	p_ostream << Class()->ClassNameForDisplay() << "<" << name_ << ">";
}

slim_trait_offset_t Trait::_DrawIndividualOffset(void) const
{
	// draws from a normal distribution defined by individualOffsetMean_ and individualOffsetSD_
	// note the individualOffsetSD_ == 0 case was already handled by DrawIndividualOffset()
	gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
	
	if (type_ == TraitType::kMultiplicative)
	{
		// multiplicative traits use an exp() transformation to get a lognormal distribution
		// (effects for multiplicative traits also clip at 0.0, but exp() guarantees that anyway)
		double normal_draw = gsl_ran_gaussian(rng, individualOffsetDistributionSD_) + individualOffsetDistributionMean_;
		
		return static_cast<slim_trait_offset_t>(std::exp(normal_draw));
	}
	else
	{
		// additive and logistic traits use a normal distribution, so the mean is the mean
		double normal_draw = gsl_ran_gaussian(rng, individualOffsetDistributionSD_) + individualOffsetDistributionMean_;
		
		return static_cast<slim_trait_offset_t>(normal_draw);
	}
}

EidosValue_SP Trait::GetProperty(EidosGlobalStringID p_property_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
			// constants
		case gID_baselineAccumulation:
		{
			return (baselineAccumulation_ ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
		}
		case gID_directFitnessEffect:
		{
			return (directFitnessEffect_ ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
		}
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
			static EidosValue_SP static_type_string_logistic;
			
			// FIXME PARALLEL static string allocation like this should be done at startup, before we go multithreaded; this should not need a critical section
			// search for "static EidosValue_SP" and fix all of them
#pragma omp critical (GetProperty_trait_type)
			{
				if (!static_type_string_multiplicative)
				{
					static_type_string_multiplicative = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String(gStr_multiplicative));
					static_type_string_additive = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String(gStr_additive));
					static_type_string_logistic = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String(gStr_logistic));
				}
			}
			
			switch (type_)
			{
				case TraitType::kMultiplicative:		return static_type_string_multiplicative;
				case TraitType::kAdditive:				return (logistic_post_ ? static_type_string_logistic : static_type_string_additive);
				default:	return gStaticEidosValueNULL;	// never hit; here to make the compiler happy
			}
		}
			
			// variables
		case gID_baselineOffset:
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float((double)baselineOffset_));
		}
		case gID_individualOffsetMean:
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(individualOffsetDistributionMean_));
		}
		case gID_individualOffsetSD:
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(individualOffsetDistributionSD_));
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
			
			// effects for multiplicative traits clip at 0.0
			if ((type_ == TraitType::kMultiplicative) && (value < 0.0))
				baselineOffset_ = (slim_trait_offset_t)0.0;
			else
				baselineOffset_ = (slim_trait_offset_t)value;
			
			return;
		}
		case gID_individualOffsetMean:
		{
			double value = p_value.FloatAtIndex_NOCAST(0, nullptr);
			
			if (!std::isfinite(value))
				EIDOS_TERMINATION << "ERROR (Trait::SetProperty): property individualOffsetMean requires a finite value (not NAN or INF)." << EidosTerminate();
			
			individualOffsetDistributionMean_ = value;
			_RecacheIndividualOffsetDistribution();
			IndividualOffsetChanged();
			return;
		}
		case gID_individualOffsetSD:
		{
			double value = p_value.FloatAtIndex_NOCAST(0, nullptr);
			
			if (!std::isfinite(value) || (value < 0.0))
				EIDOS_TERMINATION << "ERROR (Trait::SetProperty): property individualOffsetSD requires a nonnegative finite value (not NAN or INF)." << EidosTerminate();
			
			individualOffsetDistributionSD_ = value;
			_RecacheIndividualOffsetDistribution();
			IndividualOffsetChanged();
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

Trait_Class *gSLiM_Trait_Class = nullptr;

std::vector<EidosPropertySignature_CSP> *Trait_Class::Properties_MUTABLE(void) const
{
	static std::vector<EidosPropertySignature_CSP> *properties = nullptr;
	
	if (!properties)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("Trait_Class::Properties(): not warmed up");
		
		properties = new std::vector<EidosPropertySignature_CSP>(*super::Properties_MUTABLE());
		
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_baselineAccumulation,					true,	kEidosValueMaskLogical | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_baselineOffset,							false,	kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_directFitnessEffect,					true,	kEidosValueMaskLogical | kEidosValueMaskSingleton)));
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






























