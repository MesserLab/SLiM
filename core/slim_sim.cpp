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

#ifndef SLIMCORE
#include "script_test.h"
#endif

#include <iostream>
#include <fstream>
#include <stdexcept>

#ifndef SLIMCORE
#include "script_interpreter.h"
#endif


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
	
	for (auto introduced_mutation : introduced_mutations_)
		delete introduced_mutation.second;
	
	for (auto partial_sweep : partial_sweeps_)
		delete partial_sweep;
	
	for (auto mutation_type : mutation_types_)
		delete mutation_type.second;
	
	for (auto genomic_element_type : genomic_element_types_)
		delete genomic_element_type.second;
	
#ifndef SLIMCORE
	for (auto script_event : scripts_)
		delete script_event;
	
	// We should not have any interpreter instances that still refer to us
	delete simFunctionSig;
#endif
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
				std::pair<multimap<const int,SLIMCONST IntroducedMutation*>::iterator,multimap<const int,SLIMCONST IntroducedMutation*>::iterator> introd_mut_range = introduced_mutations_.equal_range(generation_);
				
				for (multimap<const int,SLIMCONST IntroducedMutation*>::iterator introduced_mutations_iter = introd_mut_range.first; introduced_mutations_iter != introd_mut_range.second; introduced_mutations_iter++)
					population_.IntroduceMutation(*introduced_mutations_iter->second);
				
#ifndef SLIMCORE
				// execute script events
				for (auto script_event : scripts_)
				{
					if ((generation_ >= script_event->generation_start_) && (generation_ <= script_event->generation_end_))
						population_.ExecuteScript(script_event, generation_, chromosome_, *this);
				}
#endif
				
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


#ifndef SLIMCORE
//
//	SLiMscript support
//
#pragma mark SLiMscript support

// a static member function is used as a funnel, so that we can get a pointer to function for it
ScriptValue *SLiMSim::StaticFunctionDelegationFunnel(void *delegate, std::string const &p_function_name, std::vector<ScriptValue*> const &p_arguments, std::ostream &p_output_stream, ScriptInterpreter &p_interpreter)
{
	SLiMSim *sim = static_cast<SLiMSim *>(delegate);
	
	return sim->FunctionDelegationFunnel(p_function_name, p_arguments, p_output_stream, p_interpreter);
}

// the static member function calls this member function; now we're completely in context and can execute the function
ScriptValue *SLiMSim::FunctionDelegationFunnel(std::string const &p_function_name, std::vector<ScriptValue*> const &p_arguments, std::ostream &p_output_stream, ScriptInterpreter &p_interpreter)
{
	// a test function, for proof of concept; not registered below at present
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
	global_symbols.RemoveValueForSymbol("sim", true);
	global_symbols.SetConstantForSymbol("sim", new ScriptValue_Object(this));
	
	// Add constants for our genomic element types
	for (auto getype_pair : genomic_element_types_)
	{
		GenomicElementType *getype_type = getype_pair.second;
		int id = getype_type->genomic_element_type_id_;
		std::ostringstream getype_stream;
		
		getype_stream << "g" << id;
		
		std::string getype_string = getype_stream.str();
		
		global_symbols.RemoveValueForSymbol(getype_string, true);
		global_symbols.SetConstantForSymbol(getype_string, new ScriptValue_Object(getype_type));
	}
	
	// Add constants for our mutation types
	for (auto mut_type_pair : mutation_types_)
	{
		MutationType *mut_type = mut_type_pair.second;
		int id = mut_type->mutation_type_id_;
		std::ostringstream mut_type_stream;
		
		mut_type_stream << "m" << id;
		
		std::string mut_type_string = mut_type_stream.str();
		
		global_symbols.RemoveValueForSymbol(mut_type_string, true);
		global_symbols.SetConstantForSymbol(mut_type_string, new ScriptValue_Object(mut_type));
	}
	
	// Add constants for our mutation types
	for (auto pop_pair : population_)
	{
		Subpopulation *subpop = pop_pair.second;
		int id = pop_pair.first;
		std::ostringstream subpop_stream;
		
		subpop_stream << "p" << id;
		
		std::string subpop_string = subpop_stream.str();
		
		global_symbols.RemoveValueForSymbol(subpop_string, true);
		global_symbols.SetConstantForSymbol(subpop_string, new ScriptValue_Object(subpop));
	}
	
	// Add our functions to the interpreter's function map; we allocate our own FunctionSignature objects since they point to us
	if (!simFunctionSig)
	{
		simFunctionSig = new FunctionSignature("Sim", FunctionIdentifier::kDelegatedFunction, kScriptValueMaskNULL, SLiMSim::StaticFunctionDelegationFunnel, static_cast<void *>(this), "SLiM");
	}
	
	//p_interpreter.RegisterSignature(simFunctionSig);
}

std::string SLiMSim::ElementType(void) const
{
	return "SLiMSim";
}

std::vector<std::string> SLiMSim::ReadOnlyMembers(void) const
{
	std::vector<std::string> constants = ScriptObjectElement::ReadOnlyMembers();
	
	constants.push_back("chromosome");			// chromosome_
	constants.push_back("chromosomeType");		// modeled_chromosome_type_
	constants.push_back("genomicElementTypes");	// genomic_element_types_
	constants.push_back("mutations");			// population_.mutation_registry_
	constants.push_back("mutationTypes");		// mutation_types_
	constants.push_back("parameters");			// input_parameters_
	constants.push_back("populations");			// population_
	constants.push_back("sexEnabled");			// sex_enabled_
	constants.push_back("start");				// time_start_
	constants.push_back("substitutions");		// population_.substitutions_
	
	return constants;
}

std::vector<std::string> SLiMSim::ReadWriteMembers(void) const
{
	std::vector<std::string> variables = ScriptObjectElement::ReadWriteMembers();
	
	if (sex_enabled_ && (modeled_chromosome_type_ == GenomeType::kXChromosome))
		variables.push_back("dominanceCoeffX");		// x_chromosome_dominance_coeff_; defined only when we're modeling sex chromosomes
	variables.push_back("duration");			// time_duration_
	variables.push_back("generation");			// generation_
	variables.push_back("randomSeed");			// rng_seed_
	
	return variables;
}

ScriptValue *SLiMSim::GetValueForMember(const std::string &p_member_name)
{
	// constants
	if (p_member_name.compare("chromosome") == 0)
		return new ScriptValue_Object(&chromosome_);
	if (p_member_name.compare("chromosomeType") == 0)
	{
		switch (modeled_chromosome_type_)
		{
			case GenomeType::kAutosome:		return new ScriptValue_String("autosome");
			case GenomeType::kXChromosome:	return new ScriptValue_String("X chromosome");
			case GenomeType::kYChromosome:	return new ScriptValue_String("Y chromosome");
		}
	}
	if (p_member_name.compare("genomicElementTypes") == 0)
	{
		ScriptValue_Object *vec = new ScriptValue_Object();
		
		for (auto ge_type = genomic_element_types_.begin(); ge_type != genomic_element_types_.end(); ++ge_type)
			vec->PushElement(ge_type->second);
		
		return vec;
	}
	if (p_member_name.compare("mutations") == 0)
	{
		ScriptValue_Object *vec = new ScriptValue_Object();
		Genome &mutation_registry = population_.mutation_registry_;
		int mutation_count = mutation_registry.size();
		
		for (int mut_index = 0; mut_index < mutation_count; ++mut_index)
			vec->PushElement(mutation_registry[mut_index]);
		
		return vec;
	}
	if (p_member_name.compare("mutationTypes") == 0)
	{
		ScriptValue_Object *vec = new ScriptValue_Object();
		
		for (auto mutation_type = mutation_types_.begin(); mutation_type != mutation_types_.end(); ++mutation_type)
			vec->PushElement(mutation_type->second);
		
		return vec;
	}
	if (p_member_name.compare("parameters") == 0)
		return new ScriptValue_String(input_parameters_);
	if (p_member_name.compare("populations") == 0)
	{
		ScriptValue_Object *vec = new ScriptValue_Object();
		
		for (auto pop = population_.begin(); pop != population_.end(); ++pop)
			vec->PushElement(pop->second);
		
		return vec;
	}
	if (p_member_name.compare("sexEnabled") == 0)
		return new ScriptValue_Logical(sex_enabled_);
	if (p_member_name.compare("start") == 0)
		return new ScriptValue_Int(time_start_);
	if (p_member_name.compare("substitutions") == 0)
	{
		ScriptValue_Object *vec = new ScriptValue_Object();
		
		for (auto sub_iter = population_.substitutions_.begin(); sub_iter != population_.substitutions_.end(); ++sub_iter)
			vec->PushElement(*sub_iter);
		
		return vec;
	}
	
	// variables
	if ((p_member_name.compare("dominanceCoeffX") == 0) && sex_enabled_ && (modeled_chromosome_type_ == GenomeType::kXChromosome))
		return new ScriptValue_Float(x_chromosome_dominance_coeff_);
	if (p_member_name.compare("duration") == 0)
		return new ScriptValue_Int(time_duration_);
	if (p_member_name.compare("generation") == 0)
		return new ScriptValue_Int(generation_);
	if (p_member_name.compare("randomSeed") == 0)
		return new ScriptValue_Int(rng_seed_);
	
	return ScriptObjectElement::GetValueForMember(p_member_name);
}

void SLiMSim::SetValueForMember(const std::string &p_member_name, ScriptValue *p_value)
{
	if (p_member_name.compare("generationDuration") == 0)
	{
		TypeCheckValue(__func__, p_member_name, p_value, kScriptValueMaskInt);
		
		int64_t value = p_value->IntAtIndex(0);
		RangeCheckValue(__func__, p_member_name, (value > 0) && (value <= 1000000000));
		
		time_duration_ = (int)value;
		return;
	}
	
	if (p_member_name.compare("generation") == 0)
	{
		TypeCheckValue(__func__, p_member_name, p_value, kScriptValueMaskInt);
		
		int64_t value = p_value->IntAtIndex(0);
		RangeCheckValue(__func__, p_member_name, (value >= time_start_) && (value <= time_start_ + time_duration_));
		
		generation_ = (int)value;
		return;
	}
	
	if ((p_member_name.compare("dominanceCoeffX") == 0) && sex_enabled_ && (modeled_chromosome_type_ == GenomeType::kXChromosome))
	{
		TypeCheckValue(__func__, p_member_name, p_value, kScriptValueMaskInt | kScriptValueMaskFloat);
		
		double value = p_value->FloatAtIndex(0);
		
		x_chromosome_dominance_coeff_ = value;
		return;
	}
	
	if (p_member_name.compare("randomSeed") == 0)
	{
		TypeCheckValue(__func__, p_member_name, p_value, kScriptValueMaskInt);
		
		int64_t value = p_value->IntAtIndex(0);
		
		rng_seed_ = (int)value;
		
		// Reseed generator; see InitializeRNGFromSeed()
		gsl_rng_set(g_rng, static_cast<long>(rng_seed_));
		g_random_bool_bit_counter = 0;
		g_random_bool_bit_buffer = 0;
		
		return;
	}
	
	// Check for constants that the user should not try to set
	if ((p_member_name.compare("generationStart") == 0) ||
		(p_member_name.compare("sexEnabled") == 0) ||
		(p_member_name.compare("chromosome") == 0) ||
		(p_member_name.compare("genomicElementTypes") == 0) ||
		(p_member_name.compare("mutationTypes") == 0) ||
		(p_member_name.compare("populations") == 0) ||
		(p_member_name.compare("chromosomeType") == 0))
		ConstantSetError(__func__, p_member_name);
	
	return ScriptObjectElement::SetValueForMember(p_member_name, p_value);
}

std::vector<std::string> SLiMSim::Methods(void) const
{
	std::vector<std::string> methods = ScriptObjectElement::Methods();
	
	return methods;
}

const FunctionSignature *SLiMSim::SignatureForMethod(std::string const &p_method_name) const
{
	return ScriptObjectElement::SignatureForMethod(p_method_name);
}

ScriptValue *SLiMSim::ExecuteMethod(std::string const &p_method_name, std::vector<ScriptValue*> const &p_arguments, std::ostream &p_output_stream, ScriptInterpreter &p_interpreter)
{
	return ScriptObjectElement::ExecuteMethod(p_method_name, p_arguments, p_output_stream, p_interpreter);
}

/*
	Population population_;															// the population, which contains sub-populations
	std::multimap<const int,Event*> events_;										// OWNED POINTERS: demographic and structure events
	std::multimap<const int,Event*> outputs_;										// OWNED POINTERS: output events (time, output)
	std::vector<Script*> scripts_;													// OWNED POINTERS: script events
	std::multimap<const int,const IntroducedMutation*> introduced_mutations_;		// OWNED POINTERS: user-defined mutations that will be introduced (time, mutation)
	std::vector<const PartialSweep*> partial_sweeps_;								// OWNED POINTERS: mutations undergoing partial sweeps
	std::vector<MutationType*> tracked_mutations_;									// tracked mutation-types; these pointers are not owned (they are owned by mutation_types_, above)
*/

#endif	// #ifndef SLIMCORE






























































