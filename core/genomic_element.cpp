//
//  genomic_element.cpp
//  SLiM
//
//  Created by Ben Haller on 12/13/14.
//  Copyright (c) 2014 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/software/
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


#include "genomic_element.h"
#include "stacktrace.h"


bool GenomicElement::s_log_copy_and_assign_ = true;


GenomicElement::GenomicElement(const GenomicElementType *p_genomic_element_type_ptr, int p_start_position, int p_end_position)
{
	genomic_element_type_ptr_ = p_genomic_element_type_ptr;
	start_position_ = p_start_position;
	end_position_ = p_end_position;
}

std::ostream &operator<<(std::ostream &p_outstream, const GenomicElement &p_genomic_element)
{
	p_outstream << "GenomicElement{genomic_element_type_ g" << p_genomic_element.genomic_element_type_ptr_->genomic_element_type_id_ << ", start_position_ " << p_genomic_element.start_position_ << ", end_position_ " << p_genomic_element.end_position_ << "}";
	
	return p_outstream;
}


//
//	Methods to enforce limited copying
//

GenomicElement::GenomicElement(const GenomicElement& p_original)
{
	if (s_log_copy_and_assign_)
	{
		std::clog << "********* GenomicElement::GenomicElement(GenomicElement&) called!" << std::endl;
		print_stacktrace(stderr);
		std::clog << "************************************************" << std::endl;
	}
	
	genomic_element_type_ptr_ = p_original.genomic_element_type_ptr_;
	start_position_ = p_original.start_position_;
	end_position_ = p_original.end_position_;
}

GenomicElement& GenomicElement::operator= (const GenomicElement& p_original)
{
	if (s_log_copy_and_assign_)
	{
		std::clog << "********* GenomicElement::operator=(GenomicElement&) called!" << std::endl;
		print_stacktrace(stderr);
		std::clog << "************************************************" << std::endl;
	}
	
	genomic_element_type_ptr_ = p_original.genomic_element_type_ptr_;
	start_position_ = p_original.start_position_;
	end_position_ = p_original.end_position_;
	
	return *this;
}

bool GenomicElement::LogGenomicElementCopyAndAssign(bool p_log)
{
	bool old_value = s_log_copy_and_assign_;
	
	s_log_copy_and_assign_ = p_log;
	
	return old_value;
}



































































