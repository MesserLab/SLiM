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


#include "input_parsing.h"

#include <fstream>
#include <sstream>


using std::cerr;
using std::endl;
using std::string;
using std::istringstream;


void get_line(std::ifstream& infile, string& line)
{
	getline(infile,line);
	if (line.find("/")!= string::npos) { line.erase(line.find("/")); } // remove all after "/"
	line.erase(0,line.find_first_not_of(' ') ); // remove leading whitespaces
	line.erase(line.find_last_not_of(' ') + 1); // remove trailing whitespaces
};


void input_error(int type, string line)
{
	cerr << endl;
	
	if (type==-2) // no population defined
	{
		cerr << "ERROR (parameter file): no population to simulate:" << endl << endl;
	}
	
	else if (type==-1) // unknown parameter
	{
		cerr << "ERROR (parameter file): unknown parameter: " << line << endl << endl;
	}
	
	else if (type==0) // invalid parameter file
	{
		cerr << "ERROR (parameter file): could not open: " << line << endl << endl;
	}
	
	else if (type==1) // mutation rate
	{
		cerr << "ERROR (parameter file): invalid mutation rate: " << line << endl << endl;
		cerr << "Required syntax:" << endl << endl;
		cerr << "#MUTATION RATE" << endl;
		cerr << "<u>" << endl << endl;
		cerr << "Example:" << endl << endl;
		cerr << "#MUTATION RATE" << endl;
		cerr << "1.5e-8" << endl << endl;
	}
	
	else if (type==2) // mutation type
	{
		cerr << "ERROR (parameter file): invalid mutation type: " << line << endl << endl;
		cerr << "Required syntax:" << endl << endl;
		cerr << "#MUTATION TYPES" << endl;
		cerr << "<mutation-type-id> <h> <DFE-type> [DFE parameters]" << endl;
		cerr << "..." << endl << endl;
		cerr << "Example:" << endl << endl;
		cerr << "#MUTATION TYPES" << endl;
		cerr << "m1 0.2 g -0.05 0.2" << endl;
		cerr << "m2 0.0 f 0.0" << endl;
		cerr << "m3 0.5 e 0.01" << endl << endl;
	}
	
	
	else if (type==3) // genomic element type
	{
		cerr << "ERROR (parameter file): invalid genomic element type: " << line << endl << endl;
		cerr << "Required syntax:" << endl << endl;
		cerr << "#GENOMIC ELEMENT TYPES" << endl;
		cerr << "<element-type-id> <mut-type> <x> [<mut-type> <x>...]" << endl;
		cerr << "..." << endl << endl;
		cerr << "Example:" << endl << endl;
		cerr << "#GENOMIC ELEMENT TYPES" << endl;
		cerr << "g1 m3 0.8 m2 0.01 m1 0.19" << endl << endl;
	}
	
	else if (type==4) // chromosome organization
	{
		cerr << "ERROR (parameter file): invalid chromosome organization: " << line << endl << endl;
		cerr << "Required syntax:" << endl << endl;
		cerr << "#CHROMOSOME ORGANIZATION" << endl;
		cerr << "<element-type> <start> <end>" << endl;
		cerr << "..." << endl << endl;
		cerr << "Example:" << endl << endl;
		cerr << "#CHROMOSOME ORGANIZATION" << endl;
		cerr << "g1 1000 1999" << endl << endl;
	}
	
	else if (type==5) // recombination rate
	{
		cerr << "ERROR (parameter file): invalid recombination rate: " << line << endl << endl;
		cerr << "Required syntax:" << endl << endl;
		cerr << "#RECOMBINATION RATE" << endl;
		cerr << "<interval-end> <r>" << endl;
		cerr << "..." << endl << endl;
		cerr << "Example:" << endl << endl;
		cerr << "#RECOMBINATION RATE" << endl;
		cerr << "10000 1e-8" << endl;
		cerr << "20000 4.5e-8" << endl << endl;
	}
	
	else if (type==6) // generations
	{
		cerr << "ERROR (parameter file): invalid generations: " << line << endl << endl;
		cerr << "Required syntax:" << endl << endl;
		cerr << "#GENERATIONS" << endl;
		cerr << "<t> [<start>]" << endl << endl;
		cerr << "Example:" << endl << endl;
		cerr << "#GENERATIONS" << endl;
		cerr << "10000" << endl << endl;
	}
	
	else if (type==7) // demography and structure
	{
		cerr << "ERROR (parameter file): invalid demography and structure: " << line << endl << endl;
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
	}
	
	else if (type==8) // output
	{
		cerr << "ERROR (parameter file): invalid output: " << line << endl << endl;
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
	}
	
	else if (type==9) // initialization
	{
		cerr << "ERROR (parameter file): invalid initialization: " << line << endl << endl;
		cerr << "Required syntax:" << endl << endl;
		cerr << "#INITIALIZATION" << endl;
		cerr << "<filename>" << endl << endl;
		cerr << "Example:" << endl << endl;
		cerr << "#INITIALIZATION" << endl;
		cerr << "outfile" << endl << endl;
	}
	
	else if (type==10) // seed
	{
		cerr << "ERROR (parameter file): invalid seed: " << line << endl << endl;
		cerr << "Required syntax:" << endl << endl;
		cerr << "#SEED" << endl;
		cerr << "<seed>" << endl << endl;
		cerr << "Example:" << endl << endl;
		cerr << "#SEED" << endl;
		cerr << "141235" << endl << endl;
	}
	
	else if (type==11) // predetermined mutation
	{
		cerr << "ERROR (parameter file): invalid predetermined mutations: " << line << endl << endl;
		cerr << "Required syntax:" << endl << endl;
		cerr << "#PREDETERMINED MUTATIONS" << endl;
		cerr << "<time> <mut-type> <x> <pop> <nAA> <nAa>" << endl << endl;
		cerr << "Example:" << endl << endl;
		cerr << "#PREDETERMINED MUTATIONS" << endl;
		cerr << "5000 m7 45000 p1 0 1" << endl << endl;
	}
	
	else if (type==12) // gene conversion
	{
		cerr << "ERROR (parameter file): invalid gene conversion: " << line << endl << endl;
		cerr << "Required syntax:" << endl << endl;
		cerr << "#GENE CONVERSION" << endl;
		cerr << "<fraction> <average-length>" << endl << endl;
		cerr << "Example:" << endl << endl;
		cerr << "#GENE CONVERSION" << endl;
		cerr << "0.5 20" << endl << endl;
	}
	
	exit(1);
};


void check_input_file(char* file)
{
	int mutation_types = 0;
	int mutation_rate  = 0;
	int genomic_element_types = 0;
	int chromosome_organization = 0;
	int recombination_rate = 0;
	int generations = 0;
	int population = 0;
	
	std::ifstream infile (file);
	if (!infile.is_open()) { input_error(0,string(file)); }
	
	string line; string sub;
	
	get_line(infile,line);
	
	while (!infile.eof())
	{
		if (line.find('#') != string::npos) 
		{
	  if (line.find("MUTATION RATE") != string::npos) { get_line(infile,line);
		  while (line.find('#') == string::npos && !infile.eof()) {
			  if (line.length()>0)
			  {
				  if (line.find_first_not_of("1234567890.e-") != string::npos ) { input_error(1,line); }
				  else { mutation_rate++; }
			  }
			  get_line(infile,line); } }
			
			
	  else if (line.find("MUTATION TYPES") != string::npos) { get_line(infile,line);
		  while (line.find('#') == string::npos && !infile.eof()) {
			  if (line.length()>0)
			  {
				  int good = 1;
				  
				  istringstream iss(line); iss >> sub;
				  
				  if (sub.compare(0,1,"m") != 0) { good = 0; } sub.erase(0,1);
				  
				  if (sub.find_first_not_of("1234567890") != string::npos )   { good = 0; } // id
				  if (iss.eof()) { good = 0; } iss >> sub;
				  if (sub.find_first_not_of("1234567890.-") != string::npos ) { good = 0; } // h
				  if (iss.eof()) { good = 0; } iss >> sub; 
				  if (sub.find_first_not_of("fge") != string::npos )          { good = 0; } // DFE-type
				  
				  if (sub.compare("f")==0 || sub.compare("e")==0) // one parameter
		    { 
				if (iss.eof()) { good = 0; } iss >> sub;
				if (sub.find_first_not_of("1234567890.-") != string::npos ) { good = 0; }
				if (!iss.eof()) { good = 0; }
			}
				  if (sub.compare("g")==0) // two parameters
		    {
				if (iss.eof()) { good = 0; } iss >> sub;
				if (sub.find_first_not_of("1234567890.-") != string::npos ) { good = 0; }
				if (iss.eof()) { good = 0; } iss >> sub;
				if (sub.find_first_not_of("1234567890.-") != string::npos ) { good = 0; }
				if (!iss.eof()) { good = 0; }
			}
				  
				  if (good==0) { input_error(2,line); }
				  else { mutation_types++; }
			  }
			  get_line(infile,line); } }
			
			
	  else if (line.find("GENOMIC ELEMENT TYPES") != string::npos) { get_line(infile,line);
		  while (line.find('#') == string::npos && !infile.eof()) {
			  if (line.length()>0)
			  {
				  int good = 1;
				  
				  istringstream iss(line); iss >> sub;
				  
				  if (sub.compare(0,1,"g") != 0) { good = 0; } sub.erase(0,1);
				  
				  if (sub.find_first_not_of("1234567890") != string::npos )   { good = 0; } // id
				  if (iss.eof()) { good = 0; }
				  
				  while (!iss.eof())
		    {
				iss >> sub;
				if (sub.compare(0,1,"m") != 0) { good = 0; } sub.erase(0,1); // mutation type id
				if (sub.find_first_not_of("1234567890") != string::npos ) { good = 0; } 
				if (iss.eof()) { good = 0; } iss >> sub; 
				if (sub.find_first_not_of("1234567890e.") != string::npos ) { good = 0; } // fraction
			}
				  
				  if (good==0) { input_error(3,line); }
				  else { genomic_element_types++; }
			  }
			  get_line(infile,line); } }
			
			
	  else if (line.find("CHROMOSOME ORGANIZATION") != string::npos) { get_line(infile,line);
		  while (line.find('#') == string::npos && !infile.eof()) {
			  if (line.length()>0)
			  {
				  int good = 1;
				  
				  istringstream iss(line); iss >> sub;
				  
				  if (sub.compare(0,1,"g") != 0) { good = 0; } sub.erase(0,1);
				  
				  if (sub.find_first_not_of("1234567890") != string::npos )   { good = 0; } // id
				  if (iss.eof()) { good = 0; } iss >> sub;
				  if (sub.find_first_not_of("1234567890e") != string::npos )   { good = 0; } // start
				  if (iss.eof()) { good = 0; } iss >> sub;
				  if (sub.find_first_not_of("1234567890e") != string::npos )   { good = 0; } // end
				  if (!iss.eof()) { good = 0; }
				  
				  
				  if (good==0) { input_error(4,line); }
				  else { chromosome_organization++; }
			  }
			  get_line(infile,line); } }
			
	  
	  else if (line.find("RECOMBINATION RATE") != string::npos) { get_line(infile,line);
		  while (line.find('#') == string::npos && !infile.eof()) {
			  if (line.length()>0)
			  {
				  int good = 1;
				  
				  istringstream iss(line); iss >> sub;
				  
				  if (sub.find_first_not_of("1234567890e") != string::npos )   { good = 0; } // end
				  if (iss.eof()) { good = 0; } iss >> sub;
				  if (sub.find_first_not_of("1234567890e.-") != string::npos )   { good = 0; } // rate
				  if (!iss.eof()) { good = 0; }
				  
				  if (good==0) { input_error(5,line); }
				  else { recombination_rate++; }
			  }
			  get_line(infile,line); } }
			
			
	  else if (line.find("GENE CONVERSION") != string::npos) { get_line(infile,line);
		  while (line.find('#') == string::npos && !infile.eof()) {
			  if (line.length()>0)
			  {
				  int good = 1;
				  
				  istringstream iss(line); iss >> sub;
				  
				  if (sub.find_first_not_of("1234567890e.-") != string::npos )   { good = 0; } // fraction
				  if (iss.eof()) { good = 0; } iss >> sub;
				  if (sub.find_first_not_of("1234567890e.-") != string::npos )   { good = 0; } // average length
				  if (!iss.eof()) { good = 0; }
				  
				  if (good==0) { input_error(12,line); }
			  }
			  get_line(infile,line); } }
			
			
	  else if (line.find("GENERATIONS") != string::npos) { get_line(infile,line);
		  while (line.find('#') == string::npos && !infile.eof()) {
			  if (line.length()>0)
			  {
				  int good = 1;
				  
				  istringstream iss(line); iss >> sub;
				  
				  if (sub.find_first_not_of("1234567890e") != string::npos )   { good = 0; } // t
				  if (!iss.eof())
		    { 
				iss >> sub;
				if (sub.find_first_not_of("1234567890e") != string::npos )   { good = 0; } // start
			}
				  if (!iss.eof()) { good = 0; }
				  
				  if (good==0) { input_error(6,line); }
				  else { generations++; }
			  }
			  get_line(infile,line); } }
			
			
	  else if (line.find("DEMOGRAPHY AND STRUCTURE") != string::npos) { get_line(infile,line);
		  while (line.find('#') == string::npos && !infile.eof()) {
			  if (line.length()>0)
			  {
				  int good = 1;
				  
				  istringstream iss(line); iss >> sub;
				  
				  if (sub.find_first_not_of("1234567890e") != string::npos )   { good = 0; } // t
				  if (iss.eof()) { good = 0; } iss >> sub;
				  if (sub.find_first_not_of("PSMN") != string::npos )  { good = 0; } // event type
				  
				  if (sub.compare("P")==0) // two or three positive integers
		    { 
				if (iss.eof()) { good = 0; } iss >> sub; // p1
				if (sub.compare(0,1,"p") != 0) { good = 0; } sub.erase(0,1);
				if (sub.find_first_not_of("1234567890") != string::npos ) { good = 0; }
				
				if (iss.eof()) { good = 0; } iss >> sub; // N
				if (sub.find_first_not_of("1234567890e") != string::npos ) { good = 0; }
				
				if (!iss.eof()) // p2
				{ 
			  iss >> sub;
			  if (sub.compare(0,1,"p") != 0) { good = 0; } sub.erase(0,1);
			  if (sub.find_first_not_of("1234567890") != string::npos ) { good = 0; }
			  if (!iss.eof()) { good = 0; }
				}
				
				population++;
			}
				  
				  if (sub.compare("N")==0) // two positive integers
		    { 
				if (iss.eof()) { good = 0; } iss >> sub; // p
				if (sub.compare(0,1,"p") != 0) { good = 0; } sub.erase(0,1);
				if (sub.find_first_not_of("1234567890") != string::npos ) { good = 0; }
				if (iss.eof()) { good = 0; } iss >> sub; // N
				if (sub.find_first_not_of("1234567890e") != string::npos ) { good = 0; }		      
				if (!iss.eof()) { good = 0; }
			}
				  
				  if (sub.compare("S")==0) // one positive integer and a double
		    { 
				if (iss.eof()) { good = 0; } iss >> sub; // p
				if (sub.compare(0,1,"p") != 0) { good = 0; } sub.erase(0,1);
				if (sub.find_first_not_of("1234567890") != string::npos ) { good = 0; }
				if (iss.eof()) { good = 0; } iss >> sub; // sigma
				if (sub.find_first_not_of("1234567890.-e") != string::npos ) { good = 0; }	      
				if (!iss.eof()) { good = 0; }
			}
				  
				  if (sub.compare("M")==0) // two positive integers and a double
		    { 
				if (iss.eof()) { good = 0; } iss >> sub; // p
				if (sub.compare(0,1,"p") != 0) { good = 0; } sub.erase(0,1);
				if (sub.find_first_not_of("1234567890") != string::npos ) { good = 0; }
				if (iss.eof()) { good = 0; } iss >> sub; // p
				if (sub.compare(0,1,"p") != 0) { good = 0; } sub.erase(0,1);
				if (sub.find_first_not_of("1234567890") != string::npos ) { good = 0; }
				if (iss.eof()) { good = 0; } iss >> sub; // M
				if (sub.find_first_not_of("1234567890.-e") != string::npos ) { good = 0; }
				if (!iss.eof()) { good = 0; }
			}
				  
				  if (good==0) { input_error(7,line); }
			  }
			  get_line(infile,line); } }
			
			
	  else if (line.find("OUTPUT") != string::npos) { get_line(infile,line);
		  while (line.find('#') == string::npos && !infile.eof()) {
			  if (line.length()>0)
			  {
				  int good = 1;
				  
				  istringstream iss(line); iss >> sub;
				  
				  if (sub.find_first_not_of("1234567890e") != string::npos )   { good = 0; } // t
				  if (iss.eof()) { good = 0; } iss >> sub;
				  if (sub.find_first_not_of("ARFT") != string::npos )  { good = 0; } // event type
				  
				  if (sub.compare("A")==0) // no parameter of filename
		    { 
				if (!iss.eof()) { iss >> sub; if (!iss.eof()) { good = 0; } }
			}
				  
				  if (sub.compare("R")==0) // two positive integers
		    { 
				if (iss.eof()) { good = 0; } iss >> sub; // p1
				if (sub.compare(0,1,"p") != 0) { good = 0; } sub.erase(0,1);
				if (sub.find_first_not_of("1234567890") != string::npos ) { good = 0; }
				if (iss.eof()) { good = 0; } iss >> sub; // p2
				if (sub.find_first_not_of("1234567890") != string::npos ) { good = 0; }		      
				if (!iss.eof()) 
				{
			  iss >> sub; // MS
			  if (sub != "MS") { good = 0; }
				}
				if (!iss.eof()) { good = 0; }
			}
				  
				  if (sub.compare("F")==0) // no parameter
		    { 
				if (!iss.eof()) { good = 0; }
			}
				  
				  if (good==0) { input_error(8,line); }
			  }
			  get_line(infile,line); } }
			
			
	  else if (line.find("INITIALIZATION") != string::npos) { get_line(infile,line);
		  while (line.find('#') == string::npos && !infile.eof()) {
			  if (line.length()>0)
			  {
				  int good = 1;
				  
				  istringstream iss(line); iss >> sub;
				  if (!iss.eof()) { good = 0; }
				  
				  if (good==0) { input_error(9,line); }
				  
				  population++;
			  }
			  get_line(infile,line); } }
			
			
	  else if (line.find("SEED") != string::npos) { get_line(infile,line);
		  while (line.find('#') == string::npos && !infile.eof()) {
			  if (line.length()>0)
			  {
				  int good = 1;
				  
				  istringstream iss(line); iss >> sub;
				  if (sub.find_first_not_of("1234567890-") != string::npos ) { good = 0; }
				  if (!iss.eof()) { good = 0; }
				  
				  if (good==0) { input_error(10,line); }
			  }
			  get_line(infile,line); } }
			
			
	  else if (line.find("PREDETERMINED MUTATIONS") != string::npos) { get_line(infile,line);
		  while (line.find('#') == string::npos && !infile.eof()) {
			  if (line.length()>0)
			  {
				  int good = 1;
				  
				  istringstream iss(line); iss >> sub; // time
				  if (sub.find_first_not_of("1234567890e") != string::npos ) { good = 0; }
				  if (iss.eof()) { good = 0; } iss >> sub; // id
				  if (sub.compare(0,1,"m") != 0) { good = 0; } sub.erase(0,1);
				  if (sub.find_first_not_of("1234567890") != string::npos ) { good = 0; }
				  if (iss.eof()) { good = 0; } iss >> sub; // x
				  if (sub.find_first_not_of("1234567890e") != string::npos ) { good = 0; }
				  if (iss.eof()) { good = 0; } iss >> sub; // sub
				  if (sub.compare(0,1,"p") != 0) { good = 0; } sub.erase(0,1);
				  if (sub.find_first_not_of("1234567890") != string::npos ) { good = 0; }
				  if (iss.eof()) { good = 0; } iss >> sub; // nAA
				  if (sub.find_first_not_of("1234567890") != string::npos ) { good = 0; }
				  if (iss.eof()) { good = 0; } iss >> sub; // nAa
				  if (sub.find_first_not_of("1234567890") != string::npos ) { good = 0; }
				  
				  if (!iss.eof())
		    {
				iss >> sub;
				if (sub.find_first_not_of("P") != string::npos ) { good = 0; }
				if (iss.eof()) { good = 0; } iss >> sub; // freq
				if (sub.find_first_not_of("1234567890.-e") != string::npos ) { good = 0; }
			}
				  
				  if (!iss.eof()) { good = 0; }
				  
				  if (good==0) { input_error(11,line); }
			  }
			  get_line(infile,line); } }
			
	  else { input_error(-1,line); }
		}
		else { get_line(infile,line); }
	}
	
	if (mutation_rate!=1) { input_error(1,string()); }
	if (mutation_types<1) { input_error(2,string()); }
	if (genomic_element_types<1) { input_error(3,string()); }
	if (chromosome_organization<1) { input_error(4,string()); }
	if (recombination_rate<1) { input_error(5,string()); }
	if (generations<1) { input_error(6,string()); }
	if (population<1) { input_error(-2,string()); }
};
