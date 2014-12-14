//
//  main.cpp
//  SLiM
//
//  Created by Ben Haller on 12/12/14.
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


#include <map>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>

#include "input_parsing.h"
#include "mutation.h"
#include "population.h"
#include "chromosome.h"


using std::string;
using std::multimap;
using std::istringstream;
using std::endl;


void initialize_from_file(population& P, const char* file, chromosome& chr)
{
	// initialize population from file
	
	std::map<int,mutation> M;
	
	string line; 
	string sub; 
	
	std::ifstream infile (file);
	
	if (!infile.is_open()) { std::cerr << "ERROR (initialize): could not open initialization file" << endl; exit(1); }
	
	get_line(infile,line);
	
	while (line.find("Populations") == string::npos && !infile.eof()) { get_line(infile,line); } 
	
	get_line(infile,line);
	
	while (line.find("Mutations") == string::npos && !infile.eof())
	{ 
		istringstream iss(line); iss >> sub; sub.erase(0,1);  
		int i = atoi(sub.c_str()); iss >> sub;  
		int n = atoi(sub.c_str());
		P.add_subpopulation(i,n);
		get_line(infile,line);      
	}
	
	get_line(infile,line);
	
	while (line.find("Genomes") == string::npos && !infile.eof()) 
	{     
		istringstream iss(line); iss >> sub; 
		int   id = atoi(sub.c_str()); iss >> sub; sub.erase(0,1); 
		int   t  = atoi(sub.c_str()); iss >> sub; 
		int   x  = atoi(sub.c_str())-1; iss >> sub;
		float s  = atof(sub.c_str()); iss >> sub; iss >> sub; sub.erase(0,1);
		int   i  = atoi(sub.c_str()); iss >> sub;
		int   g  = atoi(sub.c_str());
		
		M.insert(std::pair<int,mutation>(id,mutation(t,x,s,i,g)));
		get_line(infile,line); 
	}
	
	get_line(infile,line);
	
	while (!infile.eof())
	{
		istringstream iss(line); iss >> sub; sub.erase(0,1);
		int pos = sub.find_first_of(":"); 
		int p = atoi(sub.substr(0,pos+1).c_str()); sub.erase(0,pos+1);  
		int i = atoi(sub.c_str());
		
		while (iss >> sub) 
		{ 
	  int id = atoi(sub.c_str());
	  P.find(p)->second.G_parent[i-1].push_back(M.find(id)->second);
		}
		get_line(infile,line);
	}
	
	for (P.it=P.begin(); P.it!=P.end(); P.it++) { P.it->second.update_fitness(chr); }
};


void initialize(population& P, char* file, chromosome& chr, int &t_start, int &t_duration, multimap<int,event>& E, multimap<int,event>& O, multimap<int,introduced_mutation>& IM, std::vector<partial_sweep>& PS, std::vector<string>& parameters)
{
	string line; 
	string sub; 
	
	std::ifstream infile (file);
	
	long pid=getpid();
	time_t *tp,t; tp=&t; time(tp); t+=pid;
	int seed=t;
	
	
	get_line(infile,line);
	
	while (!infile.eof())
	{
		if (line.find('#') != string::npos) 
		{
	  if (line.find("MUTATION RATE") != string::npos) { get_line(infile,line);
		  parameters.push_back("#MUTATION RATE");
		  while (line.find('#') == string::npos && !infile.eof()) {
			  if (line.length()>0)
			  {
				  parameters.push_back(line);
				  istringstream iss(line); iss >> sub;  chr.M = atof(sub.c_str());
			  }
			  get_line(infile,line); } }
			
	  else if (line.find("MUTATION TYPES") != string::npos) { get_line(infile,line);
		  parameters.push_back("#MUTATION TYPES");
		  while (line.find('#') == string::npos && !infile.eof()) {
			  if (line.length()>0)
			  {
				  parameters.push_back(line);
				  
				  // FORMAT: i h t p1 p2 ... (identifier, dominance coefficient DFE type, DFE parameters) 
				  
				  int i; float h; char t; std::vector<double> p; istringstream iss(line);
				  iss >> sub; sub.erase(0,1); i = atoi(sub.c_str());
				  
				  if (chr.mutation_types.count(i)>0) 
				  {  
					  std::cerr << "ERROR (initialize): mutation type "<< i << " already defined" << endl; exit(1);
				  }
				  
				  iss >> sub; h = atof(sub.c_str());
				  iss >> sub; t = sub.at(0);
				  while (iss >> sub) { p.push_back(atof(sub.c_str())); }
				  chr.mutation_types.insert(std::pair<int,mutation_type>(i,mutation_type(h,t,p)));
			  }
			  get_line(infile,line); } }
			
	  else if (line.find("GENOMIC ELEMENT TYPES") != string::npos) { get_line(infile,line);
		  parameters.push_back("#GENOMIC ELEMENT TYPES");
		  while (line.find('#') == string::npos && !infile.eof()) {
			  if (line.length()>0)
			  {
				  parameters.push_back(line);
				  
				  // FORMAT: i m1 g1 [m2 g2 ...] (identifier, mut type class, fraction)
				  
				  int i; std::vector<int> m; std::vector<double> g; istringstream iss(line);
				  iss >> sub; sub.erase(0,1); i = atoi(sub.c_str());
				  while (iss >> sub) 
				  { 
					  sub.erase(0,1);
					  m.push_back(atoi(sub.c_str())); iss >> sub;
					  g.push_back(atof(sub.c_str()));
				  }
				  
				  if (chr.genomic_element_types.count(i)>0) 
				  {  
					  std::cerr << "ERROR (initialize): genomic element type "<< i << " already defined" << endl; exit(1);
				  }
				  
				  chr.genomic_element_types.insert(std::pair<int,genomic_element_type>(i,genomic_element_type(m,g)));
			  }
			  get_line(infile,line); } }
	  
	  else if (line.find("CHROMOSOME ORGANIZATION") != string::npos) { get_line(infile,line);
		  parameters.push_back("#CHROMOSOME ORGANIZATION");
		  while (line.find('#') == string::npos && !infile.eof()) {
			  if (line.length()>0)
			  {
				  parameters.push_back(line);
				  
				  // FORMAT: i s e (genomic element type identifier, start, end)
				  
				  int i,s,e; istringstream iss(line);
				  iss >> sub; sub.erase(0,1); i = atoi(sub.c_str());
				  iss >> sub; s = (int)atof(sub.c_str())-1;
				  iss >> sub; e = (int)atof(sub.c_str())-1;
				  chr.push_back(genomic_element(i,s,e));
			  }
			  get_line(infile,line); } }
			
	  else if (line.find("RECOMBINATION RATE") != string::npos) { get_line(infile,line);
		  parameters.push_back("#RECOMBINATION RATE");
		  while (line.find('#') == string::npos && !infile.eof()) {
			  if (line.length()>0)
			  {
				  parameters.push_back(line);
				  
				  // FORMAT: x r (interval end, rec rate in events per bp)
				  
				  int x; double r; istringstream iss(line);
				  iss >> sub; x = (int)atof(sub.c_str())-1;
				  iss >> sub; r = atof(sub.c_str());
				  chr.rec_x.push_back(x); chr.rec_r.push_back(r);
			  }
			  get_line(infile,line); } }
			
	  else if (line.find("GENE CONVERSION") != string::npos) { get_line(infile,line);
		  parameters.push_back("#GENE CONVERSION");
		  while (line.find('#') == string::npos && !infile.eof()) {
			  if (line.length()>0)
			  {
				  parameters.push_back(line);
				  
				  // FORMAT: G_f G_l (gene conversion fraction, average stretch length in bp)
				  
				  istringstream iss(line);
				  iss >> sub; chr.G_f = atof(sub.c_str());
				  iss >> sub; chr.G_l = atof(sub.c_str());
			  }
			  get_line(infile,line); } }
			
	  else if (line.find("GENERATIONS") != string::npos) { get_line(infile,line);
		  parameters.push_back("#GENERATIONS");
		  while (line.find('#') == string::npos && !infile.eof()) {
			  if (line.length()>0)
			  {
				  parameters.push_back(line);
				  istringstream iss(line);
				  iss >> sub; t_duration = (int)atof(sub.c_str());
				  if (iss >> sub) { t_start = (int)atof(sub.c_str()); }
				  else           { t_start = 1; }
			  }
			  get_line(infile,line); } }
			
	  else if (line.find("DEMOGRAPHY AND STRUCTURE") != string::npos) { get_line(infile,line);
		  parameters.push_back("#DEMOGRAPHY AND STRUCTURE");
		  while (line.find('#') == string::npos && !infile.eof()) {
			  if (line.length()>0)
			  {
				  parameters.push_back(line);
				  
				  // FORMAT: t event_type event_parameters
				  
				  int t; char c; std::vector<string> s; istringstream iss(line); 
				  iss >> sub; t = (int)atof(sub.c_str());
				  iss >> sub; c = sub.at(0);
				  
				  while (iss >> sub) { s.push_back(sub.c_str()); }
				  E.insert(std::pair<int,event>(t,event(c,s)));
			  }
			  get_line(infile,line); } }
			
	  else if (line.find("OUTPUT") != string::npos) { get_line(infile,line); 
		  parameters.push_back("#OUTPUT");
		  while (line.find('#') == string::npos && !infile.eof()) {
			  if (line.length()>0)
			  {
				  parameters.push_back(line);
				  
				  // FORMAT: t event_type event_paramaters
				  
				  int t; char c; std::vector<string> s; istringstream iss(line); 
				  iss >> sub; t = (int)atof(sub.c_str());
				  iss >> sub; c = sub.at(0);
				  
				  while (iss >> sub) { s.push_back(sub.c_str()); }
				  O.insert(std::pair<int,event>(t,event(c,s)));
			  }
			  get_line(infile,line); } }
			
	  else if (line.find("PREDETERMINED MUTATIONS") != string::npos) { get_line(infile,line);
		  parameters.push_back("#PREDETERMINED MUTATIONS");
		  while (line.find('#') == string::npos && !infile.eof()) {
			  if (line.length()>0)
			  {
				  parameters.push_back(line);
				  
				  // FORMAT: t type x i nAA nAa [P <freq>]
				  
				  istringstream iss(line); 
				  
				  iss >> sub; int time = (int)atof(sub.c_str());
				  iss >> sub; sub.erase(0,1); int t = atoi(sub.c_str());
				  iss >> sub; int x    = (int)atof(sub.c_str())-1;
				  iss >> sub; sub.erase(0,1); int i = atoi(sub.c_str());
				  iss >> sub; int nAA  = (int)atof(sub.c_str());
				  iss >> sub; int nAa  = (int)atof(sub.c_str());
				  
				  introduced_mutation M(t,x,i,time,nAA,nAa);
				  
				  IM.insert(std::pair<int,introduced_mutation>(time,M));
				  
				  while (iss >> sub) 
				  { 
					  if (sub.find('P')!=string::npos) 
			    {
					iss >> sub; float p = atof(sub.c_str());
					PS.push_back(partial_sweep(t,x,p));
				}
				  }
			  }
			  get_line(infile,line); } }
			
	  else if (line.find("SEED") != string::npos) { get_line(infile,line);
		  while (line.find('#') == string::npos && !infile.eof()) {
			  if (line.length()>0)
			  {
				  istringstream iss(line); iss >> sub;  seed = atoi(sub.c_str());
			  }
			  get_line(infile,line); } }
			
	  else if (line.find("INITIALIZATION") != string::npos) { get_line(infile,line);
		  parameters.push_back("#INITIALIZATION");
		  while (line.find('#') == string::npos && !infile.eof()) {
			  if (line.length()>0)
			  {
				  parameters.push_back(line);
				  
				  istringstream iss(line); iss >> sub;  
				  initialize_from_file(P,sub.c_str(),chr);
			  }
			  get_line(infile,line); } }	  
		}
		else { get_line(infile,line); }
	}
	
	// initialize chromosome
	
	chr.initialize_rng();
	
	// initialize rng
	
	g_rng=gsl_rng_alloc(gsl_rng_taus2);
	gsl_rng_set(g_rng,(long)seed); 
	
	parameters.push_back("#SEED");
	std::stringstream ss; ss << seed;
	parameters.push_back(ss.str());
	
	// parameter output
	
	for (int i=0; i<P.parameters.size(); i++) { std::cout << parameters[i] << endl; }
}


int main(int argc,char *argv[])
{
	// initialize simulation parameters
	
	if (argc<=1) { std::cerr << "usage: slim <parameter file>" << endl; exit(1); } 
	
	char* input_file = argv[1];
	check_input_file(input_file);
	
	int t_start;
	int t_duration;
	chromosome chr;
	
	population P; std::map<int,subpopulation>::iterator itP;
	
	P.parameters.push_back("#INPUT PARAMETER FILE");
	P.parameters.push_back(input_file);
	
	// demographic and structure events
	
	multimap<int,event> E; 
	multimap<int,event>::iterator itE;
	
	// output events (time, output)
	
	multimap<int,event> O; 
	multimap<int,event>::iterator itO;
	
	// user-defined mutations that will be introduced (time, mutation)
	
	multimap<int,introduced_mutation> IM; 
	multimap<int,introduced_mutation>::iterator itIM;
	
	// tracked mutation-types
	
	std::vector<int> TM; 
	
	// mutations undergoing partial sweeps
	
	std::vector<partial_sweep> PS;
	
	initialize(P,input_file,chr,t_start,t_duration,E,O,IM,PS,P.parameters);
	
	std::cout << t_start << " " << t_duration << endl;
	
	// evolve over t generations
	
	for (int g=t_start; g<(t_start+t_duration); g++)
	{ 
		// execute demographic and substructure events in this generation 
		
		std::pair<multimap<int,event>::iterator,multimap<int,event>::iterator> rangeE = E.equal_range(g);
		for (itE = rangeE.first; itE != rangeE.second; itE++) { P.execute_event(itE->second,g,chr,TM); }
		
		// evolve all subpopulations
		
		for (itP = P.begin(); itP != P.end(); itP++) { P.evolve_subpopulation(itP->first,chr,g); }     
		
		// introduce user-defined mutations
		
		std::pair<multimap<int,introduced_mutation>::iterator,multimap<int,introduced_mutation>::iterator> rangeIM = IM.equal_range(g);
		for (itIM = rangeIM.first; itIM != rangeIM.second; itIM++) { P.introduce_mutation(itIM->second,chr); }
		
		// execute output events
		
		std::pair<multimap<int,event>::iterator,multimap<int,event>::iterator> rangeO = O.equal_range(g);
		for (itO = rangeO.first; itO != rangeO.second; itO++) { P.execute_event(itO->second,g,chr,TM); }
		
		// track particular mutation-types and set s=0 for partial sweeps when completed
		
		if (TM.size()>0 || PS.size()>0) { P.track_mutations(g,TM,PS,chr); }
		
		// swap generations
		
		P.swap_generations(g,chr);   
	}
	
	return 0;
}
