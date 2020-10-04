//
//  mutation_type.h
//  SLiM
//
//  Created by Ben Haller on 12/13/14.
//  Copyright (c) 2014-2020 Philipp Messer.  All rights reserved.
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

// This include is up here because I had to jump through some hoops to break a #include loop between this and mutation_run.h;
// forward declaration wasn't enough, because MutationType includes a MutationRun inside itself now, and mutation_run.h defines
// an inline function that uses MutationType in a concrete fashion, so the mutual dependency was harder to break.
#include "mutation_run.h"

#ifndef __SLiM__mutation_type__
#define __SLiM__mutation_type__


#include <vector>
#include <string>
#include "eidos_value.h"
#include "eidos_symbol_table.h"
#include "slim_globals.h"

class SLiMSim;


extern EidosObjectClass *gSLiM_MutationType_Class;


// This enumeration represents a type of distribution of fitness effects (DFE) that a mutation type can draw from
enum class DFEType : char {
	kFixed = 0,
	kGamma,
	kExponential,
	kNormal,
	kWeibull,
	kScript
};

std::ostream& operator<<(std::ostream& p_out, DFEType p_dfe_type);

	
class MutationType : public EidosDictionary
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
	
	SLiMSim &sim_;								// We have a reference back to our simulation, for running type "s" DFE scripts
	
	slim_objectid_t mutation_type_id_;			// the id by which this mutation type is indexed in the chromosome
	EidosValue_SP cached_value_muttype_id_;		// a cached value for mutation_type_id_; reset() if that changes
	
	slim_selcoeff_t dominance_coeff_;			// dominance coefficient (h)
	bool dominance_coeff_changed_;				// if true, dominance_coeff_ has been changed and fitness caches need to be cleaned up
	
	DFEType dfe_type_;							// distribution of fitness effects (DFE) type (f: fixed, g: gamma, e: exponential, n: normal, w: Weibull)
	std::vector<double> dfe_parameters_;		// DFE parameters, of type double (originally float or integer type)
	std::vector<std::string> dfe_strings_;		// DFE parameters, of type std::string (originally string type)
	
	bool nucleotide_based_;						// if true, the mutation type is nucleotide-based (i.e. mutations keep associated nucleotides)
	
	bool convert_to_substitution_;				// if true (the default in WF models), mutations of this type are converted to substitutions
	MutationStackPolicy stack_policy_;			// the mutation stacking policy; see above (kStack is the default)
	int64_t stack_group_;						// the mutation stacking group this mutation type is in (== mutation_type_id_ is default)
	
	std::string color_;											// color to use when displayed (in SLiMgui), when segregating
	float color_red_, color_green_, color_blue_;				// cached color components from color_; should always be in sync
	std::string color_sub_;										// color to use when displayed (in SLiMgui), when fixed
	float color_sub_red_, color_sub_green_, color_sub_blue_;	// cached color components from color_sub_; should always be in sync
	
	slim_usertag_t tag_value_ = SLIM_TAG_UNSET_VALUE;			// a user-defined tag value

	mutable EidosScript *cached_dfe_script_;	// used by DFE type 's' to hold a cached script for the DFE
	
#ifdef SLIM_KEEP_MUTTYPE_REGISTRIES
	// MutationType now has the ability to (optionally) keep a registry of all extant mutations of its type in the simulation,
	// separate from the main registry kept by Population.  This allows much faster response to SLiMSim::mutationsOfType()
	// and SLiMSim::countOfMutationsOfType(), because the muttype's registry can be consulted rather than doing a full scan of
	// main registry.  However, there is obviously overhead associated with adding/removing mutations from another registry, so
	// this feature is only turned on if there is demand: when more than one call per generation is made to SLiMSim::mutationsOfType()
	// or SLiMSim::countOfMutationsOfType() for a given muttype.  From then on, that muttype will track its own registry.  Obviously
	// this heuristic could be refined, perhaps with a way to stop tracking if it no longer seems to be used.
	mutable int muttype_registry_call_count_;
	mutable bool keeping_muttype_registry_;
	MutationRun muttype_registry_;
#endif
	
	// For optimizing the fitness calculation code, the exact situation for each mutation type is of great interest: does it have
	// a neutral DFE, and if so has any mutation of that type had its selection coefficient changed to be non-zero, are mutations
	// of this type made neutral by a constant callback like "return 1.0;", and so forth.  Different parts of the code need to
	// know slightly different things, so we have several different flags of this sort.
	
	// all_pure_neutral_DFE_ is true if the DFE is "f" 0.0.  It is cleared if any mutation of this type has its selection coefficient
	// changed, so it can be used as a reliable indicator that mutations of a given mutation type are actually neutral – except for
	// the effects of fitness callbacks, which might make them non-neutral in a given generation / subpopulation.
	mutable bool all_pure_neutral_DFE_;
	
	// is_pure_neutral_now_ is set up by Subpopulation::UpdateFitness(), and is valid only inside a given UpdateFitness() call.
	// If set, it indicates that the mutation type is currently pure neutral – either because all_pure_neutral_DFE_ is set and the
	// mutation type cannot be influenced by any callbacks in the current subpopulation / generation, or because an active callback
	// actually sets the mutation type to be a constant value of 1.0 in this subpopulation / generation.  Mutations for which this
	// flag is set can be safely elided from fitness calculations altogether; the flag will not be set if other active callbacks
	// could mess things up for the mutation type by, e.g., deactivating the neutral-making callback.  If this flag is set for all
	// muttypes, chromosome-based fitness calculations will be skipped altogether for this generation.
	mutable bool is_pure_neutral_now_;
	
	// set_neutral_by_global_active_callback_ is set by RecalculateFitness() if the muttype is made neutral by a constant callback
	// (i.e., return 1.0) that is global (i.e., applies to all subpops) and active.  This flag should be consulted only when the
	// "nonneutral regime" (i.e., sim.last_nonneutral_regime_) is 2 (constant neutral fitness callbacks only); it is not valid in
	// other scenarios, so it should be used with extreme caution.
	mutable bool set_neutral_by_global_active_callback_ = false;
	mutable bool previous_set_neutral_by_global_active_callback_;	// the previous value; scratch space for RecalculateFitness()
	
	// subject_to_fitness_callback_ is set by RecalculateFitness() if the muttype is currently influenced by a callback in any subpop.
	// Mutations with this flag set are considered to be non-neutral, since their fitness value is unpredictable; mutations without
	// this flag set, on the other hand, are not influenced by any callback (active or inactive), so their selcoeff may be consulted.
	// This flag is valid only when the "nonneutral regime" (i.e., sim.last_nonneutral_regime_) is 3 (non-constant or non-neutral
	// callbacks present); it is not valid in other scenarios, so it should be used with extreme caution.
	mutable bool subject_to_fitness_callback_ = false;
	mutable bool previous_subject_to_fitness_callback_;				// the previous value; scratch space for RecalculateFitness()
	
#ifdef SLIMGUI
	int mutation_type_index_;					// a zero-based index for this mutation type, used by SLiMgui to bin data by mutation type
	bool mutation_type_displayed_;				// a flag used by SLiMgui to indicate whether this mutation type is being displayed in the chromosome view
#endif
	
	MutationType(const MutationType&) = delete;					// no copying
	MutationType& operator=(const MutationType&) = delete;		// no copying
	MutationType(void) = delete;								// no null construction
#ifdef SLIMGUI
	MutationType(SLiMSim &p_sim, slim_objectid_t p_mutation_type_id, double p_dominance_coeff, bool p_nuc_based, DFEType p_dfe_type, std::vector<double> p_dfe_parameters, std::vector<std::string> p_dfe_strings, int p_mutation_type_index);
#else
	MutationType(SLiMSim &p_sim, slim_objectid_t p_mutation_type_id, double p_dominance_coeff, bool p_nuc_based, DFEType p_dfe_type, std::vector<double> p_dfe_parameters, std::vector<std::string> p_dfe_strings);
#endif
	~MutationType(void);
	
	static void ParseDFEParameters(std::string &p_dfe_type_string, const EidosValue_SP *const p_arguments, int p_argument_count,
								   DFEType *p_dfe_type, std::vector<double> *p_dfe_parameters, std::vector<std::string> *p_dfe_strings);
	
	double DrawSelectionCoefficient(void) const;					// draw a selection coefficient from this mutation type's DFE
	
	//
	// Eidos support
	//
	inline EidosSymbolTableEntry &SymbolTableEntry(void) { return self_symbol_; }
	
	virtual const EidosObjectClass *Class(void) const override;
	virtual void Print(std::ostream &p_ostream) const override;
	
	virtual EidosValue_SP GetProperty(EidosGlobalStringID p_property_id) override;
	virtual void SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value) override;
	
	virtual EidosValue_SP ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) override;
	EidosValue_SP ExecuteMethod_drawSelectionCoefficient(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_setDistribution(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	
	// Accelerated property access; see class EidosObjectElement for comments on this mechanism
	static EidosValue *GetProperty_Accelerated_id(EidosObjectElement **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_tag(EidosObjectElement **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_dominanceCoeff(EidosObjectElement **p_values, size_t p_values_size);
	
	static void SetProperty_Accelerated_convertToSubstitution(EidosObjectElement **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size);
	static void SetProperty_Accelerated_tag(EidosObjectElement **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size);
};

// support stream output of MutationType, for debugging
std::ostream &operator<<(std::ostream &p_outstream, const MutationType &p_mutation_type);


#endif /* defined(__SLiM__mutation_type__) */




































































