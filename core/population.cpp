//
//  population.cpp
//  SLiM
//
//  Created by Ben Haller on 12/13/14.
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


#include "population.h"

#include <fstream>
#include <iomanip>

#include "slim_sim.h"
#include "slim_global.h"
#include "eidos_script.h"
#include "eidos_interpreter.h"
#include "eidos_symbol_table.h"


using std::endl;
using std::string;
using std::multimap;


Population::Population(SLiMSim &p_sim) : sim_(p_sim)
{
}

Population::~Population(void)
{
	RemoveAllSubpopulationInfo();
	
	// dispose of any freed subpops
	for (auto removed_subpop : removed_subpops_)
		delete removed_subpop;
	
	removed_subpops_.clear();
}

void Population::RemoveAllSubpopulationInfo(void)
{
	// Free all subpopulations and then clear out our subpopulation list
	for (auto subpopulation : *this)
		delete subpopulation.second;
	
	this->clear();
	
	// Free all substitutions andclear out the substitution vector
	for (auto substitution : substitutions_)
		delete substitution;
	
	substitutions_.clear();
	
	// The malloced storage of mutation_registry_ will be freed when it is destroyed, but it
	// does not know that the Mutation pointers inside it are owned, so we need to free them.
	Mutation **registry_iter = mutation_registry_.begin_pointer();
	Mutation **registry_iter_end = mutation_registry_.end_pointer();
	
	for (; registry_iter != registry_iter_end; ++registry_iter)
		delete *registry_iter;
	
	mutation_registry_.clear();
	
#ifdef SLIMGUI
	// release malloced storage for SLiMgui statistics collection
	if (mutation_loss_times_)
	{
		free(mutation_loss_times_);
		mutation_loss_times_ = nullptr;
		mutation_loss_gen_slots_ = 0;
	}
	if (mutation_fixation_times_)
	{
		free(mutation_fixation_times_);
		mutation_fixation_times_ = nullptr;
		mutation_fixation_gen_slots_ = 0;
	}
	if (fitness_history_)
	{
		free(fitness_history_);
		fitness_history_ = nullptr;
		fitness_history_length_ = 0;
	}
#endif
}

// add new empty subpopulation p_subpop_id of size p_subpop_size
Subpopulation *Population::AddSubpopulation(slim_objectid_t p_subpop_id, slim_popsize_t p_subpop_size, double p_initial_sex_ratio) 
{ 
	if (count(p_subpop_id) != 0)
		EIDOS_TERMINATION << "ERROR (Population::AddSubpopulation): subpopulation p" << p_subpop_id << " already exists." << eidos_terminate();
	if (p_subpop_size < 1)
		EIDOS_TERMINATION << "ERROR (Population::AddSubpopulation): subpopulation p" << p_subpop_id << " empty." << eidos_terminate();
	
	// make and add the new subpopulation
	Subpopulation *new_subpop = nullptr;
	
	if (sim_.SexEnabled())
		new_subpop = new Subpopulation(*this, p_subpop_id, p_subpop_size, p_initial_sex_ratio, sim_.ModeledChromosomeType(), sim_.XDominanceCoefficient());	// SEX ONLY
	else
		new_subpop = new Subpopulation(*this, p_subpop_id, p_subpop_size);
	
	new_subpop->child_generation_valid_ = child_generation_valid_;	// synchronize its stage with ours
	
#ifdef SLIMGUI
	// When running under SLiMgui, we need to decide whether this subpopulation comes in selected or not.  We can't defer that
	// to SLiMgui's next update, because then mutation tallies are not kept properly up to date, resulting in a bad GUI refresh.
	// The rule is: if all currently existing subpops are selected, then the new subpop comes in selected as well.
	new_subpop->gui_selected_ = true;
	
	for (auto subpop_pair : *this)
	{
		if (!subpop_pair.second->gui_selected_)
		{
			new_subpop->gui_selected_ = false;
			break;
		}
	}
#endif
	
	insert(std::pair<const slim_objectid_t,Subpopulation*>(p_subpop_id, new_subpop));
	
	return new_subpop;
}

// add new subpopulation p_subpop_id of size p_subpop_size individuals drawn from source subpopulation p_source_subpop_id
Subpopulation *Population::AddSubpopulation(slim_objectid_t p_subpop_id, Subpopulation &p_source_subpop, slim_popsize_t p_subpop_size, double p_initial_sex_ratio)
{
	if (count(p_subpop_id) != 0)
		EIDOS_TERMINATION << "ERROR (Population::AddSubpopulation): subpopulation p" << p_subpop_id << " already exists." << eidos_terminate();
	if (p_subpop_size < 1)
		EIDOS_TERMINATION << "ERROR (Population::AddSubpopulation): subpopulation p" << p_subpop_id << " empty." << eidos_terminate();
	
	// make and add the new subpopulation
	Subpopulation *new_subpop = nullptr;
 
	if (sim_.SexEnabled())
		new_subpop = new Subpopulation(*this, p_subpop_id, p_subpop_size, p_initial_sex_ratio, sim_.ModeledChromosomeType(), sim_.XDominanceCoefficient());	// SEX ONLY
	else
		new_subpop = new Subpopulation(*this, p_subpop_id, p_subpop_size);
	
	new_subpop->child_generation_valid_ = child_generation_valid_;	// synchronize its stage with ours
	
#ifdef SLIMGUI
	// When running under SLiMgui, we need to decide whether this subpopulation comes in selected or not.  We can't defer that
	// to SLiMgui's next update, because then mutation tallies are not kept properly up to date, resulting in a bad GUI refresh.
	// The rule is: if all currently existing subpops are selected, then the new subpop comes in selected as well.
	new_subpop->gui_selected_ = true;
	
	for (auto subpop_pair : *this)
	{
		if (!subpop_pair.second->gui_selected_)
		{
			new_subpop->gui_selected_ = false;
			break;
		}
	}
#endif
	
	insert(std::pair<const slim_objectid_t,Subpopulation*>(p_subpop_id, new_subpop));
	
	// then draw parents from the source population according to fitness, obeying the new subpop's sex ratio
	Subpopulation &subpop = *new_subpop;
	
	for (slim_popsize_t parent_index = 0; parent_index < subpop.parent_subpop_size_; parent_index++)
	{
		// draw individual from p_source_subpop and assign to be a parent in subpop
		slim_popsize_t migrant_index;
		
		if (sim_.SexEnabled())
		{
			if (parent_index < subpop.parent_first_male_index_)
				migrant_index = p_source_subpop.DrawFemaleParentUsingFitness();
			else
				migrant_index = p_source_subpop.DrawMaleParentUsingFitness();
		}
		else
		{
			migrant_index = p_source_subpop.DrawParentUsingFitness();
		}
		
		subpop.parent_genomes_[2 * parent_index].copy_from_genome(p_source_subpop.parent_genomes_[2 * migrant_index]);
		subpop.parent_genomes_[2 * parent_index + 1].copy_from_genome(p_source_subpop.parent_genomes_[2 * migrant_index + 1]);
	}
	
	// UpdateFitness() is not called here - all fitnesses are kept as equal.  This is because the parents were drawn from the source subpopulation according
	// to their fitness already; fitness has already been applied.  If UpdateFitness() were called, fitness would be double-applied in this generation.
	
	return new_subpop;
}

// set size of subpopulation p_subpop_id to p_subpop_size
void Population::SetSize(Subpopulation &p_subpop, slim_popsize_t p_subpop_size)
{
	// SetSize() can only be called when the child generation has not yet been generated.  It sets the size on the child generation,
	// and then that size takes effect when the children are generated from the parents in EvolveSubpopulation().
	if (child_generation_valid_)
		EIDOS_TERMINATION << "ERROR (Population::SetSize): called when the child generation was valid." << eidos_terminate();
	
	if (p_subpop_size == 0) // remove subpopulation p_subpop_id
	{
		// Note that we don't free the subpopulation here, because there may be live references to it; instead we keep it to the end of the generation and then free it
		slim_objectid_t subpop_id = p_subpop.subpopulation_id_;
		
		erase(subpop_id);
		
		for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : *this)
			subpop_pair.second->migrant_fractions_.erase(subpop_id);
		
		// remember the subpop for later disposal
		removed_subpops_.push_back(&p_subpop);
	}
	else
	{
		// After we change the subpop size, we need to generate new children genomes to fit the new requirements
		p_subpop.child_subpop_size_ = p_subpop_size;
		p_subpop.GenerateChildrenToFit(false);	// false means generate only new children, not new parents
	}
}

// set fraction p_migrant_fraction of p_subpop_id that originates as migrants from p_source_subpop_id per generation  
void Population::SetMigration(Subpopulation &p_subpop, slim_objectid_t p_source_subpop_id, double p_migrant_fraction) 
{ 
	if (count(p_source_subpop_id) == 0)
		EIDOS_TERMINATION << "ERROR (Population::SetMigration): no subpopulation p" << p_source_subpop_id << "." << eidos_terminate();
	if (p_migrant_fraction < 0.0 || p_migrant_fraction > 1.0)
		EIDOS_TERMINATION << "ERROR (Population::SetMigration): migration fraction has to be within [0,1] (" << p_migrant_fraction << " supplied)." << eidos_terminate();
	
	if (p_subpop.migrant_fractions_.count(p_source_subpop_id) != 0)
		p_subpop.migrant_fractions_.erase(p_source_subpop_id);
	
	if (p_migrant_fraction > 0.0)	// BCH 4 March 2015: Added this if so we don't put a 0.0 migration rate into the table; harmless but looks bad in SLiMgui...
		p_subpop.migrant_fractions_.insert(std::pair<const slim_objectid_t,double>(p_source_subpop_id, p_migrant_fraction)); 
}

// execute a script event in the population; the script is assumed to be due to trigger
void Population::ExecuteScript(SLiMEidosBlock *p_script_block, slim_generation_t p_generation, const Chromosome &p_chromosome)
{
#pragma unused(p_generation, p_chromosome)
	EidosSymbolTable global_symbols(&p_script_block->eidos_contains_);
	EidosInterpreter interpreter(p_script_block->compound_statement_node_, global_symbols, &sim_);
	
	sim_.InjectIntoInterpreter(interpreter, p_script_block, true);
	
	// Interpret the script; the result from the interpretation is not used for anything
	EidosValue_SP result = interpreter.EvaluateInternalBlock(p_script_block->script_);
	
	// Output generated by the interpreter goes to our output stream
	SLIM_OUTSTREAM << interpreter.ExecutionOutput();
}

// apply mateChoice() callbacks to a mating event with a chosen first parent; the return is the second parent index, or -1 to force a redraw
slim_popsize_t Population::ApplyMateChoiceCallbacks(slim_popsize_t p_parent1_index, Subpopulation *p_subpop, Subpopulation *p_source_subpop, std::vector<SLiMEidosBlock*> &p_mate_choice_callbacks)
{
	// We start out using standard weights taken from the source subpopulation.  If, when we are done handling callbacks, we are still
	// using those standard weights, then we can do a draw using our fast lookup tables.  Otherwise, we will do a draw the hard way.
	bool sex_enabled = p_subpop->sex_enabled_;
	double *standard_weights = (sex_enabled ? p_source_subpop->cached_male_fitness_ : p_source_subpop->cached_parental_fitness_);
	double *current_weights = standard_weights;
	slim_popsize_t weights_length = p_source_subpop->cached_fitness_size_;
	bool weights_modified = false;
	SLiMEidosBlock *last_interventionist_mate_choice_callback = nullptr;
	
	for (SLiMEidosBlock *mate_choice_callback : p_mate_choice_callbacks)
	{
		if (mate_choice_callback->active_)
		{
			// local variables for the callback parameters that we might need to allocate here, and thus need to free below
			EidosValue_SP local_weights_ptr;
			bool redraw_mating = false;
			
			// The callback is active, so we need to execute it; we start a block here to manage the lifetime of the symbol table
			{
				EidosSymbolTable global_symbols(&mate_choice_callback->eidos_contains_);
				EidosInterpreter interpreter(mate_choice_callback->compound_statement_node_, global_symbols, &sim_);
				
				sim_.InjectIntoInterpreter(interpreter, mate_choice_callback, true);
				
				// Set all of the callback's parameters; note we use InitializeConstantSymbolEntry() for speed.
				// We can use that method because we know the lifetime of the symbol table is shorter than that of
				// the value objects, and we know that the values we are setting here will not change (the objects
				// referred to by the values may change, but the values themselves will not change).
				if (mate_choice_callback->contains_genome1_)
				{
					Genome *parent1_genome1 = &(p_source_subpop->parent_genomes_[p_parent1_index * 2]);
					global_symbols.InitializeConstantSymbolEntry(gStr_genome1, parent1_genome1->CachedEidosValue());
				}
				
				if (mate_choice_callback->contains_genome2_)
				{
					Genome *parent1_genome2 = &(p_source_subpop->parent_genomes_[p_parent1_index * 2 + 1]);
					global_symbols.InitializeConstantSymbolEntry(gStr_genome2, parent1_genome2->CachedEidosValue());
				}
				
				if (mate_choice_callback->contains_subpop_)
					global_symbols.InitializeConstantSymbolEntry(gStr_subpop, p_subpop->CachedSymbolTableEntry()->second);
				
				if (mate_choice_callback->contains_sourceSubpop_)
					global_symbols.InitializeConstantSymbolEntry(gStr_sourceSubpop, p_source_subpop->CachedSymbolTableEntry()->second);
				
				if (mate_choice_callback->contains_weights_)
				{
					local_weights_ptr = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector(current_weights, weights_length));
					global_symbols.InitializeConstantSymbolEntry(gStr_weights, local_weights_ptr);
				}
				
				// Interpret the script; the result from the interpretation can be one of several things, so this is a bit complicated
				EidosValue_SP result_SP = interpreter.EvaluateInternalBlock(mate_choice_callback->script_);
				EidosValue *result = result_SP.get();
				
				if (result->Type() == EidosValueType::kValueNULL)
				{
					// NULL indicates that the mateChoice() callback did not wish to alter the weights, so we do nothing
				}
				else if (result->Type() == EidosValueType::kValueFloat)
				{
					int result_count = result->Count();
					
					if (result_count == 0)
					{
						// a return of float(0) indicates that there is no acceptable mate for the first parent; the first parent must be redrawn
						redraw_mating = true;
					}
					else if (result_count == weights_length)
					{
						// a non-zero float vector must match the size of the source subpop, and provides a new set of weights for us to use
						if (!weights_modified)
						{
							current_weights = (double *)malloc(sizeof(double) * weights_length);	// allocate a new weights vector
							weights_modified = true;
						}
						
						// We really want to use EidosValue_Float_vector's FloatVector() method to get the values; if the dynamic_cast
						// fails, we presumably have an EidosValue_Float_singleton and must get its value with FloatAtIndex.
						EidosValue_Float_vector *result_vector_type = dynamic_cast<EidosValue_Float_vector *>(result);
						
						if (result_vector_type)
							memcpy(current_weights, result_vector_type->FloatVector().data(), sizeof(double) * weights_length);
						else
							current_weights[0] = result->FloatAtIndex(0, nullptr);
						
						// remember this callback for error attribution below
						last_interventionist_mate_choice_callback = mate_choice_callback;
					}
					else
					{
						EIDOS_TERMINATION << "ERROR (Population::ApplyMateChoiceCallbacks): invalid return value for mateChoice() callback." << eidos_terminate(mate_choice_callback->identifier_token_);
					}
				}
				else
				{
					EIDOS_TERMINATION << "ERROR (Population::ApplyMateChoiceCallbacks): invalid return value for mateChoice() callback." << eidos_terminate(mate_choice_callback->identifier_token_);
				}
				
				// Output generated by the interpreter goes to our output stream
				SLIM_OUTSTREAM << interpreter.ExecutionOutput();
			}
			
			// If this callback told us not to generate the child, we do not call the rest of the callback chain; we're done
			if (redraw_mating)
			{
				if (weights_modified)
					delete current_weights;
				
				return -1;
			}
		}
	}
	
	// If a callback supplied a different set of weights, we need to use those weights to draw a male parent
	if (weights_modified)
	{
		slim_popsize_t drawn_parent = -1;
		double weights_sum = 0;
		int positive_count = 0;
		
		// first we assess the weights vector: get its sum, bounds-check it, etc.
		for (slim_popsize_t weight_index = 0; weight_index < weights_length; ++weight_index)
		{
			double x = current_weights[weight_index];
			
			if (!isfinite(x))
				EIDOS_TERMINATION << "ERROR (Population::ApplyMateChoiceCallbacks): weight returned by mateChoice() callback is not finite." << eidos_terminate(last_interventionist_mate_choice_callback->identifier_token_);
			if (x < 0.0)
				EIDOS_TERMINATION << "ERROR (Population::ApplyMateChoiceCallbacks): weight returned by mateChoice() callback is less than 0.0." << eidos_terminate(last_interventionist_mate_choice_callback->identifier_token_);
			
			if (x > 0.0)
				positive_count++;
			
			weights_sum += x;
		}
		
		if (weights_sum <= 0.0)
			EIDOS_TERMINATION << "ERROR (Population::ApplyMateChoiceCallbacks): weights returned by mateChoice() callback sum to 0.0 or less." << eidos_terminate(last_interventionist_mate_choice_callback->identifier_token_);
		
		// then we draw from the weights vector
		if (positive_count == 1)
		{
			// there is only a single positive value, so the callback has chosen a parent for us; we just need to locate it
			// we could have noted it above, but I don't want to slow down that loop, since many positive weights is the likely case
			for (slim_popsize_t weight_index = 0; weight_index < weights_length; ++weight_index)
				if (current_weights[weight_index] > 0.0)
				{
					drawn_parent = weight_index;
					break;
				}
		}
		else
		{
			// there are multiple positive values, so we need to do a uniform draw and see who gets the rose
			double the_rose_in_the_teeth = gsl_rng_uniform_pos(gEidos_rng) * weights_sum;
			double bachelor_sum = 0.0;
			
			for (slim_popsize_t weight_index = 0; weight_index < weights_length; ++weight_index)
			{
				bachelor_sum += current_weights[weight_index];
				
				if (the_rose_in_the_teeth <= bachelor_sum)
				{
					drawn_parent = weight_index;
					break;
				}
			}
			
			// roundoff error goes to the last candidate (but this should not happen)
			if (drawn_parent == -1)
				drawn_parent = weights_length - 1;
		}
		
		delete current_weights;
		
		return drawn_parent;
	}
	
	// The standard behavior, with no active callbacks, is to draw a male parent using the standard fitness values
	return (sex_enabled ? p_source_subpop->DrawMaleParentUsingFitness() : p_source_subpop->DrawParentUsingFitness());
}

// apply modifyChild() callbacks to a generated child; a return of false means "do not use this child, generate a new one"
bool Population::ApplyModifyChildCallbacks(slim_popsize_t p_child_index, IndividualSex p_child_sex, slim_popsize_t p_parent1_index, slim_popsize_t p_parent2_index, bool p_is_selfing, bool p_is_cloning, Subpopulation *p_subpop, Subpopulation *p_source_subpop, std::vector<SLiMEidosBlock*> &p_modify_child_callbacks)
{
	for (SLiMEidosBlock *modify_child_callback : p_modify_child_callbacks)
	{
		if (modify_child_callback->active_)
		{
			// The callback is active, so we need to execute it
			EidosSymbolTable global_symbols(&modify_child_callback->eidos_contains_);
			EidosInterpreter interpreter(modify_child_callback->compound_statement_node_, global_symbols, &sim_);
			
			sim_.InjectIntoInterpreter(interpreter, modify_child_callback, true);
			
			// Set all of the callback's parameters; note we use InitializeConstantSymbolEntry() for speed.
			// We can use that method because we know the lifetime of the symbol table is shorter than that of
			// the value objects, and we know that the values we are setting here will not change (the objects
			// referred to by the values may change, but the values themselves will not change).
			if (modify_child_callback->contains_childGenome1_)
			{
				Genome *child_genome1 = &(p_subpop->child_genomes_[p_child_index * 2]);
				global_symbols.InitializeConstantSymbolEntry(gStr_childGenome1, child_genome1->CachedEidosValue());
			}
			
			if (modify_child_callback->contains_childGenome2_)
			{
				Genome *child_genome2 = &(p_subpop->child_genomes_[p_child_index * 2 + 1]);
				global_symbols.InitializeConstantSymbolEntry(gStr_childGenome2, child_genome2->CachedEidosValue());
			}
			
			if (modify_child_callback->contains_childIsFemale_)
			{
				if (p_child_sex == IndividualSex::kHermaphrodite)
					global_symbols.InitializeConstantSymbolEntry(gStr_childIsFemale, gStaticEidosValueNULL);
				else
					global_symbols.InitializeConstantSymbolEntry(gStr_childIsFemale, (p_child_sex == IndividualSex::kFemale) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
			}
			
			if (modify_child_callback->contains_parent1Genome1_)
			{
				Genome *parent1_genome1 = &(p_source_subpop->parent_genomes_[p_parent1_index * 2]);
				global_symbols.InitializeConstantSymbolEntry(gStr_parent1Genome1, parent1_genome1->CachedEidosValue());
			}
			
			if (modify_child_callback->contains_parent1Genome2_)
			{
				Genome *parent1_genome2 = &(p_source_subpop->parent_genomes_[p_parent1_index * 2 + 1]);
				global_symbols.InitializeConstantSymbolEntry(gStr_parent1Genome2, parent1_genome2->CachedEidosValue());
			}
			
			if (modify_child_callback->contains_isSelfing_)
				global_symbols.InitializeConstantSymbolEntry(gStr_isSelfing, p_is_selfing ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
			
			if (modify_child_callback->contains_isCloning_)
				global_symbols.InitializeConstantSymbolEntry(gStr_isCloning, p_is_cloning ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
			
			if (modify_child_callback->contains_parent2Genome1_)
			{
				Genome *parent2_genome1 = &(p_source_subpop->parent_genomes_[p_parent2_index * 2]);
				global_symbols.InitializeConstantSymbolEntry(gStr_parent2Genome1, parent2_genome1->CachedEidosValue());
			}
			
			if (modify_child_callback->contains_parent2Genome2_)
			{
				Genome *parent2_genome2 = &(p_source_subpop->parent_genomes_[p_parent2_index * 2 + 1]);
				global_symbols.InitializeConstantSymbolEntry(gStr_parent2Genome2, parent2_genome2->CachedEidosValue());
			}
			
			if (modify_child_callback->contains_subpop_)
				global_symbols.InitializeConstantSymbolEntry(gStr_subpop, p_subpop->CachedSymbolTableEntry()->second);
			
			if (modify_child_callback->contains_sourceSubpop_)
				global_symbols.InitializeConstantSymbolEntry(gStr_sourceSubpop, p_source_subpop->CachedSymbolTableEntry()->second);
			
			// Interpret the script; the result from the interpretation must be a singleton double used as a new fitness value
			EidosValue_SP result_SP = interpreter.EvaluateInternalBlock(modify_child_callback->script_);
			EidosValue *result = result_SP.get();
			
			if ((result->Type() != EidosValueType::kValueLogical) || (result->Count() != 1))
				EIDOS_TERMINATION << "ERROR (Population::ApplyModifyChildCallbacks): modifyChild() callbacks must provide a logical singleton return value." << eidos_terminate(modify_child_callback->identifier_token_);
			
			bool generate_child = result->LogicalAtIndex(0, nullptr);
			
			// Output generated by the interpreter goes to our output stream
			SLIM_OUTSTREAM << interpreter.ExecutionOutput();
			
			// If this callback told us not to generate the child, we do not call the rest of the callback chain; we're done
			if (!generate_child)
				return false;
		}
	}
	
	return true;
}

// generate children for subpopulation p_subpop_id, drawing from all source populations, handling crossover and mutation
void Population::EvolveSubpopulation(Subpopulation &p_subpop, const Chromosome &p_chromosome, slim_generation_t p_generation, bool p_mate_choice_callbacks_present, bool p_modify_child_callbacks_present)
{
	bool sex_enabled = p_subpop.sex_enabled_;
	slim_popsize_t total_children = p_subpop.child_subpop_size_;
	
	// BCH 27 Dec. 2014: Note that child_map has been removed here, so the order of generated children is NOT RANDOM!
	// Any code that chooses individuals from the population should choose randomly to avoid order-dependency!
	
	// set up to draw migrants; this works the same in the sex and asex cases, and for males / females / hermaphrodites
	// the way the code is now structured, "migrant" really includes everybody; we are a migrant source subpop for ourselves
	int migrant_source_count = static_cast<int>(p_subpop.migrant_fractions_.size());
	double migration_rates[migrant_source_count + 1];
	Subpopulation *migration_sources[migrant_source_count + 1];
	unsigned int num_migrants[migrant_source_count + 1];			// used by client code below; type constrained by gsl_ran_multinomial()
	
	if (migrant_source_count > 0)
	{
		double migration_rate_sum = 0.0;
		int pop_count = 0;
		
		for (const std::pair<const slim_objectid_t,double> &fractions_pair : p_subpop.migrant_fractions_)
		{
			slim_objectid_t migrant_source_id = fractions_pair.first;
			auto migrant_source_pair = find(migrant_source_id);
			
			if (migrant_source_pair == end())
				EIDOS_TERMINATION << "ERROR (Population::EvolveSubpopulation): no migrant source subpopulation p" << migrant_source_id << "." << eidos_terminate();
			
			migration_rates[pop_count] = fractions_pair.second;
			migration_sources[pop_count] = migrant_source_pair->second;
			migration_rate_sum += fractions_pair.second;
			pop_count++;
		}
		
		if (migration_rate_sum <= 1.0)
		{
			// the remaining fraction is within-subpopulation mating
			migration_rates[pop_count] = 1.0 - migration_rate_sum;
			migration_sources[pop_count] = &p_subpop;
		}
		else
			EIDOS_TERMINATION << "ERROR (Population::EvolveSubpopulation): too many migrants in subpopulation p" << p_subpop.subpopulation_id_ << "; migration fractions must sum to <= 1.0." << eidos_terminate();
	}
	else
	{
		migration_rates[0] = 1.0;
		migration_sources[0] = &p_subpop;
	}
	
	// SEX ONLY: the sex and asex cases share code but work a bit differently; the sex cases generates females and then males in
	// separate passes, and selfing is disabled in the sex case.  This block sets up for the sex case to diverge in these ways.
	slim_popsize_t total_female_children = 0, total_male_children = 0;
	int number_of_sexes = 1;
	
	if (sex_enabled)
	{
		double sex_ratio = p_subpop.child_sex_ratio_;
		
		total_male_children = static_cast<slim_popsize_t>(lround(total_children * sex_ratio));		// sex ratio is defined as proportion male
		total_female_children = total_children - total_male_children;
		number_of_sexes = 2;
		
		if (total_male_children <= 0 || total_female_children <= 0)
			EIDOS_TERMINATION << "ERROR (Population::EvolveSubpopulation): sex ratio " << sex_ratio << " results in a unisexual child population." << eidos_terminate();
	}
	
	// Now we're ready to actually generate offspring.  We loop to generate females first (sex_index == 0) and
	// males second (sex_index == 1).  In nonsexual simulations number_of_sexes == 1 and this loops just once.
	slim_popsize_t child_count = 0;	// counter over all subpop_size_ children
	
	for (int sex_index = 0; sex_index < number_of_sexes; ++sex_index)
	{
		slim_popsize_t total_children_of_sex = sex_enabled ? ((sex_index == 0) ? total_female_children : total_male_children) : total_children;
		IndividualSex child_sex = sex_enabled ? ((sex_index == 0) ? IndividualSex::kFemale : IndividualSex::kMale) : IndividualSex::kHermaphrodite;
		
		// draw the number of individuals from the migrant source subpops, and from ourselves, for the current sex
		if (migrant_source_count == 0)
			num_migrants[0] = (unsigned int)total_children_of_sex;
		else
			gsl_ran_multinomial(gEidos_rng, migrant_source_count + 1, (unsigned int)total_children_of_sex, migration_rates, num_migrants);
		
		// loop over all source subpops, including ourselves
		for (int pop_count = 0; pop_count < migrant_source_count + 1; ++pop_count)
		{
			slim_popsize_t migrants_to_generate = static_cast<slim_popsize_t>(num_migrants[pop_count]);
			
			if (migrants_to_generate > 0)
			{
				Subpopulation &source_subpop = *(migration_sources[pop_count]);
				slim_objectid_t subpop_id = source_subpop.subpopulation_id_;
				double selfing_fraction = sex_enabled ? 0.0 : source_subpop.selfing_fraction_;
				double cloning_fraction = (sex_index == 0) ? source_subpop.female_clone_fraction_ : source_subpop.male_clone_fraction_;
				
				// figure out how many from this source subpop are the result of selfing and/or cloning
				slim_popsize_t number_to_self = 0, number_to_clone = 0;
				
				if (selfing_fraction > 0)
				{
					if (cloning_fraction > 0)
					{
						double fractions[3] = {selfing_fraction, cloning_fraction, 1.0 - (selfing_fraction + cloning_fraction)};
						unsigned int counts[3] = {0, 0, 0};
						
						gsl_ran_multinomial(gEidos_rng, 3, (unsigned int)migrants_to_generate, fractions, counts);
						
						number_to_self = static_cast<slim_popsize_t>(counts[0]);
						number_to_clone = static_cast<slim_popsize_t>(counts[1]);
					}
					else
						number_to_self = static_cast<slim_popsize_t>(gsl_ran_binomial(gEidos_rng, selfing_fraction, (unsigned int)migrants_to_generate));
				}
				else if (cloning_fraction > 0)
					number_to_clone = static_cast<slim_popsize_t>(gsl_ran_binomial(gEidos_rng, cloning_fraction, (unsigned int)migrants_to_generate));
				
				// figure out our callback situation for this source subpop; callbacks come from the source, not the destination
				std::vector<SLiMEidosBlock*> *mate_choice_callbacks = nullptr, *modify_child_callbacks = nullptr;
				
				if (p_mate_choice_callbacks_present && source_subpop.registered_mate_choice_callbacks_.size())
					mate_choice_callbacks = &source_subpop.registered_mate_choice_callbacks_;
				if (p_modify_child_callbacks_present && source_subpop.registered_modify_child_callbacks_.size())
					modify_child_callbacks = &source_subpop.registered_modify_child_callbacks_;
				
				// generate all selfed, cloned, and autogamous offspring in one shared loop
				slim_popsize_t migrant_count = 0;
				
				if ((number_to_self == 0) && (number_to_clone == 0) && !mate_choice_callbacks && !modify_child_callbacks)
				{
					// a simple loop for the base case with no selfing, no cloning, and no callbacks
					while (migrant_count < migrants_to_generate)
					{
						slim_popsize_t parent1 = sex_enabled ? source_subpop.DrawFemaleParentUsingFitness() : source_subpop.DrawParentUsingFitness();
						slim_popsize_t parent2 = sex_enabled ? source_subpop.DrawMaleParentUsingFitness() : source_subpop.DrawParentUsingFitness();	// note this does not prohibit selfing!
						
						// recombination, gene-conversion, mutation
						DoCrossoverMutation(&p_subpop, &source_subpop, 2 * child_count, subpop_id, 2 * parent1, 2 * parent1 + 1, p_chromosome, p_generation, child_sex);
						DoCrossoverMutation(&p_subpop, &source_subpop, 2 * child_count + 1, subpop_id, 2 * parent2, 2 * parent2 + 1, p_chromosome, p_generation, child_sex);
						
						migrant_count++;
						child_count++;
					}
				}
				else
				{
					// the full loop with support for selfing/ cloning, and callbacks
					while (migrant_count < migrants_to_generate)
					{
						bool selfed = false, cloned = false;
						slim_popsize_t parent1, parent2;
						
						if (number_to_clone > 0)
						{
							if (sex_enabled)
								parent1 = (child_sex == IndividualSex::kFemale) ? source_subpop.DrawFemaleParentUsingFitness() : source_subpop.DrawMaleParentUsingFitness();
							else
								parent1 = source_subpop.DrawParentUsingFitness();
							
							parent2 = parent1;
							cloned = true;
							
							DoClonalMutation(&p_subpop, &source_subpop, 2 * child_count, subpop_id, 2 * parent1, p_chromosome, p_generation, child_sex);
							DoClonalMutation(&p_subpop, &source_subpop, 2 * child_count + 1, subpop_id, 2 * parent1 + 1, p_chromosome, p_generation, child_sex);
						}
						else
						{
							parent1 = sex_enabled ? source_subpop.DrawFemaleParentUsingFitness() : source_subpop.DrawParentUsingFitness();
							
							if (number_to_self > 0)				parent2 = parent1, selfed = true;
							else if (!mate_choice_callbacks)	parent2 = sex_enabled ? source_subpop.DrawMaleParentUsingFitness() : source_subpop.DrawParentUsingFitness();	// selfing possible!
							else
							{
								while (true)	// loop while parent2 == -1, indicating a request for a new first parent
								{
									parent2 = ApplyMateChoiceCallbacks(parent1, &p_subpop, &source_subpop, *mate_choice_callbacks);
									
									if (parent2 != -1)
										break;
									
									// parent1 was rejected by the callbacks, so we need to redraw a new parent1
									parent1 = sex_enabled ? source_subpop.DrawFemaleParentUsingFitness() : source_subpop.DrawParentUsingFitness();
								}
							}
							
							// recombination, gene-conversion, mutation
							DoCrossoverMutation(&p_subpop, &source_subpop, 2 * child_count, subpop_id, 2 * parent1, 2 * parent1 + 1, p_chromosome, p_generation, child_sex);
							DoCrossoverMutation(&p_subpop, &source_subpop, 2 * child_count + 1, subpop_id, 2 * parent2, 2 * parent2 + 1, p_chromosome, p_generation, child_sex);
						}
						
						if (modify_child_callbacks)
							if (!ApplyModifyChildCallbacks(child_count, child_sex, parent1, parent2, selfed, cloned, &p_subpop, &source_subpop, *modify_child_callbacks))
								continue;
						
						// if the child was accepted, change all our counters; can't be done before the modifyChild() callback since it might reject the child!
						if (cloned)
							--number_to_clone;
						else if (selfed)
							--number_to_self;
						migrant_count++;
						child_count++;
					}
				}
			}
		}
	}
}

// generate a child genome from parental genomes, with recombination, gene conversion, and mutation
void Population::DoCrossoverMutation(Subpopulation *p_subpop, Subpopulation *p_source_subpop, slim_popsize_t p_child_genome_index, slim_objectid_t p_source_subpop_id, slim_popsize_t p_parent1_genome_index, slim_popsize_t p_parent2_genome_index, const Chromosome &p_chromosome, slim_generation_t p_generation, IndividualSex p_child_sex)
{
	// child genome p_child_genome_index in subpopulation p_subpop_id is assigned outcome of cross-overs at breakpoints in all_breakpoints
	// between parent genomes p_parent1_genome_index and p_parent2_genome_index from subpopulation p_source_subpop_id and new mutations added
	// 
	// example: all_breakpoints = (r1, r2)
	// 
	// mutations (      x < r1) assigned from p1
	// mutations (r1 <= x < r2) assigned from p2
	// mutations (r2 <= x     ) assigned from p1
	
	// A lot of the checks here are only on when DEBUG is defined.  They should absolutely never be hit; if they are, it indicates a flaw
	// in SLiM's internal logic, not user error.  This method gets called a whole lot; every test makes a speed difference.  So disabling
	// these checks seems to make sense.  Of course, if you want the checks on, just define DEBUG.
	
#ifdef DEBUG
	if (p_child_sex == IndividualSex::kUnspecified)
		EIDOS_TERMINATION << "ERROR (Population::DoCrossoverMutation): Child sex cannot be IndividualSex::kUnspecified." << eidos_terminate();
#endif
	
	bool use_only_strand_1 = false;		// if true, we are in a case where crossover cannot occur, and we are to use only parent strand 1
	bool do_swap = true;				// if true, we are to swap the parental strands at the beginning, either 50% of the time (if use_only_strand_1 is false), or always (if use_only_strand_1 is true – in other words, we are directed to use only strand 2)
	
	Genome &child_genome = p_subpop->child_genomes_[p_child_genome_index];
	GenomeType child_genome_type = child_genome.GenomeType();
	Genome *parent_genome_1 = &(p_source_subpop->parent_genomes_[p_parent1_genome_index]);
	GenomeType parent1_genome_type = parent_genome_1->GenomeType();
	Genome *parent_genome_2 = &(p_source_subpop->parent_genomes_[p_parent2_genome_index]);
	GenomeType parent2_genome_type = parent_genome_2->GenomeType();
	
	if (child_genome_type == GenomeType::kAutosome)
	{
		// If we're modeling autosomes, we can disregard p_child_sex entirely; we don't care whether we're modeling sexual or hermaphrodite individuals
#ifdef DEBUG
		if (parent1_genome_type != GenomeType::kAutosome || parent2_genome_type != GenomeType::kAutosome)
			EIDOS_TERMINATION << "ERROR (Population::DoCrossoverMutation): Mismatch between parent and child genome types (case 1)." << eidos_terminate();
#endif
	}
	else
	{
		// SEX ONLY: If we're modeling sexual individuals, then there are various degenerate cases to be considered, since X and Y don't cross over, there are null chromosomes, etc.
#ifdef DEBUG
		if (p_child_sex == IndividualSex::kHermaphrodite)
			EIDOS_TERMINATION << "ERROR (Population::DoCrossoverMutation): A hermaphrodite child is requested but the child genome is not autosomal." << eidos_terminate();
		if (parent1_genome_type == GenomeType::kAutosome || parent2_genome_type == GenomeType::kAutosome)
			EIDOS_TERMINATION << "ERROR (Population::DoCrossoverMutation): Mismatch between parent and child genome types (case 2)." << eidos_terminate();
#endif
		
		if (child_genome_type == GenomeType::kXChromosome)
		{
			if (p_child_sex == IndividualSex::kMale)
			{
				// If our parent is male (XY or YX), then we have a mismatch, because we're supposed to be male and we're supposed to be getting an X chromosome, but the X must come from the female
				if (parent1_genome_type == GenomeType::kYChromosome || parent2_genome_type == GenomeType::kYChromosome)
					EIDOS_TERMINATION << "ERROR (Population::DoCrossoverMutation): Mismatch between parent and child genome types (case 3)." << eidos_terminate();
				
				// else: we're doing inheritance from the female (XX) to get our X chromosome; we treat this just like the autosomal case
			}
			else if (p_child_sex == IndividualSex::kFemale)
			{
				if (parent1_genome_type == GenomeType::kYChromosome && parent2_genome_type == GenomeType::kXChromosome)
				{
					// we're doing inheritance from the male (YX) to get an X chromosome; we need to ensure that we take the X
					use_only_strand_1 = true, do_swap = true;	// use strand 2
				}
				else if (parent1_genome_type == GenomeType::kXChromosome && parent2_genome_type == GenomeType::kYChromosome)
				{
					// we're doing inheritance from the male (XY) to get an X chromosome; we need to ensure that we take the X
					use_only_strand_1 = true, do_swap = false;	// use strand 1
				}
				// else: we're doing inheritance from the female (XX) to get an X chromosome; we treat this just like the autosomal case
			}
		}
		else // (child_genome_type == GenomeType::kYChromosome), so p_child_sex == IndividualSex::kMale
		{
			if (p_child_sex == IndividualSex::kFemale)
				EIDOS_TERMINATION << "ERROR (Population::DoCrossoverMutation): A female child is requested but the child genome is a Y chromosome." << eidos_terminate();
			
			if (parent1_genome_type == GenomeType::kYChromosome && parent2_genome_type == GenomeType::kXChromosome)
			{
				// we're doing inheritance from the male (YX) to get a Y chromosome; we need to ensure that we take the Y
				use_only_strand_1 = true, do_swap = false;	// use strand 1
			}
			else if (parent1_genome_type == GenomeType::kXChromosome && parent2_genome_type == GenomeType::kYChromosome)
			{
				// we're doing inheritance from the male (XY) to get an X chromosome; we need to ensure that we take the Y
				use_only_strand_1 = true, do_swap = true;	// use strand 2
			}
			else
			{
				// else: we're doing inheritance from the female (XX) to get a Y chromosome, so this is a mismatch
				EIDOS_TERMINATION << "ERROR (Population::DoCrossoverMutation): Mismatch between parent and child genome types (case 4)." << eidos_terminate();
			}
		}
	}
	
	// swap strands in half of cases to assure random assortment (or in all cases, if use_only_strand_1 == true, meaning that crossover cannot occur)
	if (do_swap && (use_only_strand_1 || eidos_random_bool(gEidos_rng)))
	{
		slim_popsize_t swap = p_parent1_genome_index;
		p_parent1_genome_index = p_parent2_genome_index;
		p_parent2_genome_index = swap;
		
		Genome *swap2 = parent_genome_1;
		parent_genome_1 = parent_genome_2;
		parent_genome_2 = swap2;
		
		// Not used below this point...
		//GenomeType swap3 = parent1_genome_type;
		//parent1_genome_type = parent2_genome_type;
		//parent2_genome_type = swap3;
	}
	
	// check for null cases
	bool child_genome_null = child_genome.IsNull();
#ifdef DEBUG
	bool parent_genome_1_null = parent_genome_1->IsNull();
	bool parent_genome_2_null = parent_genome_2->IsNull();
#endif
	
	if (child_genome_null)
	{
#ifdef DEBUG
		if (!use_only_strand_1)
		{
			// If we're trying to cross over, both parental strands had better be null
			if (!parent_genome_1_null || !parent_genome_2_null)
				EIDOS_TERMINATION << "ERROR (Population::DoCrossoverMutation): Child genome is null, but crossover is requested and a parental genome is non-null." << eidos_terminate();
		}
		else
		{
			// So we are not crossing over, and we are supposed to use strand 1; it should also be null, otherwise something has gone wrong
			if (!parent_genome_1_null)
				EIDOS_TERMINATION << "ERROR (Population::DoCrossoverMutation): Child genome is null, but the parental strand is not." << eidos_terminate();
		}
#endif
		
		// a null strand cannot cross over and cannot mutate, so we are done
		return;
	}
	
#ifdef DEBUG
	if (use_only_strand_1 && parent_genome_1_null)
		EIDOS_TERMINATION << "ERROR (Population::DoCrossoverMutation): Child genome is non-null, but the parental strand is null." << eidos_terminate();
	
	if (!use_only_strand_1 && (parent_genome_1_null || parent_genome_2_null))
		EIDOS_TERMINATION << "ERROR (Population::DoCrossoverMutation): Child genome is non-null, but a parental strand is null." << eidos_terminate();
#endif
	
	//
	//	OK!  We should have covered all error cases above, so we can now proceed with more alacrity.  We just need to follow
	//	the instructions given to us from above, namely use_only_strand_1.  We know we are doing a non-null strand.
	//
	
	// start with a clean slate in the child genome
	child_genome.clear();
	
	// determine how many mutations and breakpoints we have
	int num_mutations, num_breakpoints;
	
	if (use_only_strand_1)
	{
		num_breakpoints = 0;
		num_mutations = p_chromosome.DrawMutationCount();
	}
	else
	{
		// get both the number of mutations and the number of breakpoints here; this allows us to draw both jointly, super fast!
		//int num_mutations = p_chromosome.DrawMutationCount();
		//int num_breakpoints = p_chromosome.DrawBreakpointCount();
		
		p_chromosome.DrawMutationAndBreakpointCounts(&num_mutations, &num_breakpoints);
	}
	
	// mutations are usually rare, so let's streamline the case where none occur
	if (num_mutations == 0)
	{
		if (num_breakpoints == 0)
		{
			// no mutations and no crossovers, so the child genome is just a copy of the parental genome
			child_genome.copy_from_genome(p_source_subpop->parent_genomes_[p_parent1_genome_index]);
		}
		else
		{
			// create vector with uniqued recombination breakpoints
			std::vector<slim_position_t> all_breakpoints = p_chromosome.DrawBreakpoints(num_breakpoints);
			
			all_breakpoints.push_back(p_chromosome.last_position_ + 1);
			sort(all_breakpoints.begin(), all_breakpoints.end());
			all_breakpoints.erase(unique(all_breakpoints.begin(), all_breakpoints.end()), all_breakpoints.end());
			
			// do the crossover
			Mutation **parent1_iter		= p_source_subpop->parent_genomes_[p_parent1_genome_index].begin_pointer();
			Mutation **parent2_iter		= p_source_subpop->parent_genomes_[p_parent2_genome_index].begin_pointer();
			
			Mutation **parent1_iter_max	= p_source_subpop->parent_genomes_[p_parent1_genome_index].end_pointer();
			Mutation **parent2_iter_max	= p_source_subpop->parent_genomes_[p_parent2_genome_index].end_pointer();
			
			Mutation **parent_iter		= parent1_iter;
			Mutation **parent_iter_max	= parent1_iter_max;
			
			int break_index_max = static_cast<int>(all_breakpoints.size());	// can be != num_breakpoints+1 due to gene conversion and dup removal!
			
			for (int break_index = 0; break_index != break_index_max; break_index++)
			{
				slim_position_t breakpoint = all_breakpoints[break_index];
				
				// while there are still old mutations in the parent before the current breakpoint...
				while (parent_iter != parent_iter_max)
				{
					Mutation *current_mutation = *parent_iter;
					
					if (current_mutation->position_ >= breakpoint)
						break;
					
					// add the old mutation; no need to check for a duplicate here since the parental genome is already duplicate-free
					child_genome.push_back(current_mutation);
					
					parent_iter++;
				}
				
				// we have reached the breakpoint, so swap parents
				parent1_iter = parent2_iter;	parent1_iter_max = parent2_iter_max;
				parent2_iter = parent_iter;		parent2_iter_max = parent_iter_max;
				parent_iter = parent1_iter;		parent_iter_max = parent1_iter_max; 
				
				// skip over anything in the new parent that occurs prior to the breakpoint; it was not the active strand
				while (parent_iter != parent_iter_max && (*parent_iter)->position_ < breakpoint)
					parent_iter++;
			}
		}
	}
	else
	{
		// we have to be careful here not to touch the second strand if we have no breakpoints, because it could be null
		
		// create vector with the mutations to be added
		Genome mutations_to_add;
		
		for (int k = 0; k < num_mutations; k++)
		{
			Mutation *new_mutation = p_chromosome.DrawNewMutation(p_source_subpop_id, p_generation);
			
			mutations_to_add.insert_sorted_mutation(new_mutation);	// keeps it sorted; since few mutations are expected, this is fast
			mutation_registry_.push_back(new_mutation);
		}
		
		// create vector with uniqued recombination breakpoints
		std::vector<slim_position_t> all_breakpoints = p_chromosome.DrawBreakpoints(num_breakpoints); 
		all_breakpoints.push_back(p_chromosome.last_position_ + 1);
		sort(all_breakpoints.begin(), all_breakpoints.end());
		all_breakpoints.erase(unique(all_breakpoints.begin(), all_breakpoints.end()), all_breakpoints.end());
		
		// do the crossover
		Mutation **parent1_iter		= p_source_subpop->parent_genomes_[p_parent1_genome_index].begin_pointer();
		Mutation **parent2_iter		= (num_breakpoints == 0) ? nullptr : p_source_subpop->parent_genomes_[p_parent2_genome_index].begin_pointer();
		
		Mutation **parent1_iter_max	= p_source_subpop->parent_genomes_[p_parent1_genome_index].end_pointer();
		Mutation **parent2_iter_max	= (num_breakpoints == 0) ? nullptr : p_source_subpop->parent_genomes_[p_parent2_genome_index].end_pointer();
		
		Mutation **mutation_iter		= mutations_to_add.begin_pointer();
		Mutation **mutation_iter_max	= mutations_to_add.end_pointer();
		
		Mutation **parent_iter		= parent1_iter;
		Mutation **parent_iter_max	= parent1_iter_max;
		
		int break_index_max = static_cast<int>(all_breakpoints.size());	// can be != num_breakpoints+1 due to gene conversion and dup removal!
		
		for (int break_index = 0; ; )	// the other parts are below, but this is conceptually a for loop, so I've kept it that way...
		{
			slim_position_t breakpoint = all_breakpoints[break_index];
			
			// NOTE these caches are valid from here...
			Mutation *parent_iter_mutation, *mutation_iter_mutation;
			slim_position_t parent_iter_pos, mutation_iter_pos;
			
			if (parent_iter != parent_iter_max) {
				parent_iter_mutation = *parent_iter;
				parent_iter_pos = parent_iter_mutation->position_;
			} else {
				parent_iter_mutation = nullptr;
				parent_iter_pos = SLIM_MAX_BASE_POSITION + 100;		// past the maximum legal end position
			}
			
			if (mutation_iter != mutation_iter_max) {
				mutation_iter_mutation = *mutation_iter;
				mutation_iter_pos = mutation_iter_mutation->position_;
			} else {
				mutation_iter_mutation = nullptr;
				mutation_iter_pos = SLIM_MAX_BASE_POSITION + 100;		// past the maximum legal end position
			}
			
			// while there are still old mutations in the parent, or new mutations to be added, before the current breakpoint...
			while ((parent_iter_pos < breakpoint) || (mutation_iter_pos < breakpoint))
			{
				// while an old mutation in the parent is before the breakpoint and before the next new mutation...
				while (parent_iter_pos < breakpoint && parent_iter_pos <= mutation_iter_pos)
				{
					// add the mutation; we know it is not already present
					child_genome.push_back(parent_iter_mutation);
					parent_iter++;
					
					if (parent_iter != parent_iter_max) {
						parent_iter_mutation = *parent_iter;
						parent_iter_pos = parent_iter_mutation->position_;
					} else {
						parent_iter_mutation = nullptr;
						parent_iter_pos = SLIM_MAX_BASE_POSITION + 100;		// past the maximum legal end position
					}
				}
				
				// while a new mutation is before the breakpoint and before the next old mutation in the parent...
				while (mutation_iter_pos < breakpoint && mutation_iter_pos <= parent_iter_pos)
				{
					// add the mutation; we know it is not already present
					child_genome.push_back(mutation_iter_mutation);
					mutation_iter++;
					
					if (mutation_iter != mutation_iter_max) {
						mutation_iter_mutation = *mutation_iter;
						mutation_iter_pos = mutation_iter_mutation->position_;
					} else {
						mutation_iter_mutation = nullptr;
						mutation_iter_pos = SLIM_MAX_BASE_POSITION + 100;		// past the maximum legal end position
					}
				}
			}
			// NOTE ...to here
			
			// these statements complete our for loop; they are here so that if we have no breakpoints we do not touch the second strand below
			break_index++;
			
			if (break_index == break_index_max)
				break;
			
			// we have reached the breakpoint, so swap parents
			parent1_iter = parent2_iter;	parent1_iter_max = parent2_iter_max;
			parent2_iter = parent_iter;		parent2_iter_max = parent_iter_max;
			parent_iter = parent1_iter;		parent_iter_max = parent1_iter_max; 
			
			// skip over anything in the new parent that occurs prior to the breakpoint; it was not the active strand
			while (parent_iter != parent_iter_max && (*parent_iter)->position_ < breakpoint)
				parent_iter++;
		}
	}
}

void Population::DoClonalMutation(Subpopulation *p_subpop, Subpopulation *p_source_subpop, slim_popsize_t p_child_genome_index, slim_objectid_t p_source_subpop_id, slim_popsize_t p_parent_genome_index, const Chromosome &p_chromosome, slim_generation_t p_generation, IndividualSex p_child_sex)
{
#pragma unused(p_child_sex)
#ifdef DEBUG
	if (p_child_sex == IndividualSex::kUnspecified)
		EIDOS_TERMINATION << "ERROR (Population::DoClonalMutation): Child sex cannot be IndividualSex::kUnspecified." << eidos_terminate();
#endif
	
	Genome &child_genome = p_subpop->child_genomes_[p_child_genome_index];
	GenomeType child_genome_type = child_genome.GenomeType();
	Genome *parent_genome = &(p_source_subpop->parent_genomes_[p_parent_genome_index]);
	GenomeType parent_genome_type = parent_genome->GenomeType();
	
	if (child_genome_type != parent_genome_type)
		EIDOS_TERMINATION << "ERROR (Population::DoClonalMutation): Mismatch between parent and child genome types (type != type)." << eidos_terminate();
	
	// check for null cases
	bool child_genome_null = child_genome.IsNull();
	bool parent_genome_null = parent_genome->IsNull();
	
	if (child_genome_null != parent_genome_null)
		EIDOS_TERMINATION << "ERROR (Population::DoClonalMutation): Mismatch between parent and child genome types (null != null)." << eidos_terminate();
	
	if (child_genome_null)
	{
		// a null strand cannot mutate, so we are done
		return;
	}
	
	// start with a clean slate in the child genome
	child_genome.clear();
	
	// determine how many mutations and breakpoints we have
	int num_mutations = p_chromosome.DrawMutationCount();
	
	// mutations are usually rare, so let's streamline the case where none occur
	if (num_mutations == 0)
	{
		// no mutations, so the child genome is just a copy of the parental genome
		child_genome.copy_from_genome(p_source_subpop->parent_genomes_[p_parent_genome_index]);
	}
	else
	{
		// create vector with the mutations to be added
		Genome mutations_to_add;
		
		for (int k = 0; k < num_mutations; k++)
		{
			Mutation *new_mutation = p_chromosome.DrawNewMutation(p_source_subpop_id, p_generation);
			
			mutations_to_add.insert_sorted_mutation(new_mutation);	// keeps it sorted; since few mutations are expected, this is fast
			mutation_registry_.push_back(new_mutation);
		}
		
		// interleave the parental genome with the new mutations
		Mutation **parent_iter		= p_source_subpop->parent_genomes_[p_parent_genome_index].begin_pointer();
		Mutation **parent_iter_max	= p_source_subpop->parent_genomes_[p_parent_genome_index].end_pointer();
		Mutation **mutation_iter		= mutations_to_add.begin_pointer();
		Mutation **mutation_iter_max	= mutations_to_add.end_pointer();
		
		// while there are still old mutations in the parent, or new mutations to be added, before the end...
		while ((parent_iter != parent_iter_max) || (mutation_iter != mutation_iter_max))
		{
			// while an old mutation in the parent is before or at the next new mutation...
			slim_position_t mutation_iter_pos = (mutation_iter == mutation_iter_max) ? (SLIM_MAX_BASE_POSITION + 100) : (*mutation_iter)->position_;
			
			while ((parent_iter != parent_iter_max) && ((*parent_iter)->position_ <= mutation_iter_pos))
			{
				// we know the mutation is not already present, since mutations on the parent strand are already uniqued,
				// and new mutations are, by definition, new and thus cannot match the existing mutations
				child_genome.push_back(*parent_iter);
				parent_iter++;
			}
			
			// while a new mutation is before or at the next old mutation in the parent...
			slim_position_t parent_iter_pos = (parent_iter == parent_iter_max) ? (SLIM_MAX_BASE_POSITION + 100) : (*parent_iter)->position_;
			
			while ((mutation_iter != mutation_iter_max) && ((*mutation_iter)->position_ <= parent_iter_pos))
			{
				// we know the mutation is not already present, since mutations on the parent strand are already uniqued,
				// and new mutations are, by definition, new and thus cannot match the existing mutations
				child_genome.push_back(*mutation_iter);
				mutation_iter++;
			}
		}
	}
}

#ifdef SLIMGUI
// This method is used to record population statistics that are kept per generation for SLiMgui
void Population::SurveyPopulation(void)
{
	// Calculate mean fitness for this generation; this integrates the subpop mean fitness values from UpdateFitness()
	double totalFitness = 0.0;
	slim_popsize_t individualCount = 0;
	
	for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : *this)
	{ 
		Subpopulation *subpop = subpop_pair.second;
		
		totalFitness += subpop->parental_total_fitness_;
		individualCount += subpop->parent_subpop_size_;
	}
	
	double meanFitness = totalFitness / individualCount;
	slim_generation_t historyIndex = sim_.generation_ - 1;	// zero-base: the first generation we put something in is generation 1, and we put it at index 0
	
	// Add the mean fitness to the population history
	if (historyIndex >= fitness_history_length_)
	{
		slim_generation_t oldHistoryLength = fitness_history_length_;
		
		fitness_history_length_ = historyIndex + 1000;			// give some elbow room for expansion
		fitness_history_ = (double *)realloc(fitness_history_, fitness_history_length_ * sizeof(double));
		
		for (slim_generation_t i = oldHistoryLength; i < fitness_history_length_; ++i)
			fitness_history_[i] = NAN;
	}
	
	fitness_history_[historyIndex] = meanFitness;
}
#endif

#ifdef SLIMGUI
// This method is used to tally up histogram metrics that are kept per mutation type for SLiMgui
// FIXME the way that signed/unsigned and int sizes are handled for this buffer stuff seems pretty unsafe and careless
void Population::AddTallyForMutationTypeAndBinNumber(int p_mutation_type_index, int p_mutation_type_count, slim_generation_t p_bin_number, slim_generation_t **p_buffer, uint32_t *p_bufferBins)
{
	slim_generation_t *buffer = *p_buffer;
	uint32_t bufferBins = *p_bufferBins;
	
	if (p_bin_number >= (int64_t)bufferBins)
	{
		int oldEntryCount = bufferBins * p_mutation_type_count;
		
		bufferBins = static_cast<uint32_t>(ceil((p_bin_number + 1) / 128.0) * 128.0);			// give ourselves some headroom so we're not reallocating too often
		int newEntryCount = bufferBins * p_mutation_type_count;
		
		buffer = (slim_generation_t *)realloc(buffer, newEntryCount * sizeof(slim_generation_t));
		
		// Zero out the new entries; the compiler should be smart enough to optimize this...
		for (int i = oldEntryCount; i < newEntryCount; ++i)
			buffer[i] = 0;
		
		// Since we reallocated the buffer, we need to set it back through our pointer parameters
		*p_buffer = buffer;
		*p_bufferBins = bufferBins;
	}
	
	// Add a tally to the appropriate bin
	(buffer[p_mutation_type_index + p_bin_number * p_mutation_type_count])++;
}
#endif

void Population::RecalculateFitness(slim_generation_t p_generation)
{
	// calculate the fitnesses of the parents and make lookup tables; the main thing we do here is manage the fitness() callbacks
	// as per the SLiM design spec, we get the list of callbacks once, and use that list throughout this stage, but we construct
	// subsets of it for each subpopulation, so that UpdateFitness() can just use the callback list as given to it
	// note that generation+1 is used; we are computing fitnesses for the next generation
	std::vector<SLiMEidosBlock*> fitness_callbacks = sim_.ScriptBlocksMatching(p_generation, SLiMEidosBlockType::SLiMEidosFitnessCallback, -1, -1);
	bool no_active_callbacks = true;
	
	for (SLiMEidosBlock *callback : fitness_callbacks)
		if (callback->active_)
		{
			no_active_callbacks = false;
			break;
		}
	
	if (no_active_callbacks)
	{
		std::vector<SLiMEidosBlock*> no_fitness_callbacks;
		
		for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : *this)
			subpop_pair.second->UpdateFitness(no_fitness_callbacks);
	}
	else
	{
		for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : *this)
		{
			slim_objectid_t subpop_id = subpop_pair.first;
			Subpopulation *subpop = subpop_pair.second;
			std::vector<SLiMEidosBlock*> subpop_fitness_callbacks;
			
			// Get fitness callbacks that apply to this subpopulation
			for (SLiMEidosBlock *callback : fitness_callbacks)
			{
				slim_objectid_t callback_subpop_id = callback->subpopulation_id_;
				
				if ((callback_subpop_id == -1) || (callback_subpop_id == subpop_id))
					subpop_fitness_callbacks.push_back(callback);
			}
			
			// Update fitness values, using the callbacks
			subpop->UpdateFitness(subpop_fitness_callbacks);
		}
	}
}

// step forward a generation: remove fixed mutations, then make the children become the parents and update fitnesses
void Population::SwapGenerations(void)
{
	// go through all genomes and increment mutation reference counts; this updates total_genome_count_
	TallyMutationReferences();
	
	// remove any mutations that have been eliminated or have fixed
	RemoveFixedMutations();
	
	// check that the mutation registry does not have any "zombies" – mutations that have been removed and should no longer be there
#if DEBUG_MUTATION_ZOMBIES
	CheckMutationRegistry();
#endif
	
	// dispose of any freed subpops
	if (removed_subpops_.size())
	{
		for (auto removed_subpop : removed_subpops_)
			delete removed_subpop;
		
		removed_subpops_.clear();
	}
	
	// make children the new parents; each subpop flips its child_generation_valid flag at the end of this call
	for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : *this)
		subpop_pair.second->SwapChildAndParentGenomes();
	
	// flip our flag to indicate that the good genomes are now in the parental generation, and the next child generation is ready to be produced
	child_generation_valid_ = false;
	
	// do fitness recalculations with the new parental generation
	RecalculateFitness(sim_.Generation() + 1);
	sim_.DeregisterScheduledScriptBlocks();
	
#ifdef SLIMGUI
	SurveyPopulation();
#endif
}

// count the total number of times that each Mutation in the registry is referenced by a population, and return the maximum possible number of references (i.e. fixation)
// the only tricky thing is that if we're running in the GUI, we also tally up references within the selected subpopulations only
void Population::TallyMutationReferences(void)
{
	slim_refcount_t total_genome_count = 0;
#ifdef SLIMGUI
	slim_refcount_t gui_total_genome_count = 0;
#endif
	
	// first zero out the refcounts in all registered Mutation objects
	Mutation **registry_iter = mutation_registry_.begin_pointer();
	Mutation **registry_iter_end = mutation_registry_.end_pointer();
	
	for (; registry_iter != registry_iter_end; ++registry_iter)
	{
		(*registry_iter)->reference_count_ = 0;
#ifdef SLIMGUI
		(*registry_iter)->gui_reference_count_ = 0;
#endif
	}
	
	// then increment the refcounts through all pointers to Mutation in all genomes
	for (const std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : *this)			// subpopulations
	{
		Subpopulation *subpop = subpop_pair.second;
		
		// Particularly for SLiMgui, we need to be able to tally mutation references after the generations have been swapped, i.e.
		// when the parental generation is active and the child generation is invalid.
		slim_popsize_t subpop_genome_count = (child_generation_valid_ ? 2 * subpop->child_subpop_size_ : 2 * subpop->parent_subpop_size_);
		std::vector<Genome> &subpop_genomes = (child_generation_valid_ ? subpop->child_genomes_ : subpop->parent_genomes_);
		
#ifdef SLIMGUI
		// When running under SLiMgui, we need to tally up mutation references within the selected subpops, too; note
		// the else clause here drops outside of the #ifdef to the standard tally code.
		if (subpop->gui_selected_)
		{
			for (slim_popsize_t i = 0; i < subpop_genome_count; i++)							// child genomes
			{
				Genome &genome = subpop_genomes[i];
				
				if (!genome.IsNull())
				{
					Mutation **genome_iter = genome.begin_pointer();
					Mutation **genome_end_iter = genome.end_pointer();
					
					for (; genome_iter != genome_end_iter; ++genome_iter)
					{
						const Mutation *mutation = *genome_iter;
						
						(mutation->reference_count_)++;
						(mutation->gui_reference_count_)++;
					}
					
					total_genome_count++;	// count only non-null genomes to determine fixation
					gui_total_genome_count++;
				}
			}
		}
		else
#endif
		{
			for (slim_popsize_t i = 0; i < subpop_genome_count; i++)							// child genomes
			{
				Genome &genome = subpop_genomes[i];
				
				if (!genome.IsNull())
				{
					Mutation **genome_iter = genome.begin_pointer();
					Mutation **genome_end_iter = genome.end_pointer();
					
					for (; genome_iter != genome_end_iter; ++genome_iter)
						++((*genome_iter)->reference_count_);
					
					total_genome_count++;	// count only non-null genomes to determine fixation
				}
			}
		}
	}
	
	total_genome_count_ = total_genome_count;
#ifdef SLIMGUI
	gui_total_genome_count_ = gui_total_genome_count;
#endif
}

// handle negative fixation (remove from the registry) and positive fixation (convert to Substitution), using reference counts from TallyMutationReferences()
void Population::RemoveFixedMutations(void)
{
	Genome removed_mutation_accumulator;
	Genome fixed_mutation_accumulator;
#ifdef SLIMGUI
	int mutation_type_count = static_cast<int>(sim_.mutation_types_.size());
#endif
	
	// remove Mutation objects that are no longer referenced, freeing them; avoid using an iterator since it would be invalidated
	int registry_length = mutation_registry_.size();
	
	for (int i = 0; i < registry_length; ++i)
	{
		Mutation *mutation = mutation_registry_[i];
		slim_refcount_t reference_count = mutation->reference_count_;
		bool remove_mutation = false;
		
		if (reference_count == 0)
		{
#if DEBUG_MUTATIONS
			SLIM_ERRSTREAM << "Mutation unreferenced, will remove: " << mutation << endl;
#endif

#ifdef SLIMGUI
			// If we're running under SLiMgui, make a note of the lifetime of the mutation
			slim_generation_t loss_time = sim_.generation_ - mutation->generation_;
			int mutation_type_index = mutation->mutation_type_ptr_->mutation_type_index_;
			
			AddTallyForMutationTypeAndBinNumber(mutation_type_index, mutation_type_count, loss_time / 10, &mutation_loss_times_, &mutation_loss_gen_slots_);
#endif
			
			remove_mutation = true;
		}
		else if (reference_count == total_genome_count_)
		{
#if DEBUG_MUTATIONS
			SLIM_ERRSTREAM << "Mutation fixed, will substitute: " << mutation << endl;
#endif
			
#ifdef SLIMGUI
			// If we're running under SLiMgui, make a note of the fixation time of the mutation
			slim_generation_t fixation_time = sim_.generation_ - mutation->generation_;
			int mutation_type_index = mutation->mutation_type_ptr_->mutation_type_index_;
			
			AddTallyForMutationTypeAndBinNumber(mutation_type_index, mutation_type_count, fixation_time / 10, &mutation_fixation_times_, &mutation_fixation_gen_slots_);
#endif
			
			// add the fixed mutation to a vector, to be converted to a Substitution object below
			fixed_mutation_accumulator.insert_sorted_mutation(mutation);
			
			remove_mutation = true;
		}
		
		if (remove_mutation)
		{
			// We have an unreferenced mutation object, so we want to remove it quickly
			if (i == registry_length - 1)
			{
				mutation_registry_.pop_back();
				
				--registry_length;
			}
			else
			{
				Mutation *last_mutation = mutation_registry_[registry_length - 1];
				mutation_registry_[i] = last_mutation;
				mutation_registry_.pop_back();
				
				--registry_length;
				--i;	// revisit this index
			}
			
			// We can't delete the mutation yet, because we might need to make a Substitution object from it, so add it to a vector for deletion below
			removed_mutation_accumulator.push_back(mutation);
		}
	}
	
	// replace fixed mutations with Substitution objects
	if (fixed_mutation_accumulator.size() > 0)
	{
		for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : *this)		// subpopulations
		{
			for (slim_popsize_t i = 0; i < 2 * subpop_pair.second->child_subpop_size_; i++)	// child genomes
			{
				Genome *genome = &(subpop_pair.second->child_genomes_[i]);
				
				// Fixed mutations are removed by looking at refcounts, so fixed_mutation_accumulator is not needed here
				if (!genome->IsNull())
					genome->RemoveFixedMutations(total_genome_count_);
			}
		}
		
		slim_generation_t generation = sim_.Generation();
		
		for (int i = 0; i < fixed_mutation_accumulator.size(); i++)
			substitutions_.push_back(new Substitution(*(fixed_mutation_accumulator[i]), generation));
	}
	
	// now we can delete (or zombify) removed mutation objects
	if (removed_mutation_accumulator.size() > 0)
	{
		for (int i = 0; i < removed_mutation_accumulator.size(); i++)
		{
			const Mutation *mutation = removed_mutation_accumulator[i];
			
#if DEBUG_MUTATION_ZOMBIES
			const_cast<Mutation *>(mutation)->mutation_type_ptr_ = nullptr;		// render lethal
			mutation->reference_count_ = -1;									// zombie
#else
			delete mutation;
#endif
		}
	}
}

void Population::CheckMutationRegistry(void)
{
	Mutation **registry_iter = mutation_registry_.begin_pointer();
	Mutation **registry_iter_end = mutation_registry_.end_pointer();
	
	// first check that we don't have any zombies in our registry
	for (; registry_iter != registry_iter_end; ++registry_iter)
		if ((*registry_iter)->reference_count_ == -1)
			SLIM_ERRSTREAM << "Zombie found in registry with address " << (*registry_iter) << endl;
	
	// then check that we don't have any zombies in any genomes
	for (const std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : *this)		// subpopulations
	{
		Subpopulation *subpop = subpop_pair.second;
		slim_popsize_t subpop_genome_count = 2 * subpop->child_subpop_size_;
		std::vector<Genome> &subpop_genomes = subpop->child_genomes_;
		
		for (slim_popsize_t i = 0; i < subpop_genome_count; i++)							// child genomes
		{
			Genome &genome = subpop_genomes[i];
			
			if (!genome.IsNull())
			{
				Mutation **genome_iter = genome.begin_pointer();
				Mutation **genome_end_iter = genome.end_pointer();
				
				for (; genome_iter != genome_end_iter; ++genome_iter)
					if ((*genome_iter)->reference_count_ == -1)
						SLIM_ERRSTREAM << "Zombie found in genome with address " << (*genome_iter) << endl;
			}
		}
	}
}

// print all mutations and all genomes to a stream
void Population::PrintAll(std::ostream &p_out) const
{
	// This function is written to be able to print the population whether child_generation_valid is true or false.
	// This is a little tricky, so be careful when modifying this code!
	
	p_out << "Populations:" << endl;
	for (const std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : *this)
	{
		Subpopulation *subpop = subpop_pair.second;
		slim_popsize_t subpop_size = (child_generation_valid_ ? subpop->child_subpop_size_ : subpop->parent_subpop_size_);
		double subpop_sex_ratio = (child_generation_valid_ ? subpop->child_sex_ratio_ : subpop->parent_sex_ratio_);
		
		p_out << "p" << subpop_pair.first << " " << subpop_size;
		
		// SEX ONLY
		if (subpop->sex_enabled_)
			p_out << " S " << subpop_sex_ratio;
		else
			p_out << " H";
		
		p_out << endl;
	}
	
	multimap<const slim_position_t,Polymorphism> polymorphisms;
	
	// add all polymorphisms
	for (const std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : *this)			// go through all subpopulations
	{
		Subpopulation *subpop = subpop_pair.second;
		slim_popsize_t subpop_size = (child_generation_valid_ ? subpop->child_subpop_size_ : subpop->parent_subpop_size_);
		
		for (slim_popsize_t i = 0; i < 2 * subpop_size; i++)				// go through all children
		{
			Genome &genome = child_generation_valid_ ? subpop->child_genomes_[i] : subpop->parent_genomes_[i];
			
			if (!genome.IsNull())
			{
				for (int k = 0; k < genome.size(); k++)	// go through all mutations
					AddMutationToPolymorphismMap(&polymorphisms, *genome[k]);
			}
		}
	}
	
	// print all polymorphisms
	p_out << "Mutations:"  << endl;
	
	for (const std::pair<const slim_position_t,Polymorphism> &polymorphism_pair : polymorphisms)
		polymorphism_pair.second.print(p_out, polymorphism_pair.first);
	
	// print all individuals
	p_out << "Individuals:" << endl;
	
	for (const std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : *this)			// go through all subpopulations
	{
		Subpopulation *subpop = subpop_pair.second;
		slim_objectid_t subpop_id = subpop_pair.first;
		slim_popsize_t subpop_size = (child_generation_valid_ ? subpop->child_subpop_size_ : subpop->parent_subpop_size_);
		slim_popsize_t first_male_index = (child_generation_valid_ ? subpop->child_first_male_index_ : subpop->parent_first_male_index_);
		
		for (slim_popsize_t i = 0; i < subpop_size; i++)				// go through all children
		{
			p_out << "p" << subpop_id << ":i" << i;						// individual identifier
			
			if (subpop->sex_enabled_)
				p_out << ((i < first_male_index) ? " F " : " M ");		// sex: SEX ONLY
			else
				p_out << " H ";											// hermaphrodite
			
			p_out << "p" << subpop_id << ":" << (i * 2);				// genome identifier 1
			p_out << " p" << subpop_id << ":" << (i * 2 + 1);			// genome identifier 2
			p_out << endl;
		}
	}
	
	// print all genomes
	p_out << "Genomes:" << endl;
	
	for (const std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : *this)			// go through all subpopulations
	{
		Subpopulation *subpop = subpop_pair.second;
		slim_objectid_t subpop_id = subpop_pair.first;
		slim_popsize_t subpop_size = (child_generation_valid_ ? subpop->child_subpop_size_ : subpop->parent_subpop_size_);
		
		for (slim_popsize_t i = 0; i < 2 * subpop_size; i++)							// go through all children
		{
			Genome &genome = child_generation_valid_ ? subpop->child_genomes_[i] : subpop->parent_genomes_[i];
			
			p_out << "p" << subpop_id << ":" << i << " " << genome.GenomeType();
			
			if (genome.IsNull())
			{
				p_out << " <null>";
			}
			else
			{
				for (int k = 0; k < genome.size(); k++)								// go through all mutations
				{
					int id = FindMutationInPolymorphismMap(polymorphisms, *genome[k]);
					p_out << " " << id; 
				}
			}
			
			p_out << endl;
		}
	}
}

// print sample of p_sample_size genomes from subpopulation p_subpop_id
void Population::PrintSample(Subpopulation &p_subpop, slim_popsize_t p_sample_size, IndividualSex p_requested_sex) const
{
	// This function is written to be able to print the population whether child_generation_valid is true or false.
	
	std::vector<Genome> &subpop_genomes = (child_generation_valid_ ? p_subpop.child_genomes_ : p_subpop.parent_genomes_);
	
	if (p_requested_sex == IndividualSex::kFemale && p_subpop.modeled_chromosome_type_ == GenomeType::kYChromosome)
		EIDOS_TERMINATION << "ERROR (Population::PrintSample): called to output Y chromosomes from females." << eidos_terminate();
	
	// assemble a sample (with replacement, for statistics) and get the polymorphisms within it
	std::vector<slim_popsize_t> sample; 
	multimap<const slim_position_t,Polymorphism> polymorphisms;
	
	for (slim_popsize_t s = 0; s < p_sample_size; s++)
	{
		slim_popsize_t j;
		
		// Scan for a genome that is not null and that belongs to an individual of the requested sex
		do {
			j = static_cast<slim_popsize_t>(gsl_rng_uniform_int(gEidos_rng, subpop_genomes.size()));		// select a random genome (not a random individual)
		} while (subpop_genomes[j].IsNull() || (p_subpop.sex_enabled_ && p_requested_sex != IndividualSex::kUnspecified && p_subpop.SexOfIndividual(j / 2) != p_requested_sex));
		
		sample.push_back(j);
		
		for (int k = 0; k < subpop_genomes[j].size(); k++)			// go through all mutations
			AddMutationToPolymorphismMap(&polymorphisms, *subpop_genomes[j][k]);
	}
	
	// print the sample's polymorphisms
	SLIM_OUTSTREAM << "Mutations:"  << endl;
	
	for (const std::pair<const slim_position_t,Polymorphism> &polymorphism_pair : polymorphisms) 
		polymorphism_pair.second.print(SLIM_OUTSTREAM, polymorphism_pair.first);
	
	// print the sample's genomes
	SLIM_OUTSTREAM << "Genomes:" << endl;
	
	for (unsigned int j = 0; j < sample.size(); j++)														// go through all individuals
	{
		Genome &genome = subpop_genomes[sample[j]];
		
		SLIM_OUTSTREAM << "p" << p_subpop.subpopulation_id_ << ":" << sample[j] << " " << genome.GenomeType();
		
		if (genome.IsNull())
		{
			SLIM_OUTSTREAM << " <null>";
		}
		else
		{
			for (int k = 0; k < genome.size(); k++)	// go through all mutations
			{
				int mutation_id = FindMutationInPolymorphismMap(polymorphisms, *genome[k]);
				
				SLIM_OUTSTREAM << " " << mutation_id;
			}
		}
		
		SLIM_OUTSTREAM << endl;
	}
}

// print sample of p_sample_size genomes from subpopulation p_subpop_id, using "ms" format
void Population::PrintSample_ms(Subpopulation &p_subpop, slim_popsize_t p_sample_size, const Chromosome &p_chromosome, IndividualSex p_requested_sex) const
{
	// This function is written to be able to print the population whether child_generation_valid is true or false.
	
	std::vector<Genome> &subpop_genomes = (child_generation_valid_ ? p_subpop.child_genomes_ : p_subpop.parent_genomes_);
	
	if (p_requested_sex == IndividualSex::kFemale && p_subpop.modeled_chromosome_type_ == GenomeType::kYChromosome)
		EIDOS_TERMINATION << "ERROR (Population::PrintSample_ms): called to output Y chromosomes from females." << eidos_terminate();
	
	// assemble a sample (with replacement, for statistics) and get the polymorphisms within it
	std::vector<slim_popsize_t> sample; 
	multimap<const slim_position_t,Polymorphism> polymorphisms;
	
	for (slim_popsize_t s = 0; s < p_sample_size; s++)
	{
		slim_popsize_t j;
		
		// Scan for a genome that is not null and that belongs to an individual of the requested sex
		do {
			j = static_cast<slim_popsize_t>(gsl_rng_uniform_int(gEidos_rng, subpop_genomes.size()));		// select a random genome (not a random individual)
		} while (subpop_genomes[j].IsNull() || (p_subpop.sex_enabled_ && p_requested_sex != IndividualSex::kUnspecified && p_subpop.SexOfIndividual(j / 2) != p_requested_sex));
		
		sample.push_back(j);
		
		for (int k = 0; k < subpop_genomes[j].size(); k++)			// go through all mutations
			AddMutationToPolymorphismMap(&polymorphisms, *subpop_genomes[j][k]);
	}
	
	// print header
	SLIM_OUTSTREAM << endl << "//" << endl << "segsites: " << polymorphisms.size() << endl;
	
	// print the sample's positions
	if (polymorphisms.size() > 0)
	{
		SLIM_OUTSTREAM << "positions:";
		
		for (const std::pair<const slim_position_t,Polymorphism> &polymorphism_pair : polymorphisms) 
			SLIM_OUTSTREAM << " " << std::fixed << std::setprecision(7) << static_cast<double>(polymorphism_pair.first) / p_chromosome.last_position_;	// this prints positions as being in the interval [0,1], which Philipp decided was the best policy
		
		SLIM_OUTSTREAM << endl;
	}
	
	// print the sample's genotypes
	for (unsigned int j = 0; j < sample.size(); j++)														// go through all individuals
	{
		string genotype(polymorphisms.size(), '0'); // fill with 0s
		
		for (int k = 0; k < subpop_genomes[sample[j]].size(); k++)	// go through all mutations
		{
			const Mutation *mutation = subpop_genomes[sample[j]][k];
			int genotype_string_position = 0;
			
			for (const std::pair<const slim_position_t,Polymorphism> &polymorphism_pair : polymorphisms) 
			{
				if (polymorphism_pair.first == mutation->position_ && polymorphism_pair.second.mutation_type_ptr_ == mutation->mutation_type_ptr_ && polymorphism_pair.second.selection_coeff_ == mutation->selection_coeff_)
				{
					// mark this polymorphism as present in the genome, and move on since this mutation can't also match any other polymorphism
					genotype.replace(genotype_string_position, 1, "1");
					break;
				}
				
				genotype_string_position++;
			}
		}
		
		SLIM_OUTSTREAM << genotype << endl;
	}
}






































































