//
//  individual.h
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
#include "slim_eidos_dictionary.h"


class Subpopulation;

extern EidosObjectClass *gSLiM_Individual_Class;

// A global counter used to assign all Individual objects a unique ID
extern slim_mutationid_t gSLiM_next_pedigree_id;


class Individual : public SLiMEidosDictionary
{
	// This class has a restricted copying policy; see below
	
private:
	
	EidosValue_SP self_value_;			// cached EidosValue object for speed
	
	Subpopulation &subpopulation_;		// the subpop to which we refer; we get deleted when our subpop gets destructed
	slim_popsize_t index_;				// the individual index in that subpop (0-based, and not multiplied by 2)
	slim_usertag_t tag_value_;			// a user-defined tag value
	
	// Pedigree-tracking ivars.  These are -1 if unknown, otherwise assigned sequentially from 0 counting upward.  They
	// uniquely identify individuals within the simulation, so that relatedness of individuals can be assessed.  They can
	// be accessed through the read-only pedigree properties.  These are only maintained if sim->pedigrees_enabled_ is on.
	slim_mutationid_t pedigree_id_;		// the id of this individual
	slim_mutationid_t pedigree_p1_;		// the id of parent 1
	slim_mutationid_t pedigree_p2_;		// the id of parent 2
	slim_mutationid_t pedigree_g1_;		// the id of grandparent 1
	slim_mutationid_t pedigree_g2_;		// the id of grandparent 2
	slim_mutationid_t pedigree_g3_;		// the id of grandparent 3
	slim_mutationid_t pedigree_g4_;		// the id of grandparent 4
	
#ifdef DEBUG
	static bool s_log_copy_and_assign_;							// true if logging is disabled (see below)
#endif
	
public:
	
	//
	//	This class should not be copied, in general, but the default copy constructor cannot be entirely
	//	disabled, because we want to keep instances of this class inside STL containers.  We therefore
	//	override it to log whenever it is called, to reduce the risk of unintentional copying.
	//
	Individual(const Individual &p_original);
#ifdef DEBUG
	static bool LogIndividualCopyAndAssign(bool p_log);		// returns the old value; save and restore that value!
#endif
	
	Individual& operator= (const Individual &p_original) = delete;						// no copy construction
	Individual(void) = delete;															// no null construction
	Individual(Subpopulation &p_subpopulation, slim_popsize_t p_individual_index);		// construct with a subpop and an index
	~Individual(void);																	// destructor
	
	void GetGenomes(Genome **p_genome1, Genome **p_genome2) const;
	IndividualSex Sex(void) const;
	
	// This sets the receiver up as a new individual, with a newly assigned pedigree id, and gets
	// parental and grandparental information from the supplied parents.
	inline void TrackPedigreeWithParents(Individual &parent1, Individual &parent2)
	{
		pedigree_id_ = gSLiM_next_pedigree_id++;
		
		pedigree_p1_ = parent1.pedigree_id_;
		pedigree_p2_ = parent2.pedigree_id_;
		
		pedigree_g1_ = parent1.pedigree_p1_;
		pedigree_g2_ = parent1.pedigree_p2_;
		pedigree_g3_ = parent2.pedigree_p1_;
		pedigree_g4_ = parent2.pedigree_p2_;
	}
	
	double RelatednessToIndividual(Individual &ind);
	
	//
	// Eidos support
	//
	void GenerateCachedEidosValue(void);
	inline void ClearCachedEidosValue(void) { if (self_value_) self_value_.reset(); };
	inline EidosValue_SP CachedEidosValue(void) { if (!self_value_) GenerateCachedEidosValue(); return self_value_; };
	
	virtual const EidosObjectClass *Class(void) const;
	virtual void Print(std::ostream &p_ostream) const;
	
	virtual EidosValue_SP GetProperty(EidosGlobalStringID p_property_id);
	virtual void SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value);
	virtual EidosValue_SP ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	
	// Accelerated property access; see class EidosObjectElement for comments on this mechanism
	virtual int64_t GetProperty_Accelerated_Int(EidosGlobalStringID p_property_id);
	virtual EidosObjectElement *GetProperty_Accelerated_ObjectElement(EidosGlobalStringID p_property_id);
};


#endif /* defined(__SLiM__individual__) */













































