//
//  subpopulation.cpp
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


#include "subpopulation.h"


subpopulation::subpopulation(int n)
{
	N = n;
	S = 0.0;
	G_parent.resize(2*N); G_child.resize(2*N);
	double A[N]; for (int i=0; i<N; i++) { A[i] = 1.0; }
	LT = gsl_ran_discrete_preproc(N,A);
}


int subpopulation::draw_individual() { return gsl_ran_discrete(g_rng,LT); }


void subpopulation::update_fitness(chromosome& chr)
{
	// calculate fitnesses in parent population and create new lookup table
	
	gsl_ran_discrete_free(LT);
	double A[(int)(G_parent.size()/2)]; for (int i=0; i<(int)(G_parent.size()/2); i++) { A[i] = W(2*i,2*i+1,chr); }
	LT = gsl_ran_discrete_preproc((int)(G_parent.size()/2),A);
}


double subpopulation::W(int i, int j, chromosome& chr)
{
	// calculate the fitness of the individual constituted by genomes i and j in the parent population
	
	double w = 1.0;
	
	
	std::vector<mutation>::iterator pi = G_parent[i].begin();
	std::vector<mutation>::iterator pj = G_parent[j].begin();
	
	std::vector<mutation>::iterator pi_max = G_parent[i].end();
	std::vector<mutation>::iterator pj_max = G_parent[j].end();
	
	while (w>0 && (pi != pi_max || pj != pj_max))
	{
		// advance i while pi.x < pj.x
		
		while (pi != pi_max && (pj == pj_max || (*pi).x < (*pj).x))
		{
			if ((*pi).s != 0) { w = w*(1.0+chr.mutation_types.find((*pi).t)->second.h*(*pi).s); }
			pi++;
		}
		
		// advance j while pj.x < pi.x
		
		while (pj != pj_max && (pi == pi_max || (*pj).x < (*pi).x))
		{
			if ((*pj).s != 0) { w = w*(1.0+chr.mutation_types.find((*pj).t)->second.h*(*pj).s); }
			pj++;
		}
		
		// check for homozygotes and heterozygotes at x
		
		if (pi != pi_max && pj != pj_max && (*pj).x == (*pi).x)
		{
			int x = (*pi).x; 
			
			std::vector<mutation>::iterator pi_start = pi;
			
			// advance through pi
			
			while (pi != pi_max && (*pi).x == x)
			{
				if ((*pi).s != 0.0)
				{
					std::vector<mutation>::iterator temp_j = pj; 
					bool homo = 0;
					
					while (homo == 0 && temp_j != pj_max && (*temp_j).x == x)
					{
						if ((*pi).t == (*temp_j).t && (*pi).s == (*temp_j).s) 
						{ 
							w = w*(1.0+(*pi).s); homo = 1; 
						}
						temp_j++;
					}
					
					if (homo == 0) { w = w*(1.0+chr.mutation_types.find((*pi).t)->second.h*(*pi).s); }
				}
				pi++;
			}
			
			// advance through pj
			
			while (pj != pj_max && (*pj).x == x)
			{
				if ((*pj).s != 0.0)
				{
					std::vector<mutation>::iterator temp_i = pi_start; 
					bool homo = 0;
					
					while (homo == 0 && temp_i != pi_max && (*temp_i).x == x)
					{
						if ((*pj).t == (*temp_i).t && (*pj).s == (*temp_i).s) { homo = 1; }
						temp_i++;
					}
					if (homo == 0) { w = w*(1.0+chr.mutation_types.find((*pj).t)->second.h*(*pj).s); }
				}
				pj++;
			}
		}
	}
	
	if (w<0) { w = 0.0; }
	
	return w;
}

void subpopulation::swap()
{
	G_child.swap(G_parent);
	G_child.resize(2*N);
}

