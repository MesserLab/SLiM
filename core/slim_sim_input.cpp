//
//  input_parsing.cpp
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


#include "slim_sim.h"
#include "g_rng.h"

#include <iostream>
#include <fstream>
#include <sstream>

#include "slim_global.h"
#include <malloc/malloc.h>		// for malloc_size(), for debugging; can be removed


using std::endl;
using std::string;
using std::istringstream;
using std::istream;
using std::ifstream;
using std::multimap;


// an enumeration of possible error types for InputError()
enum class InputErrorType {
	kNoPopulationDefined = 1,
	kUnknownParameter,
	kNonParameterInput,
	kInvalidMutationRate,
	kInvalidMutationType,
	kInvalidGenomicElementType,
	kInvalidChromosomeOrganization,
	kInvalidRecombinationRate,
	kInvalidGenerations,
	kInvalidDemographyAndStructure,
	kInvalidOutput,
	kInvalidInitialization,
	kInvalidSeed,
	kInvalidPredeterminedMutations,
	kInvalidGeneConversion,
	kInvalidSex,
	kSexNotDeclared,
	kSexDeclaredLate
};

// an enumeration of possible expectations regarding the presence of an end-of-file in EatSubstringWithPrefixAndCharactersAtEOF
enum class EOFExpectation
{
	kAgnostic = -1,
	kNoEOF = 0,
	kEOF = 1
};

// set by CheckInputFile() and used by SLiMgui
int gLineNumberOfParseError = 0;


// get one line of input, sanitizing by removing comments and whitespace
void GetInputLine(istream &p_input_file, string &p_line);

// generate cerr output describing an error that has occurred
std::string InputError(InputErrorType p_error_type, string p_line);

// eat a substring matching a set of possible characters, with an optional prefix and an optional EOF expectation; return false if expectations are not met
bool EatSubstringWithCharactersAtEOF(istringstream &p_string_stream, string &p_substring, const char *p_match_chars, EOFExpectation p_eof_expected);
bool EatSubstringWithPrefixAndCharactersAtEOF(istringstream &p_string_stream, string &p_substring, const char *p_prefix, const char *p_match_chars, EOFExpectation p_eof_expected);


void GetInputLine(istream &p_input_file, string &p_line)
{
	getline(p_input_file, p_line);
	
	// remove all after "//", the comment start sequence
	// BCH 16 Dec 2014: note this was "/" in SLiM 1.8 and earlier, changed to allow full filesystem paths to be specified.
	if (p_line.find("//") != string::npos)
		p_line.erase(p_line.find("//"));
	
	// remove leading and trailing whitespace (spaces and tabs)
	p_line.erase(0, p_line.find_first_not_of(" \t"));
	p_line.erase(p_line.find_last_not_of(" \t") + 1);
}

std::string InputError(InputErrorType p_error_type, string p_line)
{
	std::ostringstream input_error_stream;
	
	input_error_stream << endl;
	
#ifndef SLIMGUI
	input_error_stream << "ERROR (parameter file): ";
#endif
	
	switch (p_error_type)
	{
		case InputErrorType::kNoPopulationDefined:
			input_error_stream << "No population to simulate:" << endl;
			break;
			
		case InputErrorType::kUnknownParameter:
			input_error_stream << "Unknown parameter: " << p_line << endl;
			break;
			
		case InputErrorType::kNonParameterInput:
			input_error_stream << "Non-parameter input line: " << p_line << endl;
			break;
			
		case InputErrorType::kInvalidMutationRate:
			input_error_stream << "Invalid mutation rate: " << p_line << endl << endl;
			input_error_stream << "Required syntax:" << endl << endl;
			input_error_stream << "#MUTATION RATE" << endl;
			input_error_stream << "<u>" << endl << endl;
			input_error_stream << "Example:" << endl << endl;
			input_error_stream << "#MUTATION RATE" << endl;
			input_error_stream << "1.5e-8" << endl;
			break;
			
		case InputErrorType::kInvalidMutationType:
			input_error_stream << "Invalid mutation type: " << p_line << endl << endl;
			input_error_stream << "Required syntax:" << endl << endl;
			input_error_stream << "#MUTATION TYPES" << endl;
			input_error_stream << "<mutation-type-id> <h> <DFE-type> [DFE parameters]" << endl;
			input_error_stream << "..." << endl << endl;
			input_error_stream << "Example:" << endl << endl;
			input_error_stream << "#MUTATION TYPES" << endl;
			input_error_stream << "m1 0.2 g -0.05 0.2" << endl;
			input_error_stream << "m2 0.0 f 0.0" << endl;
			input_error_stream << "m3 0.5 e 0.01" << endl;
			break;
			
		case InputErrorType::kInvalidGenomicElementType:
			input_error_stream << "Invalid genomic element type: " << p_line << endl << endl;
			input_error_stream << "Required syntax:" << endl << endl;
			input_error_stream << "#GENOMIC ELEMENT TYPES" << endl;
			input_error_stream << "<element-type-id> <mut-type> <x> [<mut-type> <x>...]" << endl;
			input_error_stream << "..." << endl << endl;
			input_error_stream << "Example:" << endl << endl;
			input_error_stream << "#GENOMIC ELEMENT TYPES" << endl;
			input_error_stream << "g1 m3 0.8 m2 0.01 m1 0.19" << endl;
			break;
			
		case InputErrorType::kInvalidChromosomeOrganization:
			input_error_stream << "Invalid chromosome organization: " << p_line << endl << endl;
			input_error_stream << "Required syntax:" << endl << endl;
			input_error_stream << "#CHROMOSOME ORGANIZATION" << endl;
			input_error_stream << "<element-type> <start> <end>" << endl;
			input_error_stream << "..." << endl << endl;
			input_error_stream << "Example:" << endl << endl;
			input_error_stream << "#CHROMOSOME ORGANIZATION" << endl;
			input_error_stream << "g1 1000 1999" << endl;
			break;
			
		case InputErrorType::kInvalidRecombinationRate:
			input_error_stream << "Invalid recombination rate: " << p_line << endl << endl;
			input_error_stream << "Required syntax:" << endl << endl;
			input_error_stream << "#RECOMBINATION RATE" << endl;
			input_error_stream << "<interval-end> <r>" << endl;
			input_error_stream << "..." << endl << endl;
			input_error_stream << "Example:" << endl << endl;
			input_error_stream << "#RECOMBINATION RATE" << endl;
			input_error_stream << "10000 1e-8" << endl;
			input_error_stream << "20000 4.5e-8" << endl;
			break;
			
		case InputErrorType::kInvalidGenerations:
			input_error_stream << "Invalid generations: " << p_line << endl << endl;
			input_error_stream << "Required syntax:" << endl << endl;
			input_error_stream << "#GENERATIONS" << endl;
			input_error_stream << "<t> [<start>]" << endl << endl;
			input_error_stream << "Example:" << endl << endl;
			input_error_stream << "#GENERATIONS" << endl;
			input_error_stream << "10000" << endl;
			break;
			
		case InputErrorType::kInvalidDemographyAndStructure:
			input_error_stream << "Invalid demography and structure: " << p_line << endl << endl;
			input_error_stream << "Required syntax:" << endl << endl;
			input_error_stream << "#DEMOGRAPHY AND STRUCTURE" << endl;
			input_error_stream << "<time> <event-type> [event parameters]" << endl;
			input_error_stream << "..." << endl << endl;
			input_error_stream << "Example:" << endl << endl;
			input_error_stream << "DEMOGRAPHY AND STRUCTURE" << endl;
			input_error_stream << "1 P p1 1000" << endl;
			input_error_stream << "1 S p1 0.05" << endl;
			input_error_stream << "1000 P p2 100 p1 0.8" << endl;
			input_error_stream << "1000 S p2 0.05" << endl;
			input_error_stream << "1500 X p2 0.4     // only if #SEX has been declared" << endl;
			input_error_stream << "2000 N p1 1e4" << endl;
			input_error_stream << "2000 M p2 p1 0.01" << endl;
			break;
			
		case InputErrorType::kInvalidOutput:
			input_error_stream << "Invalid output: " << p_line << endl << endl;
			input_error_stream << "Required syntax:" << endl << endl;
			input_error_stream << "#OUTPUT" << endl;
			input_error_stream << "<time> <output-type> [output parameters]" << endl;
			input_error_stream << "..." << endl << endl;
			input_error_stream << "Example:" << endl << endl;
			input_error_stream << "OUTPUT" << endl;
			input_error_stream << "2000 A outfile" << endl;
			input_error_stream << "1000 R p1 10" << endl;
			input_error_stream << "1000 R p1 10 MS" << endl;
			input_error_stream << "2000 F" << endl;
			input_error_stream << "1 T m3" << endl;
			break;
			
		case InputErrorType::kInvalidInitialization:
			input_error_stream << "Invalid initialization: " << p_line << endl << endl;
			input_error_stream << "Required syntax:" << endl << endl;
			input_error_stream << "#INITIALIZATION" << endl;
			input_error_stream << "<filename>" << endl << endl;
			input_error_stream << "Example:" << endl << endl;
			input_error_stream << "#INITIALIZATION" << endl;
			input_error_stream << "outfile" << endl;
			break;
			
		case InputErrorType::kInvalidSeed:
			input_error_stream << "Invalid seed: " << p_line << endl << endl;
			input_error_stream << "Required syntax:" << endl << endl;
			input_error_stream << "#SEED" << endl;
			input_error_stream << "<seed>" << endl << endl;
			input_error_stream << "Example:" << endl << endl;
			input_error_stream << "#SEED" << endl;
			input_error_stream << "141235" << endl;
			break;
			
		case InputErrorType::kInvalidPredeterminedMutations:
			input_error_stream << "Invalid predetermined mutations: " << p_line << endl << endl;
			input_error_stream << "Required syntax:" << endl << endl;
			input_error_stream << "#PREDETERMINED MUTATIONS" << endl;
			input_error_stream << "<time> <mut-type> <x> <pop> <nAA> <nAa>" << endl << endl;
			input_error_stream << "Example:" << endl << endl;
			input_error_stream << "#PREDETERMINED MUTATIONS" << endl;
			input_error_stream << "5000 m7 45000 p1 0 1" << endl;
			break;
			
		case InputErrorType::kInvalidGeneConversion:
			input_error_stream << "Invalid gene conversion: " << p_line << endl << endl;
			input_error_stream << "Required syntax:" << endl << endl;
			input_error_stream << "#GENE CONVERSION" << endl;
			input_error_stream << "<fraction> <average-length>" << endl << endl;
			input_error_stream << "Example:" << endl << endl;
			input_error_stream << "#GENE CONVERSION" << endl;
			input_error_stream << "0.5 20" << endl;
			break;
			
			// SEX ONLY
		case InputErrorType::kInvalidSex:
			input_error_stream << "Invalid sex specification: " << p_line << endl << endl;
			input_error_stream << "Required syntax:" << endl << endl;
			input_error_stream << "#SEX" << endl;
			input_error_stream << "<chromosome-type:AXY> [<x-dominance>]" << endl << endl;
			input_error_stream << "Example:" << endl << endl;
			input_error_stream << "#SEX" << endl;
			input_error_stream << "X 0.75" << endl;
			break;
			
			// SEX ONLY
		case InputErrorType::kSexNotDeclared:
			input_error_stream << "A SEX ONLY feature was used before #SEX was declared: " << p_line << endl;
			break;
			
			// SEX ONLY
		case InputErrorType::kSexDeclaredLate:
			input_error_stream << "#SEX was declared too late; it must occur before subpopulations are added or read in." << endl;
			break;
	}
	
#ifndef SLIMGUI
	SLIM_TERMINATION << input_error_stream.str() << endl;
	SLIM_TERMINATION << slim_terminate();
#endif
	
	return input_error_stream.str();
}

bool EatSubstringWithCharactersAtEOF(istringstream &p_string_stream, string &p_substring, const char *p_match_chars, EOFExpectation p_eof_expected)
{
	return EatSubstringWithPrefixAndCharactersAtEOF(p_string_stream, p_substring, "", p_match_chars, p_eof_expected);
}

bool EatSubstringWithPrefixAndCharactersAtEOF(istringstream &p_string_stream, string &p_substring, const char *p_prefix, const char *p_match_chars, EOFExpectation p_eof_expected)
{
	bool good = true;
	size_t prefix_length = strlen(p_prefix);
	
	// there should be at least one character in sub; if not, we are expecting to eat something but instead hit the EOF
	if (p_substring.length() == 0)
		good = false;
	
	// first eat the prefix, if there is one
	if (prefix_length > 0)
	{
		if (p_substring.compare(0, prefix_length, p_prefix) != 0)
			good = false;
		p_substring.erase(0, prefix_length);
	}
	
	// all of the characters in substring should match match_chars
	if (p_substring.find_first_not_of(p_match_chars) != string::npos)
		good = false;
	
	// check for a match with our expectation that we are at the end of string_stream
	if (p_eof_expected == EOFExpectation::kNoEOF)
	{
		if (p_string_stream.eof())
			good = false;
	}
	else if (p_eof_expected == EOFExpectation::kEOF)
	{
		if (!p_string_stream.eof())
			good = false;
	}
	
	// if we do not expect to be at the end of string_stream, or we are agnostic, then prefetch the next substring
	if (p_eof_expected == EOFExpectation::kNoEOF)
	{
		p_string_stream >> p_substring;
	}
	else if (p_eof_expected == EOFExpectation::kAgnostic)
	{
		// FIXME once I understand streams better this can probably be fixed up...
		if (p_string_stream.eof())
			p_substring = "";
		else
			p_string_stream >> p_substring;
	}
	
	return good;
}

std::string SLiMSim::CheckInputFile(std::istream &infile)
{
	int num_mutation_types = 0;
	int num_mutation_rates = 0;
	int num_genomic_element_types = 0;
	int num_chromosome_organizations = 0;
	int num_recombination_rates = 0;
	int num_generations = 0;
	int num_subpopulations = 0;
	int num_sex_declarations = 0;	// SEX ONLY; used to check for sex vs. non-sex errors in the file, so the #SEX tag must come before any reliance on SEX ONLY features
	
	string line, sub;
	
	gLineNumberOfParseError = 0;	// SLiMgui uses this to better report parse errors; reset it to zero here
	
	while (!infile.eof())
	{
		if (line.find('#') != string::npos) 
		{
			bool good = true;
			
			// SEX ONLY
			#pragma mark Check:SEX
			if (line.find("SEX") != string::npos)
			{
				do
				{
					if (infile.eof())
						break;
					
					GetInputLine(infile, line);
					gLineNumberOfParseError++;
					
					if (line.find('#') != string::npos) break;
					if (line.length() == 0) continue;
					
					if (num_subpopulations > 0)
						return InputError(InputErrorType::kSexDeclaredLate, line);
					
					istringstream iss(line);
					iss >> sub;
					
					string chromosome_type = sub;
					good = good && EatSubstringWithCharactersAtEOF(iss, sub, "AXY", EOFExpectation::kAgnostic);								// SLiMSim.modeled_chromosome_type_
					
					if (chromosome_type.compare("X") == 0)																					// "X": one optional parameter
					{
						if (sub.length() > 0)
							good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890.-", EOFExpectation::kEOF);					// SLiMSim.x_chromosome_dominance_coeff_
					}
					
					if (!iss.eof())
						good = false;
					
					if (!good)
						return InputError(InputErrorType::kInvalidSex, line);
					else
						num_sex_declarations++;
				} while (true);
				
				continue;
			}
			
			#pragma mark Check:MUTATION RATE
			if (line.find("MUTATION RATE") != string::npos)
			{
				do
				{
					if (infile.eof())
						break;
					
					GetInputLine(infile, line);
					gLineNumberOfParseError++;
					
					if (line.find('#') != string::npos) break;
					if (line.length() == 0) continue;
					
					istringstream iss(line);
					iss >> sub;
					
					good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890.e-", EOFExpectation::kEOF);						// Chromosome.overall_mutation_rate_
					
					if (!good)
						return InputError(InputErrorType::kInvalidMutationRate, line);
					else
						num_mutation_rates++;
				} while (true);
				
				continue;
			}
			
			#pragma mark Check:MUTATION TYPES
			if (line.find("MUTATION TYPES") != string::npos)
			{
				do
				{
					if (infile.eof())
						break;
					
					GetInputLine(infile, line);
					gLineNumberOfParseError++;
					
					if (line.find('#') != string::npos) break;
					if (line.length() == 0) continue;
					
					istringstream iss(line);
					iss >> sub;
					
					good = good && EatSubstringWithPrefixAndCharactersAtEOF(iss, sub, "m", "1234567890", EOFExpectation::kNoEOF);			// id: Chromosome.mutation_types_ index
					good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890.-", EOFExpectation::kNoEOF);						// MutationType.dominance_coeff_
					
					string dfe_type = sub;
					good = good && EatSubstringWithCharactersAtEOF(iss, sub, "fge", EOFExpectation::kNoEOF);								// MutationType.dfe_type_
					
					if (dfe_type.compare("f") == 0 || dfe_type.compare("e") == 0)															// MutationType.dfe_parameters_: one parameter
					{ 
						good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890.-", EOFExpectation::kEOF);
					}
					else if (dfe_type.compare("g") == 0)																					// MutationType.dfe_parameters_: two parameters
					{
						good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890.-", EOFExpectation::kNoEOF);
						good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890.-", EOFExpectation::kEOF);
					}
					
					if (!good)
						return InputError(InputErrorType::kInvalidMutationType, line);
					else
						num_mutation_types++;
				} while (true);
				
				continue;
			}
			
			#pragma mark Check:GENOMIC ELEMENT TYPES
			if (line.find("GENOMIC ELEMENT TYPES") != string::npos)
			{
				do
				{
					if (infile.eof())
						break;
					
					GetInputLine(infile, line);
					gLineNumberOfParseError++;
					
					if (line.find('#') != string::npos) break;
					if (line.length() == 0) continue;
					
					istringstream iss(line);
					iss >> sub;
					
					good = good && EatSubstringWithPrefixAndCharactersAtEOF(iss, sub, "g", "1234567890", EOFExpectation::kNoEOF);			// id: Chromosome.genomic_element_types_ index
					
					while (good && (sub.length() > 0))
					{
						good = good && EatSubstringWithPrefixAndCharactersAtEOF(iss, sub, "m", "1234567890", EOFExpectation::kNoEOF);		// GenomicElementType.mutation_types_
						good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890e.", EOFExpectation::kAgnostic);				// GenomicElementType.mutation_fraction_
					}
					
					if (!good)
						return InputError(InputErrorType::kInvalidGenomicElementType, line);
					else
						num_genomic_element_types++;
				} while (true);
				
				continue;
			}
			
			#pragma mark Check:CHROMOSOME ORGANIZATION
			if (line.find("CHROMOSOME ORGANIZATION") != string::npos)
			{
				do
				{
					if (infile.eof())
						break;
					
					GetInputLine(infile, line);
					gLineNumberOfParseError++;
					
					if (line.find('#') != string::npos) break;
					if (line.length() == 0) continue;
					
					istringstream iss(line);
					iss >> sub;
					
					good = good && EatSubstringWithPrefixAndCharactersAtEOF(iss, sub, "g", "1234567890", EOFExpectation::kNoEOF);			// GenomicElement.genomic_element_type_
					good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890e", EOFExpectation::kNoEOF);						// GenomicElement.start_position_
					good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890e", EOFExpectation::kEOF);							// GenomicElement.end_position_
					
					if (!good)
						return InputError(InputErrorType::kInvalidChromosomeOrganization, line);
					else
						num_chromosome_organizations++;
				} while (true);
				
				continue;
			}
			
			#pragma mark Check:RECOMBINATION RATE
			if (line.find("RECOMBINATION RATE") != string::npos)
			{
				do
				{
					if (infile.eof())
						break;
					
					GetInputLine(infile, line);
					gLineNumberOfParseError++;
					
					if (line.find('#') != string::npos) break;
					if (line.length() == 0) continue;
					
					istringstream iss(line);
					iss >> sub;
					
					good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890e", EOFExpectation::kNoEOF);						// Chromosome.recombination_end_positions_
					good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890e.-", EOFExpectation::kEOF);						// Chromosome.recombination_rates_
					
					if (!good)
						return InputError(InputErrorType::kInvalidRecombinationRate, line);
					else
						num_recombination_rates++;
				} while (true);
				
				continue;
			}
			
			#pragma mark Check:GENE CONVERSION
			if (line.find("GENE CONVERSION") != string::npos)
			{
				do
				{
					if (infile.eof())
						break;
					
					GetInputLine(infile, line);
					gLineNumberOfParseError++;
					
					if (line.find('#') != string::npos) break;
					if (line.length() == 0) continue;
					
					istringstream iss(line);
					iss >> sub;
					
					good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890e.-", EOFExpectation::kNoEOF);						// Chromosome.gene_conversion_fraction_
					good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890e.-", EOFExpectation::kEOF);						// Chromosome.gene_conversion_avg_length_
					
					if (!good)
						return InputError(InputErrorType::kInvalidGeneConversion, line);
				} while (true);
				
				continue;
			}
			
			#pragma mark Check:GENERATIONS
			if (line.find("GENERATIONS") != string::npos)
			{
				do
				{
					if (infile.eof())
						break;
					
					GetInputLine(infile, line);
					gLineNumberOfParseError++;
					
					if (line.find('#') != string::npos) break;
					if (line.length() == 0) continue;
					
					istringstream iss(line);
					iss >> sub;
					
					good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890e", EOFExpectation::kAgnostic);						// main() time_duration
					
					if (sub.length() > 0)
						good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890e", EOFExpectation::kEOF);						// [main() time_start]
					
					if (!good)
						return InputError(InputErrorType::kInvalidGenerations, line);
					else
						num_generations++;
				} while (true);
				
				continue;
			}
			
			#pragma mark Check:DEMOGRAPHY AND STRUCTURE
			if (line.find("DEMOGRAPHY AND STRUCTURE") != string::npos)
			{
				do
				{
					if (infile.eof())
						break;
					
					GetInputLine(infile, line);
					gLineNumberOfParseError++;
					
					if (line.find('#') != string::npos) break;
					if (line.length() == 0) continue;
					
					istringstream iss(line);
					iss >> sub;
					
					good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890e", EOFExpectation::kNoEOF);						// time: main() events index
					
					string event_type = sub;
					good = good && EatSubstringWithCharactersAtEOF(iss, sub, "PSMNX", EOFExpectation::kNoEOF);								// Event.event_type_
					
					if (event_type.compare("P") == 0)																						// === TYPE P: two or three positive integers
					{
						good = good && EatSubstringWithPrefixAndCharactersAtEOF(iss, sub, "p", "1234567890", EOFExpectation::kNoEOF);		// Event.parameters_: uint p1
						good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890e", EOFExpectation::kAgnostic);					// Event.parameters_: uint N
						
						if (sub.length() > 0 && sub.at(0) == 'p')
							good = good && EatSubstringWithPrefixAndCharactersAtEOF(iss, sub, "p", "1234567890", EOFExpectation::kAgnostic);	// Event.parameters_: [uint p2]
						
						// SEX ONLY
						if (sub.length() > 0)
						{
							if (num_sex_declarations == 0)
								return InputError(InputErrorType::kSexNotDeclared, line);
							
							good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890.", EOFExpectation::kEOF);					// Event.parameters_: double initial_sex_ratio
						}
						
						if (!iss.eof())
							good = false;
						
						num_subpopulations++;
					}
					else if (event_type.compare("N") == 0)																					// === TYPE N: two positive integers
					{
						good = good && EatSubstringWithPrefixAndCharactersAtEOF(iss, sub, "p", "1234567890", EOFExpectation::kNoEOF);		// Event.parameters_: uint p1
						good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890e", EOFExpectation::kEOF);						// Event.parameters_: uint N
					}
					else if (event_type.compare("M") == 0)																					// === TYPE M: two positive integers and a double
					{
						good = good && EatSubstringWithPrefixAndCharactersAtEOF(iss, sub, "p", "1234567890", EOFExpectation::kNoEOF);		// Event.parameters_: uint p
						good = good && EatSubstringWithPrefixAndCharactersAtEOF(iss, sub, "p", "1234567890", EOFExpectation::kNoEOF);		// Event.parameters_: uint p
						good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890.-e", EOFExpectation::kEOF);					// Event.parameters_: double M
					}
					else if (event_type.compare("S") == 0)																					// === TYPE S: one positive integer and a double
					{
						good = good && EatSubstringWithPrefixAndCharactersAtEOF(iss, sub, "p", "1234567890", EOFExpectation::kNoEOF);		// Event.parameters_: uint p
						good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890.-e", EOFExpectation::kEOF);					// Event.parameters_: double sigma
					}
					else if (event_type.compare("X") == 0)																					// === TYPE X: one positive integer and a double
					{
						// SEX ONLY
						if (num_sex_declarations == 0)
							return InputError(InputErrorType::kSexNotDeclared, line);
						
						good = good && EatSubstringWithPrefixAndCharactersAtEOF(iss, sub, "p", "1234567890", EOFExpectation::kNoEOF);		// Event.parameters_: uint p
						good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890.", EOFExpectation::kEOF);						// Event.parameters_: double sex_ratio
					}
					
					if (!good)
						return InputError(InputErrorType::kInvalidDemographyAndStructure, line);
				} while (true);
				
				continue;
			}
			
			#pragma mark Check:OUTPUT
			if (line.find("OUTPUT") != string::npos)
			{
				do
				{
					if (infile.eof())
						break;
					
					GetInputLine(infile, line);
					gLineNumberOfParseError++;
					
					if (line.find('#') != string::npos) break;
					if (line.length() == 0) continue;
					
					istringstream iss(line);
					iss >> sub;
					
					good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890e", EOFExpectation::kNoEOF);						// time: main() outputs index

					string output_type = sub;
					good = good && EatSubstringWithCharactersAtEOF(iss, sub, "ARFT", EOFExpectation::kAgnostic);							// Event.event_type_
					
					if (output_type.compare("A") == 0)																						// === TYPE A: no parameter, or a filename
					{
						// sub may or may not now contain a filename																		// Event.parameters_: [filename]
						// we don't make an EatSubstring call here because we don't do a lexical check on filenames
					}
					else if (output_type.compare("R") == 0)																					// === TYPE R: two positive integers
					{
						good = good && EatSubstringWithPrefixAndCharactersAtEOF(iss, sub, "p", "1234567890", EOFExpectation::kNoEOF);		// Event.parameters_: uint p
						good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890", EOFExpectation::kAgnostic);					// Event.parameters_: uint size
						
						// SEX ONLY
						if (sub.length() > 0 && (sub == "M" || sub == "F" || sub == "*"))													// Event.parameters_: ['M'|'F'|'*']
						{
							if (num_sex_declarations == 0)
								return InputError(InputErrorType::kSexNotDeclared, line);
							
							good = good && EatSubstringWithCharactersAtEOF(iss, sub, "MF*", EOFExpectation::kAgnostic);
						}
						
						if (sub.length() > 0 && sub != "MS")																				// Event.parameters_: ['MS']
							good = false;
					}
					else if (output_type.compare("F") == 0)																					// === TYPE F: no parameter
					{
					}
					else if (output_type.compare("T") == 0)																					// === TYPE T: ??? FIXME code for this case missing!
					{
					}
					
					if (!iss.eof())
						good = false;
					
					if (!good)
						return InputError(InputErrorType::kInvalidOutput, line);
				} while (true);
				
				continue;
			}
			
			#pragma mark Check:PREDETERMINED MUTATIONS
			if (line.find("PREDETERMINED MUTATIONS") != string::npos)
			{
				do
				{
					if (infile.eof())
						break;
					
					GetInputLine(infile, line);
					gLineNumberOfParseError++;
					
					if (line.find('#') != string::npos) break;
					if (line.length() == 0) continue;
					
					istringstream iss(line);
					iss >> sub;
					
					good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890e", EOFExpectation::kNoEOF);						// Mutation.generation_
					good = good && EatSubstringWithPrefixAndCharactersAtEOF(iss, sub, "m", "1234567890", EOFExpectation::kNoEOF);			// Mutation.mutation_type_
					good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890e", EOFExpectation::kNoEOF);						// Mutation.position_
					good = good && EatSubstringWithPrefixAndCharactersAtEOF(iss, sub, "p", "1234567890", EOFExpectation::kNoEOF);			// Mutation.subpop_index_
					good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890", EOFExpectation::kNoEOF);							// IntroducedMutation.num_AA_
					good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890", EOFExpectation::kAgnostic);						// IntroducedMutation.num_Aa_
					
					if (sub.length() > 0)
					{
						good = good && EatSubstringWithPrefixAndCharactersAtEOF(iss, sub, "P", "", EOFExpectation::kNoEOF);					// ['P']
						good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890.-e", EOFExpectation::kEOF);					// PartialSweep.target_prevalence_
					}
					
					if (!good)
						return InputError(InputErrorType::kInvalidPredeterminedMutations, line);
				} while (true);
				
				continue;
			}
			
			#pragma mark Check:SEED
			if (line.find("SEED") != string::npos)
			{
				do
				{
					if (infile.eof())
						break;
					
					GetInputLine(infile, line);
					gLineNumberOfParseError++;
					
					if (line.find('#') != string::npos) break;
					if (line.length() == 0) continue;
					
					istringstream iss(line);
					iss >> sub;
					
					good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890-", EOFExpectation::kEOF);							// Initialize() seed
					
					if (!good)
						return InputError(InputErrorType::kInvalidSeed, line);
				} while (true);
				
				continue;
			}
			
			#pragma mark Check:INITIALIZATION
			if (line.find("INITIALIZATION") != string::npos)
			{
				do
				{
					if (infile.eof())
						break;
					
					GetInputLine(infile, line);
					gLineNumberOfParseError++;
					
					if (line.find('#') != string::npos) break;
					if (line.length() == 0) continue;
					
					istringstream iss(line);
					iss >> sub;
					
					// sub should now contain a filename, but no checking of filenames is presently done here								// filename
					
					if (!iss.eof())
						good = false;
					
					if (!good)
						return InputError(InputErrorType::kInvalidInitialization, line);
					
					num_subpopulations++;
				} while (true);
				
				continue;
			}
			
			return InputError(InputErrorType::kUnknownParameter, line);
		}
		else
		{
			if (line.length() == 0)
			{
				GetInputLine(infile, line);
				gLineNumberOfParseError++;
			}
			else
				return InputError(InputErrorType::kNonParameterInput, line);	// BCH 2 Jan 2015: added this as an error
		}
	}
	
	// Check that all elements occurred an allowed number of times.  FIXME check these; shouldn't some of the "< 1" tests be "!= 1" tests?
	if (num_mutation_rates != 1)
		return InputError(InputErrorType::kInvalidMutationRate, string());
	
	if (num_mutation_types < 1)
		return InputError(InputErrorType::kInvalidMutationType, string());
	
	if (num_genomic_element_types < 1)
		return InputError(InputErrorType::kInvalidGenomicElementType, string());
	
	if (num_chromosome_organizations < 1)
		return InputError(InputErrorType::kInvalidChromosomeOrganization, string());
	
	if (num_recombination_rates < 1)
		return InputError(InputErrorType::kInvalidRecombinationRate, string());
	
	if (num_generations < 1)
		return InputError(InputErrorType::kInvalidGenerations, string());
	
	if (num_subpopulations < 1)
		return InputError(InputErrorType::kNoPopulationDefined, string());
	
	// SEX ONLY
	if (num_sex_declarations > 1)
		return InputError(InputErrorType::kInvalidSex, string());
	
	if (DEBUG_INPUT)
	{
		SLIM_OUTSTREAM << "CheckInputFile: file checked out:" << endl;
		SLIM_OUTSTREAM << "   num_mutation_rates == " << num_mutation_rates << endl;
		SLIM_OUTSTREAM << "   num_mutation_types == " << num_mutation_types << endl;
		SLIM_OUTSTREAM << "   num_genomic_element_types == " << num_genomic_element_types << endl;
		SLIM_OUTSTREAM << "   num_chromosome_organizations == " << num_chromosome_organizations << endl;
		SLIM_OUTSTREAM << "   num_recombination_rates == " << num_recombination_rates << endl;
		SLIM_OUTSTREAM << "   num_generations == " << num_generations << endl;
		SLIM_OUTSTREAM << "   num_subpopulations == " << num_subpopulations << endl;
		SLIM_OUTSTREAM << "   num_sex_declarations == " << num_sex_declarations << endl;
	}
	
	return std::string();
}

void SLiMSim::InitializePopulationFromFile(const char *p_file)
{
	std::map<int,const Mutation*> mutations;
	string line, sub; 
	ifstream infile(p_file);
	
	if (!infile.is_open())
		SLIM_TERMINATION << "ERROR (Initialize): could not open initialization file" << endl << slim_terminate();
	
	// Read and ignore initial stuff until we hit the Populations section
	while (!infile.eof())
	{
		GetInputLine(infile, line);
		
		if (line.find("Populations") != string::npos)
			break;
	}
	
	// Now we are in the Populations section; read and instantiate each population until we hit the Mutations section
	while (!infile.eof())
	{ 
		GetInputLine(infile, line);
		
		if (line.length() == 0)
			continue;
		if (line.find("Mutations") != string::npos)
			break;
		
		istringstream iss(line);
		
		iss >> sub;
		sub.erase(0, 1);  // p
		int subpop_index = atoi(sub.c_str());
		
		iss >> sub;
		int subpop_size = atoi(sub.c_str());
		
		// SLiM 2.0 output format has <H | S <ratio>> here; if that is missing or "H" is given, the population is hermaphroditic and the ratio given is irrelevant
		double sex_ratio = 0.0;
		
		if (iss >> sub)
		{
			if (sub == "S")
			{
				iss >> sub;
				
				sex_ratio = atof(sub.c_str());
			}
		}
		
		// Create the population population
		population_.AddSubpopulation(subpop_index, subpop_size, sex_ratio, *this);
	}
	
	// Now we are in the Mutations section; read and instantiate all mutations and add them to our map and to the registry
	while (!infile.eof()) 
	{
		GetInputLine(infile, line);
		
		if (line.length() == 0)
			continue;
		if (line.find("Genomes") != string::npos)
			break;
		if (line.find("Individuals") != string::npos)	// SLiM 2.0 added this section
			break;
		
		istringstream iss(line);
		
		iss >> sub; 
		int mutation_id = atoi(sub.c_str());
		
		iss >> sub;
		sub.erase(0, 1);	// m
		int mutation_type_id = atoi(sub.c_str());
		
		iss >> sub;
		int position = atoi(sub.c_str()) - 1;
		
		iss >> sub;
		double selection_coeff = atof(sub.c_str());
		
		iss >> sub;		// dominance coefficient, which is given in the mutation type and presumably matches here
		
		iss >> sub;
		sub.erase(0, 1);	// p
		int subpop_index = atoi(sub.c_str());
		
		iss >> sub;
		int generation = atoi(sub.c_str());
		
		// look up the mutation type from its index
		auto found_muttype_pair = mutation_types_.find(mutation_type_id);
		
		if (found_muttype_pair == mutation_types_.end()) 
			SLIM_TERMINATION << "ERROR (InitializePopulationFromFile): mutation type m"<< mutation_type_id << " has not been defined" << endl << slim_terminate();
		
		const MutationType *mutation_type_ptr = found_muttype_pair->second;
		
		// construct the new mutation
		const Mutation *new_mutation = new Mutation(mutation_type_ptr, position, selection_coeff, subpop_index, generation);
		
		// add it to our local map, so we can find it when making genomes, and to the population's mutation registry
		mutations.insert(std::pair<const int,const Mutation*>(mutation_id, new_mutation));
		population_.mutation_registry_.push_back(new_mutation);
	}
	
	// If there is an Individuals section (added in SLiM 2.0), we skip it; we don't need any of the information that it gives, it is mainly for human readability
	if (line.find("Individuals") != string::npos)
	{
		while (!infile.eof()) 
		{
			GetInputLine(infile, line);
			
			if (line.length() == 0)
				continue;
			if (line.find("Genomes") != string::npos)
				break;
		}
	}
	
	// Now we are in the Genomes section, which should take us to the end of the file
	while (!infile.eof())
	{
		GetInputLine(infile, line);
		
		if (line.length() == 0)
			continue;
		
		istringstream iss(line);
		
		iss >> sub;
		sub.erase(0, 1);	// p
		int pos = static_cast<int>(sub.find_first_of(":"));
		const char *subpop_id_string = sub.substr(0, pos).c_str();
		int subpop_id = atoi(subpop_id_string);
		
		Subpopulation &subpop = population_.SubpopulationWithID(subpop_id);
		
		sub.erase(0, pos + 1);	// remove the subpop_id and the colon
		int genome_index = atoi(sub.c_str()) - 1;
		Genome &genome = subpop.parent_genomes_[genome_index];
		
		// Now we might have [A|X|Y] (SLiM 2.0), or we might have the first mutation id - or we might have nothing at all
		if (iss >> sub)
		{
			char genome_type = sub[0];
			
			// check whether this token is a genome type
			if (genome_type == 'A' || genome_type == 'X' || genome_type == 'Y')
			{
				// Let's do a little error-checking against what has already been instantiated for us...
				if (genome_type == 'A' && genome.GenomeType() != GenomeType::kAutosome)
					SLIM_TERMINATION << "ERROR (InitializePopulationFromFile): genome is specified as A (autosome), but the instantiated genome does not match" << endl << slim_terminate();
				if (genome_type == 'X' && genome.GenomeType() != GenomeType::kXChromosome)
					SLIM_TERMINATION << "ERROR (InitializePopulationFromFile): genome is specified as X (X-chromosome), but the instantiated genome does not match" << endl << slim_terminate();
				if (genome_type == 'Y' && genome.GenomeType() != GenomeType::kYChromosome)
					SLIM_TERMINATION << "ERROR (InitializePopulationFromFile): genome is specified as Y (Y-chromosome), but the instantiated genome does not match" << endl << slim_terminate();
				
				if (iss >> sub)
				{
					if (sub == "<null>")
					{
						if (!genome.IsNull())
							SLIM_TERMINATION << "ERROR (InitializePopulationFromFile): genome is specified as null, but the instantiated genome is non-null" << endl << slim_terminate();
						
						continue;	// this line is over
					}
					else
					{
						if (genome.IsNull())
							SLIM_TERMINATION << "ERROR (InitializePopulationFromFile): genome is specified as non-null, but the instantiated genome is null" << endl << slim_terminate();
						
						// drop through, and sub will be interpreted as a mutation id below
					}
				}
				else
					continue;
			}
			
			do
			{
				int id = atoi(sub.c_str());
				
				auto found_mut_pair = mutations.find(id);
				
				if (found_mut_pair == mutations.end()) 
					SLIM_TERMINATION << "ERROR (InitializePopulationFromFile): mutation " << id << " has not been defined" << endl << slim_terminate();
				
				const Mutation *mutation = found_mut_pair->second;
				
				genome.push_back(mutation);
			}
			while (iss >> sub);
		}
	}
	
	// Now that we have the info on everybody, update fitnesses so that we're ready to run the next generation
	for (std::pair<const int,Subpopulation*> &subpop_pair : population_)
		subpop_pair.second->UpdateFitness();
}

void SLiMSim::InitializeFromFile(std::istream &infile)
{
	string line, sub;
	
#ifdef SLIMGUI
	int mutation_type_index = 0;
#endif
	
	if (!rng_seed_supplied_to_constructor_)
		rng_seed_ = GenerateSeedFromPIDAndTime();
	
	if (DEBUG_INPUT)
		SLIM_OUTSTREAM << "InitializeFromFile():" << endl;
	
	while (!infile.eof())
	{
		if (line.find('#') != string::npos) 
		{
			// SEX ONLY
			#pragma mark Initialize:SEX
			if (line.find("SEX") != string::npos)
			{
				input_parameters_.push_back("#SEX");
				
				do
				{
					if (infile.eof())
						break;
					GetInputLine(infile, line);
					
					if (line.find('#') != string::npos) break;
					if (line.length() == 0) continue;
					input_parameters_.push_back(line);
					
					// FORMAT: chromosome_type [x-dominance]
					istringstream iss(line);
					
					iss >> sub;
					char chromosome_type = sub.at(0);
					
					if (chromosome_type == 'A')
					{
						modeled_chromosome_type_ = GenomeType::kAutosome;
					}
					else if (chromosome_type == 'X')
					{
						modeled_chromosome_type_ = GenomeType::kXChromosome;
						
						// optional x-dominance coefficient
						if (iss >> sub)
							x_chromosome_dominance_coeff_ = atof(sub.c_str());
					}
					else if (chromosome_type == 'Y')
					{
						modeled_chromosome_type_ = GenomeType::kYChromosome;
					}
					
					sex_enabled_ = true;	// whether we're modeling an autosome or a sex chromosome, the presence of this tag indicates that tracking of sex is turned on
					
					if (DEBUG_INPUT)
						SLIM_OUTSTREAM << "   #SEX: " << chromosome_type << " " << x_chromosome_dominance_coeff_ << endl;
				} while (true);
				
				continue;
			}
			
			#pragma mark Initialize:MUTATION RATE
			if (line.find("MUTATION RATE") != string::npos)
			{
				input_parameters_.push_back("#MUTATION RATE");
				
				do
				{
					if (infile.eof())
						break;
					GetInputLine(infile, line);
					
					if (line.find('#') != string::npos) break;
					if (line.length() == 0) continue;
					input_parameters_.push_back(line);
					
					// FORMAT: overall_mutation_rate
					istringstream iss(line);
					
					iss >> sub;
					double overall_mutation_rate = atof(sub.c_str());
					
					chromosome_.overall_mutation_rate_ = overall_mutation_rate;
					
					if (DEBUG_INPUT)
						SLIM_OUTSTREAM << "   #MUTATION RATE: overall_mutation_rate " << overall_mutation_rate << endl;
				} while (true);
				
				continue;
			}
			
			#pragma mark Initialize:MUTATION TYPES
			if (line.find("MUTATION TYPES") != string::npos)
			{
				input_parameters_.push_back("#MUTATION TYPES");
				
				do
				{
					if (infile.eof())
						break;
					GetInputLine(infile, line);
					
					if (line.find('#') != string::npos) break;
					if (line.length() == 0) continue;
					input_parameters_.push_back(line);
					
					// FORMAT: map_identifier dominance_coeff dfe_type dfe_parameters...
					std::vector<double> dfe_parameters;
					istringstream iss(line);
					
					iss >> sub;
					sub.erase(0, 1);	// m
					int map_identifier = atoi(sub.c_str());
					
					if (mutation_types_.count(map_identifier) > 0) 
						SLIM_TERMINATION << "ERROR (Initialize): mutation type " << map_identifier << " already defined" << endl << slim_terminate();
					
					iss >> sub;
					double dominance_coeff = atof(sub.c_str());
					
					iss >> sub;
					char dfe_type = sub.at(0);
					
					while (iss >> sub)
						dfe_parameters.push_back(atof(sub.c_str()));
					
#ifdef SLIMGUI
					MutationType *new_mutation_type = new MutationType(map_identifier, dominance_coeff, dfe_type, dfe_parameters, mutation_type_index);
					
					mutation_type_index++;	// each new mutation type gets a unique zero-based index, used by SLiMgui to categorize mutations
#else
					MutationType *new_mutation_type = new MutationType(map_identifier, dominance_coeff, dfe_type, dfe_parameters);
#endif
					
					mutation_types_.insert(std::pair<const int,MutationType*>(map_identifier, new_mutation_type));
					
					if (DEBUG_INPUT)
						SLIM_OUTSTREAM << "   #MUTATION TYPES: " << "m" << map_identifier << " " << *new_mutation_type << endl;
				} while (true);
				
				continue;
			}
			
			#pragma mark Initialize:GENOMIC ELEMENT TYPES
			if (line.find("GENOMIC ELEMENT TYPES") != string::npos)
			{
				input_parameters_.push_back("#GENOMIC ELEMENT TYPES");
				
				do
				{
					if (infile.eof())
						break;
					GetInputLine(infile, line);
					
					if (line.find('#') != string::npos) break;
					if (line.length() == 0) continue;
					input_parameters_.push_back(line);
					
					// FORMAT: map_identifier mutation_type mutation_fraction [more type/fraction pairs...]
					std::vector<MutationType*> mutation_types;
					std::vector<double> mutation_fractions;
					istringstream iss(line);
					
					iss >> sub;
					sub.erase(0, 1);		// g
					int map_identifier = atoi(sub.c_str());
					
					while (iss >> sub) 
					{ 
						sub.erase(0, 1);	// m
						int mutation_type_id = atoi(sub.c_str());
						auto found_muttype_pair = mutation_types_.find(mutation_type_id);
						
						if (found_muttype_pair == mutation_types_.end())
							SLIM_TERMINATION << "ERROR (Initialize): mutation type m" << mutation_type_id << " not defined" << endl << slim_terminate();
						
						MutationType *mutation_type_ptr = found_muttype_pair->second;
						mutation_types.push_back(mutation_type_ptr);
						iss >> sub;
						
						mutation_fractions.push_back(atof(sub.c_str()));
					}
					
					if (genomic_element_types_.count(map_identifier) > 0) 
						SLIM_TERMINATION << "ERROR (Initialize): genomic element type " << map_identifier << " already defined" << endl << slim_terminate();
					
					GenomicElementType *new_genomic_element_type = new GenomicElementType(map_identifier, mutation_types, mutation_fractions);
					genomic_element_types_.insert(std::pair<const int,GenomicElementType*>(map_identifier, new_genomic_element_type));
					
					if (DEBUG_INPUT)
						SLIM_OUTSTREAM << "   #GENOMIC ELEMENT TYPES: " << "g" << map_identifier << " " << new_genomic_element_type << endl;
				} while (true);
				
				continue;
			}
			
			#pragma mark Initialize:CHROMOSOME ORGANIZATION
			if (line.find("CHROMOSOME ORGANIZATION") != string::npos)
			{
				input_parameters_.push_back("#CHROMOSOME ORGANIZATION");
				
				do
				{
					if (infile.eof())
						break;
					GetInputLine(infile, line);
					
					if (line.find('#') != string::npos) break;
					if (line.length() == 0) continue;
					input_parameters_.push_back(line);
					
					// FORMAT: genomic_element_type start_position end_position
					istringstream iss(line);
					
					iss >> sub;
					sub.erase(0, 1);	// g
					int genomic_element_type = atoi(sub.c_str());
					
					iss >> sub;
					int start_position = static_cast<int>(atof(sub.c_str())) - 1;
					
					iss >> sub;
					int end_position = static_cast<int>(atof(sub.c_str())) - 1;
					
					auto found_getype_pair = genomic_element_types_.find(genomic_element_type);
					
					if (found_getype_pair == genomic_element_types_.end())
						SLIM_TERMINATION << "ERROR (Initialize): genomic element type m" << genomic_element_type << " not defined" << endl << slim_terminate();
					
					const GenomicElementType *genomic_element_type_ptr = found_getype_pair->second;
					GenomicElement new_genomic_element(genomic_element_type_ptr, start_position, end_position);
					
					bool old_log = GenomicElement::LogGenomicElementCopyAndAssign(false);
					chromosome_.push_back(new_genomic_element);
					GenomicElement::LogGenomicElementCopyAndAssign(old_log);
					
					if (DEBUG_INPUT)
						SLIM_OUTSTREAM << "   #CHROMOSOME ORGANIZATION: " << new_genomic_element << endl;
				} while (true);
				
				continue;
			}
			
			#pragma mark Initialize:RECOMBINATION RATE
			if (line.find("RECOMBINATION RATE") != string::npos)
			{
				input_parameters_.push_back("#RECOMBINATION RATE");
				
				do
				{
					if (infile.eof())
						break;
					GetInputLine(infile, line);
					
					if (line.find('#') != string::npos) break;
					if (line.length() == 0) continue;
					input_parameters_.push_back(line);
					
					// FORMAT: recombination_end_position recombination_rate
					istringstream iss(line);
					
					iss >> sub;
					int recombination_end_position = static_cast<int>(atof(sub.c_str())) - 1;
					
					iss >> sub;
					double recombination_rate = atof(sub.c_str());
					
					chromosome_.recombination_end_positions_.push_back(recombination_end_position);
					chromosome_.recombination_rates_.push_back(recombination_rate);
					
					if (DEBUG_INPUT)
						SLIM_OUTSTREAM << "   #RECOMBINATION RATE: recombination_end_position " << recombination_end_position << ", recombination_rate " << recombination_rate << endl;
				} while (true);
				
				continue;
			}
			
			#pragma mark Initialize:GENE CONVERSION
			if (line.find("GENE CONVERSION") != string::npos)
			{
				input_parameters_.push_back("#GENE CONVERSION");
				
				do
				{
					if (infile.eof())
						break;
					GetInputLine(infile, line);
					
					if (line.find('#') != string::npos) break;
					if (line.length() == 0) continue;
					input_parameters_.push_back(line);
					
					// FORMAT: gene_conversion_fraction gene_conversion_avg_length
					istringstream iss(line);
					
					iss >> sub;
					double gene_conversion_fraction = atof(sub.c_str());
					
					iss >> sub;
					double gene_conversion_avg_length = atof(sub.c_str());
					
					chromosome_.gene_conversion_fraction_ = gene_conversion_fraction;
					chromosome_.gene_conversion_avg_length_ = gene_conversion_avg_length;
					
					if (DEBUG_INPUT)
						SLIM_OUTSTREAM << "   #GENE CONVERSION: gene_conversion_fraction " << gene_conversion_fraction << ", gene_conversion_avg_length_ " << gene_conversion_avg_length << endl;
				} while (true);
				
				continue;
			}
			
			#pragma mark Initialize:GENERATIONS
			if (line.find("GENERATIONS") != string::npos)
			{
				input_parameters_.push_back("#GENERATIONS");
				
				do
				{
					if (infile.eof())
						break;
					GetInputLine(infile, line);
					
					if (line.find('#') != string::npos) break;
					if (line.length() == 0) continue;
					input_parameters_.push_back(line);
					
					// FORMAT: time_duration [time_start]
					istringstream iss(line);
					
					iss >> sub;
					time_duration_ = static_cast<int>(atof(sub.c_str()));
					
					if (iss >> sub)
						time_start_ = static_cast<int>(atof(sub.c_str()));
					else
						time_start_ = 1;
					
					if (DEBUG_INPUT)
						SLIM_OUTSTREAM << "   #GENERATIONS: time_duration " << time_duration_ << ", time_start " << time_start_ << endl;
				} while (true);
				
				continue;
			}
			
			#pragma mark Initialize:DEMOGRAPHY AND STRUCTURE
			if (line.find("DEMOGRAPHY AND STRUCTURE") != string::npos)
			{
				input_parameters_.push_back("#DEMOGRAPHY AND STRUCTURE");
				
				do
				{
					if (infile.eof())
						break;
					GetInputLine(infile, line);
					
					if (line.find('#') != string::npos) break;
					if (line.length() == 0) continue;
					input_parameters_.push_back(line);
					
					// FORMAT: event_time event_type [event_parameters...]
					istringstream iss(line);
					
					iss >> sub;
					int event_time = static_cast<int>(atof(sub.c_str()));
					
					iss >> sub;
					char event_type = sub.at(0);
					
					std::vector<string> event_parameters;
					while (iss >> sub)
						event_parameters.push_back(sub.c_str());
					
					Event *new_event_ptr = new Event(event_type, event_parameters);
					
					events_.insert(std::pair<const int,Event*>(event_time, new_event_ptr));
					
					if (DEBUG_INPUT)
						SLIM_OUTSTREAM << "   #DEMOGRAPHY AND STRUCTURE: event_time " << event_time << " " << *new_event_ptr << endl;
				} while (true);
				
				continue;
			}
			
			#pragma mark Initialize:OUTPUT
			if (line.find("OUTPUT") != string::npos)
			{
				input_parameters_.push_back("#OUTPUT");
				
				do
				{
					if (infile.eof())
						break;
					GetInputLine(infile, line);
					
					if (line.find('#') != string::npos) break;
					if (line.length() == 0) continue;
					input_parameters_.push_back(line);
					
					// FORMAT: event_time event_type [event_parameters...]
					istringstream iss(line);
					
					iss >> sub;
					int event_time = static_cast<int>(atof(sub.c_str()));
					
					iss >> sub;
					char event_type = sub.at(0);
					
					std::vector<string> event_parameters;
					while (iss >> sub)
						event_parameters.push_back(sub.c_str());
					
					Event *new_event_ptr = new Event(event_type, event_parameters);
					
					outputs_.insert(std::pair<const int,Event*>(event_time, new_event_ptr));
					
					if (DEBUG_INPUT)
						SLIM_OUTSTREAM << "   #OUTPUT: event_time " << event_time << " " << *new_event_ptr << endl;
				} while (true);
				
				continue;
			}
			
			#pragma mark Initialize:PREDETERMINED MUTATIONS
			if (line.find("PREDETERMINED MUTATIONS") != string::npos)
			{
				input_parameters_.push_back("#PREDETERMINED MUTATIONS");
				
				do
				{
					if (infile.eof())
						break;
					GetInputLine(infile, line);
					
					if (line.find('#') != string::npos) break;
					if (line.length() == 0) continue;
					input_parameters_.push_back(line);
					
					// FORMAT: generation mutation_type position subpop_index num_AA num_Aa ['P' target_prevalence]
					istringstream iss(line); 
					
					iss >> sub;
					int generation = static_cast<int>(atof(sub.c_str()));
					
					iss >> sub;
					sub.erase(0, 1);	// m
					int mutation_type_id = atoi(sub.c_str());
					
					iss >> sub;
					int position = static_cast<int>(atof(sub.c_str())) - 1;
					
					iss >> sub;
					sub.erase(0, 1);	// p
					int subpop_index = atoi(sub.c_str());
					
					iss >> sub;
					int num_AA = static_cast<int>(atof(sub.c_str()));
					
					iss >> sub;
					int num_Aa = static_cast<int>(atof(sub.c_str()));
					
					auto found_muttype_pair = mutation_types_.find(mutation_type_id);
					
					if (found_muttype_pair == mutation_types_.end())
						SLIM_TERMINATION << "ERROR (Initialize): mutation type m" << mutation_type_id << " not defined" << endl << slim_terminate();
					
					const MutationType *mutation_type_ptr = found_muttype_pair->second;
					
					const IntroducedMutation *new_introduced_mutation = new IntroducedMutation(mutation_type_ptr, position, subpop_index, generation, num_AA, num_Aa);
					
					introduced_mutations_.insert(std::pair<const int,const IntroducedMutation*>(generation, new_introduced_mutation));
					
					if (DEBUG_INPUT)
						SLIM_OUTSTREAM << "   #PREDETERMINED MUTATIONS: generation " << generation << " " << *new_introduced_mutation << endl;
					
					while (iss >> sub) 
					{ 
						if (sub.find('P') != string::npos) 
						{
							iss >> sub;
							double target_prevalence = atof(sub.c_str());
							const PartialSweep *new_partial_sweep = new PartialSweep(mutation_type_ptr, position, target_prevalence);
							
							partial_sweeps_.push_back(new_partial_sweep);
							
							if (DEBUG_INPUT)
								SLIM_OUTSTREAM << "      " << *new_partial_sweep << endl;
						}
					}
				} while (true);
				
				continue;
			}
			
			#pragma mark Initialize:SEED
			if (line.find("SEED") != string::npos)
			{
				// #SEED is pushed back below
				
				do
				{
					if (infile.eof())
						break;
					GetInputLine(infile, line);
					
					if (line.find('#') != string::npos) break;
					if (line.length() == 0) continue;
					// seed is pushed back below
					
					// FORMAT: seed
					istringstream iss(line);
					
					iss >> sub;
					int directive_seed = atoi(sub.c_str());
					
					if (!rng_seed_supplied_to_constructor_)
						rng_seed_ = directive_seed;
					
					if (DEBUG_INPUT)
						SLIM_OUTSTREAM << "   #SEED: seed " << directive_seed << endl;
				} while (true);
				
				continue;
			}
			
			#pragma mark Initialize:INITIALIZATION
			if (line.find("INITIALIZATION") != string::npos)
			{
				input_parameters_.push_back("#INITIALIZATION");
				
				do
				{
					if (infile.eof())
						break;
					GetInputLine(infile, line);
					
					if (line.find('#') != string::npos) break;
					if (line.length() == 0) continue;
					input_parameters_.push_back(line);
					
					// FORMAT: filename
					istringstream iss(line);
					
					iss >> sub;
					
					InitializePopulationFromFile(sub.c_str());
				} while (true);
				
				continue;
			}
		}
		else
		{
			GetInputLine(infile, line);
		}
	}
	
	// initialize rng
	InitializeRNGFromSeed(rng_seed_);
	
	input_parameters_.push_back("#SEED");
	
	std::stringstream ss;
	ss << rng_seed_;
	input_parameters_.push_back(ss.str());
	
	// parameter output
	for (int i = 0; i < input_parameters_.size(); i++)
		SLIM_OUTSTREAM << input_parameters_[i] << endl;
	
	// initialize chromosome
	chromosome_.InitializeDraws();
}



































































