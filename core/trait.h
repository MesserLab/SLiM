//
//  trait.h
//  SLiM
//
//  Created by Ben Haller on 6/25/25.
//  Copyright (c) 2025 Benjamin C. Haller.  All rights reserved.
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
	
#ifdef SLIMGUI
public:
#else
private:
#endif
	
	int64_t index_;											// the index of this trait within its species
	std::string name_;										// the user-visible name of this trait
	TraitType type_;										// multiplicative or additive
	
	// offsets
	slim_effect_t baselineOffset_;
	
	bool individualOffsetFixed_;							// true if individualOffsetSD_ == 0.0
	slim_effect_t individualOffsetFixedValue_;				// equal to individualOffsetMean_ if individualOffsetFixed_ == true; pre-cast for speed
	double individualOffsetMean_;
	double individualOffsetSD_;
	
	// if true, the calculated trait value is used directly as a fitness effect, automatically
	// this mimics the previous behavior of SLiM, for multiplicative traits
	bool directFitnessEffect_;
	
public:
	
	Community &community_;
	Species &species_;
	
	// a user-defined tag value
	slim_usertag_t tag_value_ = SLIM_TAG_UNSET_VALUE;
	
	Trait(const Trait&) = delete;									// no copying
	Trait& operator=(const Trait&) = delete;						// no copying
	Trait(void) = delete;											// no null constructor
	
	explicit Trait(Species &p_species, const std::string &p_name, TraitType p_type, slim_effect_t p_baselineOffset, double p_individualOffsetMean, double p_individualOffsetSD, bool directFitnessEffect);
	~Trait(void);
	
	inline __attribute__((always_inline)) int64_t Index(void) const				{ return index_; }
	inline __attribute__((always_inline)) void SetIndex(int64_t p_index)		{ index_ = p_index; }	// only from AddTrait()
	inline __attribute__((always_inline)) TraitType Type(void) const			{ return type_; }
	inline __attribute__((always_inline)) const std::string &Name(void) const	{ return name_; }
	
	slim_effect_t BaselineOffset(void) const { return baselineOffset_; };
	
	void _RecacheIndividualOffsetDistribution(void);		// caches individualOffsetFixed_ and individualOffsetFixedValue_
	slim_effect_t _DrawIndividualOffset(void) const;		// draws from a normal distribution defined by individualOffsetMean_ and individualOffsetSD_
	inline __attribute__((always_inline)) slim_effect_t DrawIndividualOffset(void) const { return (individualOffsetFixed_) ? individualOffsetFixedValue_ : _DrawIndividualOffset(); }
	
	inline __attribute__((always_inline)) bool HasDirectFitnessEffect(void) const { return directFitnessEffect_; }
	
	
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
	
	virtual const std::vector<EidosPropertySignature_CSP> *Properties(void) const override;
	virtual const std::vector<EidosMethodSignature_CSP> *Methods(void) const override;
};


#endif /* defined(__SLiM__trait__) */






















