//
//  slim_sim.h
//  SLiM
//
//  Created by Ben Haller on 12/26/14.
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
 
 SLiM encapsulates a simulation run as a SLiMSim object.  This allows a simulation to be stepped and controlled by a GUI.
 
 */


#ifndef __SLiM__slim_sim__
#define __SLiM__slim_sim__


#include <stdio.h>
#include <map>
#include <vector>
#include <iostream>

#include "mutation.h"
#include "mutation_type.h"
#include "population.h"
#include "chromosome.h"
#include "eidos_script.h"
#include "eidos_value.h"
#include "eidos_functions.h"
#include "slim_eidos_block.h"


class EidosInterpreter;


extern EidosObjectClass *gSLiM_SLiMSim_Class;


class SLiMSim : public EidosObjectElement
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.
	
#ifdef SLIMGUI
public:
	
	bool simulationValid = true;													// set to false if a terminating condition is encountered while running in SLiMgui
#else
private:
#endif
	
	slim_generation_t time_start_ = 0;												// the first generation number for which the simulation will run
	slim_generation_t generation_ = 0;												// the current generation reached in simulation
	EidosValue *cached_value_generation_ = nullptr;									// OWNED POINTER: a cached value for generation_; delete and nil if changed
	
	Chromosome chromosome_;															// the chromosome, which defines genomic elements
	Population population_;															// the population, which contains sub-populations
	std::map<slim_objectid_t,MutationType*> mutation_types_;						// OWNED POINTERS: this map is the owner of all allocated MutationType objects
	std::map<slim_objectid_t,GenomicElementType*> genomic_element_types_;			// OWNED POINTERS: this map is the owner of all allocated GenomicElementType objects
	unsigned long int rng_seed_ = 0;												// random number generator seed; unsigned long int is the type used by gsl_rng_set()
	bool rng_seed_supplied_to_constructor_ = false;									// true if the RNG seed was supplied, which means it overrides other RNG seed sources
	
	// SEX ONLY: sex-related instance variables
	bool sex_enabled_ = false;														// true if sex is tracked for individuals; if false, all individuals are considered hermaphroditic
	GenomeType modeled_chromosome_type_ = GenomeType::kAutosome;					// the type of the chromosome being modeled; other chromosome types might still be instantiated (Y, if X is modeled, e.g.)
	double x_chromosome_dominance_coeff_ = 1.0;										// the dominance coefficient for heterozygosity at the X locus (i.e. males); this is global
	
	SLiMEidosScript *script_;														// OWNED POINTER: the whole input file script
	std::vector<SLiMEidosBlock*> script_blocks_;									// OWNED POINTERS: script blocks, both from the input file script and programmatic
	std::vector<SLiMEidosBlock*> scheduled_deregistrations_;						// NOT OWNED: blocks in script_blocks_ that are scheduled for deregistration
	std::vector<const EidosFunctionSignature*> sim_0_signatures_;					// OWNED POINTERS: Eidos function signatures
	
	// private initialization methods
	void InitializePopulationFromFile(const char *p_file);							// initialize the population from the information in the file given
	void InitializeFromFile(std::istream &infile);									// parse a input file and set up the simulation state from its contents
	
	// initialization completeness check counts; used only when running initialize() callbacks
	int num_mutation_types_;
	int num_mutation_rates_;
	int num_genomic_element_types_;
	int num_genomic_elements_;
	int num_recombination_rates_;
	int num_gene_conversions_;
	int num_sex_declarations_;	// SEX ONLY; used to check for sex vs. non-sex errors in the file, so the #SEX tag must come before any reliance on SEX ONLY features
	
	// change flags; used only by SLiMgui, to know that something has changed and a UI update is needed; start as true to provoke an initial display
	bool mutation_types_changed_ = true;
	bool genomic_element_types_changed_ = true;
	bool chromosome_changed_ = true;
	bool scripts_changed_ = true;
	
	EidosSymbolTableEntry *self_symbol_ = nullptr;									// OWNED POINTER: EidosSymbolTableEntry object for fast setup of the symbol table
	
	slim_usertag_t tag_value_;														// a user-defined tag value
	
public:
	
	SLiMSim(const SLiMSim&) = delete;														// no copying
	SLiMSim& operator=(const SLiMSim&) = delete;											// no copying
	SLiMSim(std::istream &infile, unsigned long int *p_override_seed_ptr = nullptr);		// construct a SLiMSim from an input stream, with an optional RNG seed value
	SLiMSim(const char *p_input_file, unsigned long int *p_override_seed_ptr = nullptr);	// construct a SLiMSim from an input file, with an optional RNG seed value
	~SLiMSim(void);																			// destructor
	
	// Managing script blocks; these two methods should be used as a matched pair, bracketing each generation stage that calls out to script
	std::vector<SLiMEidosBlock*> ScriptBlocksMatching(slim_generation_t p_generation, SLiMEidosBlockType p_event_type, slim_objectid_t p_mutation_type_id, slim_objectid_t p_subpopulation_id);
	void DeregisterScheduledScriptBlocks(void);
	
	void RunInitializeCallbacks(void);												// run initialize() callbacks and check for complete initialization
	bool RunOneGeneration(void);													// run a single simulation generation and advance the generation counter; returns false if the simulation is over
	bool _RunOneGeneration(void);													// does the work of RunOneGeneration(), with no try/catch
	slim_generation_t EstimatedLastGeneration();									// derived from the last generation in which an Eidos block is registered
	
	// accessors
	inline slim_generation_t Generation(void) const									{ return generation_; }
	inline Chromosome &Chromosome(void)												{ return chromosome_; }
	inline Population &Population(void)												{ return population_; }
	inline const std::map<slim_objectid_t,MutationType*> &MutationTypes(void) const	{ return mutation_types_; }
	inline const std::map<slim_objectid_t,GenomicElementType*> &GenomicElementTypes(void) { return genomic_element_types_; }
	
	inline bool SexEnabled(void) const												{ return sex_enabled_; }
	inline GenomeType ModeledChromosomeType(void) const								{ return modeled_chromosome_type_; }
	inline double XDominanceCoefficient(void) const									{ return x_chromosome_dominance_coeff_; }
	
	//
	// Eidos support
	//
	void GenerateCachedSymbolTableEntry(void);
	inline EidosSymbolTableEntry *CachedSymbolTableEntry(void) { if (!self_symbol_) GenerateCachedSymbolTableEntry(); return self_symbol_; };
	
	static EidosValue *StaticFunctionDelegationFunnel(void *delegate, const std::string &p_function_name, EidosValue *const *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue *FunctionDelegationFunnel(const std::string &p_function_name, EidosValue *const *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	
	void InjectIntoInterpreter(EidosInterpreter &p_interpreter, SLiMEidosBlock *p_script_block, bool p_fresh_symbol_table);	// add SLiM constructs to an interpreter instance
	const std::vector<const EidosFunctionSignature*> *InjectedFunctionSignatures(void);		// depends on the simulation state
	static const std::vector<const EidosMethodSignature*> *AllMethodSignatures(void);		// does not depend on sim state, so can be a class method
	
	virtual const EidosObjectClass *Class(void) const;
	
	virtual EidosValue *GetProperty(EidosGlobalStringID p_property_id);
	virtual void SetProperty(EidosGlobalStringID p_property_id, EidosValue *p_value);
	virtual EidosValue *ExecuteInstanceMethod(EidosGlobalStringID p_method_id, EidosValue *const *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
};


#endif /* defined(__SLiM__slim_sim__) */












































































