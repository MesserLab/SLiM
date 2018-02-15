//
//  slim_sim.h
//  SLiM
//
//  Created by Ben Haller on 12/26/14.
//  Copyright (c) 2014-2017 Philipp Messer.  All rights reserved.
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

//TREE SEQUENCE 
//adding this so I can write tables to txt file for debugging
//TODO remove when done.
#include <fstream>  

#include "slim_global.h"
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

//TREE SEQUENCE
//INCLUDE JEROME's TABLES API
#include "../tables-api-files/tables.h"


class EidosInterpreter;


extern EidosObjectClass *gSLiM_SLiMSim_Class;

enum class SLiMModelType
{
	kModelTypeWF = 0,	// a Wright-Fisher model: the original model type supported by SLiM
	kModelTypeNonWF		// a non-Wright-Fisher model: a new model type that is more general
};

enum class SLiMGenerationStage
{
	kStage0PreGeneration = 0,
	
	// stages for WF models
	kWFStage1ExecuteEarlyScripts = 1,
	kWFStage2GenerateOffspring,
	kWFStage3RemoveFixedMutations,
	kWFStage4SwapGenerations,
	kWFStage5ExecuteLateScripts,
	kWFStage6CalculateFitness,
	kWFStage7AdvanceGenerationCounter,
	
	// stages for nonWF models
	kNonWFStage1GenerateOffspring = 101,
	kNonWFStage2ExecuteEarlyScripts,
	kNonWFStage3CalculateFitness,
	kNonWFStage4SurvivalSelection,
	kNonWFStage5RemoveFixedMutations,
	kNonWFStage6ExecuteLateScripts,
	kNonWFStage7AdvanceGenerationCounter,
};


class SLiMSim : public SLiMEidosDictionary
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.
	
private:
	// the way we handle script blocks is complicated and is private even against SLiMgui
	
	SLiMEidosScript *script_;														// OWNED POINTER: the whole input file script
	std::vector<SLiMEidosBlock*> script_blocks_;									// OWNED POINTERS: script blocks, both from the input file script and programmatic
	std::vector<SLiMEidosBlock*> scheduled_deregistrations_;						// NOT OWNED: blocks in script_blocks_ that are scheduled for deregistration
	std::vector<SLiMEidosBlock*> scheduled_interaction_deregs_;						// NOT OWNED: interaction() callbacks in script_blocks_ that are scheduled for deregistration
	
	// a cache of the last generation for the simulation, for speed
	bool last_script_block_gen_cached_ = false;
	slim_generation_t last_script_block_gen_;										// the last generation in which a bounded script block is scheduled to run
	
	// scripts blocks prearranged for fast lookup; these are all stored in script_blocks_ as well
	bool script_block_types_cached_ = false;
	std::vector<SLiMEidosBlock*> cached_early_events_;
	std::vector<SLiMEidosBlock*> cached_late_events_;
	std::vector<SLiMEidosBlock*> cached_initialize_callbacks_;
	std::vector<SLiMEidosBlock*> cached_fitness_callbacks_;
	std::unordered_multimap<slim_generation_t, SLiMEidosBlock*> cached_fitnessglobal_callbacks_onegen_;	// see ValidateScriptBlockCaches() for details
	std::vector<SLiMEidosBlock*> cached_fitnessglobal_callbacks_multigen_;
	std::vector<SLiMEidosBlock*> cached_interaction_callbacks_;
	std::vector<SLiMEidosBlock*> cached_matechoice_callbacks_;
	std::vector<SLiMEidosBlock*> cached_modifychild_callbacks_;
	std::vector<SLiMEidosBlock*> cached_recombination_callbacks_;
	std::vector<SLiMEidosBlock*> cached_reproduction_callbacks_;
	std::vector<SLiMEidosBlock*> cached_userdef_functions_;
	
#ifdef SLIMGUI
public:
	
	bool simulation_valid_ = true;													// set to false if a terminating condition is encountered while running in SLiMgui

#if defined(SLIMGUI) && (SLIMPROFILING == 1)
	// PROFILING
	eidos_profile_t profile_stage_totals_[7];										// profiling clocks; index 0 is initialize(), the rest follow SLiMGenerationStage
	eidos_profile_t profile_callback_totals_[10];									// profiling clocks; these follow SLiMEidosBlockType, except no SLiMEidosUserDefinedFunction
#if SLIM_USE_NONNEUTRAL_CACHES
	std::vector<int32_t> profile_mutcount_history_;									// a record of the mutation run count used in each generation
	std::vector<int32_t> profile_nonneutral_regime_history_;						// a record of the nonneutral regime used in each generation
	int64_t profile_mutation_total_usage_;											// how many (non-unique) mutations were used by mutation runs, summed across generations
	int64_t profile_nonneutral_mutation_total_;										// of profile_mutation_total_usage_, how many were deemed to be nonneutral
	int64_t profile_mutrun_total_usage_;											// how many (non-unique) mutruns were used by genomes, summed across generations
	int64_t profile_unique_mutrun_total_;											// of profile_mutrun_total_usage_, how many unique mutruns existed, summed across generations
	int64_t profile_mutrun_nonneutral_recache_total_;								// of profile_unique_mutrun_total_, how many mutruns regenerated their nonneutral cache
	int64_t profile_max_mutation_index_;											// the largest mutation index seen over the course of the profile
#endif
#endif
	
#else
private:
#endif
	
	SLiMModelType model_type_ = SLiMModelType::kModelTypeWF;						// the overall model type: WF or nonWF, at present; affects many other things!
	
	EidosSymbolTable *simulation_constants_ = nullptr;								// A symbol table of constants defined by SLiM (p1, g1, m1, s1, etc.)
	EidosFunctionMap simulation_functions_;											// A map of all defined functions in the simulation
	
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
	
	bool mutation_stack_policy_changed_;											// when set, the stacking policy settings need to be checked for consistency
	
	// SEX ONLY: sex-related instance variables
	bool sex_enabled_ = false;														// true if sex is tracked for individuals; if false, all individuals are hermaphroditic
	GenomeType modeled_chromosome_type_ = GenomeType::kAutosome;					// the chromosome type; other types might still be instantiated (Y, if X is modeled, e.g.)
	double x_chromosome_dominance_coeff_ = 1.0;										// the dominance coefficient for heterozygosity at the X locus (i.e. males); this is global
	
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
	int num_modeltype_declarations_;
	
	slim_position_t last_genomic_element_position_ = -1;	// used to check new genomic elements for consistency
	
	// change flags; used only by SLiMgui, to know that something has changed and a UI update is needed; start as true to provoke an initial display
	bool interaction_types_changed_ = true;
	bool mutation_types_changed_ = true;
	bool genomic_element_types_changed_ = true;
	bool chromosome_changed_ = true;
	bool scripts_changed_ = true;
	
	// pedigree tracking: off by default, optionally turned on at init time to enable calls to TrackPedigreeWithParents()
	bool pedigrees_enabled_ = false;
	bool pedigrees_enabled_by_user_ = false;	// pedigree tracking is turned on as a side effect of tree sequence recording, but that shouldn't be user-visible
	
	// continuous space support
	int spatial_dimensionality_ = 0;
	bool periodic_x_ = false;
	bool periodic_y_ = false;
	bool periodic_z_ = false;
	
	// preferred mutation run length
	int preferred_mutrun_count_ = 0;												// 0 represents no preference
	
	// preventing incidental selfing in hermaphroditic models
	bool prevent_incidental_selfing_ = false;
	
	EidosSymbolTableEntry self_symbol_;												// for fast setup of the symbol table
	
	slim_usertag_t tag_value_;														// a user-defined tag value
	
	// Mutation run optimization.  The ivars here are used only internally by SLiMSim; the canonical reference regarding the
	// number and length of mutation runs is kept by Chromosome (for the simulation) and by Genome (for each genome object).
	// If SLiMSim decides to change the number of mutation runs, it will update those canonical repositories accordingly.
	// A prefix of x_ is used on all mutation run experiment ivars, to avoid confusion.
#define SLIM_MUTRUN_EXPERIMENT_LENGTH	50		// kind of based on how large a sample size is needed to detect important differences fairly reliably by t-test
#define SLIM_MUTRUN_MAXIMUM_COUNT		1024	// the most mutation runs we will ever use; hard to imagine that any model will want more than this
	
	bool x_experiments_enabled_;		// if false, no experiments are run and no generation runtimes are recorded
	
	int32_t x_current_mutcount_;		// the number of mutation runs we're currently using
	double *x_current_runtimes_;		// generation runtimes recorded at this mutcount (SLIM_MUTRUN_EXPERIMENT_MAXLENGTH length)
	int x_current_buflen_;				// the number of runtimes in the current_mutcount_runtimes_ buffer
	
	int32_t x_previous_mutcount_;		// the number of mutation runs we previously used
	double *x_previous_runtimes_;		// generation runtimes recorded at that mutcount (SLIM_MUTRUN_EXPERIMENT_MAXLENGTH length)
	int x_previous_buflen_;				// the number of runtimes in the previous_mutcount_runtimes_ buffer
	
	bool x_continuing_trend_;			// if true, the current experiment continues a trend, such that the opposite trend can be excluded
	
	int64_t x_stasis_limit_;			// how many stasis experiments we're running between change experiments; gets longer over time
	double x_stasis_alpha_;				// the alpha threshold at which we decide that stasis has been broken; gets smaller over time
	int64_t x_stasis_counter_;			// how many stasis experiments we have run so far
	int32_t x_prev1_stasis_mutcount_;	// the number of mutation runs we settled on when we reached stasis last time
	int32_t x_prev2_stasis_mutcount_;	// the number of mutation runs we settled on when we reached stasis the time before last
	
	std::vector<int32_t> x_mutcount_history_;	// a record of the mutation run count used in each generation
	
	// TREE SEQUENCE RECORDING
	bool recording_tree_ = false;		// true if we are doing tree sequence recording
	std::string recording_tree_path_;	// the path to write the final tree file to; given to initializeSLiMOptions(treeRecordingPath)
	Individual *CurrentTreeSequenceIndividual;
	bool FirstRecombinationCalled = false;
	
	// TABLE IVARS
	int ret;
	node_table_t nodes;
    	edge_table_t edges;
    	migration_table_t migrations;
    	site_table_t sites;
    	mutation_table_t mutations;
    	simplifier_t simplifier;

	// TABLE SIMPLIFICATION
	int simplificationInterval;		//interval at which we will simplify the tree
	int FSIDAS;				//First slim ID we see after simplification
	int FMIDAS;				//First MSPrime ID we see after simplification
	//int numberOfSamplesInLastSimplification;	
	//int lastSimplificationGeneration;
	std::map<int,node_id_t> SLiM_MSP_Id_Map;

	// add further ivars you need for tree sequence recording here; don't forget to add cleanup for them to SLiMSim::~SLiMSim() if necessary

	//ofstream to write txt file tree sequences
	//std::ofstream MspTxtNodeFile;
	//std::ofstream MspTxtEdgeFile;

	
public:
	
	// optimization of the pure neutral case; this is set to false if (a) a non-neutral mutation is added by the user, (b) a genomic element type is configured to use a
	// non-neutral mutation type, (c) an already existing mutation type (assumed to be in use) is set to a non-neutral DFE, or (d) a mutation's selection coefficient is
	// changed to non-neutral.  The flag is never set back to true.  Importantly, simply defining a non-neutral mutation type does NOT clear this flag; we want sims to be
	// able to run a neutral burn-in at full speed, only slowing down when the non-neutral mutation type is actually used.  BCH 12 January 2018: Also, note that this flag
	// is unaffected by the fitness_scaling_ properties on Subpopulation and Individual, which are taken into account even when this flag is set.
	bool pure_neutral_ = true;														// optimization flag
	
	// this counter is incremented when a selection coefficient is changed on any mutation object in the simulation.  This is used as a signal to mutation runs that their
	// cache of non-neutral mutations is invalid (because their counter is not equal to this counter).  The caches will be re-validated the next time they are used.  Other
	// code can also increment this counter in order to trigger a re-validation of all non-neutral mutation caches; it is a general-purpose mechanism.
	int32_t nonneutral_change_counter_ = 0;
	int32_t last_nonneutral_regime_ = 0;		// see mutation_run.h; 1 = no fitness callbacks, 2 = only constant-effect neutral callbacks, 3 = arbitrary callbacks
	
	// this flag is set if dominance_coeff_changed_ is set on any mutation type, as a signal that recaching needs to occur in Subpopulation::UpdateFitness()
	bool any_dominance_coeff_changed_ = false;
	
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
	void ValidateScriptBlockCaches(void);
	std::vector<SLiMEidosBlock*> ScriptBlocksMatching(slim_generation_t p_generation, SLiMEidosBlockType p_event_type, slim_objectid_t p_mutation_type_id, slim_objectid_t p_interaction_type_id, slim_objectid_t p_subpopulation_id);
	std::vector<SLiMEidosBlock*> &AllScriptBlocks();
	void OptimizeScriptBlock(SLiMEidosBlock *p_script_block);
	void AddScriptBlock(SLiMEidosBlock *p_script_block, EidosInterpreter *p_interpreter, const EidosToken *p_error_token);
	void DeregisterScheduledScriptBlocks(void);
	void DeregisterScheduledInteractionBlocks(void);
	void ExecuteFunctionDefinitionBlock(SLiMEidosBlock *p_script_block);			// execute a SLiMEidosBlock that defines a function
	
	// Running generations
	void RunInitializeCallbacks(void);												// run initialize() callbacks and check for complete initialization
	bool RunOneGeneration(void);													// run one generation and advance the generation count; returns false if finished
	bool _RunOneGeneration(void);													// does the work of RunOneGeneration(), with no try/catch
#ifdef SLIM_WF_ONLY
	bool _RunOneGenerationWF(void);													// called by _RunOneGeneration() to run a generation (WF models)
#endif
#ifdef SLIM_NONWF_ONLY
	bool _RunOneGenerationNonWF(void);												// called by _RunOneGeneration() to run a generation (nonWF models)
#endif
	
	slim_generation_t FirstGeneration(void);										// derived from the first gen in which an Eidos block is registered
	slim_generation_t EstimatedLastGeneration(void);								// derived from the last generation in which an Eidos block is registered
	void SimulationFinished(void);
	
	// Mutation run experiments
	void InitiateMutationRunExperiments(void);
	void TransitionToNewExperimentAgainstCurrentExperiment(int32_t p_new_mutrun_count);
	void TransitionToNewExperimentAgainstPreviousExperiment(int32_t p_new_mutrun_count);
	void EnterStasisForMutationRunExperiments(void);
	void MaintainMutationRunExperiments(double p_last_gen_runtime);
	
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
	// PROFILING
#if SLIM_USE_NONNEUTRAL_CACHES
	void CollectSLiMguiMutationProfileInfo(void);
#endif
#endif
	
	// Mutation stack policy checking
	inline __attribute__((always_inline)) void MutationStackPolicyChanged(void)												{ mutation_stack_policy_changed_ = true; }
	inline __attribute__((always_inline)) void CheckMutationStackPolicy(void)												{ if (mutation_stack_policy_changed_) _CheckMutationStackPolicy(); }
	void _CheckMutationStackPolicy(void);
	
	// accessors
	inline __attribute__((always_inline)) EidosSymbolTable &SymbolTable(void) const											{ return *simulation_constants_; }
	inline __attribute__((always_inline)) EidosFunctionMap &FunctionMap(void)												{ return simulation_functions_; }
	
	inline __attribute__((always_inline)) SLiMModelType ModelType(void) const												{ return model_type_; }
	inline __attribute__((always_inline)) slim_generation_t Generation(void) const											{ return generation_; }
	inline __attribute__((always_inline)) SLiMGenerationStage GenerationStage(void) const									{ return generation_stage_; }
	inline __attribute__((always_inline)) Chromosome &TheChromosome(void)													{ return chromosome_; }
	inline __attribute__((always_inline)) Population &ThePopulation(void)													{ return population_; }
	inline __attribute__((always_inline)) const std::map<slim_objectid_t,MutationType*> &MutationTypes(void) const			{ return mutation_types_; }
	inline __attribute__((always_inline)) const std::map<slim_objectid_t,GenomicElementType*> &GenomicElementTypes(void)	{ return genomic_element_types_; }
	inline __attribute__((always_inline)) const std::map<slim_objectid_t,InteractionType*> &InteractionTypes(void)			{ return interaction_types_; }
	
	inline __attribute__((always_inline)) bool SexEnabled(void) const														{ return sex_enabled_; }
	inline __attribute__((always_inline)) bool PedigreesEnabled(void) const													{ return pedigrees_enabled_; }
	inline __attribute__((always_inline)) bool PreventIncidentalSelfing(void) const											{ return prevent_incidental_selfing_; }
	inline __attribute__((always_inline)) GenomeType ModeledChromosomeType(void) const										{ return modeled_chromosome_type_; }
	inline __attribute__((always_inline)) double XDominanceCoefficient(void) const											{ return x_chromosome_dominance_coeff_; }
	inline __attribute__((always_inline)) int SpatialDimensionality(void) const												{ return spatial_dimensionality_; }
	inline __attribute__((always_inline)) void SpatialPeriodicity(bool *p_x, bool *p_y, bool *p_z) const
	{
		if (p_x) *p_x = periodic_x_;
		if (p_y) *p_y = periodic_y_;
		if (p_z) *p_z = periodic_z_;
	}
	
	// TREE SEQUENCE RECORDING
	inline __attribute__((always_inline)) bool RecordingTreeSequence(void) const											{ return recording_tree_; }
	void StartTreeRecording(void);
	void RecordNewIndividual(Individual *p_individual);
	void RecordRecombination(std::vector<slim_position_t> *p_breakpoints, bool p_start_strand_2);
	void WriteTreeSequence(void);
	void simplifyTables(void);
	node_id_t getMSPID(int GenomeID);
	// put any other methods you need for the tree sequence stuff here
	
	//
	// Eidos support
	//
	inline EidosSymbolTableEntry &SymbolTableEntry(void) { return self_symbol_; };
	
	virtual EidosValue_SP ContextDefinedFunctionDispatch(const std::string &p_function_name, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteContextFunction_initializeGenomicElement(const std::string &p_function_name, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteContextFunction_initializeGenomicElementType(const std::string &p_function_name, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteContextFunction_initializeInteractionType(const std::string &p_function_name, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteContextFunction_initializeMutationType(const std::string &p_function_name, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteContextFunction_initializeRecombinationRate(const std::string &p_function_name, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteContextFunction_initializeGeneConversion(const std::string &p_function_name, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteContextFunction_initializeMutationRate(const std::string &p_function_name, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteContextFunction_initializeSex(const std::string &p_function_name, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteContextFunction_initializeSLiMOptions(const std::string &p_function_name, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteContextFunction_initializeSLiMModelType(const std::string &p_function_name, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	
	EidosSymbolTable *SymbolsFromBaseSymbols(EidosSymbolTable *p_base_symbols);				// derive a symbol table, adding our own symbols if needed
	
	static const std::vector<EidosFunctionSignature_SP> *ZeroGenerationFunctionSignatures(void);		// all zero-gen functions
	static void AddZeroGenerationFunctionsToMap(EidosFunctionMap &p_map);
	static void RemoveZeroGenerationFunctionsFromMap(EidosFunctionMap &p_map);
	
	static const std::vector<const EidosMethodSignature*> *AllMethodSignatures(void);		// does not depend on sim state, so can be a class method
	static const std::vector<const EidosPropertySignature*> *AllPropertySignatures(void);	// does not depend on sim state, so can be a class method
	
	virtual const EidosObjectClass *Class(void) const;
	
	virtual EidosValue_SP GetProperty(EidosGlobalStringID p_property_id);
	virtual void SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value);
	
	virtual EidosValue_SP ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	
#ifdef SLIM_WF_ONLY
	EidosValue_SP ExecuteMethod_addSubpopSplit(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
#endif	// SLIM_WF_ONLY
	
	EidosValue_SP ExecuteMethod_addSubpop(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_deregisterScriptBlock(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_mutationFreqsCounts(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_mutationsOfType(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_countOfMutationsOfType(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_outputFixedMutations(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_outputFull(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_outputMutations(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_readFromPopulationFile(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_recalculateFitness(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_registerEarlyLateEvent(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_registerFitnessCallback(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_registerInteractionCallback(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_registerMateModifyRecCallback(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_registerReproductionCallback(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_rescheduleScriptBlock(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_simulationFinished(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
};


#endif /* defined(__SLiM__slim_sim__) */












































































