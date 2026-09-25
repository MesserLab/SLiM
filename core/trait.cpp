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
#include "subpopulation.h"
#include "individual.h"
#include "substitution.h"


Trait::Trait(Species &p_species, const std::string &p_name, TraitType p_type, bool p_logistic_post, double p_individualOffsetDistributionMean, double p_individualOffsetDistributionSD, bool p_directFitnessEffect, bool p_substitutionAccumulation) :
	index_(-1), name_(p_name), type_(p_type), logistic_post_(p_logistic_post),
	individualOffsetDistributionMean_(p_individualOffsetDistributionMean), individualOffsetDistributionSD_(p_individualOffsetDistributionSD),
	directFitnessEffect_(p_directFitnessEffect), substitutionAccumulation_(p_substitutionAccumulation),
	community_(p_species.community_), species_(p_species)
{
	// offsets must always be finite
	if (!std::isfinite(individualOffsetDistributionMean_))
		EIDOS_TERMINATION << "ERROR (Trait::Trait): (internal error) individualOffsetDistributionMean_ requires a finite value (not NAN or INF)." << EidosTerminate();
	if (!std::isfinite(individualOffsetDistributionSD_) || (individualOffsetDistributionSD_ < 0.0))
		EIDOS_TERMINATION << "ERROR (Trait::Trait): (internal error) individualOffsetDistributionSD_ requires a nonnegative finite value (not NAN or INF)." << EidosTerminate();
	
	if (p_logistic_post && (type_ != TraitType::kAdditive))
		EIDOS_TERMINATION << "ERROR (Trait::Trait): (internal error) p_logistic_post is only supported for additive traits." << EidosTerminate();
	
	// set up initial baseline offsets and substitution offsets; the initial baseline is no longer passed in
	// to Trait::Trait(), since it is no longer a parameter for initializeTrait(); the user sets it after
	// BCH 9/7/2026: Note that for the default trait, SexEnabled() can be wrong here because initializeSex()
	// has not been called.  We fix that in _FixDefaultTraitInit().  For initializeTrait() it will be correct.
	if (species_.SexEnabled())
	{
		if (type_ == TraitType::kMultiplicative)
		{
			baselineOffset_M_ = 1.0;
			substitutionOffset_M_ = 1.0;
			compositeOffset_M_ = baselineOffset_M_ * substitutionOffset_M_;
			
			baselineOffset_F_ = 1.0;
			substitutionOffset_F_ = 1.0;
			compositeOffset_F_ = baselineOffset_F_ * substitutionOffset_F_;
		} else {
			baselineOffset_M_ = 0.0;
			substitutionOffset_M_ = 0.0;
			compositeOffset_M_ = baselineOffset_M_ + substitutionOffset_M_;
			
			baselineOffset_F_ = 0.0;
			substitutionOffset_F_ = 0.0;
			compositeOffset_F_ = baselineOffset_F_ + substitutionOffset_F_;
		}
		
		baselineOffset_H_ = std::numeric_limits<slim_trait_offset_t>::quiet_NaN();
		substitutionOffset_H_ = std::numeric_limits<slim_trait_offset_t>::quiet_NaN();
		compositeOffset_H_ = std::numeric_limits<slim_trait_offset_t>::quiet_NaN();
	}
	else
	{
		if (type_ == TraitType::kMultiplicative)
		{
			baselineOffset_H_ = 1.0;
			substitutionOffset_H_ = 1.0;
			compositeOffset_H_ = baselineOffset_H_ * substitutionOffset_H_;
		} else {
			baselineOffset_H_ = 0.0;
			substitutionOffset_H_ = 0.0;
			compositeOffset_H_ = baselineOffset_H_ + substitutionOffset_H_;
		}
		
		baselineOffset_M_ = std::numeric_limits<slim_trait_offset_t>::quiet_NaN();
		baselineOffset_F_ = std::numeric_limits<slim_trait_offset_t>::quiet_NaN();
		substitutionOffset_M_ = std::numeric_limits<slim_trait_offset_t>::quiet_NaN();
		substitutionOffset_F_ = std::numeric_limits<slim_trait_offset_t>::quiet_NaN();
		compositeOffset_M_ = std::numeric_limits<slim_trait_offset_t>::quiet_NaN();
		compositeOffset_F_ = std::numeric_limits<slim_trait_offset_t>::quiet_NaN();
	}
	
	_RecacheIndividualOffsetDistribution();
	
	// set up and retain the appropriate default palettes for SLiMgui; the user can change these with Trait properties
	if (type_ == TraitType::kMultiplicative)
    {
        individual_phenotype_palette_ = gEidos_Palette_IndividualMultiplicativePhenotype;
        mutation_effect_palette_ = gEidos_Palette_MutationMultiplicativeEffect;
    }
	else
    {
		if (logistic_post_)
		{
			individual_phenotype_palette_ = gEidos_Palette_IndividualLogisticPhenotype;
			mutation_effect_palette_ = gEidos_Palette_MutationLogisticEffect;
		}
		else
		{
			individual_phenotype_palette_ = gEidos_Palette_IndividualAdditivePhenotype;
			mutation_effect_palette_ = gEidos_Palette_MutationAdditiveEffect;
		}
    }
    
    individual_phenotype_palette_->Retain();
    mutation_effect_palette_->Retain();
}

void Trait::_FixDefaultTraitInit(void)
{
	// BCH 9/7/2026: If a default trait was created and the model turns out to be sexual, the default trait's
	// offset initialization needs to be re-done.  This is ugly; it would be nicer to require initializeSex()
	// to be called before the default trait is created.  However, that would break backward compatibility.
	if (!species_.SexEnabled())
		EIDOS_TERMINATION << "ERROR (Trait::_FixDefaultTraitInit): (internal error) called without separate sexes being enabled." << EidosTerminate();
	if (type_ != TraitType::kMultiplicative)
		EIDOS_TERMINATION << "ERROR (Trait::_FixDefaultTraitInit): (internal error) called for a trait that is not multiplicative (and thus not the default trait)." << EidosTerminate();
	
	baselineOffset_M_ = 1.0;
	substitutionOffset_M_ = 1.0;
	compositeOffset_M_ = baselineOffset_M_ * substitutionOffset_M_;
	
	baselineOffset_F_ = 1.0;
	substitutionOffset_F_ = 1.0;
	compositeOffset_F_ = baselineOffset_F_ * substitutionOffset_F_;
	
	baselineOffset_H_ = std::numeric_limits<slim_trait_offset_t>::quiet_NaN();
	substitutionOffset_H_ = std::numeric_limits<slim_trait_offset_t>::quiet_NaN();
	compositeOffset_H_ = std::numeric_limits<slim_trait_offset_t>::quiet_NaN();
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
    
    if (individual_phenotype_palette_)
    {
        individual_phenotype_palette_->Release();
        individual_phenotype_palette_ = nullptr;
    }
    if (mutation_effect_palette_)
    {
        mutation_effect_palette_->Release();
        mutation_effect_palette_ = nullptr;
    }
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

void Trait::InvalidateTraitValuesForAllIndividuals(IndividualSex p_sex)
{
	// TRAIT INVALIDATION: invalidate the trait values for the focal trait, for all individuals.
	// At first glance, it would be nice for this to be done via a flag on Trait instead.  The tricky thing about
	// that is that the flag might get set (invalidating everyone) and then the trait values of just a subset of
	// individuals might get validated.  The trait values of all remaining individuals would then need to be set
	// to NAN to preserve their invalidation; but managing that in general seems tricky.  Let's keep it simple.
	
	if ((p_sex == IndividualSex::kUnspecified) || (p_sex == IndividualSex::kHermaphrodite))
	{
		for (const auto &subpop_iter : species_.population_.subpops_)
		{
			const Subpopulation *subpop = subpop_iter.second;
			
			for (Individual *individual : subpop->parent_individuals_)
			{
				IndividualTraitInfo *trait_info = individual->trait_info_;
				
				trait_info[Index()].phenotype_ = SLIM_PHENOTYPE_NAN;
			}
		}
	}
	else
	{
		// sex-specific invalidation; this happens when we know that something sex-specific has changed,
		// such as the baseline offset for just one sex, so we only invalidate the individuals affected.
		for (const auto &subpop_iter : species_.population_.subpops_)
		{
			const Subpopulation *subpop = subpop_iter.second;
			
			for (Individual *individual : subpop->parent_individuals_)
			{
				if (individual->sex_ == p_sex)
				{
					IndividualTraitInfo *trait_info = individual->trait_info_;
					
					trait_info[Index()].phenotype_ = SLIM_PHENOTYPE_NAN;
				}
			}
		}
	}
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

void Trait::_SetBaselineOffset_H(slim_trait_offset_t p_offset)
{
#if DEBUG
	if (!std::isfinite(p_offset))
		EIDOS_TERMINATION << "ERROR (Trait::_SetBaselineOffset_H): (internal error) p_offset is not finite." << EidosTerminate();
	if ((Type() == TraitType::kMultiplicative) && (p_offset < 0.0))
		EIDOS_TERMINATION << "ERROR (Trait::_SetBaselineOffset_H): (internal error) p_offset < 0.0." << EidosTerminate();
#endif
	
	baselineOffset_H_ = p_offset;
	
	if (Type() == TraitType::kMultiplicative)
		compositeOffset_H_ = baselineOffset_H_ * substitutionOffset_H_;
	else
		compositeOffset_H_ = baselineOffset_H_ + substitutionOffset_H_;
}

void Trait::_SetBaselineOffset_M(slim_trait_offset_t p_offset)
{
#if DEBUG
	if (!std::isfinite(p_offset))
		EIDOS_TERMINATION << "ERROR (Trait::_SetBaselineOffset_M): (internal error) p_offset is not finite." << EidosTerminate();
	if ((Type() == TraitType::kMultiplicative) && (p_offset < 0.0))
		EIDOS_TERMINATION << "ERROR (Trait::_SetBaselineOffset_M): (internal error) p_offset < 0.0." << EidosTerminate();
#endif
	
	baselineOffset_M_ = p_offset;
	
	if (Type() == TraitType::kMultiplicative)
		compositeOffset_M_ = baselineOffset_M_ * substitutionOffset_M_;
	else
		compositeOffset_M_ = baselineOffset_M_ + substitutionOffset_M_;
}

void Trait::_SetBaselineOffset_F(slim_trait_offset_t p_offset)
{
#if DEBUG
	if (!std::isfinite(p_offset))
		EIDOS_TERMINATION << "ERROR (Trait::_SetBaselineOffset_F): (internal error) p_offset is not finite." << EidosTerminate();
	if ((Type() == TraitType::kMultiplicative) && (p_offset < 0.0))
		EIDOS_TERMINATION << "ERROR (Trait::_SetBaselineOffset_F): (internal error) p_offset < 0.0." << EidosTerminate();
#endif
	
	baselineOffset_F_ = p_offset;
	
	if (Type() == TraitType::kMultiplicative)
		compositeOffset_F_ = baselineOffset_F_ * substitutionOffset_F_;
	else
		compositeOffset_F_ = baselineOffset_F_ + substitutionOffset_F_;
}

void Trait::_ClearSubstitutionOffsets(void)
{
	if (species_.SexEnabled())
	{
		if (Type() == TraitType::kMultiplicative)
		{
			substitutionOffset_M_ = 1.0;
			compositeOffset_M_ = baselineOffset_M_ * substitutionOffset_M_;
			
			substitutionOffset_F_ = 1.0;
			compositeOffset_F_ = baselineOffset_F_ * substitutionOffset_F_;
		}
		else
		{
			substitutionOffset_M_ = 0.0;
			compositeOffset_M_ = baselineOffset_M_ + substitutionOffset_M_;
			
			substitutionOffset_F_ = 0.0;
			compositeOffset_F_ = baselineOffset_F_ + substitutionOffset_F_;
		}
		
		substitutionOffset_H_ = std::numeric_limits<double>::quiet_NaN();
		compositeOffset_H_ = std::numeric_limits<double>::quiet_NaN();
	}
	else
	{
		if (Type() == TraitType::kMultiplicative)
		{
			substitutionOffset_H_ = 1.0;
			compositeOffset_H_ = baselineOffset_H_ * substitutionOffset_H_;
		}
		else
		{
			substitutionOffset_H_ = 0.0;
			compositeOffset_H_ = baselineOffset_H_ + substitutionOffset_H_;
		}
		
		substitutionOffset_M_ = std::numeric_limits<double>::quiet_NaN();
		compositeOffset_M_ = std::numeric_limits<double>::quiet_NaN();
		
		substitutionOffset_F_ = std::numeric_limits<double>::quiet_NaN();
		compositeOffset_F_ = std::numeric_limits<double>::quiet_NaN();
	}
}

void Trait::AccumulateSubstitutionOffset(const Substitution *p_substitution, const SubstitutionTraitInfo &p_trait_info)
{
	slim_trait_offset_t effect_size = (slim_trait_offset_t)p_trait_info.effect_size_;
	
	if (effect_size == 0.0)
		return;
	
	slim_trait_offset_t hemizygous_dominance = (slim_trait_offset_t)p_trait_info.hemizygous_dominance_coeff_;
	Chromosome *associated_chromosome = species_.Chromosomes()[p_substitution->chromosome_index_];
	ChromosomeType chromosome_type = associated_chromosome->Type();
	
	if ((hemizygous_dominance != 1.0) &&
		((chromosome_type == ChromosomeType::kA_DiploidAutosome) || (chromosome_type == ChromosomeType::kH_HaploidAutosome)))
	{
		// BCH 9/7/2026: OK, tricky stuff here.  With separate substitution offsets for males and females, we
		// can now handle substitution accumulation for sex chromosomes, including any hemizygous dominance
		// coefficient, because we know how sex chromosomes occur in males versus females.  But we can't handle
		// substitution accumulation for autosomes, with a hemizygous dominance coefficient != 1.0, if the
		// substitution occurs in a chromosome that is found hemizygously, because we don't know what pattern
		// of occurrence will be followed, and we have no way of representing that pattern of occurrence.  So
		// we need to detect that and raise an error.  We do that with two flags, per chromosome.  One says
		// "a substitution has occurred, with hemizygous dominance != 1.0, for this chromosome".  The other
		// says "an individual has been observed that has a null haplosome for this chromosome".  If both flags
		// are true for a given autosome, substitution accumulation cannot be used and an error results.  (Note
		// that hemi_sub_accumulation_occurred_ does not need to be per-trait, because null_haplosome_observed_
		// is not per-trait in any case, and so if hemi_sub_accumulation_occurred_ is true for *any* trait and
		// null_haplosome_observed_ is also true then the error condition has been met.)
		associated_chromosome->hemi_sub_accumulation_occurred_ = true;
		
		if (associated_chromosome->null_haplosome_observed_)
			EIDOS_TERMINATION << "ERROR (Trait::AccumulateSubstitutionOffset): " << "substitution accumulation cannot occur for mutations that (1) are non-neutral for a given trait, and (2) are associated with a given autosome, IF (3) the given trait has a hemizygous dominance coefficient != 1.0, and (4) the given autosome is represented by a null haplosome in any individual.  Under these conditions, the effect of the substitution cannot be reliably represented by the trait's substitution offset(s).  To fix this error, you must change your model so that one of the four preconditions for this error is no longer met, OR -- most commonly -- you must turn off substitution for the mutation type(s) that trigger this problem by setting their convertToSubstitution property to F.  (Note that turning off substitution accumulation is typically NOT a valid fix, since then substitution will cause trait values to omit the trait effects of the mutations that get substituted, unless you compensate for that yourself in script.)" << EidosTerminate();
	}
	
	if (species_.SexEnabled())
	{
		if (Type() == TraitType::kMultiplicative)
		{
			switch (chromosome_type)
			{
					// the substitution offset effect in both sexes is the homozygous effect, 1+s
				case ChromosomeType::kA_DiploidAutosome:
				case ChromosomeType::kH_HaploidAutosome:
				case ChromosomeType::kHF_HaploidFemaleInherited:
				case ChromosomeType::kHM_HaploidMaleInherited:
				case ChromosomeType::kHNull_HaploidAutosomeWithNull:
					substitutionOffset_M_ *= (1.0 + effect_size);
					substitutionOffset_F_ *= (1.0 + effect_size);
					break;
					
					// the substitution offset effect is 1+s in females, 1+h_hemi*s in males
				case ChromosomeType::kX_XSexChromosome:
					substitutionOffset_M_ *= (1.0 + hemizygous_dominance * effect_size);
					substitutionOffset_F_ *= (1.0 + effect_size);
					break;
					
					// the substitution offset effect is 1 in females, 1+s in males
				case ChromosomeType::kY_YSexChromosome:
				case ChromosomeType::kML_HaploidMaleLine:
				case ChromosomeType::kNullY_YSexChromosomeWithNull:
					substitutionOffset_M_ *= (1.0 + effect_size);
					break;
					
					// the substitution offset effect is 1+h_hemi*s in females, 1+s in males
				case ChromosomeType::kZ_ZSexChromosome:
					substitutionOffset_M_ *= (1.0 + effect_size);
					substitutionOffset_F_ *= (1.0 + hemizygous_dominance * effect_size);
					break;
					
					// the substitution offset effect is 1+s in females, 1 in males
				case ChromosomeType::kW_WSexChromosome:
				case ChromosomeType::kFL_HaploidFemaleLine:
					substitutionOffset_F_ *= (1.0 + effect_size);
					break;
			}
			
			if (substitutionOffset_M_ < 0.0)
				substitutionOffset_M_ = 0.0;
			if (substitutionOffset_F_ < 0.0)
				substitutionOffset_F_ = 0.0;
			
			compositeOffset_M_ = baselineOffset_M_ * substitutionOffset_M_;
			compositeOffset_F_ = baselineOffset_F_ * substitutionOffset_F_;
		}
		else
		{
			switch (chromosome_type)
			{
					// the substitution offset effect in both sexes is the homozygous effect, 2a
				case ChromosomeType::kA_DiploidAutosome:
				case ChromosomeType::kH_HaploidAutosome:
				case ChromosomeType::kHF_HaploidFemaleInherited:
				case ChromosomeType::kHM_HaploidMaleInherited:
				case ChromosomeType::kHNull_HaploidAutosomeWithNull:
					substitutionOffset_M_ += (effect_size + effect_size);
					substitutionOffset_F_ += (effect_size + effect_size);
					break;
					
					// the substitution offset effect is 2a in females, 2*h_hemi*a in males
				case ChromosomeType::kX_XSexChromosome:
					substitutionOffset_M_ += (2.0 * hemizygous_dominance * effect_size);
					substitutionOffset_F_ += (effect_size + effect_size);
					break;
					
					// the substitution offset effect is 0 in females, 2a in males
				case ChromosomeType::kY_YSexChromosome:
				case ChromosomeType::kML_HaploidMaleLine:
				case ChromosomeType::kNullY_YSexChromosomeWithNull:
					substitutionOffset_M_ += (effect_size + effect_size);
					break;
					
					// the substitution offset effect is 2*h_hemi*a in females, 2a in males
				case ChromosomeType::kZ_ZSexChromosome:
					substitutionOffset_M_ += (effect_size + effect_size);
					substitutionOffset_F_ += (2.0 * hemizygous_dominance * effect_size);
					break;
					
					// the substitution offset effect is 2a in females, 0 in males
				case ChromosomeType::kW_WSexChromosome:
				case ChromosomeType::kFL_HaploidFemaleLine:
					substitutionOffset_F_ += (effect_size + effect_size);
					break;
			}
			
			compositeOffset_M_ = baselineOffset_M_ + substitutionOffset_M_;
			compositeOffset_F_ = baselineOffset_F_ + substitutionOffset_F_;
		}
	}
	else
	{
		if (Type() == TraitType::kMultiplicative)
		{
			substitutionOffset_H_ *= (1.0 + effect_size);			// 1+s
			
			if (substitutionOffset_H_ < 0.0)
				substitutionOffset_H_ = 0.0;
			
			compositeOffset_H_ = baselineOffset_H_ * substitutionOffset_H_;
		}
		else
		{
			substitutionOffset_H_ += (effect_size + effect_size);	// 2a
			
			compositeOffset_H_ = baselineOffset_H_ + substitutionOffset_H_;
		}
	}
}

#if DEBUG
void Trait::CheckTraitIntegrity(void) const
{
	if (type_ == TraitType::kMultiplicative)
	{
		if (species_.SexEnabled())
		{
			if (compositeOffset_M_ != baselineOffset_M_ * substitutionOffset_M_)
				EIDOS_TERMINATION << "ERROR (Trait::CheckTraitIntegrity): (internal error) compositeOffset_M_ mismatch (multiplicative)." << EidosTerminate();
			if (compositeOffset_F_ != baselineOffset_F_ * substitutionOffset_F_)
				EIDOS_TERMINATION << "ERROR (Trait::CheckTraitIntegrity): (internal error) compositeOffset_F_ mismatch (multiplicative)." << EidosTerminate();
			if (!std::isnan(compositeOffset_H_))
				EIDOS_TERMINATION << "ERROR (Trait::CheckTraitIntegrity): (internal error) compositeOffset_H_ not NAN." << EidosTerminate();
		}
		else
		{
			if (!std::isnan(compositeOffset_M_))
				EIDOS_TERMINATION << "ERROR (Trait::CheckTraitIntegrity): (internal error) compositeOffset_M_ not NAN." << EidosTerminate();
			if (!std::isnan(compositeOffset_F_))
				EIDOS_TERMINATION << "ERROR (Trait::CheckTraitIntegrity): (internal error) compositeOffset_F_ not NAN." << EidosTerminate();
			if (compositeOffset_H_ != baselineOffset_H_ * substitutionOffset_H_)
				EIDOS_TERMINATION << "ERROR (Trait::CheckTraitIntegrity): (internal error) compositeOffset_H_ mismatch (multiplicative)." << EidosTerminate();
		}
	}
	else
	{
		if (species_.SexEnabled())
		{
			if (compositeOffset_M_ != baselineOffset_M_ + substitutionOffset_M_)
				EIDOS_TERMINATION << "ERROR (Trait::CheckTraitIntegrity): (internal error) compositeOffset_M_ mismatch (additive)." << EidosTerminate();
			if (compositeOffset_F_ != baselineOffset_F_ + substitutionOffset_F_)
				EIDOS_TERMINATION << "ERROR (Trait::CheckTraitIntegrity): (internal error) compositeOffset_F_ mismatch (additive)." << EidosTerminate();
			if (!std::isnan(compositeOffset_H_))
				EIDOS_TERMINATION << "ERROR (Trait::CheckTraitIntegrity): (internal error) compositeOffset_H_ not NAN." << EidosTerminate();
		}
		else
		{
			if (!std::isnan(compositeOffset_M_))
				EIDOS_TERMINATION << "ERROR (Trait::CheckTraitIntegrity): (internal error) compositeOffset_M_ not NAN." << EidosTerminate();
			if (!std::isnan(compositeOffset_F_))
				EIDOS_TERMINATION << "ERROR (Trait::CheckTraitIntegrity): (internal error) compositeOffset_F_ not NAN." << EidosTerminate();
			if (compositeOffset_H_ != baselineOffset_H_ + substitutionOffset_H_)
				EIDOS_TERMINATION << "ERROR (Trait::CheckTraitIntegrity): (internal error) compositeOffset_H_ mismatch (additive)." << EidosTerminate();
		}
	}
}
#endif

EidosValue_SP Trait::GetProperty(EidosGlobalStringID p_property_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
			// constants
		case gID_compositeOffsetH:
		{
			if (species_.SexEnabled())
				EIDOS_TERMINATION << "ERROR (Trait::GetProperty): property compositeOffsetH can only be used in hermaphroditic species." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float((double)compositeOffset_H_));
		}
		case gID_compositeOffsetM:
		{
			if (!species_.SexEnabled())
				EIDOS_TERMINATION << "ERROR (Trait::GetProperty): property compositeOffsetM can only be used in sexual species." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float((double)compositeOffset_M_));
		}
		case gID_compositeOffsetF:
		{
			if (!species_.SexEnabled())
				EIDOS_TERMINATION << "ERROR (Trait::GetProperty): property compositeOffsetF can only be used in sexual species." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float((double)compositeOffset_F_));
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
		case gID_individualPhenotypePalette:
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(individual_phenotype_palette_, gEidosPalette_Class));
		}
		case gID_mutationEffectPalette:
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(mutation_effect_palette_, gEidosPalette_Class));
		}
		case gID_species:
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(&species_, gSLiM_Species_Class));
		}
		case gID_substitutionAccumulation:
		{
			return (substitutionAccumulation_ ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
		}
		case gID_substitutionOffsetH:
		{
			if (species_.SexEnabled())
				EIDOS_TERMINATION << "ERROR (Trait::GetProperty): property substitutionOffsetH can only be used in hermaphroditic species." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float((double)substitutionOffset_H_));
		}
		case gID_substitutionOffsetM:
		{
			if (!species_.SexEnabled())
				EIDOS_TERMINATION << "ERROR (Trait::GetProperty): property substitutionOffsetM can only be used in sexual species." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float((double)substitutionOffset_M_));
		}
		case gID_substitutionOffsetF:
		{
			if (!species_.SexEnabled())
				EIDOS_TERMINATION << "ERROR (Trait::GetProperty): property substitutionOffsetF can only be used in sexual species." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float((double)substitutionOffset_F_));
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
		case gID_baselineOffsetH:
		{
			if (species_.SexEnabled())
				EIDOS_TERMINATION << "ERROR (Trait::GetProperty): property baselineOffsetH can only be used in hermaphroditic species." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float((double)baselineOffset_H_));
		}
		case gID_baselineOffsetM:
		{
			if (!species_.SexEnabled())
				EIDOS_TERMINATION << "ERROR (Trait::GetProperty): property baselineOffsetM can only be used in sexual species." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float((double)baselineOffset_M_));
		}
		case gID_baselineOffsetF:
		{
			if (!species_.SexEnabled())
				EIDOS_TERMINATION << "ERROR (Trait::GetProperty): property baselineOffsetF can only be used in sexual species." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float((double)baselineOffset_F_));
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
		case gID_baselineOffsetH:
		{
			if (species_.SexEnabled())
				EIDOS_TERMINATION << "ERROR (Trait::SetProperty): property baselineOffsetH can only be used in hermaphroditic species." << EidosTerminate();
			
			double value = p_value.FloatAtIndex_NOCAST(0, nullptr);
			
			if (!std::isfinite(value))
				EIDOS_TERMINATION << "ERROR (Trait::SetProperty): property baselineOffsetH requires a finite value (not NAN or INF)." << EidosTerminate();
			
			// effects for multiplicative traits clip at 0.0
			if ((type_ == TraitType::kMultiplicative) && (value < 0.0))
				EIDOS_TERMINATION << "ERROR (Trait::SetProperty): property baselineOffsetH may not be set to a negative value for a multiplicative trait." << EidosTerminate();
			
			slim_trait_offset_t new_baseline = (slim_trait_offset_t)value;
			
			// if the baseline offset is not actually changing, ignore the property set
			if (baselineOffset_H_ == new_baseline)
				return;
			
			baselineOffset_H_ = new_baseline;
			
			if (Type() == TraitType::kMultiplicative)
				compositeOffset_H_ = baselineOffset_H_ * substitutionOffset_H_;
			else
				compositeOffset_H_ = baselineOffset_H_ + substitutionOffset_H_;
			
			if (!std::isfinite(baselineOffset_H_) || !std::isfinite(compositeOffset_H_))
				EIDOS_TERMINATION << "ERROR (Trait::SetProperty): the new baselineOffsetH value could not be represented due to numerical issues (roundoff, overflow)." << EidosTerminate();
			
			// TRAIT INVALIDATION: the trait value for this trait is invalidated in all individuals
			InvalidateTraitValuesForAllIndividuals();
			
			return;
		}
		case gID_baselineOffsetM:
		{
			if (!species_.SexEnabled())
				EIDOS_TERMINATION << "ERROR (Trait::GetProperty): property baselineOffsetM can only be used in sexual species." << EidosTerminate();
			
			double value = p_value.FloatAtIndex_NOCAST(0, nullptr);
			
			if (!std::isfinite(value))
				EIDOS_TERMINATION << "ERROR (Trait::SetProperty): property baselineOffsetM requires a finite value (not NAN or INF)." << EidosTerminate();
			
			// effects for multiplicative traits clip at 0.0
			if ((type_ == TraitType::kMultiplicative) && (value < 0.0))
				EIDOS_TERMINATION << "ERROR (Trait::SetProperty): property baselineOffsetM may not be set to a negative value for a multiplicative trait." << EidosTerminate();
			
			slim_trait_offset_t new_baseline = (slim_trait_offset_t)value;
			
			// if the baseline offset is not actually changing, ignore the property set
			if (baselineOffset_M_ == new_baseline)
				return;
			
			baselineOffset_M_ = new_baseline;
			
			if (Type() == TraitType::kMultiplicative)
				compositeOffset_M_ = baselineOffset_M_ * substitutionOffset_M_;
			else
				compositeOffset_M_ = baselineOffset_M_ + substitutionOffset_M_;
			
			if (!std::isfinite(baselineOffset_M_) || !std::isfinite(compositeOffset_M_))
				EIDOS_TERMINATION << "ERROR (Trait::SetProperty): the new baselineOffsetM value could not be represented due to numerical issues (roundoff, overflow)." << EidosTerminate();
			
			// TRAIT INVALIDATION: the trait value for this trait is invalidated in all individuals
			InvalidateTraitValuesForAllIndividuals(IndividualSex::kMale);
			
			return;
		}
		case gID_baselineOffsetF:
		{
			if (!species_.SexEnabled())
				EIDOS_TERMINATION << "ERROR (Trait::GetProperty): property baselineOffsetF can only be used in sexual species." << EidosTerminate();
			
			double value = p_value.FloatAtIndex_NOCAST(0, nullptr);
			
			if (!std::isfinite(value))
				EIDOS_TERMINATION << "ERROR (Trait::SetProperty): property baselineOffsetF requires a finite value (not NAN or INF)." << EidosTerminate();
			
			// effects for multiplicative traits clip at 0.0
			if ((type_ == TraitType::kMultiplicative) && (value < 0.0))
				EIDOS_TERMINATION << "ERROR (Trait::SetProperty): property baselineOffsetF may not be set to a negative value for a multiplicative trait." << EidosTerminate();
			
			slim_trait_offset_t new_baseline = (slim_trait_offset_t)value;
			
			// if the baseline offset is not actually changing, ignore the property set
			if (baselineOffset_F_ == new_baseline)
				return;
			
			baselineOffset_F_ = new_baseline;
			
			if (Type() == TraitType::kMultiplicative)
				compositeOffset_F_ = baselineOffset_F_ * substitutionOffset_F_;
			else
				compositeOffset_F_ = baselineOffset_F_ + substitutionOffset_F_;
			
			if (!std::isfinite(baselineOffset_F_) || !std::isfinite(compositeOffset_F_))
				EIDOS_TERMINATION << "ERROR (Trait::SetProperty): the new baselineOffsetF value could not be represented due to numerical issues (roundoff, overflow)." << EidosTerminate();
			
			// TRAIT INVALIDATION: the trait value for this trait is invalidated in all individuals
			InvalidateTraitValuesForAllIndividuals(IndividualSex::kFemale);
			
			return;
		}
		case gID_individualOffsetMean:
		{
			double value = p_value.FloatAtIndex_NOCAST(0, nullptr);
			
			if (!std::isfinite(value))
				EIDOS_TERMINATION << "ERROR (Trait::SetProperty): property individualOffsetMean requires a finite value (not NAN or INF)." << EidosTerminate();
			
			SetIndividualOffsetDistributionMean(value);
			return;
		}
		case gID_individualOffsetSD:
		{
			double value = p_value.FloatAtIndex_NOCAST(0, nullptr);
			
			if (!std::isfinite(value) || (value < 0.0))
				EIDOS_TERMINATION << "ERROR (Trait::SetProperty): property individualOffsetSD requires a nonnegative finite value (not NAN or INF)." << EidosTerminate();
			
			SetIndividualOffsetDistributionSD(value);
			return;
		}
		case gID_individualPhenotypePalette:
		{
			if (individual_phenotype_palette_) {
				individual_phenotype_palette_->Release();
				individual_phenotype_palette_ = nullptr;
			}
			
			EidosObject *obj = p_value.ObjectElementAtIndex_NOCAST(0, nullptr);
			
			individual_phenotype_palette_ = dynamic_cast<EidosPalette *>(obj);
			individual_phenotype_palette_->Retain();
			
			// FIXME: tell the community about the change so SLiMgui can redisplay
			//species_.community_.trait_changed_;
			return;
		}
		case gID_mutationEffectPalette:
		{
			if (mutation_effect_palette_) {
				mutation_effect_palette_->Release();
				mutation_effect_palette_ = nullptr;
			}
			
			EidosObject *obj = p_value.ObjectElementAtIndex_NOCAST(0, nullptr);
			
			mutation_effect_palette_ = dynamic_cast<EidosPalette *>(obj);
			mutation_effect_palette_->Retain();
			
			// FIXME: tell the community about the change so SLiMgui can redisplay
			//species_.community_.trait_changed_;
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
		
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_baselineOffsetH,						false,	kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_baselineOffsetM,						false,	kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_baselineOffsetF,						false,	kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_compositeOffsetH,						true,	kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_compositeOffsetM,						true,	kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_compositeOffsetF,						true,	kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_directFitnessEffect,					true,	kEidosValueMaskLogical | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_index,									true,	kEidosValueMaskInt | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_individualOffsetMean,					false,	kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_individualOffsetSD,						false,	kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_name,									true,	kEidosValueMaskString | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_individualPhenotypePalette,				false,	kEidosValueMaskObject | kEidosValueMaskSingleton, gEidosPalette_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_mutationEffectPalette,					false,	kEidosValueMaskObject | kEidosValueMaskSingleton, gEidosPalette_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_species,								true,	kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_Species_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_substitutionAccumulation,				true,	kEidosValueMaskLogical | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_substitutionOffsetH,					true,	kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_substitutionOffsetM,					true,	kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_substitutionOffsetF,					true,	kEidosValueMaskFloat | kEidosValueMaskSingleton)));
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






























