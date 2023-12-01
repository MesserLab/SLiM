//
//  individual.cpp
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


#include "individual.h"
#include "subpopulation.h"
#include "species.h"
#include "community.h"
#include "eidos_property_signature.h"
#include "eidos_call_signature.h"

#include <string>
#include <algorithm>
#include <vector>
#include <cmath>


#pragma mark -
#pragma mark Individual
#pragma mark -

// A global counter used to assign all Individual objects a unique ID
slim_pedigreeid_t gSLiM_next_pedigree_id = 0;

// Static member bools that track whether any individual has ever sustained a particular type of change
bool Individual::s_any_individual_color_set_ = false;
bool Individual::s_any_individual_dictionary_set_ = false;
bool Individual::s_any_individual_tag_set_ = false;
bool Individual::s_any_individual_tagF_set_ = false;
bool Individual::s_any_individual_tagL_set_ = false;
bool Individual::s_any_genome_tag_set_ = false;
bool Individual::s_any_individual_fitness_scaling_set_ = false;


Individual::Individual(Subpopulation *p_subpopulation, slim_popsize_t p_individual_index, Genome *p_genome1, Genome *p_genome2, IndividualSex p_sex, slim_age_t p_age, double p_fitness, float p_mean_parent_age) :
	color_set_(false), mean_parent_age_(p_mean_parent_age), pedigree_id_(-1), pedigree_p1_(-1), pedigree_p2_(-1),
	pedigree_g1_(-1), pedigree_g2_(-1), pedigree_g3_(-1), pedigree_g4_(-1), reproductive_output_(0),
	sex_(p_sex), migrant_(false), killed_(false), cached_fitness_UNSAFE_(p_fitness),
#ifdef SLIMGUI
	cached_unscaled_fitness_(p_fitness),
#endif
	genome1_(p_genome1), genome2_(p_genome2), age_(p_age), index_(p_individual_index), subpopulation_(p_subpopulation)
{
#if DEBUG
	if (!p_genome1 || !p_genome2)
		EIDOS_TERMINATION << "ERROR (Individual::Individual): (internal error) nullptr passed for genome." << EidosTerminate();
#endif
	
	// Set up the pointers from our genomes to us
	p_genome1->individual_ = this;
	p_genome2->individual_ = this;
	
	// Initialize tag values to the "unset" value
	tag_value_ = SLIM_TAG_UNSET_VALUE;
	tagF_value_ = SLIM_TAGF_UNSET_VALUE;
	tagL0_set_ = false;
	tagL1_set_ = false;
	tagL2_set_ = false;
	tagL3_set_ = false;
	tagL4_set_ = false;
	
	p_genome1->tag_value_ = SLIM_TAG_UNSET_VALUE;
	p_genome1->tag_value_ = SLIM_TAG_UNSET_VALUE;
    
    // Initialize x/y/z to 0.0, only when leak-checking (they show up as used before initialized in Valgrind)
#if SLIM_LEAK_CHECKING
    spatial_x_ = 0.0;
    spatial_y_ = 0.0;
    spatial_z_ = 0.0;
#endif
}

static inline bool _InPedigree(slim_pedigreeid_t A, slim_pedigreeid_t A_P1, slim_pedigreeid_t A_P2, slim_pedigreeid_t A_G1, slim_pedigreeid_t A_G2, slim_pedigreeid_t A_G3, slim_pedigreeid_t A_G4, slim_pedigreeid_t B)
{
	if (B == -1)
		return false;
	
	if ((A == B) || (A_P1 == B) || (A_P2 == B) || (A_G1 == B) || (A_G2 == B) || (A_G3 == B) || (A_G4 == B))
		return true;
	
	return false;
}

static double _Relatedness(slim_pedigreeid_t A, slim_pedigreeid_t A_P1, slim_pedigreeid_t A_P2, slim_pedigreeid_t A_G1, slim_pedigreeid_t A_G2, slim_pedigreeid_t A_G3, slim_pedigreeid_t A_G4,
						   slim_pedigreeid_t B, slim_pedigreeid_t B_P1, slim_pedigreeid_t B_P2, slim_pedigreeid_t B_G1, slim_pedigreeid_t B_G2, slim_pedigreeid_t B_G3, slim_pedigreeid_t B_G4)
{
	if ((A == -1) || (B == -1))
	{
		// Unknown pedigree IDs do not match anybody
		return 0.0;
	}
	else if (A == B)
	{
		// An individual matches itself with relatedness 1.0
		return 1.0;
	}
	else {
		double out = 0.0;
		
		if (_InPedigree(B, B_P1, B_P2, B_G1, B_G2, B_G3, B_G4, A))		// if A is in B...
		{
			out += _Relatedness(A, A_P1, A_P2, A_G1, A_G2, A_G3, A_G4, B_P1, B_G1, B_G2, -1, -1, -1, -1) / 2.0;
			out += _Relatedness(A, A_P1, A_P2, A_G1, A_G2, A_G3, A_G4, B_P2, B_G3, B_G4, -1, -1, -1, -1) / 2.0;
		}
		else
		{
			out += _Relatedness(A_P1, A_G1, A_G2, -1, -1, -1, -1, B, B_P1, B_P2, B_G1, B_G2, B_G3, B_G4) / 2.0;
			out += _Relatedness(A_P2, A_G3, A_G4, -1, -1, -1, -1, B, B_P1, B_P2, B_G1, B_G2, B_G3, B_G4) / 2.0;
		}
		
		return out;
	}
}

double Individual::_Relatedness(slim_pedigreeid_t A, slim_pedigreeid_t A_P1, slim_pedigreeid_t A_P2, slim_pedigreeid_t A_G1, slim_pedigreeid_t A_G2, slim_pedigreeid_t A_G3, slim_pedigreeid_t A_G4,
								slim_pedigreeid_t B, slim_pedigreeid_t B_P1, slim_pedigreeid_t B_P2, slim_pedigreeid_t B_G1, slim_pedigreeid_t B_G2, slim_pedigreeid_t B_G3, slim_pedigreeid_t B_G4,
								IndividualSex A_sex, IndividualSex B_sex, GenomeType modeledChromosomeType)
{
	// This version of _Relatedness() corrects for the sex chromosome case.  It should be regarded as the top-level internal API here.
	// This is separate from RelatednessToIndividual(), and implemented as a static member function, for unit testing; we want an
	// API that unit tests can call without needing to actually have a constructed Individual object.
	
	// Correct for sex-chromosome simulations; the only individuals that count are those that pass on the sex chromosome to the
	// child.  We can do that here since we know that the first parent of a given individual is female and the second is male.
	// If individuals are cloning, then both parents will be the same sex as the offspring, in fact, but we still want to
	// treat it the same I think (?).  For example, a male offspring from biparental mating inherits an X from its female
	// parent only; a male offspring from cloning still inherits only one sex chromosome from its parent, so the same correction
	// seems appropriate still.
	
#if DEBUG
	if ((modeledChromosomeType != GenomeType::kAutosome) && ((A_sex == IndividualSex::kHermaphrodite) || (B_sex == IndividualSex::kHermaphrodite)))
		EIDOS_TERMINATION << "ERROR (Individual::_Relatedness): (internal error) hermaphrodites cannot exist when modeling a sex chromosome" << EidosTerminate();
	if (((A_sex == IndividualSex::kHermaphrodite) && (B_sex != IndividualSex::kHermaphrodite)) || ((A_sex != IndividualSex::kHermaphrodite) && (B_sex == IndividualSex::kHermaphrodite)))
		EIDOS_TERMINATION << "ERROR (Individual::_Relatedness): (internal error) hermaphrodites cannot coexist with males and females" << EidosTerminate();
	if (((A_sex == IndividualSex::kMale) && (B_P1 == A) && (B_P1 != B_P2)) ||
		((B_sex == IndividualSex::kMale) && (A_P1 == B) && (A_P1 != A_P2)) ||
		((A_sex == IndividualSex::kFemale) && (B_P2 == A) && (B_P2 != B_P1)) ||
		((B_sex == IndividualSex::kFemale) && (A_P2 == B) && (A_P2 != A_P1)))
		EIDOS_TERMINATION << "ERROR (Individual::_Relatedness): (internal error) a male was indicated as a first parent, or a female as second parent, without clonality" << EidosTerminate();
#endif
	
	if (modeledChromosomeType == GenomeType::kXChromosome)
	{
		// Whichever sex A is, its second parent (A_P2) is male and so its male parent (A_G4) gave A_P2 a Y, not an X
		A_G4 = A_G3;
		
		if (A_sex == IndividualSex::kMale)
		{
			// If A is male, its second parent (male) gave it a Y, not an X
			A_P2 = A_P1;
			A_G3 = A_G1;
			A_G4 = A_G2;
		}
		
		// Whichever sex B is, its second parent (B_P2) is male and so its male parent (B_G4) gave B_P2 a Y, not an X
		B_G4 = B_G3;
		
		if (B_sex == IndividualSex::kMale)
		{
			// If B is male, its second parent (male) gave it a Y, not an X
			B_P2 = B_P1;
			B_G3 = B_G1;
			B_G4 = B_G2;
		}
	}
	else if (modeledChromosomeType == GenomeType::kYChromosome)
	{
		// When modeling the Y, females have no relatedness to anybody else except themselves, defined as 1.0 for consistency
		if ((A_sex == IndividualSex::kFemale) || (B_sex == IndividualSex::kFemale))
		{
			if (A == B)
				return 1.0;
			return 0.0;
		}
		
		// The female parents (A_P1 and B_P1) and their parents, and female grandparents (A_G3 and B_G3), do not contribute
		A_G3 = A_G4;
		A_P1 = A_P2;
		A_G1 = A_G3;
		A_G2 = A_G4;
		
		B_G3 = B_G4;
		B_P1 = B_P2;
		B_G1 = B_G3;
		B_G2 = B_G4;
	}
	
	return ::_Relatedness(A, A_P1, A_P2, A_G1, A_G2, A_G3, A_G4, B, B_P1, B_P2, B_G1, B_G2, B_G3, B_G4);
}

double Individual::RelatednessToIndividual(Individual &p_ind)
{
	// So, the goal is to calculate A and B's relatedness, given pedigree IDs for themselves and (perhaps) for their parents and
	// grandparents.  Note that a pedigree ID of -1 means "no information"; for a given cycle, information should either be
	// available for everybody, or for nobody (the latter occurs when that cycle is prior to the start of forward simulation).
	// So we have these ancestry trees:
	//
	//         G1  G2 G3  G4     G5  G6 G7  G8
	//          \  /   \  /       \  /   \  /
	//           P1     P2         P3     P4
	//            \     /           \     /
	//             \   /             \   /
	//              \ /               \ /
	//               A                 B
	//
	// If A and B are same individual, the relatedness is 1.0.  Otherwise, we need to determine the amount of consanguinity between
	// A and B.  If A is a parent of B (P3 or P4), their relatedness is 0.5; if A is a grandparent of B (G5/G6/G7/G8), then their
	// relatedness is 0.25.  A could also appear in B's tree more than once, but A cannot be its own parent.  So if A==P3, then A
	// cannot also be G5 or G6, and indeed, we do not need to look at G5 or G6 at all; the fact that A==P3 tells us everything we
	// we need to know about that half of B's tree, with a contribution of 0.5.  But it could *additionally* be true that A==P4,
	// giving another 0.5 for 1.0 total; or that A==G7, for 0.25; or that A==G8, for 0.25; for that A==G7 *and* A==G8, for 0.5,
	// making 1.0 total.  Basically, whenever you see A at a given position you do not need to look further upward from that node,
	// but you must still look at other nodes.  To do this properly, recursion is the simplest approach; this algorithm is thanks
	// to Peter Ralph.
	//
	Individual &indA = *this, &indB = p_ind;
	
	slim_pedigreeid_t A = indA.pedigree_id_;
	slim_pedigreeid_t A_P1 = indA.pedigree_p1_;
	slim_pedigreeid_t A_P2 = indA.pedigree_p2_;
	slim_pedigreeid_t A_G1 = indA.pedigree_g1_;
	slim_pedigreeid_t A_G2 = indA.pedigree_g2_;
	slim_pedigreeid_t A_G3 = indA.pedigree_g3_;
	slim_pedigreeid_t A_G4 = indA.pedigree_g4_;
	slim_pedigreeid_t B = indB.pedigree_id_;
	slim_pedigreeid_t B_P1 = indB.pedigree_p1_;
	slim_pedigreeid_t B_P2 = indB.pedigree_p2_;
	slim_pedigreeid_t B_G1 = indB.pedigree_g1_;
	slim_pedigreeid_t B_G2 = indB.pedigree_g2_;
	slim_pedigreeid_t B_G3 = indB.pedigree_g3_;
	slim_pedigreeid_t B_G4 = indB.pedigree_g4_;
	
	GenomeType chrtype = subpopulation_->species_.ModeledChromosomeType();
	
	return _Relatedness(A, A_P1, A_P2, A_G1, A_G2, A_G3, A_G4, B, B_P1, B_P2, B_G1, B_G2, B_G3, B_G4, indA.sex_, indB.sex_, chrtype);
}

int Individual::_SharedParentCount(slim_pedigreeid_t X_P1, slim_pedigreeid_t X_P2, slim_pedigreeid_t Y_P1, slim_pedigreeid_t Y_P2)
{
	// This is the top-level internal API here.  It is separate from RelatednessToIndividual(), and
	// implemented as a static member function, for unit testing; we want an
	// API that unit tests can call without needing to actually have a constructed Individual object.
	
	// If one individual is missing parent information, return 0
	if ((X_P1 == -1) || (X_P2 == -1) || (Y_P1 == -1) || (Y_P2 == -1))
		return 0;
	
	// If both parents match, in one way or another, then they must be full siblings
	if ((X_P1 == Y_P1) && (X_P2 == Y_P2))
		return 2;
	if ((X_P1 == Y_P2) && (X_P2 == Y_P1))
		return 2;
	
	// Otherwise, if one parent matches, they must be half siblings
	if ((X_P1 == Y_P1) || (X_P1 == Y_P2) || (X_P2 == Y_P1) || (X_P2 == Y_P2))
		return 1;
	
	// Otherwise, they are not siblings
	return 0;
}

int Individual::SharedParentCountWithIndividual(Individual &p_ind)
{
	// This is much simpler than Individual::RelatednessToIndividual(); we just want the shared parent count.  That is
	// defined, for two individuals X and Y with parents in {A, B, C, D}, as:
	//
	//	AB CD -> 0 (no shared parents)
	//	AB CC -> 0 (no shared parents)
	//	AB AC -> 1 (half siblings)
	//	AB AA -> 1 (half siblings)
	//	AA AB -> 1 (half siblings)
	//	AB AB -> 2 (full siblings)
	//	AB BA -> 2 (full siblings)
	//	AA AA -> 2 (full siblings)
	//
	// If X is itself a parent of Y, or vice versa, that is irrelevant for this method; we are not measuring
	// consanguinity here.
	//
	Individual &indX = *this, &indY = p_ind;
	
	slim_pedigreeid_t X_P1 = indX.pedigree_p1_;
	slim_pedigreeid_t X_P2 = indX.pedigree_p2_;
	slim_pedigreeid_t Y_P1 = indY.pedigree_p1_;
	slim_pedigreeid_t Y_P2 = indY.pedigree_p2_;
	
	return _SharedParentCount(X_P1, X_P2, Y_P1, Y_P2);
}


//
// Eidos support
//
#pragma mark -
#pragma mark Eidos support
#pragma mark -

void Individual::GenerateCachedEidosValue(void)
{
	// Note that this cache cannot be invalidated as long as a symbol table might exist that this value has been placed into
	self_value_ = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(this, gSLiM_Individual_Class));
}

const EidosClass *Individual::Class(void) const
{
	return gSLiM_Individual_Class;
}

void Individual::Print(std::ostream &p_ostream) const
{
	if (killed_)
		p_ostream << Class()->ClassName() << "<KILLED>";
	else
		p_ostream << Class()->ClassName() << "<p" << subpopulation_->subpopulation_id_ << ":i" << index_ << ">";
}

EidosValue_SP Individual::GetProperty(EidosGlobalStringID p_property_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
			// constants
		case gID_subpopulation:		// ACCELERATED
		{
			if (killed_)
				EIDOS_TERMINATION << "ERROR (Individual::GetProperty): property subpopulation is not available for individuals that have been killed; they have no subpopulation." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(subpopulation_, gSLiM_Subpopulation_Class));
		}
		case gID_index:				// ACCELERATED
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(index_));
		}
		case gID_genomes:
		{
			EidosValue_Object_vector *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Genome_Class))->resize_no_initialize(2);
			
			vec->set_object_element_no_check_NORR(genome1_, 0);
			vec->set_object_element_no_check_NORR(genome2_, 1);
			
			return EidosValue_SP(vec);
		}
		case gID_genomesNonNull:
		{
			bool genome1null = genome1_->IsNull();
			bool genome2null = genome2_->IsNull();
			
			if (genome1null)
			{
				if (genome2null)
					return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Genome_Class));
				else
					return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(genome2_, gSLiM_Genome_Class));
			}
			else
			{
				if (genome2null)
					return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(genome1_, gSLiM_Genome_Class));
				else
				{
					EidosValue_Object_vector *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Genome_Class))->resize_no_initialize(2);
					
					vec->set_object_element_no_check_NORR(genome1_, 0);
					vec->set_object_element_no_check_NORR(genome2_, 1);
					
					return EidosValue_SP(vec);
				}
			}
		}
		case gID_genome1:			// ACCELERATED
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(genome1_, gSLiM_Genome_Class));
		}
		case gID_genome2:			// ACCELERATED
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(genome2_, gSLiM_Genome_Class));
		}
		case gID_sex:
		{
			static EidosValue_SP static_sex_string_H;
			static EidosValue_SP static_sex_string_F;
			static EidosValue_SP static_sex_string_M;
			static EidosValue_SP static_sex_string_O;
			
#pragma omp critical (GetProperty_sex_cache)
			{
				if (!static_sex_string_H)
				{
					static_sex_string_H = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("H"));
					static_sex_string_F = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("F"));
					static_sex_string_M = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("M"));
					static_sex_string_O = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("?"));
				}
			}
			
			switch (sex_)
			{
				case IndividualSex::kHermaphrodite:	return static_sex_string_H;
				case IndividualSex::kFemale:		return static_sex_string_F;
				case IndividualSex::kMale:			return static_sex_string_M;
				default:							return static_sex_string_O;
			}
		}
		case gID_age:				// ACCELERATED
		{
			if (age_ == -1)
				EIDOS_TERMINATION << "ERROR (Individual::GetProperty): property age is not available in WF models." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(age_));
		}
		case gID_meanParentAge:
		{
			if (mean_parent_age_ == -1)
				EIDOS_TERMINATION << "ERROR (Individual::GetProperty): property meanParentAge is not available in WF models." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(mean_parent_age_));
		}
		case gID_pedigreeID:		// ACCELERATED
		{
			if (!subpopulation_->species_.PedigreesEnabledByUser())
				EIDOS_TERMINATION << "ERROR (Individual::GetProperty): property pedigreeID is not available because pedigree recording has not been enabled." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(pedigree_id_));
		}
		case gID_pedigreeParentIDs:
		{
			if (!subpopulation_->species_.PedigreesEnabledByUser())
				EIDOS_TERMINATION << "ERROR (Individual::GetProperty): property pedigreeParentIDs is not available because pedigree recording has not been enabled." << EidosTerminate();
			
			EidosValue_Int_vector *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(2);
			
			vec->set_int_no_check(pedigree_p1_, 0);
			vec->set_int_no_check(pedigree_p2_, 1);
			
			return EidosValue_SP(vec);
		}
		case gID_pedigreeGrandparentIDs:
		{
			if (!subpopulation_->species_.PedigreesEnabledByUser())
				EIDOS_TERMINATION << "ERROR (Individual::GetProperty): property pedigreeGrandparentIDs is not available because pedigree recording has not been enabled." << EidosTerminate();
			
			EidosValue_Int_vector *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(4);
			
			vec->set_int_no_check(pedigree_g1_, 0);
			vec->set_int_no_check(pedigree_g2_, 1);
			vec->set_int_no_check(pedigree_g3_, 2);
			vec->set_int_no_check(pedigree_g4_, 3);
			
			return EidosValue_SP(vec);
		}
		case gID_reproductiveOutput:				// ACCELERATED
		{
			if (!subpopulation_->species_.PedigreesEnabledByUser())
				EIDOS_TERMINATION << "ERROR (Individual::GetProperty): property reproductiveOutput is not available because pedigree recording has not been enabled." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(reproductive_output_));
		}
		case gID_spatialPosition:					// ACCELERATED
		{
			Species &species = subpopulation_->species_;
			
			switch (species.SpatialDimensionality())
			{
				case 0:
					EIDOS_TERMINATION << "ERROR (Individual::GetProperty): position cannot be accessed in non-spatial simulations." << EidosTerminate();
				case 1:
					return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(spatial_x_));
				case 2:
					return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{spatial_x_, spatial_y_});
				case 3:
					return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{spatial_x_, spatial_y_, spatial_z_});
				default:
					return gStaticEidosValueNULL;	// never hit; here to make the compiler happy
			}
		}
		case gID_uniqueMutations:
		{
			if (genome1_->IsDeferred() || genome2_->IsDeferred())
				EIDOS_TERMINATION << "ERROR (Individual::GetProperty): the mutations of deferred genomes cannot be accessed." << EidosTerminate();
			
			// We reserve a vector large enough to hold all the mutations from both genomes; probably usually overkill, but it does little harm
			int genome1_size = (genome1_->IsNull() ? 0 : genome1_->mutation_count());
			int genome2_size = (genome2_->IsNull() ? 0 : genome2_->mutation_count());
			EidosValue_Object_vector *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Mutation_Class));
			EidosValue_SP result_SP = EidosValue_SP(vec);
			
			if ((genome1_size == 0) && (genome2_size == 0))
				return result_SP;
			
			vec->reserve(genome1_size + genome2_size);
			
			Mutation *mut_block_ptr = gSLiM_Mutation_Block;
			int mutrun_count = (genome1_size ? genome1_->mutrun_count_ : genome2_->mutrun_count_);
			
			for (int run_index = 0; run_index < mutrun_count; ++run_index)
			{
				// We want to interleave mutations from the two genomes, keeping only the uniqued mutations.  For a given position, we take mutations
				// from g1 first, and then look at the mutations in g2 at the same position and add them if they are not in g1.
				const MutationRun *mutrun1 = (genome1_size ? genome1_->mutruns_[run_index] : nullptr);
				const MutationRun *mutrun2 = (genome2_size ? genome2_->mutruns_[run_index] : nullptr);
				int g1_size = (mutrun1 ? mutrun1->size() : 0);
				int g2_size = (mutrun2 ? mutrun2->size() : 0);
				int g1_index = 0, g2_index = 0;
				
				if (g1_size && g2_size)
				{
					// Get the position of the mutations at g1_index and g2_index
					MutationIndex g1_mut = (*mutrun1)[g1_index], g2_mut = (*mutrun2)[g2_index];
					slim_position_t pos1 = (mut_block_ptr + g1_mut)->position_, pos2 = (mut_block_ptr + g2_mut)->position_;
					
					// Process mutations as long as both genomes still have mutations left in them
					do
					{
						if (pos1 < pos2)
						{
							vec->push_object_element_no_check_RR(mut_block_ptr + g1_mut);
							
							// Move to the next mutation in g1
							if (++g1_index >= g1_size)
								break;
							g1_mut = (*mutrun1)[g1_index];
							pos1 = (mut_block_ptr + g1_mut)->position_;
						}
						else if (pos1 > pos2)
						{
							vec->push_object_element_no_check_RR(mut_block_ptr + g2_mut);
							
							// Move to the next mutation in g2
							if (++g2_index >= g2_size)
								break;
							g2_mut = (*mutrun2)[g2_index];
							pos2 = (mut_block_ptr + g2_mut)->position_;
						}
						else
						{
							// pos1 == pos2; copy mutations from g1 until we are done with this position, then handle g2
							slim_position_t focal_pos = pos1;
							int first_index = g1_index;
							bool done = false;
							
							while (pos1 == focal_pos)
							{
								vec->push_object_element_no_check_RR(mut_block_ptr + g1_mut);
								
								// Move to the next mutation in g1
								if (++g1_index >= g1_size)
								{
									done = true;
									break;
								}
								g1_mut = (*mutrun1)[g1_index];
								pos1 = (mut_block_ptr + g1_mut)->position_;
							}
							
							// Note that we may be done with g1 here, so be careful
							int last_index_plus_one = g1_index;
							
							while (pos2 == focal_pos)
							{
								int check_index;
								
								for (check_index = first_index; check_index < last_index_plus_one; ++check_index)
									if ((*mutrun1)[check_index] == g2_mut)
										break;
								
								// If the check indicates that g2_mut is not in g1, we copy it over
								if (check_index == last_index_plus_one)
									vec->push_object_element_no_check_RR(mut_block_ptr + g2_mut);
								
								// Move to the next mutation in g2
								if (++g2_index >= g2_size)
								{
									done = true;
									break;
								}
								g2_mut = (*mutrun2)[g2_index];
								pos2 = (mut_block_ptr + g2_mut)->position_;
							}
							
							// Note that we may be done with both g1 and/or g2 here; if so, done will be set and we will break out
							if (done)
								break;
						}
					}
					while (true);
				}
				
				// Finish off any tail ends, which must be unique and sorted already
				while (g1_index < g1_size)
					vec->push_object_element_no_check_RR(mut_block_ptr + (*mutrun1)[g1_index++]);
				while (g2_index < g2_size)
					vec->push_object_element_no_check_RR(mut_block_ptr + (*mutrun2)[g2_index++]);
			}
		
			return result_SP;
		}
			
			// variables
		case gEidosID_color:
		{
			// as of SLiM 4.0.1, we construct a color string from the RGB values, which will
			// not necessarily be what the user set, but will represent the same color
			if (!color_set_)
				return gStaticEidosValue_StringEmpty;
			
			char hex_chars[8];
			
			Eidos_GetColorString(colorR_, colorG_, colorB_, hex_chars);
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(std::string(hex_chars)));
		}
		case gID_tag:				// ACCELERATED
		{
			slim_usertag_t tag_value = tag_value_;
			
			if (tag_value == SLIM_TAG_UNSET_VALUE)
				EIDOS_TERMINATION << "ERROR (Individual::GetProperty): property tag accessed on individual before being set." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(tag_value));
		}
		case gID_tagF:				// ACCELERATED
		{
			double tagF_value = tagF_value_;
			
			if (tagF_value == SLIM_TAGF_UNSET_VALUE)
				EIDOS_TERMINATION << "ERROR (Individual::GetProperty): property tagF accessed on individual before being set." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(tagF_value));
		}
		case gID_tagL0:				// ACCELERATED
		{
			if (!tagL0_set_)
				EIDOS_TERMINATION << "ERROR (Individual::GetProperty): property tagL0 accessed on individual before being set." << EidosTerminate();
			return (tagL0_value_ ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
		}
		case gID_tagL1:				// ACCELERATED
		{
			if (!tagL1_set_)
				EIDOS_TERMINATION << "ERROR (Individual::GetProperty): property tagL1 accessed on individual before being set." << EidosTerminate();
			return (tagL1_value_ ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
		}
		case gID_tagL2:				// ACCELERATED
		{
			if (!tagL2_set_)
				EIDOS_TERMINATION << "ERROR (Individual::GetProperty): property tagL2 accessed on individual before being set." << EidosTerminate();
			return (tagL2_value_ ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
		}
		case gID_tagL3:				// ACCELERATED
		{
			if (!tagL3_set_)
				EIDOS_TERMINATION << "ERROR (Individual::GetProperty): property tagL3 accessed on individual before being set." << EidosTerminate();
			return (tagL3_value_ ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
		}
		case gID_tagL4:				// ACCELERATED
		{
			if (!tagL4_set_)
				EIDOS_TERMINATION << "ERROR (Individual::GetProperty): property tagL4 accessed on individual before being set." << EidosTerminate();
			return (tagL4_value_ ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
		}
		case gID_migrant:			// ACCELERATED
		{
			return (migrant_ ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
		}
		case gID_fitnessScaling:	// ACCELERATED
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(fitness_scaling_));
		}
		case gEidosID_x:			// ACCELERATED
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(spatial_x_));
		}
		case gEidosID_y:			// ACCELERATED
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(spatial_y_));
		}
		case gEidosID_z:			// ACCELERATED
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(spatial_z_));
		}
			
			// These properties are presently undocumented, used for testing purposes, but maybe they are useful to others?
			// They provide x/y/z as pairs or a triplet, whether the model is spatial or not, regardless of dimensionality
		case gEidosID_xy:
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector({spatial_x_, spatial_y_}));
		}
		case gEidosID_xz:
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector({spatial_x_, spatial_z_}));
		}
		case gEidosID_yz:
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector({spatial_y_, spatial_z_}));
		}
		case gEidosID_xyz:
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector({spatial_x_, spatial_y_, spatial_z_}));
		}
			
			// all others, including gID_none
		default:
			return super::GetProperty(p_property_id);
	}
}

EidosValue *Individual::GetProperty_Accelerated_index(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Individual *value = (Individual *)(p_values[value_index]);
		
		int_result->set_int_no_check(value->index_, value_index);
	}
	
	return int_result;
}

EidosValue *Individual::GetProperty_Accelerated_pedigreeID(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(p_values_size);
	size_t value_index = 0;
	
	// check that pedigrees are enabled, once
	if (value_index < p_values_size)
	{
		Individual *value = (Individual *)(p_values[value_index]);
		
		if (!value->subpopulation_->species_.PedigreesEnabledByUser())
			EIDOS_TERMINATION << "ERROR (Individual::GetProperty): property pedigreeID is not available because pedigree recording has not been enabled." << EidosTerminate();
		
		int_result->set_int_no_check(value->pedigree_id_, value_index);
		++value_index;
	}
	
	for ( ; value_index < p_values_size; ++value_index)
	{
		Individual *value = (Individual *)(p_values[value_index]);
		
		int_result->set_int_no_check(value->pedigree_id_, value_index);
	}
	
	return int_result;
}

EidosValue *Individual::GetProperty_Accelerated_tag(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Individual *value = (Individual *)(p_values[value_index]);
		slim_usertag_t tag_value = value->tag_value_;
		
		if (tag_value == SLIM_TAG_UNSET_VALUE)
			EIDOS_TERMINATION << "ERROR (Individual::GetProperty): property tag accessed on individual before being set." << EidosTerminate();
		
		int_result->set_int_no_check(tag_value, value_index);
	}
	
	return int_result;
}

EidosValue *Individual::GetProperty_Accelerated_age(EidosObject **p_values, size_t p_values_size)
{
	if ((p_values_size > 0) && (((Individual *)(p_values[0]))->subpopulation_->community_.ModelType() == SLiMModelType::kModelTypeWF))
		EIDOS_TERMINATION << "ERROR (Individual::GetProperty): property age is not available in WF models." << EidosTerminate();
	
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Individual *value = (Individual *)(p_values[value_index]);
		
		int_result->set_int_no_check(value->age_, value_index);
	}
	
	return int_result;
}

EidosValue *Individual::GetProperty_Accelerated_reproductiveOutput(EidosObject **p_values, size_t p_values_size)
{
	if ((p_values_size > 0) && !((Individual *)(p_values[0]))->subpopulation_->species_.PedigreesEnabledByUser())
		EIDOS_TERMINATION << "ERROR (Individual::GetProperty): property reproductiveOutput is not available because pedigree recording has not been enabled." << EidosTerminate();
	
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Individual *value = (Individual *)(p_values[value_index]);
		
		int_result->set_int_no_check(value->reproductive_output_, value_index);
	}
	
	return int_result;
}

EidosValue *Individual::GetProperty_Accelerated_tagF(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Individual *value = (Individual *)(p_values[value_index]);
		double tagF_value = value->tagF_value_;
		
		if (tagF_value == SLIM_TAGF_UNSET_VALUE)
			EIDOS_TERMINATION << "ERROR (Individual::GetProperty): property tagF accessed on individual before being set." << EidosTerminate();
		
		float_result->set_float_no_check(tagF_value, value_index);
	}
	
	return float_result;
}

EidosValue *Individual::GetProperty_Accelerated_tagL0(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Individual *value = (Individual *)(p_values[value_index]);
		
		if (!value->tagL0_set_)
			EIDOS_TERMINATION << "ERROR (Individual::GetProperty): property tagL0 accessed on individual before being set." << EidosTerminate();
		
		logical_result->set_logical_no_check(value->tagL0_value_, value_index);
	}
	
	return logical_result;
}

EidosValue *Individual::GetProperty_Accelerated_tagL1(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Individual *value = (Individual *)(p_values[value_index]);
		
		if (!value->tagL1_set_)
			EIDOS_TERMINATION << "ERROR (Individual::GetProperty): property tagL1 accessed on individual before being set." << EidosTerminate();
		
		logical_result->set_logical_no_check(value->tagL1_value_, value_index);
	}
	
	return logical_result;
}

EidosValue *Individual::GetProperty_Accelerated_tagL2(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Individual *value = (Individual *)(p_values[value_index]);
		
		if (!value->tagL2_set_)
			EIDOS_TERMINATION << "ERROR (Individual::GetProperty): property tagL2 accessed on individual before being set." << EidosTerminate();
		
		logical_result->set_logical_no_check(value->tagL2_value_, value_index);
	}
	
	return logical_result;
}

EidosValue *Individual::GetProperty_Accelerated_tagL3(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Individual *value = (Individual *)(p_values[value_index]);
		
		if (!value->tagL3_set_)
			EIDOS_TERMINATION << "ERROR (Individual::GetProperty): property tagL3 accessed on individual before being set." << EidosTerminate();
		
		logical_result->set_logical_no_check(value->tagL3_value_, value_index);
	}
	
	return logical_result;
}

EidosValue *Individual::GetProperty_Accelerated_tagL4(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Individual *value = (Individual *)(p_values[value_index]);
		
		if (!value->tagL4_set_)
			EIDOS_TERMINATION << "ERROR (Individual::GetProperty): property tagL4 accessed on individual before being set." << EidosTerminate();
		
		logical_result->set_logical_no_check(value->tagL4_value_, value_index);
	}
	
	return logical_result;
}

EidosValue *Individual::GetProperty_Accelerated_migrant(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Individual *value = (Individual *)(p_values[value_index]);
		
		logical_result->set_logical_no_check(value->migrant_, value_index);
	}
	
	return logical_result;
}

EidosValue *Individual::GetProperty_Accelerated_fitnessScaling(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Individual *value = (Individual *)(p_values[value_index]);
		
		float_result->set_float_no_check(value->fitness_scaling_, value_index);
	}
	
	return float_result;
}

EidosValue *Individual::GetProperty_Accelerated_x(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Individual *value = (Individual *)(p_values[value_index]);
		
		float_result->set_float_no_check(value->spatial_x_, value_index);
	}
	
	return float_result;
}

EidosValue *Individual::GetProperty_Accelerated_y(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Individual *value = (Individual *)(p_values[value_index]);
		
		float_result->set_float_no_check(value->spatial_y_, value_index);
	}
	
	return float_result;
}

EidosValue *Individual::GetProperty_Accelerated_z(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Individual *value = (Individual *)(p_values[value_index]);
		
		float_result->set_float_no_check(value->spatial_z_, value_index);
	}
	
	return float_result;
}

EidosValue *Individual::GetProperty_Accelerated_spatialPosition(EidosObject **p_values, size_t p_values_size)
{
	Species *consensus_species = Community::SpeciesForIndividualsVector((Individual **)p_values, (int)p_values_size);
	EidosValue_Float_vector *float_result;
	
	if (consensus_species)
	{
		// All individuals belong to the same species (common case), so we have the same dimensionality for all
		int dimensionality = consensus_species->SpatialDimensionality();
		
		if (dimensionality == 0)
			EIDOS_TERMINATION << "ERROR (Individual::GetProperty): position cannot be accessed in non-spatial simulations." << EidosTerminate();
		
		float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(p_values_size * dimensionality);
		
		if (dimensionality == 1)
		{
			for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			{
				Individual *value = (Individual *)(p_values[value_index]);
				float_result->set_float_no_check(value->spatial_x_, value_index);
			}
		}
		else if (dimensionality == 2)
		{
			size_t result_index = 0;
			
			for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			{
				Individual *value = (Individual *)(p_values[value_index]);
				float_result->set_float_no_check(value->spatial_x_, result_index++);
				float_result->set_float_no_check(value->spatial_y_, result_index++);
			}
		}
		else // dimensionality == 3
		{
			size_t result_index = 0;
			
			for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			{
				Individual *value = (Individual *)(p_values[value_index]);
				float_result->set_float_no_check(value->spatial_x_, result_index++);
				float_result->set_float_no_check(value->spatial_y_, result_index++);
				float_result->set_float_no_check(value->spatial_z_, result_index++);
			}
		}
	}
	else
	{
		// Mixed-species group, so we have to figure out the dimensionality for each individual separately
		// FIXME: Do we really want to allow this?  seems crazy - how would the user actually use this?
		float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
		{
			Individual *value = (Individual *)(p_values[value_index]);
			Species &species = value->subpopulation_->species_;
			
			switch (species.SpatialDimensionality())
			{
				case 0:
					EIDOS_TERMINATION << "ERROR (Individual::GetProperty): position cannot be accessed in non-spatial simulations." << EidosTerminate();
				case 1:
					float_result->push_float(value->spatial_x_);
					break;
				case 2:
					float_result->push_float(value->spatial_x_);
					float_result->push_float(value->spatial_y_);
					break;
				case 3:
					float_result->push_float(value->spatial_x_);
					float_result->push_float(value->spatial_y_);
					float_result->push_float(value->spatial_z_);
					break;
				default:
					break;	// never hit; here to make the compiler happy
			}
		}
	}
	
	return float_result;
}

EidosValue *Individual::GetProperty_Accelerated_subpopulation(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Object_vector *object_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Subpopulation_Class))->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Individual *value = (Individual *)(p_values[value_index]);
		
		if (value->killed_)
			EIDOS_TERMINATION << "ERROR (Individual::GetProperty): property subpopulation is not available for individuals that have been killed; they have no subpopulation." << EidosTerminate();
		
		object_result->set_object_element_no_check_NORR(value->subpopulation_, value_index);
	}
	
	return object_result;
}

EidosValue *Individual::GetProperty_Accelerated_genome1(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Object_vector *object_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Genome_Class))->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Individual *value = (Individual *)(p_values[value_index]);
		
		object_result->set_object_element_no_check_NORR(value->genome1_, value_index);
	}
	
	return object_result;
}

EidosValue *Individual::GetProperty_Accelerated_genome2(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Object_vector *object_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Genome_Class))->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Individual *value = (Individual *)(p_values[value_index]);
		
		object_result->set_object_element_no_check_NORR(value->genome2_, value_index);
	}
	
	return object_result;
}

void Individual::SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		case gEidosID_color:		// ACCELERATED
		{
			const std::string &color_string = ((EidosValue_String &)p_value).StringRefAtIndex(0, nullptr);
			
			if (color_string.empty())
			{
				color_set_ = false;
			}
			else
			{
				Eidos_GetColorComponents(color_string, &colorR_, &colorG_, &colorB_);
				color_set_ = true;
				s_any_individual_color_set_ = true;		// keep track of the fact that an individual's color has been set
			}
			return;
		}
		case gID_tag:				// ACCELERATED
		{
			slim_usertag_t value = SLiMCastToUsertagTypeOrRaise(p_value.IntAtIndex(0, nullptr));
			
			tag_value_ = value;
			s_any_individual_tag_set_ = true;
			return;
		}
		case gID_tagF:				// ACCELERATED
		{
			tagF_value_ = p_value.FloatAtIndex(0, nullptr);
			s_any_individual_tagF_set_ = true;
			return;
		}
		case gID_tagL0:				// ACCELERATED
		{
			eidos_logical_t value = p_value.LogicalAtIndex(0, nullptr);
			
			tagL0_set_ = true;
			tagL0_value_ = value;
			s_any_individual_tagL_set_ = true;
			return;
		}
		case gID_tagL1:				// ACCELERATED
		{
			eidos_logical_t value = p_value.LogicalAtIndex(0, nullptr);
			
			tagL1_set_ = true;
			tagL1_value_ = value;
			s_any_individual_tagL_set_ = true;
			return;
		}
		case gID_tagL2:				// ACCELERATED
		{
			eidos_logical_t value = p_value.LogicalAtIndex(0, nullptr);
			
			tagL2_set_ = true;
			tagL2_value_ = value;
			s_any_individual_tagL_set_ = true;
			return;
		}
		case gID_tagL3:				// ACCELERATED
		{
			eidos_logical_t value = p_value.LogicalAtIndex(0, nullptr);
			
			tagL3_set_ = true;
			tagL3_value_ = value;
			s_any_individual_tagL_set_ = true;
			return;
		}
		case gID_tagL4:				// ACCELERATED
		{
			eidos_logical_t value = p_value.LogicalAtIndex(0, nullptr);
			
			tagL4_set_ = true;
			tagL4_value_ = value;
			s_any_individual_tagL_set_ = true;
			return;
		}
		case gID_fitnessScaling:	// ACCELERATED
		{
			fitness_scaling_ = p_value.FloatAtIndex(0, nullptr);
			Individual::s_any_individual_fitness_scaling_set_ = true;
			
			if ((fitness_scaling_ < 0.0) || (std::isnan(fitness_scaling_)))
				EIDOS_TERMINATION << "ERROR (Individual::SetProperty): property fitnessScaling must be >= 0.0." << EidosTerminate();
			
			return;
		}
		case gEidosID_x:			// ACCELERATED
		{
			spatial_x_ = p_value.FloatAtIndex(0, nullptr);
			return;
		}
		case gEidosID_y:			// ACCELERATED
		{
			spatial_y_ = p_value.FloatAtIndex(0, nullptr);
			return;
		}
		case gEidosID_z:			// ACCELERATED
		{
			spatial_z_ = p_value.FloatAtIndex(0, nullptr);
			return;
		}
		case gID_age:				// ACCELERATED
		{
			slim_age_t value = SLiMCastToAgeTypeOrRaise(p_value.IntAtIndex(0, nullptr));
			
			age_ = value;
			return;
		}
			
			// all others, including gID_none
		default:
			return super::SetProperty(p_property_id, p_value);
	}
}

void Individual::SetProperty_Accelerated_tag(EidosObject **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size)
{
	s_any_individual_tag_set_ = true;
	
	// SLiMCastToUsertagTypeOrRaise() is a no-op at present
	if (p_source_size == 1)
	{
		int64_t source_value = p_source.IntAtIndex(0, nullptr);
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Individual *)(p_values[value_index]))->tag_value_ = source_value;
	}
	else
	{
		const int64_t *source_data = p_source.IntVector()->data();
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Individual *)(p_values[value_index]))->tag_value_ = source_data[value_index];
	}
}

void Individual::SetProperty_Accelerated_tagF(EidosObject **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size)
{
	s_any_individual_tagF_set_ = true;
	
	// SLiMCastToUsertagTypeOrRaise() is a no-op at present
	if (p_source_size == 1)
	{
		double source_value = p_source.FloatAtIndex(0, nullptr);
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Individual *)(p_values[value_index]))->tagF_value_ = source_value;
	}
	else
	{
		const double *source_data = p_source.FloatVector()->data();
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Individual *)(p_values[value_index]))->tagF_value_ = source_data[value_index];
	}
}

void Individual::SetProperty_Accelerated_tagL0(EidosObject **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size)
{
	s_any_individual_tagL_set_ = true;
	
	if (p_source_size == 1)
	{
		eidos_logical_t source_value = p_source.LogicalAtIndex(0, nullptr);
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
		{
			Individual *individual = ((Individual *)(p_values[value_index]));
			
			individual->tagL0_set_ = true;
			individual->tagL0_value_ = source_value;
		}
	}
	else
	{
		const eidos_logical_t *source_data = p_source.LogicalVector()->data();
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
		{
			Individual *individual = ((Individual *)(p_values[value_index]));
			
			individual->tagL0_set_ = true;
			individual->tagL0_value_ = source_data[value_index];
		}
	}
}

void Individual::SetProperty_Accelerated_tagL1(EidosObject **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size)
{
	s_any_individual_tagL_set_ = true;
	
	if (p_source_size == 1)
	{
		eidos_logical_t source_value = p_source.LogicalAtIndex(0, nullptr);
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
		{
			Individual *individual = ((Individual *)(p_values[value_index]));
			
			individual->tagL1_set_ = true;
			individual->tagL1_value_ = source_value;
		}
	}
	else
	{
		const eidos_logical_t *source_data = p_source.LogicalVector()->data();
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
		{
			Individual *individual = ((Individual *)(p_values[value_index]));
			
			individual->tagL1_set_ = true;
			individual->tagL1_value_ = source_data[value_index];
		}
	}
}

void Individual::SetProperty_Accelerated_tagL2(EidosObject **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size)
{
	s_any_individual_tagL_set_ = true;
	
	if (p_source_size == 1)
	{
		eidos_logical_t source_value = p_source.LogicalAtIndex(0, nullptr);
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
		{
			Individual *individual = ((Individual *)(p_values[value_index]));
			
			individual->tagL2_set_ = true;
			individual->tagL2_value_ = source_value;
		}
	}
	else
	{
		const eidos_logical_t *source_data = p_source.LogicalVector()->data();
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
		{
			Individual *individual = ((Individual *)(p_values[value_index]));
			
			individual->tagL2_set_ = true;
			individual->tagL2_value_ = source_data[value_index];
		}
	}
}

void Individual::SetProperty_Accelerated_tagL3(EidosObject **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size)
{
	s_any_individual_tagL_set_ = true;
	
	if (p_source_size == 1)
	{
		eidos_logical_t source_value = p_source.LogicalAtIndex(0, nullptr);
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
		{
			Individual *individual = ((Individual *)(p_values[value_index]));
			
			individual->tagL3_set_ = true;
			individual->tagL3_value_ = source_value;
		}
	}
	else
	{
		const eidos_logical_t *source_data = p_source.LogicalVector()->data();
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
		{
			Individual *individual = ((Individual *)(p_values[value_index]));
			
			individual->tagL3_set_ = true;
			individual->tagL3_value_ = source_data[value_index];
		}
	}
}

void Individual::SetProperty_Accelerated_tagL4(EidosObject **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size)
{
	s_any_individual_tagL_set_ = true;
	
	if (p_source_size == 1)
	{
		eidos_logical_t source_value = p_source.LogicalAtIndex(0, nullptr);
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
		{
			Individual *individual = ((Individual *)(p_values[value_index]));
			
			individual->tagL4_set_ = true;
			individual->tagL4_value_ = source_value;
		}
	}
	else
	{
		const eidos_logical_t *source_data = p_source.LogicalVector()->data();
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
		{
			Individual *individual = ((Individual *)(p_values[value_index]));
			
			individual->tagL4_set_ = true;
			individual->tagL4_value_ = source_data[value_index];
		}
	}
}

bool Individual::_SetFitnessScaling_1(double source_value, EidosObject **p_values, size_t p_values_size)
{
	if ((source_value < 0.0) || (std::isnan(source_value)))
		return true;
	
	// Note that parallelization here only helps on machines with very high memory bandwidth,
	// because this loop spends all of its time writing to memory.  It also introduces a
	// potential race condition if the same Individual is referenced more than once in
	// p_values; that is considered a bug in the user's script, and we could check for it
	// in DEBUG mode if we wanted to.
	EIDOS_THREAD_COUNT(gEidos_OMP_threads_SET_FITNESS_SCALE_1);
#pragma omp parallel for simd schedule(simd:static) default(none) shared(p_values_size) firstprivate(p_values, source_value) if(parallel:p_values_size >= EIDOS_OMPMIN_SET_FITNESS_SCALE_1) num_threads(thread_count)
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
		((Individual *)(p_values[value_index]))->fitness_scaling_ = source_value;
	
	return false;
}

bool Individual::_SetFitnessScaling_N(const double *source_data, EidosObject **p_values, size_t p_values_size)
{
	bool saw_error = false;	// deferred raises for OpenMP compliance
	
	// Note that parallelization here only helps on machines with very high memory bandwidth,
	// because this loop spends all of its time writing to memory.  It also introduces a
	// potential race condition if the same Individual is referenced more than once in
	// p_values; that is considered a bug in the user's script, and we could check for it
	// in DEBUG mode if we wanted to.
	EIDOS_THREAD_COUNT(gEidos_OMP_threads_SET_FITNESS_SCALE_2);
#pragma omp parallel for schedule(static) default(none) shared(p_values_size) firstprivate(p_values, source_data) reduction(||: saw_error) if(p_values_size >= EIDOS_OMPMIN_SET_FITNESS_SCALE_2) num_threads(thread_count)
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		double source_value = source_data[value_index];
		
		if ((source_value < 0.0) || (std::isnan(source_value)))
			saw_error = true;
		
		((Individual *)(p_values[value_index]))->fitness_scaling_ = source_value;
	}
	
	return saw_error;
}

void Individual::SetProperty_Accelerated_fitnessScaling(EidosObject **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size)
{
	Individual::s_any_individual_fitness_scaling_set_ = true;
	bool needs_raise = false;
	
	if (p_source_size == 1)
	{
		double source_value = p_source.FloatAtIndex(0, nullptr);
		
		needs_raise = _SetFitnessScaling_1(source_value, p_values, p_values_size);
	}
	else
	{
		const double *source_data = p_source.FloatVector()->data();
		
		needs_raise = _SetFitnessScaling_N(source_data, p_values, p_values_size);
	}
	
	if (needs_raise)
		EIDOS_TERMINATION << "ERROR (Individual::SetProperty_Accelerated_fitnessScaling): property fitnessScaling must be >= 0.0." << EidosTerminate();
}

void Individual::SetProperty_Accelerated_x(EidosObject **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size)
{
	if (p_source_size == 1)
	{
		double source_value = p_source.FloatAtIndex(0, nullptr);
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Individual *)(p_values[value_index]))->spatial_x_ = source_value;
	}
	else
	{
		const double *source_data = p_source.FloatVector()->data();
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Individual *)(p_values[value_index]))->spatial_x_ = source_data[value_index];
	}
}

void Individual::SetProperty_Accelerated_y(EidosObject **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size)
{
	if (p_source_size == 1)
	{
		double source_value = p_source.FloatAtIndex(0, nullptr);
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Individual *)(p_values[value_index]))->spatial_y_ = source_value;
	}
	else
	{
		const double *source_data = p_source.FloatVector()->data();
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Individual *)(p_values[value_index]))->spatial_y_ = source_data[value_index];
	}
}

void Individual::SetProperty_Accelerated_z(EidosObject **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size)
{
	if (p_source_size == 1)
	{
		double source_value = p_source.FloatAtIndex(0, nullptr);
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Individual *)(p_values[value_index]))->spatial_z_ = source_value;
	}
	else
	{
		const double *source_data = p_source.FloatVector()->data();
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Individual *)(p_values[value_index]))->spatial_z_ = source_data[value_index];
	}
}

void Individual::SetProperty_Accelerated_color(EidosObject **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size)
{
	if (p_source_size == 1)
	{
		const std::string &source_value = ((EidosValue_String &)p_source).StringRefAtIndex(0, nullptr);
		
		if (source_value.empty())
		{
			for (size_t value_index = 0; value_index < p_values_size; ++value_index)
				((Individual *)(p_values[value_index]))->color_set_ = false;
		}
		else
		{
			uint8_t color_red, color_green, color_blue;
			
			Eidos_GetColorComponents(source_value, &color_red, &color_green, &color_blue);
			
			for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			{
				Individual *individual = ((Individual *)(p_values[value_index]));
				
				individual->colorR_ = color_red;
				individual->colorG_ = color_green;
				individual->colorB_ = color_blue;
				individual->color_set_ = true;
			}
			
			s_any_individual_color_set_ = true;		// keep track of the fact that an individual's color has been set
		}
	}
	else
	{
		const std::vector<std::string> *source_data = p_source.StringVector();
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
		{
			Individual *individual = ((Individual *)(p_values[value_index]));
			const std::string &source_value = (*source_data)[value_index];
			
			if (source_value.empty())
			{
				individual->color_set_ = false;
			}
			else
			{
				Eidos_GetColorComponents(source_value, &individual->colorR_, &individual->colorG_, &individual->colorB_);
				individual->color_set_ = true;
				s_any_individual_color_set_ = true;		// keep track of the fact that an individual's color has been set
			}
		}
	}
}

void Individual::SetProperty_Accelerated_age(EidosObject **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size)
{
	if (p_source_size == 1)
	{
		int64_t source_value = p_source.IntAtIndex(0, nullptr);
		slim_age_t source_age = SLiMCastToAgeTypeOrRaise(source_value);
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Individual *)(p_values[value_index]))->age_ = source_age;
	}
	else
	{
		const int64_t *source_data = p_source.IntVector()->data();
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Individual *)(p_values[value_index]))->age_ = SLiMCastToAgeTypeOrRaise(source_data[value_index]);
	}
}

EidosValue_SP Individual::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	switch (p_method_id)
	{
		case gID_containsMutations:			return ExecuteMethod_containsMutations(p_method_id, p_arguments, p_interpreter);
		//case gID_countOfMutationsOfType:	return ExecuteMethod_Accelerated_countOfMutationsOfType(p_method_id, p_arguments, p_interpreter);
		case gID_relatedness:				return ExecuteMethod_relatedness(p_method_id, p_arguments, p_interpreter);
		case gID_sharedParentCount:			return ExecuteMethod_sharedParentCount(p_method_id, p_arguments, p_interpreter);
		//case gID_sumOfMutationsOfType:	return ExecuteMethod_Accelerated_sumOfMutationsOfType(p_method_id, p_arguments, p_interpreter);
		case gID_uniqueMutationsOfType:		return ExecuteMethod_uniqueMutationsOfType(p_method_id, p_arguments, p_interpreter);
			
		default:
		{
			// In a sense, we here "subclass" EidosDictionaryUnretained to override setValue(); we set a flag remembering that
			// an individual's dictionary has been modified, and then we call "super" for the usual behavior.
			if (p_method_id == gEidosID_setValue)
				s_any_individual_dictionary_set_ = true;
			
			return super::ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
		}
	}
}

//	*********************	- (logical)containsMutations(object<Mutation> mutations)
//
EidosValue_SP Individual::ExecuteMethod_containsMutations(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	if (genome1_->IsDeferred() || genome2_->IsDeferred())
		EIDOS_TERMINATION << "ERROR (Individual::ExecuteMethod_containsMutations): the mutations of deferred genomes cannot be accessed." << EidosTerminate();
	
	EidosValue *mutations_value = p_arguments[0].get();
	int mutations_count = mutations_value->Count();
	
	// SPECIES CONSISTENCY CHECK
	if (mutations_count > 0)
	{
		Species *species = Community::SpeciesForMutations(mutations_value);
		
		if (species != &subpopulation_->species_)
			EIDOS_TERMINATION << "ERROR (Individual::ExecuteMethod_containsMutations): containsMutations() requires that all mutations belong to the same species as the target individual." << EidosTerminate();
	}
	
	if (mutations_count == 1)
	{
		MutationIndex mut = ((Mutation *)(mutations_value->ObjectElementAtIndex(0, nullptr)))->BlockIndex();
		
		if ((!genome1_->IsNull() && genome1_->contains_mutation(mut)) || (!genome2_->IsNull() && genome2_->contains_mutation(mut)))
			return gStaticEidosValue_LogicalT;
		else
			return gStaticEidosValue_LogicalF;
	}
	else
	{
		EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(mutations_count);
		
		for (int value_index = 0; value_index < mutations_count; ++value_index)
		{
			MutationIndex mut = ((Mutation *)(mutations_value->ObjectElementAtIndex(value_index, nullptr)))->BlockIndex();
			bool contains_mut = ((!genome1_->IsNull() && genome1_->contains_mutation(mut)) || (!genome2_->IsNull() && genome2_->contains_mutation(mut)));
			
			logical_result->set_logical_no_check(contains_mut, value_index);
		}
		
		return EidosValue_SP(logical_result);
	}
	
	return gStaticEidosValueNULL;
}

//	*********************	- (integer$)countOfMutationsOfType(io<MutationType>$ mutType)
//
EidosValue_SP Individual::ExecuteMethod_Accelerated_countOfMutationsOfType(EidosObject **p_elements, size_t p_elements_size, EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	if (p_elements_size == 0)
		return gStaticEidosValue_Integer_ZeroVec;
	
	// SPECIES CONSISTENCY CHECK
	Species *species = Community::SpeciesForIndividualsVector((Individual **)p_elements, (int)p_elements_size);
	
	if (species == nullptr)
		EIDOS_TERMINATION << "ERROR (Individual::ExecuteMethod_Accelerated_countOfMutationsOfType): countOfMutationsOfType() requires that mutType belongs to the same species as the target individual." << EidosTerminate();
	
	species->population_.CheckForDeferralInIndividualsVector((Individual **)p_elements, p_elements_size, "Individual::ExecuteMethod_Accelerated_countOfMutationsOfType");
	
	EidosValue *mutType_value = p_arguments[0].get();
	MutationType *mutation_type_ptr = SLiM_ExtractMutationTypeFromEidosValue_io(mutType_value, 0, &species->community_, species, "countOfMutationsOfType()");		// SPECIES CONSISTENCY CHECK
	
	// Count the number of mutations of the given type
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	EidosValue_Int_vector *integer_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(p_elements_size);
	
	EIDOS_THREAD_COUNT(gEidos_OMP_threads_I_COUNT_OF_MUTS_OF_TYPE);
#pragma omp parallel for schedule(dynamic, 1) default(none) shared(p_elements_size) firstprivate(p_elements, mut_block_ptr, mutation_type_ptr, integer_result) if(p_elements_size >= EIDOS_OMPMIN_I_COUNT_OF_MUTS_OF_TYPE) num_threads(thread_count)
	for (size_t element_index = 0; element_index < p_elements_size; ++element_index)
	{
		Individual *element = (Individual *)(p_elements[element_index]);
		Genome *genome1 = element->genome1_;
		Genome *genome2 = element->genome2_;
		int match_count = 0;
	
		if (!genome1->IsNull())
		{
			int mutrun_count = genome1->mutrun_count_;
			
			for (int run_index = 0; run_index < mutrun_count; ++run_index)
			{
				const MutationRun *mutrun = genome1->mutruns_[run_index];
				int genome1_count = mutrun->size();
				const MutationIndex *genome1_ptr = mutrun->begin_pointer_const();
				
				for (int mut_index = 0; mut_index < genome1_count; ++mut_index)
					if ((mut_block_ptr + genome1_ptr[mut_index])->mutation_type_ptr_ == mutation_type_ptr)
						++match_count;
			}
		}
		if (!genome2->IsNull())
		{
			int mutrun_count = genome2->mutrun_count_;
			
			for (int run_index = 0; run_index < mutrun_count; ++run_index)
			{
				const MutationRun *mutrun = genome2->mutruns_[run_index];
				int genome2_count = mutrun->size();
				const MutationIndex *genome2_ptr = mutrun->begin_pointer_const();
				
				for (int mut_index = 0; mut_index < genome2_count; ++mut_index)
					if ((mut_block_ptr + genome2_ptr[mut_index])->mutation_type_ptr_ == mutation_type_ptr)
						++match_count;
			}
		}
		
		integer_result->set_int_no_check(match_count, element_index);
	}
	
	return EidosValue_SP(integer_result);
}

//	*********************	- (float)relatedness(o<Individual> individuals)
//
EidosValue_SP Individual::ExecuteMethod_relatedness(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *individuals_value = p_arguments[0].get();
	int individuals_count = individuals_value->Count();
	
	// SPECIES CONSISTENCY CHECK
	if (individuals_count > 0)
	{
		Species *species = Community::SpeciesForIndividuals(individuals_value);
		
		if (species != &subpopulation_->species_)
			EIDOS_TERMINATION << "ERROR (Individual::ExecuteMethod_relatedness): relatedness() requires that all individuals belong to the same species as the target individual." << EidosTerminate();
	}
	
	bool pedigree_tracking_enabled = subpopulation_->species_.PedigreesEnabledByUser();
	
	if (individuals_count == 1)
	{
		Individual *ind = (Individual *)(individuals_value->ObjectElementAtIndex(0, nullptr));
		double relatedness;
		
		if (pedigree_tracking_enabled)
			relatedness = RelatednessToIndividual(*ind);
		else
			relatedness = (ind == this) ? 1.0 : 0.0;
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(relatedness));
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(individuals_count);
		
		if (pedigree_tracking_enabled)
		{
			// this parallelizes the case of one_individual.relatedness(many_individuals)
			// it would be nice to also parallelize the case of many_individuals.relatedness(one_individual); that would require accelerating this method
			EIDOS_THREAD_COUNT(gEidos_OMP_threads_RELATEDNESS);
#pragma omp parallel for schedule(dynamic, 128) default(none) shared(individuals_count, individuals_value) firstprivate(float_result) if(individuals_count >= EIDOS_OMPMIN_RELATEDNESS) num_threads(thread_count)
			for (int value_index = 0; value_index < individuals_count; ++value_index)
			{
				Individual *ind = (Individual *)(individuals_value->ObjectElementAtIndex(value_index, nullptr));
				double relatedness = RelatednessToIndividual(*ind);
				
				float_result->set_float_no_check(relatedness, value_index);
			}
		}
		else
		{
			for (int value_index = 0; value_index < individuals_count; ++value_index)
			{
				Individual *ind = (Individual *)(individuals_value->ObjectElementAtIndex(value_index, nullptr));
				double relatedness = (ind == this) ? 1.0 : 0.0;
				
				float_result->set_float_no_check(relatedness, value_index);
			}
		}
		
		return EidosValue_SP(float_result);
	}
	
	return gStaticEidosValueNULL;
}

//	*********************	- (integer)sharedParentCount(o<Individual> individuals)
//
EidosValue_SP Individual::ExecuteMethod_sharedParentCount(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *individuals_value = p_arguments[0].get();
	int individuals_count = individuals_value->Count();
	
	// SPECIES CONSISTENCY CHECK
	if (individuals_count > 0)
	{
		Species *species = Community::SpeciesForIndividuals(individuals_value);
		
		if (species != &subpopulation_->species_)
			EIDOS_TERMINATION << "ERROR (Individual::ExecuteMethod_sharedParentCount): sharedParentCount() requires that all individuals belong to the same species as the target individual." << EidosTerminate();
	}
	
	bool pedigree_tracking_enabled = subpopulation_->species_.PedigreesEnabledByUser();
	
	if (individuals_count == 1)
	{
		Individual *ind = (Individual *)(individuals_value->ObjectElementAtIndex(0, nullptr));
		int shared_count;
		
		if (pedigree_tracking_enabled)
			shared_count = SharedParentCountWithIndividual(*ind);
		else
			shared_count = (ind == this) ? 2.0 : 0.0;
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(shared_count));
	}
	else
	{
		EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(individuals_count);
		
		if (pedigree_tracking_enabled)
		{
			// FIXME needs parallelization, see relatedness()
			for (int value_index = 0; value_index < individuals_count; ++value_index)
			{
				Individual *ind = (Individual *)(individuals_value->ObjectElementAtIndex(value_index, nullptr));
				int shared_count = SharedParentCountWithIndividual(*ind);
				
				int_result->set_int_no_check(shared_count, value_index);
			}
		}
		else
		{
			for (int value_index = 0; value_index < individuals_count; ++value_index)
			{
				Individual *ind = (Individual *)(individuals_value->ObjectElementAtIndex(value_index, nullptr));
				int shared_count = (ind == this) ? 2.0 : 0.0;
				
				int_result->set_int_no_check(shared_count, value_index);
			}
		}
		
		return EidosValue_SP(int_result);
	}
	
	return gStaticEidosValueNULL;
}

//	*********************	- (integer$)sumOfMutationsOfType(io<MutationType>$ mutType)
//
EidosValue_SP Individual::ExecuteMethod_Accelerated_sumOfMutationsOfType(EidosObject **p_elements, size_t p_elements_size, EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	if (p_elements_size == 0)
		return gStaticEidosValue_Float_ZeroVec;
	
	// SPECIES CONSISTENCY CHECK
	Species *species = Community::SpeciesForIndividualsVector((Individual **)p_elements, (int)p_elements_size);
	
	if (species == nullptr)
		EIDOS_TERMINATION << "ERROR (Individual::ExecuteMethod_Accelerated_sumOfMutationsOfType): sumOfMutationsOfType() requires that mutType belongs to the same species as the target individual." << EidosTerminate();
	
	species->population_.CheckForDeferralInIndividualsVector((Individual **)p_elements, p_elements_size, "Individual::ExecuteMethod_Accelerated_sumOfMutationsOfType");
	
	EidosValue *mutType_value = p_arguments[0].get();
	MutationType *mutation_type_ptr = SLiM_ExtractMutationTypeFromEidosValue_io(mutType_value, 0, &species->community_, species, "sumOfMutationsOfType()");		// SPECIES CONSISTENCY CHECK
	
	// Count the number of mutations of the given type
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(p_elements_size);
	
	EIDOS_THREAD_COUNT(gEidos_OMP_threads_SUM_OF_MUTS_OF_TYPE);
#pragma omp parallel for schedule(dynamic, 1) default(none) shared(p_elements_size) firstprivate(p_elements, mut_block_ptr, mutation_type_ptr, float_result) if(p_elements_size >= EIDOS_OMPMIN_SUM_OF_MUTS_OF_TYPE) num_threads(thread_count)
	for (size_t element_index = 0; element_index < p_elements_size; ++element_index)
	{
		Individual *element = (Individual *)(p_elements[element_index]);
		Genome *genome1 = element->genome1_;
		Genome *genome2 = element->genome2_;
		double selcoeff_sum = 0.0;
		
		if (!genome1->IsNull())
		{
			int mutrun_count = genome1->mutrun_count_;
			
			for (int run_index = 0; run_index < mutrun_count; ++run_index)
			{
				const MutationRun *mutrun = genome1->mutruns_[run_index];
				int genome1_count = mutrun->size();
				const MutationIndex *genome1_ptr = mutrun->begin_pointer_const();
				
				for (int mut_index = 0; mut_index < genome1_count; ++mut_index)
				{
					Mutation *mut_ptr = mut_block_ptr + genome1_ptr[mut_index];
					
					if (mut_ptr->mutation_type_ptr_ == mutation_type_ptr)
						selcoeff_sum += mut_ptr->selection_coeff_;
				}
			}
		}
		if (!genome2->IsNull())
		{
			int mutrun_count = genome2->mutrun_count_;
			
			for (int run_index = 0; run_index < mutrun_count; ++run_index)
			{
				const MutationRun *mutrun = genome2->mutruns_[run_index];
				int genome2_count = mutrun->size();
				const MutationIndex *genome2_ptr = mutrun->begin_pointer_const();
				
				for (int mut_index = 0; mut_index < genome2_count; ++mut_index)
				{
					Mutation *mut_ptr = mut_block_ptr + genome2_ptr[mut_index];
					
					if (mut_ptr->mutation_type_ptr_ == mutation_type_ptr)
						selcoeff_sum += mut_ptr->selection_coeff_;
				}
			}
		}
		
		float_result->set_float_no_check(selcoeff_sum, element_index);
	}
	
	return EidosValue_SP(float_result);
}

//	*********************	- (object<Mutation>)uniqueMutationsOfType(io<MutationType>$ mutType)
//
EidosValue_SP Individual::ExecuteMethod_uniqueMutationsOfType(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	if (genome1_->IsDeferred() || genome2_->IsDeferred())
		EIDOS_TERMINATION << "ERROR (Individual::ExecuteMethod_uniqueMutationsOfType): the mutations of deferred genomes cannot be accessed." << EidosTerminate();
	
	EidosValue *mutType_value = p_arguments[0].get();
	
	Species &species = subpopulation_->species_;
	MutationType *mutation_type_ptr = SLiM_ExtractMutationTypeFromEidosValue_io(mutType_value, 0, &species.community_, &species, "uniqueMutationsOfType()");		// SPECIES CONSISTENCY CHECK
	
	// This code is adapted from uniqueMutations and follows its logic closely
	
	// We try to reserve a vector large enough to hold all the mutations; probably usually overkill, but it does little harm
	int genome1_size = (genome1_->IsNull() ? 0 : genome1_->mutation_count());
	int genome2_size = (genome2_->IsNull() ? 0 : genome2_->mutation_count());
	EidosValue_Object_vector *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Mutation_Class));
	EidosValue_SP result_SP = EidosValue_SP(vec);
	
	if ((genome1_size == 0) && (genome2_size == 0))
		return result_SP;
	
	if (genome1_size + genome2_size < 100)			// an arbitrary limit, but we don't want to make something *too* unnecessarily big...
		vec->reserve(genome1_size + genome2_size);	// since we do not always reserve, we have to use push_object_element() below to check
	
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	int mutrun_count = (genome1_size ? genome1_->mutrun_count_ : genome2_->mutrun_count_);
	
	for (int run_index = 0; run_index < mutrun_count; ++run_index)
	{
		// We want to interleave mutations from the two genomes, keeping only the uniqued mutations.  For a given position, we take mutations
		// from g1 first, and then look at the mutations in g2 at the same position and add them if they are not in g1.
		const MutationRun *mutrun1 = (genome1_size ? genome1_->mutruns_[run_index] : nullptr);
		const MutationRun *mutrun2 = (genome2_size ? genome2_->mutruns_[run_index] : nullptr);
		int g1_size = (mutrun1 ? mutrun1->size() : 0);
		int g2_size = (mutrun2 ? mutrun2->size() : 0);
		int g1_index = 0, g2_index = 0;
		
		if (g1_size && g2_size)
		{
			MutationIndex g1_mut = (*mutrun1)[g1_index], g2_mut = (*mutrun2)[g2_index];
			
			// At this point, we need to loop forward in g1 and g2 until we have found mutations of the right type in both
			while ((mut_block_ptr + g1_mut)->mutation_type_ptr_ != mutation_type_ptr)
			{
				if (++g1_index >= g1_size)
					break;
				g1_mut = (*mutrun1)[g1_index];
			}
			
			while ((mut_block_ptr + g2_mut)->mutation_type_ptr_ != mutation_type_ptr)
			{
				if (++g2_index >= g2_size)
					break;
				g2_mut = (*mutrun2)[g2_index];
			}
			
			if ((g1_index < g1_size) && (g2_index < g2_size))
			{
				slim_position_t pos1 = (mut_block_ptr + g1_mut)->position_;
				slim_position_t pos2 = (mut_block_ptr + g2_mut)->position_;
				
				// Process mutations as long as both genomes still have mutations left in them
				do
				{
					// Now we have mutations of the right type, so we can start working with them by position
					if (pos1 < pos2)
					{
						vec->push_object_element_RR(mut_block_ptr + g1_mut);
						
						// Move to the next mutation in g1
					loopback1:
						if (++g1_index >= g1_size)
							break;
						
						g1_mut = (*mutrun1)[g1_index];
						if ((mut_block_ptr + g1_mut)->mutation_type_ptr_ != mutation_type_ptr)
							goto loopback1;
						
						pos1 = (mut_block_ptr + g1_mut)->position_;
					}
					else if (pos1 > pos2)
					{
						vec->push_object_element_RR(mut_block_ptr + g2_mut);
						
						// Move to the next mutation in g2
					loopback2:
						if (++g2_index >= g2_size)
							break;
						
						g2_mut = (*mutrun2)[g2_index];
						if ((mut_block_ptr + g2_mut)->mutation_type_ptr_ != mutation_type_ptr)
							goto loopback2;
						
						pos2 = (mut_block_ptr + g2_mut)->position_;
					}
					else
					{
						// pos1 == pos2; copy mutations from g1 until we are done with this position, then handle g2
						slim_position_t focal_pos = pos1;
						int first_index = g1_index;
						bool done = false;
						
						while (pos1 == focal_pos)
						{
							vec->push_object_element_RR(mut_block_ptr + g1_mut);
							
							// Move to the next mutation in g1
						loopback3:
							if (++g1_index >= g1_size)
							{
								done = true;
								break;
							}
							g1_mut = (*mutrun1)[g1_index];
							if ((mut_block_ptr + g1_mut)->mutation_type_ptr_ != mutation_type_ptr)
								goto loopback3;
							
							pos1 = (mut_block_ptr + g1_mut)->position_;
						}
						
						// Note that we may be done with g1 here, so be careful
						int last_index_plus_one = g1_index;
						
						while (pos2 == focal_pos)
						{
							int check_index;
							
							for (check_index = first_index; check_index < last_index_plus_one; ++check_index)
								if ((*mutrun1)[check_index] == g2_mut)
									break;
							
							// If the check indicates that g2_mut is not in g1, we copy it over
							if (check_index == last_index_plus_one)
								vec->push_object_element_RR(mut_block_ptr + g2_mut);
							
							// Move to the next mutation in g2
						loopback4:
							if (++g2_index >= g2_size)
							{
								done = true;
								break;
							}
							g2_mut = (*mutrun2)[g2_index];
							if ((mut_block_ptr + g2_mut)->mutation_type_ptr_ != mutation_type_ptr)
								goto loopback4;
							
							pos2 = (mut_block_ptr + g2_mut)->position_;
						}
						
						// Note that we may be done with both g1 and/or g2 here; if so, done will be set and we will break out
						if (done)
							break;
					}
				}
				while (true);
			}
		}
		
		// Finish off any tail ends, which must be unique and sorted already
		while (g1_index < g1_size)
		{
			MutationIndex mut = (*mutrun1)[g1_index++];
			
			if ((mut_block_ptr + mut)->mutation_type_ptr_ == mutation_type_ptr)
				vec->push_object_element_RR(mut_block_ptr + mut);
		}
		while (g2_index < g2_size)
		{
			MutationIndex mut = (*mutrun2)[g2_index++];
			
			if ((mut_block_ptr + mut)->mutation_type_ptr_ == mutation_type_ptr)
				vec->push_object_element_RR(mut_block_ptr + mut);
		}
	}
	
	return result_SP;
	/*
	 A SLiM model to test the above code:
	 
	 initialize() {
	 initializeMutationRate(1e-5);
	 initializeMutationType("m1", 0.5, "f", 0.0);
	 initializeMutationType("m2", 0.5, "f", 0.0);
	 initializeMutationType("m3", 0.5, "f", 0.0);
	 initializeGenomicElementType("g1", c(m1, m2, m3), c(1.0, 1.0, 1.0));
	 initializeGenomicElement(g1, 0, 99999);
	 initializeRecombinationRate(1e-8);
	 }
	 1 early() {
	 sim.addSubpop("p1", 500);
	 }
	 1:20000 late() {
	 for (i in p1.individuals)
	 {
	 // check m1
	 um1 = i.uniqueMutationsOfType(m1);
	 um2 = sortBy(unique(i.genomes.mutationsOfType(m1)), "position");
	 
	 if (!identical(um1.position, um2.position))
	 {
	 print("Mismatch for m1!");
	 print(um1.position);
	 print(um2.position);
	 }
	 }
	 }
	 */
}


//
//	Individual_Class
//
#pragma mark -
#pragma mark Individual_Class
#pragma mark -

EidosClass *gSLiM_Individual_Class = nullptr;


const std::vector<EidosPropertySignature_CSP> *Individual_Class::Properties(void) const
{
	static std::vector<EidosPropertySignature_CSP> *properties = nullptr;
	
	if (!properties)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("Individual_Class::Properties(): not warmed up");
		
		properties = new std::vector<EidosPropertySignature_CSP>(*super::Properties());
		
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_subpopulation,			true,	kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_Subpopulation_Class))->DeclareAcceleratedGet(Individual::GetProperty_Accelerated_subpopulation));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_index,					true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Individual::GetProperty_Accelerated_index));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_genomes,				true,	kEidosValueMaskObject, gSLiM_Genome_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_genomesNonNull,			true,	kEidosValueMaskObject, gSLiM_Genome_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_genome1,				true,	kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_Genome_Class))->DeclareAcceleratedGet(Individual::GetProperty_Accelerated_genome1));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_genome2,				true,	kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_Genome_Class))->DeclareAcceleratedGet(Individual::GetProperty_Accelerated_genome2));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_sex,					true,	kEidosValueMaskString | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_tag,					false,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Individual::GetProperty_Accelerated_tag)->DeclareAcceleratedSet(Individual::SetProperty_Accelerated_tag));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_tagF,					false,	kEidosValueMaskFloat | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Individual::GetProperty_Accelerated_tagF)->DeclareAcceleratedSet(Individual::SetProperty_Accelerated_tagF));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_tagL0,					false,	kEidosValueMaskLogical | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Individual::GetProperty_Accelerated_tagL0)->DeclareAcceleratedSet(Individual::SetProperty_Accelerated_tagL0));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_tagL1,					false,	kEidosValueMaskLogical | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Individual::GetProperty_Accelerated_tagL1)->DeclareAcceleratedSet(Individual::SetProperty_Accelerated_tagL1));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_tagL2,					false,	kEidosValueMaskLogical | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Individual::GetProperty_Accelerated_tagL2)->DeclareAcceleratedSet(Individual::SetProperty_Accelerated_tagL2));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_tagL3,					false,	kEidosValueMaskLogical | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Individual::GetProperty_Accelerated_tagL3)->DeclareAcceleratedSet(Individual::SetProperty_Accelerated_tagL3));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_tagL4,					false,	kEidosValueMaskLogical | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Individual::GetProperty_Accelerated_tagL4)->DeclareAcceleratedSet(Individual::SetProperty_Accelerated_tagL4));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_migrant,				true,	kEidosValueMaskLogical | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Individual::GetProperty_Accelerated_migrant));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_fitnessScaling,			false,	kEidosValueMaskFloat | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Individual::GetProperty_Accelerated_fitnessScaling)->DeclareAcceleratedSet(Individual::SetProperty_Accelerated_fitnessScaling));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gEidosStr_x,					false,	kEidosValueMaskFloat | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Individual::GetProperty_Accelerated_x)->DeclareAcceleratedSet(Individual::SetProperty_Accelerated_x));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gEidosStr_y,					false,	kEidosValueMaskFloat | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Individual::GetProperty_Accelerated_y)->DeclareAcceleratedSet(Individual::SetProperty_Accelerated_y));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gEidosStr_z,					false,	kEidosValueMaskFloat | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Individual::GetProperty_Accelerated_z)->DeclareAcceleratedSet(Individual::SetProperty_Accelerated_z));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gEidosStr_xy,				true,	kEidosValueMaskFloat)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gEidosStr_xz,				true,	kEidosValueMaskFloat)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gEidosStr_yz,				true,	kEidosValueMaskFloat)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gEidosStr_xyz,				true,	kEidosValueMaskFloat)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_age,					false,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Individual::GetProperty_Accelerated_age)->DeclareAcceleratedSet(Individual::SetProperty_Accelerated_age));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_meanParentAge,			true,	kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_pedigreeID,				true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Individual::GetProperty_Accelerated_pedigreeID));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_pedigreeParentIDs,		true,	kEidosValueMaskInt)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_pedigreeGrandparentIDs,	true,	kEidosValueMaskInt)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_reproductiveOutput,		true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Individual::GetProperty_Accelerated_reproductiveOutput));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_spatialPosition,		true,	kEidosValueMaskFloat))->DeclareAcceleratedGet(Individual::GetProperty_Accelerated_spatialPosition));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_uniqueMutations,		true,	kEidosValueMaskObject, gSLiM_Mutation_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gEidosStr_color,				false,	kEidosValueMaskString | kEidosValueMaskSingleton))->DeclareAcceleratedSet(Individual::SetProperty_Accelerated_color));
		
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const std::vector<EidosMethodSignature_CSP> *Individual_Class::Methods(void) const
{
	static std::vector<EidosMethodSignature_CSP> *methods = nullptr;
	
	if (!methods)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("Individual_Class::Methods(): not warmed up");
		
		methods = new std::vector<EidosMethodSignature_CSP>(*super::Methods());
		
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_containsMutations, kEidosValueMaskLogical))->AddObject("mutations", gSLiM_Mutation_Class));
		methods->emplace_back(((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_countOfMutationsOfType, kEidosValueMaskInt | kEidosValueMaskSingleton))->AddIntObject_S("mutType", gSLiM_MutationType_Class))->DeclareAcceleratedImp(Individual::ExecuteMethod_Accelerated_countOfMutationsOfType));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_relatedness, kEidosValueMaskFloat))->AddObject("individuals", gSLiM_Individual_Class));
		methods->emplace_back((EidosClassMethodSignature *)(new EidosClassMethodSignature(gStr_setSpatialPosition, kEidosValueMaskVOID))->AddFloat("position"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_sharedParentCount, kEidosValueMaskInt))->AddObject("individuals", gSLiM_Individual_Class));
		methods->emplace_back(((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_sumOfMutationsOfType, kEidosValueMaskFloat | kEidosValueMaskSingleton))->AddIntObject_S("mutType", gSLiM_MutationType_Class))->DeclareAcceleratedImp(Individual::ExecuteMethod_Accelerated_sumOfMutationsOfType));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_uniqueMutationsOfType, kEidosValueMaskObject, gSLiM_Mutation_Class))->AddIntObject_S("mutType", gSLiM_MutationType_Class));
		
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}

EidosValue_SP Individual_Class::ExecuteClassMethod(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) const
{
	switch (p_method_id)
	{
		case gID_setSpatialPosition:	return ExecuteMethod_setSpatialPosition(p_method_id, p_target, p_arguments, p_interpreter);
		default:						return EidosDictionaryUnretained_Class::ExecuteClassMethod(p_method_id, p_target, p_arguments, p_interpreter);
	}
}

//	*********************	– (void)setSpatialPosition(float position)
//
EidosValue_SP Individual_Class::ExecuteMethod_setSpatialPosition(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) const
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *position_value = p_arguments[0].get();
	int dimensionality = 0;
	int value_count = position_value->Count();
	int target_size = p_target->Count();
	
	// Determine the spatiality of the individuals involved, and make sure it is the same for all
	if (target_size == 1)
	{
		Individual *target = (Individual *)p_target->ObjectElementAtIndex(0, nullptr);
		
		dimensionality = target->subpopulation_->species_.SpatialDimensionality();
	}
	else if (target_size > 1)
	{
		const EidosValue_Object_vector *target_vec = p_target->ObjectElementVector();
		Individual * const *targets = (Individual * const *)(target_vec->data());
		
		dimensionality = targets[0]->subpopulation_->species_.SpatialDimensionality();
		
		for (int target_index = 1; target_index < target_size; ++target_index)
		{
			Individual *target = targets[target_index];
			
			if (target->subpopulation_->species_.SpatialDimensionality() != dimensionality)
				EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_setSpatialPosition): setSpatialPosition() requires that all individuals in the target vector have the same spatial dimensionality." << EidosTerminate();
		}
	}
	
	if ((target_size > 0) && (dimensionality == 0))
		EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_setSpatialPosition): setSpatialPosition() cannot be called in non-spatial simulations." << EidosTerminate();
	if ((dimensionality < 0) || (dimensionality > 3))
		EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_setSpatialPosition): (internal error) unrecognized dimensionality." << EidosTerminate();
	
	if (value_count < dimensionality)
		EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_setSpatialPosition): setSpatialPosition() requires at least as many coordinates as the spatial dimensionality of the simulation." << EidosTerminate();
	
	if (value_count == dimensionality)
	{
		// One point is being set across all targets
		if (target_size == 1)
		{
			// Handle the singleton target case separately so we can handle the vector target case faster
			Individual *target = (Individual *)p_target->ObjectElementAtIndex(0, nullptr);
			
			switch (dimensionality)
			{
				case 1:
					target->spatial_x_ = position_value->FloatAtIndex(0, nullptr);
					break;
				case 2:
					target->spatial_x_ = position_value->FloatAtIndex(0, nullptr);
					target->spatial_y_ = position_value->FloatAtIndex(1, nullptr);
					break;
				case 3:
					target->spatial_x_ = position_value->FloatAtIndex(0, nullptr);
					target->spatial_y_ = position_value->FloatAtIndex(1, nullptr);
					target->spatial_z_ = position_value->FloatAtIndex(2, nullptr);
					break;
				default:
					EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_setSpatialPosition): (internal error) dimensionality out of range." << EidosTerminate(nullptr);
			}
		}
		else if (target_size > 1)
		{
			// Vector target case, one point
			const EidosValue_Object_vector *target_vec = p_target->ObjectElementVector();
			Individual * const *targets = (Individual * const *)(target_vec->data());
			
			switch (dimensionality)
			{
				case 1:
				{
					double x = position_value->FloatAtIndex(0, nullptr);
					
					EIDOS_THREAD_COUNT(gEidos_OMP_threads_SET_SPATIAL_POS_1_1D);
#pragma omp parallel for simd schedule(simd:static) default(none) shared(target_size) firstprivate(targets, x) if(target_size >= EIDOS_OMPMIN_SET_SPATIAL_POS_1_1D) num_threads(thread_count)
					for (int target_index = 0; target_index < target_size; ++target_index)
					{
						Individual *target = targets[target_index];
						target->spatial_x_ = x;
					}
					break;
				}
				case 2:
				{
					double x = position_value->FloatAtIndex(0, nullptr);
					double y = position_value->FloatAtIndex(1, nullptr);
					
					EIDOS_THREAD_COUNT(gEidos_OMP_threads_SET_SPATIAL_POS_1_2D);
#pragma omp parallel for simd schedule(simd:static) default(none) shared(target_size) firstprivate(targets, x, y) if(target_size >= EIDOS_OMPMIN_SET_SPATIAL_POS_1_2D) num_threads(thread_count)
					for (int target_index = 0; target_index < target_size; ++target_index)
					{
						Individual *target = targets[target_index];
						target->spatial_x_ = x;
						target->spatial_y_ = y;
					}
					break;
				}
				case 3:
				{
					double x = position_value->FloatAtIndex(0, nullptr);
					double y = position_value->FloatAtIndex(1, nullptr);
					double z = position_value->FloatAtIndex(2, nullptr);
					
					EIDOS_THREAD_COUNT(gEidos_OMP_threads_SET_SPATIAL_POS_1_3D);
#pragma omp parallel for simd schedule(simd:static) default(none) shared(target_size) firstprivate(targets, x, y, z) if(target_size >= EIDOS_OMPMIN_SET_SPATIAL_POS_1_3D) num_threads(thread_count)
					for (int target_index = 0; target_index < target_size; ++target_index)
					{
						Individual *target = targets[target_index];
						target->spatial_x_ = x;
						target->spatial_y_ = y;
						target->spatial_z_ = z;
					}
					break;
				}
				default:
					EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_setSpatialPosition): (internal error) dimensionality out of range." << EidosTerminate(nullptr);
			}
		}
	}
	else if (value_count == dimensionality * target_size)
	{
		// Vector target case, one point per target (so the point vector has to be non-singleton too)
		const EidosValue_Object_vector *target_vec = p_target->ObjectElementVector();
		Individual * const *targets = (Individual * const *)(target_vec->data());
		const EidosValue_Float_vector *position_vec = position_value->FloatVector();
		const double *positions = position_vec->data();
		
#ifdef _OPENMP
		if (((dimensionality == 1) && (target_size >= EIDOS_OMPMIN_SET_SPATIAL_POS_2_1D)) ||
			((dimensionality == 2) && (target_size >= EIDOS_OMPMIN_SET_SPATIAL_POS_2_2D)) ||
			((dimensionality == 3) && (target_size >= EIDOS_OMPMIN_SET_SPATIAL_POS_2_3D)))
		{
			switch (dimensionality)
			{
				case 1:
				{
					EIDOS_THREAD_COUNT(gEidos_OMP_threads_SET_SPATIAL_POS_2_1D);
#pragma omp parallel for schedule(static) default(none) shared(target_size) firstprivate(targets, positions) num_threads(thread_count) // if(EIDOS_OMPMIN_SET_SPATIAL_POS_2_1D) is above
					for (int target_index = 0; target_index < target_size; ++target_index)
					{
						targets[target_index]->spatial_x_ = positions[target_index];
					}
					break;
				}
				case 2:
				{
					EIDOS_THREAD_COUNT(gEidos_OMP_threads_SET_SPATIAL_POS_2_2D);
#pragma omp parallel for schedule(static) default(none) shared(target_size) firstprivate(targets, positions) num_threads(thread_count) // if(EIDOS_OMPMIN_SET_SPATIAL_POS_2_2D) is above
					for (int target_index = 0; target_index < target_size; ++target_index)
					{
						Individual *target = targets[target_index];
						const double *target_pos = positions + target_index * 2;
						
						target->spatial_x_ = target_pos[0];
						target->spatial_y_ = target_pos[1];
					}
					break;
				}
				case 3:
				{
					EIDOS_THREAD_COUNT(gEidos_OMP_threads_SET_SPATIAL_POS_2_3D);
#pragma omp parallel for schedule(static) default(none) shared(target_size) firstprivate(targets, positions) num_threads(thread_count) // if(EIDOS_OMPMIN_SET_SPATIAL_POS_2_3D) is above
					for (int target_index = 0; target_index < target_size; ++target_index)
					{
						Individual *target = targets[target_index];
						const double *target_pos = positions + target_index * 3;
						
						target->spatial_x_ = target_pos[0];
						target->spatial_y_ = target_pos[1];
						target->spatial_z_ = target_pos[2];
					}
					break;
				}
			}
		} else
#endif
		{
			switch (dimensionality)
			{
				case 1:
				{
					for (int target_index = 0; target_index < target_size; ++target_index)
					{
						Individual *target = targets[target_index];
						target->spatial_x_ = *(positions++);
					}
					break;
				}
				case 2:
				{
					for (int target_index = 0; target_index < target_size; ++target_index)
					{
						Individual *target = targets[target_index];
						target->spatial_x_ = *(positions++);
						target->spatial_y_ = *(positions++);
					}
					break;
				}
				case 3:
				{
					for (int target_index = 0; target_index < target_size; ++target_index)
					{
						Individual *target = targets[target_index];
						target->spatial_x_ = *(positions++);
						target->spatial_y_ = *(positions++);
						target->spatial_z_ = *(positions++);
					}
					break;
				}
				default:
					EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_setSpatialPosition): (internal error) dimensionality out of range." << EidosTerminate(nullptr);
			}
		}
	}
	else
	{
		EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_setSpatialPosition): setSpatialPosition() requires the position parameter to contain either one point, or one point per individual (where each point has a number of coordinates equal to the spatial dimensionality of the simulation)." << EidosTerminate();
	}
	
	return gStaticEidosValueVOID;
}			












































