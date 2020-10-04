//
//  slim_sim.h
//  SLiM
//
//  Created by Ben Haller on 12/26/14.
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
 
 SLiM encapsulates a simulation run as a SLiMSim object.  This allows a simulation to be stepped and controlled by a GUI.
 
 */

#ifndef __SLiM__slim_sim__
#define __SLiM__slim_sim__


#include <stdio.h>
#include <map>
#include <vector>
#include <iostream>

#include "slim_globals.h"
#include "mutation.h"
#include "mutation_type.h"
#include "population.h"
#include "chromosome.h"
#include "eidos_script.h"
#include "eidos_value.h"
#include "eidos_functions.h"
#include "slim_eidos_block.h"
#include "interaction_type.h"

//TREE SEQUENCE
//INCLUDE JEROME's TABLES API
#ifdef __cplusplus
extern "C" {
#endif
#include "../treerec/tskit/trees.h"
#include "../treerec/tskit/tables.h"
#include "../treerec/tskit/text_input.h"
#ifdef __cplusplus
}
#endif


class EidosInterpreter;
class Individual;
struct ts_subpop_info;
struct ts_mut_info;

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

enum class SLiMFileFormat
{
	kFileNotFound = -1,
	kFormatUnrecognized = 0,
	kFormatSLiMText,				// as saved by outputFull(filePath, binary=F)
	kFormatSLiMBinary,				// as saved by outputFull(filePath, binary=T)
	kFormatTskitText,				// as saved by treeSeqOutput(path, binary=F)
	kFormatTskitBinary_HDF5,		// old file format, no longer supported
	kFormatTskitBinary_kastore,	// as saved by treeSeqOutput(path, binary=T)
};


// TREE SEQUENCE RECORDING
#pragma mark -
#pragma mark treeseq recording metadata records
#pragma mark -

// These structs are used by the tree-rec code to record all metadata about an object that needs to be saved.
// Note that this information is a snapshot taken at one point in time, and may become stale; be careful.
// Changing these structs will break binary compatibility in our output files, and requires changes elsewhere.
// Note that these structs are packed, and so accesses to them and within them may be unaligned; we assume
// that is OK on the platforms we run on, so as to keep file sizes down.

typedef struct __attribute__((__packed__)) {
	slim_objectid_t mutation_type_id_;		// 4 bytes (int32_t): the id of the mutation type the mutation belongs to
	slim_selcoeff_t selection_coeff_;		// 4 bytes (float): the selection coefficient
	slim_objectid_t subpop_index_;			// 4 bytes (int32_t): the id of the subpopulation in which the mutation arose
	slim_generation_t origin_generation_;	// 4 bytes (int32_t): the generation in which the mutation arose
	int8_t nucleotide_;						// 1 byte (int8_t): the nucleotide for the mutation (0='A', 1='C', 2='G', 3='T'), or -1
} MutationMetadataRec;
typedef struct __attribute__((__packed__)) {
	slim_objectid_t mutation_type_id_;		// 4 bytes (int32_t): the id of the mutation type the mutation belongs to
	slim_selcoeff_t selection_coeff_;		// 4 bytes (float): the selection coefficient
	slim_objectid_t subpop_index_;			// 4 bytes (int32_t): the id of the subpopulation in which the mutation arose
	slim_generation_t origin_generation_;	// 4 bytes (int32_t): the generation in which the mutation arose
} MutationMetadataRec_PRENUC;	// used to read .trees file version 0.2, before nucleotides were added

typedef struct __attribute__((__packed__)) {
	slim_genomeid_t genome_id_;				// 8 bytes (int64_t): the SLiM genome ID for this genome, assigned by pedigree rec
	uint8_t is_null_;						// 1 byte (uint8_t): true if this is a null genome (should never contain mutations)
	GenomeType type_;						// 1 byte (uint8_t): the type of the genome (A, X, Y)
} GenomeMetadataRec;

typedef struct __attribute__((__packed__)) {
	slim_pedigreeid_t pedigree_id_;			// 8 bytes (int64_t): the SLiM pedigree ID for this individual, assigned by pedigree rec
	slim_age_t age_;                        // 4 bytes (int32_t): the age of the individual (-1 for WF models)
	slim_objectid_t subpopulation_id_;      // 4 bytes (int32_t): the subpopulation the individual belongs to
	IndividualSex sex_;						// 4 bytes (int32_t): the sex of the individual, as defined by the IndividualSex enum
	uint32_t flags_;						// 4 bytes (uint32_t): assorted flags, see below
} IndividualMetadataRec;

#define SLIM_INDIVIDUAL_METADATA_MIGRATED	0x01	// set if the individual has migrated in this generation

typedef struct __attribute__((__packed__)) {
	slim_objectid_t subpopulation_id_;      // 4 bytes (int32_t): the id of this subpopulation
	double selfing_fraction_;				// 8 bytes (double): selfing fraction (unused in non-sexual models), unused in nonWF models
	double female_clone_fraction_;			// 8 bytes (double): cloning fraction (females / hermaphrodites), unused in nonWF models
	double male_clone_fraction_;			// 8 bytes (double): cloning fraction (males / hermaphrodites), unused in nonWF models
	double sex_ratio_;						// 8 bytes (double): sex ratio (M:M+F), unused in nonWF models
	double bounds_x0_;						// 8 bytes (double): spatial bounds, unused in non-spatial models
	double bounds_x1_;						// 8 bytes (double): spatial bounds, unused in non-spatial models
	double bounds_y0_;						// 8 bytes (double): spatial bounds, unused in non-spatial / 1D models
	double bounds_y1_;						// 8 bytes (double): spatial bounds, unused in non-spatial / 1D models
	double bounds_z0_;						// 8 bytes (double): spatial bounds, unused in non-spatial / 1D / 2D models
	double bounds_z1_;						// 8 bytes (double): spatial bounds, unused in non-spatial / 1D / 2D models
	uint32_t migration_rec_count_;			// 4 bytes (int32_t): the number of migration records, 0 in nonWF models
	// followed by migration_rec_count_ instances of SubpopulationMigrationMetadataRec
} SubpopulationMetadataRec;

typedef struct __attribute__((__packed__)) {
	slim_objectid_t source_subpop_id_;		// 4 bytes (int32_t): the id of the source subpopulation, unused in nonWF models
	double migration_rate_;					// 8 bytes (double): the migration rate from source_subpop_id_, unused in nonWF models
} SubpopulationMigrationMetadataRec;

// We double-check the size of these records to make sure we understand what they contain and how they're packed
static_assert(sizeof(MutationMetadataRec) == 17, "MutationMetadataRec is not 17 bytes!");
static_assert(sizeof(GenomeMetadataRec) == 10, "GenomeMetadataRec is not 10 bytes!");
static_assert(sizeof(IndividualMetadataRec) == 24, "IndividualMetadataRec is not 24 bytes!");
static_assert(sizeof(SubpopulationMetadataRec) == 88, "SubpopulationMetadataRec is not 88 bytes!");
static_assert(sizeof(SubpopulationMigrationMetadataRec) == 12, "SubpopulationMigrationMetadataRec is not 12 bytes!");

// We check endianness on the platform we're building on; we assume little-endianness in our read/write code, I think.
#if defined(__BYTE_ORDER__)
#if (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#warning Reading and writing binary files with SLiM may produce non-standard results on this (big-endian) platform due to endianness
#endif
#endif


// Memory usage assessment as done by SLiMSim::TabulateMemoryUsage() is placed into this struct
typedef struct
{
	int64_t chromosomeObjects_count;
	size_t chromosomeObjects;
	size_t chromosomeMutationRateMaps;
	size_t chromosomeRecombinationRateMaps;
	size_t chromosomeAncestralSequence;
	
	int64_t genomeObjects_count;
	size_t genomeObjects;
	size_t genomeExternalBuffers;
	size_t genomeUnusedPoolSpace;
	size_t genomeUnusedPoolBuffers;
	
	int64_t genomicElementObjects_count;
	size_t genomicElementObjects;
	
	int64_t genomicElementTypeObjects_count;
	size_t genomicElementTypeObjects;
	
	int64_t individualObjects_count;
	size_t individualObjects;
	size_t individualUnusedPoolSpace;
	
	int64_t interactionTypeObjects_count;
	size_t interactionTypeObjects;
	size_t interactionTypeKDTrees;
	size_t interactionTypePositionCaches;
	size_t interactionTypeSparseArrays;
	
	int64_t mutationObjects_count;
	size_t mutationObjects;
	size_t mutationRefcountBuffer;
	size_t mutationUnusedPoolSpace;
	
	int64_t mutationRunObjects_count;
	size_t mutationRunObjects;
	size_t mutationRunExternalBuffers;
	size_t mutationRunNonneutralCaches;
	size_t mutationRunUnusedPoolSpace;
	size_t mutationRunUnusedPoolBuffers;
	
	int64_t mutationTypeObjects_count;
	size_t mutationTypeObjects;
	
	int64_t slimsimObjects_count;
	size_t slimsimObjects;
	size_t slimsimTreeSeqTables;
	
	int64_t subpopulationObjects_count;
	size_t subpopulationObjects;
	size_t subpopulationFitnessCaches;
	size_t subpopulationParentTables;
	size_t subpopulationSpatialMaps;
	size_t subpopulationSpatialMapsDisplay;
	
	int64_t substitutionObjects_count;
	size_t substitutionObjects;
	
	size_t eidosASTNodePool;
	size_t eidosSymbolTablePool;
	size_t eidosValuePool;
	
	size_t totalMemoryUsage;
} SLiM_MemoryUsage;


#pragma mark -
#pragma mark SLiMSim
#pragma mark -

class SLiMSim : public EidosDictionary
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
	std::vector<SLiMEidosBlock*> cached_mutation_callbacks_;
	std::vector<SLiMEidosBlock*> cached_reproduction_callbacks_;
	std::vector<SLiMEidosBlock*> cached_userdef_functions_;
	
#ifdef SLIMGUI
public:
	
	bool simulation_valid_ = true;													// set to false if a terminating condition is encountered while running in SLiMgui

#if (defined(SLIMGUI) && (SLIMPROFILING == 1))
	// PROFILING
	eidos_profile_t profile_stage_totals_[8];										// profiling clocks; index 0 is initialize(), the rest follow sequentially; [7] is TS simplification
	eidos_profile_t profile_callback_totals_[11];									// profiling clocks; these follow SLiMEidosBlockType, except no SLiMEidosUserDefinedFunction
	
	SLiM_MemoryUsage profile_last_memory_usage_;
	SLiM_MemoryUsage profile_total_memory_usage_;
	int64_t total_memory_tallies_;
	
#if SLIM_USE_NONNEUTRAL_CACHES
	std::vector<int32_t> profile_mutcount_history_;									// a record of the mutation run count used in each generation
	std::vector<int32_t> profile_nonneutral_regime_history_;						// a record of the nonneutral regime used in each generation
	int64_t profile_mutation_total_usage_;											// how many (non-unique) mutations were used by mutation runs, summed across generations
	int64_t profile_nonneutral_mutation_total_;										// of profile_mutation_total_usage_, how many were deemed to be nonneutral
	int64_t profile_mutrun_total_usage_;											// how many (non-unique) mutruns were used by genomes, summed across generations
	int64_t profile_unique_mutrun_total_;											// of profile_mutrun_total_usage_, how many unique mutruns existed, summed across generations
	int64_t profile_mutrun_nonneutral_recache_total_;								// of profile_unique_mutrun_total_, how many mutruns regenerated their nonneutral cache
	int64_t profile_max_mutation_index_;											// the largest mutation index seen over the course of the profile
#endif	// SLIM_USE_NONNEUTRAL_CACHES
#endif	// (defined(SLIMGUI) && (SLIMPROFILING == 1))
	
#else
private:
#endif
	
	SLiMModelType model_type_ = SLiMModelType::kModelTypeWF;						// the overall model type: WF or nonWF, at present; affects many other things!
	
	EidosSymbolTable *simulation_constants_ = nullptr;								// A symbol table of constants defined by SLiM (p1, g1, m1, s1, etc.)
	EidosFunctionMap simulation_functions_;											// A map of all defined functions in the simulation
	
	// these ivars track the generation, generation stage, and related state
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
	SLiMFileFormat FormatOfPopulationFile(const std::string &p_file_string);		// determine the format of a file/folder at the given path using leading bytes, etc.
	void InitializeFromFile(std::istream &p_infile);								// parse a input file and set up the simulation state from its contents
	slim_generation_t InitializePopulationFromFile(const std::string &p_file_string, EidosInterpreter *p_interpreter);	// initialize the population from the file
	slim_generation_t _InitializePopulationFromTextFile(const char *p_file, EidosInterpreter *p_interpreter);			// initialize the population from a SLiM text file
	slim_generation_t _InitializePopulationFromBinaryFile(const char *p_file, EidosInterpreter *p_interpreter);			// initialize the population from a SLiM binary file
	
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
	int num_treeseq_declarations_;
	int num_modeltype_declarations_;
	int num_ancseq_declarations_;
	int num_hotspot_maps_;
	
	slim_position_t last_genomic_element_position_ = -1;	// used to check new genomic elements for consistency
	
	// pedigree tracking: off by default, optionally turned on at init time to enable calls to TrackPedigreeWithParents()
	//bool pedigrees_enabled_ = false;				// BCH 3 Sept. 2020: this flag is deprecated; pedigree tracking is now ALWAYS ENABLED
	
	// continuous space support
	int spatial_dimensionality_ = 0;
	bool periodic_x_ = false;
	bool periodic_y_ = false;
	bool periodic_z_ = false;
	
	// preferred mutation run length
	int preferred_mutrun_count_ = 0;												// 0 represents no preference
	
	// preventing incidental selfing in hermaphroditic models
	bool prevent_incidental_selfing_ = false;
	
	// nucleotide-based models
	bool nucleotide_based_ = false;
	double max_nucleotide_mut_rate_;				// the highest rate for any genetic background in any genomic element type
	
	EidosSymbolTableEntry self_symbol_;												// for fast setup of the symbol table
	
	slim_usertag_t tag_value_ = SLIM_TAG_UNSET_VALUE;								// a user-defined tag value
	
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
#pragma mark -
#pragma mark treeseq recording ivars
#pragma mark -
	bool recording_tree_ = false;				// true if we are doing tree sequence recording
	bool recording_mutations_ = false;			// true if we are recording mutations in our tree sequence tables
	
	tsk_table_collection_t tables_;
	tsk_bookmark_t table_position_;
	
    std::vector<tsk_id_t> remembered_genomes_;
	//Individual *current_new_individual_;
	
	bool running_coalescence_checks_ = false;	// true if we check for coalescence after each simplification
	bool last_coalescence_state_ = false;		// if running_coalescence_checks_==true, updated every simplification
	
	bool running_treeseq_crosschecks_ = false;	// true if crosschecks between our tree sequence tables and SLiM's data are enabled
	int treeseq_crosschecks_interval_ = 1;		// crosschecks, if enabled, will be done every treeseq_crosschecks_interval_ generations
	
	double simplification_ratio_;				// the pre:post table size ratio we target with our automatic simplification heuristic
	int64_t simplification_interval_;			// the generation interval between simplifications; -1 if not used (in which case the ratio is used)
	int64_t simplify_elapsed_ = 0;				// the number of generations elapsed since a simplification was done (automatic or otherwise)
	double simplify_interval_;					// the current number of generations between automatic simplifications when using simplification_ratio_
	
	slim_generation_t tree_seq_generation_ = 0;	// the generation for the tree sequence code, incremented after offspring generation
												// this is needed since addSubpop() in an early() event makes one gen, and then the offspring
												// arrive in the same generation according to SLiM, which confuses the tree-seq code
	double tree_seq_generation_offset_ = 0;		// this is a fractional offset added to tree_seq_generation_; this is needed to make successive calls
												// to addSubpopSplit() arrive at successively later times; see Population::AddSubpopulationSplit()
	
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
	bool warned_no_max_distance_ = false;
	bool warned_inSLiMgui_deprecated_ = false;
	bool warned_readFromVCF_mutIDs_unused_ = false;
	
	// these ivars are set around callbacks so we know what type of callback we're in, to prevent illegal operations during callbacks
	SLiMEidosBlockType executing_block_type_ = SLiMEidosBlockType::SLiMEidosNoBlockType;	// the innermost callback type we're executing now
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
	
	SLiMSim(const SLiMSim&) = delete;												// no copying
	SLiMSim& operator=(const SLiMSim&) = delete;									// no copying
	explicit SLiMSim(std::istream &p_infile);										// construct a SLiMSim from an input stream
	~SLiMSim(void);																	// destructor
	
	void InitializeRNGFromSeed(unsigned long int *p_override_seed_ptr);				// should be called right after construction, generally
	void TabulateMemoryUsage(SLiM_MemoryUsage *p_usage, EidosSymbolTable *p_current_symbols);	// used by outputUsage() and SLiMgui profiling
	
	// Managing script blocks; these two methods should be used as a matched pair, bracketing each generation stage that calls out to script
	void ValidateScriptBlockCaches(void);
	std::vector<SLiMEidosBlock*> ScriptBlocksMatching(slim_generation_t p_generation, SLiMEidosBlockType p_event_type, slim_objectid_t p_mutation_type_id, slim_objectid_t p_interaction_type_id, slim_objectid_t p_subpopulation_id);
	std::vector<SLiMEidosBlock*> &AllScriptBlocks();
	void OptimizeScriptBlock(SLiMEidosBlock *p_script_block);
	void AddScriptBlock(SLiMEidosBlock *p_script_block, EidosInterpreter *p_interpreter, const EidosToken *p_error_token);
	void DeregisterScheduledScriptBlocks(void);
	void DeregisterScheduledInteractionBlocks(void);
	void ExecuteFunctionDefinitionBlock(SLiMEidosBlock *p_script_block);			// execute a SLiMEidosBlock that defines a function
	void CheckScheduling(slim_generation_t p_target_gen, SLiMGenerationStage p_target_stage);
	
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
	void CollectSLiMguiMemoryUsageProfileInfo(void);
#if SLIM_USE_NONNEUTRAL_CACHES
	void CollectSLiMguiMutationProfileInfo(void);
#endif
#endif
	
	// Mutation stack policy checking
	inline __attribute__((always_inline)) void MutationStackPolicyChanged(void)												{ mutation_stack_policy_changed_ = true; }
	inline __attribute__((always_inline)) void CheckMutationStackPolicy(void)												{ if (mutation_stack_policy_changed_) _CheckMutationStackPolicy(); }
	void _CheckMutationStackPolicy(void);
	
	// Nucleotide-based models
	void CacheNucleotideMatrices(void);
	void CreateNucleotideMutationRateMap(void);
	
	// accessors
	inline __attribute__((always_inline)) EidosSymbolTable &SymbolTable(void) const											{ return *simulation_constants_; }
	inline __attribute__((always_inline)) EidosFunctionMap &FunctionMap(void)												{ return simulation_functions_; }
	
	inline __attribute__((always_inline)) SLiMModelType ModelType(void) const												{ return model_type_; }
	inline __attribute__((always_inline)) slim_generation_t Generation(void) const											{ return generation_; }
	void SetGeneration(slim_generation_t p_new_generation);
	inline __attribute__((always_inline)) SLiMGenerationStage GenerationStage(void) const									{ return generation_stage_; }
	inline __attribute__((always_inline)) Chromosome &TheChromosome(void)													{ return chromosome_; }
	inline __attribute__((always_inline)) Population &ThePopulation(void)													{ return population_; }
	inline __attribute__((always_inline)) const std::map<slim_objectid_t,MutationType*> &MutationTypes(void) const			{ return mutation_types_; }
	inline __attribute__((always_inline)) const std::map<slim_objectid_t,GenomicElementType*> &GenomicElementTypes(void)	{ return genomic_element_types_; }
	inline __attribute__((always_inline)) const std::map<slim_objectid_t,InteractionType*> &InteractionTypes(void)			{ return interaction_types_; }
	
	inline Subpopulation *SubpopulationWithID(slim_objectid_t p_subpop_id) {
		auto id_iter = population_.subpops_.find(p_subpop_id);
		return (id_iter == population_.subpops_.end()) ? nullptr : id_iter->second;
	}
	inline MutationType *MutationTypeWithID(slim_objectid_t p_muttype_id) {
		auto id_iter = mutation_types_.find(p_muttype_id);
		return (id_iter == mutation_types_.end()) ? nullptr : id_iter->second;
	}
	inline GenomicElementType *GenomicElementTypeTypeWithID(slim_objectid_t p_getype_id) {
		auto id_iter = genomic_element_types_.find(p_getype_id);
		return (id_iter == genomic_element_types_.end()) ? nullptr : id_iter->second;
	}
	inline InteractionType *InteractionTypeWithID(slim_objectid_t p_inttype_id) {
		auto id_iter = interaction_types_.find(p_inttype_id);
		return (id_iter == interaction_types_.end()) ? nullptr : id_iter->second;
	}
	
	inline __attribute__((always_inline)) bool SexEnabled(void) const														{ return sex_enabled_; }
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
	
	inline __attribute__((always_inline)) bool IsNucleotideBased(void) const												{ return nucleotide_based_; }

	
	// TREE SEQUENCE RECORDING
#pragma mark -
#pragma mark treeseq recording methods
#pragma mark -
	inline __attribute__((always_inline)) bool RecordingTreeSequence(void) const											{ return recording_tree_; }
	inline __attribute__((always_inline)) bool RecordingTreeSequenceMutations(void) const									{ return recording_mutations_; }
	inline __attribute__((always_inline)) void AboutToSplitSubpop(void)														{ tree_seq_generation_offset_ += 0.00001; }	// see Population::AddSubpopulationSplit()
	
	static void handle_error(std::string msg, int error);
	static void MetadataForMutation(Mutation *p_mutation, MutationMetadataRec *p_metadata);
	static void MetadataForSubstitution(Substitution *p_substitution, MutationMetadataRec *p_metadata);
	static void MetadataForGenome(Genome *p_genome, GenomeMetadataRec *p_metadata);
	static void MetadataForIndividual(Individual *p_individual, IndividualMetadataRec *p_metadata);
	static void TreeSequenceDataToAscii(tsk_table_collection_t *p_tables);
	static void DerivedStatesFromAscii(tsk_table_collection_t *p_tables);
	static void DerivedStatesToAscii(tsk_table_collection_t *p_tables);
	
	void RecordTablePosition(void);
	void AllocateTreeSequenceTables(void);
	void SetCurrentNewIndividual(Individual *p_individual);
	void RecordNewGenome(std::vector<slim_position_t> *p_breakpoints, Genome *p_new_genome, const Genome *p_initial_parental_genome, const Genome *p_second_parental_genome);
	void RecordNewDerivedState(const Genome *p_genome, slim_position_t p_position, const std::vector<Mutation *> &p_derived_mutations);
	void RetractNewIndividual(void);
    void AddIndividualsToTable(Individual * const *p_individual, size_t p_num_individuals, tsk_table_collection_t *p_tables, uint32_t p_flags);
	void AddCurrentGenerationToIndividuals(tsk_table_collection_t *p_tables);
	void UnmarkFirstGenerationSamples(tsk_table_collection_t *p_tables);
	void RemarkFirstGenerationSamples(tsk_table_collection_t *p_tables);
	void FixAliveIndividuals(tsk_table_collection_t *p_tables);
	void WritePopulationTable(tsk_table_collection_t *p_tables);
	void WriteProvenanceTable(tsk_table_collection_t *p_tables, bool p_use_newlines, bool p_include_model);
	void WriteTreeSequenceMetadata(tsk_table_collection_t *p_tables);
	void ReadTreeSequenceMetadata(tsk_table_collection_t *p_tables, slim_generation_t *p_generation, SLiMModelType *p_model_type, int *p_file_version);
	void WriteTreeSequence(std::string &p_recording_tree_path, bool p_binary, bool p_simplify, bool p_include_model);
    void ReorderIndividualTable(tsk_table_collection_t *p_tables, std::vector<int> p_individual_map, bool p_keep_unmapped);
	void SimplifyTreeSequence(void);
	void CheckCoalescenceAfterSimplification(void);
	void CheckAutoSimplification(void);
    void TreeSequenceDataFromAscii(std::string NodeFileName, 
            std::string EdgeFileName, std::string SiteFileName, std::string MutationFileName, 
            std::string IndividualsFileName, std::string PopulationFileName, std::string ProvenanceFileName);
	void FreeTreeSequence(void);
	void RecordAllDerivedStatesFromSLiM(void);
	void DumpMutationTable(void);
	void CheckTreeSeqIntegrity(void);		// checks the tree sequence tables themselves
	void CrosscheckTreeSeqIntegrity(void);	// checks the tree sequence tables against SLiM's data structures
	void TSXC_Enable(void);
	
	void __TabulateSubpopulationsFromTreeSequence(std::unordered_map<slim_objectid_t, ts_subpop_info> &p_subpopInfoMap, tsk_treeseq_t *p_ts, SLiMModelType p_file_model_type);
	void __CreateSubpopulationsFromTabulation(std::unordered_map<slim_objectid_t, ts_subpop_info> &p_subpopInfoMap, EidosInterpreter *p_interpreter, std::unordered_map<tsk_id_t, Genome *> &p_nodeToGenomeMap);
	void __ConfigureSubpopulationsFromTables(EidosInterpreter *p_interpreter);
	void __TabulateMutationsFromTables(std::unordered_map<slim_mutationid_t, ts_mut_info> &p_mutMap, int p_file_version);
	void __TallyMutationReferencesWithTreeSequence(std::unordered_map<slim_mutationid_t, ts_mut_info> &p_mutMap, std::unordered_map<tsk_id_t, Genome *> p_nodeToGenomeMap, tsk_treeseq_t *p_ts);
	void __CreateMutationsFromTabulation(std::unordered_map<slim_mutationid_t, ts_mut_info> &p_mutInfoMap, std::unordered_map<slim_mutationid_t, MutationIndex> &p_mutIndexMap);
	void __AddMutationsFromTreeSequenceToGenomes(std::unordered_map<slim_mutationid_t, MutationIndex> &p_mutIndexMap, std::unordered_map<tsk_id_t, Genome *> p_nodeToGenomeMap, tsk_treeseq_t *p_ts);
	slim_generation_t _InstantiateSLiMObjectsFromTables(EidosInterpreter *p_interpreter);								// given tree-seq tables, makes individuals, genomes, and mutations
	slim_generation_t _InitializePopulationFromTskitTextFile(const char *p_file, EidosInterpreter *p_interpreter);	// initialize the population from an tskit text file
	slim_generation_t _InitializePopulationFromTskitBinaryFile(const char *p_file, EidosInterpreter *p_interpreter);	// initialize the population from an tskit binary file
	size_t MemoryUsageForTables(tsk_table_collection_t &p_tables);
	
	//
	// Eidos support
	//
#pragma mark -
#pragma mark Eidos support
#pragma mark -
	inline EidosSymbolTableEntry &SymbolTableEntry(void) { return self_symbol_; };
	
	virtual EidosValue_SP ContextDefinedFunctionDispatch(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) override;
	EidosValue_SP ExecuteContextFunction_initializeAncestralNucleotides(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteContextFunction_initializeGenomicElement(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteContextFunction_initializeGenomicElementType(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteContextFunction_initializeInteractionType(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteContextFunction_initializeMutationType(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteContextFunction_initializeRecombinationRate(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteContextFunction_initializeGeneConversion(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteContextFunction_initializeMutationRate(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteContextFunction_initializeHotspotMap(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteContextFunction_initializeSex(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteContextFunction_initializeSLiMOptions(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteContextFunction_initializeTreeSeq(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteContextFunction_initializeSLiMModelType(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	
	EidosSymbolTable *SymbolsFromBaseSymbols(EidosSymbolTable *p_base_symbols);				// derive a symbol table, adding our own symbols if needed
	
	static const std::vector<EidosFunctionSignature_CSP> *ZeroGenerationFunctionSignatures(void);		// all zero-gen functions
	static void AddZeroGenerationFunctionsToMap(EidosFunctionMap &p_map);
	static void RemoveZeroGenerationFunctionsFromMap(EidosFunctionMap &p_map);
	
	static const std::vector<EidosFunctionSignature_CSP> *SLiMFunctionSignatures(void);		// all non-zero-gen functions
	static void AddSLiMFunctionsToMap(EidosFunctionMap &p_map);
	
	static const std::vector<EidosMethodSignature_CSP> *AllMethodSignatures(void);		// does not depend on sim state, so can be a class method
	static const std::vector<EidosPropertySignature_CSP> *AllPropertySignatures(void);	// does not depend on sim state, so can be a class method
	
	virtual const EidosObjectClass *Class(void) const override;
	
	virtual EidosValue_SP GetProperty(EidosGlobalStringID p_property_id) override;
	virtual void SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value) override;
	
	virtual EidosValue_SP ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) override;
	
#ifdef SLIM_WF_ONLY
	EidosValue_SP ExecuteMethod_addSubpopSplit(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
#endif	// SLIM_WF_ONLY
	
	EidosValue_SP ExecuteMethod_addSubpop(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_deregisterScriptBlock(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_mutationFreqsCounts(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_mutationsOfType(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_countOfMutationsOfType(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_outputFixedMutations(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_outputFull(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_outputMutations(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_outputUsage(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_readFromPopulationFile(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_recalculateFitness(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_registerEarlyLateEvent(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_registerFitnessCallback(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_registerInteractionCallback(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_registerMateModifyRecCallback(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_registerMutationCallback(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_registerReproductionCallback(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_rescheduleScriptBlock(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_simulationFinished(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_subsetMutations(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_treeSeqCoalesced(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_treeSeqSimplify(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_treeSeqRememberIndividuals(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_treeSeqOutput(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
};


#endif /* defined(__SLiM__slim_sim__) */












































































