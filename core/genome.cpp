//
//  genome.cpp
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


#include "genome.h"

// FIXME name conflict with std::fixed, would be wise to change names
genome fixed(genome& G1, genome& G2)
{
	// return genome G consisting only of the mutations that are present in both G1 and G2
	
	genome G;
	
	std::vector<mutation>::iterator g1 = G1.begin();
	std::vector<mutation>::iterator g2 = G2.begin();
	
	std::vector<mutation>::iterator g1_max = G1.end();
	std::vector<mutation>::iterator g2_max = G2.end();
	
	while (g1 != g1_max && g2 != g2_max)
	{
		// advance g1 while g1.x < g2.x
		
		while (g1 != g1_max && g2 != g2_max && (*g1).x < (*g2).x) { g1++; }
		
		// advance g2 while g1.x < g2.x
		
		while (g1 != g1_max && g2 != g2_max && (*g2).x < (*g1).x) { g2++; }
		
		// identify shared mutations at positions x and add to G
		
		if (g2 != g2_max && g1 != g1_max && (*g2).x == (*g1).x)
		{
			int x = (*g1).x;
			
			std::vector<mutation>::iterator temp;
			
			while (g1 != g1_max && (*g1).x == x)
			{
				temp = g2;
				while (temp != g2_max && (*temp).x == x)
		  {
			  if ((*temp).t==(*g1).t && (*temp).s==(*g1).s) { G.push_back(*g1); }
			  temp++;
		  }
				g1++;
			}
		}
	}
	
	return G;
}


genome polymorphic(genome& G1, genome& G2)
{
	// return genome G consisting only of the mutations in G1 that are not in G2
	
	genome G;
	
	std::vector<mutation>::iterator g1 = G1.begin();
	std::vector<mutation>::iterator g2 = G2.begin();
	
	std::vector<mutation>::iterator g1_max = G1.end();
	std::vector<mutation>::iterator g2_max = G2.end();
	
	while (g1 != g1_max && g2 != g2_max)
	{
		// advance g1 while g1.x < g2.x
		
		while (g1 != g1_max && g2 != g2_max && (*g1).x < (*g2).x) { G.push_back(*g1); g1++; }
		
		// advance g2 while g1.x < g2.x
		
		while (g2 != g2_max && g1 != g1_max && (*g2).x < (*g1).x) { g2++; }
		
		// identify polymorphic mutations at positions x and add to G
		
		if (g2 != g2_max && g1 != g1_max && (*g2).x == (*g1).x)
		{
			int x = (*g1).x;
			
			// go through g1 and check for those mutations that are not present in g2
			
			std::vector<mutation>::iterator temp = g2;
			
			while (g1 != g1_max && (*g1).x == x)
			{
				bool poly = 1;
				
				while (temp != g2_max && (*temp).x == x)
		  {
			  if ((*g1).t==(*temp).t && (*g1).s==(*temp).s) { poly = 0; }
			  temp++;
		  }
				if (poly == 1) { G.push_back(*g1); }
				g1++;
			}
			
			while (g2 != g2_max && (*g2).x == x) { g2++; }
		}
	}
	
	while (g1 != g1_max) { G.push_back(*g1); g1++; }
	
	return G;
}
