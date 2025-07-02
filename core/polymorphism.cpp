//
//  polymorphism.cpp
//  SLiM
//
//  Created by Ben Haller on 12/13/14.
//  Copyright (c) 2014-2025 Benjamin C. Haller.  All rights reserved.
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


#include "polymorphism.h"
#include "species.h"

#include <fstream>
#include <map>
#include <utility>
#include <vector>


Polymorphism::Polymorphism(slim_polymorphismid_t p_polymorphism_id, const Mutation *p_mutation_ptr, slim_refcount_t p_prevalence) :
	polymorphism_id_(p_polymorphism_id), mutation_ptr_(p_mutation_ptr), prevalence_(p_prevalence)
{
}

void Polymorphism::Print_ID_Tag(std::ostream &p_out) const
{
	// BCH 4/6/2025: This is a copy of Print_ID() with output of
	// mutation_ptr_->tag_value_ added at the end
	
	// Added mutation_ptr_->mutation_id_ to this output, BCH 11 June 2016
	// Switched to full-precision output of selcoeff and domcoeff, for accurate reloading; BCH 22 March 2019
	THREAD_SAFETY_IN_ACTIVE_PARALLEL("Polymorphism::Print_ID_Tag(): usage of statics");
	
	static char double_buf[40];
	
	p_out << polymorphism_id_ << " " << mutation_ptr_->mutation_id_ << " " << "m" << mutation_ptr_->mutation_type_ptr_->mutation_type_id_ << " " << mutation_ptr_->position_ << " ";
	
	snprintf(double_buf, 40, "%.*g", EIDOS_FLT_DIGS, mutation_ptr_->selection_coeff_);		// necessary precision for non-lossiness
	p_out << double_buf;
	
	p_out << " ";
	
	snprintf(double_buf, 40, "%.*g", EIDOS_FLT_DIGS, mutation_ptr_->dominance_coeff_);		// necessary precision for non-lossiness
	p_out << double_buf;
	
	p_out << " p" << mutation_ptr_->subpop_index_ << " " << mutation_ptr_->origin_tick_ << " " << prevalence_;
	
	// output a nucleotide if available
	if (mutation_ptr_->mutation_type_ptr_->nucleotide_based_)
		p_out << " " << gSLiM_Nucleotides[mutation_ptr_->nucleotide_];
	
	// output the tag value, or '?' or the tag is not defined
	slim_usertag_t tag = mutation_ptr_->tag_value_;
	
	if (tag == SLIM_TAG_UNSET_VALUE)
		p_out << ' ' << '?';
	else
		p_out << ' ' << tag;
	
	p_out << std::endl;
}

void Polymorphism::Print_ID(std::ostream &p_out) const
{
	// Added mutation_ptr_->mutation_id_ to this output, BCH 11 June 2016
	// Switched to full-precision output of selcoeff and domcoeff, for accurate reloading; BCH 22 March 2019
	THREAD_SAFETY_IN_ACTIVE_PARALLEL("Polymorphism::Print_ID(): usage of statics");
	
	static char double_buf[40];
	
	p_out << polymorphism_id_ << " " << mutation_ptr_->mutation_id_ << " " << "m" << mutation_ptr_->mutation_type_ptr_->mutation_type_id_ << " " << mutation_ptr_->position_ << " ";
	
	snprintf(double_buf, 40, "%.*g", EIDOS_FLT_DIGS, mutation_ptr_->selection_coeff_);		// necessary precision for non-lossiness
	p_out << double_buf;
	
	p_out << " ";
	
	snprintf(double_buf, 40, "%.*g", EIDOS_FLT_DIGS, mutation_ptr_->dominance_coeff_);		// necessary precision for non-lossiness
	p_out << double_buf;
	
	p_out << " p" << mutation_ptr_->subpop_index_ << " " << mutation_ptr_->origin_tick_ << " " << prevalence_;
	
	// output a nucleotide if available
	if (mutation_ptr_->mutation_type_ptr_->nucleotide_based_)
		p_out << " " << gSLiM_Nucleotides[mutation_ptr_->nucleotide_];
	
	p_out << std::endl;
}

void Polymorphism::Print_NoID_Tag(std::ostream &p_out) const
{
	// Added mutation_ptr_->mutation_id_ to this output, BCH 11 June 2016
	// Note that Print_ID() now outputs selcoeff and domcoeff in full precision, whereas here we do not; BCH 22 March 2019
	p_out << mutation_ptr_->mutation_id_ << " " << "m" << mutation_ptr_->mutation_type_ptr_->mutation_type_id_ << " " << mutation_ptr_->position_;
	
	// BCH 2/2/2025: Note that in multi-chrom models, this method now prints the chromosome symbol after the position
	// For brevity and backward compatibility, the chromosome symbol is not printed in single-chromosome models
	Species &species = mutation_ptr_->mutation_type_ptr_->species_;
	const std::vector<Chromosome *> &chromosomes = species.Chromosomes();
	
	if (chromosomes.size() > 1)
	{
		Chromosome *chromosome = chromosomes[mutation_ptr_->chromosome_index_];
		
		p_out << " \"" << chromosome->Symbol() << "\"";
	}
	
	// and then the remainder of the output line
	p_out << " " << mutation_ptr_->selection_coeff_ << " " << mutation_ptr_->dominance_coeff_ << " p" << mutation_ptr_->subpop_index_ << " " << mutation_ptr_->origin_tick_ << " " << prevalence_;
	
	// output a nucleotide if available
	if (mutation_ptr_->mutation_type_ptr_->nucleotide_based_)
		p_out << " " << gSLiM_Nucleotides[mutation_ptr_->nucleotide_];
	
	// output the tag value, or '?' or the tag is not defined
	slim_usertag_t tag = mutation_ptr_->tag_value_;
	
	if (tag == SLIM_TAG_UNSET_VALUE)
		p_out << ' ' << '?';
	else
		p_out << ' ' << tag;
	
	p_out << std::endl;
}

void Polymorphism::Print_NoID(std::ostream &p_out) const
{
	// Added mutation_ptr_->mutation_id_ to this output, BCH 11 June 2016
	// Note that Print_ID() now outputs selcoeff and domcoeff in full precision, whereas here we do not; BCH 22 March 2019
	p_out << mutation_ptr_->mutation_id_ << " " << "m" << mutation_ptr_->mutation_type_ptr_->mutation_type_id_ << " " << mutation_ptr_->position_;
	
	// BCH 2/2/2025: Note that in multi-chrom models, this method now prints the chromosome symbol after the position
	// For brevity and backward compatibility, the chromosome symbol is not printed in single-chromosome models
	Species &species = mutation_ptr_->mutation_type_ptr_->species_;
	const std::vector<Chromosome *> &chromosomes = species.Chromosomes();
	
	if (chromosomes.size() > 1)
	{
		Chromosome *chromosome = chromosomes[mutation_ptr_->chromosome_index_];
		
		p_out << " \"" << chromosome->Symbol() << "\"";
	}
	
	// and then the remainder of the output line
	p_out << " " << mutation_ptr_->selection_coeff_ << " " << mutation_ptr_->dominance_coeff_ << " p" << mutation_ptr_->subpop_index_ << " " << mutation_ptr_->origin_tick_ << " " << prevalence_;
	
	// output a nucleotide if available
	if (mutation_ptr_->mutation_type_ptr_->nucleotide_based_)
		p_out << " " << gSLiM_Nucleotides[mutation_ptr_->nucleotide_];
	
	p_out << std::endl;
}

// find p_mutation in p_polymorphisms and return its id
slim_polymorphismid_t FindMutationInPolymorphismMap(const PolymorphismMap &p_polymorphisms, const Mutation *p_mutation)
{
	auto poly_iter = p_polymorphisms.find(p_mutation->mutation_id_);
	
	if (poly_iter == p_polymorphisms.end())
		return -1;								// a flag indicating a failed lookup
	
	return poly_iter->second.polymorphism_id_;
}

// if mutation p_mutation is present in p_polymorphisms increase its prevalence, otherwise add it
void AddMutationToPolymorphismMap(PolymorphismMap *p_polymorphisms, const Mutation *p_mutation)
{
	auto poly_iter = p_polymorphisms->find(p_mutation->mutation_id_);
	
	if (poly_iter == p_polymorphisms->end())
	{
		// the mutation was not found, so add it to p_polymorphisms with a unique index counting up from 0
		auto polymorphisms_size = p_polymorphisms->size();
		
		if (polymorphisms_size > INT32_MAX)
			EIDOS_TERMINATION << "ERROR (AddMutationToPolymorphismMap): (internal error) polymorphism_id does not fit in int32_t." << EidosTerminate();
		
		slim_polymorphismid_t polymorphism_id = static_cast<slim_polymorphismid_t>(polymorphisms_size);
		Polymorphism new_polymorphism = Polymorphism(polymorphism_id, p_mutation, 1);
		
		p_polymorphisms->emplace(p_mutation->mutation_id_, new_polymorphism);
	}
	else
	{
		poly_iter->second.prevalence_++;
	}
}



































































