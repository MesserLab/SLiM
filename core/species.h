//
//  species.h
//  SLiM
//
//  Created by Ben Haller on 12/26/14.
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
#include "trait.h"
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
class MutationBlock;
class MutationType;
class GenomicElementType;
class InteractionType;
struct ts_subpop_info;
struct ts_mut_info;


class Species_Class;
extern Species_Class *gSLiM_Species_Class;


enum class SLiMFileFormat
{
	kFileNotFound = -1,
	kFormatUnrecognized = 0,
	kFormatSLiMText,				// as saved by outputFull(filePath, binary=F)
	kFormatSLiMBinary,				// as saved by outputFull(filePath, binary=T)
	kFormatTskitBinary_HDF5,		// old file format, no longer supported
	kFormatTskitBinary_kastore,		// as saved by treeSeqOutput(path)
	kFormatDirectory,				// a directory, presumed to contain .trees files for multiple chromosomes
};


// We have a defined maximum number of chromosomes that we resize to immediately, so the chromosome vector never reallocs
// There would be an upper limit of 256 anyway because Mutation uses uint8_t to keep the index of its chromosome
#define SLIM_MAX_CHROMOSOMES	256

// We have a defined maximum number of traits; it is not clear that this is necessary, however.  FIXME MULTITRAIT
#define SLIM_MAX_TRAITS	256


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
	slim_effect_t selection_coeff_;			// 4 bytes (float): the mutation effect (e.g., selection coefficient)
	// FIXME MULTITRAIT need to add a dominance_coeff_ property here!
	slim_objectid_t subpop_index_;			// 4 bytes (int32_t): the id of the subpopulation in which the mutation arose
	slim_tick_t origin_tick_;				// 4 bytes (int32_t): the tick in which the mutation arose
	int8_t nucleotide_;						// 1 byte (int8_t): the nucleotide for the mutation (0='A', 1='C', 2='G', 3='T'), or -1
} MutationMetadataRec;

typedef struct __attribute__((__packed__)) {
	// BCH 12/10/2024: This metadata record is becoming a bit complicated, for multichromosome SLiM, and is now actually variable-length.
	// The difficulty is that this metadata gets attached to nodes in the tree sequence, and in multichrom models the node table is
	// shared by all of the chromosome-specific tree sequences.  That implies that the haplosome metadata has to be the *same* for all
	// of the haplosomes that reference that node -- all the first haplosomes of an individual, or all the second haplosomes of an
	// individual.  We want to keep is_vacant_ state separately for each haplosome; within one individual, some haplosomes might be nulls,
	// other might not be, and we need to know the difference to correctly read/analyze a tree sequence.  To achieve this, each node's
	// metadata -- HaplosomeMetadataRec -- will record a *vector* of is_vacant_ bytes, each containing 8 bits, recording the is_vacant_
	// state for each of the haplosome slots represented by the node in its owning individual.  Note that haplosome slots for a given
	// node can actually have three states in an individual: "real", "null", or "unused".  "Real" would be the first haplosome for the
	// Y in a male; "null" would be the first haplosome for the Y in a female (a placeholder for the Y that could be there but is not);
	// and "unused" would be the *second* haplosome for the Y in either sex (because the Y is a haploid chromosome, and haplosomes for
	// the second position therefore do not exist -- but a node for that slot still exists, because we *always* make two nodes in the
	// tree sequence for each chromosome, to maintain the 1:2 individual_id:node_id invariant that we assume throughout the code).  The
	// flags in is_vacant_ differentiate between "real" and "unused"/"null"; the value for "unused" positions should indicate "vacant",
	// just as for "null" positions.  See Species::_MakeHaplosomeMetadataRecords() and elsewhere.
	//
	slim_haplosomeid_t haplosome_id_;		// 8 bytes (int64_t): the SLiM haplosome ID for this haplosome, assigned by pedigree rec
											// 		note that the ID is the same across all chromosomes in an individual!
	uint8_t is_vacant_[1];					// 1 byte (8 bits, handled bitwise) -- but this field is actually variable-length, see above
	// BCH 12/6/2024: type_, the chromosome type for the haplosome, has moved to top-level metadata; it is constant across a tree sequence
} HaplosomeMetadataRec;

typedef struct __attribute__((__packed__)) {
	slim_pedigreeid_t pedigree_id_;			// 8 bytes (int64_t): the SLiM pedigree ID for this individual, assigned by pedigree rec
	slim_pedigreeid_t pedigree_p1_;			// 8 bytes (int64_t): the SLiM pedigree ID for this individual's parent 1
	slim_pedigreeid_t pedigree_p2_;			// 8 bytes (int64_t): the SLiM pedigree ID for this individual's parent 2
	slim_age_t age_;                        // 4 bytes (int32_t): the age of the individual (-1 for WF models)
	slim_objectid_t subpopulation_id_;      // 4 bytes (int32_t): the subpopulation the individual belongs to
	int32_t sex_;							// 4 bytes (int32_t): the sex of the individual, as defined by the IndividualSex enum
	uint32_t flags_;						// 4 bytes (uint32_t): assorted flags, see below
} IndividualMetadataRec;

#define SLIM_INDIVIDUAL_METADATA_MIGRATED	0x01	// set if the individual has migrated in this cycle

// We double-check the size of these records to make sure we understand what they contain and how they're packed
static_assert(sizeof(MutationMetadataRec) == 17, "MutationMetadataRec is not 17 bytes!");
static_assert(sizeof(HaplosomeMetadataRec) == 9, "HaplosomeMetadataRec is not 9 bytes!");	// but its size is dynamic at runtime
static_assert(sizeof(IndividualMetadataRec) == 40, "IndividualMetadataRec is not 40 bytes!");

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
	
	// We keep a MutationBlock object that stores all of the Mutation objects that belong to this species.
	// Our mutations get allocated and freed using this block, and we use MutationIndex to reference them.
	// This remains nullptr in no-genetics species, and is allocated only after initialize() is done.
	MutationBlock *mutation_block_ = nullptr;			// OWNED; contains all of our mutations
	
	// for multiple chromosomes, we now have a vector of pointers to Chromosome objects,
	// as well as hash tables for quick lookup by id and symbol
#if EIDOS_ROBIN_HOOD_HASHING
	typedef robin_hood::unordered_flat_map<int64_t, Chromosome *> CHROMOSOME_ID_HASH;
	typedef robin_hood::unordered_flat_map<std::string, Chromosome *> CHROMOSOME_SYMBOL_HASH;
#elif STD_UNORDERED_MAP_HASHING
	typedef std::unordered_map<int64_t, Chromosome *> CHROMOSOME_ID_HASH;
	typedef std::unordered_map<std::string, Chromosome *> CHROMOSOME_SYMBOL_HASH;
#endif
	
	// Chromosome state
	std::vector<Chromosome *> chromosomes_;				// OWNED (retained); all our chromosomes, in the order in which they were defined
	CHROMOSOME_ID_HASH chromosome_from_id_;				// NOT OWNED; get a chromosome from a chromosome id quickly
	CHROMOSOME_SYMBOL_HASH chromosome_from_symbol_;		// NOT OWNED; get a chromosome from a chromosome symbol quickly
	
	std::vector<Chromosome *> chromosome_for_haplosome_index_;	// NOT OWNED; of length haplosome_count_per_individual_
	std::vector<uint8_t> chromosome_subindex_for_haplosome_index_;	// 0 or 1, the first or second haplosome for the chromosome
	std::vector<int> first_haplosome_index_;			// the first index in haplosomes_ for a given chromosome (synced to chromosomes_)
	std::vector<int> last_haplosome_index_;				// the last index in haplosomes_ for a given chromosome (synced to chromosomes_)
	int haplosome_count_per_individual_ = 0;			// the total number of haplosomes per individual, based on the chromosome types
	bool chromosomes_use_null_haplosomes_ = false;		// set to true if our chromosome types use null haplosomes; check this with CouldContainNullHaplosomes()
	
	// std::map is used instead of std::unordered_map mostly for convenience, for sorted order in the UI; these are unlikely to be bottlenecks I think
	std::map<slim_objectid_t,MutationType*> mutation_types_;						// OWNED POINTERS: this map is the owner of all allocated MutationType objects
	std::map<slim_objectid_t,GenomicElementType*> genomic_element_types_;			// OWNED POINTERS: this map is the owner of all allocated GenomicElementType objects
	
	// for multiple traits, we now have a vector of pointers to Trait objects, as well as hash tables for quick
	// lookup by name and by string ID; the latter is to make using trait names as properties on Individual fast
#if EIDOS_ROBIN_HOOD_HASHING
	typedef robin_hood::unordered_flat_map<std::string, Trait *> TRAIT_NAME_HASH;
	typedef robin_hood::unordered_flat_map<EidosGlobalStringID, Trait *> TRAIT_STRID_HASH;
#elif STD_UNORDERED_MAP_HASHING
	typedef std::unordered_map<std::string, Trait *> TRAIT_NAME_HASH;
	typedef std::unordered_map<EidosGlobalStringID, Trait *> TRAIT_STRID_HASH;
#endif
	
	// Trait state
	std::vector<Trait *> traits_;						// OWNED (retained); all our traits, in the order in which they were defined
	TRAIT_NAME_HASH trait_from_name;					// NOT OWNED; get a trait from a trait name quickly
	TRAIT_STRID_HASH trait_from_string_id;				// NOT OWNED; get a trait from a string ID quickly
	
	bool mutation_stack_policy_changed_ = true;										// when set, the stacking policy settings need to be checked for consistency
	
	// SEX ONLY: sex-related instance variables
	bool sex_enabled_ = false;														// true if sex is tracked for individuals; if false, all individuals are hermaphroditic
	
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
	
	// preventing incidental selfing in hermaphroditic models
	bool prevent_incidental_selfing_ = false;
	
	// mutation run timing experiment configuration
	bool do_mutrun_experiments_ = true;				// user-level flag in initializeSLiMOptions(); if false, experiments are never run
	bool doing_any_mutrun_experiments_ = false;		// is any chromosome actually running mutation run timing experiments?
	
	// nucleotide-based models
	bool nucleotide_based_ = false;
	
	double max_nucleotide_mut_rate_;				// the highest rate for any genetic background in any genomic element type
	void CacheNucleotideMatrices(void);
	
	EidosSymbolTableEntry self_symbol_;												// for fast setup of the symbol table
	
	slim_usertag_t tag_value_ = SLIM_TAG_UNSET_VALUE;								// a user-defined tag value
	
	// Shuffle buffer.  This is a shared buffer of sequential values that can be used by client code to shuffle the order in which
	// operations are performed.  The buffer always contains [0, 1, ..., N-1] shuffled into a new random order with each request
	// if randomized callbacks are enabled (the default in SLiM 4), or [0, 1, ..., N-1] in sequence if they are disabled.
	// Never access these ivars directly; always use BorrowShuffleBuffer() and ReturnShuffleBuffer().
	slim_popsize_t *shuffle_buffer_ = nullptr;
	slim_popsize_t shuffle_buf_capacity_ = 0;	// allocated capacity
	slim_popsize_t shuffle_buf_size_ = 0;		// the number of entries actually usable
	bool shuffle_buf_borrowed_ = false;			// a safeguard against re-entrancy
	bool shuffle_buf_is_enabled_ = true;		// if false, the buffer is "pass-through" - just sequential integers
	
	//
	// Initialization completeness check counts; should be used only when running initialize() callbacks
	//
	
	// per-species initialization; zeroed by Species::RunInitializeCallbacks()
	int num_species_inits_;				// number of calls to initializeSpecies()
	int num_slimoptions_inits_;			// number of calls to initializeSLiMOptions()
	int num_mutation_type_inits_;		// number of calls to initializeMutationType() and initializeMutationTypeNuc()
	int num_ge_type_inits_;				// number of calls to initializeGenomicElementType()
	int num_sex_inits_;					// SEX ONLY: number of calls to initializeSex()
	int num_treeseq_inits_;				// number of calls to initializeTreeSeq()
	int num_trait_inits_;				// number of calls to initializeTrait()
	bool has_implicit_trait_;			// true if the model implicitly defines a trait, with no initializeTrait() call
	int num_chromosome_inits_;			// number of calls to initializeChromosome()
	bool has_implicit_chromosome_;		// true if the model implicitly defines a chromosome, with no initializeChromosome() call
	bool has_currently_initializing_chromosome_ = false;
	
	// per-chromosome initialization; zeroed by initializeChromosome()
	int num_mutrate_inits_;				// number of calls to initializeMutationRate()
	int num_recrate_inits_;				// number of calls to initializeRecombinationRate()
	int num_genomic_element_inits_;		// number of calls to initializeGenomicElement()
	int num_gene_conv_inits_;			// number of calls to initializeGeneConversion()
	int num_ancseq_inits_;				// number of calls to initializeAncestralNucleotides()
	int num_hotmap_inits_;				// number of calls to initializeHotspotMap()
	
	slim_position_t last_genomic_element_position_ = -1;	// used to check new genomic elements for consistency
	
	//
	// TREE SEQUENCE RECORDING
	//
#pragma mark -
#pragma mark treeseq recording ivars
#pragma mark -
	// ********** first we have tree-seq state that is shared across all chromosomes
	//
	bool recording_tree_ = false;				// true if we are doing tree sequence recording
	bool recording_mutations_ = false;			// true if we are recording mutations in our tree sequence tables
	bool retain_coalescent_only_ = true;		// true if "retain" keeps only individuals for coalescent nodes, not also individuals for unary nodes
	
	bool tables_initialized_ = false;			// not checked everywhere, just when allocing and freeing, to avoid crashes
	
	std::vector<tsk_id_t> remembered_nodes_;	// used to be called remembered_genomes_, but it remembers tskit nodes, which might
												// actually be shared by multiple haplosomes in different chromosomes
	//Individual *current_new_individual_;
	
#if EIDOS_ROBIN_HOOD_HASHING
	typedef robin_hood::unordered_flat_map<slim_pedigreeid_t, tsk_id_t> INDIVIDUALS_HASH;
#elif STD_UNORDERED_MAP_HASHING
	typedef std::unordered_map<slim_pedigreeid_t, tsk_id_t> INDIVIDUALS_HASH;
#endif
	INDIVIDUALS_HASH tabled_individuals_hash_;	// look up individuals table row numbers from pedigree IDs
	
	bool running_coalescence_checks_ = false;	// true if we check for coalescence after each simplification
	bool running_treeseq_crosschecks_ = false;	// true if crosschecks between our tree sequence tables and SLiM's data are enabled
	int treeseq_crosschecks_interval_ = 1;		// crosschecks, if enabled, will be done every treeseq_crosschecks_interval_ cycles
	
	double simplification_ratio_;				// the pre:post table size ratio we target with our automatic simplification heuristic
	int64_t simplification_interval_;			// the cycle interval between simplifications; -1 if not used (in which case the ratio is used)
	int64_t simplify_elapsed_ = 0;				// the number of cycles elapsed since a simplification was done (automatic or otherwise)
	double simplify_interval_;					// the current number of cycles between automatic simplifications when using simplification_ratio_
	
	size_t haplosome_metadata_size_;			// the number of bytes for haplosome metadata, for this species, including is_vacant_ flags
	size_t haplosome_metadata_is_vacant_bytes_;	// the number of bytes used for is_vacant_ in the haplosome metadata
	HaplosomeMetadataRec *hap_metadata_1F_ = nullptr;		// malloced; default is_vacant_ flags for first haplosomes in females/hermaphrodites
	HaplosomeMetadataRec *hap_metadata_1M_ = nullptr;		// malloced; default is_vacant_ flags for first haplosomes in males
	HaplosomeMetadataRec *hap_metadata_2F_ = nullptr;		// malloced; default is_vacant_ flags for second haplosomes in females/hermaphrodites
	HaplosomeMetadataRec *hap_metadata_2M_ = nullptr;		// malloced; default is_vacant_ flags for second haplosomes in males
	void _MakeHaplosomeMetadataRecords(void);
	
	// ********** then we have tree-seq state that is kept separately for each chromosome; each has its own tree sequence
	//
	typedef struct _TreeSeqInfo {
		slim_chromosome_index_t chromosome_index_;	// this should range from 0 to N-1, following the corresponding chromosome indices
		tsk_table_collection_t tables_;				// the table collection; the node, individual, and popultation tables are shared
		tsk_bookmark_t table_position_;				// a bookmarked position in tables_ for retraction of a proposed child
		bool last_coalescence_state_;				// have we coalesced? updated after simplify if running_coalescence_checks_==true
	} TreeSeqInfo;
	
	std::vector<TreeSeqInfo> treeseq_;				// OWNED; all our tree-sequence state, in the order the chromosomes were defined
													// index 0's table collection contains the shared tables; see CopySharedTablesIn()
	
public:
	
	// Object pools for individuals and haplosomes, kept population-wide; these must be above their clients in the declaration order
	// BCH 28 Jan. 2025: These are now kept by the Species, not the Population, so that they can be destructed after all clients have destructed
	EidosObjectPool species_haplosome_pool_;	// a pool out of which haplosomes are allocated, for within-species locality
	EidosObjectPool species_individual_pool_;	// a pool out of which individuals are allocated, for within-species locality
	
	SLiMModelType model_type_;
	Community &community_;						// the community that this species belongs to
	Population population_;						// the population, which contains sub-populations

	std::string avatar_;						// a string used as the "avatar" for this species in SLiMgui, and perhaps elsewhere
	std::string name_;							// the `name` property; "sim" by default, configurable in script (not by setting the property)
	std::string description_;					// the `description` property; the empty string by default
	slim_objectid_t species_id_;				// the identifier for the species, which its index into the Community's species vector
	
	bool has_recalculated_fitness_ = false;		// set to true when recalculateFitness() is called, so we know fitness values are valid
	
	// optimization of the pure neutral case; this is set to false if (a) a non-neutral mutation is added by the user, (b) a genomic element type is configured to use a
	// non-neutral mutation type, (c) an already existing mutation type (assumed to be in use) is set to a non-neutral DES, or (d) a mutation's selection coefficient is
	// changed to non-neutral.  The flag is never set back to true.  Importantly, simply defining a non-neutral mutation type does NOT clear this flag; we want sims to be
	// able to run a neutral burn-in at full speed, only slowing down when the non-neutral mutation type is actually used.  BCH 12 January 2018: Also, note that this flag
	// is unaffected by the fitness_scaling_ properties on Subpopulation and Individual, which are taken into account even when this flag is set.
	bool pure_neutral_ = true;														// optimization flag
	
	// this flag tracks whether a type 's' mutation type has ever been seen; we just set it to true if we see one, we never set it back to false again, for simplicity
	// this switches to a less optimized case when evolving in WF models, if a type 's' DES could be present, since that can open up various cans of worms
	bool type_s_DESs_present_ = false;												// optimization flag
	
	// this counter is incremented when a selection coefficient is changed on any mutation object in the simulation.  This is used as a signal to mutation runs that their
	// cache of non-neutral mutations is invalid (because their counter is not equal to this counter).  The caches will be re-validated the next time they are used.  Other
	// code can also increment this counter in order to trigger a re-validation of all non-neutral mutation caches; it is a general-purpose mechanism.
	int32_t nonneutral_change_counter_ = 0;
	int32_t last_nonneutral_regime_ = 0;		// see mutation_run.h; 1 = no mutationEffect() callbacks, 2 = only constant-effect neutral callbacks, 3 = arbitrary callbacks
	
	// state about what symbols/names/identifiers have been used or are being used
	// used_subpop_ids_ has every subpop id ever used, even if no longer in use, with the *last* name used for that subpop
	// used_subpop_names_ has every name ever used EXCEPT standard p1, p2... names, even if the name got replaced by a new name later
	std::unordered_map<slim_objectid_t, std::string> used_subpop_ids_;
	std::unordered_set<std::string> used_subpop_names_;
	
#if (SLIMPROFILING == 1)
	// PROFILING : Species keeps track of its memory usage profile info and mutation-related profile info
	// BCH 11/24/2024: Note that Chromosome now keeps additional profile information that is per-chromosome
	SLiMMemoryUsage_Species profile_last_memory_usage_Species;
	SLiMMemoryUsage_Species profile_total_memory_usage_Species;
	
#if SLIM_USE_NONNEUTRAL_CACHES
	std::vector<int32_t> profile_nonneutral_regime_history_;						// a record of the nonneutral regime used in each cycle
	int64_t profile_max_mutation_index_;											// the largest mutation index seen over the course of the profile
#endif	// SLIM_USE_NONNEUTRAL_CACHES
#endif	// (SLIMPROFILING == 1)
	
	Species(const Species&) = delete;																	// no copying
	Species& operator=(const Species&) = delete;														// no copying
	Species(Community &p_community, slim_objectid_t p_species_id, const std::string &p_name);			// construct a Species from a community / id / name
	~Species(void);																						// destructor
	
	// Chromosome configuration and access
	inline __attribute__((always_inline)) const std::vector<Chromosome *> &Chromosomes(void)	{ return chromosomes_; }
	inline __attribute__((always_inline)) const std::vector<Chromosome *> &ChromosomesForHaplosomeIndices(void) { return chromosome_for_haplosome_index_; }
	inline __attribute__((always_inline)) const std::vector<uint8_t> &ChromosomeSubindicesForHaplosomeIndices(void) { return chromosome_subindex_for_haplosome_index_; }
	inline __attribute__((always_inline)) const std::vector<int> &FirstHaplosomeIndices(void) { return first_haplosome_index_; }
	inline __attribute__((always_inline)) const std::vector<int> &LastHaplosomeIndices(void) { return last_haplosome_index_; }
	Chromosome *ChromosomeFromID(int64_t p_id);
	Chromosome *ChromosomeFromSymbol(const std::string &p_symbol);
	void MakeImplicitChromosome(ChromosomeType p_type);
	Chromosome *CurrentlyInitializingChromosome(void);								// the last chromosome defined (currently initializing)
	void AddChromosome(Chromosome *p_chromosome);									// takes over a retain count from the caller
	inline __attribute__((always_inline)) bool ChromosomesUseNullHaplosomes(void) { return chromosomes_use_null_haplosomes_; }
	inline __attribute__((always_inline)) int HaplosomeCountPerIndividual(void) { return haplosome_count_per_individual_; }
	
	Chromosome *GetChromosomeFromEidosValue(EidosValue *chromosome_value);																// with a singleton EidosValue
	void GetChromosomeIndicesFromEidosValue(std::vector<slim_chromosome_index_t> &chromosome_indices, EidosValue *chromosomes_value);	// with a vector EidosValue
	
	// Trait configuration and access
	inline __attribute__((always_inline)) const std::vector<Trait *> &Traits(void)	{ return traits_; }
	inline __attribute__((always_inline)) int TraitCount(void)	{ return (int)traits_.size(); }
	Trait *TraitFromName(const std::string &p_name) const;
	inline __attribute__((always_inline)) Trait *TraitFromStringID(EidosGlobalStringID p_string_id) const
	{
		// This is used for (hopefully) very fast lookup of a trait based on a string id in Eidos,
		// so that the user can do "individual.trait" and get a trait value like a property access
		auto iter = trait_from_string_id.find(p_string_id);
		
		if (iter == trait_from_string_id.end())
			return nullptr;
		
		return (*iter).second;
	}
	void MakeImplicitTrait(void);
	void AddTrait(Trait *p_trait);													// takes over a retain count from the caller
	
	int64_t GetTraitIndexFromEidosValue(EidosValue *trait_value, const std::string &p_method_name);											// with a singleton EidosValue
	void GetTraitIndicesFromEidosValue(std::vector<int64_t> &trait_indices, EidosValue *traits_value, const std::string &p_method_name);
	
	// Memory usage
	void TabulateSLiMMemoryUsage_Species(SLiMMemoryUsage_Species *p_usage);			// used by outputUsage() and SLiMgui profiling
	void DeleteAllMutationRuns(void);												// for cleanup
	
	// Running cycles
	std::vector<SLiMEidosBlock*> CallbackBlocksMatching(slim_tick_t p_tick, SLiMEidosBlockType p_event_type, slim_objectid_t p_mutation_type_id, slim_objectid_t p_interaction_type_id, slim_objectid_t p_subpopulation_id, slim_objectid_t p_trait_index, int64_t p_chromosome_id);
	void RunInitializeCallbacks(void);
	void CreateAndPromulgateMutationBlock(void);
	void EndCurrentChromosome(bool starting_new_chromosome);
	bool HasDoneAnyInitialization(void);
	void PrepareForCycle(void);
	void MaintainMutationRegistry(void);
	void RecalculateFitness(void);
	void MaintainTreeSequence(void);
	void EmptyGraveyard(void);
	void FinishMutationRunExperimentTimings(void);
	
	void WF_GenerateOffspring(void);
	void WF_SwitchToChildGeneration(void);
	void WF_SwapGenerations(void);
	
	void nonWF_GenerateOffspring(void);
	void nonWF_MergeOffspring(void);
	void nonWF_ViabilitySurvival(void);
	
	void AdvanceCycleCounter(void);
	void SimulationHasFinished(void);
	void Species_CheckIntegrity(void);
	
	// Reproduction pattern inference
	void InferInheritanceForClone(Chromosome *chromosome, Individual *parent, IndividualSex sex, Haplosome **strand1, Haplosome **strand3, const char *caller_name);
	void InferInheritanceForCross(Chromosome *chromosome, Individual *parent1, Individual *parent2, IndividualSex sex, Haplosome **strand1, Haplosome **strand2, Haplosome **strand3, Haplosome **strand4, const char *caller_name);
	
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
	
	// Mutation stack policy checking
	inline __attribute__((always_inline)) void MutationStackPolicyChanged(void)												{ mutation_stack_policy_changed_ = true; }
	inline __attribute__((always_inline)) void CheckMutationStackPolicy(void)												{ if (mutation_stack_policy_changed_) _CheckMutationStackPolicy(); }
	void _CheckMutationStackPolicy(void);
	
	// Nucleotide-based models
	inline __attribute__((always_inline)) double MaxNucleotideMutationRate(void)											{ return max_nucleotide_mut_rate_; }
	void MaxNucleotideMutationRateChanged(void);
	
	// accessors
	inline __attribute__((always_inline)) slim_tick_t Cycle(void) const														{ return cycle_; }
	void SetCycle(slim_tick_t p_new_cycle);
	
	inline __attribute__((always_inline)) bool Active(void) { return species_active_; }
	inline __attribute__((always_inline)) void SetActive(bool p_active) { species_active_ = p_active; }
	inline __attribute__((always_inline)) slim_tick_t TickModulo(void) { return tick_modulo_; }
	inline __attribute__((always_inline)) slim_tick_t TickPhase(void) { return tick_phase_; }
	
	inline __attribute__((always_inline)) bool HasGenetics(void)															{ return has_genetics_; }
	inline __attribute__((always_inline)) MutationBlock *SpeciesMutationBlock(void)											{ return mutation_block_; }
	inline __attribute__((always_inline)) const std::map<slim_objectid_t,MutationType*> &MutationTypes(void) const			{ return mutation_types_; }
	inline __attribute__((always_inline)) const std::map<slim_objectid_t,GenomicElementType*> &GenomicElementTypes(void)	{ return genomic_element_types_; }
	inline __attribute__((always_inline)) size_t GraveyardSize(void) const													{ return graveyard_.size(); }
	
	inline Subpopulation *SubpopulationWithID(slim_objectid_t p_subpop_id) {
		auto id_iter = population_.subpops_.find(p_subpop_id);
		return (id_iter == population_.subpops_.end()) ? nullptr : id_iter->second;
	}
	Subpopulation *SubpopulationWithName(const std::string &p_subpop_name);
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
	inline __attribute__((always_inline)) int SpatialDimensionality(void) const												{ return spatial_dimensionality_; }
	inline __attribute__((always_inline)) void SpatialPeriodicity(bool *p_x, bool *p_y, bool *p_z) const
	{
		if (p_x) *p_x = periodic_x_;
		if (p_y) *p_y = periodic_y_;
		if (p_z) *p_z = periodic_z_;
	}
	
	inline __attribute__((always_inline)) bool UserWantsMutrunExperiments(void) const										{ return do_mutrun_experiments_; }
	inline __attribute__((always_inline)) void DoingMutrunExperimentsForChromosome(void)									{ doing_any_mutrun_experiments_ = true; }
	inline __attribute__((always_inline)) bool DoingAnyMutationRunExperiments(void) const									{ return doing_any_mutrun_experiments_; }
	
	inline __attribute__((always_inline)) bool IsNucleotideBased(void) const												{ return nucleotide_based_; }
	
	
	// TREE SEQUENCE RECORDING
#pragma mark -
#pragma mark treeseq recording methods
#pragma mark -
	inline __attribute__((always_inline)) bool RecordingTreeSequence(void) const											{ return recording_tree_; }
	inline __attribute__((always_inline)) bool RecordingTreeSequenceMutations(void) const									{ return recording_mutations_; }
	void AboutToSplitSubpop(void);	// see Population::AddSubpopulationSplit()
	
	void CopySharedTablesIn(tsk_table_collection_t &p_tables);				// copies the shared tables for the species into p_tables
	void DisconnectCopiedSharedTables(tsk_table_collection_t &p_tables);	// zeroes out the shared table copies in p_tables
	
	static void handle_error(const std::string &msg, int error) __attribute__((__noreturn__)) __attribute__((cold)) __attribute__((analyzer_noreturn));
	static void MetadataForMutation(Mutation *p_mutation, MutationMetadataRec *p_metadata);
	static void MetadataForSubstitution(Substitution *p_substitution, MutationMetadataRec *p_metadata);
	static void MetadataForIndividual(Individual *p_individual, IndividualMetadataRec *p_metadata);
	static void DerivedStatesFromAscii(tsk_table_collection_t *p_tables);
	static void DerivedStatesToAscii(tsk_table_collection_t *p_tables);
	
	bool _SubpopulationIDInUse(slim_objectid_t p_subpop_id);
	void RecordTablePosition(void);
	void AllocateTreeSequenceTables(void);
	void SetCurrentNewIndividual(Individual *p_individual);
	void RecordNewHaplosome(slim_position_t *p_breakpoints, int p_breakpoints_count, Haplosome *p_new_haplosome, const Haplosome *p_initial_parental_haplosome, const Haplosome *p_second_parental_haplosome);
	void RecordNewHaplosome_NULL(Haplosome *p_new_haplosome);
	void RecordNewDerivedState(const Haplosome *p_haplosome, slim_position_t p_position, const std::vector<Mutation *> &p_derived_mutations);
	void RetractNewIndividual(void);
	void AddIndividualsToTable(Individual * const *p_individual, size_t p_num_individuals, tsk_table_collection_t *p_tables, INDIVIDUALS_HASH *p_individuals_hash, tsk_flags_t p_flags);
	void AddLiveIndividualsToIndividualsTable(tsk_table_collection_t *p_tables, INDIVIDUALS_HASH *p_individuals_hash);
	void FixAliveIndividuals(tsk_table_collection_t *p_tables);
	void WritePopulationTable(tsk_table_collection_t *p_tables);
	void WriteProvenanceTable(tsk_table_collection_t *p_tables, bool p_use_newlines, bool p_include_model, slim_chromosome_index_t p_chromosome_index);
	void WriteTreeSequenceMetadata(tsk_table_collection_t *p_tables, EidosDictionaryUnretained *p_metadata_dict, slim_chromosome_index_t p_chromosome_index);
	void _MungeIsNullNodeMetadataToIndex0(TreeSeqInfo &p_treeseq, int original_index);
	void ReadTreeSequenceMetadata(TreeSeqInfo &p_treeseq, slim_tick_t *p_tick, slim_tick_t *p_cycle, SLiMModelType *p_model_type, int *p_file_version);
	void _CreateDirectoryForMultichromArchive(std::string resolved_user_path, bool p_overwrite_directory);
	void WriteTreeSequence(std::string &p_recording_tree_path, bool p_simplify, bool p_include_model, EidosDictionaryUnretained *p_metadata_dict, bool p_overwrite_directory);
    void ReorderIndividualTable(tsk_table_collection_t *p_tables, std::vector<int> p_individual_map, bool p_keep_unmapped);
	void AddParentsColumnForOutput(tsk_table_collection_t *p_tables, INDIVIDUALS_HASH *p_individuals_hash);
	void BuildTabledIndividualsHash(tsk_table_collection_t *p_tables, INDIVIDUALS_HASH *p_individuals_hash);
	void _SimplifyTreeSequence(TreeSeqInfo &tsinfo, const std::vector<tsk_id_t> &samples);
	void SimplifyAllTreeSequences(void);
	void CheckCoalescenceAfterSimplification(TreeSeqInfo &tsinfo);
	void CheckAutoSimplification(void);
	void FreeTreeSequence();
	void RecordAllDerivedStatesFromSLiM(void);
	void CheckTreeSeqIntegrity(void);		// checks the tree sequence tables themselves
	void CrosscheckTreeSeqIntegrity(void);	// checks the tree sequence tables against SLiM's data structures
	
	void __CheckPopulationMetadata(TreeSeqInfo &p_treeseq);
	void __RemapSubpopulationIDs(SUBPOP_REMAP_HASH &p_subpop_map, TreeSeqInfo &p_treeseq, int p_file_version);
	void __PrepareSubpopulationsFromTables(std::unordered_map<slim_objectid_t, ts_subpop_info> &p_subpopInfoMap, TreeSeqInfo &p_treeseq);
	void __TabulateSubpopulationsFromTreeSequence(std::unordered_map<slim_objectid_t, ts_subpop_info> &p_subpopInfoMap, tsk_treeseq_t *p_ts, TreeSeqInfo &p_treeseq, SLiMModelType p_file_model_type);
	void __CreateSubpopulationsFromTabulation(std::unordered_map<slim_objectid_t, ts_subpop_info> &p_subpopInfoMap, EidosInterpreter *p_interpreter, std::unordered_map<tsk_id_t, Haplosome *> &p_nodeToHaplosomeMap, TreeSeqInfo &p_treeseq);
	void __CreateSubpopulationsFromTabulation_SECONDARY(std::unordered_map<slim_objectid_t, ts_subpop_info> &p_subpopInfoMap, EidosInterpreter *p_interpreter, std::unordered_map<tsk_id_t, Haplosome *> &p_nodeToHaplosomeMap, TreeSeqInfo &p_treeseq);
	void __ConfigureSubpopulationsFromTables(EidosInterpreter *p_interpreter, TreeSeqInfo &p_treeseq);
	void __ConfigureSubpopulationsFromTables_SECONDARY(EidosInterpreter *p_interpreter, TreeSeqInfo &p_treeseq);
	void __TabulateMutationsFromTables(std::unordered_map<slim_mutationid_t, ts_mut_info> &p_mutMap, TreeSeqInfo &p_treeseq, int p_file_version);
	void __TallyMutationReferencesWithTreeSequence(std::unordered_map<slim_mutationid_t, ts_mut_info> &p_mutMap, std::unordered_map<tsk_id_t, Haplosome *> p_nodeToHaplosomeMap, tsk_treeseq_t *p_ts);
	void __CreateMutationsFromTabulation(std::unordered_map<slim_mutationid_t, ts_mut_info> &p_mutInfoMap, std::unordered_map<slim_mutationid_t, MutationIndex> &p_mutIndexMap, TreeSeqInfo &p_treeseq);
	void __AddMutationsFromTreeSequenceToHaplosomes(std::unordered_map<slim_mutationid_t, MutationIndex> &p_mutIndexMap, std::unordered_map<tsk_id_t, Haplosome *> p_nodeToHaplosomeMap, tsk_treeseq_t *p_ts, TreeSeqInfo &p_treeseq);
	void __CheckNodePedigreeIDs(EidosInterpreter *p_interpreter, TreeSeqInfo &p_treeseq);
	void _ReadAncestralSequence(const char *p_file, Chromosome &p_chromosome);
	void _InstantiateSLiMObjectsFromTables(EidosInterpreter *p_interpreter, slim_tick_t p_metadata_tick, slim_tick_t p_metadata_cycle, SLiMModelType p_file_model_type, int p_file_version, SUBPOP_REMAP_HASH &p_subpop_map, TreeSeqInfo &p_treeseq);	// given tree-seq tables, makes individuals, haplosomes, and mutations
	void _InstantiateSLiMObjectsFromTables_SECONDARY(EidosInterpreter *p_interpreter, slim_tick_t p_metadata_tick, slim_tick_t p_metadata_cycle, SLiMModelType p_file_model_type, int p_file_version, SUBPOP_REMAP_HASH &p_subpop_map, TreeSeqInfo &p_treeseq);	// given tree-seq tables, makes individuals, haplosomes, and mutations
	void _PostInstantiationCleanup(EidosInterpreter *p_interpreter);
	slim_tick_t _InitializePopulationFromTskitBinaryFile(const char *p_file, EidosInterpreter *p_interpreter, SUBPOP_REMAP_HASH &p_subpop_remap, Chromosome &p_chromosome);	// initialize the population from an tskit binary file
	slim_tick_t _InitializePopulationFromTskitDirectory(std::string p_directory, EidosInterpreter *p_interpreter, SUBPOP_REMAP_HASH &p_subpop_remap);	// initialize the population from a multi-chromosome directory
	
	size_t MemoryUsageForTreeSeqInfo(TreeSeqInfo &p_tsinfo, bool p_count_shared_tables);
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
	EidosValue_SP ExecuteContextFunction_initializeChromosome(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteContextFunction_initializeGenomicElement(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteContextFunction_initializeGenomicElementType(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteContextFunction_initializeMutationType(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteContextFunction_initializeRecombinationRate(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteContextFunction_initializeTrait(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
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
	
	EidosValue_SP ExecuteMethod_addPatternForClone(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_addPatternForCross(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_addPatternForNull(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_addPatternForRecombinant(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_addSubpop(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_addSubpopSplit(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_chromosomesOfType(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_chromosomesWithIDs(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_chromosomesWithSymbols(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_traitsWithIndices(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_traitsWithNames(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
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
	EidosValue_SP ExecuteMethod_substitutionsOfType(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_treeSeqCoalesced(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_treeSeqSimplify(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_treeSeqRememberIndividuals(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_treeSeqOutput(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod__debug(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
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












































































