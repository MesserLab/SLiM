//
//  slim_sim.h
//  SLiM
//
//  Created by Ben Haller on 12/26/14.
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
#include "slim_eidos_dictionary.h"
#include "interaction_type.h"


class EidosInterpreter;


extern EidosObjectClass *gSLiM_SLiMSim_Class;

enum class SLiMGenerationStage
{
	kStage0PreGeneration = 0,
	kStage1ExecuteEarlyScripts,
	kStage2GenerateOffspring,
	kStage3RemoveFixedMutations,
	kStage4SwapGenerations,
	kStage5ExecuteLateScripts,
	kStage6CalculateFitness,
	kStage7AdvanceGenerationCounter
};


class SLiMSim : public SLiMEidosDictionary
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.
	
#ifdef SLIMGUI
public:
	
	bool simulationValid = true;													// set to false if a terminating condition is encountered while running in SLiMgui
#else
private:
#endif
	
	EidosSymbolTable *simulation_constants_ = nullptr;								// A symbol table of constants defined by SLiM (p1, g1, m1, s1, etc.)
	
	slim_generation_t time_start_ = 0;												// the first generation number for which the simulation will run
	slim_generation_t generation_ = 0;												// the current generation reached in simulation
	SLiMGenerationStage generation_stage_ = SLiMGenerationStage::kStage0PreGeneration;		// the within-generation stage currently being executed
	bool sim_declared_finished_ = false;											// a flag set by simulationFinished() to halt the sim at the end of the current generation
	EidosValue_SP cached_value_generation_;											// a cached value for generation_; reset() if changed
	
	Chromosome chromosome_;															// the chromosome, which defines genomic elements
	Population population_;															// the population, which contains sub-populations
	
	// std::map is used instead of std::unordered_map mostly for convenience, for sorted order in the UI; these are unlikely to be bottlenecks I think
	std::map<slim_objectid_t,MutationType*> mutation_types_;						// OWNED POINTERS: this map is the owner of all allocated MutationType objects
	std::map<slim_objectid_t,GenomicElementType*> genomic_element_types_;			// OWNED POINTERS: this map is the owner of all allocated GenomicElementType objects
	std::map<slim_objectid_t,InteractionType*> interaction_types_;					// OWNED POINTERS: this map is the owner of all allocated InteractionType objects
	
	// SEX ONLY: sex-related instance variables
	bool sex_enabled_ = false;														// true if sex is tracked for individuals; if false, all individuals are hermaphroditic
	GenomeType modeled_chromosome_type_ = GenomeType::kAutosome;					// the chromosome type; other types might still be instantiated (Y, if X is modeled, e.g.)
	double x_chromosome_dominance_coeff_ = 1.0;										// the dominance coefficient for heterozygosity at the X locus (i.e. males); this is global
	
	SLiMEidosScript *script_;														// OWNED POINTER: the whole input file script
	std::vector<SLiMEidosBlock*> script_blocks_;									// OWNED POINTERS: script blocks, both from the input file script and programmatic
	std::vector<SLiMEidosBlock*> scheduled_deregistrations_;						// NOT OWNED: blocks in script_blocks_ that are scheduled for deregistration
	std::vector<SLiMEidosBlock*> scheduled_interaction_deregs_;						// NOT OWNED: interaction() callbacks in script_blocks_ that are scheduled for deregistration
	
	std::vector<const EidosFunctionSignature*> sim_0_signatures_;					// OWNED POINTERS: Eidos function signatures
	EidosFunctionMap *sim_0_function_map_ = nullptr;								// OWNED POINTER: the function map with sim_0_signatures_ added, used only in gen 0
	
	// private initialization methods
	int FormatOfPopulationFile(const char *p_file);			// -1 is file does not exist, 0 is format unrecognized, 1 is text, 2 is binary
	slim_generation_t InitializePopulationFromFile(const char *p_file, EidosInterpreter *p_interpreter);		// initialize the population from the file
	slim_generation_t _InitializePopulationFromTextFile(const char *p_file, EidosInterpreter *p_interpreter);	// initialize the population from a text file
	slim_generation_t _InitializePopulationFromBinaryFile(const char *p_file, EidosInterpreter *p_interpreter);	// initialize the population from a binary file
	void InitializeFromFile(std::istream &p_infile);								// parse a input file and set up the simulation state from its contents
	
	// initialization completeness check counts; used only when running initialize() callbacks
	int num_interaction_types_;
	int num_mutation_types_;
	int num_mutation_rates_;
	int num_genomic_element_types_;
	int num_genomic_elements_;
	int num_recombination_rates_;
	int num_gene_conversions_;
	int num_sex_declarations_;	// SEX ONLY; used to check for sex vs. non-sex errors in the file, so the #SEX tag must come before any reliance on SEX ONLY features
	int num_options_declarations_;
	
	slim_position_t last_genomic_element_position = -1;	// used to check new genomic elements for consistency
	
	// change flags; used only by SLiMgui, to know that something has changed and a UI update is needed; start as true to provoke an initial display
	bool interaction_types_changed_ = true;
	bool mutation_types_changed_ = true;
	bool genomic_element_types_changed_ = true;
	bool chromosome_changed_ = true;
	bool scripts_changed_ = true;
	
	// pedigree tracking: off by default, optionally turned on at init time to enable calls to TrackPedigreeWithParents()
	bool pedigrees_enabled_ = false;
	
	// continuous space support
	int spatial_dimensionality_ = 0;
	
	// preferred mutation run length
	int preferred_mutrun_count_ = 0;												// 0 represents no preference
	
	EidosSymbolTableEntry self_symbol_;												// for fast setup of the symbol table
	
	slim_usertag_t tag_value_;														// a user-defined tag value
	
public:
	
	// optimization of the pure neutral case; this is set to false if (a) a non-neutral mutation is added by the user, (b) a genomic element type is configured to use a
	// non-neutral mutation type, (c) an already existing mutation type (assumed to be in use) is set to a non-neutral DFE, or (d) a mutation's selection coefficient is
	// changed to non-neutral.  The flag is never set back to true.  Importantly, simply defining a non-neutral mutation type does NOT clear this flag; we want sims to be
	// able to run a neutral burn-in at full speed, only slowing down when the non-neutral mutation type is actually used.
	bool pure_neutral_ = true;														// optimization flag
	
	// warning flags; used to issue warnings only once per run of the simulation
	bool warned_early_mutation_add_ = false;
	bool warned_early_mutation_remove_ = false;
	bool warned_early_output_ = false;
	bool warned_early_read_ = false;
	
	SLiMSim(const SLiMSim&) = delete;												// no copying
	SLiMSim& operator=(const SLiMSim&) = delete;									// no copying
	explicit SLiMSim(std::istream &p_infile);										// construct a SLiMSim from an input stream
	explicit SLiMSim(const char *p_input_file);										// construct a SLiMSim from an input file
	~SLiMSim(void);																	// destructor
	
	void InitializeRNGFromSeed(unsigned long int *p_override_seed_ptr);				// should be called right after construction, generally
	
	// Managing script blocks; these two methods should be used as a matched pair, bracketing each generation stage that calls out to script
	std::vector<SLiMEidosBlock*> ScriptBlocksMatching(slim_generation_t p_generation, SLiMEidosBlockType p_event_type, slim_objectid_t p_mutation_type_id, slim_objectid_t p_interaction_type_id, slim_objectid_t p_subpopulation_id);
	void DeregisterScheduledScriptBlocks(void);
	void DeregisterScheduledInteractionBlocks(void);
	
	void RunInitializeCallbacks(void);												// run initialize() callbacks and check for complete initialization
	bool RunOneGeneration(void);													// run one generation and advance the generation count; returns false if finished
	bool _RunOneGeneration(void);													// does the work of RunOneGeneration(), with no try/catch
	slim_generation_t EstimatedLastGeneration(void);								// derived from the last generation in which an Eidos block is registered
	
	// accessors
	inline EidosSymbolTable &SymbolTable(void) const								{ return *simulation_constants_; }
	inline slim_generation_t Generation(void) const									{ return generation_; }
	inline SLiMGenerationStage GenerationStage(void) const							{ return generation_stage_; }
	inline Chromosome &TheChromosome(void)											{ return chromosome_; }
	inline Population &ThePopulation(void)											{ return population_; }
	inline const std::map<slim_objectid_t,MutationType*> &MutationTypes(void) const	{ return mutation_types_; }
	inline const std::map<slim_objectid_t,GenomicElementType*> &GenomicElementTypes(void) { return genomic_element_types_; }
	inline const std::map<slim_objectid_t,InteractionType*> &InteractionTypes(void) { return interaction_types_; }
	
	inline bool SexEnabled(void) const												{ return sex_enabled_; }
	inline bool PedigreesEnabled(void) const										{ return pedigrees_enabled_; }
	inline GenomeType ModeledChromosomeType(void) const								{ return modeled_chromosome_type_; }
	inline double XDominanceCoefficient(void) const									{ return x_chromosome_dominance_coeff_; }
	inline int SpatialDimensionality(void) const									{ return spatial_dimensionality_; }
	
	//
	// Eidos support
	//
	EidosSymbolTableEntry &SymbolTableEntry(void) { return self_symbol_; };
	
	static EidosValue_SP StaticFunctionDelegationFunnel(void *p_delegate, const std::string &p_function_name, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP FunctionDelegationFunnel(const std::string &p_function_name, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	
	EidosSymbolTable *SymbolsFromBaseSymbols(EidosSymbolTable *p_base_symbols);				// derive a symbol table, adding our own symbols if needed
	EidosFunctionMap *FunctionMapFromBaseMap(EidosFunctionMap *p_base_map, bool p_force_addition = false);	// derive a function map, adding zero-gen functions if needed
	
	static void _AddZeroGenerationFunctionsToSignatureVector(std::vector<const EidosFunctionSignature*> &p_signature_vector, SLiMSim *p_delegate);	// low-level funnel
	static const std::vector<const EidosFunctionSignature*> *ZeroGenerationFunctionSignatures_NO_DELEGATE(void);									// for code-completion only!
	
	const std::vector<const EidosFunctionSignature*> *ZeroGenerationFunctionSignatures(void);		// all zero-gen functions
	void AddZeroGenerationFunctionsToMap(EidosFunctionMap *p_map);
	void RemoveZeroGenerationFunctionsFromMap(EidosFunctionMap *p_map);
	static const std::vector<const EidosMethodSignature*> *AllMethodSignatures(void);		// does not depend on sim state, so can be a class method
	static const std::vector<const EidosPropertySignature*> *AllPropertySignatures(void);	// does not depend on sim state, so can be a class method
	
	virtual const EidosObjectClass *Class(void) const;
	
	virtual EidosValue_SP GetProperty(EidosGlobalStringID p_property_id);
	virtual void SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value);
	virtual EidosValue_SP ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
};


#endif /* defined(__SLiM__slim_sim__) */












































































