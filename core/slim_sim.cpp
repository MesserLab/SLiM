//
//  slim_sim.cpp
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


#include "slim_sim.h"
#include "script_test.h"

#include <iostream>
#include <fstream>
#include <stdexcept>

// SLiMscript headers for InjectIntoInterpreter()
#include "script_interpreter.h"


using std::multimap;


SLiMSim::SLiMSim(std::istream &infile, int *p_override_seed_ptr)
{
	// track the random number seed given, if there is one
	if (p_override_seed_ptr)
	{
		rng_seed_ = *p_override_seed_ptr;
		rng_seed_supplied_to_constructor_ = true;
	}
	else
	{
		rng_seed_ = 0;
		rng_seed_supplied_to_constructor_ = false;
	}
	
	// check the input file for syntactic correctness
	infile.clear();
	infile.seekg(0, std::fstream::beg);
	std::string checkErrors = CheckInputFile(infile);
	
	if (checkErrors.length())
		SLIM_TERMINATION << checkErrors << slim_terminate();
	
	// read all configuration information from the input file
	infile.clear();
	infile.seekg(0, std::fstream::beg);
	InitializeFromFile(infile);
	
	// evolve over t generations
	SLIM_OUTSTREAM << "\n// Starting run with <start> <duration>:\n" << time_start_ << " " << time_duration_ << "\n" << std::endl;
	
	// start at the beginning
	generation_ = time_start_;
}

SLiMSim::SLiMSim(const char *p_input_file, int *p_override_seed_ptr)
{
	// track the random number seed given, if there is one
	if (p_override_seed_ptr)
	{
		rng_seed_ = *p_override_seed_ptr;
		rng_seed_supplied_to_constructor_ = true;
	}
	else
	{
		rng_seed_ = 0;
		rng_seed_supplied_to_constructor_ = false;
	}
	
	// Open our file stream
	std::ifstream infile(p_input_file);
	
	if (!infile.is_open())
	{
		SLIM_TERMINATION << std::endl;
		SLIM_TERMINATION << "ERROR (parameter file): could not open: " << p_input_file << std::endl << std::endl;
		SLIM_TERMINATION << slim_terminate();
	}
	
	// check the input file for syntactic correctness
	infile.seekg(0, std::fstream::beg);
	std::string checkErrors = CheckInputFile(infile);
	
	if (checkErrors.length())
		SLIM_TERMINATION << checkErrors << slim_terminate();
	
	// initialize simulation parameters
	input_parameters_.push_back("#INPUT PARAMETER FILE");
	input_parameters_.push_back(p_input_file);
	
	// read all configuration information from the input file
	infile.seekg(0, std::fstream::beg);
	InitializeFromFile(infile);
	
	// evolve over t generations
	SLIM_OUTSTREAM << "\n// Starting run with <start> <duration>:\n" << time_start_ << " " << time_duration_ << "\n" << std::endl;
	
	// start at the beginning
	generation_ = time_start_;
}

SLiMSim::~SLiMSim(void)
{
	//SLIM_ERRSTREAM << "SLiMSim::~SLiMSim" << std::endl;
	
	for (auto event : events_)
		delete event.second;
	
	for (auto output_event : outputs_)
		delete output_event.second;
	
	for (auto script_event : scripts_)
		delete script_event;
	
	for (auto introduced_mutation : introduced_mutations_)
		delete introduced_mutation.second;
	
	for (auto partial_sweep : partial_sweeps_)
		delete partial_sweep;
	
	for (auto mutation_type : mutation_types_)
		delete mutation_type.second;
	
	for (auto genomic_element_type : genomic_element_types_)
		delete genomic_element_type.second;
	
	// We should not have any interpreter instances that still refer to us
	delete simFunctionSig;
}

// a static member function is used as a funnel, so that we can get a pointer to function for it
ScriptValue *SLiMSim::StaticFunctionDelegationFunnel(void *delegate, std::string const &p_function_name, std::vector<ScriptValue*> const &p_arguments, std::ostream &p_output_stream, ScriptInterpreter &p_interpreter)
{
	SLiMSim *sim = static_cast<SLiMSim *>(delegate);
	
	return sim->FunctionDelegationFunnel(p_function_name, p_arguments, p_output_stream, p_interpreter);
}

// the static member function calls this member function; now we're completely in context and can execute the function
ScriptValue *SLiMSim::FunctionDelegationFunnel(std::string const &p_function_name, std::vector<ScriptValue*> const &p_arguments, std::ostream &p_output_stream, ScriptInterpreter &p_interpreter)
{
	if (p_function_name.compare("Sim") == 0)
	{
		p_output_stream << "This is Sim()!" << std::endl;
		return ScriptValue_NULL::ScriptValue_NULL_Invisible();
	}
	
	return nullptr;
}

void SLiMSim::InjectIntoInterpreter(ScriptInterpreter &p_interpreter)
{
	// Set up global symbols for things that live within SLiM
	SymbolTable &global_symbols = p_interpreter.BorrowSymbolTable();
	
	// A constant for quick reference; we have to remove it each time because you can't overwrite a constant value
	global_symbols.RemoveValueForMember("generation");
	global_symbols.SetConstantForMember("generation", new ScriptValue_Int(generation_));
	
	// Add our functions to the interpreter's function map; we allocate our own FunctionSignature objects since they point to us
	if (!simFunctionSig)
	{
		simFunctionSig = new FunctionSignature("Sim", FunctionIdentifier::kDelegatedFunction, kScriptValueMaskNULL, SLiMSim::StaticFunctionDelegationFunnel, static_cast<void *>(this), "SLiM");
	}
	
	p_interpreter.RegisterSignature(simFunctionSig);
}

bool SLiMSim::RunOneGeneration(void)
{
#ifdef SLIMGUI
	if (simulationValid)
	{
#endif
		if (generation_ < time_start_ + time_duration_)
		{
#ifdef SLIMGUI
			try
			{
#endif
				// execute demographic and substructure events in this generation 
				std::pair<multimap<const int,Event*>::iterator,multimap<const int,Event*>::iterator> event_range = events_.equal_range(generation_);
				
				for (multimap<const int,Event*>::iterator eventsIterator = event_range.first; eventsIterator != event_range.second; eventsIterator++)
					population_.ExecuteEvent(*eventsIterator->second, generation_, chromosome_, *this, &tracked_mutations_);
				
				// evolve all subpopulations
				for (const std::pair<const int,Subpopulation*> &subpop_pair : population_)
					population_.EvolveSubpopulation(subpop_pair.first, chromosome_, generation_);
				
				// introduce user-defined mutations
				std::pair<multimap<const int,const IntroducedMutation*>::iterator,multimap<const int,const IntroducedMutation*>::iterator> introd_mut_range = introduced_mutations_.equal_range(generation_);
				
				for (multimap<const int,const IntroducedMutation*>::iterator introduced_mutations_iter = introd_mut_range.first; introduced_mutations_iter != introd_mut_range.second; introduced_mutations_iter++)
					population_.IntroduceMutation(*introduced_mutations_iter->second);
				
				// execute script events
				for (auto script_event : scripts_)
				{
					if ((generation_ >= script_event->generation_start_) && (generation_ <= script_event->generation_end_))
						population_.ExecuteScript(script_event, generation_, chromosome_, *this);
				}
				
				// execute output events
				std::pair<multimap<const int,Event*>::iterator,multimap<const int,Event*>::iterator> output_event_range = outputs_.equal_range(generation_);
				
				for (multimap<const int,Event*>::iterator outputsIterator = output_event_range.first; outputsIterator != output_event_range.second; outputsIterator++)
					population_.ExecuteEvent(*outputsIterator->second, generation_, chromosome_, *this, &tracked_mutations_);
				
				// track particular mutation-types and set s = 0 for partial sweeps when completed
				if (tracked_mutations_.size() > 0 || partial_sweeps_.size() > 0)
					population_.TrackMutations(generation_, tracked_mutations_, &partial_sweeps_);
				
				// swap generations
				population_.SwapGenerations(*this);
				
				// advance the generation counter as soon as the generation is done
				generation_++;
				return (generation_ < time_start_ + time_duration_);
#ifdef SLIMGUI
			}
			catch (std::runtime_error err)
			{
				simulationValid = false;
				
				return false;
			}
#endif
		}
#ifdef SLIMGUI
	}
#endif
	
	return false;
}

void SLiMSim::RunToEnd(void)
{
	while (generation_ < time_start_ + time_duration_)
		RunOneGeneration();
}





































































