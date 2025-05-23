//
//  genomic_element_type.h
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

/*
 
 The class GenomicElementType represents a possible type of genomic element, defined by the types of mutations the element undergoes,
 and the relative fractions of each of those mutation types.  Exons and introns might be represented by different genomic element types,
 for example, and might have different types of mutations (exons undergo adaptive mutations while introns do not, perhaps).  At present,
 these mutational dynamics are the only defining characteristics of genomic elements.
 
 */

#ifndef __SLiM__genomic_element_type__
#define __SLiM__genomic_element_type__


#include <vector>
#include <iostream>

#include "slim_globals.h"
#include "eidos_rng.h"
#include "mutation_type.h"
#include "eidos_value.h"

class Species;


extern EidosClass *gSLiM_GenomicElementType_Class;


class GenomicElementType : public EidosDictionaryUnretained
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.

private:
	typedef EidosDictionaryUnretained super;

private:
	
	gsl_ran_discrete_t *lookup_mutation_type_ = nullptr;				// OWNED POINTER: a lookup table for getting a mutation type for this genomic element
	EidosSymbolTableEntry self_symbol_;									// for fast setup of the symbol table
	
public:
	
	Species &species_;
	
	slim_objectid_t genomic_element_type_id_;							// the id by which this genomic element type is indexed in the chromosome
	EidosValue_SP cached_value_getype_id_;								// a cached value for genomic_element_type_id_; reset() if that changes
	
	std::vector<MutationType*> mutation_type_ptrs_;						// mutation types identifiers in this element
	std::vector<double> mutation_fractions_;							// relative fractions of each mutation type
	
	std::string color_;													// color to use when displayed (in SLiMgui)
	float color_red_, color_green_, color_blue_;						// cached color components from color_; should always be in sync
	
	slim_usertag_t tag_value_ = SLIM_TAG_UNSET_VALUE;					// a user-defined tag value
	
	EidosValue_Float_SP mutation_matrix_;								// in nucleotide-based models only, the 4x4 or 64x4 float mutation matrix
	double *mm_thresholds = nullptr;									// mutation matrix threshold values for determining derived nucleotides; cached in CacheNucleotideMatrices()
	
	GenomicElementType(const GenomicElementType&) = delete;				// no copying
	GenomicElementType& operator=(const GenomicElementType&) = delete;	// no copying
	GenomicElementType(void) = delete;									// no null construction
	GenomicElementType(Species &p_species, slim_objectid_t p_genomic_element_type_id, std::vector<MutationType*> p_mutation_type_ptrs, std::vector<double> p_mutation_fractions);
	~GenomicElementType(void);
	
	void InitializeDraws(void);									// reinitialize our mutation-type lookup after changing our mutation type or proportions
	MutationType *DrawMutationType(void) const;					// draw a mutation type from the distribution for this genomic element type
	
	void SetNucleotideMutationMatrix(const EidosValue_Float_SP &p_mutation_matrix);
	
	
	//
	// Eidos support
	//
	inline EidosSymbolTableEntry &SymbolTableEntry(void) { return self_symbol_; }
	
	virtual const EidosClass *Class(void) const override;
	virtual void Print(std::ostream &p_ostream) const override;
	
	virtual EidosValue_SP GetProperty(EidosGlobalStringID p_property_id) override;
	virtual void SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value) override;
	
	virtual EidosValue_SP ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) override;
	EidosValue_SP ExecuteMethod_setMutationFractions(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_setMutationMatrix(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	
	// Accelerated property access; see class EidosObject for comments on this mechanism
	static EidosValue *GetProperty_Accelerated_id(EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_tag(EidosObject **p_values, size_t p_values_size);
};

// support stream output of GenomicElementType, for debugging
std::ostream &operator<<(std::ostream &p_outstream, const GenomicElementType &p_genomic_element_type);

class GenomicElementType_Class : public EidosDictionaryUnretained_Class
{
private:
	typedef EidosDictionaryUnretained_Class super;

public:
	GenomicElementType_Class(const GenomicElementType_Class &p_original) = delete;	// no copy-construct
	GenomicElementType_Class& operator=(const GenomicElementType_Class&) = delete;	// no copying
	inline GenomicElementType_Class(const std::string &p_class_name, EidosClass *p_superclass) : super(p_class_name, p_superclass) { }
	
	virtual const std::vector<EidosPropertySignature_CSP> *Properties(void) const override;
	virtual const std::vector<EidosMethodSignature_CSP> *Methods(void) const override;
};


#endif /* defined(__SLiM__genomic_element_type__) */




































































