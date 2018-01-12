//
//  genome.h
//  SLiM
//
//  Created by Ben Haller on 12/13/14.
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
 
 The class Genome represents a particular genome, defined as a vector of mutations.  Each individual in the simulation has a genome,
 which determines that individual's fitness (from the fitness effects of all of the mutations possessed).
 
 */

#ifndef __SLiM__genome__
#define __SLiM__genome__


#include "mutation.h"
#include "slim_global.h"
#include "eidos_value.h"
#include "chromosome.h"
#include "mutation_run.h"

#include <vector>
#include <string.h>


class SLiMSim;
class Population;
class Subpopulation;
class Individual;


extern EidosObjectClass *gSLiM_Genome_Class;


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

// BCH 5 May 2017: Well, it turns out that allocating the array of runs is in fact substantial overhead in some cases, so let's try
// to avoid it.  We can keep an internal buffer of mutation run pointers, which we can use as long as we are within the buffer size.
// Using a size of 1 for now, since larger sizes increase memory usage substantially for some models, and also slow us down somehow.
#define SLIM_GENOME_MUTRUN_BUFSIZE 1


class Genome : public EidosObjectElement
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.
	
	EidosValue_SP self_value_;									// cached EidosValue object for speed
	
	Genome *patch_pointer_;										// used only by Subpopulation::ExecuteMethod_takeMigrants(); see that method
	
#ifdef SLIMGUI
public:
#else
private:
#endif
	
	GenomeType genome_type_ = GenomeType::kAutosome;			// SEX ONLY: the type of chromosome represented by this genome
	Subpopulation *subpop_;										// NOT OWNED: the Subpopulation this genome belongs to
	
	int32_t mutrun_count_;										// number of runs being used; 0 for a null genome, otherwise >= 1
	int32_t mutrun_length_;										// the length, in base pairs, of each run; the last run may not use its full length
	MutationRun_SP run_buffer_[SLIM_GENOME_MUTRUN_BUFSIZE];		// an internal buffer used to avoid allocation and memory nonlocality for simple models
	MutationRun_SP *mutruns_;									// mutation runs; nullptr if a null genome OR an empty genome
	
	slim_usertag_t tag_value_;									// a user-defined tag value
	
	// Bulk operation optimization; see WillModifyRunForBulkOperation().  The idea is to keep track of changes to MutationRun
	// objects in a bulk operation, and short-circuit the operation for all Genomes with the same initial MutationRun (since
	// the bulk operation will produce the same product MutationRun given the same initial MutationRun).
	static int64_t s_bulk_operation_id_;
	static int s_bulk_operation_mutrun_index_;
	static std::map<MutationRun*, MutationRun*> s_bulk_operation_runs_;
	
public:
	
	Genome(const Genome &p_original) = delete;
	Genome& operator= (const Genome &p_original) = delete;
	Genome(Subpopulation *p_subpop, int p_mutrun_count, int p_mutrun_length, GenomeType p_genome_type_, bool p_is_null);
	~Genome(void);
	
	void NullGenomeAccessError(void) const __attribute__((__noreturn__)) __attribute__((cold)) __attribute__((analyzer_noreturn));		// prints an error message, a stacktrace, and exits; called only for DEBUG
	
	inline bool IsNull(void) const									// returns true if the genome is a null (placeholder) genome, false otherwise
	{
		return (mutrun_count_ == 0);
	}
	
	void MakeNull(void);	// transform into a null genome
	
	// used to re-initialize Genomes to a new state, reusing them for efficiency
	void ReinitializeGenomeToMutrun(GenomeType p_genome_type, int32_t p_mutrun_count, int32_t p_mutrun_length, MutationRun *p_run);
	void ReinitializeGenomeNullptr(GenomeType p_genome_type, int32_t p_mutrun_count, int32_t p_mutrun_length);
	
	// This should be called before starting to define a mutation run from scratch, as the crossover-mutation code does.  It will
	// discard the current MutationRun and start over from scratch with a unique, new MutationRun which is returned by the call.
	inline MutationRun *WillCreateRun(int p_run_index)
	{
#ifdef DEBUG
		if (p_run_index < 0)
			EIDOS_TERMINATION << "ERROR (Genome::WillCreateRun): (internal error) attempt to create a negative-index run." << EidosTerminate();
		if (p_run_index >= mutrun_count_)
			EIDOS_TERMINATION << "ERROR (Genome::WillCreateRun): (internal error) attempt to create an out-of-index run." << EidosTerminate();
#endif
		
		MutationRun *new_run = MutationRun::NewMutationRun();	// take from shared pool of used objects
		
		mutruns_[p_run_index].reset(new_run);
		return new_run;
	}
	
	// This should be called before modifying the run at a given index.  It will replicate the run to produce a single-referenced copy
	// if necessary, thus guaranteeting that the run can be modified legally.  If the run is already single-referenced, it is a no-op.
	void WillModifyRun(int p_run_index);
	
	// This is an alternate version of WillModifyRun().  It labels the upcoming modification as being the result of a bulk operation
	// being applied across multiple genomes, such that identical input genomes will produce identical output genomes, such as adding
	// the same mutation to all target genomes.  It returns T if the caller needs to actually perform the operation on this genome,
	// or F if this call performed the run for the caller (because the operation had already been performed on an identical genome).
	// The goal is that genomes that share the same mutation run should continue to share the same mutation run after being processed
	// by a bulk operation using this method.  A bit strange, but potentially important for efficiency.  Note that this method knows
	// nothing about the operation being performed; it just plays around with MutationRun pointers, recognizing when the runs are
	// identical.  The first call for a new operation ID will always return T, and the caller will then perform the operation;
	// subsequent calls for genomes with the same starting MutationRun will substitute the same final MutationRun and return F.
	static void BulkOperationStart(int64_t p_operation_id, int p_mutrun_index);
	bool WillModifyRunForBulkOperation(int64_t p_operation_id, int p_mutrun_index);
	static void BulkOperationEnd(int64_t p_operation_id, int p_mutrun_index);
	
	GenomeType Type(void) const										// returns the type of the genome: automosomal, X chromosome, or Y chromosome
	{
		return genome_type_;
	}
	
	void RemoveFixedMutations(int64_t p_operation_id, int p_mutrun_index);		// Remove all mutations with a refcount of -1, indicating that they have fixed
	
	// This counts up the total MutationRun references, using their usage counts, as a checkback
	void TallyGenomeReferences(slim_refcount_t *p_mutrun_ref_tally, slim_refcount_t *p_mutrun_tally, int64_t p_operation_id);
	
	// This tallies up individual Mutation references, using MutationRun usage counts for speed
	void TallyGenomeMutationReferences(int64_t p_operation_id);
	
	inline int mutation_count(void) const	// used to be called size(); renamed to avoid confusion with MutationRun::size() and break code using the wrong method
	{
#ifdef DEBUG
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
	
	inline void clear_to_empty(void)
	{
#ifdef DEBUG
		if (mutrun_count_ == 0)
			NullGenomeAccessError();
#endif
		for (int run_index = 0; run_index < mutrun_count_; ++run_index)
		{
			MutationRun_SP *mutrun_sp = mutruns_ + run_index;
			MutationRun *mutrun = mutrun_sp->get();
			
			if (mutrun->size() != 0)
			{
				// If the MutationRun is private to us, we can just empty it out, otherwise we replace it with a new empty one
				if (mutrun->UseCount() == 1)
					mutrun->clear();
				else
					*mutrun_sp = MutationRun_SP(MutationRun::NewMutationRun());
			}
		}
	}
	
	inline void clear_to_nullptr(void)
	{
		// It is legal to call this method on null genomes, for speed/simplicity; it does no harm
		for (int run_index = 0; run_index < mutrun_count_; ++run_index)
			mutruns_[run_index].reset();
	}
	
	inline void check_cleared_to_nullptr(void)
	{
		// It is legal to call this method on null genomes, for speed/simplicity; it does no harm
		for (int run_index = 0; run_index < mutrun_count_; ++run_index)
			if (mutruns_[run_index])
				EIDOS_TERMINATION << "ERROR (Genome::check_cleared_to_nullptr): (internal error) genome should be cleared but is not." << EidosTerminate();
	}
	
	inline bool contains_mutation(MutationIndex p_mutation_index)
	{
#ifdef DEBUG
		if (mutrun_count_ == 0)
			NullGenomeAccessError();
#endif
		return mutruns_[(gSLiM_Mutation_Block + p_mutation_index)->position_ / mutrun_length_]->contains_mutation(p_mutation_index);
	}
	
	inline bool contains_mutation_with_type_and_position(MutationType *p_mut_type, slim_position_t p_position, slim_position_t p_last_position)
	{
#ifdef DEBUG
		if (mutrun_count_ == 0)
			NullGenomeAccessError();
#endif
		return mutruns_[p_position / mutrun_length_]->contains_mutation_with_type_and_position(p_mut_type, p_position, p_last_position);
	}
	
	inline void insert_sorted_mutation(MutationIndex p_mutation_index)
	{
		slim_position_t position = (gSLiM_Mutation_Block + p_mutation_index)->position_;
		int32_t run_index = position / mutrun_length_;
		
		mutruns_[run_index]->insert_sorted_mutation(p_mutation_index);
	}
	
	inline void insert_sorted_mutation_if_unique(MutationIndex p_mutation_index)
	{
		slim_position_t position = (gSLiM_Mutation_Block + p_mutation_index)->position_;
		int32_t run_index = position / mutrun_length_;
		
		mutruns_[run_index]->insert_sorted_mutation_if_unique(p_mutation_index);
	}
	
	inline bool enforce_stack_policy_for_addition(slim_position_t p_position, MutationType *p_mut_type_ptr)
	{
#ifdef DEBUG
		if (mutrun_count_ == 0)
			NullGenomeAccessError();
#endif
		MutationStackPolicy policy = p_mut_type_ptr->stack_policy_;
		
		if (policy == MutationStackPolicy::kStack)
		{
			// If mutations are allowed to stack (the default), then we have no work to do and the new mutation is always added
			return true;
		}
		else
		{
			// Otherwise, a relatively complicated check is needed, so we call out to a non-inline function
			MutationRun *mutrun = mutruns_[p_position / mutrun_length_].get();
			
			return mutrun->_EnforceStackPolicyForAddition(p_position, policy, p_mut_type_ptr->stack_group_);
		}
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
#ifdef DEBUG
			if (mutrun_count_ == 0)
				NullGenomeAccessError();
#endif
#ifdef DEBUG
			if ((mutrun_count_ != p_source_genome.mutrun_count_) || (mutrun_length_ != p_source_genome.mutrun_length_))
				EIDOS_TERMINATION << "ERROR (Genome::copy_from_genome): (internal error) assignment from genome with different count/length." << EidosTerminate();
#endif
			
			if (mutrun_count_ == 1)
			{
				// This does seem to make a significant difference, interestingly.  Not sure if it is
				// avoid the for loop, or avoiding the extra indirection through mutruns_, or what...
				run_buffer_[0] = p_source_genome.mutruns_[0];
			}
			else
			{
				for (int run_index = 0; run_index < mutrun_count_; ++run_index)
					mutruns_[run_index] = p_source_genome.mutruns_[run_index];
			}
		}
		
		// and copy other state
		genome_type_ = p_source_genome.genome_type_;
		
		// DO NOT copy the subpop pointer!  That is not part of the genetic state of the genome,
		// it's a back-pointer to the Subpopulation that owns this genome, and never changes!
		// subpop_ = p_source_genome.subpop_;
	}
	
	//void assert_identical_to_runs(MutationRun_SP *p_mutruns, int32_t p_mutrun_count);
	
	// print the sample represented by genomes, using SLiM's own format
	static void PrintGenomes_SLiM(std::ostream &p_out, std::vector<Genome *> &p_genomes, slim_objectid_t p_source_subpop_id);
	
	// print the sample represented by genomes, using "ms" format
	static void PrintGenomes_MS(std::ostream &p_out, std::vector<Genome *> &p_genomes, const Chromosome &p_chromosome);
	
	// print the sample represented by genomes, using "vcf" format
	static void PrintGenomes_VCF(std::ostream &p_out, std::vector<Genome *> &p_genomes, bool p_output_multiallelics);
	
	
	//
	// Eidos support
	//
	void GenerateCachedEidosValue(void);
	inline EidosValue_SP CachedEidosValue(void) { if (!self_value_) GenerateCachedEidosValue(); return self_value_; };
	
	virtual const EidosObjectClass *Class(void) const;
	virtual void Print(std::ostream &p_ostream) const;
	
	virtual EidosValue_SP GetProperty(EidosGlobalStringID p_property_id);
	virtual void SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value);
	
	virtual EidosValue_SP ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_containsMarkerMutation(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_containsMutations(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_countOfMutationsOfType(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_mutationsOfType(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_positionsOfMutationsOfType(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_sumOfMutationsOfType(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	
	// Accelerated property access; see class EidosObjectElement for comments on this mechanism
	virtual eidos_logical_t GetProperty_Accelerated_Logical(EidosGlobalStringID p_property_id);
	virtual int64_t GetProperty_Accelerated_Int(EidosGlobalStringID p_property_id);
	
	// Accelerated property writing; see class EidosObjectElement for comments on this mechanism
	virtual void SetProperty_Accelerated_Int(EidosGlobalStringID p_property_id, int64_t p_value);
	
	friend class Genome_Class;
	
	
	// With the new mutation run structure, the simplest course of action is to just let some SLiM classes delve
	// in Genome directly; we really don't want to get into trying to define an iterator that loops over mutation
	// runs, etc., transparently and pretends that a genome is just a single bag of mutations.
	friend SLiMSim;
	friend Population;
	friend Subpopulation;
	friend Individual;
};


#endif /* defined(__SLiM__genome__) */




































































