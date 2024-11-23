//
//  chromosome.h
//  SLiM
//
//  Created by Ben Haller on 12/13/14.
//  Copyright (c) 2014-2024 Philipp Messer.  All rights reserved.
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
 
 The class Chromosome represents an entire chromosome.  Only the portions of the chromosome that are relevant to the simulation are
 explicitly modeled, so in practice, a chromosome is a vector of genomic elements defined by the input file.  A chromosome also has
 a length, an overall mutation rate, an overall recombination rate, and parameters related to gene conversion.
 
 */

#ifndef __SLiM__chromosome__
#define __SLiM__chromosome__


#include <vector>
#include <map>

#include "mutation.h"
#include "mutation_type.h"
#include "genomic_element.h"
#include "genomic_element_type.h"
#include "eidos_rng.h"
#include "eidos_value.h"

struct GESubrange;
class Haplosome;
class Species;
class Individual;


extern EidosClass *gSLiM_Chromosome_Class;


class Chromosome : public EidosDictionaryRetained
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.

private:
	typedef EidosDictionaryRetained super;

#ifdef SLIMGUI
public:
#else
private:
#endif
	
	int64_t id_;
	std::string symbol_;
	std::string name_;
	slim_chromosome_index_t index_;
	ChromosomeType type_;
	
	// This vector contains all the genomic elements for this chromosome.  It is in sorted order once initialization is complete.
	std::vector<GenomicElement *> genomic_elements_;		// OWNED POINTERS: genomic elements belong to the chromosome
	
	// We now allow different recombination maps for males and females, optionally.  Unfortunately, this means we have a bit of an
	// explosion of state involved with recombination.  We now have _H, _M, and _F versions of many ivars.  The _M and _F versions
	// are used if sex is enabled and different recombination maps have been specified for males versus females.  The _H version
	// is used in all other cases – when sex is not enabled (i.e., hermaphrodites), and when separate maps have not been specified.
	// This flag indicates which option has been chosen; after initialize() time this cannot be changed.
	bool single_recombination_map_ = true;
	
	// The same is now true for mutation rate maps as well; we now have _H, _M, and _F versions of those, just as with recombination
	// maps.  This flag indicates which option has been chosen; after initialize() time this cannot be changed.
	bool single_mutation_map_ = true;
	
	gsl_ran_discrete_t *lookup_mutation_H_ = nullptr;		// OWNED POINTER: lookup table for drawing mutations
	gsl_ran_discrete_t *lookup_mutation_M_ = nullptr;
	gsl_ran_discrete_t *lookup_mutation_F_ = nullptr;
	
	gsl_ran_discrete_t *lookup_recombination_H_ = nullptr;	// OWNED POINTER: lookup table for drawing recombination breakpoints
	gsl_ran_discrete_t *lookup_recombination_M_ = nullptr;
	gsl_ran_discrete_t *lookup_recombination_F_ = nullptr;
	
	// caches to speed up Poisson draws in CrossoverMutation()
	double exp_neg_overall_mutation_rate_H_;			
	double exp_neg_overall_mutation_rate_M_;
	double exp_neg_overall_mutation_rate_F_;
	
	double exp_neg_overall_recombination_rate_H_;			
	double exp_neg_overall_recombination_rate_M_;
	double exp_neg_overall_recombination_rate_F_;
	
#ifndef USE_GSL_POISSON
	// joint probabilities, used to accelerate drawing a mutation count and breakpoint count jointly
	double probability_both_0_H_;
	double probability_both_0_OR_mut_0_break_non0_H_;
	double probability_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0_H_;
	
	double probability_both_0_M_;
	double probability_both_0_OR_mut_0_break_non0_M_;
	double probability_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0_M_;
	
	double probability_both_0_F_;
	double probability_both_0_OR_mut_0_break_non0_F_;
	double probability_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0_F_;
#endif
	
	// GESubrange vectors used to facilitate new mutation generation – draw a subrange, then draw a position inside the subrange
	std::vector<GESubrange> mutation_subranges_H_;
	std::vector<GESubrange> mutation_subranges_M_;
	std::vector<GESubrange> mutation_subranges_F_;
	
	// This object pool for haplosomes is owned by Species; we have a reference to it just for efficiency
	EidosObjectPool &haplosome_pool_;				// NOT OWNED: a pool out of which haplosomes are allocated, for within-species locality
	
	// These haplosome junkyards, on the other hand, belong to us; each Chromosome keeps its own junkyards in which objects
	// are guaranteed to have the correct chromosome index already set up, and the correct chromosome mutrun configuration
	std::vector<Haplosome *> haplosomes_junkyard_nonnull;	// OWNED: non-null haplosomes go here when we're done with them, for reuse
	std::vector<Haplosome *> haplosomes_junkyard_null;		// OWNED: null haplosomes go here when we're done with them, for reuse
	
	Haplosome *_NewHaplosome_NULL(Individual *p_individual);		// internal use only
	Haplosome *_NewHaplosome_NONNULL(Individual *p_individual);		// internal use only
	
	// Chromosome now keeps two MutationRunPools, one for freed MutationRun objects and one for in-use MutationRun objects,
	// as well as an object pool out of which completely new MutationRuns are allocated, all bundled in a MutationRunContext.
	// When running multithreaded, each of these becomes a vector of per-thread objects, so we can alloc/free runs in parallel code.
	// This stuff is not set up until after initialize() callbacks; nobody should be using MutationRuns before then.
#ifndef _OPENMP
	MutationRunContext mutation_run_context_SINGLE_;
#else
	int mutation_run_context_COUNT_ = 0;											// the number of PERTHREAD contexts
	std::vector<MutationRunContext *> mutation_run_context_PERTHREAD;
#endif
	
	// Mutation run optimization.  The ivars here are used only internally by Species; the canonical reference regarding the
	// number and length of mutation runs is kept by Chromosome (for the simulation) and by Haplosome (for each haplosome object).
	// If Species decides to change the number of mutation runs, it will update those canonical repositories accordingly.
	// A prefix of x_ is used on all mutation run experiment ivars, to avoid confusion.
	int preferred_mutrun_count_ = 0;			// preferred mutation run length (from the user); 0 represents no preference
	
#define SLIM_MUTRUN_EXPERIMENT_LENGTH	50		// kind of based on how large a sample size is needed to detect important differences fairly reliably by t-test
#define SLIM_MUTRUN_MAXIMUM_COUNT		1024	// the most mutation runs we will ever use; hard to imagine that any model will want more than this
	
	bool x_experiments_enabled_;				// if false, no experiments are run and no cycle runtimes are recorded
	int32_t x_experiment_count_;				// a counter of the number of experiments we've run (the number of t-tests we've conducted)
	
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
	
	eidos_profile_t x_current_clock_ = 0;		// the clock for timing current being done
	bool x_clock_running_ = false;
	
	eidos_profile_t x_total_gen_clocks_ = 0;	// a counter of clocks accumulated for the current cycle's runtime (across measured code blocks)
												// look at StartMutationRunExperimentClock() usage to see which blocks are measured
	
	// Mutation run experiments
	void TransitionToNewExperimentAgainstCurrentExperiment(int32_t p_new_mutrun_count);
	void TransitionToNewExperimentAgainstPreviousExperiment(int32_t p_new_mutrun_count);
	void EnterStasisForMutationRunExperiments(void);
	void MaintainMutationRunExperiments(double p_last_gen_runtime);
	
public:
	
	Community &community_;
	Species &species_;
	
	// the total haplosome count depends on the chromosome; it will be different for an autosome versus a sex chromosome, for example
	slim_refcount_t total_haplosome_count_ = 0;				// the number of non-null haplosomes in the population; a fixed mutation has this count
#ifdef SLIMGUI
	slim_refcount_t gui_total_haplosome_count_ = 0;			// the number of non-null haplosomes in the selected subpopulations in SLiMgui
#endif
	
	slim_refcount_t tallied_haplosome_count_ = 0;			// the total non-null haplosomes counted for this chromosome in the last tally
	
	// mutation and recombination machinery
	std::vector<slim_position_t> mutation_end_positions_H_;		// end positions of each defined mutation region (BEFORE intersection with GEs)
	std::vector<slim_position_t> mutation_end_positions_M_;
	std::vector<slim_position_t> mutation_end_positions_F_;
	
	std::vector<double> mutation_rates_H_;						// mutation rates, in events per base pair (BEFORE intersection with GEs)
	std::vector<double> mutation_rates_M_;
	std::vector<double> mutation_rates_F_;
	
	std::vector<slim_position_t> recombination_end_positions_H_;	// end positions of each defined recombination region
	std::vector<slim_position_t> recombination_end_positions_M_;
	std::vector<slim_position_t> recombination_end_positions_F_;
	
	std::vector<double> recombination_rates_H_;					// recombination rates, in probability of crossover per base pair (user-specified)
	std::vector<double> recombination_rates_M_;
	std::vector<double> recombination_rates_F_;
	
	bool any_recombination_rates_05_ = false;				// set to T if any recombination rate is 0.5; those are excluded from gene conversion
	
	slim_position_t first_position_;						// first valid position
	slim_position_t last_position_;							// last valid position
	bool extent_immutable_;									// can the start/end still be changed?
	
	double overall_mutation_rate_H_;						// overall mutation rate (AFTER intersection with GEs)
	double overall_mutation_rate_M_;						// overall mutation rate
	double overall_mutation_rate_F_;						// overall mutation rate
	
	double overall_mutation_rate_H_userlevel_;				// requested (un-adjusted) overall mutation rate (AFTER intersection with GEs)
	double overall_mutation_rate_M_userlevel_;				// requested (un-adjusted) overall mutation rate
	double overall_mutation_rate_F_userlevel_;				// requested (un-adjusted) overall mutation rate
	
	double overall_recombination_rate_H_;					// overall recombination rate (reparameterized; see _InitializeOneRecombinationMap)
	double overall_recombination_rate_M_;					// overall recombination rate (reparameterized; see _InitializeOneRecombinationMap)
	double overall_recombination_rate_F_;					// overall recombination rate (reparameterized; see _InitializeOneRecombinationMap)
	
	double overall_recombination_rate_H_userlevel_;			// overall recombination rate (unreparameterized; see _InitializeOneRecombinationMap)
	double overall_recombination_rate_M_userlevel_;			// overall recombination rate (unreparameterized; see _InitializeOneRecombinationMap)
	double overall_recombination_rate_F_userlevel_;			// overall recombination rate (unreparameterized; see _InitializeOneRecombinationMap)
	
	bool using_DSB_model_;									// if true, we are using the DSB recombination model, involving the variables below
	double non_crossover_fraction_;							// fraction of DSBs that do not result in crossover
	double gene_conversion_avg_length_;						// average gene conversion stretch length
	double gene_conversion_inv_half_length_;				// 1.0 / (gene_conversion_avg_length_ / 2.0), used for geometric draws
	double simple_conversion_fraction_;						// fraction of gene conversion tracts that are "simple" (no heteroduplex mismatche repair)
	double mismatch_repair_bias_;							// GC bias in heteroduplex mismatch repair in complex gene conversion tracts
	bool redraw_lengths_on_failure_;						// if T, we redraw lengths, not only positions, if tract layout fails
	
	int32_t mutrun_count_base_;								// minimum number of mutruns used (number of threads, typically); can be multiplied by a factor
	int32_t mutrun_count_multiplier_;						// the current factor by which mutrun_count_base_ is multiplied; a power of two in [1, 1024]
	int32_t mutrun_count_;									// the number of mutation runs being used for all haplosomes: base x multiplier
	slim_position_t mutrun_length_;							// the length, in base pairs, of each mutation run; the last run might not use its full length
	slim_position_t last_position_mutrun_;					// (mutrun_count_ * mutrun_length_ - 1), for complete coverage in crossover-mutation
	
	std::string color_sub_;										// color to use for substitutions by default (in SLiMgui)
	float color_sub_red_, color_sub_green_, color_sub_blue_;	// cached color components from color_sub_; should always be in sync
	
	// nucleotide-based models
	NucleotideArray *ancestral_seq_buffer_ = nullptr;
	std::vector<slim_position_t> hotspot_end_positions_H_;		// end positions of each defined hotspot region (BEFORE intersection with GEs)
	std::vector<slim_position_t> hotspot_end_positions_M_;
	std::vector<slim_position_t> hotspot_end_positions_F_;
	std::vector<double> hotspot_multipliers_H_;					// hotspot multipliers (BEFORE intersection with GEs)
	std::vector<double> hotspot_multipliers_M_;
	std::vector<double> hotspot_multipliers_F_;
	
	slim_usertag_t tag_value_ = SLIM_TAG_UNSET_VALUE;			// a user-defined tag value
	
	Chromosome(const Chromosome&) = delete;									// no copying
	Chromosome& operator=(const Chromosome&) = delete;						// no copying
	Chromosome(void) = delete;												// no null constructor
	
	explicit Chromosome(Species &p_species, ChromosomeType p_type, int64_t p_id, std::string p_symbol, slim_chromosome_index_t p_index, int p_preferred_mutcount);
	~Chromosome(void);
	
	inline __attribute__((always_inline)) int64_t ID(void)	{ return id_; }
	inline __attribute__((always_inline)) const std::string &Symbol(void)	{ return symbol_; }
	inline __attribute__((always_inline)) slim_chromosome_index_t Index(void) { return index_; }
	inline __attribute__((always_inline)) ChromosomeType Type(void) { return type_; }
	inline __attribute__((always_inline)) const std::string &Name(void) { return name_; }
	inline __attribute__((always_inline)) void SetName(const std::string &p_name) { name_ = p_name; }
	
	inline __attribute__((always_inline)) std::vector<GenomicElement *> &GenomicElements(void)			{ return genomic_elements_; }
	inline __attribute__((always_inline)) NucleotideArray *AncestralSequence(void)						{ return ancestral_seq_buffer_; }
	
	// initialize the random lookup tables used by Chromosome to draw mutation and recombination events
	void CreateNucleotideMutationRateMap(void);
	void InitializeDraws(void);
	void _InitializeOneRecombinationMap(gsl_ran_discrete_t *&p_lookup, std::vector<slim_position_t> &p_end_positions, std::vector<double> &p_rates, double &p_overall_rate, double &p_exp_neg_overall_rate, double &p_overall_rate_userlevel);
	void _InitializeOneMutationMap(gsl_ran_discrete_t *&p_lookup, std::vector<slim_position_t> &p_end_positions, std::vector<double> &p_rates, double &p_requested_overall_rate, double &p_overall_rate, double &p_exp_neg_overall_rate, std::vector<GESubrange> &p_subranges);
	void ChooseMutationRunLayout(void);
	
	inline bool UsingSingleRecombinationMap(void) const { return single_recombination_map_; }
	inline bool UsingSingleMutationMap(void) const { return single_mutation_map_; }
	inline size_t GenomicElementCount(void) const { return genomic_elements_.size(); }
	
	// draw the number of mutations that occur, based on the overall mutation rate
	int DrawMutationCount(IndividualSex p_sex) const;
	
	// draw a vector of mutation positions (and the corresponding GenomicElementType objects), which is sorted and uniqued for the caller
	int DrawSortedUniquedMutationPositions(int p_count, IndividualSex p_sex, std::vector<std::pair<slim_position_t, GenomicElement *>> &p_positions);
	
	// draw a new mutation, based on the genomic element types present and their mutational proclivities
	MutationIndex DrawNewMutation(std::pair<slim_position_t, GenomicElement *> &p_position, slim_objectid_t p_subpop_index, slim_tick_t p_tick) const;
	
	// draw a new mutation with reference to the genomic background upon which it is occurring, for nucleotide-based models and/or mutation() callbacks
	Mutation *ApplyMutationCallbacks(Mutation *p_mut, Haplosome *p_haplosome, GenomicElement *p_genomic_element, int8_t p_original_nucleotide, std::vector<SLiMEidosBlock*> &p_mutation_callbacks) const;
	MutationIndex DrawNewMutationExtended(std::pair<slim_position_t, GenomicElement *> &p_position, slim_objectid_t p_subpop_index, slim_tick_t p_tick, Haplosome *parent_haplosome_1, Haplosome *parent_haplosome_2, std::vector<slim_position_t> *all_breakpoints, std::vector<SLiMEidosBlock*> *p_mutation_callbacks) const;
	
	// draw the number of breakpoints that occur, based on the overall recombination rate
	int DrawBreakpointCount(IndividualSex p_sex) const;
	
	// choose a set of recombination breakpoints, based on recomb. intervals, overall recomb. rate, and gene conversion parameters
	void DrawCrossoverBreakpoints(IndividualSex p_parent_sex, const int p_num_breakpoints, std::vector<slim_position_t> &p_crossovers) const;
	void DrawDSBBreakpoints(IndividualSex p_parent_sex, const int p_num_breakpoints, std::vector<slim_position_t> &p_crossovers, std::vector<slim_position_t> &p_heteroduplex) const;
	
#ifndef USE_GSL_POISSON
	// draw both the mutation count and breakpoint count, using a single Poisson draw for speed
	void DrawMutationAndBreakpointCounts(IndividualSex p_sex, int *p_mut_count, int *p_break_count) const;
	
	// initialize the joint probabilities used by DrawMutationAndBreakpointCounts()
	void _InitializeJointProbabilities(double p_overall_mutation_rate, double p_exp_neg_overall_mutation_rate,
												   double p_overall_recombination_rate, double p_exp_neg_overall_recombination_rate,
												   double &p_both_0, double &p_both_0_OR_mut_0_break_non0, double &p_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0);
#endif
	
	// looking up genomic elements quickly, by binary search
	GenomicElement *ElementForPosition(slim_position_t pos);
	
	// internal methods for throwing errors from inline functions when assumptions about the configuration of maps are violated
	void MutationMapConfigError(void) const __attribute__((__noreturn__)) __attribute__((cold)) __attribute__((analyzer_noreturn));
	void RecombinationMapConfigError(void) const __attribute__((__noreturn__)) __attribute__((cold)) __attribute__((analyzer_noreturn));
	
	// Memory usage tallying, for outputUsage()
	size_t MemoryUsageForMutationMaps(void);
	size_t MemoryUsageForRecombinationMaps(void);
	size_t MemoryUsageForAncestralSequence(void);
	
	// Make a null haplosome, which is associated with an individual, but has no associated chromosome, or
	// make a non-null haplosome, which is associated with an individual and has an associated chromosome
	Haplosome *NewHaplosome_NULL(Individual *p_individual);
	Haplosome *NewHaplosome_NONNULL(Individual *p_individual);
	void FreeHaplosome(Haplosome *p_haplosome);
	
	const std::vector<Haplosome *> &HaplosomesJunkyardNonnull(void) { return haplosomes_junkyard_nonnull; }
	const std::vector<Haplosome *> &HaplosomesJunkyardNull(void) { return haplosomes_junkyard_null; }

	// Mutation run contexts: each chromosome keeps per-thread "contexts" out of which mutation runs get allocated, and
	// into which they get freed.  This eliminates between-thread locking when working with mutation runs.
	void SetUpMutationRunContexts(void);
	
#ifndef _OPENMP
	inline int ChromosomeMutationRunContextCount(void) { return 1; }
	inline __attribute__((always_inline)) MutationRunContext &ChromosomeMutationRunContextForThread(__attribute__((unused)) int p_thread_num)
	{
#if DEBUG
		if (p_thread_num != 0)
			EIDOS_TERMINATION << "ERROR (Chromosome::ChromosomeMutationRunContextForThread): (internal error) p_thread_num out of range." << EidosTerminate();
#endif
		return mutation_run_context_SINGLE_;
	}
	inline __attribute__((always_inline)) MutationRunContext &ChromosomeMutationRunContextForMutationRunIndex(__attribute__((unused)) slim_mutrun_index_t p_mutrun_index)
	{
#if DEBUG
		if ((p_mutrun_index < 0) || (p_mutrun_index >= mutrun_count_))
			EIDOS_TERMINATION << "ERROR (Chromosome::ChromosomeMutationRunContextForMutationRunIndex): (internal error) p_mutrun_index out of range." << EidosTerminate();
#endif
		return mutation_run_context_SINGLE_;
	}
#else
	inline int ChromosomeMutationRunContextCount(void) { return mutation_run_context_COUNT_; }
	inline __attribute__((always_inline)) MutationRunContext &ChromosomeMutationRunContextForThread(int p_thread_num)
	{
#if DEBUG
		if ((p_thread_num < 0) || (p_thread_num >= mutation_run_context_COUNT_))
			EIDOS_TERMINATION << "ERROR (Chromosome::ChromosomeMutationRunContextForThread): (internal error) p_thread_num out of range." << EidosTerminate();
#endif
		return *(mutation_run_context_PERTHREAD[p_thread_num]);
	}
	inline __attribute__((always_inline)) MutationRunContext &ChromosomeMutationRunContextForMutationRunIndex(__attribute__((unused)) slim_mutrun_index_t p_mutrun_index)
	{
#if DEBUG
		if ((p_mutrun_index < 0) || (p_mutrun_index >= mutrun_count_))
			EIDOS_TERMINATION << "ERROR (Chromosome::ChromosomeMutationRunContextForMutationRunIndex): (internal error) p_mutrun_index out of range." << EidosTerminate();
#endif
		// The range of the haplosome that each thread is responsible for does not change across
		// splits/joins; one mutrun becomes two, or two become one, owned by the same thread
		int thread_num = (int)(p_mutrun_index / mutrun_count_multiplier_);
		return *(mutation_run_context_PERTHREAD[thread_num]);
	}
#endif
	
	// Mutation run experiments
	void InitiateMutationRunExperiments(void);
	void ZeroMutationRunExperimentClock(void);
	void FinishMutationRunExperimentTiming(void);
	void PrintMutationRunExperimentSummary(void);
	inline __attribute__((always_inline)) bool MutationRunExperimentsEnabled(void) { return x_experiments_enabled_; }
	
	// Mutation run experiment timing.  We use these methods to accumulate clocks taken in critical sections of the code.
	// Note that this design does NOT include time taken in first()/early()/late() events; since script blocks can do very
	// different work from one cycle to the next, this seems best, although it does mean that the impact of the number
	// of mutation runs on the execution time of Eidos events is not measured.
	inline __attribute__((always_inline)) void StartMutationRunExperimentClock(void)
	{
		if (x_experiments_enabled_)
		{
#if DEBUG
				if (x_clock_running_)
					std::cerr << "WARNING: mutation run experiment clock was started when already running!";
#endif
				x_clock_running_ = true;
				x_current_clock_ = Eidos_BenchmarkTime();
		}
	}
	
	inline __attribute__((always_inline)) void StopMutationRunExperimentClock(__attribute__((unused)) const char *p_caller_name)
	{
		if (x_experiments_enabled_)
		{
			eidos_profile_t end_clock = Eidos_BenchmarkTime();
			
#if DEBUG
			if (!x_clock_running_)
				std::cerr << "WARNING: mutation run experiment clock was stopped when not running!";
#endif
			
#if MUTRUN_EXPERIMENT_TIMING_OUTPUT
			// this log generates an unreasonable amount of output and is not usually desirable
			//std::cout << "   tick " << community_.Tick() << ", chromosome " << id_ << ": mutrun experiment clock for " << p_caller_name << " count == " << (end_clock - x_current_clock_) << std::endl;
#endif
			
			x_clock_running_ = false;
			x_total_gen_clocks_ += (end_clock - x_current_clock_);
			x_current_clock_ = 0;
		}
	}
	
	
	//
	// Eidos support
	//
	virtual const EidosClass *Class(void) const override;
	virtual void Print(std::ostream &p_ostream) const override;
	
	virtual EidosValue_SP GetProperty(EidosGlobalStringID p_property_id) override;
	virtual void SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value) override;
	
	virtual EidosValue_SP ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) override;
	EidosValue_SP ExecuteMethod_ancestralNucleotides(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_genomicElementForPosition(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_hasGenomicElementForPosition(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_setAncestralNucleotides(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_setGeneConversion(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_setHotspotMap(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_setMutationRate(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_setRecombinationRate(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_drawBreakpoints(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
};

// draw the number of mutations that occur, based on the overall mutation rate
inline __attribute__((always_inline)) int Chromosome::DrawMutationCount(IndividualSex p_sex) const
{
	gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
	
#ifdef USE_GSL_POISSON
	if (single_mutation_map_)
	{
		// With a single map, we don't care what sex we are passed; same map for all, and sex may be enabled or disabled
		return gsl_ran_poisson(rng, overall_mutation_rate_H_);
	}
	else
	{
		// With sex-specific maps, we treat males and females separately, and the individual we're given better be one of the two
		if (p_sex == IndividualSex::kMale)
		{
			return gsl_ran_poisson(rng, overall_mutation_rate_M_);
		}
		else if (p_sex == IndividualSex::kFemale)
		{
			return gsl_ran_poisson(rng, overall_mutation_rate_F_);
		}
		else
		{
			MutationMapConfigError();
		}
	}
#else
	if (single_mutation_map_)
	{
		// With a single map, we don't care what sex we are passed; same map for all, and sex may be enabled or disabled
		return Eidos_FastRandomPoisson(rng, overall_mutation_rate_H_, exp_neg_overall_mutation_rate_H_);
	}
	else
	{
		// With sex-specific maps, we treat males and females separately, and the individual we're given better be one of the two
		if (p_sex == IndividualSex::kMale)
		{
			return Eidos_FastRandomPoisson(rng, overall_mutation_rate_M_, exp_neg_overall_mutation_rate_M_);
		}
		else if (p_sex == IndividualSex::kFemale)
		{
			return Eidos_FastRandomPoisson(rng, overall_mutation_rate_F_, exp_neg_overall_mutation_rate_F_);
		}
		else
		{
			MutationMapConfigError();
		}
	}
#endif
}

// draw the number of breakpoints that occur, based on the overall recombination rate
inline __attribute__((always_inline)) int Chromosome::DrawBreakpointCount(IndividualSex p_sex) const
{
	gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
	
#ifdef USE_GSL_POISSON
	if (single_recombination_map_)
	{
		// With a single map, we don't care what sex we are passed; same map for all, and sex may be enabled or disabled
		return gsl_ran_poisson(rng, overall_recombination_rate_H_);
	}
	else
	{
		// With sex-specific maps, we treat males and females separately, and the individual we're given better be one of the two
		if (p_sex == IndividualSex::kMale)
		{
			return gsl_ran_poisson(rng, overall_recombination_rate_M_);
		}
		else if (p_sex == IndividualSex::kFemale)
		{
			return gsl_ran_poisson(rng, overall_recombination_rate_F_);
		}
		else
		{
			RecombinationMapConfigError();
		}
	}
#else
	if (single_recombination_map_)
	{
		// With a single map, we don't care what sex we are passed; same map for all, and sex may be enabled or disabled
		return Eidos_FastRandomPoisson(rng, overall_recombination_rate_H_, exp_neg_overall_recombination_rate_H_);
	}
	else
	{
		// With sex-specific maps, we treat males and females separately, and the individual we're given better be one of the two
		if (p_sex == IndividualSex::kMale)
		{
			return Eidos_FastRandomPoisson(rng, overall_recombination_rate_M_, exp_neg_overall_recombination_rate_M_);
		}
		else if (p_sex == IndividualSex::kFemale)
		{
			return Eidos_FastRandomPoisson(rng, overall_recombination_rate_F_, exp_neg_overall_recombination_rate_F_);
		}
		else
		{
			RecombinationMapConfigError();
		}
	}
#endif
}

#ifndef USE_GSL_POISSON
// determine both the mutation count and the breakpoint count with (usually) a single RNG draw
// this method relies on Eidos_FastRandomPoisson_NONZERO() and cannot be called when USE_GSL_POISSON is defined
inline __attribute__((always_inline)) void Chromosome::DrawMutationAndBreakpointCounts(IndividualSex p_sex, int *p_mut_count, int *p_break_count) const
{
	gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
	double u = Eidos_rng_uniform(rng);
	
	if (single_recombination_map_ && single_mutation_map_)
	{
		// With a single map, we don't care what sex we are passed; same map for all, and sex may be enabled or disabled.
		// We use the _H_ variants of all ivars in this case, which are all that is set up.
		if (u <= probability_both_0_H_)
		{
			*p_mut_count = 0;
			*p_break_count = 0;
		}
		else if (u <= probability_both_0_OR_mut_0_break_non0_H_)
		{
			*p_mut_count = 0;
			*p_break_count = Eidos_FastRandomPoisson_NONZERO(rng, overall_recombination_rate_H_, exp_neg_overall_recombination_rate_H_);
		}
		else if (u <= probability_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0_H_)
		{
			*p_mut_count = Eidos_FastRandomPoisson_NONZERO(rng, overall_mutation_rate_H_, exp_neg_overall_mutation_rate_H_);
			*p_break_count = 0;
		}
		else
		{
			*p_mut_count = Eidos_FastRandomPoisson_NONZERO(rng, overall_mutation_rate_H_, exp_neg_overall_mutation_rate_H_);
			*p_break_count = Eidos_FastRandomPoisson_NONZERO(rng, overall_recombination_rate_H_, exp_neg_overall_recombination_rate_H_);
		}
	}
	else
	{
		// With sex-specific maps, we treat males and females separately, and the individual we're given better be one of the two.
		// We use the _M_ and _F_ variants in this case; either the mutation map or the recombination map might be non-sex-specific,
		// so the _H_ variants were originally set up, but InitializeDraws() copies them into the _M_ and _F_ variants for us
		// so that we don't have to worry about finding the correct variant for each subcase of single/double maps.
		if (p_sex == IndividualSex::kMale)
		{
			if (u <= probability_both_0_M_)
			{
				*p_mut_count = 0;
				*p_break_count = 0;
			}
			else if (u <= probability_both_0_OR_mut_0_break_non0_M_)
			{
				*p_mut_count = 0;
				*p_break_count = Eidos_FastRandomPoisson_NONZERO(rng, overall_recombination_rate_M_, exp_neg_overall_recombination_rate_M_);
			}
			else if (u <= probability_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0_M_)
			{
				*p_mut_count = Eidos_FastRandomPoisson_NONZERO(rng, overall_mutation_rate_M_, exp_neg_overall_mutation_rate_M_);
				*p_break_count = 0;
			}
			else
			{
				*p_mut_count = Eidos_FastRandomPoisson_NONZERO(rng, overall_mutation_rate_M_, exp_neg_overall_mutation_rate_M_);
				*p_break_count = Eidos_FastRandomPoisson_NONZERO(rng, overall_recombination_rate_M_, exp_neg_overall_recombination_rate_M_);
			}
		}
		else if (p_sex == IndividualSex::kFemale)
		{
			if (u <= probability_both_0_F_)
			{
				*p_mut_count = 0;
				*p_break_count = 0;
			}
			else if (u <= probability_both_0_OR_mut_0_break_non0_F_)
			{
				*p_mut_count = 0;
				*p_break_count = Eidos_FastRandomPoisson_NONZERO(rng, overall_recombination_rate_F_, exp_neg_overall_recombination_rate_F_);
			}
			else if (u <= probability_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0_F_)
			{
				*p_mut_count = Eidos_FastRandomPoisson_NONZERO(rng, overall_mutation_rate_F_, exp_neg_overall_mutation_rate_F_);
				*p_break_count = 0;
			}
			else
			{
				*p_mut_count = Eidos_FastRandomPoisson_NONZERO(rng, overall_mutation_rate_F_, exp_neg_overall_mutation_rate_F_);
				*p_break_count = Eidos_FastRandomPoisson_NONZERO(rng, overall_recombination_rate_F_, exp_neg_overall_recombination_rate_F_);
			}
		}
		else
		{
			RecombinationMapConfigError();
		}
	}
}
#endif

// defer this include until now, to resolve mutual dependencies
#include "haplosome.h"

inline __attribute__((always_inline)) Haplosome *Chromosome::NewHaplosome_NULL(Individual *p_individual)
{
	if (haplosomes_junkyard_null.size())
	{
		Haplosome *back = haplosomes_junkyard_null.back();
		haplosomes_junkyard_null.pop_back();
		
		//back->chromosome_index_ = index_;		// guaranteed already set
		back->individual_ = p_individual;
		return back;
	}
	
	return _NewHaplosome_NULL(p_individual);
}

inline __attribute__((always_inline)) Haplosome *Chromosome::NewHaplosome_NONNULL(Individual *p_individual)
{
	if (haplosomes_junkyard_nonnull.size())
	{
		Haplosome *back = haplosomes_junkyard_nonnull.back();
		haplosomes_junkyard_nonnull.pop_back();
		
		// Conceptually, we want to call ReinitializeHaplosomeNoClear() to set the haplosome up with the
		// current type, mutrun setup, etc.  But we know that the haplosome is nonnull, and that we
		// want it to be nonnull, so we can do less work than ReinitializeHaplosomeNoClear(), inline.
		if (back->mutrun_count_ != mutrun_count_)
		{
			// the number of mutruns has changed; need to reallocate
			if (back->mutruns_ != back->run_buffer_)
				free(back->mutruns_);
			
			back->mutrun_count_ = mutrun_count_;
			back->mutrun_length_ = mutrun_length_;
			
			if (mutrun_count_ <= SLIM_HAPLOSOME_MUTRUN_BUFSIZE)
			{
				back->mutruns_ = back->run_buffer_;
#if SLIM_CLEAR_HAPLOSOMES
				EIDOS_BZERO(back->run_buffer_, SLIM_HAPLOSOME_MUTRUN_BUFSIZE * sizeof(const MutationRun *));
#endif
			}
			else
			{
#if SLIM_CLEAR_HAPLOSOMES
				back->mutruns_ = (const MutationRun **)calloc(mutrun_count_, sizeof(const MutationRun *));
#else
				back->mutruns_ = (const MutationRun **)malloc(mutrun_count_ * sizeof(const MutationRun *));
#endif
			}
		}
		else
		{
#if SLIM_CLEAR_HAPLOSOMES
			// the number of mutruns is unchanged, but we need to zero out the reused buffer here
			if (mutrun_count_ <= SLIM_HAPLOSOME_MUTRUN_BUFSIZE)
				EIDOS_BZERO(back->run_buffer_, SLIM_HAPLOSOME_MUTRUN_BUFSIZE * sizeof(const MutationRun *));		// much faster because optimized at compile time
			else
				EIDOS_BZERO(back->mutruns_, mutrun_count_ * sizeof(const MutationRun *));
#endif
		}
		
		//back->chromosome_index_ = index_;		// guaranteed already set
		back->individual_ = p_individual;
		return back;
	}
	
	return _NewHaplosome_NONNULL(p_individual);
}

// Frees a haplosome object (puts it in one of the junkyards); we do not clear the mutrun buffer, so it must be cleared when reused!
inline __attribute__((always_inline)) void Chromosome::FreeHaplosome(Haplosome *p_haplosome)
{
#if DEBUG
	p_haplosome->individual_ = nullptr;		// crash if anybody tries to use this pointer after the free
#endif
	
	// somebody needs to reset the tag value of reused haplosomes; it might as well be us
	// this used to be done by Individual::Individual(), which got passed the individual's haplosomes
#warning this should only get cleared, in bulk, based on a flag, like individual tag values; big waste of time; see s_any_haplosome_tag_set_, we are already doing this in SwapChildAndParentHaplosomes() for WF
	p_haplosome->tag_value_ = SLIM_TAG_UNSET_VALUE;
	
	if (p_haplosome->IsNull())
		haplosomes_junkyard_null.emplace_back(p_haplosome);
	else
		haplosomes_junkyard_nonnull.emplace_back(p_haplosome);
}


class Chromosome_Class : public EidosDictionaryRetained_Class
{
private:
	typedef EidosDictionaryRetained_Class super;

public:
	Chromosome_Class(const Chromosome_Class &p_original) = delete;	// no copy-construct
	Chromosome_Class& operator=(const Chromosome_Class&) = delete;	// no copying
	inline Chromosome_Class(const std::string &p_class_name, EidosClass *p_superclass) : super(p_class_name, p_superclass) { }
	
	virtual const std::vector<EidosPropertySignature_CSP> *Properties(void) const override;
	virtual const std::vector<EidosMethodSignature_CSP> *Methods(void) const override;
};


#endif /* defined(__SLiM__chromosome__) */




































































