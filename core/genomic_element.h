//
//  genomic_element.h
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

/*
 
 The class GenomicElement represents a portion of a chromosome with particular properties.  A genomic element is defined by its type,
 which might represent introns versus extrons for example, and the start and end positions of the element on the chromosome.
 
 */

#ifndef __SLiM__genomic_element__
#define __SLiM__genomic_element__


#include <iostream>

#include "genomic_element_type.h"


class GenomicElement
{
	// This class has a restricted copying policy; see below
	
private:
	
	static bool s_log_copy_and_assign_;
	
public:
	
	//
	//	This class should not be copied, in general, but the default copy constructor and assignment operator cannot be entirely
	//	disabled, because we want to keep instances of this class inside STL containers.  We therefore override the default copy
	//	constructor and the default assignment operator to log whenever they are called.  This is intended to reduce the risk of
	//	unintentional copying.  Logging can be disabled by calling LogGenomeCopyAndAssign() when appropriate.
	//
	GenomicElement(const GenomicElement &p_original);
	GenomicElement& operator= (const GenomicElement &p_original);
	static bool LogGenomicElementCopyAndAssign(bool p_log);			// returns the old value; save and restore that value!
	
	
	const GenomicElementType *genomic_element_type_ptr_;
	int start_position_;
	int end_position_;
	
	GenomicElement(const GenomicElementType *p_genomic_element_type_ptr, int p_start_position, int p_end_position);
};

// support stream output of GenomicElement, for debugging
std::ostream &operator<<(std::ostream &p_outstream, const GenomicElement &p_genomic_element);


#endif /* defined(__SLiM__genomic_element__) */




































































