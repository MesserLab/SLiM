//
//  individual.h
//  SLiM
//
//  Created by Ben Haller on 6/10/16.
//  Copyright (c) 2016-2023 Philipp Messer.  All rights reserved.
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
 they do not get deallocated and reallocated between cycles; the same object continues to represent individual #17 of the
 subpopulation for as long as that subpopulation exists.  This is safe because of the way that objects cannot live across code
 block boundaries in SLiM.  The tag values of particular Individual objects will persist between cycles, even though the
 individual that is conceptually represented has changed, but that is fine since those values are officially undefined until set.
 
 */

#ifndef __SLiM__individual__
#define __SLiM__individual__


#include "genome.h"


class Subpopulation;

extern EidosClass *gSLiM_Individual_Class;

// A global counter used to assign all Individual objects a unique ID.  Note this is shared by all species.
extern slim_pedigreeid_t gSLiM_next_pedigree_id;			// use SLiM_GetNextPedigreeID() instead, for THREAD_SAFETY_IN_ACTIVE_PARALLEL()

inline slim_pedigreeid_t SLiM_GetNextPedigreeID(void)
{
	THREAD_SAFETY_IN_ACTIVE_PARALLEL("SLiM_GetNextPedigreeID(): gSLiM_next_pedigree_id change");
	return gSLiM_next_pedigree_id++;
}

inline slim_pedigreeid_t SLiM_GetNextPedigreeID_Block(int p_block_size)
{
	THREAD_SAFETY_IN_ACTIVE_PARALLEL("SLiM_GetNextPedigreeID_Block(): gSLiM_next_pedigree_id change");
	slim_pedigreeid_t block_base = gSLiM_next_pedigree_id;
	
	gSLiM_next_pedigree_id += p_block_size;
	
	return block_base;
}

class Individual : public EidosDictionaryUnretained
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.
	
private:
	typedef EidosDictionaryUnretained super;

#ifdef SLIMGUI
public:
#else
private:
#endif
	
	EidosValue_SP self_value_;						// cached EidosValue object for speed
	
	uint8_t color_set_;								// set to true if the color for the individual has been set
	uint8_t colorR_, colorG_, colorB_;				// cached color components from the color property
	
	// Pedigree-tracking ivars.  These are -1 if unknown, otherwise assigned sequentially from 0 counting upward.  They
	// uniquely identify individuals within the simulation, so that relatedness of individuals can be assessed.  They can
	// be accessed through the read-only pedigree properties.  These are only maintained if sim->pedigrees_enabled_ is on.
	// If these are maintained, genome pedigree IDs are also maintained in parallel; see genome.h.
	float mean_parent_age_;				// the mean age of this individual's parents; 0 if parentless, -1 in WF models
	slim_pedigreeid_t pedigree_id_;		// the id of this individual
	slim_pedigreeid_t pedigree_p1_;		// the id of parent 1
	slim_pedigreeid_t pedigree_p2_;		// the id of parent 2
	slim_pedigreeid_t pedigree_g1_;		// the id of grandparent 1
	slim_pedigreeid_t pedigree_g2_;		// the id of grandparent 2
	slim_pedigreeid_t pedigree_g3_;		// the id of grandparent 3
	slim_pedigreeid_t pedigree_g4_;		// the id of grandparent 4
	int32_t reproductive_output_;		// the number of offspring for which this individual has been a parent, so far
	
public:
	
	// BCH 6 April 2017: making these ivars public; lots of other classes want to access them, but writing
	// accessors for them seems excessively complicated / slow, and friending the whole class is too invasive.
	// Basically I think of the Individual class as just being a struct-like bag in some aspects.
	
	uint8_t scratch_;					// available for use by algorithms
	IndividualSex sex_;					// must correspond to our position in the Subpopulation vector we live in
	unsigned int migrant_ : 1;			// T if the individual has migrated in the current cycle, F otherwise
	unsigned int killed_ : 1;			// T if the individual has been killed by killIndividuals(), F otherwise
	
	// note there are 4 bits free here for other logical flags
	
	unsigned tagL0_set_ : 1;			// T if tagL0 has been set by the user
	unsigned tagL0_value_ : 1;			// a user-defined tag value of logical type
	unsigned tagL1_set_ : 1;			// T if tagL1 has been set by the user
	unsigned tagL1_value_ : 1;			// a user-defined tag value of logical type
	unsigned tagL2_set_ : 1;			// T if tagL2 has been set by the user
	unsigned tagL2_value_ : 1;			// a user-defined tag value of logical type
	unsigned tagL3_set_ : 1;			// T if tagL3 has been set by the user
	unsigned tagL3_value_ : 1;			// a user-defined tag value of logical type
	unsigned tagL4_set_ : 1;			// T if tagL4 has been set by the user
	unsigned tagL4_value_ : 1;			// a user-defined tag value of logical type
	
	slim_usertag_t tag_value_;			// a user-defined tag value of integer type
	double tagF_value_;					// a user-defined tag value of float type
	
	double fitness_scaling_ = 1.0;		// the fitnessScaling property value
	double cached_fitness_UNSAFE_;		// the last calculated fitness value for this individual; NaN for new offspring, 1.0 for new subpops
										// this is marked UNSAFE because Subpopulation's individual_cached_fitness_OVERRIDE_ flag can override
										// this value in neutral models; that flag must be checked before using this cached value
#ifdef SLIMGUI
	double cached_unscaled_fitness_;	// the last calculated fitness value for this individual, WITHOUT subpop fitnessScaling; used only in
										// in SLiMgui, which wants to exclude that scaling because it usually represents density-dependence
										// that confuses interpretation; note that individual_cached_fitness_OVERRIDE_ is not relevant to this
#endif
	
	Genome *genome1_, *genome2_;		// NOT OWNED; must correspond to the entries in the Subpopulation we live in
	slim_age_t age_;					// nonWF only: the age of the individual, in cycles; -1 in WF models
	
	slim_popsize_t index_;				// the individual index in that subpop (0-based, and not multiplied by 2)
	Subpopulation *subpopulation_;		// the subpop to which we belong; cannot be a reference because it changes on migration!
	
	// Continuous space ivars.  These are effectively free tag values of type float, unless they are used by interactions.
	double spatial_x_, spatial_y_, spatial_z_;
	
	//
	//	This class should not be copied, in general, but the default copy constructor cannot be entirely
	//	disabled, because we want to keep instances of this class inside STL containers.  We therefore
	//	override it to log whenever it is called, to reduce the risk of unintentional copying.
	//
	Individual(const Individual &p_original) = delete;
	Individual& operator= (const Individual &p_original) = delete;						// no copy construction
	Individual(void) = delete;															// no null construction
	Individual(Subpopulation *p_subpopulation, slim_popsize_t p_individual_index, Genome *p_genome1, Genome *p_genome2, IndividualSex p_sex, slim_age_t p_age, double p_fitness, float p_mean_parent_age);
	inline virtual ~Individual(void) override { }
	
	inline __attribute__((always_inline)) void ClearColor(void) { color_set_ = false; }
	
	// This sets the receiver up as a new individual, with a newly assigned pedigree id, and gets
	// parental and grandparental information from the supplied parents.
	inline __attribute__((always_inline)) void TrackParentage_Biparental(slim_pedigreeid_t p_pedigree_id, Individual &p_parent1, Individual &p_parent2)
	{
		pedigree_id_ = p_pedigree_id;
		
		genome1_->genome_id_ = p_pedigree_id * 2;
		genome2_->genome_id_ = p_pedigree_id * 2 + 1;
		
		pedigree_p1_ = p_parent1.pedigree_id_;
		pedigree_p2_ = p_parent2.pedigree_id_;
		
		pedigree_g1_ = p_parent1.pedigree_p1_;
		pedigree_g2_ = p_parent1.pedigree_p2_;
		pedigree_g3_ = p_parent2.pedigree_p1_;
		pedigree_g4_ = p_parent2.pedigree_p2_;
		
#pragma omp critical (ReproductiveOutput)
		{
			p_parent1.reproductive_output_++;
			p_parent2.reproductive_output_++;
		}
	}
	
	inline __attribute__((always_inline)) void RevokeParentage_Biparental(Individual &p_parent1, Individual &p_parent2)
	{
		// note this does not need to be in #pragma omp critical (ReproductiveOutput) because it never gets hit when parallel
		// that is because it only happens when modifyChild() rejects a child, and that does not happen when parallel
		p_parent1.reproductive_output_--;
		p_parent2.reproductive_output_--;
	}
	
	inline __attribute__((always_inline)) void TrackParentage_Uniparental(slim_pedigreeid_t p_pedigree_id, Individual &p_parent)
	{
		pedigree_id_ = p_pedigree_id;
		
		genome1_->genome_id_ = p_pedigree_id * 2;
		genome2_->genome_id_ = p_pedigree_id * 2 + 1;
		
		pedigree_p1_ = p_parent.pedigree_id_;
		pedigree_p2_ = p_parent.pedigree_id_;
		
		pedigree_g1_ = p_parent.pedigree_p1_;
		pedigree_g2_ = p_parent.pedigree_p2_;
		pedigree_g3_ = p_parent.pedigree_p1_;
		pedigree_g4_ = p_parent.pedigree_p2_;
		
#pragma omp critical (ReproductiveOutput)
		{
			p_parent.reproductive_output_ += 2;
		}
	}
	
	inline __attribute__((always_inline)) void RevokeParentage_Uniparental(Individual &p_parent)
	{
		// note this does not need to be in #pragma omp critical (ReproductiveOutput) because it never gets hit when parallel
		// that is because it only happens when modifyChild() rejects a child, and that does not happen when parallel
		p_parent.reproductive_output_ -= 2;
	}
	
	// This alternative to TrackParentage() is used when the parents are not known, as in
	// addEmpty() and addRecombined(); the unset ivars are set to -1 by the Individual constructor
	inline __attribute__((always_inline)) void TrackParentage_Parentless(slim_pedigreeid_t p_pedigree_id)
	{
		pedigree_id_ = p_pedigree_id;
		
		genome1_->genome_id_ = p_pedigree_id * 2;
		genome2_->genome_id_ = p_pedigree_id * 2 + 1;
	}
	
	inline __attribute__((always_inline)) void RevokeParentage_Parentless()
	{
		// just for parallel design, no parentage to revoke
	}
	
	// Relatedness using pedigree data.  Most clients will use RelatednessToIndividual() and SharedParentCountWithIndividual;
	// _Relatedness() and _SharedParentCount() are internal API made public for unit testing.
	double RelatednessToIndividual(Individual &p_ind);
	static double _Relatedness(slim_pedigreeid_t A, slim_pedigreeid_t A_P1, slim_pedigreeid_t A_P2, slim_pedigreeid_t A_G1, slim_pedigreeid_t A_G2, slim_pedigreeid_t A_G3, slim_pedigreeid_t A_G4,
							   slim_pedigreeid_t B, slim_pedigreeid_t B_P1, slim_pedigreeid_t B_P2, slim_pedigreeid_t B_G1, slim_pedigreeid_t B_G2, slim_pedigreeid_t B_G3, slim_pedigreeid_t B_G4,
							   IndividualSex A_sex, IndividualSex B_sex, GenomeType modeledChromosomeType);
	
	int SharedParentCountWithIndividual(Individual &p_ind);
	static int _SharedParentCount(slim_pedigreeid_t X_P1, slim_pedigreeid_t X_P2, slim_pedigreeid_t Y_P1, slim_pedigreeid_t Y_P2);
	
	inline __attribute__((always_inline)) slim_pedigreeid_t PedigreeID()			{ return pedigree_id_; }
	inline __attribute__((always_inline)) void SetPedigreeID(slim_pedigreeid_t p_new_id)		{ pedigree_id_ = p_new_id; }	// should basically never be called
	inline __attribute__((always_inline)) slim_pedigreeid_t Parent1PedigreeID()		{ return pedigree_p1_; }
	inline __attribute__((always_inline)) slim_pedigreeid_t Parent2PedigreeID()		{ return pedigree_p2_; }
	inline __attribute__((always_inline)) void SetParentPedigreeID(slim_pedigreeid_t p1_new_id, slim_pedigreeid_t p2_new_id)		{ pedigree_p1_ = p1_new_id; pedigree_p2_ = p2_new_id; }	// also?
	inline __attribute__((always_inline)) int32_t ReproductiveOutput()				{ return reproductive_output_; }
	
	// Spatial position inheritance from a parent; should be called in every code path that generates an offspring from a parent
	inline __attribute__((always_inline)) void InheritSpatialPosition(int p_dimensionality, Individual *p_parent) {
		if (p_dimensionality > 0)
		{
			switch (p_dimensionality)
			{
				case 1:
					spatial_x_ = p_parent->spatial_x_;
					break;
				case 2:
					spatial_x_ = p_parent->spatial_x_;
					spatial_y_ = p_parent->spatial_y_;
					break;
				case 3:
					spatial_x_ = p_parent->spatial_x_;
					spatial_y_ = p_parent->spatial_y_;
					spatial_z_ = p_parent->spatial_z_;
					break;
			}
		}
	}
	
	//
	// Eidos support
	//
	void GenerateCachedEidosValue(void);
	inline __attribute__((always_inline)) EidosValue_SP CachedEidosValue(void) { if (!self_value_) GenerateCachedEidosValue(); return self_value_; };
	
	virtual const EidosClass *Class(void) const override;
	virtual void Print(std::ostream &p_ostream) const override;
	
	virtual EidosValue_SP GetProperty(EidosGlobalStringID p_property_id) override;
	virtual void SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value) override;
	
	virtual EidosValue_SP ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) override;
	EidosValue_SP ExecuteMethod_containsMutations(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	static EidosValue_SP ExecuteMethod_Accelerated_countOfMutationsOfType(EidosObject **p_values, size_t p_values_size, EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_relatedness(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_sharedParentCount(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	static EidosValue_SP ExecuteMethod_Accelerated_sumOfMutationsOfType(EidosObject **p_values, size_t p_values_size, EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_uniqueMutationsOfType(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	
	// Accelerated property access; see class EidosObject for comments on this mechanism
	static EidosValue *GetProperty_Accelerated_index(EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_pedigreeID(EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_tagL0(EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_tagL1(EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_tagL2(EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_tagL3(EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_tagL4(EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_tag(EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_age(EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_reproductiveOutput(EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_tagF(EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_migrant(EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_fitnessScaling(EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_x(EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_y(EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_z(EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_spatialPosition(EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_subpopulation(EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_genome1(EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_genome2(EidosObject **p_values, size_t p_values_size);
	
	// Accelerated property writing; see class EidosObject for comments on this mechanism
	static void SetProperty_Accelerated_tag(EidosObject **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size);
	static void SetProperty_Accelerated_tagF(EidosObject **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size);
	static void SetProperty_Accelerated_tagL0(EidosObject **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size);
	static void SetProperty_Accelerated_tagL1(EidosObject **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size);
	static void SetProperty_Accelerated_tagL2(EidosObject **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size);
	static void SetProperty_Accelerated_tagL3(EidosObject **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size);
	static void SetProperty_Accelerated_tagL4(EidosObject **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size);
	static bool _SetFitnessScaling_1(double source_value, EidosObject **p_values, size_t p_values_size);
	static bool _SetFitnessScaling_N(const double *source_data, EidosObject **p_values, size_t p_values_size);
	static void SetProperty_Accelerated_fitnessScaling(EidosObject **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size);
	static void SetProperty_Accelerated_x(EidosObject **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size);
	static void SetProperty_Accelerated_y(EidosObject **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size);
	static void SetProperty_Accelerated_z(EidosObject **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size);
	static void SetProperty_Accelerated_age(EidosObject **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size);
	static void SetProperty_Accelerated_color(EidosObject **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size);
	
	// These flags are used to minimize the work done by Subpopulation::SwapChildAndParentGenomes(); it only needs to
	// reset colors or dictionaries if they have ever been touched by the model.  These flags are set and never cleared.
	// BCH 5/24/2022: Note that these globals are shared across species, so if one species uses a given facility, all
	// species will suffer the associated speed penalty for it.  This is a bit unfortunate, but keeps the design simple.
	static bool s_any_individual_color_set_;
	static bool s_any_individual_dictionary_set_;
	static bool s_any_individual_tag_set_;
	static bool s_any_individual_tagF_set_;
	static bool s_any_individual_tagL_set_;
	static bool s_any_genome_tag_set_;
	static bool s_any_individual_fitness_scaling_set_;
	
	// for Subpopulation::ExecuteMethod_takeMigrants()
	friend Subpopulation;
};

class Individual_Class : public EidosDictionaryUnretained_Class
{
private:
	typedef EidosDictionaryUnretained_Class super;

public:
	Individual_Class(const Individual_Class &p_original) = delete;	// no copy-construct
	Individual_Class& operator=(const Individual_Class&) = delete;	// no copying
	inline Individual_Class(const std::string &p_class_name, EidosClass *p_superclass) : super(p_class_name, p_superclass) { }
	
	virtual const std::vector<EidosPropertySignature_CSP> *Properties(void) const override;
	virtual const std::vector<EidosMethodSignature_CSP> *Methods(void) const override;
	
	virtual EidosValue_SP ExecuteClassMethod(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) const override;
	EidosValue_SP ExecuteMethod_setSpatialPosition(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) const;
};


#endif /* defined(__SLiM__individual__) */













































