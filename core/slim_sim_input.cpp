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

#include <fstream>
#include <sstream>


using std::cerr;
using std::endl;
using std::string;
using std::istringstream;
using std::ifstream;
using std::multimap;


#define DEBUG_INPUT	0


// an enumeration of possible error types for InputError()
enum InputErrorType {
	kNoPopulationDefinedError = 1,
	kUnknownParameterError,
	kInvalidParameterFileError,
	kInvalidMutationRateError,
	kInvalidMutationTypeError,
	kInvalidGenomicElementTypeError,
	kInvalidChromosomeOrganizationError,
	kInvalidRecombinationRateError,
	kInvalidGenerationsError,
	kInvalidDemographyAndStructureError,
	kInvalidOutputError,
	kInvalidInitializationError,
	kInvalidSeedError,
	kInvalidPredeterminedMutationsError,
	kInvalidGeneConversionError
};

// an enumeration of possible expectations regarding the presence of an end-of-file in EatSubstringWithPrefixAndCharactersAtEOF
enum EOFExpected
{
	kAgnosticEOFExpectation = -1,
	kEOFNotExpected = 0,
	kEOFExpected = 1
};


// get one line of input, sanitizing by removing comments and whitespace
void GetInputLine(ifstream &p_input_file, string &p_line);

// generate cerr output describing an error that has occurred
void InputError(InputErrorType p_error_type, string p_line);

// eat a substring matching a set of possible characters, with an optional prefix and an optional EOF expectation; return false if expectations are not met
bool EatSubstringWithCharactersAtEOF(istringstream &p_string_stream, string &p_substring, const char *p_match_chars, EOFExpected p_eof_expected);
bool EatSubstringWithPrefixAndCharactersAtEOF(istringstream &p_string_stream, string &p_substring, const char *p_prefix, const char *p_match_chars, EOFExpected p_eof_expected);


void GetInputLine(ifstream &p_input_file, string &p_line)
{
	getline(p_input_file, p_line);
	
	// remove all after "//", the comment start sequence
	// BCH 16 Dec 2014: note this was "/" in SLiM 1.8 and earlier, changed to allow full filesystem paths to be specified.
	if (p_line.find("//") != string::npos)
		p_line.erase(p_line.find("//"));
	
	// remove leading and trailing whitespace
	p_line.erase(0, p_line.find_first_not_of(' '));
	p_line.erase(p_line.find_last_not_of(' ') + 1);
}

void InputError(InputErrorType p_error_type, string p_line)
{
	cerr << endl;
	
	switch (p_error_type)
	{
		case kNoPopulationDefinedError:
			cerr << "ERROR (parameter file): no population to simulate:" << endl << endl;
			break;
			
		case kUnknownParameterError:
			cerr << "ERROR (parameter file): unknown parameter: " << p_line << endl << endl;
			break;
			
		case kInvalidParameterFileError:
			cerr << "ERROR (parameter file): could not open: " << p_line << endl << endl;
			break;
			
		case kInvalidMutationRateError:
			cerr << "ERROR (parameter file): invalid mutation rate: " << p_line << endl << endl;
			cerr << "Required syntax:" << endl << endl;
			cerr << "#MUTATION RATE" << endl;
			cerr << "<u>" << endl << endl;
			cerr << "Example:" << endl << endl;
			cerr << "#MUTATION RATE" << endl;
			cerr << "1.5e-8" << endl << endl;
			break;
			
		case kInvalidMutationTypeError:
			cerr << "ERROR (parameter file): invalid mutation type: " << p_line << endl << endl;
			cerr << "Required syntax:" << endl << endl;
			cerr << "#MUTATION TYPES" << endl;
			cerr << "<mutation-type-id> <h> <DFE-type> [DFE parameters]" << endl;
			cerr << "..." << endl << endl;
			cerr << "Example:" << endl << endl;
			cerr << "#MUTATION TYPES" << endl;
			cerr << "m1 0.2 g -0.05 0.2" << endl;
			cerr << "m2 0.0 f 0.0" << endl;
			cerr << "m3 0.5 e 0.01" << endl << endl;
			break;
			
		case kInvalidGenomicElementTypeError:
			cerr << "ERROR (parameter file): invalid genomic element type: " << p_line << endl << endl;
			cerr << "Required syntax:" << endl << endl;
			cerr << "#GENOMIC ELEMENT TYPES" << endl;
			cerr << "<element-type-id> <mut-type> <x> [<mut-type> <x>...]" << endl;
			cerr << "..." << endl << endl;
			cerr << "Example:" << endl << endl;
			cerr << "#GENOMIC ELEMENT TYPES" << endl;
			cerr << "g1 m3 0.8 m2 0.01 m1 0.19" << endl << endl;
			break;
			
		case kInvalidChromosomeOrganizationError:
			cerr << "ERROR (parameter file): invalid chromosome organization: " << p_line << endl << endl;
			cerr << "Required syntax:" << endl << endl;
			cerr << "#CHROMOSOME ORGANIZATION" << endl;
			cerr << "<element-type> <start> <end>" << endl;
			cerr << "..." << endl << endl;
			cerr << "Example:" << endl << endl;
			cerr << "#CHROMOSOME ORGANIZATION" << endl;
			cerr << "g1 1000 1999" << endl << endl;
			break;
			
		case kInvalidRecombinationRateError:
			cerr << "ERROR (parameter file): invalid recombination rate: " << p_line << endl << endl;
			cerr << "Required syntax:" << endl << endl;
			cerr << "#RECOMBINATION RATE" << endl;
			cerr << "<interval-end> <r>" << endl;
			cerr << "..." << endl << endl;
			cerr << "Example:" << endl << endl;
			cerr << "#RECOMBINATION RATE" << endl;
			cerr << "10000 1e-8" << endl;
			cerr << "20000 4.5e-8" << endl << endl;
			break;
			
		case kInvalidGenerationsError:
			cerr << "ERROR (parameter file): invalid generations: " << p_line << endl << endl;
			cerr << "Required syntax:" << endl << endl;
			cerr << "#GENERATIONS" << endl;
			cerr << "<t> [<start>]" << endl << endl;
			cerr << "Example:" << endl << endl;
			cerr << "#GENERATIONS" << endl;
			cerr << "10000" << endl << endl;
			break;
			
		case kInvalidDemographyAndStructureError:
			cerr << "ERROR (parameter file): invalid demography and structure: " << p_line << endl << endl;
			cerr << "Required syntax:" << endl << endl;
			cerr << "#DEMOGRAPHY AND STRUCTURE" << endl;
			cerr << "<time> <event-type> [event parameters]" << endl;
			cerr << "..." << endl << endl;
			cerr << "Example:" << endl << endl;
			cerr << "DEMOGRAPHY AND STRUCTURE" << endl;
			cerr << "1 P p1 1000" << endl;
			cerr << "1 S p1 0.05" << endl;
			cerr << "1000 P p2 100 p1" << endl;
			cerr << "1000 S p2 0.05" << endl;
			cerr << "2000 N p1 1e4" << endl;
			cerr << "2000 M p2 p1 0.01" << endl << endl;
			break;
			
		case kInvalidOutputError:
			cerr << "ERROR (parameter file): invalid output: " << p_line << endl << endl;
			cerr << "Required syntax:" << endl << endl;
			cerr << "#OUTPUT" << endl;
			cerr << "<time> <output-type> [output parameters]" << endl;
			cerr << "..." << endl << endl;
			cerr << "Example:" << endl << endl;
			cerr << "OUTPUT" << endl;
			cerr << "2000 A outfile" << endl;
			cerr << "1000 R p1 10" << endl;
			cerr << "1000 R p1 10 MS" << endl;
			cerr << "2000 F" << endl;
			cerr << "1 T m3" << endl << endl;
			break;
			
		case kInvalidInitializationError:
			cerr << "ERROR (parameter file): invalid initialization: " << p_line << endl << endl;
			cerr << "Required syntax:" << endl << endl;
			cerr << "#INITIALIZATION" << endl;
			cerr << "<filename>" << endl << endl;
			cerr << "Example:" << endl << endl;
			cerr << "#INITIALIZATION" << endl;
			cerr << "outfile" << endl << endl;
			break;
			
		case kInvalidSeedError:
			cerr << "ERROR (parameter file): invalid seed: " << p_line << endl << endl;
			cerr << "Required syntax:" << endl << endl;
			cerr << "#SEED" << endl;
			cerr << "<seed>" << endl << endl;
			cerr << "Example:" << endl << endl;
			cerr << "#SEED" << endl;
			cerr << "141235" << endl << endl;
			break;
			
		case kInvalidPredeterminedMutationsError:
			cerr << "ERROR (parameter file): invalid predetermined mutations: " << p_line << endl << endl;
			cerr << "Required syntax:" << endl << endl;
			cerr << "#PREDETERMINED MUTATIONS" << endl;
			cerr << "<time> <mut-type> <x> <pop> <nAA> <nAa>" << endl << endl;
			cerr << "Example:" << endl << endl;
			cerr << "#PREDETERMINED MUTATIONS" << endl;
			cerr << "5000 m7 45000 p1 0 1" << endl << endl;
			break;
			
		case kInvalidGeneConversionError:
			cerr << "ERROR (parameter file): invalid gene conversion: " << p_line << endl << endl;
			cerr << "Required syntax:" << endl << endl;
			cerr << "#GENE CONVERSION" << endl;
			cerr << "<fraction> <average-length>" << endl << endl;
			cerr << "Example:" << endl << endl;
			cerr << "#GENE CONVERSION" << endl;
			cerr << "0.5 20" << endl << endl;
			break;
	}
	
	exit(EXIT_FAILURE);
}

bool EatSubstringWithCharactersAtEOF(istringstream &p_string_stream, string &p_substring, const char *p_match_chars, EOFExpected p_eof_expected)
{
	return EatSubstringWithPrefixAndCharactersAtEOF(p_string_stream, p_substring, "", p_match_chars, p_eof_expected);
}

bool EatSubstringWithPrefixAndCharactersAtEOF(istringstream &p_string_stream, string &p_substring, const char *p_prefix, const char *p_match_chars, EOFExpected p_eof_expected)
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
	if (p_eof_expected == kEOFNotExpected)
	{
		if (p_string_stream.eof())
			good = false;
	}
	else if (p_eof_expected == kEOFExpected)
	{
		if (!p_string_stream.eof())
			good = false;
	}
	
	// if we do not expect to be at the end of string_stream, or we are agnostic, then prefetch the next substring
	if (p_eof_expected == kEOFNotExpected)
	{
		p_string_stream >> p_substring;
	}
	else if (p_eof_expected == kAgnosticEOFExpectation)
	{
		// FIXME once I understand streams better this can probably be fixed up...
		if (p_string_stream.eof())
			p_substring = "";
		else
			p_string_stream >> p_substring;
	}
	
	return good;
}


void SLiMSim::CheckInputFile(const char *p_input_file)
{
	int num_mutation_types = 0;
	int num_mutation_rates = 0;
	int num_genomic_element_types = 0;
	int num_chromosome_organizations = 0;
	int num_recombination_rates = 0;
	int num_generations = 0;
	int num_subpopulations = 0;
	
	ifstream infile (p_input_file);
	
	if (!infile.is_open())
		InputError(kInvalidParameterFileError, string(p_input_file));
	
	string line, sub;
	
	GetInputLine(infile, line);
	
	while (!infile.eof())
	{
		if (line.find('#') != string::npos) 
		{
			bool good = true;
			
			if (line.find("MUTATION RATE") != string::npos)
			{
				do
				{
					GetInputLine(infile, line);
					if (line.find('#') != string::npos || infile.eof()) break;
					if (line.length() == 0) continue;
					
					istringstream iss(line);
					iss >> sub;
					
					good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890.e-", kEOFExpected);						// Chromosome.overall_mutation_rate_
					
					if (!good)
						InputError(kInvalidMutationRateError, line);
					else
						num_mutation_rates++;
				} while (true);
				
				continue;
			}
			
			if (line.find("MUTATION TYPES") != string::npos)
			{
				do
				{
					GetInputLine(infile, line);
					if (line.find('#') != string::npos || infile.eof()) break;
					if (line.length() == 0) continue;
					
					istringstream iss(line);
					iss >> sub;
					
					good = good && EatSubstringWithPrefixAndCharactersAtEOF(iss, sub, "m", "1234567890", kEOFNotExpected);			// id: Chromosome.mutation_types_ index
					good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890.-", kEOFNotExpected);						// MutationType.dominance_coeff_
					
					string dfe_type = sub;
					good = good && EatSubstringWithCharactersAtEOF(iss, sub, "fge", kEOFNotExpected);								// MutationType.dfe_type_
					
					if (dfe_type.compare("f") == 0 || dfe_type.compare("e") == 0)													// MutationType.dfe_parameters_: one parameter
					{ 
						good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890.-", kEOFExpected);
					}
					else if (dfe_type.compare("g") == 0)																			// MutationType.dfe_parameters_: two parameters
					{
						good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890.-", kEOFNotExpected);
						good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890.-", kEOFExpected);
					}
					
					if (!good)
						InputError(kInvalidMutationTypeError, line);
					else
						num_mutation_types++;
				} while (true);
				
				continue;
			}
			
			
			if (line.find("GENOMIC ELEMENT TYPES") != string::npos)
			{
				do
				{
					GetInputLine(infile, line);
					if (line.find('#') != string::npos || infile.eof()) break;
					if (line.length() == 0) continue;
					
					istringstream iss(line);
					iss >> sub;
					
					good = good && EatSubstringWithPrefixAndCharactersAtEOF(iss, sub, "g", "1234567890", kEOFNotExpected);			// id: Chromosome.genomic_element_types_ index
					
					while (sub.length() > 0)
					{
						good = good && EatSubstringWithPrefixAndCharactersAtEOF(iss, sub, "m", "1234567890", kEOFNotExpected);		// GenomicElementType.mutation_types_
						good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890e.", kAgnosticEOFExpectation);			// GenomicElementType.mutation_fraction_
					}
					
					if (!good)
						InputError(kInvalidGenomicElementTypeError, line);
					else
						num_genomic_element_types++;
				} while (true);
				
				continue;
			}
			
			if (line.find("CHROMOSOME ORGANIZATION") != string::npos)
			{
				do
				{
					GetInputLine(infile, line);
					if (line.find('#') != string::npos || infile.eof()) break;
					if (line.length() == 0) continue;
					
					istringstream iss(line);
					iss >> sub;
					
					good = good && EatSubstringWithPrefixAndCharactersAtEOF(iss, sub, "g", "1234567890", kEOFNotExpected);			// GenomicElement.genomic_element_type_
					good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890e", kEOFNotExpected);						// GenomicElement.start_position_
					good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890e", kEOFExpected);							// GenomicElement.end_position_
					
					if (!good)
						InputError(kInvalidChromosomeOrganizationError, line);
					else
						num_chromosome_organizations++;
				} while (true);
				
				continue;
			}
			
			if (line.find("RECOMBINATION RATE") != string::npos)
			{
				do
				{
					GetInputLine(infile, line);
					if (line.find('#') != string::npos || infile.eof()) break;
					if (line.length() == 0) continue;
					
					istringstream iss(line);
					iss >> sub;
					
					good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890e", kEOFNotExpected);						// Chromosome.recombination_end_positions_
					good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890e.-", kEOFExpected);						// Chromosome.recombination_rates_
					
					if (!good)
						InputError(kInvalidRecombinationRateError, line);
					else
						num_recombination_rates++;
				} while (true);
				
				continue;
			}
			
			if (line.find("GENE CONVERSION") != string::npos)
			{
				do
				{
					GetInputLine(infile, line);
					if (line.find('#') != string::npos || infile.eof()) break;
					if (line.length() == 0) continue;
					
					istringstream iss(line);
					iss >> sub;
					
					good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890e.-", kEOFNotExpected);						// Chromosome.gene_conversion_fraction_
					good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890e.-", kEOFExpected);						// Chromosome.gene_conversion_avg_length_
					
					if (!good)
						InputError(kInvalidGeneConversionError, line);
				} while (true);
				
				continue;
			}
			
			if (line.find("GENERATIONS") != string::npos)
			{
				do
				{
					GetInputLine(infile, line);
					if (line.find('#') != string::npos || infile.eof()) break;
					if (line.length() == 0) continue;
					
					istringstream iss(line);
					iss >> sub;
					
					good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890e", kAgnosticEOFExpectation);				// main() time_duration
					
					if (sub.length() > 0)
						good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890e", kEOFExpected);						// [main() time_start]
					
					if (!good)
						InputError(kInvalidGenerationsError, line);
					else
						num_generations++;
				} while (true);
				
				continue;
			}
			
			if (line.find("DEMOGRAPHY AND STRUCTURE") != string::npos)
			{
				do
				{
					GetInputLine(infile, line);
					if (line.find('#') != string::npos || infile.eof()) break;
					if (line.length() == 0) continue;
					
					istringstream iss(line);
					iss >> sub;
					
					good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890e", kEOFNotExpected);						// time: main() events index
					
					string event_type = sub;
					good = good && EatSubstringWithCharactersAtEOF(iss, sub, "PSMN", kEOFNotExpected);								// Event.event_type_

					if (event_type.compare("P") == 0)																				// === TYPE P: two or three positive integers
					{
						good = good && EatSubstringWithPrefixAndCharactersAtEOF(iss, sub, "p", "1234567890", kEOFNotExpected);		// Event.parameters_: uint p1
						good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890e", kAgnosticEOFExpectation);			// Event.parameters_: uint N
						
						if (sub.length() > 0)
							good = good && EatSubstringWithPrefixAndCharactersAtEOF(iss, sub, "p", "1234567890", kEOFExpected);		// Event.parameters_: [uint p2]
						
						num_subpopulations++;
					}
					else if (event_type.compare("N") == 0)																			// === TYPE N: two positive integers
					{ 
						good = good && EatSubstringWithPrefixAndCharactersAtEOF(iss, sub, "p", "1234567890", kEOFNotExpected);		// Event.parameters_: uint p1
						good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890e", kEOFExpected);						// Event.parameters_: uint N
					}
					else if (event_type.compare("M") == 0)																			// === TYPE M: two positive integers and a double
					{ 
						good = good && EatSubstringWithPrefixAndCharactersAtEOF(iss, sub, "p", "1234567890", kEOFNotExpected);		// Event.parameters_: uint p
						good = good && EatSubstringWithPrefixAndCharactersAtEOF(iss, sub, "p", "1234567890", kEOFNotExpected);		// Event.parameters_: uint p
						good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890.-e", kEOFExpected);					// Event.parameters_: double M
					}
					else if (event_type.compare("S") == 0)																			// === TYPE S: one positive integer and a double
					{ 
						good = good && EatSubstringWithPrefixAndCharactersAtEOF(iss, sub, "p", "1234567890", kEOFNotExpected);		// Event.parameters_: uint p
						good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890.-e", kEOFExpected);					// Event.parameters_: double sigma
					}
					
					if (!good)
						InputError(kInvalidDemographyAndStructureError, line);
				} while (true);
				
				continue;
			}
			
			if (line.find("OUTPUT") != string::npos)
			{
				do
				{
					GetInputLine(infile, line);
					if (line.find('#') != string::npos || infile.eof()) break;
					if (line.length() == 0) continue;
					
					istringstream iss(line);
					iss >> sub;
					
					good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890e", kEOFNotExpected);						// time: main() outputs index

					string output_type = sub;
					good = good && EatSubstringWithCharactersAtEOF(iss, sub, "ARFT", kAgnosticEOFExpectation);						// Event.event_type_
					
					if (output_type.compare("A") == 0)																				// === TYPE A: no parameter, or a filename
					{
						// sub may or may not now contain a filename																// Event.parameters_: [filename]
						// we don't make an EatSubstring call here because we don't do a lexical check on filenames
					}
					else if (output_type.compare("R") == 0)																			// === TYPE R: two positive integers
					{
						good = good && EatSubstringWithPrefixAndCharactersAtEOF(iss, sub, "p", "1234567890", kEOFNotExpected);		// Event.parameters_: uint p
						good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890", kAgnosticEOFExpectation);			// Event.parameters_: uint size
						
						if (sub.length() > 0 && sub != "MS")																		// Event.parameters_: ['MS']
								good = false;
					}
					else if (output_type.compare("F") == 0)																			// === TYPE F: no parameter
					{
					}
					else if (output_type.compare("T") == 0)																			// === TYPE T: ??? FIXME code for this case missing!
					{
					}
					
					if (!iss.eof())
						good = false;
					
					if (!good)
						InputError(kInvalidOutputError, line);
				} while (true);
				
				continue;
			}
			
			if (line.find("PREDETERMINED MUTATIONS") != string::npos)
			{
				do
				{
					GetInputLine(infile, line);
					if (line.find('#') != string::npos || infile.eof()) break;
					if (line.length() == 0) continue;
					
					istringstream iss(line);
					iss >> sub;
					
					good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890e", kEOFNotExpected);						// Mutation.generation_
					good = good && EatSubstringWithPrefixAndCharactersAtEOF(iss, sub, "m", "1234567890", kEOFNotExpected);			// Mutation.mutation_type_
					good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890e", kEOFNotExpected);						// Mutation.position_
					good = good && EatSubstringWithPrefixAndCharactersAtEOF(iss, sub, "p", "1234567890", kEOFNotExpected);			// Mutation.subpop_index_
					good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890", kEOFNotExpected);						// IntroducedMutation.num_AA_
					good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890", kAgnosticEOFExpectation);				// IntroducedMutation.num_Aa_
					
					if (sub.length() > 0)
					{
						good = good && EatSubstringWithPrefixAndCharactersAtEOF(iss, sub, "P", "", kEOFNotExpected);				// ['P']
						good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890.-e", kEOFExpected);					// PartialSweep.target_prevalence_
					}
					
					if (!good)
						InputError(kInvalidPredeterminedMutationsError, line);
				} while (true);
				
				continue;
			}
			
			if (line.find("SEED") != string::npos)
			{
				do
				{
					GetInputLine(infile, line);
					if (line.find('#') != string::npos || infile.eof()) break;
					if (line.length() == 0) continue;
					
					istringstream iss(line);
					iss >> sub;
					
					good = good && EatSubstringWithCharactersAtEOF(iss, sub, "1234567890-", kEOFExpected);							// Initialize() seed
					
					if (!good)
						InputError(kInvalidSeedError, line);
				} while (true);
				
				continue;
			}
			
			if (line.find("INITIALIZATION") != string::npos)
			{
				do
				{
					GetInputLine(infile, line);
					if (line.find('#') != string::npos || infile.eof()) break;
					if (line.length() == 0) continue;
					
					istringstream iss(line);
					iss >> sub;
					
					// sub should now contain a filename, but no checking of filenames is presently done here						// filename
					
					if (!iss.eof())
						good = false;
					
					if (!good)
						InputError(kInvalidInitializationError, line);
					
					num_subpopulations++;
				} while (true);
				
				continue;
			}
			
			InputError(kUnknownParameterError, line);
		}
		else
		{
			GetInputLine(infile, line);
			//FIXME should this really not signal an error?  I think it should...
		}
	}
	
	// Check that all elements occurred an allowed number of times.  FIXME check these; shouldn't some of the "< 1" tests be "!= 1" tests?
	if (num_mutation_rates != 1)
		InputError(kInvalidMutationRateError, string());
	
	if (num_mutation_types < 1)
		InputError(kInvalidMutationTypeError, string());
	
	if (num_genomic_element_types < 1)
		InputError(kInvalidGenomicElementTypeError, string());
	
	if (num_chromosome_organizations < 1)
		InputError(kInvalidChromosomeOrganizationError, string());
	
	if (num_recombination_rates < 1)
		InputError(kInvalidRecombinationRateError, string());
	
	if (num_generations < 1)
		InputError(kInvalidGenerationsError, string());
	
	if (num_subpopulations < 1)
		InputError(kNoPopulationDefinedError, string());
	
	if (DEBUG_INPUT)
	{
		std::cout << "CheckInputFile: file checked out:" << endl;
		std::cout << "   num_mutation_rates == " << num_mutation_rates << endl;
		std::cout << "   num_mutation_types == " << num_mutation_types << endl;
		std::cout << "   num_genomic_element_types == " << num_genomic_element_types << endl;
		std::cout << "   num_chromosome_organizations == " << num_chromosome_organizations << endl;
		std::cout << "   num_recombination_rates == " << num_recombination_rates << endl;
		std::cout << "   num_generations == " << num_generations << endl;
		std::cout << "   num_subpopulations == " << num_subpopulations << endl;
	}
}

void SLiMSim::InitializePopulationFromFile(const char *p_file)
{
	std::map<int,Mutation> M;
	string line, sub; 
	ifstream infile (p_file);
	
	if (!infile.is_open())
	{
		std::cerr << "ERROR (Initialize): could not open initialization file" << endl;
		exit(EXIT_FAILURE);
	}
	
	GetInputLine(infile, line);
	
	while (line.find("Populations") == string::npos && !infile.eof())
		GetInputLine(infile, line);
	
	GetInputLine(infile, line);
	
	while (line.find("Mutations") == string::npos && !infile.eof())
	{ 
		istringstream iss(line);
		iss >> sub;
		
		sub.erase(0, 1);  
		int i = atoi(sub.c_str());
		iss >> sub;
		
		int n = atoi(sub.c_str());
		population_.AddSubpopulation(i, n);
		
		GetInputLine(infile, line);      
	}
	
	GetInputLine(infile, line);
	
	while (line.find("Genomes") == string::npos && !infile.eof()) 
	{
		istringstream iss(line);
		iss >> sub; 
		
		int id = atoi(sub.c_str());
		iss >> sub;
		
		sub.erase(0, 1);
		int t = atoi(sub.c_str());
		iss >> sub;
		
		int x = atoi(sub.c_str()) - 1;
		iss >> sub;
		
		double s = atof(sub.c_str());
		iss >> sub;
		iss >> sub;
		
		sub.erase(0, 1);
		int i = atoi(sub.c_str());
		iss >> sub;
		
		int g = atoi(sub.c_str());
		
		auto found_muttype_pair = mutation_types_.find(t);
		
		if (found_muttype_pair == mutation_types_.end()) 
		{ 
			cerr << "ERROR (InitializePopulationFromFile): mutation type m"<< t << " has not been defined" << endl;
			exit(EXIT_FAILURE); 
		}
		
		const MutationType *mutation_type_ptr = found_muttype_pair->second;
		M.insert(std::pair<const int,Mutation>(id, Mutation(mutation_type_ptr, x, s, i, g)));
		
		GetInputLine(infile, line); 
	}
	
	GetInputLine(infile, line);
	
	while (!infile.eof())
	{
		istringstream iss(line); iss >> sub; sub.erase(0, 1);
		int pos = static_cast<int>(sub.find_first_of(":")); 
		int p = atoi(sub.substr(0, pos + 1).c_str()); sub.erase(0, pos + 1);  
		int i = atoi(sub.c_str());
		
		while (iss >> sub) 
		{
			int id = atoi(sub.c_str());
			auto found_subpop_pair = population_.find(p);
			
			if (found_subpop_pair == population_.end()) 
			{ 
				cerr << "ERROR (InitializePopulationFromFile): subpopulation p"<< p << " has not been defined" << endl;
				exit(EXIT_FAILURE); 
			}
			
			auto found_mut_pair = M.find(id);
			
			if (found_mut_pair == M.end()) 
			{ 
				cerr << "ERROR (InitializePopulationFromFile): mutation "<< id << " has not been defined" << endl;
				exit(EXIT_FAILURE); 
			}
			
			Mutation &mutation = found_mut_pair->second;
			
			found_subpop_pair->second->parent_genomes_[i - 1].push_back(mutation);
		}
		
		GetInputLine(infile, line);
	}
	
	for (std::pair<const int,Subpopulation*> &subpop_pair : population_)
		subpop_pair.second->UpdateFitness();
}

void SLiMSim::InitializeFromFile(const char *p_input_file)
{
	string line, sub; 
	ifstream infile (p_input_file);
	
	if (!rng_seed_supplied_to_constructor_)
		rng_seed_ = GenerateSeedFromPIDAndTime();
	
	if (DEBUG_INPUT)
		std::cout << "Initialize():" << endl;
	
	GetInputLine(infile, line);
	
	while (!infile.eof())
	{
		if (line.find('#') != string::npos) 
		{
			if (line.find("MUTATION RATE") != string::npos)
			{
				input_parameters_.push_back("#MUTATION RATE");
				
				do
				{
					GetInputLine(infile, line);
					if (line.find('#') != string::npos || infile.eof()) break;
					if (line.length() == 0) continue;
					input_parameters_.push_back(line);
					
					// FORMAT: overall_mutation_rate
					istringstream iss(line);
					
					iss >> sub;
					double overall_mutation_rate = atof(sub.c_str());
					
					chromosome_.overall_mutation_rate_ = overall_mutation_rate;
					
					if (DEBUG_INPUT)
						std::cout << "   #MUTATION RATE: overall_mutation_rate " << overall_mutation_rate << endl;
				} while (true);
				
				continue;
			}
			
			if (line.find("MUTATION TYPES") != string::npos)
			{
				input_parameters_.push_back("#MUTATION TYPES");
				
				do
				{
					GetInputLine(infile, line);
					if (line.find('#') != string::npos || infile.eof()) break;
					if (line.length() == 0) continue;
					input_parameters_.push_back(line);
					
					// FORMAT: map_identifier dominance_coeff dfe_type dfe_parameters...
					std::vector<double> dfe_parameters;
					istringstream iss(line);
					
					iss >> sub;
					sub.erase(0, 1);	// m
					int map_identifier = atoi(sub.c_str());
					
					if (mutation_types_.count(map_identifier) > 0) 
					{  
						std::cerr << "ERROR (Initialize): mutation type " << map_identifier << " already defined" << endl;
						exit(EXIT_FAILURE);
					}
					
					iss >> sub;
					double dominance_coeff = atof(sub.c_str());
					
					iss >> sub;
					char dfe_type = sub.at(0);
					
					while (iss >> sub)
						dfe_parameters.push_back(atof(sub.c_str()));
					
					MutationType *new_mutation_type = new MutationType(map_identifier, dominance_coeff, dfe_type, dfe_parameters);
					
					mutation_types_.insert(std::pair<const int,MutationType*>(map_identifier, new_mutation_type));
					
					if (DEBUG_INPUT)
						std::cout << "   #MUTATION TYPES: " << "m" << map_identifier << " " << *new_mutation_type << endl;
				} while (true);
				
				continue;
			}
			
			if (line.find("GENOMIC ELEMENT TYPES") != string::npos)
			{
				input_parameters_.push_back("#GENOMIC ELEMENT TYPES");
				
				do
				{
					GetInputLine(infile, line);
					if (line.find('#') != string::npos || infile.eof()) break;
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
						{
							std::cerr << "ERROR (Initialize): mutation type m" << mutation_type_id << " not defined" << endl;
							exit(EXIT_FAILURE);
						}
						
						MutationType *mutation_type_ptr = found_muttype_pair->second;
						mutation_types.push_back(mutation_type_ptr);
						iss >> sub;
						
						mutation_fractions.push_back(atof(sub.c_str()));
					}
					
					if (genomic_element_types_.count(map_identifier) > 0) 
					{
						std::cerr << "ERROR (Initialize): genomic element type " << map_identifier << " already defined" << endl;
						exit(EXIT_FAILURE);
					}
					
					GenomicElementType *new_genomic_element_type = new GenomicElementType(map_identifier, mutation_types, mutation_fractions);
					genomic_element_types_.insert(std::pair<const int,GenomicElementType*>(map_identifier, new_genomic_element_type));
					
					if (DEBUG_INPUT)
						std::cout << "   #GENOMIC ELEMENT TYPES: " << "g" << map_identifier << " " << new_genomic_element_type << endl;
				} while (true);
				
				continue;
			}
			
			if (line.find("CHROMOSOME ORGANIZATION") != string::npos)
			{
				input_parameters_.push_back("#CHROMOSOME ORGANIZATION");
				
				do
				{
					GetInputLine(infile, line);
					if (line.find('#') != string::npos || infile.eof()) break;
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
					{
						std::cerr << "ERROR (Initialize): genomic element type m" << genomic_element_type << " not defined" << endl;
						exit(EXIT_FAILURE);
					}
					
					const GenomicElementType *genomic_element_type_ptr = found_getype_pair->second;
					GenomicElement new_genomic_element = GenomicElement(genomic_element_type_ptr, start_position, end_position);
					chromosome_.push_back(new_genomic_element);
					
					if (DEBUG_INPUT)
						std::cout << "   #CHROMOSOME ORGANIZATION: " << new_genomic_element << endl;
				} while (true);
				
				continue;
			}
			
			if (line.find("RECOMBINATION RATE") != string::npos)
			{
				input_parameters_.push_back("#RECOMBINATION RATE");
				
				do
				{
					GetInputLine(infile, line);
					if (line.find('#') != string::npos || infile.eof()) break;
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
						std::cout << "   #RECOMBINATION RATE: recombination_end_position " << recombination_end_position << ", recombination_rate " << recombination_rate << endl;
				} while (true);
				
				continue;
			}
			
			if (line.find("GENE CONVERSION") != string::npos)
			{
				input_parameters_.push_back("#GENE CONVERSION");
				
				do
				{
					GetInputLine(infile, line);
					if (line.find('#') != string::npos || infile.eof()) break;
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
						std::cout << "   #GENE CONVERSION: gene_conversion_fraction " << gene_conversion_fraction << ", gene_conversion_avg_length_ " << gene_conversion_avg_length << endl;
				} while (true);
				
				continue;
			}
			
			if (line.find("GENERATIONS") != string::npos)
			{
				input_parameters_.push_back("#GENERATIONS");
				
				do
				{
					GetInputLine(infile, line);
					if (line.find('#') != string::npos || infile.eof()) break;
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
						std::cout << "   #GENERATIONS: time_duration " << time_duration_ << ", time_start " << time_start_ << endl;
				} while (true);
				
				continue;
			}
			
			if (line.find("DEMOGRAPHY AND STRUCTURE") != string::npos)
			{
				input_parameters_.push_back("#DEMOGRAPHY AND STRUCTURE");
				
				do
				{
					GetInputLine(infile, line);
					if (line.find('#') != string::npos || infile.eof()) break;
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
						std::cout << "   #DEMOGRAPHY AND STRUCTURE: event_time " << event_time << " " << *new_event_ptr << endl;
				} while (true);
				
				continue;
			}
			
			if (line.find("OUTPUT") != string::npos)
			{
				input_parameters_.push_back("#OUTPUT");
				
				do
				{
					GetInputLine(infile, line);
					if (line.find('#') != string::npos || infile.eof()) break;
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
						std::cout << "   #OUTPUT: event_time " << event_time << " " << *new_event_ptr << endl;
				} while (true);
				
				continue;
			}
			
			if (line.find("PREDETERMINED MUTATIONS") != string::npos)
			{
				input_parameters_.push_back("#PREDETERMINED MUTATIONS");
				
				do
				{
					GetInputLine(infile, line);
					if (line.find('#') != string::npos || infile.eof()) break;
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
					{
						std::cerr << "ERROR (Initialize): mutation type m" << mutation_type_id << " not defined" << endl;
						exit(EXIT_FAILURE);
					}
					
					const MutationType *mutation_type_ptr = found_muttype_pair->second;
					
					const IntroducedMutation *new_introduced_mutation = new IntroducedMutation(mutation_type_ptr, position, subpop_index, generation, num_AA, num_Aa);
					
					introduced_mutations_.insert(std::pair<const int,const IntroducedMutation*>(generation, new_introduced_mutation));
					
					if (DEBUG_INPUT)
						std::cout << "   #PREDETERMINED MUTATIONS: generation " << generation << " " << *new_introduced_mutation << endl;
					
					while (iss >> sub) 
					{ 
						if (sub.find('P') != string::npos) 
						{
							iss >> sub;
							double target_prevalence = atof(sub.c_str());
							const PartialSweep *new_partial_sweep = new PartialSweep(mutation_type_ptr, position, target_prevalence);
							
							partial_sweeps_.push_back(new_partial_sweep);
							
							if (DEBUG_INPUT)
								std::cout << "      " << *new_partial_sweep << endl;
						}
					}
				} while (true);
				
				continue;
			}
			
			if (line.find("SEED") != string::npos)
			{
				// #SEED is pushed back below
				
				do
				{
					GetInputLine(infile, line);
					if (line.find('#') != string::npos || infile.eof()) break;
					if (line.length() == 0) continue;
					// seed is pushed back below
					
					// FORMAT: seed
					istringstream iss(line);
					
					iss >> sub;
					int directive_seed = atoi(sub.c_str());
					
					if (rng_seed_supplied_to_constructor_)
						rng_seed_ = directive_seed;
					
					if (DEBUG_INPUT)
						std::cout << "   #SEED: seed " << directive_seed << endl;
				} while (true);
				
				continue;
			}
			
			if (line.find("INITIALIZATION") != string::npos)
			{
				input_parameters_.push_back("#INITIALIZATION");
				
				do
				{
					GetInputLine(infile, line);
					if (line.find('#') != string::npos || infile.eof()) break;
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
	
	// initialize chromosome
	chromosome_.InitializeDraws();
	
	// initialize rng
	InitializeRNGFromSeed(rng_seed_);
	
	input_parameters_.push_back("#SEED");
	
	std::stringstream ss;
	ss << rng_seed_;
	input_parameters_.push_back(ss.str());
	
	// parameter output
	for (int i = 0; i < input_parameters_.size(); i++)
		std::cout << input_parameters_[i] << endl;
}



































































