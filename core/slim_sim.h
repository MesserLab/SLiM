//
//  slim_sim.h
//  SLiM
//
//  Created by Ben Haller on 12/26/14.
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
#include "script.h"
#include "script_value.h"
#include "script_functions.h"
#include "slim_script_block.h"


class ScriptInterpreter;


class SLiMSim : public ScriptObjectElement
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.
	
#ifdef SLIMGUI
public:
	
	bool simulationValid = true;													// set to false if a terminating condition is encountered while running in SLiMgui
#else
private:
#endif
	
	int time_start_ = 0;															// the first generation number for which the simulation will run
	int time_duration_ = 0;															// the duration for which the simulation will run, in generations
	int generation_ = 0;															// the current generation reached in simulation
	Chromosome chromosome_;															// the chromosome, which defines genomic elements
	Population population_;															// the population, which contains sub-populations
	std::map<int,MutationType*> mutation_types_;									// OWNED POINTERS: this map is the owner of all allocated MutationType objects
	std::map<int,GenomicElementType*> genomic_element_types_;						// OWNED POINTERS: this map is the owner of all allocated MutationType objects
	int rng_seed_ = 0;																// random number generator seed
	bool rng_seed_supplied_to_constructor_ = false;									// true if the RNG seed was supplied, which means it overrides other RNG seed sources
	
	// SEX ONLY: sex-related instance variables
	bool sex_enabled_ = false;														// true if sex is tracked for individuals; if false, all individuals are considered hermaphroditic
	GenomeType modeled_chromosome_type_ = GenomeType::kAutosome;					// the type of the chromosome being modeled; other chromosome types might still be instantiated (Y, if X is modeled, e.g.)
	double x_chromosome_dominance_coeff_ = 1.0;										// the dominance coefficient for heterozygosity at the X locus (i.e. males); this is global
	
	Script *script_;																// OWNED POINTER: the whole input file script
	std::vector<SLiMScriptBlock*> script_blocks_;									// OWNED POINTERS: script blocks, both from the input file script and programmatic
	std::vector<SLiMScriptBlock*> scheduled_deregistrations_;						// NOT OWNED: blocks in script_blocks_ that are scheduled for deregistration
	std::vector<FunctionSignature*> sim_0_signatures;								// OWNED POINTERS: SLiMscript function signatures
	
	// private initialization methods
	void InitializePopulationFromFile(const char *p_file);							// initialize the population from the information in the file given
	void InitializeFromFile(std::istream &infile);									// parse a input file and set up the simulation state from its contents
	
	// initialization completeness check counts; used only in generation 0
	int num_mutation_types;
	int num_mutation_rates;
	int num_genomic_element_types;
	int num_genomic_elements;
	int num_recombination_rates;
	int num_gene_conversions;
	int num_generations;
	int num_sex_declarations;	// SEX ONLY; used to check for sex vs. non-sex errors in the file, so the #SEX tag must come before any reliance on SEX ONLY features
	
	// change flags; used only by SLiMgui, to know that something has changed and a UI update is needed; start as true to provoke an initial display
	bool mutation_types_changed_ = true;
	bool genomic_element_types_changed_ = true;
	bool chromosome_changed_ = true;
	bool scripts_changed_ = true;
	
	SymbolTableEntry *self_symbol_ = nullptr;					// OWNED POINTER: SymbolTableEntry object for fast setup of the symbol table
	
public:
	
	SLiMSim(const SLiMSim&) = delete;												// no copying
	SLiMSim& operator=(const SLiMSim&) = delete;									// no copying
	SLiMSim(std::istream &infile, int *p_override_seed_ptr = nullptr);				// construct a SLiMSim from an input stream, with an optional RNG seed value
	SLiMSim(const char *p_input_file, int *p_override_seed_ptr = nullptr);			// construct a SLiMSim from an input file, with an optional RNG seed value
	~SLiMSim(void);																	// destructor
	
	// Managing script blocks; these two methods should be used as a matched pair, bracketing each generation stage that calls out to script
	std::vector<SLiMScriptBlock*> ScriptBlocksMatching(int p_generation, SLiMScriptBlockType p_event_type, int p_mutation_type_id, int p_subpopulation_id);
	void DeregisterScheduledScriptBlocks(void);
	
	void RunZeroGeneration(void);													// run generation zero and check for complete initialization
	bool RunOneGeneration(void);													// run a single simulation generation and advance the generation counter; returns false if the simulation is over
	void RunToEnd(void);															// run the simulation to the end
	
	// accessors
	inline int Generation(void) const												{ return generation_; }
	inline Chromosome &Chromosome(void)												{ return chromosome_; }
	inline Population &Population(void)												{ return population_; }
	inline const std::map<int,MutationType*> &MutationTypes(void) const				{ return mutation_types_; }
	
	inline bool SexEnabled(void) const												{ return sex_enabled_; }
	inline GenomeType ModeledChromosomeType(void) const								{ return modeled_chromosome_type_; }
	inline double XDominanceCoefficient(void) const									{ return x_chromosome_dominance_coeff_; }
	
	//
	// SLiMscript support
	//
	void GenerateCachedSymbolTableEntry(void);
	inline SymbolTableEntry *CachedSymbolTableEntry(void) { if (!self_symbol_) GenerateCachedSymbolTableEntry(); return self_symbol_; };
	
	static ScriptValue *StaticFunctionDelegationFunnel(void *delegate, std::string const &p_function_name, std::vector<ScriptValue*> const &p_arguments, std::ostream &p_output_stream, ScriptInterpreter &p_interpreter);
	ScriptValue *FunctionDelegationFunnel(std::string const &p_function_name, std::vector<ScriptValue*> const &p_arguments, std::ostream &p_output_stream, ScriptInterpreter &p_interpreter);
	
	void InjectIntoInterpreter(ScriptInterpreter &p_interpreter, SLiMScriptBlock *p_script_block);	// add SLiM constructs to an interpreter instance
	std::vector<FunctionSignature*> *InjectedFunctionSignatures(void);
	
	virtual std::string ElementType(void) const;
	
	virtual std::vector<std::string> ReadOnlyMembers(void) const;
	virtual std::vector<std::string> ReadWriteMembers(void) const;
	virtual ScriptValue *GetValueForMember(const std::string &p_member_name);
	virtual void SetValueForMember(const std::string &p_member_name, ScriptValue *p_value);
	
	virtual std::vector<std::string> Methods(void) const;
	virtual const FunctionSignature *SignatureForMethod(std::string const &p_method_name) const;
	virtual ScriptValue *ExecuteMethod(std::string const &p_method_name, std::vector<ScriptValue*> const &p_arguments, std::ostream &p_output_stream, ScriptInterpreter &p_interpreter);
};


#endif /* defined(__SLiM__slim_sim__) */












































































