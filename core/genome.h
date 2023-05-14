//
//  genome.h
//  SLiM
//
//  Created by Ben Haller on 12/13/14.
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
 
 The class Genome represents a particular genome, defined as a vector of mutations.  Each individual in the simulation has a genome,
 which determines that individual's fitness (from the fitness effects of all of the mutations possessed).
 
 */

#ifndef __SLiM__genome__
#define __SLiM__genome__


#include "mutation.h"
#include "slim_globals.h"
#include "eidos_value.h"
#include "chromosome.h"
#include "mutation_run.h"

#include <vector>
#include <string.h>
#include <unordered_map>

//TREE SEQUENCE
//INCLUDE JEROME's TABLES API
#include "../treerec/tskit/tables.h"

#include "eidos_globals.h"
#if EIDOS_ROBIN_HOOD_HASHING
#include "robin_hood.h"
typedef robin_hood::unordered_flat_map<const MutationRun*, const MutationRun*> SLiMBulkOperationHashTable;
typedef robin_hood::pair<const MutationRun*, const MutationRun*> SLiMBulkOperationPair;
#elif STD_UNORDERED_MAP_HASHING
#include <unordered_map>
typedef std::unordered_map<const MutationRun*, const MutationRun*> SLiMBulkOperationHashTable;
typedef std::pair<const MutationRun*, const MutationRun*> SLiMBulkOperationPair;
#endif


class Species;
class Population;
class Subpopulation;
class Individual;
class GenomeWalker;


extern EidosClass *gSLiM_Genome_Class;


// Genome now keeps an array of MutationRun objects, and those objects actually hold the mutations of the Genome.  This design
// allows multiple Genome objects to share the same runs of mutations, for speed in copying runs during offspring generation.
// The number of runs can be determined at runtime, ideally as a function of the chromosome length, mutation rate, and recombination
// rate; the goal is to make it so runs are short enough that events inside them are relatively rare (allowing them to be copied
// often during reproduction), but long enough that they contain enough mutations to make all this machinery worthwhile; if they
// are usually empty then we're actually doing more work than we were before!

// Each genome knows the number of runs and the run length, but it is the same for every genome in a given simulation.  The whole
// simulation can be scaled up or down by splitting or joining mutation runs; this is done by the mutation-run experiment code.

// The array of runs is malloced; since this is two mallocs per individual, this should not be unacceptable overhead, and it avoids
// hard-coding of a maximum number of runs, wasting memory on unused pointers, etc.  For null genomes the runs pointer is nullptr,
// so if we screw up our null genome checks we will get a hard crash, which is not a bad thing.

// BCH 4/19/2023: Note that Genome's MutationRun objects are now handled using const pointers.  This reflects the fact that, in
// general, MutationRuns in a Genome may be shared with other Genomes, and so should not be modified.  The underlying objects are
// not const, in fact, they are just handled through const pointers to enforce immutability once they are added to a Genome.  This
// means that there are certain places in the code where we cast away the constness, when we know that a MutationRun is not, in
// fact, shared by more than one Genome; but in general we do not do that, because if the run is shared then modifying it is unsafe.
// See mutation_run.h for further comments on this scheme, which replaces a previous scheme based on the refcounts of the runs.

// BCH 5 May 2017: Well, it turns out that allocating the array of runs is in fact substantial overhead in some cases, so let's try
// to avoid it.  We can keep an internal buffer of mutation run pointers, which we can use as long as we are within the buffer size.
// Using a size of 1 for now, since larger sizes increase memory usage substantially for some models, and also slow us down somehow.
// BCH 22 April 2023: I tried removing this internal buffer, to see if it is really worthwhile.  It is.  I think having the pointer
// to the MutationRun in the same memory block really helps a lot with memory locality.
#define SLIM_GENOME_MUTRUN_BUFSIZE 1


class Genome : public EidosObject
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.
	
private:
	typedef EidosObject super;

private:
	EidosValue_SP self_value_;										// cached EidosValue object for speed
	
#ifdef SLIMGUI
public:
#else
private:
#endif
	
	GenomeType genome_type_ = GenomeType::kAutosome;				// SEX ONLY: the type of chromosome represented by this genome
	int8_t scratch_;												// temporary scratch space that can be used locally in algorithms
	
	int32_t mutrun_count_;											// number of runs being used; 0 for a null genome, otherwise >= 1
	slim_position_t mutrun_length_;									// the length, in base pairs, of each run; the last run may not use its full length
	const MutationRun *run_buffer_[SLIM_GENOME_MUTRUN_BUFSIZE];		// an internal buffer used to avoid allocation and memory nonlocality for simple models
	const MutationRun **mutruns_;									// mutation runs; nullptr if a null genome OR an empty genome
	
	Individual *individual_;										// NOT OWNED: the Individual this genome belongs to
	slim_usertag_t tag_value_;										// a user-defined tag value
	
	// TREE SEQUENCE RECORDING
	slim_genomeid_t genome_id_;		// a unique id assigned by SLiM, as a side effect of pedigree recording, that never changes
	tsk_id_t tsk_node_id_;			// tskit's tsk_id_t for this genome, which is its index in the nodes table kept by the tree-seq code.
	
	// Bulk operation optimization; see WillModifyRunForBulkOperation().  The idea is to keep track of changes to MutationRun
	// objects in a bulk operation, and short-circuit the operation for all Genomes with the same initial MutationRun (since
	// the bulk operation will produce the same product MutationRun given the same initial MutationRun).  Note this is shared by all species.
	static int64_t s_bulk_operation_id_;
	static slim_mutrun_index_t s_bulk_operation_mutrun_index_;
	static SLiMBulkOperationHashTable s_bulk_operation_runs_;
	
public:
	
	Genome(const Genome &p_original) = delete;
	Genome& operator= (const Genome &p_original) = delete;
	
	// make a null genome
	explicit inline Genome(GenomeType p_genome_type_) :
		genome_type_(p_genome_type_), mutrun_count_(0), mutrun_length_(0), mutruns_(nullptr), individual_(nullptr), genome_id_(-1)
	{
	};
	
	// make a non-null genome
	inline Genome(int p_mutrun_count, slim_position_t p_mutrun_length, GenomeType p_genome_type_) :
		genome_type_(p_genome_type_), mutrun_count_(p_mutrun_count), mutrun_length_(p_mutrun_length), individual_(nullptr), genome_id_(-1)
	{
		if (mutrun_count_ <= SLIM_GENOME_MUTRUN_BUFSIZE)
		{
			mutruns_ = run_buffer_;
			EIDOS_BZERO(run_buffer_, SLIM_GENOME_MUTRUN_BUFSIZE * sizeof(const MutationRun *));
		}
		else
			mutruns_ = (const MutationRun **)calloc(mutrun_count_, sizeof(const MutationRun *));
	};
	
	~Genome(void);
	
	inline __attribute__((always_inline)) slim_genomeid_t GenomeID(void)			{ return genome_id_; }
	inline __attribute__((always_inline)) void SetGenomeID(slim_genomeid_t p_new_id){ genome_id_ = p_new_id; }	// should basically never be called
	inline __attribute__((always_inline)) Individual *OwningIndividual(void)		{ return individual_; }
	
	void NullGenomeAccessError(void) const __attribute__((__noreturn__)) __attribute__((cold)) __attribute__((analyzer_noreturn));		// prints an error message, a stacktrace, and exits; called only for DEBUG
	
	inline __attribute__((always_inline)) bool IsNull(void) const									// returns true if the genome is a null (placeholder) genome, false otherwise
	{
		return (mutrun_count_ == 0);																	// null genomes have a mutrun count of 0
	}
	inline __attribute__((always_inline)) bool IsDeferred(void) const								// returns true if the genome is deferred genome (not yet generated), false otherwise
	{
		return ((mutrun_count_ != 0) && mutruns_ && !mutruns_[0]);										// when deferred, non-null genomes have a non-zero mutrun count but are cleared to nullptr
	}
	
	void MakeNull(void) __attribute__((cold));	// transform into a null genome
	
	// used to re-initialize Genomes to a new state, reusing them for efficiency
	void ReinitializeGenomeToMutruns(GenomeType p_genome_type, int32_t p_mutrun_count, slim_position_t p_mutrun_length, const std::vector<MutationRun *> &p_runs);
	void ReinitializeGenomeNullptr(GenomeType p_genome_type, int32_t p_mutrun_count, slim_position_t p_mutrun_length);
	
	// This should be called before starting to define a mutation run from scratch, as the crossover-mutation code does.  It will
	// discard the current MutationRun and start over from scratch with a unique, new MutationRun which is returned by the call.
	// Note that there is a _LOCKED version of this below, which locks around the use of the allocation pool.
	inline MutationRun *WillCreateRun(int p_run_index, MutationRunContext &p_mutrun_context)
	{
#if DEBUG
		if (p_run_index < 0)
			EIDOS_TERMINATION << "ERROR (Genome::WillCreateRun): (internal error) attempt to create a negative-index run." << EidosTerminate();
		if (p_run_index >= mutrun_count_)
			EIDOS_TERMINATION << "ERROR (Genome::WillCreateRun): (internal error) attempt to create an out-of-index run." << EidosTerminate();
#endif
		
		MutationRun *new_run = MutationRun::NewMutationRun(p_mutrun_context);	// take from shared pool of used objects
		
		mutruns_[p_run_index] = new_run;
		return new_run;
	}
	
	inline MutationRun *WillCreateRun_LOCKED(int p_run_index, MutationRunContext &p_mutrun_context)
	{
#if DEBUG
		if (p_run_index < 0)
			EIDOS_TERMINATION << "ERROR (Genome::WillCreateRun_LOCKED): (internal error) attempt to create a negative-index run." << EidosTerminate();
		if (p_run_index >= mutrun_count_)
			EIDOS_TERMINATION << "ERROR (Genome::WillCreateRun_LOCKED): (internal error) attempt to create an out-of-index run." << EidosTerminate();
#endif
		
		MutationRun *new_run = MutationRun::NewMutationRun_LOCKED(p_mutrun_context);	// take from shared pool of used objects
		
		mutruns_[p_run_index] = new_run;
		return new_run;
	}
	
	
	// This should be called before modifying the run at a given index.  It will replicate the run to produce a single-referenced copy
	// if necessary, thus guaranteeting that the run can be modified legally.  If the run is already single-referenced, it is a no-op.
	MutationRun *WillModifyRun(slim_mutrun_index_t p_run_index, MutationRunContext &p_mutrun_context);
	
	// This is an alternate version of WillModifyRun().  It labels the upcoming modification as being the result of a bulk operation
	// being applied across multiple genomes, such that identical input genomes will produce identical output genomes, such as adding
	// the same mutation to all target genomes.  It returns T if the caller needs to actually perform the operation on this genome,
	// or F if this call performed the run for the caller (because the operation had already been performed on an identical genome).
	// The goal is that genomes that share the same mutation run should continue to share the same mutation run after being processed
	// by a bulk operation using this method.  A bit strange, but potentially important for efficiency.  Note that this method knows
	// nothing about the operation being performed; it just plays around with MutationRun pointers, recognizing when the runs are
	// identical.  The first call for a new operation ID will always return a pointer, and the caller will then perform the operation;
	// subsequent calls for genomes with the same starting MutationRun will substitute the same final MutationRun and return nullptr.
	static void BulkOperationStart(int64_t p_operation_id, slim_mutrun_index_t p_mutrun_index);
	MutationRun *WillModifyRunForBulkOperation(int64_t p_operation_id, slim_mutrun_index_t p_mutrun_index, MutationRunContext &p_mutrun_context);
	static void BulkOperationEnd(int64_t p_operation_id, slim_mutrun_index_t p_mutrun_index);
	
	inline __attribute__((always_inline)) GenomeType Type(void) const			// returns the type of the genome: automosomal, X chromosome, or Y chromosome
	{
		return genome_type_;
	}
	
	// Remove all mutations in p_genome that have a state_ of MutationState::kFixedAndSubstituted, indicating that they have fixed
	void RemoveFixedMutations(int64_t p_operation_id, slim_mutrun_index_t p_mutrun_index)
	{
#if DEBUG
		if (mutrun_count_ == 0)
			NullGenomeAccessError();
#endif
		// Remove all fixed mutations from the given mutation run index.  Note that we cast away the const on our
		// mutation runs here.  This method is called only when fixed mutations are being removed from *all* genomes,
		// so the fact that it modifies other genomes that share this mutation run is a feature, not a bug.  See
		// Population::RemoveAllFixedMutations() for further context on this.
		MutationRun *mutrun = const_cast<MutationRun *>(mutruns_[p_mutrun_index]);
		
		mutrun->RemoveFixedMutations(p_operation_id);
	}
	
	// TallyGenomeReferences_Checkback() counts up the total MutationRun references, using their usage counts, as a checkback
	void TallyGenomeReferences_Checkback(slim_refcount_t *p_mutrun_ref_tally, slim_refcount_t *p_mutrun_tally, int64_t p_operation_id);
	
	inline __attribute__((always_inline)) int mutation_count(void) const	// used to be called size(); renamed to avoid confusion with MutationRun::size() and break code using the wrong method
	{
#if DEBUG
		if (mutrun_count_ == 0)
			NullGenomeAccessError();
#endif
		if (mutrun_count_ == 1)
		{
			return run_buffer_[0]->size();
		}
		else
		{
			int mut_count = 0;
			
			for (int run_index = 0; run_index < mutrun_count_; ++run_index)
				mut_count += mutruns_[run_index]->size();
			
			return mut_count;
		}
	}
	
	inline __attribute__((always_inline)) void clear_to_nullptr(void)
	{
		// It is legal to call this method on null genomes, for speed/simplicity; it does no harm
		// That is because it zeroes run_buffer_, even for null genomes; it isn't worth the time to check for a null genome
		if (mutrun_count_ <= SLIM_GENOME_MUTRUN_BUFSIZE)
			EIDOS_BZERO(run_buffer_, SLIM_GENOME_MUTRUN_BUFSIZE * sizeof(const MutationRun *));		// much faster because optimized at compile time
		else
			EIDOS_BZERO(mutruns_, mutrun_count_ * sizeof(const MutationRun *));
	}
	
	inline void check_cleared_to_nullptr(void)
	{
		// It is legal to call this method on null genomes, for speed/simplicity; it does no harm
		for (int run_index = 0; run_index < mutrun_count_; ++run_index)
			if (mutruns_[run_index])
				EIDOS_TERMINATION << "ERROR (Genome::check_cleared_to_nullptr): (internal error) genome should be cleared but is not." << EidosTerminate();
	}
	
	inline __attribute__((always_inline)) bool contains_mutation(MutationIndex p_mutation_index)
	{
#if DEBUG
		if (mutrun_count_ == 0)
			NullGenomeAccessError();
#endif
		return mutruns_[(gSLiM_Mutation_Block + p_mutation_index)->position_ / mutrun_length_]->contains_mutation(p_mutation_index);
	}
	
	inline __attribute__((always_inline)) Mutation *mutation_with_type_and_position(MutationType *p_mut_type, slim_position_t p_position, slim_position_t p_last_position)
	{
#if DEBUG
		if (mutrun_count_ == 0)
			NullGenomeAccessError();
#endif
		return mutruns_[p_position / mutrun_length_]->mutation_with_type_and_position(p_mut_type, p_position, p_last_position);
	}
	
	inline void copy_from_genome(const Genome &p_source_genome)
	{
		if (p_source_genome.IsNull())
		{
			// p_original is a null genome, so make ourselves null too, if we aren't already
			MakeNull();
		}
		else
		{
#if DEBUG
			if (mutrun_count_ == 0)
				NullGenomeAccessError();
#endif
#if DEBUG
			if ((mutrun_count_ != p_source_genome.mutrun_count_) || (mutrun_length_ != p_source_genome.mutrun_length_))
				EIDOS_TERMINATION << "ERROR (Genome::copy_from_genome): (internal error) assignment from genome with different count/length." << EidosTerminate();
#endif
			
			if (mutrun_count_ == 1)
			{
				// This optimization does seem to make a significant difference...
				run_buffer_[0] = p_source_genome.mutruns_[0];
			}
			else
			{
				memcpy(mutruns_, p_source_genome.mutruns_, mutrun_count_ * sizeof(const MutationRun *));
			}
		}
		
		// and copy other state
		genome_type_ = p_source_genome.genome_type_;
		
		// DO NOT copy the subpop pointer!  That is not part of the genetic state of the genome,
		// it's a back-pointer to the Subpopulation that owns this genome, and never changes!
		// subpop_ = p_source_genome.subpop_;
	}
	
	inline const std::vector<Mutation *> *derived_mutation_ids_at_position(slim_position_t p_position) const
	{
		slim_mutrun_index_t run_index = (slim_mutrun_index_t)(p_position / mutrun_length_);
		
		return mutruns_[run_index]->derived_mutation_ids_at_position(p_position);
	}
	
	void record_derived_states(Species *p_species) const;
	
	// print the sample represented by genomes, using SLiM's own format
	static void PrintGenomes_SLiM(std::ostream &p_out, std::vector<Genome *> &p_genomes, slim_objectid_t p_source_subpop_id);
	
	// print the sample represented by genomes, using "ms" format
	static void PrintGenomes_MS(std::ostream &p_out, std::vector<Genome *> &p_genomes, const Chromosome &p_chromosome, bool p_filter_monomorphic);
	
	// print the sample represented by genomes, using "vcf" format
	static void PrintGenomes_VCF(std::ostream &p_out, std::vector<Genome *> &p_genomes, bool p_output_multiallelics, bool p_simplify_nucs, bool p_output_nonnucs, bool p_nucleotide_based, NucleotideArray *p_ancestral_seq);
	
	// Memory usage tallying, for outputUsage()
	size_t MemoryUsageForMutrunBuffers(void);
	
	
	//
	// Eidos support
	//
	void GenerateCachedEidosValue(void);
	inline __attribute__((always_inline)) EidosValue_SP CachedEidosValue(void) { if (!self_value_) GenerateCachedEidosValue(); return self_value_; };
	
	virtual const EidosClass *Class(void) const override;
	virtual void Print(std::ostream &p_ostream) const override;
	
	virtual EidosValue_SP GetProperty(EidosGlobalStringID p_property_id) override;
	virtual void SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value) override;
	
	virtual EidosValue_SP ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) override;
	static EidosValue_SP ExecuteMethod_Accelerated_containsMarkerMutation(EidosObject **p_values, size_t p_values_size, EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	static EidosValue_SP ExecuteMethod_Accelerated_containsMutations(EidosObject **p_values, size_t p_values_size, EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	static EidosValue_SP ExecuteMethod_Accelerated_countOfMutationsOfType(EidosObject **p_values, size_t p_values_size, EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_mutationsOfType(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_nucleotides(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_positionsOfMutationsOfType(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_sumOfMutationsOfType(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	
	// Accelerated property access; see class EidosObject for comments on this mechanism
	static EidosValue *GetProperty_Accelerated_genomePedigreeID(EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_isNullGenome(EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_tag(EidosObject **p_values, size_t p_values_size);
	
	// Accelerated property writing; see class EidosObject for comments on this mechanism
	static void SetProperty_Accelerated_tag(EidosObject **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size);
	
	friend class Genome_Class;
	
	
	// With the new mutation run structure, the simplest course of action is to just let some SLiM classes delve
	// in Genome directly; we really don't want to get into trying to define an iterator that loops over mutation
	// runs, etc., transparently and pretends that a genome is just a single bag of mutations.
	// FIXME well, now we do in fact have the GenomeWalker iterator class below, which works nicely, so some
	// of the code that messes around inside Genome's internals can probably be switched over to using it...
	friend Species;
	friend Population;
	friend Subpopulation;
	friend Individual;
	friend GenomeWalker;
};

class Genome_Class : public EidosClass
{
private:
	typedef EidosClass super;

public:
	Genome_Class(const Genome_Class &p_original) = delete;	// no copy-construct
	Genome_Class& operator=(const Genome_Class&) = delete;	// no copying
	inline Genome_Class(const std::string &p_class_name, EidosClass *p_superclass) : super(p_class_name, p_superclass) { }
	
	virtual const std::vector<EidosPropertySignature_CSP> *Properties(void) const override;
	virtual const std::vector<EidosMethodSignature_CSP> *Methods(void) const override;
	
	virtual EidosValue_SP ExecuteClassMethod(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) const override;
	EidosValue_SP ExecuteMethod_addMutations(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) const;
	EidosValue_SP ExecuteMethod_addNewMutation(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) const;
	EidosValue_SP ExecuteMethod_mutationFreqsCountsInGenomes(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) const;
	EidosValue_SP ExecuteMethod_outputX(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) const;
	EidosValue_SP ExecuteMethod_readFromMS(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) const;
	EidosValue_SP ExecuteMethod_readFromVCF(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) const;
	EidosValue_SP ExecuteMethod_removeMutations(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) const;
};

// This class allows clients of Genome to walk the mutations inside a Genome without needing to know about MutationRun.
class GenomeWalker
{
private:
	Genome *genome_;							// the genome being walked
	int32_t mutrun_index_;						// the mutation run index we're presently traversing
	const MutationIndex *mutrun_ptr_;			// a pointer to the current element in the mutation run
	const MutationIndex *mutrun_end_;			// an end pointer for the mutation run
	Mutation *mutation_;						// the current mutation pointer, or nullptr if we have reached the end of the genome
	
public:
	GenomeWalker(void) = delete;
	GenomeWalker(const GenomeWalker &p_original) = default;
	GenomeWalker& operator= (const GenomeWalker &p_original) = default;
	
	inline GenomeWalker(Genome *p_genome) : genome_(p_genome), mutrun_index_(-1), mutrun_ptr_(nullptr), mutrun_end_(nullptr), mutation_(nullptr) { NextMutation(); };
	GenomeWalker(GenomeWalker&&) = default;
	inline ~GenomeWalker(void) {};
	
	inline Genome *WalkerGenome(void) { return genome_; }
	inline Mutation *CurrentMutation(void) { return mutation_; }
	inline bool Finished(void) { return (mutation_ == nullptr); }
	inline slim_position_t Position(void) { return mutation_->position_; }		// must be sure the walker is not finished!
	
	void NextMutation(void);
	void MoveToPosition(slim_position_t p_position);
	bool MutationIsStackedAtCurrentPosition(Mutation *p_search_mut);	// scans for the given mutation in any slot at the current position
	bool IdenticalAtCurrentPositionTo(GenomeWalker &p_other_walker);	// compares stacked mutations between walkers (reordered is not identical)
	int8_t NucleotideAtCurrentPosition(void);
};


#endif /* defined(__SLiM__genome__) */




































































