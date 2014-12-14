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


using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::multimap;


void population::add_subpopulation(int i, unsigned int N) 
{ 
	// add new empty subpopulation i of size N
	
	if (count(i)!=0) { cerr << "ERROR (add subpopulation): subpopulation p"<< i << " already exists" << endl; exit(1); }
	if (N<1)         { cerr << "ERROR (add subpopulation): subpopulation p"<< i << " empty" << endl; exit(1); }
	
	insert(std::pair<int,subpopulation>(i,subpopulation(N))); 
}


void population::add_subpopulation(int i, int j, unsigned int N) 
{ 
	// add new subpopulation i of size N individuals drawn from source subpopulation j
	
	if (count(i)!=0) { cerr << "ERROR (add subpopulation): subpopulation p"<< i << " already exists" << endl; exit(1); }
	if (count(j)==0) { cerr << "ERROR (add subpopulation): source subpopulation p"<< j << " does not exists" << endl; exit(1); }
	if (N<1)         { cerr << "ERROR (add subpopulation): subpopulation p"<< i << " empty" << endl; exit(1); }
	
	insert(std::pair<int,subpopulation>(i,subpopulation(N))); 
	
	for (int p=0; p<find(i)->second.N; p++)
	{
		// draw individual from subpopulation j and assign to be a parent in i  
		
		int m = find(j)->second.draw_individual();
		
		find(i)->second.G_parent[2*p] = find(j)->second.G_parent[2*m];
		find(i)->second.G_parent[2*p+1] = find(j)->second.G_parent[2*m+1];
	}
}


void population::set_size(int i, unsigned int N) 
{
	// set size of subpopulation i to N
	
	if (count(i)==0) { cerr << "ERROR (change size): no subpopulation p"<< i << endl; exit(1); }
	
	if (N==0) // remove subpopulation i 
	{
		erase(i); 
		for (it = begin(); it != end(); it++) { it->second.m.erase(i); } 
	}
	else { find(i)->second.N = N; find(i)->second.G_child.resize(2*N); }
}

void population::set_selfing(int i, double s) 
{ 
	// set fraction s of i that reproduces by selfing
	
	if (count(i)==0)    { cerr << "ERROR (set selfing): no subpopulation p"<< i << endl; exit(1); }
	if (s<0.0 || s>1.0) { cerr << "ERROR (set selfing): selfing fraction has to be within [0,1]" << endl; exit(1); }
	
	find(i)->second.S = s; 
}


void population::set_migration(int i, int j, double m) 
{ 
	// set fraction m of i that originates as migrants from j per generation  
	
	if (count(i)==0)    { cerr << "ERROR (set migration): no subpopulation p"<< i << endl; exit(1); }
	if (count(j)==0)    { cerr << "ERROR (set migration): no subpopulation p"<< j << endl; exit(1); }
	if (m<0.0 || m>1.0) { cerr << "ERROR (set migration): migration fraction has to be within [0,1]" << endl; exit(1); }
	
	if (find(i)->second.m.count(j) !=0 ) { find(i)->second.m.erase(j); }
	
	find(i)->second.m.insert(std::pair<int,double>(j,m)); 
}


void population::execute_event(event& E, int g, chromosome& chr, std::vector<int>& FM)
{
	char type = E.t;
	
	if (type == 'P') // add subpopulation
	{ 
		if (E.np==2) // empty subpopulation
		{ 
			string sub = E.s[0]; sub.erase(0,1);
			
			int i = atoi(sub.c_str());
			int n = (int)atof(E.s[1].c_str());
			add_subpopulation(i,n);
		}
		
		if (E.np==3) // drawn from source population
		{
			string sub1 = E.s[0]; sub1.erase(0,1);
			string sub2 = E.s[2]; sub2.erase(0,1);
			
			int i = atoi(sub1.c_str());
			int j = atoi(sub2.c_str());
			int n = (int)atof(E.s[1].c_str());
			add_subpopulation(i,j,n);
		} 
	}
	
	if (type == 'N') // set subpopulation size
	{ 
		string sub = E.s[0]; sub.erase(0,1);
		
		int i = atoi(sub.c_str());
		int n = (int)atof(E.s[1].c_str());
		
		set_size(i,n);
	}
	
	if (type == 'S') // set selfing rate
	{ 
		string sub = E.s[0]; sub.erase(0,1);
		
		int i    = atoi(sub.c_str());
		double s = atof(E.s[1].c_str());
		
		set_selfing(i,s);
	}
	
	if (type == 'M') // change migration rate
	{
		string sub1 = E.s[0]; sub1.erase(0,1);
		string sub2 = E.s[1]; sub2.erase(0,1);
		
		int    i = atoi(sub1.c_str());
		int    j = atoi(sub2.c_str());
		double m = atof(E.s[2].c_str());
		
		set_migration(i,j,m); 
	}
	
	if (type == 'A') // output state of entire population
	{
		if (E.s.size()==0) 
		{
			cout << "#OUT: " << g << " A" << endl;
			print_all(chr); 
		}	
		if (E.s.size()==1)
		{
			std::ofstream outfile;
			outfile.open (E.s[0].c_str());
			
			for (int i=0; i<parameters.size(); i++) { outfile << parameters[i] << endl; }
			
			if (outfile.is_open()) 
			{ 
				outfile << "#OUT: " << g << " A " << E.s[0].c_str() << endl;
				print_all(outfile,chr);
				outfile.close(); 
			}
			else { cerr << "ERROR (output): could not open "<< E.s[0].c_str() << endl; exit(1); }
		}
	}
	
	if (type == 'R') // output random subpopulation sample
	{
		string sub = E.s[0]; sub.erase(0,1);
		
		int    i = atoi(sub.c_str());
		int    n = atoi(E.s[1].c_str());   
		cout << "#OUT: " << g << " R p" << i << " " << n << endl;
		
		if (E.s.size() == 3 && E.s[2] == "MS") { print_sample_ms(i,n,chr); }
		else { print_sample(i,n,chr); }
	}
	
	if (type == 'F') // output list of fixed mutations
	{
		cout << "#OUT: " << g << " F " << endl;
		cout << "Mutations:" << endl;
		for (int i=0; i<Substitutions.size(); i++) { cout << i+1; Substitutions[i].print(chr); }
	}
	
	if (type == 'T') // track mutation-types
	{
		string sub = E.s[0]; sub.erase(0,1);
		FM.push_back(atoi(sub.c_str()));
	}
}


void population::introduce_mutation(introduced_mutation M, chromosome& chr) 
{
	// introduce user-defined mutation
	
	if (count(M.i)==0) { cerr << "ERROR (predetermined mutation): subpopulation "<< M.i << " does not exists" << endl; exit(1); }
	if (chr.mutation_types.count(M.t) == 0) 
	{ 
		cerr << "ERROR (predetermined mutation): mutation type m"<< M.t << " has not been defined" << endl; exit(1); 
	}
	if (find(M.i)->second.G_child.size()/2 < M.nAA + M.nAa) 
	{ 
		cerr << "ERROR (predetermined mutation): not enough individuals in subpopulation "<< M.i << endl; exit(1); 
	}
	
	mutation m;
	
	m.x = M.x;
	m.t = M.t;
	m.s = chr.mutation_types.find(M.t)->second.draw_s();
	m.i = M.i;
	m.g = M.g;
	
	// introduce homozygotes
	
	for (int j=0; j<M.nAA; j++) 
	{ 
		genome *g1 = &find(M.i)->second.G_child[2*j];
		genome *g2 = &find(M.i)->second.G_child[2*j+1];
		(*g1).push_back(m);
		(*g2).push_back(m);
		sort((*g1).begin(),(*g1).end());
		sort((*g2).begin(),(*g2).end());
		(*g1).erase(unique((*g1).begin(),(*g1).end()),(*g1).end());
		(*g2).erase(unique((*g2).begin(),(*g2).end()),(*g2).end());
	}
	
	// introduce heterozygotes
	
	for (int j=M.nAA; j<M.nAA+M.nAa; j++) 
	{ 
		genome *g1 = &find(M.i)->second.G_child[2*j];
		(*g1).push_back(m);
		sort((*g1).begin(),(*g1).end());
		(*g1).erase(unique((*g1).begin(),(*g1).end()),(*g1).end());
	}
}


void population::track_mutations(int g, std::vector<int>& TM, std::vector<partial_sweep>& PS, chromosome& chr)
{
	// output trajectories of followed mutations and set s=0 for partial sweeps 
	
	// find all polymorphism of the types that are to be tracked
	
	for (it = begin(); it != end(); it++) // go through all subpopulations
	{
		multimap<int,polymorphism> P;
		multimap<int,polymorphism>::iterator P_it;
		
		for (int i=0; i<2*it->second.N; i++) // go through all children
		{
			for (int k=0; k<it->second.G_child[i].size(); k++) // go through all mutations
			{
				for (int j=0; j<TM.size(); j++)
				{
					if (it->second.G_child[i][k].t == TM[j]) { add_mut(P,it->second.G_child[i][k]); }
				}
			}
		}
		
		// out put the frequencies of these mutations in each subpopulation
		
		for (P_it = P.begin(); P_it != P.end(); P_it++) 
		{ 
			cout << "#OUT: " << g << " T p" << it->first << " "; P_it->second.print_no_id(P_it->first,chr); 
		}
	}
	
	// check partial sweeps
	
	multimap<int,polymorphism> P;
	multimap<int,polymorphism>::iterator P_it;
	
	if (PS.size()>0)
	{
		P.clear();
		
		int N = 0; for (it = begin(); it != end(); it++) { N += it->second.N; }
		
		// find all polymorphism that are supposed to undergo partial sweeps
		
		for (it = begin(); it != end(); it++) // go through all subpopulations
		{
			for (int i=0; i<2*it->second.N; i++) // go through all children
			{
				for (int k=0; k<it->second.G_child[i].size(); k++) // go through all mutations
				{
					for (int j=0; j<PS.size(); j++)
					{
						if (it->second.G_child[i][k].x == PS[j].x && it->second.G_child[i][k].t == PS[j].t) 
						{
							add_mut(P,it->second.G_child[i][k]); 
						}
					}
				}
			}
		}
		
		// check whether a partial sweep has reached its target frequency
		
		for (P_it = P.begin(); P_it != P.end(); P_it++) 
		{ 
			for (int j=0; j<PS.size(); j++)
			{
				if (P_it->first == PS[j].x && P_it->second.t == PS[j].t)
				{
					if (((float)P_it->second.n)/(2*N) >= PS[j].p)
					{
						// sweep has reached target frequency, set all s to zero
						
						for (it = begin(); it != end(); it++) // go through all subpopulations
						{
							for (int i=0; i<2*it->second.N; i++) // go through all children
							{
								for (int k=0; k<it->second.G_child[i].size(); k++) // go through all mutations
								{
									if (it->second.G_child[i][k].x == PS[j].x && it->second.G_child[i][k].t == PS[j].t)
									{
										it->second.G_child[i][k].s = 0.0;
									}
								}
							}
						}
						PS.erase(PS.begin()+j); j--;
					}
				}
			}
		}
	}
}


void population::evolve_subpopulation(int i, chromosome& chr, int g)
{
	int g1,g2,p1,p2; // n_mut_1, n_mut_2 were declared here but are unused.  BCH 14 Dec 2014
	
	
	// create map of shuffled children ids
	
	int child_map[find(i)->second.N];          
	for (int j = 0; j < find(i)->second.N; j++) { child_map[j] = j; }
	gsl_ran_shuffle(g_rng,child_map,find(i)->second.N,sizeof(int));
	
	int child_count = 0; // counter over all N children (will get mapped to child_map[child_count])
	
	
	// draw number of migrant individuals
	
	map<int,double>::iterator it;
	
	double m_rates[find(i)->second.m.size()+1];
	unsigned int n_migrants[find(i)->second.m.size()+1];
	
	double m_sum = 0.0; int pop_count = 0;
	
	for (map<int,double>::iterator it = find(i)->second.m.begin(); it != find(i)->second.m.end(); it++)
	{
		m_rates[pop_count] = it->second;
		m_sum += it->second;
		pop_count++;
	}
	
	if (m_sum<=1.0) { m_rates[pop_count] = 1.0-m_sum; }
	else { cerr << "ERROR (evolve subpopulation): too many migrants in subpopulation "<< i << endl; exit(1); }
	
	gsl_ran_multinomial(g_rng, find(i)->second.m.size()+1,find(i)->second.N,m_rates,n_migrants);
	
	// loop over all migration source populations
	
	pop_count = 0;
	
	for (map<int,double>::iterator it = find(i)->second.m.begin(); it != find(i)->second.m.end(); it++)
	{
		int migrant_count = 0;
		
		while (migrant_count<n_migrants[pop_count])
		{
			g1 = 2*child_map[child_count];   // child genome 1
			g2 = 2*child_map[child_count]+1; // child genome 2
			
			// draw parents in source population
			
			p1 = gsl_rng_uniform_int(g_rng,find(it->first)->second.G_parent.size()/2);
			if (gsl_rng_uniform(g_rng) < find(it->first)->second.S) { p2 = p1; }
			else { p2 = gsl_rng_uniform_int(g_rng,find(it->first)->second.G_parent.size()/2); }
			
			// recombination, gene-conversion, mutation
			
			crossover_mutation(i,g1,it->first,2*p1,2*p1+1,chr,g);
			crossover_mutation(i,g2,it->first,2*p2,2*p2+1,chr,g);
			
			migrant_count++;
			child_count++;
		}
		pop_count++;
	}
	
	// remainder
	
	while (child_count<find(i)->second.N) 
	{
		g1 = 2*child_map[child_count];   // child genome 1
		g2 = 2*child_map[child_count]+1; // child genome 2
		
		p1 = find(i)->second.draw_individual();                   // parent 1
		if (gsl_rng_uniform(g_rng) < find(i)->second.S) { p2 = p1; } // parent 2
		else { p2 = find(i)->second.draw_individual(); }
		
		crossover_mutation(i,g1,i,2*p1,2*p1+1,chr,g);
		crossover_mutation(i,g2,i,2*p2,2*p2+1,chr,g);
		
		child_count++;
	}
}


void population::crossover_mutation(int i, int c, int j, int P1, int P2, chromosome& chr, int g)
{
	// child genome c in subpopulation i is assigned outcome of cross-overs at breakpoints r 
	// between parent genomes p1 and p2 from subpopulation j and new mutations added
	// 
	// example R = (r1,r2)
	// 
	// mutations (      x < r1) assigned from p1
	// mutations (r1 <= x < r2) assigned from p2
	// mutations (r2 <= x     ) assigned from p1
	//
	// p1 and p2 are swapped in half of the cases to assure random assortement
	
	if (gsl_rng_uniform_int(g_rng,2)==0) { int swap = P1; P1 = P2; P2 = swap; } // swap p1 and p2
	
	find(i)->second.G_child[c].clear();
	
	// create vector with the mutations to be added
	
	std::vector<mutation> M;
	int n_mut = chr.draw_n_mut();
	for (int k=0; k<n_mut; k++) { M.push_back(chr.draw_new_mut(j,g)); }
	sort(M.begin(),M.end());
	
	// create vector with recombination breakpoints
	
	std::vector<int> R = chr.draw_breakpoints(); 
	R.push_back(chr.L+1);
	sort(R.begin(),R.end());
	R.erase(unique(R.begin(),R.end()),R.end());
	
	std::vector<mutation>::iterator p1 = find(j)->second.G_parent[P1].begin();
	std::vector<mutation>::iterator p2 = find(j)->second.G_parent[P2].begin();
	
	std::vector<mutation>::iterator p1_max = find(j)->second.G_parent[P1].end();
	std::vector<mutation>::iterator p2_max = find(j)->second.G_parent[P2].end();
	
	std::vector<mutation>::iterator m     = M.begin();
	std::vector<mutation>::iterator m_max = M.end();
	
	std::vector<mutation>::iterator p     = p1;
	std::vector<mutation>::iterator p_max = p1_max;
	
	int r = 0; int r_max  = R.size(); int n = 0; bool present;
	
	while (r != r_max)
	{
		while ((p != p_max && (*p).x < R[r]) || (m != m_max && (*m).x < R[r]))
		{
			while (p != p_max && (*p).x < R[r] && (m == m_max || (*p).x <= (*m).x))
			{
				present = 0;
				if (n != 0 && find(i)->second.G_child[c].back().x == (*p).x)
				{
					int k = n-1;
					while (present == 0 && k >= 0)
					{
						if (find(i)->second.G_child[c][k] == (*p)) { present = 1; }
						k--;
					}
				}
				if (present == 0) { find(i)->second.G_child[c].push_back(*p); n++; }
				p++;
			}
			while (m != m_max && (*m).x < R[r] && (p == p_max || (*m).x <= (*p).x))
			{
				present = 0;
				if (n != 0 && find(i)->second.G_child[c].back().x == (*m).x)
				{
					int k = n-1;
					while (present == 0 && k >= 0)
					{
						if (find(i)->second.G_child[c][k] == (*m)) { present = 1; }
						k--;
					}
				}
				if (present == 0) { find(i)->second.G_child[c].push_back(*m); n++; }
				m++;
			}
		}
		
		// swap parents
		
		p1 = p2; p1_max = p2_max; p2 = p; p2_max = p_max; p = p1; p_max = p1_max; 
		
		while (p != p_max && (*p).x < R[r]) { p++; }
		
		r++;
	}
}


void population::swap_generations(int g, chromosome& chr)
{
	// find and remove fixed mutations from the children in all subpopulations
	
	remove_fixed(g);
	
	// make children the new parents and update fitnesses
	
	for (it = begin(); it != end(); it++) 
	{ 
		it->second.swap();
		it->second.update_fitness(chr); 
	}
}


void population::remove_fixed(int g)
{
	// find mutations that are fixed in all child subpopulations and return vector with their ids
	
	genome G = begin()->second.G_child[0];
	
	for (it = begin(); it != end(); it++) // subpopulations
	{
		for (int i=0; i<2*it->second.N; i++) // child genomes
		{
			G = fixed(it->second.G_child[i],G);
		}
	}
	
	if (G.size()>0)
	{
		for (it = begin(); it != end(); it++) // subpopulations
		{
			for (int i=0; i<2*it->second.N; i++) // child genomes
			{
				it->second.G_child[i] = polymorphic(it->second.G_child[i],G);
			}
		}
		for (int i=0; i<G.size(); i++) { Substitutions.push_back(substitution(G[i],g)); } 
	}
}


void population::print_all(chromosome& chr)
{
	// print all mutations and all genomes 
	
	cout << "Populations:" << endl;
	for (it = begin(); it != end(); it++) {  cout << "p" << it->first << " " << it->second.N << endl; }
	
	multimap<int,polymorphism> P;
	multimap<int,polymorphism>::iterator P_it;
	
	// add all polymorphisms
	
	for (it = begin(); it != end(); it++) // go through all subpopulations
	{
		for (int i=0; i<2*it->second.N; i++) // go through all children
		{
			for (int k=0; k<it->second.G_child[i].size(); k++) // go through all mutations
			{
				add_mut(P,it->second.G_child[i][k]);
			}
		}
	}
	
	cout << "Mutations:"  << endl;
	
	for (P_it = P.begin(); P_it != P.end(); P_it++) { P_it->second.print(P_it->first,chr); }
	
	cout << "Genomes:" << endl;
	
	// print all genomes
	
	for (it = begin(); it != end(); it++) // go through all subpopulations
	{
		for (int i=0; i<2*it->second.N; i++) // go through all children
		{
			cout << "p" << it->first << ":" << i+1;
			
			for (int k=0; k<it->second.G_child[i].size(); k++) // go through all mutations
			{
				int id = find_mut(P,it->second.G_child[i][k]);
				cout << " " << id; 
			}
			cout << endl;
		}
	}
	
}


void population::print_all(std::ofstream& outfile, chromosome& chr)
{
	// print all mutations and all genomes 
	
	outfile << "Populations:" << endl;
	for (it = begin(); it != end(); it++) {  outfile << "p" << it->first << " " << it->second.N << endl; }
	
	multimap<int,polymorphism> P;
	multimap<int,polymorphism>::iterator P_it;
	
	// add all polymorphisms
	
	for (it = begin(); it != end(); it++) // go through all subpopulations
	{
		for (int i=0; i<2*it->second.N; i++) // go through all children
		{
			for (int k=0; k<it->second.G_child[i].size(); k++) // go through all mutations
			{
				add_mut(P,it->second.G_child[i][k]);
			}
		}
	}
	
	outfile << "Mutations:"  << endl;
	
	for (P_it = P.begin(); P_it != P.end(); P_it++) { P_it->second.print(outfile,P_it->first,chr); }
	
	outfile << "Genomes:" << endl;
	
	// print all genomes
	
	for (it = begin(); it != end(); it++) // go through all subpopulations
	{
		for (int i=0; i<2*it->second.N; i++) // go through all children
		{
			outfile << "p" << it->first << ":" << i+1;
			
			for (int k=0; k<it->second.G_child[i].size(); k++) // go through all mutations
			{
				int id = find_mut(P,it->second.G_child[i][k]);
				outfile << " " << id; 
			}
			outfile << endl;
		}
	}
	
}


void population::print_sample(int i, int n, chromosome& chr)
{
	// print sample of n genomes from subpopulation  i
	
	if (count(i)==0) { cerr << "ERROR (output): subpopulation p"<< i << " does not exists" << endl; exit(1); }
	
	std::vector<int> sample; 
	
	multimap<int,polymorphism> P;
	multimap<int,polymorphism>::iterator P_it;
	
	for (int s=0; s<n; s++) 
	{ 
		int j = gsl_rng_uniform_int(g_rng,find(i)->second.G_child.size());
		sample.push_back(j);
		
		for (int k=0; k<find(i)->second.G_child[j].size(); k++) // go through all mutations
		{
			add_mut(P,find(i)->second.G_child[j][k]);
		}
	}
	
	cout << "Mutations:"  << endl;
	
	for (P_it = P.begin(); P_it != P.end(); P_it++) { P_it->second.print(P_it->first,chr); }
	
	cout << "Genomes:" << endl;
	
	// print all genomes
	
	for (int j=0; j<sample.size(); j++) // go through all children
	{
		cout << "p" << find(i)->first << ":" << sample[j]+1;
		
		for (int k=0; k<find(i)->second.G_child[sample[j]].size(); k++) // go through all mutations
		{
			int id = find_mut(P,find(i)->second.G_child[sample[j]][k]);
			cout << " " << id; 
		}
		cout << endl;
	}
}


void population::print_sample_ms(int i, int n, chromosome& chr)
{
	// print sample of n genomes from subpopulation  i
	
	if (count(i)==0) { cerr << "ERROR (output): subpopulation p"<< i << " does not exists" << endl; exit(1); }
	
	std::vector<int> sample; 
	
	multimap<int,polymorphism> P;
	multimap<int,polymorphism>::iterator P_it;
	
	for (int s=0; s<n; s++) 
	{ 
		int j = gsl_rng_uniform_int(g_rng,find(i)->second.G_child.size());
		sample.push_back(j);
		
		for (int k=0; k<find(i)->second.G_child[j].size(); k++) // go through all mutations
		{
			add_mut(P,find(i)->second.G_child[j][k]);
		}
	}
	
	// print header
	
	cout << endl << "//" << endl << "segsites: " << P.size() << endl;
	
	// print all positions
	
	if (P.size()>0)
	{
		cout << "positions:"; 
		for (P_it = P.begin(); P_it != P.end(); P_it++) 
		{ 
			cout << " " << std::fixed << std::setprecision(7) << (double)(P_it->first+1)/(chr.L+1); 
		}
		cout << endl;
	}
	
	// print genotypes
	
	for (int j=0; j<sample.size(); j++) // go through all children
	{
		string genotype(P.size(),'0');
		
		for (int k=0; k<find(i)->second.G_child[sample[j]].size(); k++) // go through all mutations
		{
			int pos = 0;
			mutation m = find(i)->second.G_child[sample[j]][k];
			
			for (P_it = P.begin(); P_it != P.end(); P_it++) 
			{
				if (P_it->first == m.x && P_it->second.t == m.t && P_it->second.s == m.s)
				{
					genotype.replace(pos,1,"1");
					break;
				}
				pos++;
			}
		}
		cout << genotype << endl;
	}
}


int population::find_mut(multimap<int,polymorphism>& P, mutation m)
{
	// find m in P and return its id
	
	int id = 0;
	
	// iterate through all mutations with same position
	
	multimap<int,polymorphism>::iterator it;
	std::pair<multimap<int,polymorphism>::iterator,multimap<int,polymorphism>::iterator> range = P.equal_range(m.x);
	it = range.first;
	
	while (it != range.second)
	{
		if (it->second.t == m.t && it->second.s == m.s) 
		{ 
			id = it->second.id;
			it = range.second;
		}
		else{ it++; }
	}
	
	return id;
}


void population::add_mut(multimap<int,polymorphism>& P, mutation m)
{
	// if mutation is present in P increase prevalence, otherwise add it
	
	int id = 0;
	
	// iterate through all mutations with same position
	
	multimap<int,polymorphism>::iterator it;
	std::pair<multimap<int,polymorphism>::iterator,multimap<int,polymorphism>::iterator> range = P.equal_range(m.x);
	it = range.first;
	
	while (it != range.second)
	{
		if (it->second.t == m.t && it->second.s == m.s) 
		{ 
			id = it->second.id;
			it->second.n++;
			it = range.second;
		}
		else{ it++; }
	}
	
	// if not already present, add mutation to P
	
	if (id == 0)
	{
		id = P.size()+1;
		P.insert(std::pair<int,polymorphism>(m.x,polymorphism(id,m.t,m.s,m.i,m.g,1)));
	}
}

