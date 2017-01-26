//
//  mutation_type.h
//  SLiM
//
//  Created by Ben Haller on 12/13/14.
//  Copyright (c) 2014-2016 Philipp Messer.  All rights reserved.
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
 
 The class MutationType represents a type of mutation defined in the input file, such as a synonymous mutation or an adaptive mutation.
 A particular mutation type is defined by its distribution of fitness effects (DFE) and its dominance coefficient.  Once a mutation type
 is defined, a draw from its DFE can be generated to determine the selection coefficient of a particular mutation of that type.
 
 */

#ifndef __SLiM__mutation_type__
#define __SLiM__mutation_type__


#include <vector>
#include <string>
#include "eidos_value.h"
#include "eidos_symbol_table.h"
#include "slim_global.h"


extern EidosObjectClass *gSLiM_MutationType_Class;


// This enumeration represents a type of distribution of fitness effects (DFE)
// that a mutation type can draw from; at present three types are supported
enum class DFEType : char {
	kFixed = 0,
	kGamma,
	kExponential,
	kNormal,
	kWeibull,
	kScript
};

std::ostream& operator<<(std::ostream& p_out, DFEType p_dfe_type);

	
// This enumeration represents the policy followed for multiple mutations at the same position.
// Such "stacked" mutations can be allowed (the default), or the first or last mutation at the position can be kept.
enum class MutationStackPolicy : char {
	kStack = 0,
	kKeepFirst,
	kKeepLast,
};


class MutationType : public EidosObjectElement
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.
	
	EidosSymbolTableEntry self_symbol_;							// for fast setup of the symbol table

public:
	
	// a mutation type is specified by the DFE and the dominance coefficient
	//
	// DFE options: f: fixed (s) 
	//              e: exponential (mean s)
	//              g: gamma distribution (mean s,shape)
	//
	// examples: synonymous, nonsynonymous, adaptive, etc.
	
	slim_objectid_t mutation_type_id_;			// the id by which this mutation type is indexed in the chromosome
	EidosValue_SP cached_value_muttype_id_;		// a cached value for mutation_type_id_; reset() if that changes
	
	slim_selcoeff_t dominance_coeff_;			// dominance coefficient (h)
	DFEType dfe_type_;							// distribution of fitness effects (DFE) type (f: fixed, g: gamma, e: exponential, n: normal, w: Weibull)
	std::vector<double> dfe_parameters_;		// DFE parameters, of type double (originally float or integer type)
	std::vector<std::string> dfe_strings_;		// DFE parameters, of type std::string (originally string type)
	
	bool convert_to_substitution_;				// if true (the default), mutations of this type are converted to substitutions
	MutationStackPolicy stack_policy_;			// the mutation stacking policy; see above (kStack be default)
	
	std::string color_;											// color to use when displayed (in SLiMgui), when segregating
	float color_red_, color_green_, color_blue_;				// cached color components from color_; should always be in sync
	std::string color_sub_;										// color to use when displayed (in SLiMgui), when fixed
	float color_sub_red_, color_sub_green_, color_sub_blue_;	// cached color components from color_sub_; should always be in sync
	
	slim_usertag_t tag_value_;					// a user-defined tag value

	mutable EidosScript *cached_dfe_script_;	// used by DFE type 's' to hold a cached script for the DFE
	
#ifdef SLIMGUI
	int mutation_type_index_;					// a zero-based index for this mutation type, used by SLiMgui to bin data by mutation type
#endif
	
	MutationType(const MutationType&) = delete;					// no copying
	MutationType& operator=(const MutationType&) = delete;		// no copying
	MutationType(void) = delete;								// no null construction
#ifdef SLIMGUI
	MutationType(slim_objectid_t p_mutation_type_id, double p_dominance_coeff, DFEType p_dfe_type, std::vector<double> p_dfe_parameters, std::vector<std::string> p_dfe_strings, int p_mutation_type_index);
#else
	MutationType(slim_objectid_t p_mutation_type_id, double p_dominance_coeff, DFEType p_dfe_type, std::vector<double> p_dfe_parameters, std::vector<std::string> p_dfe_strings);
#endif
	~MutationType(void);
	
	double DrawSelectionCoefficient(void) const;					// draw a selection coefficient from this mutation type's DFE
	
	//
	// Eidos support
	//
	EidosSymbolTableEntry &SymbolTableEntry(void) { return self_symbol_; }
	
	virtual const EidosObjectClass *Class(void) const;
	virtual void Print(std::ostream &p_ostream) const;
	
	virtual EidosValue_SP GetProperty(EidosGlobalStringID p_property_id);
	virtual void SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value);
	virtual EidosValue_SP ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	
	// Accelerated property access; see class EidosObjectElement for comments on this mechanism
	virtual int64_t GetProperty_Accelerated_Int(EidosGlobalStringID p_property_id);
	virtual double GetProperty_Accelerated_Float(EidosGlobalStringID p_property_id);
};

// support stream output of MutationType, for debugging
std::ostream &operator<<(std::ostream &p_outstream, const MutationType &p_mutation_type);


#endif /* defined(__SLiM__mutation_type__) */




































































