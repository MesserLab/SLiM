//
//  trait.h
//  SLiM
//
//  Created by Ben Haller on 6/25/25.
//  Copyright (c) 2025-2026 Benjamin C. Haller.  All rights reserved.
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

/*
 
 The class Trait represents a phenotypic trait.  More than one trait can be defined for a given species, and mutations can
 influence the value of more than one trait.  Traits can be multiplicative (typically a population genetics style of trait)
 or additive (typically a quantitative genetics style of trait).
 
 */

#ifndef __SLiM__trait__
#define __SLiM__trait__


class Community;
class Species;
class Substitution;

#include "eidos_globals.h"
#include "slim_globals.h"
#include "eidos_class_Dictionary.h"


class Trait_Class;
extern Trait_Class *gSLiM_Trait_Class;


class Trait : public EidosDictionaryRetained
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.
	
private:
	typedef EidosDictionaryRetained super;
	
private:
	// baseline offsets; these are user-provided and user-modifiable
	slim_trait_offset_t baselineOffset_H_;
	slim_trait_offset_t baselineOffset_M_;
	slim_trait_offset_t baselineOffset_F_;
	
	// substitution offsets; these accumulate the effects of mutations that fix and are substituted
	slim_trait_offset_t substitutionOffset_H_;
	slim_trait_offset_t substitutionOffset_M_;
	slim_trait_offset_t substitutionOffset_F_;
	
	// composite offsets; these are the sum of the baseline offset and the substitution offset
	// these are internal, not user-visible; they are just for a little more efficiency
	slim_trait_offset_t compositeOffset_H_;
	slim_trait_offset_t compositeOffset_M_;
	slim_trait_offset_t compositeOffset_F_;
	
#ifdef SLIMGUI
public:
#else
private:
#endif
	
	slim_trait_index_t index_;								// the index of this trait within its species
	std::string name_;										// the user-visible name of this trait
	
	// Trait type: at the user level, multiplicative / additive / logistic, but logistic is treated
	// internally here as additive with a logistic post-transformation controlled by a separate flag.
	TraitType type_;
	bool logistic_post_;
	
	// default individual offset distribution parameters, used to generate per-individual offsets
	double individualOffsetDistributionMean_;
	double individualOffsetDistributionSD_;
	
	// an optimization for the individual offset distribution, caching a fixed offset value if individualOffsetSD_
	// is 0.0; note that the cached fixed value here includes the exp() transform for multiplicative traits
	bool individualOffsetDistributionFixed_;						// true if individualOffsetSD_ == 0.0
	slim_trait_offset_t individualOffsetDistributionFixedValue_;	// pre-calculated and pre-cast for speed
	
	// this flag tracks whether every individual has an offset drawn from the trait's current offset distribution;
	// it starts false, and is set true if the script overrides an offset, or if the offset distribution changes
	bool individualOffsetEverOverridden_ = false;
	
	// if true, the calculated trait value is used directly as a fitness effect, automatically
	// this mimics the previous behavior of SLiM, for multiplicative traits
	bool directFitnessEffect_;
	
	// if true, mutation effects are combined into the substitution offset when the mutation is substituted
	bool substitutionAccumulation_;
    
    // palettes for coloring things in SLiMgui
    EidosPalette *individual_phenotype_palette_ = nullptr;		// OWNED POINTER: the palette used for coloring individuals in SLiMgui; retained
    EidosPalette *mutation_effect_palette_ = nullptr;           // OWNED POINTER: the palette used for coloring mutations in SLiMgui; retained
	
public:
	
	Community &community_;
	Species &species_;
	
	// a user-defined tag value
	slim_usertag_t tag_value_ = SLIM_TAG_UNSET_VALUE;
	
	// OPTIMIZATION FLAGS
	
	// This is set to false if any mutation has a non-neutral effect on this trait.
	mutable bool trait_all_neutral_mutations_ = true;
	
	// This is set to false if any mutation has a non-neutral effect on this trait that is not designated as
	// "independent dominance"; neutral effects don't matter.  This flag is "sticky" at false, permanently.
	mutable bool trait_all_mutations_independent_dominance_ = true;
	
	// Optimization flags set up by Species::PrepareForTraitCalculations() and valid only subsequent to that call.
	
	// If set, this flag indicates that the trait is currently completely neutral, including callbacks - either
	// because trait_all_neutral_mutations_ is set and the trait cannot be influenced by any callbacks in the
	// current subpopulation / tick, or because an active callback actually sets this trait to be neutral in
	// this subpopulation / tick.  Traits for which this flag is set can be safely elided from trait calculations
	// except for their baseline offset and individual offsets.
	mutable bool is_pure_neutral_now_;
	
	// If set, this flag indicates that the trait currently exhibits independent dominance for all mutations,
	// including the effects of callbacks - because trait_all_mutations_independent_dominance_ is set and the
	// trait cannot be influenced by ANY callbacks in the current subpopulation / tick.  The point is that the
	// intrinsic effects of all mutations in the non-neutral cache on the trait must be reliable.  (Even global-
	// neutral callbacks turn off this optimization, because independent-dominance cached values are calculated
	// based upon the mutations in the non-neutral cache, and even mutations made globally neutral for a given
	// trait may be kept in the non-neutral cache for other reasons, such as effects on other traits.)  Traits
	// for which this flag is set can be calculated more efficiently, with cached per-mutation-run values.  We
	// avoid setting this flag to true when is_pure_neutral_now_ is true; being pure-neutral takes precedence.
	mutable bool is_pure_independent_dominance_now_;
	
	// If set, subject_to_mutationEffect_callback_ indicates that the trait is influenced by a mutationEffect
	// callback in at least one subpop.  Traits with this flag set are subject to a callback, and so the effect
	// effect of mutations on that trait cannot be consulted reliably; the callback needs to be considered.
	mutable bool subject_to_mutationEffect_callback_;
	
	// If a trait is subject to a nonneutral callback, it needs to be evaluated even if a later constant-effect
	// callback overrides the nonneutral callback, because the nonneutral callback might have side effects.
	mutable bool subject_to_non_global_neutral_callback_;
	
	
	Trait(const Trait&) = delete;									// no copying
	Trait& operator=(const Trait&) = delete;						// no copying
	Trait(void) = delete;											// no null constructor
	
	explicit Trait(Species &p_species, const std::string &p_name, TraitType p_type, bool p_logistic_post, double p_individualOffsetDistributionMean, double p_individualOffsetDistributionSD, bool p_directFitnessEffect, bool p_substitutionAccumulation);
	void _FixDefaultTraitInit(void);
	~Trait(void);
	
	void InvalidateTraitValuesForAllIndividuals(void);
	
	inline __attribute__((always_inline)) slim_trait_index_t Index(void) const		{ return index_; }
	inline __attribute__((always_inline)) void SetIndex(slim_trait_index_t p_index)	{ index_ = p_index; }	// only from AddTrait()
	inline __attribute__((always_inline)) TraitType Type(void) const				{ return type_; }
	inline __attribute__((always_inline)) bool HasLogisticPostTransform(void) const	{ return logistic_post_; }
	inline __attribute__((always_inline)) const std::string &Name(void) const		{ return name_; }
	std::string UserVisibleType(void) const;
	
	// accessing offsets; most clients just want the composite offsets that are baseline + substitution,
	// but some need the components; those are considered semi-private here to de-emphasize them
	inline __attribute__((always_inline)) slim_trait_offset_t CompositeOffset_H(void) const { return compositeOffset_H_; };
	inline __attribute__((always_inline)) slim_trait_offset_t CompositeOffset_M(void) const { return compositeOffset_M_; };
	inline __attribute__((always_inline)) slim_trait_offset_t CompositeOffset_F(void) const { return compositeOffset_F_; };
	
	inline __attribute__((always_inline)) slim_trait_offset_t _BaselineOffset_H(void) const { return baselineOffset_H_; };
	inline __attribute__((always_inline)) slim_trait_offset_t _BaselineOffset_M(void) const { return baselineOffset_M_; };
	inline __attribute__((always_inline)) slim_trait_offset_t _BaselineOffset_F(void) const { return baselineOffset_F_; };
	
	inline __attribute__((always_inline)) slim_trait_offset_t _SubstitutionOffset_H(void) const { return substitutionOffset_H_; };
	inline __attribute__((always_inline)) slim_trait_offset_t _SubstitutionOffset_M(void) const { return substitutionOffset_M_; };
	inline __attribute__((always_inline)) slim_trait_offset_t _SubstitutionOffset_F(void) const { return substitutionOffset_F_; };
	
	// setting offsets; these methods correctly update the composite offsets when the component offsets change
	void _SetBaselineOffset_H(slim_trait_offset_t p_offset);
	void _SetBaselineOffset_M(slim_trait_offset_t p_offset);
	void _SetBaselineOffset_F(slim_trait_offset_t p_offset);
	
	void _ClearSubstitutionOffsets(void);
	void AccumulateSubstitutionOffset(const Substitution *p_substitution, const struct SubstitutionTraitInfo &p_trait_info);
	
	void _RecacheIndividualOffsetDistribution(void);		// caches individualOffsetDistributionFixed_ and individualOffsetDistributionFixedValue_
	slim_trait_offset_t _DrawIndividualOffset(void) const;	// draws from the distribution defined by individualOffsetDistributionMean_ and individualOffsetDistributionSD_
	inline __attribute__((always_inline)) slim_trait_offset_t DrawIndividualOffset(void) const { return (individualOffsetDistributionFixed_) ? individualOffsetDistributionFixedValue_ : _DrawIndividualOffset(); }
	inline __attribute__((always_inline)) slim_trait_offset_t IndividualOffsetDistributionMean(void) const { return individualOffsetDistributionMean_; }	// NOTE: this is the mean before the exp() post-transform
	inline __attribute__((always_inline)) slim_trait_offset_t IndividualOffsetDistributionSD(void) const { return individualOffsetDistributionSD_; }		// NOTE: this is the SD before the exp() post-transform
	inline __attribute__((always_inline)) void IndividualOffsetChanged(void) { individualOffsetEverOverridden_ = true; }
	inline __attribute__((always_inline)) bool IndividualOffsetEverChanged(void) { return individualOffsetEverOverridden_; }
	inline void SetIndividualOffsetDistributionMean(slim_trait_offset_t p_mean) {
		if (p_mean != individualOffsetDistributionMean_)
		{
			individualOffsetDistributionMean_ = p_mean;
			_RecacheIndividualOffsetDistribution();
			IndividualOffsetChanged();
		}
	}
	inline void SetIndividualOffsetDistributionSD(slim_trait_offset_t p_SD) {
		if (p_SD != individualOffsetDistributionSD_)
		{
			individualOffsetDistributionSD_ = p_SD;
			_RecacheIndividualOffsetDistribution();
			IndividualOffsetChanged();
		}
	}
	
	inline __attribute__((always_inline)) bool HasDirectFitnessEffect(void) const { return directFitnessEffect_; }
	inline __attribute__((always_inline)) bool SubstitutionAccumulationEnabled(void) const { return substitutionAccumulation_; }
	
#if DEBUG
	void CheckTraitIntegrity(void) const;
#endif
	
	//
	// Eidos support
	//
	virtual const EidosClass *Class(void) const override;
	virtual void Print(std::ostream &p_ostream) const override;
	
	virtual EidosValue_SP GetProperty(EidosGlobalStringID p_property_id) override;
	virtual void SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value) override;
	
	virtual EidosValue_SP ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) override;
};

class Trait_Class : public EidosDictionaryRetained_Class
{
private:
	typedef EidosDictionaryRetained_Class super;
	
public:
	Trait_Class(const Trait_Class &p_original) = delete;	// no copy-construct
	Trait_Class& operator=(const Trait_Class&) = delete;	// no copying
	inline Trait_Class(const std::string &p_class_name, EidosClass *p_superclass) : super(p_class_name, p_superclass) { }
	
	virtual std::vector<EidosPropertySignature_CSP> *Properties_MUTABLE(void) const override;	// use Properties() instead
	virtual const std::vector<EidosMethodSignature_CSP> *Methods(void) const override;
};


#endif /* defined(__SLiM__trait__) */






















