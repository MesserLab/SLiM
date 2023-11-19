//
//  species.h
//  SLiM
//
//  Created by Ben Haller on 12/26/14.
//  Copyright (c) 2014-2023 Philipp Messer.  All rights reserved.
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
 
 SLiM encapsulates a simulation run as a Species object.  This allows a simulation to be stepped and controlled by a GUI.
 
 */

#ifndef __SLiM__species__
#define __SLiM__species__


#include <vector>
#include <map>
#include <ctime>
#include <unordered_set>

#include "slim_globals.h"
#include "population.h"
#include "chromosome.h"
#include "eidos_value.h"
#include "mutation_run.h"

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


class Community;
class EidosInterpreter;
class Individual;
class MutationType;
class GenomicElementType;
class InteractionType;
struct ts_subpop_info;
struct ts_mut_info;

extern EidosClass *gSLiM_Species_Class;

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
	slim_tick_t origin_tick_;				// 4 bytes (int32_t): the tick in which the mutation arose
	int8_t nucleotide_;						// 1 byte (int8_t): the nucleotide for the mutation (0='A', 1='C', 2='G', 3='T'), or -1
} MutationMetadataRec;
typedef struct __attribute__((__packed__)) {
	slim_objectid_t mutation_type_id_;		// 4 bytes (int32_t): the id of the mutation type the mutation belongs to
	slim_selcoeff_t selection_coeff_;		// 4 bytes (float): the selection coefficient
	slim_objectid_t subpop_index_;			// 4 bytes (int32_t): the id of the subpopulation in which the mutation arose
	slim_tick_t origin_tick_;				// 4 bytes (int32_t): the tick in which the mutation arose
} MutationMetadataRec_PRENUC;	// used to read .trees file version 0.2, before nucleotides were added

typedef struct __attribute__((__packed__)) {
	slim_genomeid_t genome_id_;				// 8 bytes (int64_t): the SLiM genome ID for this genome, assigned by pedigree rec
	uint8_t is_null_;						// 1 byte (uint8_t): true if this is a null genome (should never contain mutations)
	GenomeType type_;						// 1 byte (uint8_t): the type of the genome (A, X, Y)
} GenomeMetadataRec;

typedef struct __attribute__((__packed__)) {
	slim_pedigreeid_t pedigree_id_;			// 8 bytes (int64_t): the SLiM pedigree ID for this individual, assigned by pedigree rec
	slim_pedigreeid_t pedigree_p1_;			// 8 bytes (int64_t): the SLiM pedigree ID for this individual's parent 1
	slim_pedigreeid_t pedigree_p2_;			// 8 bytes (int64_t): the SLiM pedigree ID for this individual's parent 2
	slim_age_t age_;                        // 4 bytes (int32_t): the age of the individual (-1 for WF models)
	slim_objectid_t subpopulation_id_;      // 4 bytes (int32_t): the subpopulation the individual belongs to
	int32_t sex_;							// 4 bytes (int32_t): the sex of the individual, as defined by the IndividualSex enum
	uint32_t flags_;						// 4 bytes (uint32_t): assorted flags, see below
} IndividualMetadataRec;
typedef struct __attribute__((__packed__)) {
	slim_pedigreeid_t pedigree_id_;			// 8 bytes (int64_t): the SLiM pedigree ID for this individual, assigned by pedigree rec
	slim_age_t age_;                        // 4 bytes (int32_t): the age of the individual (-1 for WF models)
	slim_objectid_t subpopulation_id_;      // 4 bytes (int32_t): the subpopulation the individual belongs to
	int32_t sex_;							// 4 bytes (int32_t): the sex of the individual, as defined by the IndividualSex enum
	uint32_t flags_;						// 4 bytes (uint32_t): assorted flags, see below
 } IndividualMetadataRec_PREPARENT;	// used to read .trees file versions 0.6 and earlier, before parent pedigree ids were added

#define SLIM_INDIVIDUAL_METADATA_MIGRATED	0x01	// set if the individual has migrated in this cycle

// *** Subpopulation metadata is now JSON
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
} SubpopulationMetadataRec_PREJSON;

typedef struct __attribute__((__packed__)) {
	slim_objectid_t source_subpop_id_;		// 4 bytes (int32_t): the id of the source subpopulation, unused in nonWF models
	double migration_rate_;					// 8 bytes (double): the migration rate from source_subpop_id_, unused in nonWF models
} SubpopulationMigrationMetadataRec_PREJSON;

// We double-check the size of these records to make sure we understand what they contain and how they're packed
static_assert(sizeof(MutationMetadataRec) == 17, "MutationMetadataRec is not 17 bytes!");
static_assert(sizeof(GenomeMetadataRec) == 10, "GenomeMetadataRec is not 10 bytes!");
static_assert(sizeof(IndividualMetadataRec) == 40, "IndividualMetadataRec is not 40 bytes!");
static_assert(sizeof(SubpopulationMetadataRec_PREJSON) == 88, "SubpopulationMetadataRec_PREJSON is not 88 bytes!");
static_assert(sizeof(SubpopulationMigrationMetadataRec_PREJSON) == 12, "SubpopulationMigrationMetadataRec_PREJSON is not 12 bytes!");

// We check endianness on the platform we're building on; we assume little-endianness in our read/write code, I think.
#if defined(__BYTE_ORDER__)
#if (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#warning Reading and writing binary files with SLiM may produce non-standard results on this (big-endian) platform due to endianness
#endif
#endif


#pragma mark -
#pragma mark Species
#pragma mark -

class Species : public EidosDictionaryUnretained
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.
	
private:
	typedef EidosDictionaryUnretained super;

#ifdef SLIMGUI
public:
#else
private:
#endif
	
	// Species simulation state
	slim_tick_t cycle_ = 0;													// the current cycle reached in simulation
	EidosValue_SP cached_value_cycle_;											// a cached value for cycle_; invalidates automatically when used
	
	bool species_active_ = true;													// the "active" property of the species
	slim_tick_t tick_modulo_ = 1;													// the species is active every tick_modulo_ ticks
	slim_tick_t tick_phase_ = 1;													// the species is first active in tick tick_phase_
	
	std::string color_;																// color to use when displayed (in SLiMgui)
	float color_red_, color_green_, color_blue_;									// cached color components from color_; should always be in sync
	
	bool has_genetics_ = true;														// false if the species has no mutation, no recombination, no muttypes/getypes, no genomic elements
	
	// std::map is used instead of std::unordered_map mostly for convenience, for sorted order in the UI; these are unlikely to be bottlenecks I think
	std::map<slim_objectid_t,MutationType*> mutation_types_;						// OWNED POINTERS: this map is the owner of all allocated MutationType objects
	std::map<slim_objectid_t,GenomicElementType*> genomic_element_types_;			// OWNED POINTERS: this map is the owner of all allocated GenomicElementType objects
	
	bool mutation_stack_policy_changed_ = true;										// when set, the stacking policy settings need to be checked for consistency
	
	// SEX ONLY: sex-related instance variables
	bool sex_enabled_ = false;														// true if sex is tracked for individuals; if false, all individuals are hermaphroditic
	GenomeType modeled_chromosome_type_ = GenomeType::kAutosome;					// the chromosome type; other types might still be instantiated (Y, if X is modeled, e.g.)
	
	// private initialization methods
#if EIDOS_ROBIN_HOOD_HASHING
	typedef robin_hood::unordered_flat_map<int64_t, slim_objectid_t> SUBPOP_REMAP_HASH;
 #elif STD_UNORDERED_MAP_HASHING
	typedef std::unordered_map<int64_t, slim_objectid_t> SUBPOP_REMAP_HASH;
 #endif
	
	SLiMFileFormat FormatOfPopulationFile(const std::string &p_file_string);		// determine the format of a file/folder at the given path using leading bytes, etc.
	void _CleanAllReferencesToSpecies(EidosInterpreter *p_interpreter);				// clean up in anticipation of loading new species state
	slim_tick_t InitializePopulationFromFile(const std::string &p_file_string, EidosInterpreter *p_interpreter, SUBPOP_REMAP_HASH &p_subpop_remap);	// initialize the population from the file
	slim_tick_t _InitializePopulationFromTextFile(const char *p_file, EidosInterpreter *p_interpreter);				// initialize the population from a SLiM text file
	slim_tick_t _InitializePopulationFromBinaryFile(const char *p_file, EidosInterpreter *p_interpreter);			// initialize the population from a SLiM binary file
	
	// initialization completeness check counts; used only when running initialize() callbacks
	int num_mutation_types_;
	int num_mutation_rates_;
	int num_genomic_element_types_;
	int num_genomic_elements_;
	int num_recombination_rates_;
	int num_gene_conversions_;
	int num_sex_declarations_;	// SEX ONLY; used to check for sex vs. non-sex errors in the file, so the #SEX tag must come before any reliance on SEX ONLY features
	int num_options_declarations_;
	int num_treeseq_declarations_;
	int num_ancseq_declarations_;
	int num_hotspot_maps_;
	int num_species_declarations_;
	
	slim_position_t last_genomic_element_position_ = -1;	// used to check new genomic elements for consistency
	
	// a "temporary graveyard" for keeping individuals that have been killed by killIndividuals(), until they can be freed
	std::vector<Individual *> graveyard_;
	
	// pedigree tracking: off by default, optionally turned on at init time to enable calls to TrackParentage_X()
	bool pedigrees_enabled_ = false;
	bool pedigrees_enabled_by_user_ = false;		// pedigree tracking was turned on by the user, which is user-visible
	bool pedigrees_enabled_by_SLiM_ = false;		// pedigree tracking has been forced on by tree-seq recording or SLiMgui, which is not user-visible
	
	// continuous space support
	int spatial_dimensionality_ = 0;
	bool periodic_x_ = false;
	bool periodic_y_ = false;
	bool periodic_z_ = false;
	
	// preferred mutation run length
	int preferred_mutrun_count_ = 0;												// 0 represents no preference
	
	// Species now keeps two MutationRunPools, one for freed MutationRun objects and one for in-use MutationRun objects,
	// as well as an object pool out of which completely new MutationRuns are allocated, all bundled in a MutationRunContext.
	// When running multithreaded, each of these becomes a vector of per-thread objects, so we can alloc/free runs in parallel code.
	// This stuff is not set up until after initialize() callbacks; nobody should be using MutationRuns before then.
#ifndef _OPENMP
	MutationRunContext mutation_run_context_SINGLE_;
#else
	int mutation_run_context_COUNT_ = 0;											// the number of PERTHREAD contexts
	std::vector<MutationRunContext *> mutation_run_context_PERTHREAD;
#endif
	
	// preventing incidental selfing in hermaphroditic models
	bool prevent_incidental_selfing_ = false;
	
	// nucleotide-based models
	bool nucleotide_based_ = false;
	double max_nucleotide_mut_rate_;				// the highest rate for any genetic background in any genomic element type
	
	EidosSymbolTableEntry self_symbol_;												// for fast setup of the symbol table
	
	slim_usertag_t tag_value_ = SLIM_TAG_UNSET_VALUE;								// a user-defined tag value
	
	// Mutation run optimization.  The ivars here are used only internally by Species; the canonical reference regarding the
	// number and length of mutation runs is kept by Chromosome (for the simulation) and by Genome (for each genome object).
	// If Species decides to change the number of mutation runs, it will update those canonical repositories accordingly.
	// A prefix of x_ is used on all mutation run experiment ivars, to avoid confusion.
#define SLIM_MUTRUN_EXPERIMENT_LENGTH	50		// kind of based on how large a sample size is needed to detect important differences fairly reliably by t-test
#define SLIM_MUTRUN_MAXIMUM_COUNT		1024	// the most mutation runs we will ever use; hard to imagine that any model will want more than this
	
	bool x_experiments_enabled_;				// if false, no experiments are run and no cycle runtimes are recorded
	
	int32_t x_current_mutcount_;				// the number of mutation runs we're currently using
	double *x_current_runtimes_;				// cycle runtimes recorded at this mutcount (SLIM_MUTRUN_EXPERIMENT_MAXLENGTH length)
	int x_current_buflen_;						// the number of runtimes in the current_mutcount_runtimes_ buffer
	
	int32_t x_previous_mutcount_;				// the number of mutation runs we previously used
	double *x_previous_runtimes_;				// cycle runtimes recorded at that mutcount (SLIM_MUTRUN_EXPERIMENT_MAXLENGTH length)
	int x_previous_buflen_;						// the number of runtimes in the previous_mutcount_runtimes_ buffer
	
	bool x_continuing_trend_;					// if true, the current experiment continues a trend, such that the opposite trend can be excluded
	
	int64_t x_stasis_limit_;					// how many stasis experiments we're running between change experiments; gets longer over time
	double x_stasis_alpha_;						// the alpha threshold at which we decide that stasis has been broken; gets smaller over time
	int64_t x_stasis_counter_;					// how many stasis experiments we have run so far
	int32_t x_prev1_stasis_mutcount_;			// the number of mutation runs we settled on when we reached stasis last time
	int32_t x_prev2_stasis_mutcount_;			// the number of mutation runs we settled on when we reached stasis the time before last
	
	std::vector<int32_t> x_mutcount_history_;	// a record of the mutation run count used in each cycle
	
	std::clock_t x_total_gen_clocks_ = 0;		// a counter of clocks accumulated for the current cycle's runtime (across measured code blocks)
												// look at MUTRUNEXP_START_TIMING() / MUTRUNEXP_END_TIMING() usage to see which blocks are measured
	
	// Shuffle buffer.  This is a shared buffer of sequential values that can be used by client code to shuffle the order in which
	// operations are performed.  The buffer always contains [0, 1, ..., N-1] shuffled into a new random order with each request
	// if randomized callbacks are enabled (the default in SLiM 4), or [0, 1, ..., N-1] in sequence if they are disabled.
	// Never access these ivars directly; always use BorrowShuffleBuffer() and ReturnShuffleBuffer().
	slim_popsize_t *shuffle_buffer_ = nullptr;
	slim_popsize_t shuffle_buf_capacity_ = 0;	// allocated capacity
	slim_popsize_t shuffle_buf_size_ = 0;		// the number of entries actually usable
	bool shuffle_buf_borrowed_ = false;			// a safeguard against re-entrancy
	bool shuffle_buf_is_enabled_ = true;		// if false, the buffer is "pass-through" - just sequential integers
	
	// TREE SEQUENCE RECORDING
#pragma mark -
#pragma mark treeseq recording ivars
#pragma mark -
	bool recording_tree_ = false;				// true if we are doing tree sequence recording
	bool recording_mutations_ = false;			// true if we are recording mutations in our tree sequence tables
	bool retain_coalescent_only_ = true;		// true if "retain" keeps only individuals for coalescent nodes, not also individuals for unary nodes
	
	bool tables_initialized_ = false;			// not checked everywhere, just when allocing and freeing, to avoid crashes
	tsk_table_collection_t tables_;
	tsk_bookmark_t table_position_;
	
    std::vector<tsk_id_t> remembered_genomes_;
	//Individual *current_new_individual_;
	
#if EIDOS_ROBIN_HOOD_HASHING
	typedef robin_hood::unordered_flat_map<slim_pedigreeid_t, tsk_id_t> INDIVIDUALS_HASH;
 #elif STD_UNORDERED_MAP_HASHING
	typedef std::unordered_map<slim_pedigreeid_t, tsk_id_t> INDIVIDUALS_HASH;
 #endif
	INDIVIDUALS_HASH tabled_individuals_hash_;	// look up individuals table row numbers from pedigree IDs

	bool running_coalescence_checks_ = false;	// true if we check for coalescence after each simplification
	bool last_coalescence_state_ = false;		// if running_coalescence_checks_==true, updated every simplification
	
	bool running_treeseq_crosschecks_ = false;	// true if crosschecks between our tree sequence tables and SLiM's data are enabled
	int treeseq_crosschecks_interval_ = 1;		// crosschecks, if enabled, will be done every treeseq_crosschecks_interval_ cycles
	
	double simplification_ratio_;				// the pre:post table size ratio we target with our automatic simplification heuristic
	int64_t simplification_interval_;			// the cycle interval between simplifications; -1 if not used (in which case the ratio is used)
	int64_t simplify_elapsed_ = 0;				// the number of cycles elapsed since a simplification was done (automatic or otherwise)
	double simplify_interval_;					// the current number of cycles between automatic simplifications when using simplification_ratio_
	
public:
	
	SLiMModelType model_type_;
	Community &community_;						// the community that this species belongs to
	Chromosome *chromosome_;					// the chromosome, which defines genomic elements
	Population population_;						// the population, which contains sub-populations

	std::string avatar_;						// a string used as the "avatar" for this species in SLiMgui, and perhaps elsewhere
	std::string name_;							// the `name` property; "sim" by default, configurable in script (not by setting the property)
	std::string description_;					// the `description` property; the empty string by default
	slim_objectid_t species_id_;				// the identifier for the species, which its index into the Community's species vector
	
	bool has_recalculated_fitness_ = false;		// set to true when recalculateFitness() is called, so we know fitness values are valid
	
	// optimization of the pure neutral case; this is set to false if (a) a non-neutral mutation is added by the user, (b) a genomic element type is configured to use a
	// non-neutral mutation type, (c) an already existing mutation type (assumed to be in use) is set to a non-neutral DFE, or (d) a mutation's selection coefficient is
	// changed to non-neutral.  The flag is never set back to true.  Importantly, simply defining a non-neutral mutation type does NOT clear this flag; we want sims to be
	// able to run a neutral burn-in at full speed, only slowing down when the non-neutral mutation type is actually used.  BCH 12 January 2018: Also, note that this flag
	// is unaffected by the fitness_scaling_ properties on Subpopulation and Individual, which are taken into account even when this flag is set.
	bool pure_neutral_ = true;														// optimization flag
	
	// this flag tracks whether a type 's' mutation type has ever been seen; we just set it to true if we see one, we never set it back to false again, for simplicity
	// this switches to a less optimized case when evolving in WF models, if a type 's' DFE could be present, since that can open up various cans of worms
	bool type_s_dfes_present_ = false;												// optimization flag
	
	// this counter is incremented when a selection coefficient is changed on any mutation object in the simulation.  This is used as a signal to mutation runs that their
	// cache of non-neutral mutations is invalid (because their counter is not equal to this counter).  The caches will be re-validated the next time they are used.  Other
	// code can also increment this counter in order to trigger a re-validation of all non-neutral mutation caches; it is a general-purpose mechanism.
	int32_t nonneutral_change_counter_ = 0;
	int32_t last_nonneutral_regime_ = 0;		// see mutation_run.h; 1 = no mutationEffect() callbacks, 2 = only constant-effect neutral callbacks, 3 = arbitrary callbacks
	
	// this flag is set if the dominance coeff (regular or haploid) changes on any mutation type, as a signal that recaching needs to occur in Subpopulation::UpdateFitness()
	bool any_dominance_coeff_changed_ = false;
	
	// state about what symbols/names/identifiers have been used or are being used
	std::unordered_set<slim_objectid_t> subpop_ids_;								// all subpop IDs ever used, even if no longer in use
	std::unordered_set<std::string> subpop_names_;									// all subpop names ever used, except for subpop ID names ("p1", "p2", etc.)
	
#if (SLIMPROFILING == 1)
	// PROFILING : Species keeps track of its memory usage profile info and mutation-related profile info
	SLiMMemoryUsage_Species profile_last_memory_usage_Species;
	SLiMMemoryUsage_Species profile_total_memory_usage_Species;
	
#if SLIM_USE_NONNEUTRAL_CACHES
	std::vector<int32_t> profile_mutcount_history_;									// a record of the mutation run count used in each cycle
	std::vector<int32_t> profile_nonneutral_regime_history_;						// a record of the nonneutral regime used in each cycle
	int64_t profile_mutation_total_usage_;											// how many (non-unique) mutations were used by mutation runs, summed across cycles
	int64_t profile_nonneutral_mutation_total_;										// of profile_mutation_total_usage_, how many were deemed to be nonneutral
	int64_t profile_mutrun_total_usage_;											// how many (non-unique) mutruns were used by genomes, summed across cycles
	int64_t profile_unique_mutrun_total_;											// of profile_mutrun_total_usage_, how many unique mutruns existed, summed across cycles
	int64_t profile_mutrun_nonneutral_recache_total_;								// of profile_unique_mutrun_total_, how many mutruns regenerated their nonneutral cache
	int64_t profile_max_mutation_index_;											// the largest mutation index seen over the course of the profile
#endif	// SLIM_USE_NONNEUTRAL_CACHES
#endif	// (SLIMPROFILING == 1)
	
	Species(const Species&) = delete;																	// no copying
	Species& operator=(const Species&) = delete;														// no copying
	Species(Community &p_community, slim_objectid_t p_species_id, const std::string &p_name);			// construct a Species from a community / id / name
	~Species(void);																						// destructor
	
	void TabulateSLiMMemoryUsage_Species(SLiMMemoryUsage_Species *p_usage);			// used by outputUsage() and SLiMgui profiling
	void DeleteAllMutationRuns(void);												// for cleanup
	
	// Running cycles
	std::vector<SLiMEidosBlock*> CallbackBlocksMatching(slim_tick_t p_tick, SLiMEidosBlockType p_event_type, slim_objectid_t p_mutation_type_id, slim_objectid_t p_interaction_type_id, slim_objectid_t p_subpopulation_id);
	void RunInitializeCallbacks(void);
	bool HasDoneAnyInitialization(void);
	void SetUpMutationRunContexts(void);
	void PrepareForCycle(void);
	void MaintainMutationRegistry(void);
	void RecalculateFitness(void);
	void MaintainTreeSequence(void);
	void EmptyGraveyard(void);
	void FinishMutationRunExperimentTiming(void);
	
	void WF_GenerateOffspring(void);
	void WF_SwitchToChildGeneration(void);
	void WF_SwapGenerations(void);
	
	void nonWF_GenerateOffspring(void);
	void nonWF_MergeOffspring(void);
	void nonWF_ViabilitySurvival(void);
	
	void AdvanceCycleCounter(void);
	void SimulationHasFinished(void);
	
	// Shared shuffle buffer to save
	inline bool RandomizingCallbackOrder(void) { return shuffle_buf_is_enabled_; }
	slim_popsize_t *BorrowShuffleBuffer(slim_popsize_t p_buffer_size);
	void ReturnShuffleBuffer(void);
	
#if (SLIMPROFILING == 1)
	// PROFILING
#if SLIM_USE_NONNEUTRAL_CACHES
	void CollectMutationProfileInfo(void);
#endif
#endif
	
	// Mutation run experiments
	void InitiateMutationRunExperiments(void);
	void TransitionToNewExperimentAgainstCurrentExperiment(int32_t p_new_mutrun_count);
	void TransitionToNewExperimentAgainstPreviousExperiment(int32_t p_new_mutrun_count);
	void EnterStasisForMutationRunExperiments(void);
	void MaintainMutationRunExperiments(double p_last_gen_runtime);
	
	// Mutation stack policy checking
	inline __attribute__((always_inline)) void MutationStackPolicyChanged(void)												{ mutation_stack_policy_changed_ = true; }
	inline __attribute__((always_inline)) void CheckMutationStackPolicy(void)												{ if (mutation_stack_policy_changed_) _CheckMutationStackPolicy(); }
	void _CheckMutationStackPolicy(void);
	
	// Nucleotide-based models
	void CacheNucleotideMatrices(void);
	void CreateNucleotideMutationRateMap(void);
	
	// accessors
	inline __attribute__((always_inline)) slim_tick_t Cycle(void) const														{ return cycle_; }
	void SetCycle(slim_tick_t p_new_cycle);
	
	inline __attribute__((always_inline)) bool Active(void) { return species_active_; }
	inline __attribute__((always_inline)) void SetActive(bool p_active) { species_active_ = p_active; }
	inline __attribute__((always_inline)) slim_tick_t TickModulo(void) { return tick_modulo_; }
	inline __attribute__((always_inline)) slim_tick_t TickPhase(void) { return tick_phase_; }
	
	inline __attribute__((always_inline)) Chromosome &TheChromosome(void)													{ return *chromosome_; }
	inline __attribute__((always_inline)) bool HasGenetics(void)															{ return has_genetics_; }
	inline __attribute__((always_inline)) const std::map<slim_objectid_t,MutationType*> &MutationTypes(void) const			{ return mutation_types_; }
	inline __attribute__((always_inline)) const std::map<slim_objectid_t,GenomicElementType*> &GenomicElementTypes(void)	{ return genomic_element_types_; }
	inline __attribute__((always_inline)) size_t GraveyardSize(void) const													{ return graveyard_.size(); }
	
#ifndef _OPENMP
	inline int SpeciesMutationRunContextCount(void) { return 1; }
	inline __attribute__((always_inline)) MutationRunContext &SpeciesMutationRunContextForThread(__attribute__((unused)) int p_thread_num)
	{
#if DEBUG
		if (p_thread_num != 0)
			EIDOS_TERMINATION << "ERROR (Species::SpeciesMutationRunContextForThread): (internal error) p_thread_num out of range." << EidosTerminate();
#endif
		return mutation_run_context_SINGLE_;
	}
	inline __attribute__((always_inline)) MutationRunContext &SpeciesMutationRunContextForMutationRunIndex(__attribute__((unused)) slim_mutrun_index_t p_mutrun_index)
	{
#if DEBUG
		if ((p_mutrun_index < 0) || (p_mutrun_index >= chromosome_->mutrun_count_))
			EIDOS_TERMINATION << "ERROR (Species::SpeciesMutationRunContextForMutationRunIndex): (internal error) p_mutrun_index out of range." << EidosTerminate();
#endif
		return mutation_run_context_SINGLE_;
	}
#else
	inline int SpeciesMutationRunContextCount(void) { return mutation_run_context_COUNT_; }
	inline __attribute__((always_inline)) MutationRunContext &SpeciesMutationRunContextForThread(int p_thread_num)
	{
#if DEBUG
		if ((p_thread_num < 0) || (p_thread_num >= mutation_run_context_COUNT_))
			EIDOS_TERMINATION << "ERROR (Species::SpeciesMutationRunContextForThread): (internal error) p_thread_num out of range." << EidosTerminate();
#endif
		return *(mutation_run_context_PERTHREAD[p_thread_num]);
	}
	inline __attribute__((always_inline)) MutationRunContext &SpeciesMutationRunContextForMutationRunIndex(__attribute__((unused)) slim_mutrun_index_t p_mutrun_index)
	{
#if DEBUG
		if ((p_mutrun_index < 0) || (p_mutrun_index >= chromosome_->mutrun_count_))
			EIDOS_TERMINATION << "ERROR (Species::SpeciesMutationRunContextForMutationRunIndex): (internal error) p_mutrun_index out of range." << EidosTerminate();
#endif
		// The range of the genome that each thread is responsible for does not change across
		// splits/joins; one mutrun becomes two, or two become one, owned by the same thread
		int thread_num = (int)(p_mutrun_index / chromosome_->mutrun_count_multiplier_);
		return *(mutation_run_context_PERTHREAD[thread_num]);
	}
#endif
	
	inline Subpopulation *SubpopulationWithID(slim_objectid_t p_subpop_id) {
		auto id_iter = population_.subpops_.find(p_subpop_id);
		return (id_iter == population_.subpops_.end()) ? nullptr : id_iter->second;
	}
	inline MutationType *MutationTypeWithID(slim_objectid_t p_muttype_id) {
		auto id_iter = mutation_types_.find(p_muttype_id);
		return (id_iter == mutation_types_.end()) ? nullptr : id_iter->second;
	}
#ifdef SLIMGUI
	inline MutationType *MutationTypeWithIndex(int p_muttype_index) {
		for (const std::pair<const slim_objectid_t,MutationType*> &muttype_pair : mutation_types_)
			if (muttype_pair.second->mutation_type_index_ == p_muttype_index)
				return muttype_pair.second;
		return nullptr;
	}
#endif
	inline GenomicElementType *GenomicElementTypeWithID(slim_objectid_t p_getype_id) {
		auto id_iter = genomic_element_types_.find(p_getype_id);
		return (id_iter == genomic_element_types_.end()) ? nullptr : id_iter->second;
	}
	
	inline __attribute__((always_inline)) bool SexEnabled(void) const														{ return sex_enabled_; }
	inline __attribute__((always_inline)) bool PedigreesEnabled(void) const													{ return pedigrees_enabled_; }
	inline __attribute__((always_inline)) bool PedigreesEnabledByUser(void) const											{ return pedigrees_enabled_by_user_; }
	inline __attribute__((always_inline)) bool PreventIncidentalSelfing(void) const											{ return prevent_incidental_selfing_; }
	inline __attribute__((always_inline)) GenomeType ModeledChromosomeType(void) const										{ return modeled_chromosome_type_; }
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
	void AboutToSplitSubpop(void);	// see Population::AddSubpopulationSplit()
	
	static void handle_error(const std::string &msg, int error);
	static void MetadataForMutation(Mutation *p_mutation, MutationMetadataRec *p_metadata);
	static void MetadataForSubstitution(Substitution *p_substitution, MutationMetadataRec *p_metadata);
	static void MetadataForGenome(Genome *p_genome, GenomeMetadataRec *p_metadata);
	static void MetadataForIndividual(Individual *p_individual, IndividualMetadataRec *p_metadata);
	static void TreeSequenceDataToAscii(tsk_table_collection_t *p_tables);
	static void DerivedStatesFromAscii(tsk_table_collection_t *p_tables);
	static void DerivedStatesToAscii(tsk_table_collection_t *p_tables);
	
	bool _SubpopulationIDInUse(slim_objectid_t p_subpop_id);
	void RecordTablePosition(void);
	void AllocateTreeSequenceTables(void);
	void SetCurrentNewIndividual(Individual *p_individual);
	void RecordNewGenome(std::vector<slim_position_t> *p_breakpoints, Genome *p_new_genome, const Genome *p_initial_parental_genome, const Genome *p_second_parental_genome);
	void RecordNewDerivedState(const Genome *p_genome, slim_position_t p_position, const std::vector<Mutation *> &p_derived_mutations);
	void RetractNewIndividual(void);
	void AddIndividualsToTable(Individual * const *p_individual, size_t p_num_individuals, tsk_table_collection_t *p_tables, INDIVIDUALS_HASH *p_individuals_hash, tsk_flags_t p_flags);
	void AddLiveIndividualsToIndividualsTable(tsk_table_collection_t *p_tables, INDIVIDUALS_HASH *p_individuals_hash);
	void FixAliveIndividuals(tsk_table_collection_t *p_tables);
	void WritePopulationTable(tsk_table_collection_t *p_tables);
	void WriteProvenanceTable(tsk_table_collection_t *p_tables, bool p_use_newlines, bool p_include_model);
	void WriteTreeSequenceMetadata(tsk_table_collection_t *p_tables, EidosDictionaryUnretained *p_metadata_dict);
	void ReadTreeSequenceMetadata(tsk_table_collection_t *p_tables, slim_tick_t *p_tick, slim_tick_t *p_cycle, SLiMModelType *p_model_type, int *p_file_version);
	void WriteTreeSequence(std::string &p_recording_tree_path, bool p_binary, bool p_simplify, bool p_include_model, EidosDictionaryUnretained *p_metadata_dict);
    void ReorderIndividualTable(tsk_table_collection_t *p_tables, std::vector<int> p_individual_map, bool p_keep_unmapped);
	void AddParentsColumnForOutput(tsk_table_collection_t *p_tables, INDIVIDUALS_HASH *p_individuals_hash);
	void BuildTabledIndividualsHash(tsk_table_collection_t *p_tables, INDIVIDUALS_HASH *p_individuals_hash);
	void SimplifyTreeSequence(void);
	void CheckCoalescenceAfterSimplification(void);
	void CheckAutoSimplification(void);
    void TreeSequenceDataFromAscii(const std::string &NodeFileName, const std::string &EdgeFileName, const std::string &SiteFileName, const std::string &MutationFileName, const std::string &IndividualsFileName, const std::string &PopulationFileName, const std::string &ProvenanceFileName);
	void FreeTreeSequence();
	void RecordAllDerivedStatesFromSLiM(void);
	void DumpMutationTable(void);
	void CheckTreeSeqIntegrity(void);		// checks the tree sequence tables themselves
	void CrosscheckTreeSeqIntegrity(void);	// checks the tree sequence tables against SLiM's data structures
	
	void __RewriteOldIndividualsMetadata(int p_file_version);
	void __RewriteOrCheckPopulationMetadata(void);
	void __RemapSubpopulationIDs(SUBPOP_REMAP_HASH &p_subpop_map, int p_file_version);
	void __PrepareSubpopulationsFromTables(std::unordered_map<slim_objectid_t, ts_subpop_info> &p_subpopInfoMap);
	void __TabulateSubpopulationsFromTreeSequence(std::unordered_map<slim_objectid_t, ts_subpop_info> &p_subpopInfoMap, tsk_treeseq_t *p_ts, SLiMModelType p_file_model_type);
	void __CreateSubpopulationsFromTabulation(std::unordered_map<slim_objectid_t, ts_subpop_info> &p_subpopInfoMap, EidosInterpreter *p_interpreter, std::unordered_map<tsk_id_t, Genome *> &p_nodeToGenomeMap);
	void __ConfigureSubpopulationsFromTables(EidosInterpreter *p_interpreter);
	void __TabulateMutationsFromTables(std::unordered_map<slim_mutationid_t, ts_mut_info> &p_mutMap, int p_file_version);
	void __TallyMutationReferencesWithTreeSequence(std::unordered_map<slim_mutationid_t, ts_mut_info> &p_mutMap, std::unordered_map<tsk_id_t, Genome *> p_nodeToGenomeMap, tsk_treeseq_t *p_ts);
	void __CreateMutationsFromTabulation(std::unordered_map<slim_mutationid_t, ts_mut_info> &p_mutInfoMap, std::unordered_map<slim_mutationid_t, MutationIndex> &p_mutIndexMap);
	void __AddMutationsFromTreeSequenceToGenomes(std::unordered_map<slim_mutationid_t, MutationIndex> &p_mutIndexMap, std::unordered_map<tsk_id_t, Genome *> p_nodeToGenomeMap, tsk_treeseq_t *p_ts);
	void _InstantiateSLiMObjectsFromTables(EidosInterpreter *p_interpreter, slim_tick_t p_metadata_tick, slim_tick_t p_metadata_cycle, SLiMModelType p_file_model_type, int p_file_version, SUBPOP_REMAP_HASH &p_subpop_map);	// given tree-seq tables, makes individuals, genomes, and mutations
	slim_tick_t _InitializePopulationFromTskitTextFile(const char *p_file, EidosInterpreter *p_interpreter, SUBPOP_REMAP_HASH &p_subpop_map);	// initialize the population from an tskit text file
	slim_tick_t _InitializePopulationFromTskitBinaryFile(const char *p_file, EidosInterpreter *p_interpreter, SUBPOP_REMAP_HASH &p_subpop_remap);	// initialize the population from an tskit binary file
	
	size_t MemoryUsageForTables(tsk_table_collection_t &p_tables);
	void TSXC_Enable(void);
	void TSF_Enable(void);
	
	//
	// Eidos support
	//
#pragma mark -
#pragma mark Eidos support
#pragma mark -
	inline EidosSymbolTableEntry &SymbolTableEntry(void) { return self_symbol_; };
	
	EidosValue_SP ExecuteContextFunction_initializeAncestralNucleotides(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteContextFunction_initializeGenomicElement(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteContextFunction_initializeGenomicElementType(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteContextFunction_initializeMutationType(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteContextFunction_initializeRecombinationRate(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteContextFunction_initializeGeneConversion(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteContextFunction_initializeMutationRate(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteContextFunction_initializeHotspotMap(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteContextFunction_initializeSex(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteContextFunction_initializeSLiMOptions(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteContextFunction_initializeSpecies(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteContextFunction_initializeTreeSeq(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	
	virtual const EidosClass *Class(void) const override;
	virtual void Print(std::ostream &p_ostream) const override;
	
	virtual EidosValue_SP GetProperty(EidosGlobalStringID p_property_id) override;
	virtual void SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value) override;
	
	virtual EidosValue_SP ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) override;
	
	EidosValue_SP ExecuteMethod_addSubpop(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_addSubpopSplit(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_individualsWithPedigreeIDs(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_killIndividuals(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_mutationFreqsCounts(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_mutationsOfType(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_countOfMutationsOfType(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_outputFixedMutations(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_outputFull(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_outputMutations(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_readFromPopulationFile(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_recalculateFitness(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_registerFitnessEffectCallback(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_registerMateModifyRecSurvCallback(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_registerMutationCallback(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_registerMutationEffectCallback(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_registerReproductionCallback(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_simulationFinished(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_skipTick(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_subsetMutations(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_treeSeqCoalesced(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_treeSeqSimplify(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_treeSeqRememberIndividuals(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_treeSeqOutput(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
};

class Species_Class : public EidosDictionaryUnretained_Class
{
private:
	typedef EidosDictionaryUnretained_Class super;

public:
	Species_Class(const Species_Class &p_original) = delete;	// no copy-construct
	Species_Class& operator=(const Species_Class&) = delete;	// no copying
	inline Species_Class(const std::string &p_class_name, EidosClass *p_superclass) : super(p_class_name, p_superclass) { }
	
	virtual const std::vector<EidosPropertySignature_CSP> *Properties(void) const override;
	virtual const std::vector<EidosMethodSignature_CSP> *Methods(void) const override;
};


#endif /* defined(__SLiM__species__) */












































































