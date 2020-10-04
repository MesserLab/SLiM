//
//  individual.h
//  SLiM
//
//  Created by Ben Haller on 6/10/16.
//  Copyright (c) 2016-2020 Philipp Messer.  All rights reserved.
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
 
 The class Individual is a simple placeholder for individual simulated organisms.  It is not used by SLiM's core engine at all;
 it is provided solely for scripting convenience, as a bag containing the two genomes associated with an individual.  This
 makes it easy to sample a subpopulation's individuals, rather than its genomes; to determine whether individuals have a given
 mutation on either of their genomes; and other similar tasks.
 
 Individuals are kept by Subpopulation, and have the same lifetime as the Subpopulation to which they belong.  Since they do not
 actually contain any information specific to a particular individual – just an index in the Subpopulation's genomes vector –
 they do not get deallocated and reallocated between generations; the same object continues to represent individual #17 of the
 subpopulation for as long as that subpopulation exists.  This is safe because of the way that objects cannot live across code
 block boundaries in SLiM.  The tag values of particular Individual objects will persist between generations, even though the
 individual that is conceptually represented has changed, but that is fine since those values are officially undefined until set.
 
 */

#ifndef __SLiM__individual__
#define __SLiM__individual__


#include "genome.h"


class Subpopulation;

extern EidosObjectClass *gSLiM_Individual_Class;

// A global counter used to assign all Individual objects a unique ID
extern slim_pedigreeid_t gSLiM_next_pedigree_id;


class Individual : public EidosDictionary
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.
	
#ifdef SLIMGUI
public:
#else
private:
#endif
	
	EidosValue_SP self_value_;						// cached EidosValue object for speed
	
	Individual *patch_pointer_;						// used only by Subpopulation::ExecuteMethod_takeMigrants(); see that method
	
	std::string color_;								// color to use when displayed (in SLiMgui)
	float color_red_, color_green_, color_blue_;	// cached color components from color_; should always be in sync
	
	// Pedigree-tracking ivars.  These are -1 if unknown, otherwise assigned sequentially from 0 counting upward.  They
	// uniquely identify individuals within the simulation, so that relatedness of individuals can be assessed.  They can
	// be accessed through the read-only pedigree properties.  Genome pedigree IDs are also maintained in parallel; see genome.h.
	slim_pedigreeid_t pedigree_id_;		// the id of this individual
	slim_pedigreeid_t pedigree_p1_;		// the id of parent 1
	slim_pedigreeid_t pedigree_p2_;		// the id of parent 2
	slim_pedigreeid_t pedigree_g1_;		// the id of grandparent 1
	slim_pedigreeid_t pedigree_g2_;		// the id of grandparent 2
	slim_pedigreeid_t pedigree_g3_;		// the id of grandparent 3
	slim_pedigreeid_t pedigree_g4_;		// the id of grandparent 4
	
public:
	
	// BCH 6 April 2017: making these ivars public; lots of other classes want to access them, but writing
	// accessors for them seems excessively complicated / slow, and friending the whole class is too invasive.
	// Basically I think of the Individual class as just being a struct-like bag in some aspects.
	
	slim_usertag_t tag_value_;			// a user-defined tag value
	double tagF_value_;					// a user-defined tag value of float type
	
	double fitness_scaling_ = 1.0;		// the fitnessScaling property value
	double cached_fitness_UNSAFE_;		// the last calculated fitness value for this individual; NaN for new offspring, 1.0 for new subpops
										// this is marked UNSAFE because it can be overridden by a Subpopulation-level flag, which must be
										// checked before using this cached value (except in SLiMgui, where this value is always good)
	
	Genome *genome1_, *genome2_;		// NOT OWNED; must correspond to the entries in the Subpopulation we live in
	IndividualSex sex_;					// must correspond to our position in the Subpopulation vector we live in
	
#ifdef SLIM_NONWF_ONLY
	slim_age_t age_;					// the age of the individual, in generations; -1 in WF models
#endif  // SLIM_NONWF_ONLY
	
	slim_popsize_t index_;				// the individual index in that subpop (0-based, and not multiplied by 2)
	Subpopulation &subpopulation_;		// the subpop to which we refer; we get deleted when our subpop gets destructed
	eidos_logical_t migrant_;			// T if the individual has migrated in the current generation, F otherwise
	
	// Continuous space ivars.  These are effectively free tag values of type float, unless they are used by interactions.
	double spatial_x_, spatial_y_, spatial_z_;
	
	// ********** BEWARE BEWARE BEWARE BEWARE BEWARE BEWARE BEWARE BEWARE BEWARE BEWARE BEWARE BEWARE BEWARE BEWARE **********
	//
	// New ivars added above need to be handled in some way by Subpopulation::ExecuteMethod_takeMigrants(), which will
	// need to transfer the values over to the new Individual object when migration of the original individual occurs!
	
	//
	//	This class should not be copied, in general, but the default copy constructor cannot be entirely
	//	disabled, because we want to keep instances of this class inside STL containers.  We therefore
	//	override it to log whenever it is called, to reduce the risk of unintentional copying.
	//
	Individual(const Individual &p_original) = delete;
	Individual& operator= (const Individual &p_original) = delete;						// no copy construction
	Individual(void) = delete;															// no null construction
	Individual(Subpopulation &p_subpopulation, slim_popsize_t p_individual_index, slim_pedigreeid_t p_pedigree_id, Genome *p_genome1, Genome *p_genome2, IndividualSex p_sex, slim_age_t p_age, double p_fitness);
	inline virtual ~Individual(void) override { }
	
	inline __attribute__((always_inline)) void ClearColor(void) { color_.clear(); }
	
	inline __attribute__((always_inline)) double TagFloat(void) { return tagF_value_; }
	
	// This sets the receiver up as a new individual, with a newly assigned pedigree id, and gets
	// parental and grandparental information from the supplied parents.
	inline __attribute__((always_inline)) void TrackPedigreeWithParents(Individual &p_parent1, Individual &p_parent2)
	{
		pedigree_id_ = gSLiM_next_pedigree_id++;
		
		genome1_->genome_id_ = pedigree_id_ * 2;
		genome2_->genome_id_ = pedigree_id_ * 2 + 1;
		
		pedigree_p1_ = p_parent1.pedigree_id_;
		pedigree_p2_ = p_parent2.pedigree_id_;
		
		pedigree_g1_ = p_parent1.pedigree_p1_;
		pedigree_g2_ = p_parent1.pedigree_p2_;
		pedigree_g3_ = p_parent2.pedigree_p1_;
		pedigree_g4_ = p_parent2.pedigree_p2_;
	}
	
	// This alternative to TrackPedigreeWithParents() is used when the parents are not known, as in
	// addEmpty() and addRecombined(); the unset ivars are set to -1 by the Individual constructor
	inline __attribute__((always_inline)) void TrackPedigreeWithoutParents()
	{
		pedigree_id_ = gSLiM_next_pedigree_id++;
		
		genome1_->genome_id_ = pedigree_id_ * 2;
		genome2_->genome_id_ = pedigree_id_ * 2 + 1;
	}
	
	double RelatednessToIndividual(Individual &p_ind);
	
	inline __attribute__((always_inline)) slim_pedigreeid_t PedigreeID()			{ return pedigree_id_; }
	inline __attribute__((always_inline)) void SetPedigreeID(slim_pedigreeid_t p_new_id)		{ pedigree_id_ = p_new_id; }	// should basically never be called
	inline __attribute__((always_inline)) slim_pedigreeid_t Parent1PedigreeID()		{ return pedigree_p1_; }
	inline __attribute__((always_inline)) slim_pedigreeid_t Parent2PedigreeID()		{ return pedigree_p2_; }
	
	//
	// Eidos support
	//
	void GenerateCachedEidosValue(void);
	inline __attribute__((always_inline)) EidosValue_SP CachedEidosValue(void) { if (!self_value_) GenerateCachedEidosValue(); return self_value_; };
	
	virtual const EidosObjectClass *Class(void) const override;
	virtual void Print(std::ostream &p_ostream) const override;
	
	virtual EidosValue_SP GetProperty(EidosGlobalStringID p_property_id) override;
	virtual void SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value) override;
	
	virtual EidosValue_SP ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) override;
	EidosValue_SP ExecuteMethod_containsMutations(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_countOfMutationsOfType(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_relatedness(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	static EidosValue_SP ExecuteMethod_Accelerated_sumOfMutationsOfType(EidosObjectElement **p_values, size_t p_values_size, EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_uniqueMutationsOfType(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	
	// Accelerated property access; see class EidosObjectElement for comments on this mechanism
	static EidosValue *GetProperty_Accelerated_index(EidosObjectElement **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_pedigreeID(EidosObjectElement **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_tag(EidosObjectElement **p_values, size_t p_values_size);
#ifdef SLIM_NONWF_ONLY
	static EidosValue *GetProperty_Accelerated_age(EidosObjectElement **p_values, size_t p_values_size);
#endif  // SLIM_NONWF_ONLY
	static EidosValue *GetProperty_Accelerated_tagF(EidosObjectElement **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_migrant(EidosObjectElement **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_fitnessScaling(EidosObjectElement **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_x(EidosObjectElement **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_y(EidosObjectElement **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_z(EidosObjectElement **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_subpopulation(EidosObjectElement **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_genome1(EidosObjectElement **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_genome2(EidosObjectElement **p_values, size_t p_values_size);
	
	// Accelerated property writing; see class EidosObjectElement for comments on this mechanism
	static void SetProperty_Accelerated_tag(EidosObjectElement **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size);
	static void SetProperty_Accelerated_tagF(EidosObjectElement **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size);
	static void SetProperty_Accelerated_fitnessScaling(EidosObjectElement **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size);
	static void SetProperty_Accelerated_x(EidosObjectElement **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size);
	static void SetProperty_Accelerated_y(EidosObjectElement **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size);
	static void SetProperty_Accelerated_z(EidosObjectElement **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size);
	static void SetProperty_Accelerated_age(EidosObjectElement **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size);
	static void SetProperty_Accelerated_color(EidosObjectElement **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size);
	
	// These flags are used to minimize the work done by Subpopulation::SwapChildAndParentGenomes(); it only needs to
	// reset colors or dictionaries if they have ever been touched by the model.  These flags are set and never cleared.
	static bool s_any_individual_color_set_;
	static bool s_any_individual_dictionary_set_;
	static bool s_any_individual_or_genome_tag_set_;
	static bool s_any_individual_fitness_scaling_set_;
	
	// for Subpopulation::ExecuteMethod_takeMigrants()
	friend Subpopulation;
};


#endif /* defined(__SLiM__individual__) */













































