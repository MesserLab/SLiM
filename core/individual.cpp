//
//  individual.cpp
//  SLiM
//
//  Created by Ben Haller on 6/10/16.
//  Copyright (c) 2016-2025 Benjamin C. Haller.  All rights reserved.
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
#include "polymorphism.h"

#include <string>
#include <algorithm>
#include <vector>
#include <cmath>
#include <fstream>
#include <utility>


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
bool Individual::s_any_haplosome_tag_set_ = false;
bool Individual::s_any_individual_fitness_scaling_set_ = false;


// individual first, haplosomes later; this is the new multichrom paradigm
// BCH 10/12/2024: Note that this will rarely be called after simulation startup; see NewSubpopIndividual()
Individual::Individual(Subpopulation *p_subpopulation, slim_popsize_t p_individual_index, IndividualSex p_sex, slim_age_t p_age, double p_fitness, float p_mean_parent_age) :
#ifdef SLIMGUI
	color_set_(false),
#endif
	mean_parent_age_(p_mean_parent_age), pedigree_id_(-1), pedigree_p1_(-1), pedigree_p2_(-1),
	pedigree_g1_(-1), pedigree_g2_(-1), pedigree_g3_(-1), pedigree_g4_(-1), reproductive_output_(0),
	sex_(p_sex), migrant_(false), killed_(false), cached_fitness_UNSAFE_(p_fitness),
#ifdef SLIMGUI
	cached_unscaled_fitness_(p_fitness),
#endif
	age_(p_age), index_(p_individual_index), subpopulation_(p_subpopulation)
{
	// Set up our haplosomes with nullptr values initially.  If we have 0/1/2 haplosomes total, we use our
	// internal buffer for speed; avoid malloc/free entirely, and even more important, get the memory
	// locality of having the haplosome pointers right inside the individual itself.  Otherwise, we alloc
	// an external buffer, which entails fetching a new cache line to go through the indirection.
	int haplosome_count_per_individual = subpopulation_->HaplosomeCountPerIndividual();
	
	if (haplosome_count_per_individual <= 2)
	{
		hapbuffer_[0] = nullptr;
		hapbuffer_[1] = nullptr;
		haplosomes_ = hapbuffer_;
	}
	else
	{
		haplosomes_ = (Haplosome **)calloc(haplosome_count_per_individual, sizeof(Haplosome *));
	}
	
	// Initialize tag values to the "unset" value
	tag_value_ = SLIM_TAG_UNSET_VALUE;
	tagF_value_ = SLIM_TAGF_UNSET_VALUE;
	tagL0_set_ = false;
	tagL1_set_ = false;
	tagL2_set_ = false;
	tagL3_set_ = false;
	tagL4_set_ = false;
	
	// Initialize x/y/z to 0.0, only when leak-checking (they show up as used before initialized in Valgrind)
#if SLIM_LEAK_CHECKING
	spatial_x_ = 0.0;
	spatial_y_ = 0.0;
	spatial_z_ = 0.0;
#endif
}

Individual::~Individual(void)
{
	// BCH 10/6/2024: Individual now owns the haplosomes inside it (a policy change for multichrom)
	// BCH 10/12/2024: Note that this might no longer be called except at simulation end; see FreeSubpopIndividual()
	Subpopulation *subpop = subpopulation_;
	
	// The subpopulation_ pointer is set to nullptr when an individual is placed in individuals_junkyard_;
	// in that case, its haplosomes have already been freed, so this loop does not need to run.
	if (subpopulation_)
	{
		const std::vector<Chromosome *> &chromosome_for_haplosome_index = subpopulation_->species_.ChromosomesForHaplosomeIndices();
		int haplosome_count_per_individual = subpop->HaplosomeCountPerIndividual();
		Haplosome **haplosomes = haplosomes_;
		
		for (int haplosome_index = 0; haplosome_index < haplosome_count_per_individual; ++haplosome_index)
		{
			Haplosome *haplosome = haplosomes[haplosome_index];
			
			// haplosome pointers can be nullptr, if an individual has already freed its haplosome objects;
			// this happens when an individual is placed in individuals_junkyard_, in particular
			if (haplosome)
			{
				Chromosome *chromosome = chromosome_for_haplosome_index[haplosome_index];
				
				chromosome->FreeHaplosome(haplosome);
			}
		}
	}
	
	if (haplosomes_ != hapbuffer_)
		free(haplosomes_);
	
#if DEBUG
	haplosomes_ = nullptr;
#endif
}

#if DEBUG
void Individual::AddHaplosomeAtIndex(Haplosome *p_haplosome, int p_index)
{
	int haplosome_count_per_individual = subpopulation_->HaplosomeCountPerIndividual();
	
	if ((p_index < 0) || (p_index >= haplosome_count_per_individual))
		EIDOS_TERMINATION << "ERROR (Individual::AddHaplosomeAtIndex): (internal error) haplosome index " << p_index << " out of range." << EidosTerminate();
	
	// in DEBUG haplosomes_ should be zero-filled; when not in DEBUG, it may not be!
	if (haplosomes_[p_index])
		EIDOS_TERMINATION << "ERROR (Individual::AddHaplosomeAtIndex): (internal error) haplosome index " << p_index << " already filled." << EidosTerminate();
	
	// the haplosome should already know that it belongs to the individual; this method just makes the individual aware of that
	if (p_haplosome->individual_ != this)
		EIDOS_TERMINATION << "ERROR (Individual::AddHaplosomeAtIndex): (internal error) haplosome individual_ pointer not set up." << EidosTerminate();
	
	haplosomes_[p_index] = p_haplosome;
}
#endif

void Individual::AppendHaplosomesForChromosomes(EidosValue_Object *vec, std::vector<slim_chromosome_index_t> &chromosome_indices, int64_t index, bool includeNulls)
{
	Species &species = subpopulation_->species_;
	
	for (slim_chromosome_index_t chromosome_index : chromosome_indices)
	{
		Chromosome *chromosome = species.Chromosomes()[chromosome_index];
		int first_haplosome_index = species.FirstHaplosomeIndices()[chromosome_index];
		
		switch (chromosome->Type())
		{
				// diploid chromosome types, where we will use index if supplied
			case ChromosomeType::kA_DiploidAutosome:
			case ChromosomeType::kX_XSexChromosome:
			case ChromosomeType::kZ_ZSexChromosome:
			{
				if ((index == -1) || (index == 0))
				{
					Haplosome *haplosome = haplosomes_[first_haplosome_index];
					
					if (includeNulls || !haplosome->IsNull())
						vec->push_object_element_NORR(haplosome);
				}
				if ((index == -1) || (index == 1))
				{
					Haplosome *haplosome = haplosomes_[first_haplosome_index+1];
					
					if (includeNulls || !haplosome->IsNull())
						vec->push_object_element_NORR(haplosome);
				}
				break;
			}
				
				// haploid chromosome types, where index is ignored
			case ChromosomeType::kH_HaploidAutosome:
			case ChromosomeType::kY_YSexChromosome:
			case ChromosomeType::kW_WSexChromosome:
			case ChromosomeType::kHF_HaploidFemaleInherited:
			case ChromosomeType::kFL_HaploidFemaleLine:
			case ChromosomeType::kHM_HaploidMaleInherited:
			case ChromosomeType::kML_HaploidMaleLine:
			case ChromosomeType::kHNull_HaploidAutosomeWithNull:	// the null is just ignored by this code
			{
				Haplosome *haplosome = haplosomes_[first_haplosome_index];
				
				if (includeNulls || !haplosome->IsNull())
					vec->push_object_element_NORR(haplosome);
				break;
			}
				
				// haploid chromosome types with a null haplosome first; index is ignored
			case ChromosomeType::kNullY_YSexChromosomeWithNull:
			{
				Haplosome *haplosome = haplosomes_[first_haplosome_index+1];	// the (possibly) non-null haplosome
				
				if (includeNulls || !haplosome->IsNull())
					vec->push_object_element_NORR(haplosome);
				break;
			}
		}
	}
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
								IndividualSex A_sex, IndividualSex B_sex, ChromosomeType p_chromosome_type)
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
	if ((p_chromosome_type != ChromosomeType::kA_DiploidAutosome) && ((A_sex == IndividualSex::kHermaphrodite) || (B_sex == IndividualSex::kHermaphrodite)))
		EIDOS_TERMINATION << "ERROR (Individual::_Relatedness): (internal error) hermaphrodites cannot exist when modeling a sex chromosome" << EidosTerminate();
	if (((A_sex == IndividualSex::kHermaphrodite) && (B_sex != IndividualSex::kHermaphrodite)) || ((A_sex != IndividualSex::kHermaphrodite) && (B_sex == IndividualSex::kHermaphrodite)))
		EIDOS_TERMINATION << "ERROR (Individual::_Relatedness): (internal error) hermaphrodites cannot coexist with males and females" << EidosTerminate();
	if (((A_sex == IndividualSex::kMale) && (B_P1 == A) && (B_P1 != B_P2)) ||
		((B_sex == IndividualSex::kMale) && (A_P1 == B) && (A_P1 != A_P2)) ||
		((A_sex == IndividualSex::kFemale) && (B_P2 == A) && (B_P2 != B_P1)) ||
		((B_sex == IndividualSex::kFemale) && (A_P2 == B) && (A_P2 != A_P1)))
		EIDOS_TERMINATION << "ERROR (Individual::_Relatedness): (internal error) a male was indicated as a first parent, or a female as second parent, without clonality" << EidosTerminate();
#endif
	
	switch (p_chromosome_type)
	{
		case ChromosomeType::kA_DiploidAutosome:
		case ChromosomeType::kH_HaploidAutosome:
		{
			// No intervention needed (we assume that inheritance was normal, without null haplosomes)
			// For "H", recombination is possible if there are two parents, so this is the same as "A"
			break;
		}
		case ChromosomeType::kHNull_HaploidAutosomeWithNull:
		{
			// For "H-", the second parent should always match the first (by cloning), but we make sure of it
			B_P1 = A_P1;
			B_P2 = A_P2;
			B_G1 = A_G1;
			B_G2 = A_G2;
			B_G3 = A_G3;
			B_G4 = A_G4;
			break;
		}
		case ChromosomeType::kX_XSexChromosome:
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
			
			break;
		}
		case ChromosomeType::kY_YSexChromosome:
		case ChromosomeType::kNullY_YSexChromosomeWithNull:
		case ChromosomeType::kML_HaploidMaleLine:
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
			break;
		}
		case ChromosomeType::kHM_HaploidMaleInherited:
		{
			// inherited from the male parent, so only the male (second) parents count
			// BCH 27 August 2025: Note that HM is now legal in non-sexual models; "male" just means "second"
			A_G3 = A_G4;
			A_P1 = A_P2;
			A_G1 = A_G3;
			A_G2 = A_G4;
			
			B_G3 = B_G4;
			B_P1 = B_P2;
			B_G1 = B_G3;
			B_G2 = B_G4;
			break;
		}
		case ChromosomeType::kZ_ZSexChromosome:
		{
			// Whichever sex A is, its first parent (A_P1) is female and so its female parent (A_G1) gave A_P1 a W, not a Z
			A_G1 = A_G2;
			
			if (A_sex == IndividualSex::kFemale)
			{
				// If A is female, its first parent (female) gave it a W, not a Z
				A_P1 = A_P2;
				A_G1 = A_G3;
				A_G2 = A_G4;
			}
			
			// Whichever sex B is, its first parent (B_P1) is female and so its female parent (B_G1) gave B_P1 a W, not a Z
			B_G1 = B_G2;
			
			if (B_sex == IndividualSex::kFemale)
			{
				// If B is female, its first parent (female) gave it a W, not a Z
				B_P1 = B_P2;
				B_G1 = B_G3;
				B_G2 = B_G4;
			}
			
			break;
		}
		case ChromosomeType::kW_WSexChromosome:
		case ChromosomeType::kFL_HaploidFemaleLine:
		{
			// When modeling the W, males have no relatedness to anybody else except themselves, defined as 1.0 for consistency
			if ((A_sex == IndividualSex::kMale) || (B_sex == IndividualSex::kMale))
			{
				if (A == B)
					return 1.0;
				return 0.0;
			}
			
			// The male parents (A_P2 and B_P2) and their parents, and male grandparents (A_G2 and B_G2), do not contribute
			A_G2 = A_G1;
			A_P2 = A_P1;
			A_G3 = A_G1;
			A_G4 = A_G2;
			
			B_G2 = B_G1;
			B_P2 = B_P1;
			B_G3 = B_G1;
			B_G4 = B_G2;
			break;
		}
		case ChromosomeType::kHF_HaploidFemaleInherited:
		{
			// inherited from the female parent, so only the female (first) parents count
			// BCH 27 August 2025: Note that HF is now legal in non-sexual models; "female" just means "first"
			A_G2 = A_G1;
			A_P2 = A_P1;
			A_G3 = A_G1;
			A_G4 = A_G2;
			
			B_G2 = B_G1;
			B_P2 = B_P1;
			B_G3 = B_G1;
			B_G4 = B_G2;
			break;
		}
	}
	
	return ::_Relatedness(A, A_P1, A_P2, A_G1, A_G2, A_G3, A_G4, B, B_P1, B_P2, B_G1, B_G2, B_G3, B_G4);
}

double Individual::RelatednessToIndividual(Individual &p_ind, ChromosomeType p_chromosome_type)
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
	
	return _Relatedness(A, A_P1, A_P2, A_G1, A_G2, A_G3, A_G4, B, B_P1, B_P2, B_G1, B_G2, B_G3, B_G4, indA.sex_, indB.sex_, p_chromosome_type);
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

// print a vector of individuals, with all mutations and all haplosomes, to a stream
// this takes a focal chromosome; if nullptr, data from all chromosomes is printed
void Individual::PrintIndividuals_SLiM(std::ostream &p_out, const Individual **p_individuals, int64_t p_individuals_count, Species &species, bool p_output_spatial_positions, bool p_output_ages, bool p_output_ancestral_nucs, bool p_output_pedigree_ids, bool p_output_object_tags, bool p_output_substitutions, Chromosome *p_focal_chromosome)
{
	Population &population = species.population_;
	Community &community = species.community_;
	
	if (population.child_generation_valid_)
		EIDOS_TERMINATION << "ERROR (Individual::PrintIndividuals_SLiM): (internal error) called with child generation active!." << EidosTerminate();
	
#if DO_MEMORY_CHECKS
	// This method can burn a huge amount of memory and get us killed, if we have a maximum memory usage.  It's nice to
	// try to check for that and terminate with a proper error message, to help the user diagnose the problem.
	int mem_check_counter = 0, mem_check_mod = 100;
	
	if (eidos_do_memory_checks)
		Eidos_CheckRSSAgainstMax("Individual::PrintIndividuals_SLiM", "(The memory usage was already out of bounds on entry.)");
#endif
	
	// this method now handles outputFull() as well as outputIndividuals()
	bool output_full_population = (p_individuals == nullptr);
	
	if (output_full_population)
	{
		// We need to set up an individuals vector that contains all individuals, so we can share code below
		int64_t total_population_size = 0;
		
		for (const std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population.subpops_)
		{
			Subpopulation *subpop = subpop_pair.second;
			slim_popsize_t subpop_size = subpop->parent_subpop_size_;
			
			total_population_size += subpop_size;
		}
		
		p_individuals = (const Individual **)malloc(total_population_size * sizeof(Individual *));
		p_individuals_count = total_population_size;
		
		const Individual **ind_buffer_ptr = p_individuals;
		
		for (const std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population.subpops_)
		{
			Subpopulation *subpop = subpop_pair.second;
			slim_popsize_t subpop_size = subpop->parent_subpop_size_;
			
			for (slim_popsize_t i = 0; i < subpop_size; i++)				// go through all children
				*(ind_buffer_ptr++) = subpop->parent_individuals_[i];
		}
	}
	
	// write the #OUT line
	p_out << "#OUT: " << community.Tick() << " " << species.Cycle() << (output_full_population ? " A" : " IS") << std::endl;
	
	// Figure out spatial position output.  If it was not requested, then we don't do it, and that's fine.  If it
	// was requested, then we output the number of spatial dimensions we're configured for (which might be zero).
	int spatial_output_count = (p_output_spatial_positions ? species.SpatialDimensionality() : 0);
	
	// Figure out age output.  If it was not requested, don't do it; if it was requested, do it if we use a nonWF model.
	int age_output_count = (p_output_ages && (species.model_type_ == SLiMModelType::kModelTypeNonWF)) ? 1 : 0;
	
	// Starting in SLiM 2.3, we output a version indicator at the top of the file so we can decode different
	// versions, etc.  Starting in SLiM 5, the version number is again synced with PrintAllBinary() (skipping
	// over 7 directly to 8), and the crazy four-way version number scheme that encoded flags is gone. See
	// PrintAllBinary() for the version history; but with version 8 we break backward compatibility anyway.
	p_out << "Version: 8" << std::endl;
	
	// Starting with version 8 (SLiM 5.0), we write out some flags; this information used to be incorporated into
	// the version number, which was gross.  Now we write out flags for all optional output that is enabled.
	// Reading code can assume that if a flag is not present, that output is not present.
	bool has_nucleotides = species.IsNucleotideBased();
	bool output_ancestral_nucs = has_nucleotides && p_output_ancestral_nucs;
	
	p_out << "Flags:";
	if (spatial_output_count)		p_out << " SPACE=" << spatial_output_count;
	if (age_output_count)			p_out << " AGES";
	if (p_output_pedigree_ids)		p_out << " PEDIGREES";
	if (has_nucleotides)			p_out << " NUC";
	if (output_ancestral_nucs)		p_out << " ANC_SEQ";
	if (p_output_object_tags)		p_out << " OBJECT_TAGS";
	if (p_output_substitutions)		p_out << " SUBSTITUTIONS";
	p_out << std::endl;
	
	// Output populations first, for outputFull() only
	if (output_full_population)
	{
		p_out << "Populations:" << std::endl;
		for (const std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population.subpops_)
		{
			Subpopulation *subpop = subpop_pair.second;
			slim_popsize_t subpop_size = subpop->parent_subpop_size_;
			double subpop_sex_ratio;
			
			if (species.model_type_ == SLiMModelType::kModelTypeWF)
			{
				subpop_sex_ratio = subpop->parent_sex_ratio_;
			}
			else
			{
				// We want to output empty (but not removed) subpops, so we use a sex ratio of 0.0 to prevent div by 0
				if (subpop->parent_subpop_size_ == 0)
					subpop_sex_ratio = 0.0;
				else
					subpop_sex_ratio = 1.0 - (subpop->parent_first_male_index_ / (double)subpop->parent_subpop_size_);
			}
			
			p_out << "p" << subpop_pair.first << " " << subpop_size;
			
			// SEX ONLY
			if (subpop->sex_enabled_)
				p_out << " S " << subpop_sex_ratio;
			else
				p_out << " H";
			
			if (p_output_object_tags)
			{
				if (subpop->tag_value_ == SLIM_TAG_UNSET_VALUE)
					p_out << " ?";
				else
					p_out << ' ' << subpop->tag_value_;
			}
			
			p_out << std::endl;
			
#if DO_MEMORY_CHECKS
			if (eidos_do_memory_checks)
			{
				mem_check_counter++;
				
				if (mem_check_counter % mem_check_mod == 0)
					Eidos_CheckRSSAgainstMax("Individual::PrintIndividuals_SLiM", "(Out of memory while outputting population list.)");
			}
#endif
		}
	}
	
	// print all individuals; this used to come after the Mutations: section, but now mutations are per-chromosome,
	// whereas the list of individuals is invariant across all of the chromosomes printed, and so must come before
	p_out << "Individuals:" << std::endl;
	
	THREAD_SAFETY_IN_ACTIVE_PARALLEL("Individual::PrintIndividuals_SLiM(): usage of statics");
	static char double_buf[40];
	
	for (int64_t individual_index = 0; individual_index < p_individuals_count; ++individual_index)
	{
		const Individual &individual = *(p_individuals[individual_index]);
		Subpopulation *subpop = individual.subpopulation_;
		slim_popsize_t index_in_subpop = individual.index_;
		
		if (!subpop || (index_in_subpop == -1))
		{
			if (output_full_population)
				free(p_individuals);
			
			EIDOS_TERMINATION << "ERROR (Individual::PrintIndividuals_SLiM): target individuals must be visible in a subpopulation (i.e., may not be new juveniles)." << EidosTerminate();
		}
		
		p_out << "p" << subpop->subpopulation_id_ << ":i" << index_in_subpop;						// individual identifier
		
		// BCH 9/13/2020: adding individual pedigree IDs, for SLiM 3.5, format version 5/6
		if (p_output_pedigree_ids)
			p_out << " " << individual.PedigreeID();
		
		p_out << ' ' << individual.sex_;
		
		// BCH 2/5/2025: Before version 8, we emitted haplosome identifiers here, like "p1:16" and
		// "p1:17", but now that we have multiple chromosomes that really isn't helpful; removing
		// them.  In the Haplosomes section we will now just identify the individual; that suffices.
		
		// output spatial position if requested; BCH 22 March 2019 switch to full precision for this, for accurate reloading
		if (spatial_output_count)
		{
			if (spatial_output_count >= 1)
			{
				snprintf(double_buf, 40, "%.*g", EIDOS_DBL_DIGS, individual.spatial_x_);		// necessary precision for non-lossiness
				p_out << " " << double_buf;
			}
			if (spatial_output_count >= 2)
			{
				snprintf(double_buf, 40, "%.*g", EIDOS_DBL_DIGS, individual.spatial_y_);		// necessary precision for non-lossiness
				p_out << " " << double_buf;
			}
			if (spatial_output_count >= 3)
			{
				snprintf(double_buf, 40, "%.*g", EIDOS_DBL_DIGS, individual.spatial_z_);		// necessary precision for non-lossiness
				p_out << " " << double_buf;
			}
		}
		
		// output ages if requested
		if (age_output_count)
			p_out << " " << individual.age_;
		
		// output individual tags if requested
		if (p_output_object_tags)
		{
			if (individual.tag_value_ == SLIM_TAG_UNSET_VALUE)
				p_out << " ?";
			else
				p_out << ' ' << individual.tag_value_;
			
			if (individual.tagF_value_ == SLIM_TAGF_UNSET_VALUE)
				p_out << " ?";
			else
			{
				snprintf(double_buf, 40, "%.*g", EIDOS_DBL_DIGS, individual.tagF_value_);		// necessary precision for non-lossiness
				p_out << " " << double_buf;
			}
			
			if (individual.tagL0_set_)
				p_out << ' ' << (individual.tagL0_value_ ? 'T' : 'F');
			else
				p_out << " ?";
			
			if (individual.tagL1_set_)
				p_out << ' ' << (individual.tagL1_value_ ? 'T' : 'F');
			else
				p_out << " ?";
			
			if (individual.tagL2_set_)
				p_out << ' ' << (individual.tagL2_value_ ? 'T' : 'F');
			else
				p_out << " ?";
			
			if (individual.tagL3_set_)
				p_out << ' ' << (individual.tagL3_value_ ? 'T' : 'F');
			else
				p_out << " ?";
			
			if (individual.tagL4_set_)
				p_out << ' ' << (individual.tagL4_value_ ? 'T' : 'F');
			else
				p_out << " ?";
		}
		
		p_out << std::endl;
		
#if DO_MEMORY_CHECKS
		if (eidos_do_memory_checks)
		{
			mem_check_counter++;
			
			if (mem_check_counter % mem_check_mod == 0)
				Eidos_CheckRSSAgainstMax("Population::PrintAll", "(Out of memory while printing individuals.)");
		}
#endif
	}
	
	// Loop over chromosomes and output data for each
	const std::vector<Chromosome *> &chromosomes = species.Chromosomes();
	
	for (Chromosome *chromosome : chromosomes)
	{
		// if we have a focal chromosome, skip all the other chromosomes
		if (p_focal_chromosome && (chromosome != p_focal_chromosome))
			continue;
		
		// write information about the chromosome; note that we write the chromosome symbol, but PrintAllBinary() does not
		slim_chromosome_index_t chromosome_index = chromosome->Index();
		
		p_out << "Chromosome: " << (uint32_t)chromosome_index << " " << chromosome->Type() << " " << chromosome->ID() << " " << chromosome->last_position_ << " \"" << chromosome->Symbol() << "\"";
		
		if (p_output_object_tags)
		{
			if (chromosome->tag_value_ == SLIM_TAG_UNSET_VALUE)
				p_out << " ?";
			else
				p_out << ' ' << chromosome->tag_value_;
		}
		
		p_out << std::endl;
		
		int first_haplosome_index = species.FirstHaplosomeIndices()[chromosome_index];
		int last_haplosome_index = species.LastHaplosomeIndices()[chromosome_index];
		PolymorphismMap polymorphisms;
		Mutation *mut_block_ptr = gSLiM_Mutation_Block;
		
		// add all polymorphisms for this chromosome
		for (int64_t individual_index = 0; individual_index < p_individuals_count; ++individual_index)
		{
			const Individual *ind = p_individuals[individual_index];
			Haplosome **haplosomes = ind->haplosomes_;
			
			for (int haplosome_index = first_haplosome_index; haplosome_index <= last_haplosome_index; haplosome_index++)
			{
				Haplosome *haplosome = haplosomes[haplosome_index];
				
				int mutrun_count = haplosome->mutrun_count_;
				
				for (int run_index = 0; run_index < mutrun_count; ++run_index)
				{
					const MutationRun *mutrun = haplosome->mutruns_[run_index];
					int mut_count = mutrun->size();
					const MutationIndex *mut_ptr = mutrun->begin_pointer_const();
					
					for (int mut_index = 0; mut_index < mut_count; ++mut_index)
						AddMutationToPolymorphismMap(&polymorphisms, mut_block_ptr + mut_ptr[mut_index]);
				}
				
#if DO_MEMORY_CHECKS
				if (eidos_do_memory_checks)
				{
					mem_check_counter++;
					
					if (mem_check_counter % mem_check_mod == 0)
						Eidos_CheckRSSAgainstMax("Population::PrintAll", "(Out of memory while assembling polymorphisms.)");
				}
#endif
			}
		}
		
		// print all polymorphisms
		p_out << "Mutations:"  << std::endl;
		
		for (const PolymorphismPair &polymorphism_pair : polymorphisms)
		{
			// NOTE this added mutation_id_, BCH 11 June 2016
			// NOTE the output format changed due to the addition of the nucleotide, BCH 2 March 2019
			if (p_output_object_tags)
				polymorphism_pair.second.Print_ID_Tag(p_out);
			else
				polymorphism_pair.second.Print_ID(p_out);
			
#if DO_MEMORY_CHECKS
			if (eidos_do_memory_checks)
			{
				mem_check_counter++;
				
				if (mem_check_counter % mem_check_mod == 0)
					Eidos_CheckRSSAgainstMax("Population::PrintAll", "(Out of memory while printing polymorphisms.)");
			}
#endif
		}
		
		// print all haplosomes
		p_out << "Haplosomes:" << std::endl;
		
		for (int64_t individual_index = 0; individual_index < p_individuals_count; ++individual_index)
		{
			const Individual *ind = p_individuals[individual_index];
			Haplosome **haplosomes = ind->haplosomes_;
			
			for (int haplosome_index = first_haplosome_index; haplosome_index <= last_haplosome_index; haplosome_index++)
			{
				Haplosome *haplosome = haplosomes[haplosome_index];
				
				// i used to be the haplosome index, now it is the individual index; we will have one or
				// two lines with this individual index, depending on the intrinsic ploidy of the chromosome
				// since we changed from a haplosome index to an individual index, we now emit an "i",
				// just follow the same convention as the Individuals section
				p_out << "p" << ind->subpopulation_->subpopulation_id_ << ":i" << ind->index_;
				
				if (p_output_object_tags)
				{
					if (haplosome->tag_value_ == SLIM_TAG_UNSET_VALUE)
						p_out << " ?";
					else
						p_out << ' ' << haplosome->tag_value_;
				}
				
				if (haplosome->IsNull())
				{
					p_out << " <null>";
				}
				else
				{
					int mutrun_count = haplosome->mutrun_count_;
					
					for (int run_index = 0; run_index < mutrun_count; ++run_index)
					{
						const MutationRun *mutrun = haplosome->mutruns_[run_index];
						int mut_count = mutrun->size();
						const MutationIndex *mut_ptr = mutrun->begin_pointer_const();
						
						for (int mut_index = 0; mut_index < mut_count; ++mut_index)
						{
							slim_polymorphismid_t polymorphism_id = FindMutationInPolymorphismMap(polymorphisms, mut_block_ptr + mut_ptr[mut_index]);
							
							if (polymorphism_id == -1)
								EIDOS_TERMINATION << "ERROR (Population::PrintAll): (internal error) polymorphism not found." << EidosTerminate();
							
							p_out << " " << polymorphism_id;
						}
					}
				}
				
				p_out << std::endl;
				
#if DO_MEMORY_CHECKS
				if (eidos_do_memory_checks)
				{
					mem_check_counter++;
					
					if (mem_check_counter % mem_check_mod == 0)
						Eidos_CheckRSSAgainstMax("Population::PrintAll", "(Out of memory while printing haplosomes.)");
				}
#endif
			}
		}
		
		// print ancestral sequence
		if (output_ancestral_nucs)
		{
			p_out << "Ancestral sequence:" << std::endl;
			p_out << *(chromosome->AncestralSequence());
			
			// operator<< above ends with a newline; here we add another, which the read code
			// can use to recognize that the nucleotide sequence has ended, even without an EOF
			p_out << std::endl;
		}
	}
	
	// Output substitutions at the end if requested; see Species::ExecuteMethod_outputFixedMutations()
	if (output_full_population && p_output_substitutions)
	{
		p_out << "Substitutions:" << std::endl;
		
		std::vector<Substitution*> &subs = population.substitutions_;
		
		for (unsigned int i = 0; i < subs.size(); i++)
		{
			p_out << i << " ";
			
			if (p_output_object_tags)
				subs[i]->PrintForSLiMOutput_Tag(p_out);
			else
				subs[i]->PrintForSLiMOutput(p_out);
			
#if DO_MEMORY_CHECKS
			if (eidos_do_memory_checks)
			{
				mem_check_counter++;
				
				if (mem_check_counter % mem_check_mod == 0)
					Eidos_CheckRSSAgainstMax("Species::ExecuteMethod_outputFixedMutations", "(outputFixedMutations(): Out of memory while outputting substitution objects.)");
			}
#endif
		}
	}
	
	// if we malloced a buffer of individuals above, free it now
	if (output_full_population)
		free(p_individuals);
}

void Individual::PrintIndividuals_VCF(std::ostream &p_out, const Individual **p_individuals, int64_t p_individuals_count, Species &p_species, bool p_output_multiallelics, bool p_simplify_nucs, bool p_output_nonnucs, Chromosome *p_focal_chromosome)
{
	const std::vector<Chromosome *> &chromosomes = p_species.Chromosomes();
	bool nucleotide_based = p_species.IsNucleotideBased();
	bool pedigrees_enabled = p_species.PedigreesEnabledByUser();
	
	// print the VCF header
	p_out << "##fileformat=VCFv4.2" << std::endl;
	
	{
		time_t rawtime;
		struct tm timeinfo;
		char buffer[25];	// should never be more than 10, in fact, plus a null
		
		time(&rawtime);
		localtime_r(&rawtime, &timeinfo);
		strftime(buffer, 25, "%Y%m%d", &timeinfo);
		
		p_out << "##fileDate=" << std::string(buffer) << std::endl;
	}
	
	p_out << "##source=SLiM" << std::endl;
	
	// BCH 2/11/2025: Unlike Haplosome::PrintHaplosomes_VCF(), we can print individual pedigree IDs,
	// since we are working with a vector of individuals, not a vector of haplosomes.
	if (pedigrees_enabled && (p_individuals_count > 0))
	{
		p_out << "##slimIndividualPedigreeIDs=";
		
		for (int64_t individual_index = 0; individual_index < p_individuals_count; ++individual_index)
		{
			if (individual_index > 0)
				p_out << ",";
			p_out << p_individuals[individual_index]->pedigree_id_;
		}
		
		p_out << std::endl;
	}
	
	// BCH 6 March 2019: Note that all of the INFO fields that provide per-mutation information have been
	// changed from a Number of 1 to a Number of ., since in nucleotide-based models we can call more than
	// one allele in a single call line (unlike in non-nucleotide-based models).
	p_out << "##INFO=<ID=MID,Number=.,Type=Integer,Description=\"Mutation ID in SLiM\">" << std::endl;
	p_out << "##INFO=<ID=S,Number=.,Type=Float,Description=\"Selection Coefficient\">" << std::endl;
	p_out << "##INFO=<ID=DOM,Number=.,Type=Float,Description=\"Dominance\">" << std::endl;
	// Note that at present we do not output the hemizygous dominance coefficient; too edge
	p_out << "##INFO=<ID=PO,Number=.,Type=Integer,Description=\"Population of Origin\">" << std::endl;
	p_out << "##INFO=<ID=TO,Number=.,Type=Integer,Description=\"Tick of Origin\">" << std::endl;			// changed to ticks for 4.0, and changed "GO" to "TO"
	p_out << "##INFO=<ID=MT,Number=.,Type=Integer,Description=\"Mutation Type\">" << std::endl;
	p_out << "##INFO=<ID=AC,Number=.,Type=Integer,Description=\"Allele Count\">" << std::endl;
	p_out << "##INFO=<ID=DP,Number=1,Type=Integer,Description=\"Total Depth\">" << std::endl;
	if (p_output_multiallelics && !nucleotide_based)
		p_out << "##INFO=<ID=MULTIALLELIC,Number=0,Type=Flag,Description=\"Multiallelic\">" << std::endl;
	if (nucleotide_based)
		p_out << "##INFO=<ID=AA,Number=1,Type=String,Description=\"Ancestral Allele\">" << std::endl;
	if (p_output_nonnucs && nucleotide_based)
		p_out << "##INFO=<ID=NONNUC,Number=0,Type=Flag,Description=\"Non-nucleotide-based\">" << std::endl;
	p_out << "##FORMAT=<ID=GT,Number=1,Type=String,Description=\"Genotype\">" << std::endl;
	p_out << "##contig=<ID=1,URL=https://github.com/MesserLab/SLiM>" << std::endl;
	p_out << "#CHROM\tPOS\tID\tREF\tALT\tQUAL\tFILTER\tINFO\tFORMAT";
	
	// When printing individual identifiers, we print the actual identifiers like p1:i17,
	// unlike Population::PrintSample_VCF() and Haplosome_Class::ExecuteMethod_outputX()
	for (slim_popsize_t individual_index = 0; individual_index < p_individuals_count; individual_index++)
	{
		const Individual &ind = *p_individuals[individual_index];
		slim_popsize_t index_in_subpop = ind.index_;
		Subpopulation *subpop = ind.subpopulation_;
		
		if (!subpop || (index_in_subpop == -1))
			EIDOS_TERMINATION << "ERROR (Individual::PrintIndividuals_VCF): target individuals must be visible in a subpopulation (i.e., may not be a new juvenile)." << EidosTerminate();
		
		p_out << "\tp" << subpop->subpopulation_id_ << ":i" << index_in_subpop;
	}
	p_out << std::endl;
	
	for (Chromosome *chromosome : chromosomes)
	{
		if (p_focal_chromosome && (chromosome != p_focal_chromosome))
			continue;
		
		slim_chromosome_index_t chromosome_index = chromosome->Index();
		int intrinsic_ploidy = chromosome->IntrinsicPloidy();
		int first_haplosome_index_ = p_species.FirstHaplosomeIndices()[chromosome_index];
		int last_haplosome_index_ = p_species.LastHaplosomeIndices()[chromosome_index];
		int64_t haplosome_count = p_individuals_count * intrinsic_ploidy;
		
		// assemble a vector of haplosomes, allowing us to share code with Haplosome::PrintHaplosomes_VCF()
		const Haplosome **haplosomes_buffer = (const Haplosome **)malloc(haplosome_count * sizeof(Haplosome *));
		int64_t haplosome_buffer_index = 0;
		
		for (int64_t individual_index = 0; individual_index < p_individuals_count; ++individual_index)
		{
			const Individual &ind = *p_individuals[individual_index];
			
			for (slim_popsize_t i = first_haplosome_index_; i <= last_haplosome_index_; i++)
				haplosomes_buffer[haplosome_buffer_index++] = ind.haplosomes_[i];
		}
		
		Haplosome::_PrintVCF(p_out, haplosomes_buffer, haplosome_count, *chromosome,
							 /* p_groupAsIndividuals*/ true,
							 p_simplify_nucs,
							 p_output_nonnucs,
							 p_output_multiallelics);
	}
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
	self_value_ = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(this, gSLiM_Individual_Class));
}

const EidosClass *Individual::Class(void) const
{
	return gSLiM_Individual_Class;
}

void Individual::Print(std::ostream &p_ostream) const
{
	if (killed_)
		p_ostream << Class()->ClassNameForDisplay() << "<KILLED>";
	else
		p_ostream << Class()->ClassNameForDisplay() << "<p" << subpopulation_->subpopulation_id_ << ":i" << index_ << ">";
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
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(subpopulation_, gSLiM_Subpopulation_Class));
		}
		case gID_index:				// ACCELERATED
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(index_));
		}
		case gID_haplosomes:				// ACCELERATED
		{
			int haplosome_count_per_individual = subpopulation_->HaplosomeCountPerIndividual();
			EidosValue_Object *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Haplosome_Class))->resize_no_initialize(haplosome_count_per_individual);
			Haplosome **haplosomes = haplosomes_;
			
			for (int haplosome_index = 0; haplosome_index < haplosome_count_per_individual; haplosome_index++)
			{
				Haplosome *haplosome = haplosomes[haplosome_index];
				
				vec->set_object_element_no_check_NORR(haplosome, haplosome_index);
			}
			
			return EidosValue_SP(vec);
		}
		case gID_haplosomesNonNull:			// ACCELERATED
		{
			int haplosome_count_per_individual = subpopulation_->HaplosomeCountPerIndividual();
			EidosValue_Object *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Haplosome_Class))->reserve(haplosome_count_per_individual);
			Haplosome **haplosomes = haplosomes_;
			
			for (int haplosome_index = 0; haplosome_index < haplosome_count_per_individual; haplosome_index++)
			{
				Haplosome *haplosome = haplosomes[haplosome_index];
				
				if (!haplosome->IsNull())
					vec->push_object_element_no_check_NORR(haplosome);
			}
			
			return EidosValue_SP(vec);
		}
		case gID_haploidGenome1:			// ACCELERATED
		case gID_haploidGenome1NonNull:		// ACCELERATED
			/*
			 A vector of all Haplosome objects associated with this individual that are attributed to its first parent (the female parent, in sexual models).  This method assumes the individual was generated by the typical method for each chromosome type, as explained below; it does not trace back the true ancestry of each haplosome.  The semantics of this are more obvious for some chromosome types than others, depending on the inheritance pattern of the chromosome as described in initializeChromosome().  For chromosomes with two associated haplosomes (types "A", "X", "Z", "H-", and "-Y"), the first haplosome is assumed to be from the first parent, and is thus included, whereas the second haplosome is assumed to be from the second parent and is thus not included.  For chromosomes with one associated haplosome that is inherited from the female parent in one way or another (types "W", "HF", and "FL"), that haplosome is always included.  For type "H", the single haplosome is assumed to have come from the first parent (since clonal inheritance is the common case), and so is included.  Other chromosome types ("Y", "HM", "ML") are never included.  See also the haploidGenome1NonNull property and the haplosomesForChromosomes() method.
			 */
		{
			bool allowNullHaplosomes = (p_property_id == gID_haploidGenome1);
			
			// the semantics of this property assume that the individual was generated by a biparental cross
			int haplosome_count_per_individual = subpopulation_->HaplosomeCountPerIndividual();
			EidosValue_Object *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Haplosome_Class))->reserve(haplosome_count_per_individual);
			Haplosome **haplosomes = haplosomes_;
			int haplosome_index = 0;
			
			for (Chromosome *chromosome : subpopulation_->species_.Chromosomes())
			{
				switch (chromosome->Type())
				{
						// chromosomes that have two haplosomes, the first of which is from the first parent
					case ChromosomeType::kA_DiploidAutosome:
					case ChromosomeType::kX_XSexChromosome:
					case ChromosomeType::kZ_ZSexChromosome:
					case ChromosomeType::kNullY_YSexChromosomeWithNull:
					case ChromosomeType::kHNull_HaploidAutosomeWithNull:	// assumed to follow the same pattern
					{
						Haplosome *haplosome = haplosomes[haplosome_index];
						
						if (allowNullHaplosomes || !haplosome->IsNull())
							vec->push_object_element_no_check_NORR(haplosome);
						
						haplosome_index += 2;
						break;
					}
						// chromosomes that have one haplosome, from the female parent
					case ChromosomeType::kW_WSexChromosome:
					case ChromosomeType::kHF_HaploidFemaleInherited:
					case ChromosomeType::kFL_HaploidFemaleLine:
					case ChromosomeType::kH_HaploidAutosome:				// assumed to follow the same pattern, so included
					{
						Haplosome *haplosome = haplosomes[haplosome_index];
						
						if (allowNullHaplosomes || !haplosome->IsNull())
							vec->push_object_element_no_check_NORR(haplosome);
						
						haplosome_index += 1;
						break;
					}
						// chromosomes that have one haplosome, from the male parent
					case ChromosomeType::kY_YSexChromosome:
					case ChromosomeType::kHM_HaploidMaleInherited:
					case ChromosomeType::kML_HaploidMaleLine:
					{
						haplosome_index += 1;
						break;
					}
				}
			}
			
			return EidosValue_SP(vec);
		}
		case gID_haploidGenome2:			// ACCELERATED
		case gID_haploidGenome2NonNull:		// ACCELERATED
			/*
			 A vector of all Haplosome objects associated with this individual that are attributed to its second parent (the male parent, in sexual models).  This method assumes the individual was generated by the typical method for each chromosome type, as explained below; it does not trace back the true ancestry of each haplosome.  The semantics of this are more obvious for some chromosome types than others, depending on the inheritance pattern of the chromosome as described in initializeChromosome().  For chromosomes with two associated haplosomes (types "A", "X", "Z", "H-", and"-Y"), the second haplosome is assumed to be from the second parent, and is thus included, whereas the first haplosome is assumed to be from the first parent and is thus not included.  For chromosomes with one associated haplosome that is inherited from the male parent in one way or another (types "Y", "HM", and "ML"), that haplosome is always included.  For type "H", the single haplosome is assumed to have come from the first parent (since clonal inheritance is the common case), and so is not included.  Other chromosome types ("W", "HF", "FL") are never included.  See also the haploidGenome2NonNull property and the haplosomesForChromosomes() method.
			 */
		{
			bool allowNullHaplosomes = (p_property_id == gID_haploidGenome2);
			
			// the semantics of this property assume that the individual was generated by a biparental cross
			int haplosome_count_per_individual = subpopulation_->HaplosomeCountPerIndividual();
			EidosValue_Object *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Haplosome_Class))->reserve(haplosome_count_per_individual);
			Haplosome **haplosomes = haplosomes_;
			int haplosome_index = 0;
			
			for (Chromosome *chromosome : subpopulation_->species_.Chromosomes())
			{
				switch (chromosome->Type())
				{
						// chromosomes that have two haplosomes, the second of which is from the second parent
					case ChromosomeType::kA_DiploidAutosome:
					case ChromosomeType::kX_XSexChromosome:
					case ChromosomeType::kZ_ZSexChromosome:
					case ChromosomeType::kNullY_YSexChromosomeWithNull:
					case ChromosomeType::kHNull_HaploidAutosomeWithNull:	// assumed to follow the same pattern
					{
						Haplosome *haplosome = haplosomes[haplosome_index+1];
						
						if (allowNullHaplosomes || !haplosome->IsNull())
							vec->push_object_element_no_check_NORR(haplosome);
						
						haplosome_index += 2;
						break;
					}
						// chromosomes that have one haplosome, from the female parent
					case ChromosomeType::kW_WSexChromosome:
					case ChromosomeType::kHF_HaploidFemaleInherited:
					case ChromosomeType::kFL_HaploidFemaleLine:
					case ChromosomeType::kH_HaploidAutosome:				// assumed to follow the same pattern, so skipped
					{
						haplosome_index += 1;
						break;
					}
						// chromosomes that have one haplosome, from the male parent
					case ChromosomeType::kY_YSexChromosome:
					case ChromosomeType::kHM_HaploidMaleInherited:
					case ChromosomeType::kML_HaploidMaleLine:
					{
						Haplosome *haplosome = haplosomes[haplosome_index];
						
						if (allowNullHaplosomes || !haplosome->IsNull())
							vec->push_object_element_no_check_NORR(haplosome);
						
						haplosome_index += 1;
						break;
					}
				}
			}
			
			return EidosValue_SP(vec);
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
					static_sex_string_H = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("H"));
					static_sex_string_F = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("F"));
					static_sex_string_M = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("M"));
					static_sex_string_O = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("?"));
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
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(age_));
		}
		case gID_meanParentAge:
		{
			if (mean_parent_age_ == -1)
				EIDOS_TERMINATION << "ERROR (Individual::GetProperty): property meanParentAge is not available in WF models." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(mean_parent_age_));
		}
		case gID_pedigreeID:		// ACCELERATED
		{
			if (!subpopulation_->species_.PedigreesEnabledByUser())
				EIDOS_TERMINATION << "ERROR (Individual::GetProperty): property pedigreeID is not available because pedigree recording has not been enabled." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(pedigree_id_));
		}
		case gID_pedigreeParentIDs:
		{
			if (!subpopulation_->species_.PedigreesEnabledByUser())
				EIDOS_TERMINATION << "ERROR (Individual::GetProperty): property pedigreeParentIDs is not available because pedigree recording has not been enabled." << EidosTerminate();
			
			EidosValue_Int *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(2);
			
			vec->set_int_no_check(pedigree_p1_, 0);
			vec->set_int_no_check(pedigree_p2_, 1);
			
			return EidosValue_SP(vec);
		}
		case gID_pedigreeGrandparentIDs:
		{
			if (!subpopulation_->species_.PedigreesEnabledByUser())
				EIDOS_TERMINATION << "ERROR (Individual::GetProperty): property pedigreeGrandparentIDs is not available because pedigree recording has not been enabled." << EidosTerminate();
			
			EidosValue_Int *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(4);
			
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
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(reproductive_output_));
		}
		case gID_spatialPosition:					// ACCELERATED
		{
			Species &species = subpopulation_->species_;
			
			switch (species.SpatialDimensionality())
			{
				case 0:
					EIDOS_TERMINATION << "ERROR (Individual::GetProperty): position cannot be accessed in non-spatial simulations." << EidosTerminate();
				case 1:
					return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(spatial_x_));
				case 2:
					return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float{spatial_x_, spatial_y_});
				case 3:
					return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float{spatial_x_, spatial_y_, spatial_z_});
				default:
					return gStaticEidosValueNULL;	// never hit; here to make the compiler happy
			}
		}
		case gID_uniqueMutations:
		{
			Species &species = subpopulation_->species_;
			int haplosome_count_per_individual = species.HaplosomeCountPerIndividual();
			int total_mutation_count = 0;
			
			subpopulation_->population_.CheckForDeferralInHaplosomesVector(haplosomes_, haplosome_count_per_individual, "Individual::GetProperty");
			
			for (int haplosome_index = 0; haplosome_index < haplosome_count_per_individual; haplosome_index++)
			{
				Haplosome *haplosome = haplosomes_[haplosome_index];
				
				if (!haplosome->IsNull())
					total_mutation_count += haplosome->mutation_count();
			}
			
			// We reserve a vector large enough to hold all the mutations from all haplosomes; probably usually overkill, but it does little harm
			EidosValue_Object *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Mutation_Class));
			EidosValue_SP result_SP = EidosValue_SP(vec);
			
			if (total_mutation_count == 0)
				return result_SP;
			
			vec->reserve(total_mutation_count);
			
			Mutation *mut_block_ptr = gSLiM_Mutation_Block;
			
			for (Chromosome *chromosome : species.Chromosomes())
			{
				int first_haplosome_index = species.FirstHaplosomeIndices()[chromosome->Index()];
				int last_haplosome_index = species.LastHaplosomeIndices()[chromosome->Index()];
				
				if (first_haplosome_index == last_haplosome_index)
				{
					// haploid chromosomes contain unique mutations by definition; add them all
					Haplosome *haplosome1 = haplosomes_[first_haplosome_index];
					
					if (!haplosome1->IsNull())
					{
						int mutrun_count = haplosome1->mutrun_count_;
						
						for (int run_index = 0; run_index < mutrun_count; ++run_index)
						{
							const MutationRun *mutrun1 = haplosome1->mutruns_[run_index];
							int g1_size = mutrun1->size();
							int g1_index = 0;
							
							while (g1_index < g1_size)
							{
								MutationIndex mut = (*mutrun1)[g1_index++];
								
								vec->push_object_element_no_check_RR(mut_block_ptr + mut);
							}
						}
					}
				}
				else
				{
					// diploid chromosomes require uniquing logic
					Haplosome *haplosome1 = haplosomes_[first_haplosome_index];
					Haplosome *haplosome2 = haplosomes_[last_haplosome_index];
					int haplosome1_size = (haplosome1->IsNull() ? 0 : haplosome1->mutation_count());
					int haplosome2_size = (haplosome2->IsNull() ? 0 : haplosome2->mutation_count());
					
					if (haplosome1_size + haplosome2_size > 0)
					{
						int mutrun_count = (haplosome1_size ? haplosome1->mutrun_count_ : haplosome2->mutrun_count_);
						
						for (int run_index = 0; run_index < mutrun_count; ++run_index)
						{
							// We want to interleave mutations from the two haplosomes, keeping only the uniqued mutations.  For a given position, we take mutations
							// from g1 first, and then look at the mutations in g2 at the same position and add them if they are not in g1.
							const MutationRun *mutrun1 = (haplosome1_size ? haplosome1->mutruns_[run_index] : nullptr);
							const MutationRun *mutrun2 = (haplosome2_size ? haplosome2->mutruns_[run_index] : nullptr);
							int g1_size = (mutrun1 ? mutrun1->size() : 0);
							int g2_size = (mutrun2 ? mutrun2->size() : 0);
							int g1_index = 0, g2_index = 0;
							
							if (g1_size && g2_size)
							{
								// Get the position of the mutations at g1_index and g2_index
								MutationIndex g1_mut = (*mutrun1)[g1_index], g2_mut = (*mutrun2)[g2_index];
								slim_position_t pos1 = (mut_block_ptr + g1_mut)->position_, pos2 = (mut_block_ptr + g2_mut)->position_;
								
								// Process mutations as long as both haplosomes still have mutations left in them
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
					}
				}
			}
			
			return result_SP;
		}
			
			// variables
		case gEidosID_color:
		{
			// as of SLiM 4.0.1, we construct a color string from the RGB values, which will
			// not necessarily be what the user set, but will represent the same color
#ifdef SLIMGUI
			if (!color_set_)
				return gStaticEidosValue_StringEmpty;
			
			char hex_chars[8];
			
			Eidos_GetColorString(colorR_, colorG_, colorB_, hex_chars);
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String(std::string(hex_chars)));
#else
			// BCH 3/23/2025: color variables now only exist in SLiMgui, to save on memory footprint
			return gStaticEidosValue_StringEmpty;
#endif
		}
		case gID_tag:				// ACCELERATED
		{
			slim_usertag_t tag_value = tag_value_;
			
			if (tag_value == SLIM_TAG_UNSET_VALUE)
				EIDOS_TERMINATION << "ERROR (Individual::GetProperty): property tag accessed on individual before being set." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(tag_value));
		}
		case gID_tagF:				// ACCELERATED
		{
			double tagF_value = tagF_value_;
			
			if (tagF_value == SLIM_TAGF_UNSET_VALUE)
				EIDOS_TERMINATION << "ERROR (Individual::GetProperty): property tagF accessed on individual before being set." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(tagF_value));
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
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(fitness_scaling_));
		}
		case gEidosID_x:			// ACCELERATED
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(spatial_x_));
		}
		case gEidosID_y:			// ACCELERATED
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(spatial_y_));
		}
		case gEidosID_z:			// ACCELERATED
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(spatial_z_));
		}
			
			// These properties are presently undocumented, used for testing purposes, but maybe they are useful to others?
			// They provide x/y/z as pairs or a triplet, whether the model is spatial or not, regardless of dimensionality
		case gEidosID_xy:
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float({spatial_x_, spatial_y_}));
		}
		case gEidosID_xz:
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float({spatial_x_, spatial_z_}));
		}
		case gEidosID_yz:
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float({spatial_y_, spatial_z_}));
		}
		case gEidosID_xyz:
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float({spatial_x_, spatial_y_, spatial_z_}));
		}
			
			// all others, including gID_none
		default:
			return super::GetProperty(p_property_id);
	}
}

EidosValue *Individual::GetProperty_Accelerated_index(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Individual *value = (Individual *)(p_values[value_index]);
		
		int_result->set_int_no_check(value->index_, value_index);
	}
	
	return int_result;
}

EidosValue *Individual::GetProperty_Accelerated_pedigreeID(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(p_values_size);
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
	EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(p_values_size);
	
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
	
	EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(p_values_size);
	
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
	
	EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Individual *value = (Individual *)(p_values[value_index]);
		
		int_result->set_int_no_check(value->reproductive_output_, value_index);
	}
	
	return int_result;
}

EidosValue *Individual::GetProperty_Accelerated_tagF(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->resize_no_initialize(p_values_size);
	
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
	EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Individual *value = (Individual *)(p_values[value_index]);
		
		float_result->set_float_no_check(value->fitness_scaling_, value_index);
	}
	
	return float_result;
}

EidosValue *Individual::GetProperty_Accelerated_x(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Individual *value = (Individual *)(p_values[value_index]);
		
		float_result->set_float_no_check(value->spatial_x_, value_index);
	}
	
	return float_result;
}

EidosValue *Individual::GetProperty_Accelerated_y(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Individual *value = (Individual *)(p_values[value_index]);
		
		float_result->set_float_no_check(value->spatial_y_, value_index);
	}
	
	return float_result;
}

EidosValue *Individual::GetProperty_Accelerated_z(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->resize_no_initialize(p_values_size);
	
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
	EidosValue_Float *float_result;
	
	if (consensus_species)
	{
		// All individuals belong to the same species (common case), so we have the same dimensionality for all
		int dimensionality = consensus_species->SpatialDimensionality();
		
		if (dimensionality == 0)
			EIDOS_TERMINATION << "ERROR (Individual::GetProperty): position cannot be accessed in non-spatial simulations." << EidosTerminate();
		
		float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->resize_no_initialize(p_values_size * dimensionality);
		
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
		float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float());
		
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
	EidosValue_Object *object_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Subpopulation_Class))->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Individual *value = (Individual *)(p_values[value_index]);
		
		if (value->killed_)
			EIDOS_TERMINATION << "ERROR (Individual::GetProperty): property subpopulation is not available for individuals that have been killed; they have no subpopulation." << EidosTerminate();
		
		object_result->set_object_element_no_check_NORR(value->subpopulation_, value_index);
	}
	
	return object_result;
}

EidosValue *Individual::GetProperty_Accelerated_haploidGenome1(EidosObject **p_values, size_t p_values_size)
{
	const Individual **individuals_buffer = (const Individual **)p_values;
	
	// SPECIES CONSISTENCY CHECK
	Species *species = Community::SpeciesForIndividualsVector(individuals_buffer, (int)p_values_size);
	
	if (!species)
		return nullptr;		// defer to GetProperty(); different species will have different chromosomes
	
	const std::vector<Chromosome *> &chromosomes = species->Chromosomes();
	
	if (chromosomes.size() != 1)
		return nullptr;		// defer to GetProperty(); multiple chromosomes need to be looped through
	
	Chromosome *chromosome = chromosomes[0];
	EidosValue_Object *object_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Haplosome_Class));
	
	switch (chromosome->Type())
	{
			// chromosomes that have two haplosomes, the first of which is from the first parent
		case ChromosomeType::kA_DiploidAutosome:
		case ChromosomeType::kX_XSexChromosome:
		case ChromosomeType::kZ_ZSexChromosome:
		case ChromosomeType::kNullY_YSexChromosomeWithNull:
		case ChromosomeType::kHNull_HaploidAutosomeWithNull:	// assumed to follow the same pattern
		{
			object_result->resize_no_initialize(p_values_size);
			
			for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			{
				Individual *value = (Individual *)(p_values[value_index]);
				
				object_result->set_object_element_no_check_NORR(value->haplosomes_[0], value_index);
			}
			
			return object_result;
		}
			// chromosomes that have one haplosome, from the female parent
		case ChromosomeType::kW_WSexChromosome:
		case ChromosomeType::kHF_HaploidFemaleInherited:
		case ChromosomeType::kFL_HaploidFemaleLine:
		case ChromosomeType::kH_HaploidAutosome:				// assumed to follow the same pattern, so included
		{
			object_result->resize_no_initialize(p_values_size);
			
			for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			{
				Individual *value = (Individual *)(p_values[value_index]);
				
				object_result->set_object_element_no_check_NORR(value->haplosomes_[0], value_index);
			}
			
			return object_result;
		}
			// chromosomes that have one haplosome, from the male parent
		case ChromosomeType::kY_YSexChromosome:
		case ChromosomeType::kHM_HaploidMaleInherited:
		case ChromosomeType::kML_HaploidMaleLine:
		{
			return object_result;	// zero-length return
		}
	}
	
	// some compilers warn if this is not here, even though the switch above handles all ChromosomeType values
	EIDOS_TERMINATION << "ERROR (Individual::GetProperty_Accelerated_haploidGenome1): (internal error) chromosome type not handled." << EidosTerminate();
}

EidosValue *Individual::GetProperty_Accelerated_haploidGenome1NonNull(EidosObject **p_values, size_t p_values_size)
{
	const Individual **individuals_buffer = (const Individual **)p_values;
	
	// SPECIES CONSISTENCY CHECK
	Species *species = Community::SpeciesForIndividualsVector(individuals_buffer, (int)p_values_size);
	
	if (!species)
		return nullptr;		// defer to GetProperty(); different species will have different chromosomes
	
	const std::vector<Chromosome *> &chromosomes = species->Chromosomes();
	
	if (chromosomes.size() != 1)
		return nullptr;		// defer to GetProperty(); multiple chromosomes need to be looped through
	
	Chromosome *chromosome = chromosomes[0];
	EidosValue_Object *object_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Haplosome_Class));
	
	switch (chromosome->Type())
	{
			// chromosomes that have two haplosomes, the first of which is from the first parent
		case ChromosomeType::kA_DiploidAutosome:
		case ChromosomeType::kX_XSexChromosome:
		case ChromosomeType::kZ_ZSexChromosome:
		case ChromosomeType::kNullY_YSexChromosomeWithNull:
		case ChromosomeType::kHNull_HaploidAutosomeWithNull:	// assumed to follow the same pattern
		{
			object_result->reserve(p_values_size);
			
			for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			{
				Individual *value = (Individual *)(p_values[value_index]);
				Haplosome *haplosome = value->haplosomes_[0];
				
				if (!haplosome->IsNull())
					object_result->push_object_element_no_check_NORR(haplosome);
			}
			
			return object_result;
		}
			// chromosomes that have one haplosome, from the female parent
		case ChromosomeType::kW_WSexChromosome:
		case ChromosomeType::kHF_HaploidFemaleInherited:
		case ChromosomeType::kFL_HaploidFemaleLine:
		case ChromosomeType::kH_HaploidAutosome:				// assumed to follow the same pattern, so included
		{
			object_result->reserve(p_values_size);
			
			for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			{
				Individual *value = (Individual *)(p_values[value_index]);
				Haplosome *haplosome = value->haplosomes_[0];
				
				if (!haplosome->IsNull())
					object_result->push_object_element_no_check_NORR(haplosome);
			}
			
			return object_result;
		}
			// chromosomes that have one haplosome, from the male parent
		case ChromosomeType::kY_YSexChromosome:
		case ChromosomeType::kHM_HaploidMaleInherited:
		case ChromosomeType::kML_HaploidMaleLine:
		{
			return object_result;	// zero-length return
		}
	}
	
	// some compilers warn if this is not here, even though the switch above handles all ChromosomeType values
	EIDOS_TERMINATION << "ERROR (Individual::GetProperty_Accelerated_haploidGenome1NonNull): (internal error) chromosome type not handled." << EidosTerminate();
}

EidosValue *Individual::GetProperty_Accelerated_haploidGenome2(EidosObject **p_values, size_t p_values_size)
{
	const Individual **individuals_buffer = (const Individual **)p_values;
	
	// SPECIES CONSISTENCY CHECK
	Species *species = Community::SpeciesForIndividualsVector(individuals_buffer, (int)p_values_size);
	
	if (!species)
		return nullptr;		// defer to GetProperty(); different species will have different chromosomes
	
	const std::vector<Chromosome *> &chromosomes = species->Chromosomes();
	
	if (chromosomes.size() != 1)
		return nullptr;		// defer to GetProperty(); multiple chromosomes need to be looped through
	
	Chromosome *chromosome = chromosomes[0];
	EidosValue_Object *object_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Haplosome_Class));
	
	switch (chromosome->Type())
	{
			// chromosomes that have two haplosomes, the first of which is from the first parent
		case ChromosomeType::kA_DiploidAutosome:
		case ChromosomeType::kX_XSexChromosome:
		case ChromosomeType::kZ_ZSexChromosome:
		case ChromosomeType::kNullY_YSexChromosomeWithNull:
		case ChromosomeType::kHNull_HaploidAutosomeWithNull:	// assumed to follow the same pattern
		{
			object_result->resize_no_initialize(p_values_size);
			
			for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			{
				Individual *value = (Individual *)(p_values[value_index]);
				
				object_result->set_object_element_no_check_NORR(value->haplosomes_[1], value_index);
			}
			
			return object_result;
		}
			// chromosomes that have one haplosome, from the female parent
		case ChromosomeType::kW_WSexChromosome:
		case ChromosomeType::kHF_HaploidFemaleInherited:
		case ChromosomeType::kFL_HaploidFemaleLine:
		case ChromosomeType::kH_HaploidAutosome:				// assumed to follow the same pattern, so included
		{
			return object_result;	// zero-length return
		}
			// chromosomes that have one haplosome, from the male parent
		case ChromosomeType::kY_YSexChromosome:
		case ChromosomeType::kHM_HaploidMaleInherited:
		case ChromosomeType::kML_HaploidMaleLine:
		{
			object_result->resize_no_initialize(p_values_size);
			
			for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			{
				Individual *value = (Individual *)(p_values[value_index]);
				
				object_result->set_object_element_no_check_NORR(value->haplosomes_[0], value_index);
			}
			
			return object_result;
		}
	}
	
	// some compilers warn if this is not here, even though the switch above handles all ChromosomeType values
	EIDOS_TERMINATION << "ERROR (Individual::GetProperty_Accelerated_haploidGenome2): (internal error) chromosome type not handled." << EidosTerminate();
}

EidosValue *Individual::GetProperty_Accelerated_haploidGenome2NonNull(EidosObject **p_values, size_t p_values_size)
{
	const Individual **individuals_buffer = (const Individual **)p_values;
	
	// SPECIES CONSISTENCY CHECK
	Species *species = Community::SpeciesForIndividualsVector(individuals_buffer, (int)p_values_size);
	
	if (!species)
		return nullptr;		// defer to GetProperty(); different species will have different chromosomes
	
	const std::vector<Chromosome *> &chromosomes = species->Chromosomes();
	
	if (chromosomes.size() != 1)
		return nullptr;		// defer to GetProperty(); multiple chromosomes need to be looped through
	
	Chromosome *chromosome = chromosomes[0];
	EidosValue_Object *object_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Haplosome_Class));
	
	switch (chromosome->Type())
	{
			// chromosomes that have two haplosomes, the first of which is from the first parent
		case ChromosomeType::kA_DiploidAutosome:
		case ChromosomeType::kX_XSexChromosome:
		case ChromosomeType::kZ_ZSexChromosome:
		case ChromosomeType::kNullY_YSexChromosomeWithNull:
		case ChromosomeType::kHNull_HaploidAutosomeWithNull:	// assumed to follow the same pattern
		{
			object_result->reserve(p_values_size);
			
			for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			{
				Individual *value = (Individual *)(p_values[value_index]);
				Haplosome *haplosome = value->haplosomes_[1];
				
				if (!haplosome->IsNull())
					object_result->push_object_element_no_check_NORR(haplosome);
			}
			
			return object_result;
		}
			// chromosomes that have one haplosome, from the female parent
		case ChromosomeType::kW_WSexChromosome:
		case ChromosomeType::kHF_HaploidFemaleInherited:
		case ChromosomeType::kFL_HaploidFemaleLine:
		case ChromosomeType::kH_HaploidAutosome:				// assumed to follow the same pattern, so included
		{
			return object_result;	// zero-length return
		}
			// chromosomes that have one haplosome, from the male parent
		case ChromosomeType::kY_YSexChromosome:
		case ChromosomeType::kHM_HaploidMaleInherited:
		case ChromosomeType::kML_HaploidMaleLine:
		{
			object_result->reserve(p_values_size);
			
			for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			{
				Individual *value = (Individual *)(p_values[value_index]);
				Haplosome *haplosome = value->haplosomes_[0];
				
				if (!haplosome->IsNull())
					object_result->push_object_element_no_check_NORR(haplosome);
			}
			
			return object_result;
		}
	}
	
	// some compilers warn if this is not here, even though the switch above handles all ChromosomeType values
	EIDOS_TERMINATION << "ERROR (Individual::GetProperty_Accelerated_haploidGenome2NonNull): (internal error) chromosome type not handled." << EidosTerminate();
}

EidosValue *Individual::GetProperty_Accelerated_haplosomes(EidosObject **p_values, size_t p_values_size)
{
	const Individual **individuals_buffer = (const Individual **)p_values;
	
	// SPECIES CONSISTENCY CHECK
	Species *species = Community::SpeciesForIndividualsVector(individuals_buffer, (int)p_values_size);
	
	if (!species)
		return nullptr;		// defer to GetProperty(); different species will have different chromosomes
	
	int haplosome_count_per_individual = species->HaplosomeCountPerIndividual();
	EidosValue_Object *object_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Haplosome_Class))->resize_no_initialize(p_values_size * haplosome_count_per_individual);
	size_t result_index = 0;
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Individual *value = (Individual *)(p_values[value_index]);
		Haplosome **haplosomes = value->haplosomes_;
		
		for (int haplosome_index = 0; haplosome_index < haplosome_count_per_individual; haplosome_index++)
		{
			Haplosome *haplosome = haplosomes[haplosome_index];
			
			object_result->set_object_element_no_check_NORR(haplosome, result_index++);
		}
	}
	
	return object_result;
}

EidosValue *Individual::GetProperty_Accelerated_haplosomesNonNull(EidosObject **p_values, size_t p_values_size)
{
	const Individual **individuals_buffer = (const Individual **)p_values;
	
	// SPECIES CONSISTENCY CHECK
	Species *species = Community::SpeciesForIndividualsVector(individuals_buffer, (int)p_values_size);
	
	if (!species)
		return nullptr;		// defer to GetProperty(); different species will have different chromosomes
	
	int haplosome_count_per_individual = species->HaplosomeCountPerIndividual();
	EidosValue_Object *object_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Haplosome_Class))->reserve(p_values_size * haplosome_count_per_individual);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Individual *value = (Individual *)(p_values[value_index]);
		Haplosome **haplosomes = value->haplosomes_;
		
		for (int haplosome_index = 0; haplosome_index < haplosome_count_per_individual; haplosome_index++)
		{
			Haplosome *haplosome = haplosomes[haplosome_index];
			
			if (!haplosome->IsNull())
				object_result->push_object_element_no_check_NORR(haplosome);
		}
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
#ifdef SLIMGUI
			// BCH 3/23/2025: color variables now only exist in SLiMgui, to save on memory footprint
			const std::string &color_string = ((EidosValue_String &)p_value).StringRefAtIndex_NOCAST(0, nullptr);
			
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
#endif
			return;
		}
		case gID_tag:				// ACCELERATED
		{
			slim_usertag_t value = SLiMCastToUsertagTypeOrRaise(p_value.IntAtIndex_NOCAST(0, nullptr));
			
			tag_value_ = value;
			s_any_individual_tag_set_ = true;
			return;
		}
		case gID_tagF:				// ACCELERATED
		{
			tagF_value_ = p_value.FloatAtIndex_NOCAST(0, nullptr);
			s_any_individual_tagF_set_ = true;
			return;
		}
		case gID_tagL0:				// ACCELERATED
		{
			eidos_logical_t value = p_value.LogicalAtIndex_NOCAST(0, nullptr);
			
			tagL0_set_ = true;
			tagL0_value_ = value;
			s_any_individual_tagL_set_ = true;
			return;
		}
		case gID_tagL1:				// ACCELERATED
		{
			eidos_logical_t value = p_value.LogicalAtIndex_NOCAST(0, nullptr);
			
			tagL1_set_ = true;
			tagL1_value_ = value;
			s_any_individual_tagL_set_ = true;
			return;
		}
		case gID_tagL2:				// ACCELERATED
		{
			eidos_logical_t value = p_value.LogicalAtIndex_NOCAST(0, nullptr);
			
			tagL2_set_ = true;
			tagL2_value_ = value;
			s_any_individual_tagL_set_ = true;
			return;
		}
		case gID_tagL3:				// ACCELERATED
		{
			eidos_logical_t value = p_value.LogicalAtIndex_NOCAST(0, nullptr);
			
			tagL3_set_ = true;
			tagL3_value_ = value;
			s_any_individual_tagL_set_ = true;
			return;
		}
		case gID_tagL4:				// ACCELERATED
		{
			eidos_logical_t value = p_value.LogicalAtIndex_NOCAST(0, nullptr);
			
			tagL4_set_ = true;
			tagL4_value_ = value;
			s_any_individual_tagL_set_ = true;
			return;
		}
		case gID_fitnessScaling:	// ACCELERATED
		{
			fitness_scaling_ = p_value.FloatAtIndex_NOCAST(0, nullptr);
			Individual::s_any_individual_fitness_scaling_set_ = true;
			
			if ((fitness_scaling_ < 0.0) || (std::isnan(fitness_scaling_)))
				EIDOS_TERMINATION << "ERROR (Individual::SetProperty): property fitnessScaling must be >= 0.0." << EidosTerminate();
			
			return;
		}
		case gEidosID_x:			// ACCELERATED
		{
			spatial_x_ = p_value.FloatAtIndex_NOCAST(0, nullptr);
			return;
		}
		case gEidosID_y:			// ACCELERATED
		{
			spatial_y_ = p_value.FloatAtIndex_NOCAST(0, nullptr);
			return;
		}
		case gEidosID_z:			// ACCELERATED
		{
			spatial_z_ = p_value.FloatAtIndex_NOCAST(0, nullptr);
			return;
		}
		case gID_age:				// ACCELERATED
		{
			slim_age_t value = SLiMCastToAgeTypeOrRaise(p_value.IntAtIndex_NOCAST(0, nullptr));
			
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
		int64_t source_value = p_source.IntAtIndex_NOCAST(0, nullptr);
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Individual *)(p_values[value_index]))->tag_value_ = source_value;
	}
	else
	{
		const int64_t *source_data = p_source.IntData();
		
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
		double source_value = p_source.FloatAtIndex_NOCAST(0, nullptr);
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Individual *)(p_values[value_index]))->tagF_value_ = source_value;
	}
	else
	{
		const double *source_data = p_source.FloatData();
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Individual *)(p_values[value_index]))->tagF_value_ = source_data[value_index];
	}
}

void Individual::SetProperty_Accelerated_tagL0(EidosObject **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size)
{
	s_any_individual_tagL_set_ = true;
	
	const eidos_logical_t *source_data = p_source.LogicalData();
	
	if (p_source_size == 1)
	{
		eidos_logical_t source_value = source_data[0];
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
		{
			Individual *individual = ((Individual *)(p_values[value_index]));
			
			individual->tagL0_set_ = true;
			individual->tagL0_value_ = source_value;
		}
	}
	else
	{
		
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
	
	const eidos_logical_t *source_data = p_source.LogicalData();
	
	if (p_source_size == 1)
	{
		eidos_logical_t source_value = source_data[0];
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
		{
			Individual *individual = ((Individual *)(p_values[value_index]));
			
			individual->tagL1_set_ = true;
			individual->tagL1_value_ = source_value;
		}
	}
	else
	{
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
	
	const eidos_logical_t *source_data = p_source.LogicalData();
	
	if (p_source_size == 1)
	{
		eidos_logical_t source_value = source_data[0];
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
		{
			Individual *individual = ((Individual *)(p_values[value_index]));
			
			individual->tagL2_set_ = true;
			individual->tagL2_value_ = source_value;
		}
	}
	else
	{
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
	
	const eidos_logical_t *source_data = p_source.LogicalData();
	
	if (p_source_size == 1)
	{
		eidos_logical_t source_value = source_data[0];
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
		{
			Individual *individual = ((Individual *)(p_values[value_index]));
			
			individual->tagL3_set_ = true;
			individual->tagL3_value_ = source_value;
		}
	}
	else
	{
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
	
	const eidos_logical_t *source_data = p_source.LogicalData();
	
	if (p_source_size == 1)
	{
		eidos_logical_t source_value = source_data[0];
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
		{
			Individual *individual = ((Individual *)(p_values[value_index]));
			
			individual->tagL4_set_ = true;
			individual->tagL4_value_ = source_value;
		}
	}
	else
	{
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
		double source_value = p_source.FloatAtIndex_NOCAST(0, nullptr);
		
		needs_raise = _SetFitnessScaling_1(source_value, p_values, p_values_size);
	}
	else
	{
		const double *source_data = p_source.FloatData();
		
		needs_raise = _SetFitnessScaling_N(source_data, p_values, p_values_size);
	}
	
	if (needs_raise)
		EIDOS_TERMINATION << "ERROR (Individual::SetProperty_Accelerated_fitnessScaling): property fitnessScaling must be >= 0.0." << EidosTerminate();
}

void Individual::SetProperty_Accelerated_x(EidosObject **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size)
{
	if (p_source_size == 1)
	{
		double source_value = p_source.FloatAtIndex_NOCAST(0, nullptr);
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Individual *)(p_values[value_index]))->spatial_x_ = source_value;
	}
	else
	{
		const double *source_data = p_source.FloatData();
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Individual *)(p_values[value_index]))->spatial_x_ = source_data[value_index];
	}
}

void Individual::SetProperty_Accelerated_y(EidosObject **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size)
{
	if (p_source_size == 1)
	{
		double source_value = p_source.FloatAtIndex_NOCAST(0, nullptr);
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Individual *)(p_values[value_index]))->spatial_y_ = source_value;
	}
	else
	{
		const double *source_data = p_source.FloatData();
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Individual *)(p_values[value_index]))->spatial_y_ = source_data[value_index];
	}
}

void Individual::SetProperty_Accelerated_z(EidosObject **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size)
{
	if (p_source_size == 1)
	{
		double source_value = p_source.FloatAtIndex_NOCAST(0, nullptr);
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Individual *)(p_values[value_index]))->spatial_z_ = source_value;
	}
	else
	{
		const double *source_data = p_source.FloatData();
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Individual *)(p_values[value_index]))->spatial_z_ = source_data[value_index];
	}
}

void Individual::SetProperty_Accelerated_color(EidosObject **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size)
{
#pragma unused (p_values, p_values_size, p_source, p_source_size)
#ifdef SLIMGUI
	// BCH 3/23/2025: color variables now only exist in SLiMgui, to save on memory footprint
	if (p_source_size == 1)
	{
		const std::string &source_value = ((EidosValue_String &)p_source).StringRefAtIndex_NOCAST(0, nullptr);
		
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
		const std::string *source_data = p_source.StringData();
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
		{
			Individual *individual = ((Individual *)(p_values[value_index]));
			const std::string &source_value = source_data[value_index];
			
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
#endif
}

void Individual::SetProperty_Accelerated_age(EidosObject **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size)
{
	if (p_source_size == 1)
	{
		int64_t source_value = p_source.IntAtIndex_NOCAST(0, nullptr);
		slim_age_t source_age = SLiMCastToAgeTypeOrRaise(source_value);
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Individual *)(p_values[value_index]))->age_ = source_age;
	}
	else
	{
		const int64_t *source_data = p_source.IntData();
		
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
		case gID_haplosomesForChromosomes:	return ExecuteMethod_haplosomesForChromosomes(p_method_id, p_arguments, p_interpreter);
		case gID_relatedness:				return ExecuteMethod_relatedness(p_method_id, p_arguments, p_interpreter);
		case gID_sharedParentCount:			return ExecuteMethod_sharedParentCount(p_method_id, p_arguments, p_interpreter);
		//case gID_sumOfMutationsOfType:	return ExecuteMethod_Accelerated_sumOfMutationsOfType(p_method_id, p_arguments, p_interpreter);
		case gID_uniqueMutationsOfType:		return ExecuteMethod_uniqueMutationsOfType(p_method_id, p_arguments, p_interpreter);
		case gID_mutationsFromHaplosomes:	return ExecuteMethod_mutationsFromHaplosomes(p_method_id, p_arguments, p_interpreter);
			
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
	int haplosome_count_per_individual = subpopulation_->species_.HaplosomeCountPerIndividual();

	subpopulation_->population_.CheckForDeferralInHaplosomesVector(haplosomes_, haplosome_count_per_individual, "Individual::ExecuteMethod_containsMutations");
	
	EidosValue *mutations_value = p_arguments[0].get();
	int mutations_count = mutations_value->Count();
	
	if (mutations_count == 0)
		return gStaticEidosValue_Logical_ZeroVec;
	
	// SPECIES CONSISTENCY CHECK
	Species *species = Community::SpeciesForMutations(mutations_value);
	
	if (species != &subpopulation_->species_)
		EIDOS_TERMINATION << "ERROR (Individual::ExecuteMethod_containsMutations): containsMutations() requires that all mutations belong to the same species as the target individual." << EidosTerminate();
	
	if (mutations_count == 1)
	{
		// treat the singleton case separately to return gStaticEidosValue_LogicalT / gStaticEidosValue_LogicalF
		Mutation *mut = (Mutation *)(mutations_value->ObjectElementAtIndex_NOCAST(0, nullptr));
		slim_chromosome_index_t mut_chrom_index = mut->chromosome_index_;
		int first_haplosome_index = species->FirstHaplosomeIndices()[mut_chrom_index];
		int last_haplosome_index = species->LastHaplosomeIndices()[mut_chrom_index];
		
		for (int haplosome_index = first_haplosome_index; haplosome_index <= last_haplosome_index; ++haplosome_index)
		{
			Haplosome *haplosome = haplosomes_[haplosome_index];
			
			if (!haplosome->IsNull() && haplosome->contains_mutation(mut))
				return gStaticEidosValue_LogicalT;
		}
		
		return gStaticEidosValue_LogicalF;
	}
	else
	{
		EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(mutations_count);
		Mutation * const *mutations = (Mutation * const *)mutations_value->ObjectData();
		
		for (int value_index = 0; value_index < mutations_count; ++value_index)
		{
			Mutation *mut = mutations[value_index];
			slim_chromosome_index_t mut_chrom_index = mut->chromosome_index_;
			int first_haplosome_index = species->FirstHaplosomeIndices()[mut_chrom_index];
			int last_haplosome_index = species->LastHaplosomeIndices()[mut_chrom_index];
			
			for (int haplosome_index = first_haplosome_index; haplosome_index <= last_haplosome_index; ++haplosome_index)
			{
				Haplosome *haplosome = haplosomes_[haplosome_index];
				
				if (!haplosome->IsNull() && haplosome->contains_mutation(mut))
				{
					logical_result->set_logical_no_check(true, value_index);
					continue;
				}
				
				logical_result->set_logical_no_check(false, value_index);
			}
		}
		
		return EidosValue_SP(logical_result);
	}
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
	EidosValue_Int *integer_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(p_elements_size);
	int haplosome_count_per_individual = species->HaplosomeCountPerIndividual();
	
	EIDOS_THREAD_COUNT(gEidos_OMP_threads_I_COUNT_OF_MUTS_OF_TYPE);
#pragma omp parallel for schedule(dynamic, 1) default(none) shared(p_elements_size) firstprivate(p_elements, mut_block_ptr, mutation_type_ptr, integer_result) if(p_elements_size >= EIDOS_OMPMIN_I_COUNT_OF_MUTS_OF_TYPE) num_threads(thread_count)
	for (size_t element_index = 0; element_index < p_elements_size; ++element_index)
	{
		Individual *element = (Individual *)(p_elements[element_index]);
		int match_count = 0;
		
		for (int haplosome_index = 0; haplosome_index < haplosome_count_per_individual; haplosome_index++)
		{
			Haplosome *haplosome = element->haplosomes_[haplosome_index];
			
			if (!haplosome->IsNull())
			{
				int mutrun_count = haplosome->mutrun_count_;
				
				for (int run_index = 0; run_index < mutrun_count; ++run_index)
				{
					const MutationRun *mutrun = haplosome->mutruns_[run_index];
					int haplosome1_count = mutrun->size();
					const MutationIndex *haplosome1_ptr = mutrun->begin_pointer_const();
					
					for (int mut_index = 0; mut_index < haplosome1_count; ++mut_index)
						if ((mut_block_ptr + haplosome1_ptr[mut_index])->mutation_type_ptr_ == mutation_type_ptr)
							++match_count;
				}
			}
		}
		
		integer_result->set_int_no_check(match_count, element_index);
	}
	
	return EidosValue_SP(integer_result);
}

//	*********************	- (object<Haplosome>)haplosomesForChromosomes([Niso<Chromosome> chromosomes = NULL], [Ni$ index = NULL], [logical$ includeNulls = T])
//
EidosValue_SP Individual::ExecuteMethod_haplosomesForChromosomes(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *chromosomes_value = p_arguments[0].get();
	EidosValue *index_value = p_arguments[1].get();
	EidosValue *includeNulls_value = p_arguments[2].get();
	
	// assemble a vector of chromosome indices we're fetching
	Species &species = subpopulation_->species_;
	std::vector<slim_chromosome_index_t> chromosome_indices;
	
	species.GetChromosomeIndicesFromEidosValue(chromosome_indices, chromosomes_value);
	
	// get index and includeNulls
	int64_t index = -1;	// for NULL
	
	if (index_value->Type() == EidosValueType::kValueInt)
	{
		index = index_value->IntAtIndex_NOCAST(0, nullptr);
		
		if ((index != 0) && (index != 1))
			EIDOS_TERMINATION << "ERROR (Individual::ExecuteMethod_haplosomesForChromosomes): haplosomesForChromosomes() requires that index is 0, 1, or NULL." << EidosTerminate();
	}
	
	bool includeNulls = includeNulls_value->LogicalAtIndex_NOCAST(0, nullptr);
	
	// fetch the requested haplosomes
	EidosValue_Object *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Haplosome_Class));
	
	AppendHaplosomesForChromosomes(vec, chromosome_indices, index, includeNulls);
	
	return EidosValue_SP(vec);
}
	
//	*********************	- (float)relatedness(object<Individual> individuals, [Niso<Chromosome>$ chromosome = NULL])
//
EidosValue_SP Individual::ExecuteMethod_relatedness(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *individuals_value = p_arguments[0].get();
	EidosValue *chromosome_value = p_arguments[1].get();
	int individuals_count = individuals_value->Count();
	
	if (individuals_count == 0)
		return gStaticEidosValue_Float_ZeroVec;
	
	// SPECIES CONSISTENCY CHECK
	Species *species = Community::SpeciesForIndividuals(individuals_value);
	
	if (species != &subpopulation_->species_)
		EIDOS_TERMINATION << "ERROR (Individual::ExecuteMethod_relatedness): relatedness() requires that all individuals belong to the same species as the target individual." << EidosTerminate();
	
	Chromosome *chromosome = species->GetChromosomeFromEidosValue(chromosome_value);
	
	if (!chromosome)
	{
		if (species->Chromosomes().size() == 1)
			chromosome = species->Chromosomes()[0];
		else if (species->Chromosomes().size() > 1)
			EIDOS_TERMINATION << "ERROR (Individual::ExecuteMethod_relatedness): relatedness() requires the chromosome to be specified in multi-chromosome models." << EidosTerminate();
	}
	
	// in a no-genetics model, the chromosome parameter must be NULL, so chromosome will be nullptr, and we assume type "A"
	ChromosomeType chromosome_type = (chromosome ? chromosome->Type() : ChromosomeType::kA_DiploidAutosome);
	
	bool pedigree_tracking_enabled = subpopulation_->species_.PedigreesEnabledByUser();
	EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->resize_no_initialize(individuals_count);
	Individual * const *individuals_data = (Individual * const *)individuals_value->ObjectData();
	
	if (pedigree_tracking_enabled)
	{
		// this parallelizes the case of one_individual.relatedness(many_individuals)
		// it would be nice to also parallelize the case of many_individuals.relatedness(one_individual); that would require accelerating this method
		EIDOS_THREAD_COUNT(gEidos_OMP_threads_RELATEDNESS);
#pragma omp parallel for schedule(dynamic, 128) default(none) shared(individuals_count, individuals_data) firstprivate(float_result) if(individuals_count >= EIDOS_OMPMIN_RELATEDNESS) num_threads(thread_count)
		for (int value_index = 0; value_index < individuals_count; ++value_index)
		{
			Individual *ind = individuals_data[value_index];
			double relatedness = RelatednessToIndividual(*ind, chromosome_type);
			
			float_result->set_float_no_check(relatedness, value_index);
		}
	}
	else
	{
		for (int value_index = 0; value_index < individuals_count; ++value_index)
		{
			Individual *ind = individuals_data[value_index];
			double relatedness = (ind == this) ? 1.0 : 0.0;
			
			float_result->set_float_no_check(relatedness, value_index);
		}
	}
	
	return EidosValue_SP(float_result);
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
	EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(individuals_count);
	Individual * const *individuals = (Individual * const *)individuals_value->ObjectData();
	
	if (pedigree_tracking_enabled)
	{
		// FIXME needs parallelization, see relatedness()
		for (int value_index = 0; value_index < individuals_count; ++value_index)
		{
			Individual *ind = individuals[value_index];
			int shared_count = SharedParentCountWithIndividual(*ind);
			
			int_result->set_int_no_check(shared_count, value_index);
		}
	}
	else
	{
		for (int value_index = 0; value_index < individuals_count; ++value_index)
		{
			Individual *ind = individuals[value_index];
			int shared_count = (ind == this) ? 2.0 : 0.0;
			
			int_result->set_int_no_check(shared_count, value_index);
		}
	}
	
	return EidosValue_SP(int_result);
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
	
	// Sum the selection coefficients of mutations of the given type
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->resize_no_initialize(p_elements_size);
	int haplosome_count_per_individual = species->HaplosomeCountPerIndividual();
	
	EIDOS_THREAD_COUNT(gEidos_OMP_threads_SUM_OF_MUTS_OF_TYPE);
#pragma omp parallel for schedule(dynamic, 1) default(none) shared(p_elements_size) firstprivate(p_elements, mut_block_ptr, mutation_type_ptr, float_result) if(p_elements_size >= EIDOS_OMPMIN_SUM_OF_MUTS_OF_TYPE) num_threads(thread_count)
	for (size_t element_index = 0; element_index < p_elements_size; ++element_index)
	{
		Individual *element = (Individual *)(p_elements[element_index]);
		double selcoeff_sum = 0.0;
		
		for (int haplosome_index = 0; haplosome_index < haplosome_count_per_individual; haplosome_index++)
		{
			Haplosome *haplosome = element->haplosomes_[haplosome_index];
			
			if (!haplosome->IsNull())
			{
				int mutrun_count = haplosome->mutrun_count_;
				
				for (int run_index = 0; run_index < mutrun_count; ++run_index)
				{
					const MutationRun *mutrun = haplosome->mutruns_[run_index];
					int haplosome1_count = mutrun->size();
					const MutationIndex *haplosome1_ptr = mutrun->begin_pointer_const();
					
					for (int mut_index = 0; mut_index < haplosome1_count; ++mut_index)
					{
						Mutation *mut_ptr = mut_block_ptr + haplosome1_ptr[mut_index];
						
						if (mut_ptr->mutation_type_ptr_ == mutation_type_ptr)
							selcoeff_sum += mut_ptr->selection_coeff_;
					}
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
	// NOTE: this method has been deprecated in favor of mutationsFromHaplosomes()
	int haplosome_count_per_individual = subpopulation_->species_.HaplosomeCountPerIndividual();

	subpopulation_->population_.CheckForDeferralInHaplosomesVector(haplosomes_, haplosome_count_per_individual, "Individual::ExecuteMethod_uniqueMutationsOfType");
	
	EidosValue *mutType_value = p_arguments[0].get();
	
	Species &species = subpopulation_->species_;
	MutationType *mutation_type_ptr = SLiM_ExtractMutationTypeFromEidosValue_io(mutType_value, 0, &species.community_, &species, "uniqueMutationsOfType()");		// SPECIES CONSISTENCY CHECK
	
	// This code is adapted from uniqueMutations and follows its logic closely
	
	// We try to reserve a vector large enough to hold all the mutations; probably usually overkill, but it does little harm
	// Note that since we do not *always* reserve, we have to use push_object_element() below to check for space
	EidosValue_Object *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Mutation_Class));
	EidosValue_SP result_SP = EidosValue_SP(vec);
	bool only_haploid_haplosomes = true;
	size_t vec_reserve_size = 0;
	
	for (Chromosome *chromosome : species.Chromosomes())
	{
		int first_haplosome_index = species.FirstHaplosomeIndices()[chromosome->Index()];
		int last_haplosome_index = species.LastHaplosomeIndices()[chromosome->Index()];
		
		if (first_haplosome_index == last_haplosome_index)
		{
			// haploid chromosomes contain unique mutations by definition; add them all
			Haplosome *haplosome1 = haplosomes_[first_haplosome_index];
			
			if (!haplosome1->IsNull())
				vec_reserve_size += haplosome1->mutation_count();
		}
		else
		{
			// diploid chromsomes where one is empty/null also contain unique mutations by definition
			Haplosome *haplosome1 = haplosomes_[first_haplosome_index];
			Haplosome *haplosome2 = haplosomes_[last_haplosome_index];
			int haplosome1_size = (haplosome1->IsNull() ? 0 : haplosome1->mutation_count());
			int haplosome2_size = (haplosome2->IsNull() ? 0 : haplosome2->mutation_count());
			
			if (haplosome1_size == 0)
			{
				vec_reserve_size += haplosome2_size;
			}
			else if (haplosome2_size == 0)
			{
				vec_reserve_size += haplosome1_size;
			}
			else
			{
				vec_reserve_size += (haplosome1_size + haplosome2_size);
				only_haploid_haplosomes = false;
			}
		}
	}
	
	if (vec_reserve_size == 0)
		return result_SP;
	
	if (only_haploid_haplosomes || (vec_reserve_size < 100))	// an arbitrary limit, but we don't want to make something *too* unnecessarily big...
		vec->reserve(vec_reserve_size);	
	
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	
	for (Chromosome *chromosome : species.Chromosomes())
	{
		int first_haplosome_index = species.FirstHaplosomeIndices()[chromosome->Index()];
		int last_haplosome_index = species.LastHaplosomeIndices()[chromosome->Index()];
		
		if (first_haplosome_index == last_haplosome_index)
		{
			// haploid chromosomes contain unique mutations by definition; add them all
			Haplosome *haplosome1 = haplosomes_[first_haplosome_index];
			
			if (!haplosome1->IsNull())
			{
				int mutrun_count = haplosome1->mutrun_count_;
				
				for (int run_index = 0; run_index < mutrun_count; ++run_index)
				{
					const MutationRun *mutrun1 = haplosome1->mutruns_[run_index];
					int g1_size = mutrun1->size();
					int g1_index = 0;
					
					while (g1_index < g1_size)
					{
						MutationIndex mut = (*mutrun1)[g1_index++];
						
						if ((mut_block_ptr + mut)->mutation_type_ptr_ == mutation_type_ptr)
							vec->push_object_element_RR(mut_block_ptr + mut);
					}
				}
			}
		}
		else
		{
			// diploid chromosomes require uniquing logic
			Haplosome *haplosome1 = haplosomes_[first_haplosome_index];
			Haplosome *haplosome2 = haplosomes_[last_haplosome_index];
			int haplosome1_size = (haplosome1->IsNull() ? 0 : haplosome1->mutation_count());
			int haplosome2_size = (haplosome2->IsNull() ? 0 : haplosome2->mutation_count());
			int mutrun_count = (haplosome1_size ? haplosome1->mutrun_count_ : haplosome2->mutrun_count_);
			
			for (int run_index = 0; run_index < mutrun_count; ++run_index)
			{
				// We want to interleave mutations from the two haplosomes, keeping only the uniqued mutations.  For a given position, we take mutations
				// from g1 first, and then look at the mutations in g2 at the same position and add them if they are not in g1.
				const MutationRun *mutrun1 = (haplosome1_size ? haplosome1->mutruns_[run_index] : nullptr);
				const MutationRun *mutrun2 = (haplosome2_size ? haplosome2->mutruns_[run_index] : nullptr);
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
						
						// Process mutations as long as both haplosomes still have mutations left in them
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
	 um2 = sortBy(unique(i.haplosomes.mutationsOfType(m1)), "position");
	 
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

//	*********************	- (object<Mutation>)mutationsFromHaplosomes(string$ category, [Nio<MutationType>$ mutType = NULL], [Niso<Chromosome> chromosomes = NULL])
//
EidosValue_SP Individual::ExecuteMethod_mutationsFromHaplosomes(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *category_value = p_arguments[0].get();
	EidosValue *mutType_value = p_arguments[1].get();
	EidosValue *chromosomes_value = p_arguments[2].get();
	
	Species &species = subpopulation_->species_;
	
	// parse category
	typedef enum _SLiMMutationFilteringCategory {
		kFilterUnique,
		kFilterHomozygous,
		kFilterHeterozygous,
		kFilterHemizygous,
		kFilterAll
	} SLiMMutationFilteringCategory;
	
	SLiMMutationFilteringCategory category;
	std::string category_string = category_value->StringAtIndex_NOCAST(0, nullptr);
	
	if (category_string == "unique")			category = SLiMMutationFilteringCategory::kFilterUnique;
	else if (category_string == "homozygous")	category = SLiMMutationFilteringCategory::kFilterHomozygous;
	else if (category_string == "heterozygous")	category = SLiMMutationFilteringCategory::kFilterHeterozygous;
	else if (category_string == "hemizygous")	category = SLiMMutationFilteringCategory::kFilterHemizygous;
	else if (category_string == "all")			category = SLiMMutationFilteringCategory::kFilterAll;
	else
		EIDOS_TERMINATION << "ERROR (Individual::ExecuteMethod_mutationsFromHaplosomes): mutationsFromHaplosomes() requires that category is 'unique', 'homozygous', 'heterozygous', 'hemizygous', or 'all'." << EidosTerminate();
	
	// parse mutType
	MutationType *mutation_type_ptr = nullptr;	// used if mutType_value is NULL, to indicate applicability to all mutation types
	
	if (mutType_value->Type() != EidosValueType::kValueNULL)
	{
		mutation_type_ptr = SLiM_ExtractMutationTypeFromEidosValue_io(mutType_value, 0, &species.community_, &species, "mutationsFromHaplosomes()");		// SPECIES CONSISTENCY CHECK
	}
	
	// parse chromosomes
	std::vector<slim_chromosome_index_t> chromosome_indices;
	
	species.GetChromosomeIndicesFromEidosValue(chromosome_indices, chromosomes_value);
	
	// loop through the chromosomes
	EidosValue_Object *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Mutation_Class));
	EidosValue_SP result_SP = EidosValue_SP(vec);
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	
	for (slim_chromosome_index_t chromosome_index : chromosome_indices)
	{
		Chromosome *chromosome = species.Chromosomes()[chromosome_index];
		int first_haplosome_index = species.FirstHaplosomeIndices()[chromosome_index];
		int last_haplosome_index = species.LastHaplosomeIndices()[chromosome_index];
		
		if (chromosome->IntrinsicPloidy() == 1)
		{
			// the chromosome is intrinsically haploid; add its mutations if category applies
			if ((category != SLiMMutationFilteringCategory::kFilterUnique) &&
				(category != SLiMMutationFilteringCategory::kFilterHomozygous) &&
				(category != SLiMMutationFilteringCategory::kFilterAll))
				continue;
			
			Haplosome *haplosome = haplosomes_[first_haplosome_index];
			
			if (haplosome->IsNull())
				continue;
			
			int mutrun_count = haplosome->mutrun_count_;
			
			for (int run_index = 0; run_index < mutrun_count; ++run_index)
			{
				const MutationRun *mutrun1 = haplosome->mutruns_[run_index];
				int g1_size = mutrun1->size();
				int g1_index = 0;
				
				while (g1_index < g1_size)
				{
					MutationIndex mut_index = (*mutrun1)[g1_index++];
					
					if (!mutation_type_ptr || ((mut_block_ptr + mut_index)->mutation_type_ptr_ == mutation_type_ptr))
						vec->push_object_element_RR(mut_block_ptr + mut_index);
				}
			}
		}
		else
		{
			// the chromosome is intrinsically diploid
			Haplosome *haplosome1 = haplosomes_[first_haplosome_index];
			Haplosome *haplosome2 = haplosomes_[last_haplosome_index];
			
			if (haplosome1->IsNull() && haplosome2->IsNull())
			{
				// both haplosomes are null; skip this chromosome
				continue;
			}
			else if (haplosome1->IsNull() || haplosome2->IsNull())
			{
				// exactly one haplosome is null; hemizygous case
				if ((category != SLiMMutationFilteringCategory::kFilterUnique) &&
					(category != SLiMMutationFilteringCategory::kFilterHemizygous) &&
					(category != SLiMMutationFilteringCategory::kFilterAll))
					continue;
				
				Haplosome *haplosome = haplosome1->IsNull() ? haplosome2 : haplosome1;
				int mutrun_count = haplosome->mutrun_count_;
				
				for (int run_index = 0; run_index < mutrun_count; ++run_index)
				{
					const MutationRun *mutrun1 = haplosome->mutruns_[run_index];
					int g1_size = mutrun1->size();
					int g1_index = 0;
					
					while (g1_index < g1_size)
					{
						MutationIndex mut_index = (*mutrun1)[g1_index++];
						
						if (!mutation_type_ptr || ((mut_block_ptr + mut_index)->mutation_type_ptr_ == mutation_type_ptr))
							vec->push_object_element_RR(mut_block_ptr + mut_index);
					}
				}
			}
			else
			{
				// two non-null haplosomes; run through them in synchrony
				// this code is adapted from Subpopulation::_Fitness_DiploidChromosome()
				if ((category != SLiMMutationFilteringCategory::kFilterUnique) &&
					(category != SLiMMutationFilteringCategory::kFilterHomozygous) &&
					(category != SLiMMutationFilteringCategory::kFilterHeterozygous) &&
					(category != SLiMMutationFilteringCategory::kFilterAll))
					continue;
				
				// set flags that we can quickly check for whether we are pushing particular mutations or not
				bool push_homozygous = ((category == SLiMMutationFilteringCategory::kFilterHomozygous) ||
										(category == SLiMMutationFilteringCategory::kFilterUnique) ||
										(category == SLiMMutationFilteringCategory::kFilterAll));
				bool push_heterozygous = ((category == SLiMMutationFilteringCategory::kFilterHeterozygous) ||
										  (category == SLiMMutationFilteringCategory::kFilterUnique) ||
										  (category == SLiMMutationFilteringCategory::kFilterAll));
				
				// both haplosomes are being modeled, so we need to scan through and figure out which mutations are heterozygous and which are homozygous
				const int32_t mutrun_count = haplosome1->mutrun_count_;
				
				for (int run_index = 0; run_index < mutrun_count; ++run_index)
				{
					const MutationRun *mutrun1 = haplosome1->mutruns_[run_index];
					const MutationRun *mutrun2 = haplosome2->mutruns_[run_index];
					
					// Read directly from the MutationRun buffers
					const MutationIndex *haplosome1_iter = mutrun1->begin_pointer_const();
					const MutationIndex *haplosome2_iter = mutrun2->begin_pointer_const();
					
					const MutationIndex *haplosome1_max = mutrun1->end_pointer_const();
					const MutationIndex *haplosome2_max = mutrun2->end_pointer_const();
					
					// first, handle the situation before either haplosome iterator has reached the end of its haplosome, for simplicity/speed
					if (haplosome1_iter != haplosome1_max && haplosome2_iter != haplosome2_max)
					{
						MutationIndex haplosome1_mutindex = *haplosome1_iter, haplosome2_mutindex = *haplosome2_iter;
						slim_position_t haplosome1_iter_position = (mut_block_ptr + haplosome1_mutindex)->position_, haplosome2_iter_position = (mut_block_ptr + haplosome2_mutindex)->position_;
						
						do
						{
							if (haplosome1_iter_position < haplosome2_iter_position)
							{
								// Process a mutation in haplosome1 since it is leading
								if (push_heterozygous)
									if (!mutation_type_ptr || ((mut_block_ptr + haplosome1_mutindex)->mutation_type_ptr_ == mutation_type_ptr))
										vec->push_object_element_RR(mut_block_ptr + haplosome1_mutindex);
								
								if (++haplosome1_iter == haplosome1_max)
									break;
								else {
									haplosome1_mutindex = *haplosome1_iter;
									haplosome1_iter_position = (mut_block_ptr + haplosome1_mutindex)->position_;
								}
							}
							else if (haplosome1_iter_position > haplosome2_iter_position)
							{
								// Process a mutation in haplosome2 since it is leading
								if (push_heterozygous)
									if (!mutation_type_ptr || ((mut_block_ptr + haplosome2_mutindex)->mutation_type_ptr_ == mutation_type_ptr))
										vec->push_object_element_RR(mut_block_ptr + haplosome2_mutindex);
								
								if (++haplosome2_iter == haplosome2_max)
									break;
								else {
									haplosome2_mutindex = *haplosome2_iter;
									haplosome2_iter_position = (mut_block_ptr + haplosome2_mutindex)->position_;
								}
							}
							else
							{
								// Look for homozygosity: haplosome1_iter_position == haplosome2_iter_position
								slim_position_t position = haplosome1_iter_position;
								const MutationIndex *haplosome1_start = haplosome1_iter;
								
								// advance through haplosome1 as long as we remain at the same position, handling one mutation at a time
								do
								{
									const MutationIndex *haplosome2_matchscan = haplosome2_iter; 
									
									// advance through haplosome2 with haplosome2_matchscan, looking for a match for the current mutation in haplosome1, to determine whether we are homozygous or not
									while (haplosome2_matchscan != haplosome2_max && (mut_block_ptr + *haplosome2_matchscan)->position_ == position)
									{
										if (haplosome1_mutindex == *haplosome2_matchscan)
										{
											// a homozygous match was found
											if (push_homozygous)
												if (!mutation_type_ptr || ((mut_block_ptr + haplosome1_mutindex)->mutation_type_ptr_ == mutation_type_ptr))
													vec->push_object_element_RR(mut_block_ptr + haplosome1_mutindex);
											
											// push a second copy only if we're doing category "all"
											if (category == SLiMMutationFilteringCategory::kFilterAll)
												if (!mutation_type_ptr || ((mut_block_ptr + haplosome1_mutindex)->mutation_type_ptr_ == mutation_type_ptr))
													vec->push_object_element_RR(mut_block_ptr + haplosome1_mutindex);
											goto homozygousExit1;
										}
										
										haplosome2_matchscan++;
									}
									
									// no match was found, so we are heterozygous
									if (push_heterozygous)
										if (!mutation_type_ptr || ((mut_block_ptr + haplosome1_mutindex)->mutation_type_ptr_ == mutation_type_ptr))
											vec->push_object_element_RR(mut_block_ptr + haplosome1_mutindex);
									
								homozygousExit1:
									
									if (++haplosome1_iter == haplosome1_max)
										break;
									else {
										haplosome1_mutindex = *haplosome1_iter;
										haplosome1_iter_position = (mut_block_ptr + haplosome1_mutindex)->position_;
									}
								} while (haplosome1_iter_position == position);
								
								// advance through haplosome2 as long as we remain at the same position, handling one mutation at a time
								do
								{
									const MutationIndex *haplosome1_matchscan = haplosome1_start; 
									
									// advance through haplosome1 with haplosome1_matchscan, looking for a match for the current mutation in haplosome2, to determine whether we are homozygous or not
									while (haplosome1_matchscan != haplosome1_max && (mut_block_ptr + *haplosome1_matchscan)->position_ == position)
									{
										if (haplosome2_mutindex == *haplosome1_matchscan)
										{
											// a homozygous match was found; we know this match was already found by the haplosome1 loop above
											goto homozygousExit2;
										}
										
										haplosome1_matchscan++;
									}
									
									// no match was found, so we are heterozygous; we multiply our fitness by the selection coefficient and the dominance coefficient
									if (push_heterozygous)
										if (!mutation_type_ptr || ((mut_block_ptr + haplosome2_mutindex)->mutation_type_ptr_ == mutation_type_ptr))
											vec->push_object_element_RR(mut_block_ptr + haplosome2_mutindex);
									
								homozygousExit2:
									
									if (++haplosome2_iter == haplosome2_max)
										break;
									else {
										haplosome2_mutindex = *haplosome2_iter;
										if (!mutation_type_ptr || ((mut_block_ptr + haplosome2_mutindex)->mutation_type_ptr_ == mutation_type_ptr))
											haplosome2_iter_position = (mut_block_ptr + haplosome2_mutindex)->position_;
									}
								} while (haplosome2_iter_position == position);
								
								// break out if either haplosome has reached its end
								if (haplosome1_iter == haplosome1_max || haplosome2_iter == haplosome2_max)
									break;
							}
						} while (true);
					}
					
					// one or the other haplosome has now reached its end, so now we just need to handle the remaining mutations in the unfinished haplosome
#if DEBUG
					assert(!(haplosome1_iter != haplosome1_max && haplosome2_iter != haplosome2_max));
#endif
					
					// if haplosome1 is unfinished, finish it
					while (haplosome1_iter != haplosome1_max)
					{
						MutationIndex haplosome1_mutindex = *haplosome1_iter++;
						
						if (push_heterozygous)
							if (!mutation_type_ptr || ((mut_block_ptr + haplosome1_mutindex)->mutation_type_ptr_ == mutation_type_ptr))
								vec->push_object_element_RR(mut_block_ptr + haplosome1_mutindex);
					}
					
					// if haplosome2 is unfinished, finish it
					while (haplosome2_iter != haplosome2_max)
					{
						MutationIndex haplosome2_mutindex = *haplosome2_iter++;
						
						if (push_heterozygous)
							if (!mutation_type_ptr || ((mut_block_ptr + haplosome2_mutindex)->mutation_type_ptr_ == mutation_type_ptr))
								vec->push_object_element_RR(mut_block_ptr + haplosome2_mutindex);
					}
				}
			}
		}
	}
	
	return result_SP;
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
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_haplosomes,				true,	kEidosValueMaskObject, gSLiM_Haplosome_Class))->DeclareAcceleratedGet(Individual::GetProperty_Accelerated_haplosomes));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_haplosomesNonNull,		true,	kEidosValueMaskObject, gSLiM_Haplosome_Class))->DeclareAcceleratedGet(Individual::GetProperty_Accelerated_haplosomesNonNull));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_haploidGenome1,	true,	kEidosValueMaskObject, gSLiM_Haplosome_Class))->DeclareAcceleratedGet(Individual::GetProperty_Accelerated_haploidGenome1));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_haploidGenome2,	true,	kEidosValueMaskObject, gSLiM_Haplosome_Class))->DeclareAcceleratedGet(Individual::GetProperty_Accelerated_haploidGenome2));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_haploidGenome1NonNull,	true,	kEidosValueMaskObject, gSLiM_Haplosome_Class))->DeclareAcceleratedGet(Individual::GetProperty_Accelerated_haploidGenome1NonNull));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_haploidGenome2NonNull,	true,	kEidosValueMaskObject, gSLiM_Haplosome_Class))->DeclareAcceleratedGet(Individual::GetProperty_Accelerated_haploidGenome2NonNull));
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
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_relatedness, kEidosValueMaskFloat))->AddObject("individuals", gSLiM_Individual_Class)->AddArgWithDefault(kEidosValueMaskNULL | kEidosValueMaskInt | kEidosValueMaskString | kEidosValueMaskObject | kEidosValueMaskOptional | kEidosValueMaskSingleton, "chromosome", gSLiM_Chromosome_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_haplosomesForChromosomes, kEidosValueMaskObject, gSLiM_Haplosome_Class))->AddArgWithDefault(kEidosValueMaskNULL | kEidosValueMaskInt | kEidosValueMaskString | kEidosValueMaskObject | kEidosValueMaskOptional, "chromosomes", gSLiM_Chromosome_Class, gStaticEidosValueNULL)->AddInt_OSN("index", gStaticEidosValueNULL)->AddLogical_OS("includeNulls", gStaticEidosValue_LogicalT));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_sharedParentCount, kEidosValueMaskInt))->AddObject("individuals", gSLiM_Individual_Class));
		methods->emplace_back(((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_sumOfMutationsOfType, kEidosValueMaskFloat | kEidosValueMaskSingleton))->AddIntObject_S("mutType", gSLiM_MutationType_Class))->DeclareAcceleratedImp(Individual::ExecuteMethod_Accelerated_sumOfMutationsOfType));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_uniqueMutationsOfType, kEidosValueMaskObject, gSLiM_Mutation_Class))->AddIntObject_S("mutType", gSLiM_MutationType_Class)->MarkDeprecated());
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_mutationsFromHaplosomes, kEidosValueMaskObject, gSLiM_Mutation_Class))->AddString_S("category")->AddIntObject_OSN("mutType", gSLiM_MutationType_Class, gStaticEidosValueNULL)->AddArgWithDefault(kEidosValueMaskNULL | kEidosValueMaskInt | kEidosValueMaskString | kEidosValueMaskObject | kEidosValueMaskOptional, "chromosomes", gSLiM_Chromosome_Class, gStaticEidosValueNULL));
		
		methods->emplace_back((EidosClassMethodSignature *)(new EidosClassMethodSignature(gStr_outputIndividuals, kEidosValueMaskVOID))->AddString_OSN(gEidosStr_filePath, gStaticEidosValueNULL)->AddLogical_OS("append", gStaticEidosValue_LogicalF)->AddArgWithDefault(kEidosValueMaskNULL | kEidosValueMaskInt | kEidosValueMaskString | kEidosValueMaskObject | kEidosValueMaskOptional | kEidosValueMaskSingleton, "chromosome", gSLiM_Chromosome_Class, gStaticEidosValueNULL)->AddLogical_OS("spatialPositions", gStaticEidosValue_LogicalT)->AddLogical_OS("ages", gStaticEidosValue_LogicalT)->AddLogical_OS("ancestralNucleotides", gStaticEidosValue_LogicalF)->AddLogical_OS("pedigreeIDs", gStaticEidosValue_LogicalF)->AddLogical_OS("objectTags", gStaticEidosValue_LogicalF));
		methods->emplace_back((EidosClassMethodSignature *)(new EidosClassMethodSignature(gStr_outputIndividualsToVCF, kEidosValueMaskVOID))->AddString_OSN(gEidosStr_filePath, gStaticEidosValueNULL)->AddLogical_OS("append", gStaticEidosValue_LogicalF)->AddArgWithDefault(kEidosValueMaskNULL | kEidosValueMaskInt | kEidosValueMaskString | kEidosValueMaskObject | kEidosValueMaskOptional | kEidosValueMaskSingleton, "chromosome", gSLiM_Chromosome_Class, gStaticEidosValueNULL)->AddLogical_OS("outputMultiallelics", gStaticEidosValue_LogicalT)->AddLogical_OS("simplifyNucleotides", gStaticEidosValue_LogicalF)->AddLogical_OS("outputNonnucleotides", gStaticEidosValue_LogicalT));
		methods->emplace_back((EidosClassMethodSignature *)(new EidosClassMethodSignature(gStr_readIndividualsFromVCF, kEidosValueMaskObject, gSLiM_Mutation_Class))->AddString_S(gEidosStr_filePath)->AddIntObject_OSN("mutationType", gSLiM_MutationType_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosClassMethodSignature *)(new EidosClassMethodSignature(gStr_setSpatialPosition, kEidosValueMaskVOID))->AddFloat("position"));
		
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}

EidosValue_SP Individual_Class::ExecuteClassMethod(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) const
{
	switch (p_method_id)
	{
		case gID_outputIndividuals:			return ExecuteMethod_outputIndividuals(p_method_id, p_target, p_arguments, p_interpreter);
		case gID_outputIndividualsToVCF:	return ExecuteMethod_outputIndividualsToVCF(p_method_id, p_target, p_arguments, p_interpreter);
		case gID_readIndividualsFromVCF:	return ExecuteMethod_readIndividualsFromVCF(p_method_id, p_target, p_arguments, p_interpreter);
		case gID_setSpatialPosition:		return ExecuteMethod_setSpatialPosition(p_method_id, p_target, p_arguments, p_interpreter);
		default:
		{
			// In a sense, we here "subclass" EidosDictionaryUnretained_Class to override setValuesVectorized(); we set a flag remembering that
			// an individual's dictionary has been modified, and then we call "super" for the usual behavior.
			if (p_method_id == gEidosID_setValuesVectorized)
				Individual::s_any_individual_dictionary_set_ = true;
			
			return super::ExecuteClassMethod(p_method_id, p_target, p_arguments, p_interpreter);
		}
	}
}

//	*********************	+ (void)outputIndividuals([Ns$ filePath = NULL], [logical$ append=F], [Niso<Chromosome>$ chromosome = NULL], [logical$ spatialPositions = T], [logical$ ages = T], [logical$ ancestralNucleotides = F], [logical$ pedigreeIDs = F], [logical$ objectTags = F])
//
EidosValue_SP Individual_Class::ExecuteMethod_outputIndividuals(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) const
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *filePath_value = p_arguments[0].get();
	EidosValue *append_value = p_arguments[1].get();
	EidosValue *chromosome_value = p_arguments[2].get();
	EidosValue *spatialPositions_value = p_arguments[3].get();
	EidosValue *ages_value = p_arguments[4].get();
	EidosValue *ancestralNucleotides_value = p_arguments[5].get();
	EidosValue *pedigreeIDs_value = p_arguments[6].get();
	EidosValue *objectTags_value = p_arguments[7].get();
	
	// here we need to require at least one target individual,
	// do a species consistency check and get the species/community,
	// and get the vector of individuals that we will pass in
	// (from the raw data of the EidosValue, no need to copy; but add const)
	int individuals_count = p_target->Count();
	
	if (individuals_count == 0)
		EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_outputIndividuals): outputIndividuals() cannot be called on a zero-length target vector; at least one individual is required." << EidosTerminate();
	
	const Individual **individuals_buffer = (const Individual **)p_target->ObjectData();
	
	// SPECIES CONSISTENCY CHECK
	Species *species = Community::SpeciesForIndividuals(p_target);
	
	if (!species)
		EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_outputIndividuals): outputIndividuals() requires that all individuals belong to the same species." << EidosTerminate();
	
	Community &community = species->community_;
	
	// TIMING RESTRICTION
	if (!community.warned_early_output_)
	{
		if ((community.CycleStage() == SLiMCycleStage::kWFStage0ExecuteFirstScripts) ||
			(community.CycleStage() == SLiMCycleStage::kWFStage1ExecuteEarlyScripts))
		{
			if (!gEidosSuppressWarnings)
			{
				p_interpreter.ErrorOutputStream() << "#WARNING (Individual_Class::ExecuteMethod_outputIndividuals): outputIndividuals() should probably not be called from a first() or early() event in a WF model; the output will reflect state at the beginning of the cycle, not the end." << std::endl;
				community.warned_early_output_ = true;
			}
		}
	}
	
	Chromosome *chromosome = species->GetChromosomeFromEidosValue(chromosome_value);	// NULL returns nullptr
	
	bool output_spatial_positions = spatialPositions_value->LogicalAtIndex_NOCAST(0, nullptr);
	bool output_ages = ages_value->LogicalAtIndex_NOCAST(0, nullptr);
	bool output_ancestral_nucs = ancestralNucleotides_value->LogicalAtIndex_NOCAST(0, nullptr);
	bool output_pedigree_ids = pedigreeIDs_value->LogicalAtIndex_NOCAST(0, nullptr);
	bool output_object_tags = objectTags_value->LogicalAtIndex_NOCAST(0, nullptr);
	
	if (output_pedigree_ids && !species->PedigreesEnabledByUser())
		EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_outputIndividuals): outputIndividuals() cannot output pedigree IDs, because pedigree recording has not been enabled." << EidosTerminate();
	
	if (filePath_value->Type() == EidosValueType::kValueNULL)
	{
		// before writing anything, erase a progress line if we've got one up, to try to make a clean slate
		Eidos_EraseProgress();
		
		std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
		
		Individual::PrintIndividuals_SLiM(output_stream, individuals_buffer, individuals_count, *species, output_spatial_positions, output_ages, output_ancestral_nucs, output_pedigree_ids, output_object_tags, /* p_output_substitutions */ false, chromosome);
	}
	else
	{
		std::string outfile_path = Eidos_ResolvedPath(filePath_value->StringAtIndex_NOCAST(0, nullptr));
		bool append = append_value->LogicalAtIndex_NOCAST(0, nullptr);
		std::ofstream outfile;
		
		outfile.open(outfile_path.c_str(), append ? (std::ios_base::app | std::ios_base::out) : std::ios_base::out);
		
		if (outfile.is_open())
		{
			Individual::PrintIndividuals_SLiM(outfile, individuals_buffer, individuals_count, *species, output_spatial_positions, output_ages, output_ancestral_nucs, output_pedigree_ids, output_object_tags, /* p_output_substitutions */ false, chromosome);
			
			outfile.close(); 
		}
		else
		{
			EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_outputIndividuals): outputIndividuals() could not open " << outfile_path << "." << EidosTerminate();
		}
	}
	
	return gStaticEidosValueVOID;
}

//	*********************	+ (void)outputIndividualsToVCF([Ns$ filePath = NULL], [logical$ append = F], [Niso<Chromosome>$ chromosome = NULL], [logical$ outputMultiallelics = T], [logical$ simplifyNucleotides = F], [logical$ outputNonnucleotides = T])
//
EidosValue_SP Individual_Class::ExecuteMethod_outputIndividualsToVCF(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) const
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *filePath_value = p_arguments[0].get();
	EidosValue *append_value = p_arguments[1].get();
	EidosValue *chromosome_value = p_arguments[2].get();
	EidosValue *outputMultiallelics_value = p_arguments[3].get();
	EidosValue *simplifyNucleotides_value = p_arguments[4].get();
	EidosValue *outputNonnucleotides_value = p_arguments[5].get();
	
	// here we need to require at least one target individual,
	// do a species consistency check and get the species/community,
	// and get the vector of individuals that we will pass in
	// (from the raw data of the EidosValue, no need to copy; but add const)
	int individuals_count = p_target->Count();
	
	if (individuals_count == 0)
		EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_outputIndividualsToVCF): outputIndividualsToVCF() cannot be called on a zero-length target vector; at least one individual is required." << EidosTerminate();
	
	const Individual **individuals_buffer = (const Individual **)p_target->ObjectData();
	
	// SPECIES CONSISTENCY CHECK
	Species *species = Community::SpeciesForIndividuals(p_target);
	
	if (!species)
		EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_outputIndividualsToVCF): outputIndividualsToVCF() requires that all individuals belong to the same species." << EidosTerminate();
	
	Community &community = species->community_;
	
	// TIMING RESTRICTION
	if (!community.warned_early_output_)
	{
		if ((community.CycleStage() == SLiMCycleStage::kWFStage0ExecuteFirstScripts) ||
			(community.CycleStage() == SLiMCycleStage::kWFStage1ExecuteEarlyScripts))
		{
			if (!gEidosSuppressWarnings)
			{
				p_interpreter.ErrorOutputStream() << "#WARNING (Individual_Class::ExecuteMethod_outputIndividualsToVCF): outputIndividualsToVCF() should probably not be called from a first() or early() event in a WF model; the output will reflect state at the beginning of the cycle, not the end." << std::endl;
				community.warned_early_output_ = true;
			}
		}
	}
	
	Chromosome *chromosome = species->GetChromosomeFromEidosValue(chromosome_value);	// NULL returns nullptr
	
	bool output_multiallelics = outputMultiallelics_value->LogicalAtIndex_NOCAST(0, nullptr);
	bool simplify_nucs = simplifyNucleotides_value->LogicalAtIndex_NOCAST(0, nullptr);
	bool output_nonnucs = outputNonnucleotides_value->LogicalAtIndex_NOCAST(0, nullptr);
	
	if (filePath_value->Type() == EidosValueType::kValueNULL)
	{
		// before writing anything, erase a progress line if we've got one up, to try to make a clean slate
		Eidos_EraseProgress();
		
		std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
		
		// write the #OUT line, for file output only
		output_stream << "#OUT: " << community.Tick() << " " << species->Cycle() << " IS" << std::endl;
		
		Individual::PrintIndividuals_VCF(output_stream, individuals_buffer, individuals_count, *species, output_multiallelics, simplify_nucs, output_nonnucs, chromosome);
	}
	else
	{
		std::string outfile_path = Eidos_ResolvedPath(filePath_value->StringAtIndex_NOCAST(0, nullptr));
		bool append = append_value->LogicalAtIndex_NOCAST(0, nullptr);
		std::ofstream outfile;
		
		outfile.open(outfile_path.c_str(), append ? (std::ios_base::app | std::ios_base::out) : std::ios_base::out);
		
		if (outfile.is_open())
		{
			Individual::PrintIndividuals_VCF(outfile, individuals_buffer, individuals_count, *species, output_multiallelics, simplify_nucs, output_nonnucs, chromosome);
			
			outfile.close(); 
		}
		else
		{
			EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_outputIndividuals): outputIndividuals() could not open " << outfile_path << "." << EidosTerminate();
		}
	}
	return gStaticEidosValueVOID;
}

// this is to avoid copy-pasting the same addition code repeatedly; it is gross, but should get inlined
inline __attribute__((always_inline)) static void
_AddCallToHaplosome(int call, Haplosome *haplosome, slim_mutrun_index_t &haplosome_last_mutrun_modified, MutationRun *&haplosome_last_mutrun,
					std::vector<MutationIndex> &alt_allele_mut_indices, slim_position_t mut_position, Species *species, MutationRunContext *mutrun_context,
					bool all_target_haplosomes_started_empty, bool recording_mutations)
{
	if (call == 0)
		return;
	
	slim_position_t mutrun_length = haplosome->MutrunLength();
	MutationIndex mut_index = alt_allele_mut_indices[call - 1];
	slim_mutrun_index_t mut_mutrun_index = (slim_mutrun_index_t)(mut_position / mutrun_length);
	
	if (mut_mutrun_index != haplosome_last_mutrun_modified)
	{
#ifdef _OPENMP
		// When parallel, the MutationRunContext depends upon the position in the haplosome
		mutrun_context = &species->ChromosomeMutationRunContextForMutationRunIndex(mut_mutrun_index);
#endif
		
		// We use WillModifyRun() because these are existing haplosomes we didn't create, and their runs may be shared; we have
		// no way to tell.  We avoid making excessive mutation run copies by calling this only once per mutrun per haplosome.
		haplosome_last_mutrun = haplosome->WillModifyRun(mut_mutrun_index, *mutrun_context);
		haplosome_last_mutrun_modified = mut_mutrun_index;
	}
	
	// If the haplosome started empty, we can add mutations to the end with emplace_back(); if it did not, then they need to be inserted
	if (all_target_haplosomes_started_empty)
		haplosome_last_mutrun->emplace_back(mut_index);
	else
		haplosome_last_mutrun->insert_sorted_mutation(mut_index);
	
	if (recording_mutations)
		species->RecordNewDerivedState(haplosome, mut_position, *haplosome->derived_mutation_ids_at_position(mut_position));
}

//	*********************	+ (o<Mutation>)readIndividualsFromVCF(s$ filePath = NULL, [Nio<MutationType> mutationType = NULL])
//
EidosValue_SP Individual_Class::ExecuteMethod_readIndividualsFromVCF(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) const
{
#pragma unused (p_method_id, p_interpreter)
	// BEWARE: This method shares a great deal of code with Haplosome_Class::ExecuteMethod_readHaplosomesFromVCF().  Maintain in parallel.
	THREAD_SAFETY_IN_ACTIVE_PARALLEL("Individual_Class::ExecuteMethod_readIndividualsFromVCF(): SLiM global state read");
	
	EidosValue *filePath_value = p_arguments[0].get();
	EidosValue *mutationType_value = p_arguments[1].get();
	
	// SPECIES CONSISTENCY CHECK
	if (p_target->Count() == 0)
		EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_readIndividualsFromVCF): " << "readIndividualsFromVCF() requires a target Individual vector of length 1 or more, so that the species of the target can be determined." << EidosTerminate();
	
	Species *species = Community::SpeciesForIndividuals(p_target);
	
	if (!species)
		EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_readIndividualsFromVCF): " << "readIndividualsFromVCF() requires that all target individuals belong to the same species." << EidosTerminate();
	
	Individual * const *individuals_data = (Individual * const *)p_target->ObjectData();
	int individuals_size = p_target->Count();
	
	species->population_.CheckForDeferralInIndividualsVector(individuals_data, individuals_size, "Individual_Class::ExecuteMethod_readIndividualsFromVCF");
	
	const std::vector<Chromosome *> &chromosomes = species->Chromosomes();
	bool model_is_multi_chromosome = (chromosomes.size() > 1);
	std::string chromosome_symbol;		// used in single-chromosome models to check consistency
	
	Community &community = species->community_;
	Population &pop = species->population_;
	bool recording_mutations = species->RecordingTreeSequenceMutations();
	bool nucleotide_based = species->IsNucleotideBased();
	std::string file_path = Eidos_ResolvedPath(Eidos_StripTrailingSlash(filePath_value->StringAtIndex_NOCAST(0, nullptr)));
	bool has_initial_mutations = (gSLiM_next_mutation_id != 0);
	MutationType *default_mutation_type_ptr = nullptr;
	
	if (mutationType_value->Type() != EidosValueType::kValueNULL)
		default_mutation_type_ptr = SLiM_ExtractMutationTypeFromEidosValue_io(mutationType_value, 0, &community, species, "readIndividualsFromVCF()");			// SPECIES CONSISTENCY CHECK
	
	// Parse the whole input file and retain the information from it
	std::ifstream infile(file_path);
	
	if (!infile)
		EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_readIndividualsFromVCF): could not read file at path " << file_path << "." << EidosTerminate();
	
	std::string line, sub;
	int parse_state = 0;
	int sample_id_count = 0;
	bool info_MID_defined = false, info_S_defined = false, info_DOM_defined = false, info_PO_defined = false;
	bool info_GO_defined = false, info_TO_defined = false, info_MT_defined = false, /*info_AA_defined = false,*/ info_NONNUC_defined = false;
	
	// This data structure keeps call lines that we read for each chromosome.  They could arrive from the file
	// in any order; with this we store them by chromosome, and then indexed by their mutation position.
	std::vector<std::vector<std::pair<slim_position_t, std::string>>> call_lines_per_chromosome;
	
	call_lines_per_chromosome.resize(chromosomes.size());	// make an empty call_lines vector for each chromosome
	
	while (!infile.eof())
	{
		getline(infile, line);
		
		switch (parse_state)
		{
			case 0:
			{
				// In header, parsing ## lines, until we get to the #CHROM line; the point of this is that we only want to interpret
				// INFO fields like MID, S, etc. as having their SLiM-specific meaning if their SLiM-specific definition is present
				if (line.compare(0, 2, "##") == 0)
				{
					if (line == "##INFO=<ID=MID,Number=.,Type=Integer,Description=\"Mutation ID in SLiM\">")	info_MID_defined = true;
					if (line == "##INFO=<ID=S,Number=.,Type=Float,Description=\"Selection Coefficient\">")		info_S_defined = true;
					if (line == "##INFO=<ID=DOM,Number=.,Type=Float,Description=\"Dominance\">")				info_DOM_defined = true;
					if (line == "##INFO=<ID=PO,Number=.,Type=Integer,Description=\"Population of Origin\">")	info_PO_defined = true;
					if (line == "##INFO=<ID=GO,Number=.,Type=Integer,Description=\"Generation of Origin\">")	info_GO_defined = true;
					if (line == "##INFO=<ID=TO,Number=.,Type=Integer,Description=\"Tick of Origin\">")			info_TO_defined = true;		// SLiM 4 emits TO (tick) instead of GO (generation)
					if (line == "##INFO=<ID=MT,Number=.,Type=Integer,Description=\"Mutation Type\">")			info_MT_defined = true;
					/*if (line == "##INFO=<ID=AA,Number=1,Type=String,Description=\"Ancestral Allele\">")			info_AA_defined = true;*/		// this one is standard, so we don't require this definition
					if (line == "##INFO=<ID=NONNUC,Number=0,Type=Flag,Description=\"Non-nucleotide-based\">")	info_NONNUC_defined = true;
				}
				else if (line.compare(0, 1, "#") == 0)
				{
					static const char *header_fields[9] = {"CHROM", "POS", "ID", "REF", "ALT", "QUAL", "FILTER", "INFO", "FORMAT"};
					std::istringstream iss(line);
					
					iss.get();	// eat the initial #
					
					// verify that the expected standard columns are present
					for (const char *header_field : header_fields)
					{
						if (!(iss >> sub))
							EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_readIndividualsFromVCF): missing VCF header '" << header_field << "'." << EidosTerminate();
						if (sub != header_field)
							EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_readIndividualsFromVCF): expected VCF header '" << header_field << "', saw '" << sub << "'." << EidosTerminate();
					}
					
					// the remaining columns are sample IDs; we don't care what they are, we just count them
					while (iss >> sub)
						sample_id_count++;
					
					if (sample_id_count != individuals_size)
						EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_readIndividualsFromVCF): there are " << sample_id_count << " samples in the VCF file, but " << individuals_size << " target individuals; the number of target individuals must match the number of VCF samples." << EidosTerminate();
					
					// now the remainder of the file should be call lines
					parse_state = 1;
				}
				else
				{
					EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_readIndividualsFromVCF): unexpected line in VCF header: '" << line << "'." << EidosTerminate();
				}
				break;
			}
			case 1:
			{
				// In call lines, fields are separated by tabs, and could theoretically contain spaces; here we just read a whole line,
				// extract the position field for the mutation, and save the line indexed by its mutation's position for later handling
				if (line.length() == 0)
					break;
				
				std::istringstream iss(line);
				
				std::getline(iss, sub, '\t');	// CHROM
				
				Chromosome *chromosome_for_call = species->ChromosomeFromSymbol(sub);
				
				if (model_is_multi_chromosome)
				{
					// in multi-chromosome models the CHROM value must match match a chromosome in the model
					if (!chromosome_for_call)
						EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_readIndividualsFromVCF): the CHROM field's value (\"" << sub << "\") in a call line does not match any chromosome symbol for the focal species with which the target individuals are associated.  In multi-chromosome models, the CHROM field is required to match a chromosome symbol to prevent bugs." << EidosTerminate();
				}
				else
				{
					// in single-chromosome models the CHROM value must be consistent across the whole file, but need not match
					if (chromosome_symbol.length() == 0)
						chromosome_symbol = sub;	// first call line's CHROM symbol gets remembered
					else if (sub != chromosome_symbol)
						EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_readIndividualsFromVCF): the CHROM field's value (\"" << sub << "\") in a call line does not match the initial CHROM field's value (\"" << chromosome_symbol << "\").  In single-chromosome models, the CHROM field is required to have a single consistent value across all call lines to prevent bugs." << EidosTerminate();
					
					if (!chromosome_for_call)
						chromosome_for_call = chromosomes[0];
				}
				
				slim_chromosome_index_t chromosome_index = chromosome_for_call->Index();
				slim_position_t last_position = chromosome_for_call->last_position_;
				
				std::getline(iss, sub, '\t');	// POS
				
				int64_t pos = EidosInterpreter::NonnegativeIntegerForString(sub, nullptr) - 1;		// -1 because VCF uses 1-based positions
				
				if ((pos < 0) || (pos > last_position))
					EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_readIndividualsFromVCF): VCF file POS value " << pos << " out of range." << EidosTerminate();
				
				std::vector<std::pair<slim_position_t, std::string>> &call_lines = call_lines_per_chromosome[chromosome_index];
				
				call_lines.emplace_back(pos, line);
				break;
			}
			default:
				EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_readIndividualsFromVCF): (internal error) unhandled case." << EidosTerminate();
		}
	}
	
	infile.close();
	
	// We will keep track of all mutations added, to all chromosomes, and return them as a vector
	std::vector<MutationIndex> mutation_indices;
	
	// Loop over the call lines for each chromosome, and handle chromosomes one by one
	// FIXME: from here downwards could probably be parallelized
	for (size_t chromosome_index = 0; chromosome_index < chromosomes.size(); ++chromosome_index)
	{
		std::vector<std::pair<slim_position_t, std::string>> &call_lines = call_lines_per_chromosome[chromosome_index];
		
		if (call_lines.size() == 0)
			continue;
		
		Chromosome *chromosome = species->Chromosomes()[chromosome_index];
		int first_haplosome_index = species->FirstHaplosomeIndices()[chromosome_index];
		int last_haplosome_index = species->LastHaplosomeIndices()[chromosome_index];
		int intrinsic_ploidy = (last_haplosome_index - first_haplosome_index) + 1;
		ChromosomeType chromosome_type = chromosome->Type();
		// sort call_lines by position, so that we can add them to empty haplosomes efficiently
		std::sort(call_lines.begin(), call_lines.end(), [ ](const std::pair<slim_position_t, std::string> &l1, const std::pair<slim_position_t, std::string> &l2) {return l1.first < l2.first;});
		
		// cache target haplosomes and determine whether they are initially empty, in which case we can do fast mutation addition with emplace_back()
		// NOTE that unlike readHaplosomesFromVCF(), we do not exclude null haplosomes here!
		std::vector<Haplosome *> haplosomes;
		std::vector<slim_mutrun_index_t> haplosomes_last_mutrun_modified;
		std::vector<MutationRun *> haplosomes_last_mutrun;
		bool all_target_haplosomes_started_empty = true;
		
		for (int individuals_index = 0; individuals_index < individuals_size; ++individuals_index)
		{
			Individual *ind = individuals_data[individuals_index];
			
			{
				Haplosome *haplosome = ind->haplosomes_[first_haplosome_index];
				
				if (haplosome->IsNull())
				{
					haplosomes.emplace_back(nullptr);
					haplosomes_last_mutrun_modified.emplace_back(-1);
					haplosomes_last_mutrun.emplace_back(nullptr);
				}
				else
				{
					if (haplosome->mutation_count() != 0)
						all_target_haplosomes_started_empty = false;
					
					haplosomes.emplace_back(haplosome);
					haplosomes_last_mutrun_modified.emplace_back(-1);
					haplosomes_last_mutrun.emplace_back(nullptr);
				}
			}
			
			if (intrinsic_ploidy == 2)
			{
				Haplosome *haplosome = ind->haplosomes_[last_haplosome_index];
				
				if (haplosome->IsNull())
				{
					haplosomes.emplace_back(nullptr);
					haplosomes_last_mutrun_modified.emplace_back(-1);
					haplosomes_last_mutrun.emplace_back(nullptr);
				}
				else
				{
					if (haplosome->mutation_count() != 0)
						all_target_haplosomes_started_empty = false;
					
					haplosomes.emplace_back(haplosome);
					haplosomes_last_mutrun_modified.emplace_back(-1);
					haplosomes_last_mutrun.emplace_back(nullptr);
				}
			}
		}
		
		// parse all the call lines, instantiate their mutations, and add the mutations to the target haplosomes
#ifndef _OPENMP
		MutationRunContext &mutrun_context = chromosome->ChromosomeMutationRunContextForThread(omp_get_thread_num());	// when not parallel, we have only one MutationRunContext
#endif
		
		for (std::pair<slim_position_t, std::string> &call_line : call_lines)
		{
			slim_position_t mut_position = call_line.first;
			std::istringstream iss(call_line.second);
			std::string ref_str, alt_str, info_str;
			
			std::getline(iss, sub, '\t');		// CHROM; don't care (already checked it above)
			std::getline(iss, sub, '\t');		// POS; already fetched
			std::getline(iss, sub, '\t');		// ID; don't care
			std::getline(iss, ref_str, '\t');	// REF
			std::getline(iss, alt_str, '\t');	// ALT
			std::getline(iss, sub, '\t');		// QUAL; don't care
			std::getline(iss, sub, '\t');		// FILTER; don't care
			std::getline(iss, info_str, '\t');	// INFO
			std::getline(iss, sub, '\t');		// FORMAT; don't care (GT must be first, according to the standard; we don't check)
			
			// parse/validate the REF nucleotide
			int8_t ref_nuc;
			
			if (ref_str == "A")			ref_nuc = 0;
			else if (ref_str == "C")	ref_nuc = 1;
			else if (ref_str == "G")	ref_nuc = 2;
			else if (ref_str == "T")	ref_nuc = 3;
			else						EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_readIndividualsFromVCF): VCF file REF value must be A/C/G/T." << EidosTerminate();
			
			// parse/validate the ALT nucleotides
			std::vector<std::string> alt_substrs = Eidos_string_split(alt_str, ",");
			std::vector<int8_t> alt_nucs;
			
			for (std::string &alt_substr : alt_substrs)
			{
				if (alt_substr == "A")			alt_nucs.emplace_back(0);
				else if (alt_substr == "C")		alt_nucs.emplace_back(1);
				else if (alt_substr == "G")		alt_nucs.emplace_back(2);
				else if (alt_substr == "T")		alt_nucs.emplace_back(3);
				else							EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_readIndividualsFromVCF): VCF file ALT value must be A/C/G/T." << EidosTerminate();
			}
			
			size_t alt_allele_count = alt_nucs.size();
			
			// parse/validate the INFO fields that we recognize
			std::vector<std::string> info_substrs = Eidos_string_split(info_str, ";");
			std::vector<slim_mutationid_t> info_mutids;
			std::vector<slim_effect_t> info_selcoeffs;
			std::vector<slim_effect_t> info_domcoeffs;
			std::vector<slim_objectid_t> info_poporigin;
			std::vector<slim_tick_t> info_tickorigin;
			std::vector<slim_objectid_t> info_muttype;
			int8_t info_ancestral_nuc = -1;
			bool info_is_nonnuc = false;
			
			for (std::string &info_substr : info_substrs)
			{
				if (info_MID_defined && (info_substr.compare(0, 4, "MID=") == 0))		// Mutation ID
				{
					std::vector<std::string> value_substrs = Eidos_string_split(info_substr.substr(4), ",");
					
					for (std::string &value_substr : value_substrs)
						info_mutids.emplace_back((slim_mutationid_t)EidosInterpreter::NonnegativeIntegerForString(value_substr, nullptr));
					
					if (info_mutids.size() && has_initial_mutations)
					{
						if (!gEidosSuppressWarnings)
						{
							if (!community.warned_readFromVCF_mutIDs_unused_)
							{
								p_interpreter.ErrorOutputStream() << "#WARNING (Individual_Class::ExecuteMethod_readIndividualsFromVCF): readIndividualsFromVCF(): the VCF file specifies mutation IDs with the MID field, but some mutation IDs have already been used so uniqueness cannot be guaranteed.  Use of mutation IDs is therefore disabled; mutations will not receive the mutation ID requested in the file.  To fix this warning, remove the MID field from the VCF file before reading.  To get readIndividualsFromVCF() to use the specified mutation IDs, load the VCF file into a model that has never simulated a mutation, and has therefore not used any mutation IDs." << std::endl;
								community.warned_readFromVCF_mutIDs_unused_ = true;
							}
						}
						
						// disable use of MID for this read
						info_MID_defined = false;
						info_mutids.resize(0);
					}
				}
				else if (info_S_defined && (info_substr.compare(0, 2, "S=") == 0))		// Selection Coefficient
				{
					std::vector<std::string> value_substrs = Eidos_string_split(info_substr.substr(2), ",");
					
					for (std::string &value_substr : value_substrs)
						info_selcoeffs.emplace_back(EidosInterpreter::FloatForString(value_substr, nullptr));
				}
				else if (info_DOM_defined && (info_substr.compare(0, 4, "DOM=") == 0))	// Dominance Coefficient
				{
					std::vector<std::string> value_substrs = Eidos_string_split(info_substr.substr(4), ",");
					
					for (std::string &value_substr : value_substrs)
						info_domcoeffs.emplace_back(EidosInterpreter::FloatForString(value_substr, nullptr));
				}
				else if (info_PO_defined && (info_substr.compare(0, 3, "PO=") == 0))	// Population of Origin
				{
					std::vector<std::string> value_substrs = Eidos_string_split(info_substr.substr(3), ",");
					
					for (std::string &value_substr : value_substrs)
						info_poporigin.emplace_back((slim_objectid_t)EidosInterpreter::NonnegativeIntegerForString(value_substr, nullptr));
				}
				else if (info_TO_defined && (info_substr.compare(0, 3, "TO=") == 0))	// Tick of Origin
				{
					std::vector<std::string> value_substrs = Eidos_string_split(info_substr.substr(3), ",");
					
					for (std::string &value_substr : value_substrs)
						info_tickorigin.emplace_back((slim_tick_t)EidosInterpreter::NonnegativeIntegerForString(value_substr, nullptr));
				}
				else if (info_GO_defined && (info_substr.compare(0, 3, "GO=") == 0))	// Generation of Origin - emitted by SLiM 3, treated as TO here
				{
					std::vector<std::string> value_substrs = Eidos_string_split(info_substr.substr(3), ",");
					
					for (std::string &value_substr : value_substrs)
						info_tickorigin.emplace_back((slim_tick_t)EidosInterpreter::NonnegativeIntegerForString(value_substr, nullptr));
				}
				else if (info_MT_defined && (info_substr.compare(0, 3, "MT=") == 0))	// Mutation Type
				{
					std::vector<std::string> value_substrs = Eidos_string_split(info_substr.substr(3), ",");
					
					for (std::string &value_substr : value_substrs)
						info_muttype.emplace_back((slim_objectid_t)EidosInterpreter::NonnegativeIntegerForString(value_substr, nullptr));
				}
				else if (/* info_AA_defined && */ (info_substr.compare(0, 3, "AA=") == 0))	// Ancestral Allele; definition not required since it is a standard field
				{
					std::string aa_str = info_substr.substr(3);
					
					if (aa_str == "A")			info_ancestral_nuc = 0;
					else if (aa_str == "C")		info_ancestral_nuc = 1;
					else if (aa_str == "G")		info_ancestral_nuc = 2;
					else if (aa_str == "T")		info_ancestral_nuc = 3;
					else						EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_readIndividualsFromVCF): VCF file AA value must be A/C/G/T." << EidosTerminate();
				}
				else if (info_NONNUC_defined && (info_substr == "NONNUC"))				// Non-nucleotide-based
				{
					info_is_nonnuc = true;
				}
				
				if ((info_mutids.size() != 0) && (info_mutids.size() != alt_allele_count))
					EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_readIndividualsFromVCF): VCF file unexpected value count for MID field." << EidosTerminate();
				if ((info_selcoeffs.size() != 0) && (info_selcoeffs.size() != alt_allele_count))
					EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_readIndividualsFromVCF): VCF file unexpected value count for S field." << EidosTerminate();
				if ((info_domcoeffs.size() != 0) && (info_domcoeffs.size() != alt_allele_count))
					EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_readIndividualsFromVCF): VCF file unexpected value count for DOM field." << EidosTerminate();
				if ((info_poporigin.size() != 0) && (info_poporigin.size() != alt_allele_count))
					EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_readIndividualsFromVCF): VCF file unexpected value count for PO field." << EidosTerminate();
				if ((info_tickorigin.size() != 0) && (info_tickorigin.size() != alt_allele_count))
					EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_readIndividualsFromVCF): VCF file unexpected value count for GO or TO field." << EidosTerminate();
				if ((info_muttype.size() != 0) && (info_muttype.size() != alt_allele_count))
					EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_readIndividualsFromVCF): VCF file unexpected value count for MT field." << EidosTerminate();
			}
			
			// instantiate the mutations involved in this call line; the REF allele represents no mutation, ALT alleles are each separate mutations
			std::vector<MutationIndex> alt_allele_mut_indices;
			
			for (std::size_t alt_allele_index = 0; alt_allele_index < alt_allele_count; ++alt_allele_index)
			{
				// figure out the mutation type; if specified with MT, look it up, otherwise use the default supplied
				MutationType *mutation_type_ptr = default_mutation_type_ptr;
				
				if (info_muttype.size() > 0)
				{
					slim_objectid_t mutation_type_id = info_muttype[alt_allele_index];
					
					mutation_type_ptr = species->MutationTypeWithID(mutation_type_id);
					
					if (!mutation_type_ptr)
						EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_readIndividualsFromVCF): VCF file MT field references a mutation type m" << mutation_type_id << " that is not defined." << EidosTerminate();
				}
				
				if (!mutation_type_ptr)
					EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_readIndividualsFromVCF): VCF file MT field missing, but no default mutation type was supplied in the mutationType parameter." << EidosTerminate();
				
				// get the dominance coefficient from DOM, or use the default coefficient from the mutation type
				slim_effect_t dominance_coeff;
				
				if (info_domcoeffs.size() > 0)
					dominance_coeff = info_domcoeffs[alt_allele_index];
				else
					dominance_coeff = mutation_type_ptr->effect_distributions_[0].default_dominance_coeff_;	// FIXME MULTITRAIT
				
				// get the selection coefficient from S, or draw one from the mutation type
				slim_effect_t selection_coeff;
				
				if (info_selcoeffs.size() > 0)
					selection_coeff = info_selcoeffs[alt_allele_index];
				else
					selection_coeff = static_cast<slim_effect_t>(mutation_type_ptr->DrawEffectForTrait(0));	// FIXME MULTITRAIT
				
				// get the subpop index from PO, or set to -1; no bounds checking on this
				slim_objectid_t subpop_index = -1;
				
				if (info_poporigin.size() > 0)
					subpop_index = info_poporigin[alt_allele_index];
				
				// get the origin tick from gO, or set to the current tick; no bounds checking on this
				slim_tick_t origin_tick;
				
				if (info_tickorigin.size() > 0)
					origin_tick = info_tickorigin[alt_allele_index];
				else
					origin_tick = community.Tick();
				
				// figure out the nucleotide and do nucleotide-related checks
				int8_t alt_allele_nuc = alt_nucs[alt_allele_index];		// must be defined, in all cases, but might be ignored
				int8_t nucleotide;
				
				if (nucleotide_based)
				{
					if (info_NONNUC_defined)
					{
						// We are reading a SLiM-generated VCF file that uses NONNUC to designate non-nucleotide-based mutations
						if (info_is_nonnuc)
						{
							// This call line is marked NONNUC, so there is no associated nucleotide; check against the mutation type
							if (mutation_type_ptr->nucleotide_based_)
								EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_readIndividualsFromVCF): a mutation marked NONNUC cannot use a nucleotide-based mutation type." << EidosTerminate();
							
							nucleotide = -1;
						}
						else
						{
							// This call line is not marked NONNUC, so it represents nucleotide-based alleles
							if (!mutation_type_ptr->nucleotide_based_)
								EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_readIndividualsFromVCF): a nucleotide-based mutation cannot use a non-nucleotide-based mutation type." << EidosTerminate();
							if (ref_nuc != info_ancestral_nuc)
								EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_readIndividualsFromVCF): the REF nucleotide does not match the AA nucleotide." << EidosTerminate();
							
							int8_t ancestral = (int8_t)chromosome->AncestralSequence()->NucleotideAtIndex(mut_position);
							
							if (ancestral != ref_nuc)
								EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_readIndividualsFromVCF): the REF/AA nucleotide does not match the ancestral nucleotide at the same position; a matching ancestral nucleotide sequence must be set prior to calling readIndividualsFromVCF()." << EidosTerminate();
							
							nucleotide = alt_allele_nuc;
						}
					}
					else
					{
						// We are reading a generic VCF file that does not use NONNUC, so we follow the mutation type's lead; if it is nucleotide-based, we use the nucleotide specified
						if (mutation_type_ptr->nucleotide_based_)
						{
							// The mutation type is nucleotide-based, so use the nucleotide specified; in this case we ignore REF and AA, however
							nucleotide = alt_allele_nuc;
						}
						else
						{
							// The mutation type is non-nucleotide-based, so we ignore the nucleotide supplied, as well as REF/AA
							nucleotide = -1;
						}
					}
				}
				else
				{
					// We are a non-nucleotide-based model, so NONNUC should not be defined; we do not understand nucleotides and will ignore them
					if (info_NONNUC_defined)
						EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_readIndividualsFromVCF): cannot read a VCF file generated by a nucleotide-based model into a non-nucleotide-based model." << EidosTerminate();
					
					nucleotide = -1;
				}
				
				// instantiate the mutation with the values decided upon
				MutationIndex new_mut_index = SLiM_NewMutationFromBlock();
				Mutation *new_mut;
				
				if (info_mutids.size() > 0)
				{
					// a mutation ID was supplied; we use it blindly, having checked above that we are in the case where this is legal
					slim_mutationid_t mut_mutid = info_mutids[alt_allele_index];
					
					new_mut = new (gSLiM_Mutation_Block + new_mut_index) Mutation(mut_mutid, mutation_type_ptr, chromosome->Index(), mut_position, selection_coeff, dominance_coeff, subpop_index, origin_tick, nucleotide);
				}
				else
				{
					// no mutation ID supplied, so use whatever is next
					new_mut = new (gSLiM_Mutation_Block + new_mut_index) Mutation(mutation_type_ptr, chromosome->Index(), mut_position, selection_coeff, dominance_coeff, subpop_index, origin_tick, nucleotide);
				}
				
				// This mutation type might not be used by any genomic element type (i.e. might not already be vetted), so we need to check and set pure_neutral_
				if (selection_coeff != 0.0)
				{
					species->pure_neutral_ = false;
					mutation_type_ptr->all_pure_neutral_DFE_ = false;
				}
				
				// add it to our local map, so we can find it when making haplosomes, and to the population's mutation registry
				pop.MutationRegistryAdd(new_mut);
				alt_allele_mut_indices.emplace_back(new_mut_index);
				mutation_indices.emplace_back(new_mut_index);
			}
			
			// read the genotype data for each sample id, which might be diploid or haploid, and might have data beyond GT
			// NOTE: unlike readHaplosomesFromVCF(), we place the mutations directly into the haplosomes, rather than using a genotype_calls vector
			int haplosomes_index = 0;
			
			for (int sample_index = 0; sample_index < sample_id_count; ++sample_index)
			{
				if (iss.eof())
					EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_readIndividualsFromVCF): VCF file call line ended unexpectedly before the last sample." << EidosTerminate();
				
				std::getline(iss, sub, '\t');
				
				// extract just the GT field if others are present
				std::size_t colon_pos = sub.find_first_of(':');
				
				if (colon_pos != std::string::npos)
					sub = sub.substr(0, colon_pos);
				
				// separate haploid calls that are joined by | or /; this is the hotspot of the whole method, so we try to be efficient here
				bool call_handled = false;
				int genotype_call1 = -1;
				int genotype_call2 = -1;
				
				if ((sub.length() == 3) && ((sub[1] == '|') || (sub[1] == '/')))
				{
					// diploid, both single-digit
					char sub_ch1 = sub[0];
					char sub_ch2 = sub[2];
					
					if ((sub_ch1 >= '0') && (sub_ch1 <= '9') && (sub_ch2 >= '0') && (sub_ch2 <= '9'))
					{
						genotype_call1 = (int)(sub_ch1 - '0');
						genotype_call2 = (int)(sub_ch2 - '0');
						
						call_handled = true;
					}
				}
				else if (sub.length() == 1)
				{
					char sub_ch = sub[0];
					
					if (sub_ch == '~')
					{
						// If the call is ~, a tilde, it indicates that no genetic information is present
						// (this is the case for a female if we're reading Y-chromosome data, for example).
						// We leave genotype_call1 and genotype_call2 as -1, indicating no call for either.
						// Note that this is not part of the VCF standard; it had to be invented for SLiM.
						call_handled = true;
					}
					else
					{
						// haploid, single-digit; note we always place this in genotype_call1
						if ((sub_ch >= '0') && (sub_ch <= '9'))
						{
							genotype_call1 = (int)(sub_ch - '0');
							
							call_handled = true;
						}
					}
				}
				
				if (!call_handled)
				{
					std::vector<std::string> genotype_substrs;
					
					if (sub.find('|') != std::string::npos)
						genotype_substrs = Eidos_string_split(sub, "|");	// phased
					else if (sub.find('/') != std::string::npos)
						genotype_substrs = Eidos_string_split(sub, "/");	// unphased; we don't worry about that
					else
						genotype_substrs.emplace_back(sub);					// haploid, presumably
					
					if (genotype_substrs.size() == 2)
					{
						// diploid
						genotype_call1 = (int)EidosInterpreter::NonnegativeIntegerForString(genotype_substrs[0], nullptr);
						genotype_call2 = (int)EidosInterpreter::NonnegativeIntegerForString(genotype_substrs[1], nullptr);
					}
					else if (genotype_substrs.size() == 1)
					{
						// haploid; note we always place this in genotype_call1
						genotype_call1 = (int)EidosInterpreter::NonnegativeIntegerForString(genotype_substrs[0], nullptr);
					}
					else
						EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_readIndividualsFromVCF): VCF file genotype calls must be diploid or haploid; " << genotype_substrs.size() << " calls found in one sample." << EidosTerminate();
				}
				
				if ((genotype_call1 > (int)alt_allele_count) || (genotype_call2 > (int)alt_allele_count))	// 0 is REF, 1..n are ALT alleles
					EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_readIndividualsFromVCF): VCF file call out of range (does not correspond to a REF or ALT allele in the call line)." << EidosTerminate();
				
				if ((genotype_call2 != -1) && (intrinsic_ploidy == 1))
					EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_readIndividualsFromVCF): a diploid call was seen ('" << sub << "') but the focal chromosome for the call (with symbol '" << chromosome->Symbol() << "') is intrinsically haploid." << EidosTerminate();
				
				// add the mutations to the appropriate haplosomes and record the new derived states
				// this uses the cached haplosome state we set up above, for efficient addition:
				//
				//std::vector<Haplosome *> haplosomes;
				//std::vector<slim_mutrun_index_t> haplosomes_last_mutrun_modified;
				//std::vector<MutationRun *> haplosome_last_mutrun;
				//
				// remember that haplosomes has nullptr for any null haplosomes, to catch bugs!
				
				if (genotype_call1 == -1)
				{
					if (genotype_call2 == -1)
					{
						// Neither call is present; this occurs with the ~ call, which in not VCF standard
						// We check that both haplosomes are null; a non-null haplosome should be called
						// as not having any mutation with 0, not ~.
						//
						// We do, however, allow a ~ call for chromosome type 'A' or 'H', and transmogrify
						// any existing non-null haplosome to null, as long as it is empty.  We do not want
						// to allow that for other chromosome types, since it would require changing the sex.
						if (intrinsic_ploidy == 2)
						{
							Haplosome *haplosome1 = haplosomes[haplosomes_index];
							Haplosome *haplosome2 = haplosomes[haplosomes_index + 1];
							
							if (haplosome1 || haplosome2)
							{
								if (chromosome_type == ChromosomeType::kA_DiploidAutosome)
								{
									if (haplosome1)
									{
										if (haplosome1->mutation_count())
											EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_readIndividualsFromVCF): a call of '~' was used for a haplosome that already contains mutations, and thus cannot be made into a null haplosome; use a call of 0, not ~, if the haplosome is not intended to be a null haplosome." << EidosTerminate();
										haplosome1->MakeNull();
										haplosome1->OwningIndividual()->subpopulation_->has_null_haplosomes_ = true;
									}
									if (haplosome2)
									{
										if (haplosome2->mutation_count())
											EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_readIndividualsFromVCF): a call of '~' was used for a haplosome that already contains mutations, and thus cannot be made into a null haplosome; use a call of 0, not ~, if the haplosome is not intended to be a null haplosome." << EidosTerminate();
										haplosome2->MakeNull();
										haplosome2->OwningIndividual()->subpopulation_->has_null_haplosomes_ = true;
									}
								}
								else
									EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_readIndividualsFromVCF): a call of '~' was used for an individual that has a non-null haplosome; that is not legal." << EidosTerminate();
							}
							
							// we don't need to do anything; no mutations to add
							haplosomes_index += 2;
						}
						else
						{
							Haplosome *haplosome1 = haplosomes[haplosomes_index];
							
							if (haplosome1)
							{
								if (chromosome_type == ChromosomeType::kH_HaploidAutosome)
								{
									if (haplosome1->mutation_count())
										EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_readIndividualsFromVCF): a call of '~' was used for a haplosome that already contains mutations, and thus cannot be made into a null haplosome; use a call of 0, not ~, if the haplosome is not intended to be a null haplosome." << EidosTerminate();
									haplosome1->MakeNull();
									haplosome1->OwningIndividual()->subpopulation_->has_null_haplosomes_ = true;
								}
								else
									EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_readIndividualsFromVCF): a call of '~' was used for an individual that has a non-null haplosome; that is not legal." << EidosTerminate();
							}
							
							// we don't need to do anything; no mutations to add
							haplosomes_index++;
						}
					}
					else
						EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_readIndividualsFromVCF): (internal error) call for position 2 with no call for position 1; that should not occur in the present design." << EidosTerminate();
				}
				else
				{
					if (genotype_call2 == -1)
					{
						// Haploid call; this could be for an intrinsically haploid chromosome, or
						// could be indicating that one haplosome of an intrinsically diploid
						// chromosome is null.  For now, we require the null haplosome state to
						// already be set up.
						if (intrinsic_ploidy == 2)
						{
							if (haplosomes[haplosomes_index])
							{
								// Here, for chromosome type "A" we transmogrify the second haplosome into a
								// null haplosome as long as it is empty, rather than throwing an error.
								// The choice to do this to the *second* haplosome is kind of arbitrary; all
								// we know is that we've got a haploid call for a diploid chromosome.  Since
								// there is no indication in the call syntax, we assume the second haplosome
								// is the one intended to be null.  We could extend the syntax of VCF again
								// here, like 1|~ versus ~|1 indicating the position of the null, but that
								// feels like overkill at this time.  BCH 3/6/2025.
								if (haplosomes[haplosomes_index + 1])
								{
									if (chromosome_type == ChromosomeType::kA_DiploidAutosome)
									{
										Haplosome *implied_null_haplosome = haplosomes[haplosomes_index + 1];
										
										if (implied_null_haplosome->mutation_count())
											EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_readIndividualsFromVCF): a haploid call implies that an individual's second haplosome for a diploid chromosome is null, but that haplosome already contains mutations, and thus cannot be made into a null haplosome; use a diploid call, if neither haplosome is intended to be a null haplosome." << EidosTerminate();
										implied_null_haplosome->MakeNull();
										implied_null_haplosome->OwningIndividual()->subpopulation_->has_null_haplosomes_ = true;
									}
									else
										EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_readIndividualsFromVCF): a haploid call is present for an individual that has two non-null haplosomes for the focal chromosome (which is not of type 'A'); that is not legal." << EidosTerminate();
								}
								
								// add the called mutation to the haplosome at haplosomes_index
								_AddCallToHaplosome(genotype_call1, haplosomes[haplosomes_index], haplosomes_last_mutrun_modified[haplosomes_index], haplosomes_last_mutrun[haplosomes_index],
													alt_allele_mut_indices, mut_position, species, &mutrun_context,
													all_target_haplosomes_started_empty, recording_mutations);
							}
							else if (haplosomes[haplosomes_index + 1])
							{
								// add the called mutation to the haplosome at haplosomes_index + 1
								_AddCallToHaplosome(genotype_call1, haplosomes[haplosomes_index + 1], haplosomes_last_mutrun_modified[haplosomes_index + 1], haplosomes_last_mutrun[haplosomes_index + 1],
													alt_allele_mut_indices, mut_position, species, &mutrun_context,
													all_target_haplosomes_started_empty, recording_mutations);
							}
							else
								EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_readIndividualsFromVCF): a haploid call is present for an individual that has no non-null haplosome for the focal chromosome." << EidosTerminate();
							
							haplosomes_index += 2;
						}
						else	// intrinsic_ploidy == 1
						{
							if (haplosomes[haplosomes_index])
							{
								// add the called mutation to the haplosome at haplosomes_index
								_AddCallToHaplosome(genotype_call1, haplosomes[haplosomes_index], haplosomes_last_mutrun_modified[haplosomes_index], haplosomes_last_mutrun[haplosomes_index],
													alt_allele_mut_indices, mut_position, species, &mutrun_context,
													all_target_haplosomes_started_empty, recording_mutations);
							}
							else
								EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_readIndividualsFromVCF): a haploid call is present for an individual that has no non-null haplosome for the focal chromosome." << EidosTerminate();
							
							haplosomes_index++;
						}
					}
					else
					{
						// Diploid call; must be for an intrinsically diploid chromosome, with no
						// null haplosomes present in the individual.
						if (intrinsic_ploidy == 1)
							EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_readIndividualsFromVCF): a diploid call is present for an intrinsically haploid focal chromosome." << EidosTerminate();
						
						// add the first called mutation to the haplosome at haplosomes_index
						if (haplosomes[haplosomes_index])
						{
							_AddCallToHaplosome(genotype_call1, haplosomes[haplosomes_index], haplosomes_last_mutrun_modified[haplosomes_index], haplosomes_last_mutrun[haplosomes_index],
												alt_allele_mut_indices, mut_position, species, &mutrun_context,
												all_target_haplosomes_started_empty, recording_mutations);
						}
						else
							EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_readIndividualsFromVCF): a diploid call is present for an individual that has a null haplosome for the focal chromosome." << EidosTerminate();
						
						haplosomes_index++;
						
						// add the second called mutation to the haplosome at haplosomes_index
						if (haplosomes[haplosomes_index])
						{
							_AddCallToHaplosome(genotype_call2, haplosomes[haplosomes_index], haplosomes_last_mutrun_modified[haplosomes_index], haplosomes_last_mutrun[haplosomes_index],
												alt_allele_mut_indices, mut_position, species, &mutrun_context,
												all_target_haplosomes_started_empty, recording_mutations);
						}
						else
							EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_readIndividualsFromVCF): a diploid call is present for an individual that has a null haplosome for the focal chromosome." << EidosTerminate();
						
						haplosomes_index++;
					}
				}
			}
			
			if (!iss.eof())
				EIDOS_TERMINATION << "ERROR (Individual_Class::ExecuteMethod_readIndividualsFromVCF): VCF file call line has unexpected entries following the last sample." << EidosTerminate();
		}
	}
	
	// Return the instantiated mutations
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	int mutation_count = (int)mutation_indices.size();
	EidosValue_Object *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Mutation_Class))->resize_no_initialize_RR(mutation_count);
	
	for (int mut_index = 0; mut_index < mutation_count; ++mut_index)
		vec->set_object_element_no_check_no_previous_RR(mut_block_ptr + mutation_indices[mut_index], mut_index);
	
	return EidosValue_Object_SP(vec);
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
	if (target_size >= 1)
	{
		Individual * const *targets = (Individual * const *)(p_target->ObjectData());
		
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
		if (target_size >= 1)
		{
			// Vector target case, one point
			Individual * const *targets = (Individual * const *)(p_target->ObjectData());
			
			switch (dimensionality)
			{
				case 1:
				{
					double x = position_value->FloatAtIndex_NOCAST(0, nullptr);
					
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
					double x = position_value->FloatAtIndex_NOCAST(0, nullptr);
					double y = position_value->FloatAtIndex_NOCAST(1, nullptr);
					
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
					double x = position_value->FloatAtIndex_NOCAST(0, nullptr);
					double y = position_value->FloatAtIndex_NOCAST(1, nullptr);
					double z = position_value->FloatAtIndex_NOCAST(2, nullptr);
					
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
		Individual * const *targets = (Individual * const *)(p_target->ObjectData());
		const double *positions = position_value->FloatData();
		
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

// In these methods we implement a special behavior: you can do individual.traitName to
// access the value for a trait.  We do a dynamic lookup from the trait name here.

EidosValue_SP Individual_Class::GetProperty_NO_SIGNATURE(EidosGlobalStringID p_property_id, EidosObject **p_targets, size_t p_targets_size) const
{
	const Individual *const *individuals = (const Individual *const *)p_targets;
	
	Species *species = Community::SpeciesForIndividualsVector(individuals, (int)p_targets_size);
	Trait *trait = species->TraitFromStringID(p_property_id);
	
	if (trait)
	{
		// We got a hit, but don't know what to do with it for now
		EIDOS_TERMINATION << "ERROR (Individual_Class::GetProperty_NO_SIGNATURE): trait " << trait->Name() << " cannot be accessed (FIXME MULTITRAIT)." << EidosTerminate();
	}
	
	return super::GetProperty_NO_SIGNATURE(p_property_id, p_targets, p_targets_size);
}

void Individual_Class::SetProperty_NO_SIGNATURE(EidosGlobalStringID p_property_id, EidosObject **p_targets, size_t p_targets_size, const EidosValue &p_value) const
{
	const Individual *const *individuals = (const Individual *const *)p_targets;
	
	Species *species = Community::SpeciesForIndividualsVector(individuals, (int)p_targets_size);
	Trait *trait = species->TraitFromStringID(p_property_id);
	
	if (trait)
	{
		// Eidos did not type-check for us, because there is no signature!  We have to check it ourselves.
		if (p_value.Type() != EidosValueType::kValueFloat)
			EIDOS_TERMINATION << "ERROR (Individual_Class::SetProperty_NO_SIGNATURE): assigned value must be of type float for trait-value property " << trait->Name() << "." << EidosTerminate();
		
		// We got a hit, but don't know what to do with it for now
		EIDOS_TERMINATION << "ERROR (Individual_Class::GetProperty_NO_SIGNATURE): trait " << trait->Name() << " cannot be accessed (FIXME MULTITRAIT)." << EidosTerminate();
	}
	
	return super::SetProperty_NO_SIGNATURE(p_property_id, p_targets, p_targets_size, p_value);
}












































