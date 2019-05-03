//
//  genomic_element.h
//  SLiM
//
//  Created by Ben Haller on 12/13/14.
//  Copyright (c) 2014-2019 Philipp Messer.  All rights reserved.
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
 
 The class GenomicElement represents a portion of a chromosome with particular properties.  A genomic element is defined by its type,
 which might represent introns versus extrons for example, and the start and end positions of the element on the chromosome.
 
 */

#ifndef __SLiM__genomic_element__
#define __SLiM__genomic_element__


#include <iostream>

#include "genomic_element_type.h"
#include "eidos_value.h"


extern EidosObjectClass *gSLiM_GenomicElement_Class;


class GenomicElement : public EidosObjectElement
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.
	
public:
	
	EidosValue_SP self_value_;								// cached EidosValue object for speed
	
	GenomicElementType *genomic_element_type_ptr_;			// pointer to the type of genomic element this is
	slim_position_t start_position_;						// the start position of the element
	slim_position_t end_position_;							// the end position of the element
	
	slim_usertag_t tag_value_ = SLIM_TAG_UNSET_VALUE;		// a user-defined tag value
	
	GenomicElement(const GenomicElement &p_original) = delete;						// no copying
	GenomicElement& operator= (const GenomicElement &p_original) = delete;			// no copying
	GenomicElement(void) = delete;													// no null constructor
	
	GenomicElement(GenomicElementType *p_genomic_element_type_ptr, slim_position_t p_start_position, slim_position_t p_end_position);
	
	//
	// Eidos support
	//
	void GenerateCachedEidosValue(void);
	inline __attribute__((always_inline)) EidosValue_SP CachedEidosValue(void) { if (!self_value_) GenerateCachedEidosValue(); return self_value_; };
	
	virtual const EidosObjectClass *Class(void) const;
	
	virtual EidosValue_SP GetProperty(EidosGlobalStringID p_property_id);
	virtual void SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value);
	
	virtual EidosValue_SP ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_setGenomicElementType(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	
	// Accelerated property access; see class EidosObjectElement for comments on this mechanism
	static EidosValue *GetProperty_Accelerated_startPosition(EidosObjectElement **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_endPosition(EidosObjectElement **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_tag(EidosObjectElement **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_genomicElementType(EidosObjectElement **p_values, size_t p_values_size);
};

// support stream output of GenomicElement, for debugging
std::ostream &operator<<(std::ostream &p_outstream, const GenomicElement &p_genomic_element);


#endif /* defined(__SLiM__genomic_element__) */




































































