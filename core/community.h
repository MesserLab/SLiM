//
//  community.h
//  SLiM
//
//  Created by Ben Haller on 2/28/2022.
//  Copyright (c) 2022-2023 Philipp Messer.  All rights reserved.
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
 
 SLiM encapsulates an ecological community - a multispecies simulation run - as a Community object, containing Species objects
 representing species in the simulated community.
 
 */

#ifndef __SLiM__community__
#define __SLiM__community__


#include <vector>
#include <chrono>

#include "species.h"
#include "slim_globals.h"
#include "eidos_script.h"
#include "eidos_value.h"
#include "eidos_functions.h"
#include "slim_eidos_block.h"


class EidosInterpreter;
class Individual;
class LogFile;

#ifdef SLIMGUI
// Forward declaration of EidosInterpreterDebugPointsSet; see eidos_interpreter.cpp
struct EidosInterpreterDebugPointsSet_struct;
typedef EidosInterpreterDebugPointsSet_struct EidosInterpreterDebugPointsSet;
#endif

extern EidosClass *gSLiM_Community_Class;


#pragma mark -
#pragma mark Community
#pragma mark -

class Community : public EidosDictionaryUnretained
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.
	
private:
	typedef EidosDictionaryUnretained super;

private:
	// the way we handle script blocks is complicated and is private even against SLiMgui
	
	SLiMEidosScript *script_;														// OWNED POINTER: the whole input file script
	std::vector<SLiMEidosBlock*> script_blocks_;									// OWNED POINTERS: script blocks, both from the input file script and programmatic
	std::vector<SLiMEidosBlock*> scheduled_deregistrations_;						// NOT OWNED: blocks in script_blocks_ that are scheduled for deregistration
	std::vector<SLiMEidosBlock*> scheduled_interaction_deregs_;						// NOT OWNED: interaction() callbacks in script_blocks_ that are scheduled for deregistration
	
	// a cache of the last tick for the simulation, for speed
	bool last_script_block_tick_cached_ = false;
	slim_tick_t last_script_block_tick_;											// the last tick in which a bounded script block is scheduled to run
	
	// scripts blocks prearranged for fast lookup; these are all stored in script_blocks_ as well
	bool script_block_types_cached_ = false;
	std::vector<SLiMEidosBlock*> cached_first_events_;
	std::vector<SLiMEidosBlock*> cached_early_events_;
	std::vector<SLiMEidosBlock*> cached_late_events_;
	std::vector<SLiMEidosBlock*> cached_initialize_callbacks_;
	std::vector<SLiMEidosBlock*> cached_mutationEffect_callbacks_;
	std::unordered_multimap<slim_tick_t, SLiMEidosBlock*> cached_fitnessEffect_callbacks_onetick_;	// see ValidateScriptBlockCaches() for details
	std::vector<SLiMEidosBlock*> cached_fitnessEffect_callbacks_multitick_;
	std::vector<SLiMEidosBlock*> cached_interaction_callbacks_;
	std::vector<SLiMEidosBlock*> cached_matechoice_callbacks_;
	std::vector<SLiMEidosBlock*> cached_modifychild_callbacks_;
	std::vector<SLiMEidosBlock*> cached_recombination_callbacks_;
	std::vector<SLiMEidosBlock*> cached_mutation_callbacks_;
	std::vector<SLiMEidosBlock*> cached_survival_callbacks_;
	std::vector<SLiMEidosBlock*> cached_reproduction_callbacks_;
	std::vector<SLiMEidosBlock*> cached_userdef_functions_;
	
#ifdef SLIMGUI
	EidosInterpreterDebugPointsSet *debug_points_ = nullptr;						// NOT OWNED; line numbers for all lines with debugging points set
#endif
	
	// these maps compile the objects from all species into a single sorted list for presentation in the UI etc.
	std::map<slim_objectid_t,MutationType*> all_mutation_types_;					// NOT OWNED: each species owns its own mutation types
	std::map<slim_objectid_t,GenomicElementType*> all_genomic_element_types_;		// NOT OWNED: each species owns its own genomic element types
	
	std::map<slim_objectid_t,InteractionType*> interaction_types_;					// OWNED POINTERS: this map is the owner of all allocated InteractionType objects
	
	int num_interaction_types_;
	int num_modeltype_declarations_;
	
#ifdef SLIMGUI
public:
	bool simulation_valid_ = true;													// set to false if a terminating condition is encountered while running in SLiMgui
#else
private:
#endif
	
	std::vector<Species *>all_species_;												// a vector of the species being simulated, in declaration order
	Species *active_species_ = nullptr;												// the species presently executing; currently used only for initialize() callback dispatch
	
	EidosSymbolTable *simulation_globals_ = nullptr;								// A symbol table of global variables, typically empty; the parent of simulation_constants_
	EidosSymbolTable *simulation_constants_ = nullptr;								// A symbol table of constants defined by SLiM (p1, g1, m1, s1, etc.)
	EidosFunctionMap simulation_functions_;											// A map of all defined functions in the simulation
	
	// these ivars track top-level simulation state: the tick, cycle stage, etc.
	slim_tick_t tick_start_ = 0;													// the first tick number for which the simulation will run
	slim_tick_t tick_ = 0;															// the current tick reached in simulation
	EidosValue_SP cached_value_tick_;												// a cached value for tick_; invalidates automatically when used
	
	SLiMCycleStage cycle_stage_ = SLiMCycleStage::kStagePreCycle;					// the within-cycle stage currently being executed
	bool sim_declared_finished_ = false;											// a flag set by Community.simulationFinished() to halt the sim at the end of the current tick
	
	EidosSymbolTableEntry self_symbol_;												// for fast setup of the symbol table
	
	slim_usertag_t tag_value_ = SLIM_TAG_UNSET_VALUE;								// a user-defined tag value
	
	// LogFile registry, for logging data out to a file
	std::vector<LogFile *> log_file_registry_;										// OWNED POINTERS (under retain/release)
	
public:
	
	bool is_explicit_species_ = false;												// true if we have explicit species declarations (even if only one, even if named "sim")
	
	bool model_type_set_ = false;													// true if the model type has been set, either explicitly or implicitly
	SLiMModelType model_type_ = SLiMModelType::kModelTypeWF;						// the overall model type: WF or nonWF, at present; affects many other things!
	
	// warning flags; used to issue warnings only once per run of the simulation
	bool warned_early_mutation_add_ = false;
	bool warned_early_mutation_remove_ = false;
	bool warned_early_output_ = false;
	bool warned_early_read_ = false;
	bool warned_no_max_distance_ = false;
	bool warned_readFromVCF_mutIDs_unused_ = false;
	bool warned_no_ancestry_read_ = false;
	
	// these ivars are set around callbacks so we know what type of callback we're in, to prevent illegal operations during callbacks
	// to make them easier to find, such checks should always be marked with a comment: // TIMING RESTRICTION
	SLiMEidosBlockType executing_block_type_ = SLiMEidosBlockType::SLiMEidosNoBlockType;	// the innermost callback type we're executing now
	Species *executing_species_ = nullptr;													// the species executing in the tick cycle right now
	Individual *focal_modification_child_;					// set during a modifyChild() callback to indicate the child being modified
	
	// change flags; used only by SLiMgui, to know that something has changed and a UI update is needed; start as true to provoke an initial display
	bool interaction_types_changed_ = true;
	bool mutation_types_changed_ = true;
	bool genomic_element_types_changed_ = true;
	bool chromosome_changed_ = true;
	bool scripts_changed_ = true;
	
	// provenance-related stuff: remembering the seed and command-line args
	unsigned long int original_seed_;												// the initial seed value, from the user via the -s CLI option, or auto-generated
	std::vector<std::string> cli_params_;											// CLI parameters; an empty vector when run in SLiMgui, at least for now
	
#if (SLIMPROFILING == 1)
	// PROFILING : Community now keeps track of overall profiling variables
	time_t profile_start_date;														// resolution of seconds, easy to convert to a date string
	time_t profile_end_date;														// resolution of seconds, easy to convert to a date string
	std::chrono::steady_clock::time_point profile_start_clock;						// sub-second resolution, good for elapsed time
	std::chrono::steady_clock::time_point profile_end_clock;						// sub-second resolution, good for elapsed time
	std::clock_t profile_elapsed_CPU_clock;											// elapsed CPU time inside SLiM
	eidos_profile_t profile_elapsed_wall_clock;										// elapsed wall time inside SLiM
	slim_tick_t profile_start_tick;													// the SLiM tick when profiling started
	slim_tick_t profile_end_tick;													// the SLiM tick when profiling ended
	
	// PROFILING : Community keeps track of script timing counts
	eidos_profile_t profile_stage_totals_[9];										// profiling clocks; index 0 is initialize(), the rest follow sequentially; [8] is TS simplification
	eidos_profile_t profile_callback_totals_[13];									// profiling clocks; these follow SLiMEidosBlockType, except no SLiMEidosUserDefinedFunction
	
	// PROFILING : Community keeps track of its memory usage profile info (as does Species)
	SLiMMemoryUsage_Community profile_last_memory_usage_Community;
	SLiMMemoryUsage_Community profile_total_memory_usage_Community;
	SLiMMemoryUsage_Species profile_last_memory_usage_AllSpecies;
	SLiMMemoryUsage_Species profile_total_memory_usage_AllSpecies;
	int64_t total_memory_tallies_;		// this is the total number of accumulations, for both Community and all Species under it
#endif	// (SLIMPROFILING == 1)
	
	Community(const Community&) = delete;											// no copying
	Community& operator=(const Community&) = delete;								// no copying
	explicit Community(void);														// construct a Community; call InitializeFromFile() next
	~Community(void);																// destructor
	
	void InitializeFromFile(std::istream &p_infile);								// parse an input file; call after construction
	void InitializeRNGFromSeed(unsigned long int *p_override_seed_ptr);				// call after InitializeFromFile(), generally
	
	void TabulateSLiMMemoryUsage_Community(SLiMMemoryUsage_Community *p_usage, EidosSymbolTable *p_current_symbols);		// used by outputUsage() and SLiMgui profiling
	
	// Managing script blocks; these two methods should be used as a matched pair, bracketing each cycle stage that calls out to script
	void ValidateScriptBlockCaches(void);
	std::vector<SLiMEidosBlock*> ScriptBlocksMatching(slim_tick_t p_tick, SLiMEidosBlockType p_event_type, slim_objectid_t p_mutation_type_id, slim_objectid_t p_interaction_type_id, slim_objectid_t p_subpopulation_id, Species *p_species);
	std::vector<SLiMEidosBlock*> &AllScriptBlocks();
	std::vector<SLiMEidosBlock*> AllScriptBlocksForSpecies(Species *p_species);
	void OptimizeScriptBlock(SLiMEidosBlock *p_script_block);
	void AddScriptBlock(SLiMEidosBlock *p_script_block, EidosInterpreter *p_interpreter, const EidosToken *p_error_token);
	void DeregisterScheduledScriptBlocks(void);
	void DeregisterScheduledInteractionBlocks(void);
	void ExecuteFunctionDefinitionBlock(SLiMEidosBlock *p_script_block);			// execute a SLiMEidosBlock that defines a function
	void CheckScheduling(slim_tick_t p_target_tick, SLiMCycleStage p_target_stage);
	
	// Managing resources shared across the community
	bool SubpopulationIDInUse(slim_objectid_t p_subpop_id);							// not whether a SLiM subpop with this ID currently exists, but whether the ID is "in use"
	bool SubpopulationNameInUse(const std::string &p_subpop_name);					// not whether a SLiM subpop with this name currently exists, but whether the name is "in use"
	
	Subpopulation *SubpopulationWithID(slim_objectid_t p_subpop_id);
	MutationType *MutationTypeWithID(slim_objectid_t p_muttype_id);
	GenomicElementType *GenomicElementTypeWithID(slim_objectid_t p_getype_id);
	SLiMEidosBlock *ScriptBlockWithID(slim_objectid_t p_script_block_id);
	Species *SpeciesWithID(slim_objectid_t p_species_id);
	
	Species *SpeciesWithName(const std::string &species_name);
	
	inline InteractionType *InteractionTypeWithID(slim_objectid_t p_inttype_id) {
		auto id_iter = interaction_types_.find(p_inttype_id);
		return (id_iter == interaction_types_.end()) ? nullptr : id_iter->second;
	}
	
	inline __attribute__((always_inline)) const std::map<slim_objectid_t,MutationType*> &AllMutationTypes(void) const			{ return all_mutation_types_; }
	inline __attribute__((always_inline)) const std::map<slim_objectid_t,GenomicElementType*> &AllGenomicElementTypes(void)		{ return all_genomic_element_types_; }
	inline __attribute__((always_inline)) const std::map<slim_objectid_t,InteractionType*> &AllInteractionTypes(void)			{ return interaction_types_; }
	
	void InvalidateInteractionsForSpecies(Species *p_invalid_species);
	void InvalidateInteractionsForSubpopulation(Subpopulation *p_invalid_subpop);
	
	// Checking for species identity; these return nullptr if the objects do not all belong to the same species
	// Calls to these methods, and other such species checks, should be labeled SPECIES CONSISTENCY CHECK to make them easier to find
	static Species *SpeciesForIndividualsVector(Individual **individuals, int value_count);
	static Species *SpeciesForIndividuals(EidosValue *value);
	
	static Species *SpeciesForGenomesVector(Genome **genomes, int value_count);
	static Species *SpeciesForGenomes(EidosValue *value);
	
	static Species *SpeciesForMutationsVector(Mutation **mutations, int value_count);
	static Species *SpeciesForMutations(EidosValue *value);
	
	// Running ticks
	bool RunOneTick(void);															// run one tick and advance the tick count; returns false if finished
	bool _RunOneTick(void);															// does the work of RunOneTick(), with no try/catch
	void ExecuteEidosEvent(SLiMEidosBlock *p_script_block);
	void AllSpecies_RunInitializeCallbacks(void);									// run initialize() callbacks and check for complete initialization
	void RunInitializeCallbacks(void);												// run `species all` initialize() callbacks
	void AllSpecies_CheckIntegrity(void);
	void AllSpecies_PurgeRemovedObjects(void);
	
	bool _RunOneTickWF(void);														// called by _RunOneTick() to run a tick (WF models)
	bool _RunOneTickNonWF(void);													// called by _RunOneTick() to run a tick (nonWF models)
	
	slim_tick_t FirstTick(void);													// derived from the first tick in which an Eidos block is registered
	slim_tick_t EstimatedLastTick(void);											// derived from the last tick in which an Eidos block is registered
	void SimulationHasFinished(void);
	
#if (SLIMPROFILING == 1)
	// PROFILING
	void StartProfiling(void);
	void StopProfiling(void);
	
	void CollectSLiMguiMemoryUsageProfileInfo(void);
#endif
	
	// accessors
	inline __attribute__((always_inline)) const std::vector<Species *> &AllSpecies(void)									{ return all_species_; }
	inline __attribute__((always_inline)) EidosSymbolTable &SymbolTable(void) const											{ return *simulation_constants_; }
	inline __attribute__((always_inline)) EidosFunctionMap &FunctionMap(void)												{ return simulation_functions_; }
	
	inline __attribute__((always_inline)) SLiMModelType ModelType(void) const												{ return model_type_; }
	void SetModelType(SLiMModelType p_new_type);
	
	inline __attribute__((always_inline)) slim_tick_t Tick(void) const														{ return tick_; }
	void SetTick(slim_tick_t p_new_tick);
	
	inline __attribute__((always_inline)) SLiMCycleStage CycleStage(void) const												{ return cycle_stage_; }
	
	inline __attribute__((always_inline)) std::string ScriptString(void) { return script_->String(); }
	
	
	// ***********************************  Support for debugging points in SLiMgui
#ifdef SLIMGUI
	// Set the debug points to be used; note that a copy is *not* made, the caller guarantees the lifetime of the passed object
	inline void SetDebugPoints(EidosInterpreterDebugPointsSet *p_debug_points) { debug_points_ = p_debug_points; }
	virtual EidosInterpreterDebugPointsSet *DebugPoints(void) override { return debug_points_; }
	virtual std::string DebugPointInfo(void) override { return std::string(", tick ") + std::to_string(tick_); };
#endif
	
	// ***********************************  Support for file observing notifications in SLiMgui
#ifdef SLIMGUI
	virtual void FileWriteNotification(const std::string &p_file_path, std::vector<std::string> &&p_lines, bool p_append) override;
	
	std::vector<std::string> file_write_paths_;						// full file paths for the file writes we have seen, in order of occurrence
    std::vector<std::vector<std::string>> file_write_buffers_;		// remembered file write lines, keyed by the full file path; read by SLiMgui
    std::vector<uint8_t> file_write_appends_;						// a parallel map just keeping a bool flag: true == append, false == overwrite
#endif

	// TREE SEQUENCE RECORDING
#pragma mark -
#pragma mark treeseq recording ivars
#pragma mark -
	
	// Most tree-seq stuff belongs to Species, but the tick clock is synchronized across the community, and command-line flags are managed here
	
	void AllSpecies_TSXC_Enable(void);          // forces tree-seq with crosschecks on for all species; called by the undocumented -TSXC option
	void AllSpecies_TSF_Enable(void);           // forces tree-seq without crosschecks on for all species; called by the undocumented -TSF option
	
	slim_tick_t tree_seq_tick_ = 0;				// the tick for the tree sequence code, incremented after generating offspring
												// this is needed since addSubpop() in an early() event makes one gen, and then the offspring
												// arrive in the same tick according to SLiM, which confuses the tree-seq code
												// BCH 5/13/2021: We now increment this after first() events in nonWF models, too
	double tree_seq_tick_offset_ = 0;			// this is a fractional offset added to tree_seq_tick_; this is needed to make successive calls
												// to addSubpopSplit() arrive at successively later times; see Population::AddSubpopulationSplit()
	std::string treeseq_time_unit_;				// set in initializeTreeSeq(), written out to .trees; has no effect on the simulation, just user data
	
	
	//
	// Eidos support
	//
#pragma mark -
#pragma mark Eidos support
#pragma mark -
	inline EidosSymbolTableEntry &SymbolTableEntry(void) { return self_symbol_; };
	
	virtual EidosValue_SP ContextDefinedFunctionDispatch(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) override;
	
	EidosSymbolTable *SymbolsFromBaseSymbols(EidosSymbolTable *p_base_symbols);				// derive a symbol table, adding our own symbols if needed
	
	static const std::vector<EidosFunctionSignature_CSP> *ZeroTickFunctionSignatures(void);		// all zero-tick functions
	static void AddZeroTickFunctionsToMap(EidosFunctionMap &p_map);
	static void RemoveZeroTickFunctionsFromMap(EidosFunctionMap &p_map);
	
	static const std::vector<EidosFunctionSignature_CSP> *SLiMFunctionSignatures(void);		// all non-zero-tick functions
	static void AddSLiMFunctionsToMap(EidosFunctionMap &p_map);
	
	EidosValue_SP ExecuteContextFunction_initializeSLiMModelType(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteContextFunction_initializeInteractionType(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	
	virtual const EidosClass *Class(void) const override;
	virtual void Print(std::ostream &p_ostream) const override;
	
	virtual EidosValue_SP GetProperty(EidosGlobalStringID p_property_id) override;
	virtual void SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value) override;
	
	virtual EidosValue_SP ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) override;
	
	EidosValue_SP ExecuteMethod_createLogFile(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_deregisterScriptBlock(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_genomicElementTypesWithIDs(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_interactionTypesWithIDs(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_mutationTypesWithIDs(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_scriptBlocksWithIDs(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_speciesWithIDs(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_subpopulationsWithIDs(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_outputUsage(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_registerFirstEarlyLateEvent(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_registerInteractionCallback(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_rescheduleScriptBlock(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_simulationFinished(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_usage(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
};

class Community_Class : public EidosDictionaryUnretained_Class
{
private:
	typedef EidosDictionaryUnretained_Class super;

public:
	Community_Class(const Community_Class &p_original) = delete;	// no copy-construct
	Community_Class& operator=(const Community_Class&) = delete;	// no copying
	inline Community_Class(const std::string &p_class_name, EidosClass *p_superclass) : super(p_class_name, p_superclass) { }
	
	virtual const std::vector<EidosPropertySignature_CSP> *Properties(void) const override;
	virtual const std::vector<EidosMethodSignature_CSP> *Methods(void) const override;
};


#endif /* defined(__SLiM__community__) */












































































