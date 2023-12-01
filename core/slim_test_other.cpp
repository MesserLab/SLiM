//
//  slim_test_other.cpp
//  SLiM
//
//  Created by Ben Haller on 7/11/20.
//  Copyright (c) 2020-2023 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/slim/
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


#include "slim_test.h"

#include "eidos_globals.h"

#include <string>


#pragma mark InteractionType tests
static void _RunInteractionTypeTests_Nonspatial(bool p_sex_enabled, const std::string &p_sex_segregation);
static void _RunInteractionTypeTests_Spatial(const std::string &p_max_distance, bool p_sex_enabled, const std::string &p_sex_segregation);
static void _RunInteractionTypeTests_LocalPopDensity(void);

void _RunInteractionTypeTests(void)
{
	// ************************************************************************************
	//
	//	Gen 1+ tests: InteractionType
	//
	
	// The goal here is to get good code coverage in interaction_type.cpp; with code of this complexity it's extremely difficult
	// to comprehensively test the actual functionality across all cases and code paths, but at least we can try to execute all
	// the major code paths and make sure we don't crash or anything.
	
	// Test InteractionType properties
	SLiMAssertScriptStop(gen1_setup_i1x + "1 early() { if (i1.id == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 early() { if (isInfinite(i1.maxDistance)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 early() { if (i1.reciprocal == F) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 early() { if (i1.sexSegregation == '**') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 early() { if (i1.spatiality == 'x') stop(); }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 early() { i1.id = 2; }", "read-only property", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 early() { i1.maxDistance = 0.5; if (i1.maxDistance == 0.5) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 early() { i1.reciprocal = F; }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 early() { i1.sexSegregation = '**'; }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 early() { i1.spatiality = 'x'; }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 early() { i1.tag; }", "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 early() { c(i1,i1).tag; }", "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 early() { i1.tag = 17; } 2 early() { if (i1.tag == 17) stop(); }", __LINE__);
	
	// Test clippedIntegral()
	SLiMAssertScriptRaise(gen1_setup_i1 + "1 early() { i1.maxDistance = 0.45; } late() { i1.evaluate(p1); i1.clippedIntegral(NULL); stop(); }", "non-spatial interactions", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 early() { i1.maxDistance = 0.45; } late() { i1.evaluate(p1); i1.clippedIntegral(NULL); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 early() { i1.maxDistance = 0.45; } late() { i1.evaluate(p1); i1.clippedIntegral(p1.individuals[0]); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xPx + "1 early() { i1.maxDistance = 0.45; } late() { i1.evaluate(p1); i1.clippedIntegral(NULL); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xPx + "1 early() { i1.maxDistance = 0.45; } late() { i1.evaluate(p1); i1.clippedIntegral(p1.individuals[0]); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xy + "1 early() { i1.maxDistance = 0.45; } late() { i1.evaluate(p1); i1.clippedIntegral(NULL); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xy + "1 early() { i1.maxDistance = 0.45; } late() { i1.evaluate(p1); i1.clippedIntegral(p1.individuals[0]); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyPxy + "1 early() { i1.maxDistance = 0.45; } late() { i1.evaluate(p1); i1.clippedIntegral(NULL); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyPxy + "1 early() { i1.maxDistance = 0.45; } late() { i1.evaluate(p1); i1.clippedIntegral(p1.individuals[0]); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 early() { i1.maxDistance = 0.45; } late() { i1.evaluate(p1); i1.clippedIntegral(NULL); stop(); }", "not been implemented", __LINE__);
	
	// Run tests in a variety of combinations
	_RunInteractionTypeTests_Nonspatial(false, "**");
	
	_RunInteractionTypeTests_Spatial(" INF ", false, "**");
	_RunInteractionTypeTests_Spatial("999.0", false, "**");
	
	_RunInteractionTypeTests_LocalPopDensity();		// different enough to get its own call
	
	for (int sex_seg_index = 0; sex_seg_index <= 8; ++sex_seg_index)
	{
		// For a full test, uncomment all cases below; that makes for a long test runtime, but it works.
		// Note that the tests are throttled down when sexSegregation != "**" anyway, because the results
		// will vary, and it's too much work to figure out the right answer for every test in every
		// combination; we just test for a crash or error.
		std::string seg_str;
		
		switch (sex_seg_index)		// NOLINT(*-missing-default-case) : loop bounds
		{
			case 0: seg_str = "**"; break;
			case 1: seg_str = "*M"; break;
			case 2: seg_str = "*F"; break;
			case 3: seg_str = "M*"; break;
			case 4: seg_str = "MM"; break;
			case 5: seg_str = "MF"; break;
			case 6: seg_str = "F*"; break;
			case 7: seg_str = "FM"; break;
			case 8: seg_str = "FF"; break;
			default: continue;
		}
		
		_RunInteractionTypeTests_Nonspatial(true, seg_str);
		
		_RunInteractionTypeTests_Spatial(" INF ", true, seg_str);
		_RunInteractionTypeTests_Spatial("999.0", true, seg_str);
	}
}

void _RunInteractionTypeTests_Nonspatial(bool p_sex_enabled, const std::string &p_sex_segregation)
{
	std::string sex_string = p_sex_enabled ? "initializeSex('A'); " : "                    ";
	bool sex_seg_on = (p_sex_segregation != "**");
	
	std::string gen1_setup_i1_pop("initialize() { initializeMutationRate(1e-5); " + sex_string + "initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', '', sexSegregation='" + p_sex_segregation + "'); } 1 early() { sim.addSubpop('p1', 10); } 1:10 late() { i1.evaluate(p1); i1.strength(p1.individuals[0]); } 1 late() { ind = p1.individuals; ");
	
	SLiMAssertScriptStop(gen1_setup_i1_pop + "i1.unevaluate(); i1.evaluate(p1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1_pop + "i1.unevaluate(); i1.unevaluate(); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1_pop + "i1.distance(ind[0], ind[2]); stop(); }", "interaction be spatial", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1_pop + "i1.interactionDistance(ind[0], ind[2]); stop(); }", "interaction be spatial", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1_pop + "i1.distanceFromPoint(1.0, ind[0]); stop(); }", "interaction be spatial", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1_pop + "i1.drawByStrength(ind[0]); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1_pop + "i1.drawByStrength(ind[0]); stop(); } interaction(i1) { return 2.0; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1_pop + "i1.drawByStrength(ind[0]); stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1_pop + "i1.nearestNeighbors(ind[8], 1); stop(); }", "interaction be spatial", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1_pop + "i1.nearestInteractingNeighbors(ind[8], 1); stop(); }", "interaction be spatial", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1_pop + "i1.interactingNeighborCount(ind[8]); stop(); }", "interaction be spatial", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1_pop + "i1.nearestNeighborsOfPoint(19.0, p1, 1); stop(); }", "interaction be spatial", __LINE__);
	
	if (!sex_seg_on)
	{
		SLiMAssertScriptStop(gen1_setup_i1_pop + "if (i1.strength(ind[0], ind[2]) == 1.0) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1_pop + "if (identical(i1.strength(ind[5]), c(1.0, 1.0, 1.0, 1.0, 1.0, 0.0, 1.0, 1.0, 1.0, 1.0))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1_pop + "if (identical(i1.strength(ind[5], NULL), c(1.0, 1.0, 1.0, 1.0, 1.0, 0.0, 1.0, 1.0, 1.0, 1.0))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1_pop + "if (i1.strength(ind[0], ind[2]) == 2.0) stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1_pop + "if (identical(i1.strength(ind[5]), c(2.0, 2.0, 2.0, 2.0, 2.0, 0.0, 2.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1_pop + "if (identical(i1.strength(ind[5], NULL), c(2.0, 2.0, 2.0, 2.0, 2.0, 0.0, 2.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1_pop + "if (i1.strength(ind[0], ind[2]) == 2.0) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1_pop + "if (identical(i1.strength(ind[5]), c(2.0, 2.0, 2.0, 2.0, 2.0, 0.0, 2.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1_pop + "if (identical(i1.strength(ind[5], NULL), c(2.0, 2.0, 2.0, 2.0, 2.0, 0.0, 2.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		
		SLiMAssertScriptRaise(gen1_setup_i1_pop + "i1.drawByStrength(ind[0]); stop(); } interaction(i1) { return 'foo'+'bar'; }", "callbacks must provide", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1_pop + "i1.strength(ind[0], ind[2]); stop(); } interaction(i1) { return 'foo'+'bar'; }", "callbacks must provide", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1_pop + "i1.strength(ind[5], NULL); stop(); } interaction(i1) { return 'foo'+'bar'; }", "callbacks must provide", __LINE__);
	}
	
	SLiMAssertScriptRaise(gen1_setup_i1_pop + "i1.totalOfNeighborStrengths(ind[0]); stop(); }", "interaction be spatial", __LINE__);
}

void _RunInteractionTypeTests_Spatial(const std::string &p_max_distance, bool p_sex_enabled, const std::string &p_sex_segregation)
{
	std::string sex_string = p_sex_enabled ? "initializeSex('A'); " : "                    ";
	bool sex_seg_on = (p_sex_segregation != "**");
	bool max_dist_on = (p_max_distance != " INF ");		// the spaces make this the same width as "999.0", for error position checks
	
	// *** 1D
	for (int i = 0; i < 3; ++i)
	{
		std::string gen1_setup_i1x_pop;
		std::string spatiality;
		
		// test spatiality 'x', 'y', and 'z'
		if (i == 0)
		{
			spatiality = "x";
			gen1_setup_i1x_pop = "initialize() { initializeSLiMOptions(dimensionality='xyz'); " + sex_string + "initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', '" + spatiality + "', maxDistance=" + p_max_distance + ", sexSegregation='" + p_sex_segregation + "'); } 1 early() { sim.addSubpop('p1', 10); p1.individuals." + spatiality + " = c(-10.0, 0, 1, 2, 3, 5, 7, 8, 20, 25); p1.individuals.y = runif(10); p1.individuals.z = runif(10); i1.evaluate(p1); ind = p1.individuals; ";
		}
		else if (i == 1)
		{
			spatiality = "y";
			gen1_setup_i1x_pop = "initialize() { initializeSLiMOptions(dimensionality='xyz'); " + sex_string + "initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', '" + spatiality + "', maxDistance=" + p_max_distance + ", sexSegregation='" + p_sex_segregation + "'); } 1 early() { sim.addSubpop('p1', 10); p1.individuals." + spatiality + " = c(-10.0, 0, 1, 2, 3, 5, 7, 8, 20, 25); p1.individuals.x = runif(10); p1.individuals.z = runif(10); i1.evaluate(p1); ind = p1.individuals; ";
		}
		else
		{
			spatiality = "z";
			gen1_setup_i1x_pop = "initialize() { initializeSLiMOptions(dimensionality='xyz'); " + sex_string + "initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', '" + spatiality + "', maxDistance=" + p_max_distance + ", sexSegregation='" + p_sex_segregation + "'); } 1 early() { sim.addSubpop('p1', 10); p1.individuals." + spatiality + " = c(-10.0, 0, 1, 2, 3, 5, 7, 8, 20, 25); p1.individuals.x = runif(10); p1.individuals.y = runif(10); i1.evaluate(p1); ind = p1.individuals; ";
		}
		
		// Test InteractionType – (float)distance(object<Individual> individuals1, [No<Individual> individuals2 = NULL])
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (i1.distance(ind[0], ind[2]) == 11.0) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.distance(ind[2], ind[0:1]), c(11.0, 1.0))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.distance(ind[0], ind[2:3]), c(11.0, 12.0))) stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "if (i1.distance(ind[0:1], ind[2:3]) == 11.0) stop(); }", "must be a singleton", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.distance(ind[5], ind[c(0, 5, 9, 8, 1)]), c(15.0, 0, 20, 15, 5))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.distance(ind[8], ind[integer(0)]), float(0))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.distance(ind[1], ind[integer(0)]), float(0))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.distance(ind[5]), c(15.0, 5, 4, 3, 2, 0, 2, 3, 15, 20))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.distance(ind[5], NULL), c(15.0, 5, 4, 3, 2, 0, 2, 3, 15, 20))) stop(); }", __LINE__);
		
		// Test InteractionType – (float)interactionDistance(object<Individual>$ receiver, [No<Individual> exerters = NULL])
		if (!sex_seg_on)
		{
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (i1.interactionDistance(ind[0], ind[2]) == 11.0) stop(); }", __LINE__);
			SLiMAssertScriptRaise(gen1_setup_i1x_pop + "if (identical(i1.interactionDistance(ind[0:1], ind[2]), c(11.0, 1.0))) stop(); }", "must be a singleton", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.interactionDistance(ind[0], ind[2:3]), c(11.0, 12.0))) stop(); }", __LINE__);
			SLiMAssertScriptRaise(gen1_setup_i1x_pop + "if (i1.interactionDistance(ind[0:1], ind[2:3]) == 11.0) stop(); }", "must be a singleton", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.interactionDistance(ind[5], ind[c(0, 5, 9, 8, 1)]), c(15.0, INF, 20, 15, 5))) stop(); }", __LINE__);
			SLiMAssertScriptRaise(gen1_setup_i1x_pop + "if (identical(i1.interactionDistance(ind[integer(0)], ind[8]), float(0))) stop(); }", "must be a singleton", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.interactionDistance(ind[1], ind[integer(0)]), float(0))) stop(); }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.interactionDistance(ind[5]), c(15.0, 5, 4, 3, 2, INF, 2, 3, 15, 20))) stop(); }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.interactionDistance(ind[5], NULL), c(15.0, 5, 4, 3, 2, INF, 2, 3, 15, 20))) stop(); }", __LINE__);
		}
		else
		{
			// comprehensively testing all the different sex-seg cases is complicated, but we can at least test the two branches of the code against each other
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.interactionDistance(ind[5]), i1.interactionDistance(ind[5], NULL))) stop(); }", __LINE__);
		}
		
		// Test InteractionType – (float)distanceFromPoint(float point, object<Individual> individuals1)
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (i1.distanceFromPoint(1.0, ind[0]) == 11.0) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.distanceFromPoint(1.0, ind[0:1]), c(11.0, 1.0))) stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "if (i1.distanceFromPoint(1.0:2.0, ind[0:1]) == 11.0) stop(); }", "point is of length equal to", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.distanceFromPoint(5.0, ind[c(0, 5, 9, 8, 1)]), c(15.0, 0, 20, 15, 5))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.distanceFromPoint(8.0, ind[integer(0)]), float(0))) stop(); }", __LINE__);
		
		// Test InteractionType – (object<Individual>)drawByStrength(object<Individual>$ individual, [integer$ count = 1])
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.drawByStrength(ind[0]); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.drawByStrength(ind[0], 1); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.drawByStrength(ind[0], 50); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.drawByStrength(ind[0], 0), ind[integer(0)])) stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.drawByStrength(ind[0], -1); stop(); }", "requires count >= 0", __LINE__);
		
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.drawByStrength(ind[0]); stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.drawByStrength(ind[0], 1); stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.drawByStrength(ind[0], 50); stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.drawByStrength(ind[0], 0), ind[integer(0)])) stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.drawByStrength(ind[0], -1); stop(); } interaction(i1) { return 2.0; }", "requires count >= 0", __LINE__);
		
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.drawByStrength(ind[0]); stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.drawByStrength(ind[0], 1); stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.drawByStrength(ind[0], 50); stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.drawByStrength(ind[0], 0), ind[integer(0)])) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.drawByStrength(ind[0], -1); stop(); } interaction(i1) { return strength * 2.0; }", "requires count >= 0", __LINE__);
		
		if (!sex_seg_on)
		{
			SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.drawByStrength(ind[0], 1); stop(); } interaction(i1) { return 'foo'; }", "callbacks must provide", __LINE__);
			SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.drawByStrength(ind[0], 1); stop(); } interaction(i1) { return 'foo'+'bar'; }", "callbacks must provide", __LINE__);
			SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.drawByStrength(ind[0], 50); stop(); } interaction(i1) { return 'foo'; }", "callbacks must provide", __LINE__);
			SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.drawByStrength(ind[0], 50); stop(); } interaction(i1) { return 'foo'+'bar'; }", "callbacks must provide", __LINE__);
			SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.drawByStrength(ind, returnDict=T); stop(); } interaction(i1) { return 'foo'; }", "callbacks must provide", __LINE__);
			SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.drawByStrength(ind, returnDict=T); stop(); } interaction(i1) { return 'foo'+'bar'; }", "callbacks must provide", __LINE__);
		}
		
		// Test InteractionType – (void)evaluate(io<Subpopulation> subpops)
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.evaluate(); stop(); }", "required argument subpops", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.evaluate(p1); i1.evaluate(p1); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.evaluate(p1); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.evaluate(1); stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.evaluate(NULL); stop(); }", "cannot be type NULL", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.evaluate(10); stop(); }", "p10 not defined", __LINE__);
		
		// Test InteractionType – (object<Individual>)nearestNeighbors(object<Individual>$ individual, [integer$ count = 1])
		// Test InteractionType – (integer)neighborCount(object<Individual> receivers, [No<Subpopulation>$ exerterSubpop = NULL])
		// Test InteractionType – (integer$)neighborCountOfPoint(float point, io<Subpopulation>$ exerterSubpop)
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "if (identical(i1.nearestNeighbors(ind[8], -1), ind[integer(0)])) stop(); }", "requires count >= 0", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.nearestNeighbors(ind[8], 0), ind[integer(0)])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.nearestNeighbors(ind[8], 1), ind[9])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(sortBy(i1.nearestNeighbors(ind[8], 3), 'index'), ind[c(6,7,9)])) stop(); }", __LINE__);
		for (int ind_index = 0; ind_index < 10; ++ind_index)
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (size(i1.nearestNeighbors(ind[" + std::to_string(ind_index) + "], 100)) == i1.neighborCount(ind[" + std::to_string(ind_index) + "])) stop(); }", __LINE__);
		for (int ind_index = 0; ind_index < 10; ++ind_index)
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (size(i1.nearestNeighbors(ind[" + std::to_string(ind_index) + "], 100)) + 1 == i1.neighborCountOfPoint(ind[" + std::to_string(ind_index) + "]." + spatiality + ", p1)) stop(); }", __LINE__);
		SLiMAssertScriptSuccess(gen1_setup_i1x_pop + "nn = i1.nearestNeighbors(ind, 100, returnDict=T); nc = i1.neighborCount(ind); for (i in 0:9) if (size(nn.getValue(i)) != nc[i]) stop(); }", __LINE__);
		
		// Test InteractionType – (object<Individual>)nearestInteractingNeighbors(object<Individual>$ individual, [integer$ count = 1])
		// Test InteractionType – (object<Individual>)interactingNeighborCount(object<Individual>$ individual, [integer$ count = 1])
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "if (identical(i1.nearestInteractingNeighbors(ind[8], -1), ind[integer(0)])) stop(); }", "requires count >= 0", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.nearestInteractingNeighbors(ind[8], 0), ind[integer(0)])) stop(); }", __LINE__);
		for (int ind_index = 0; ind_index < 10; ++ind_index)
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (size(i1.nearestInteractingNeighbors(ind[" + std::to_string(ind_index) + "], 100)) == i1.interactingNeighborCount(ind[" + std::to_string(ind_index) + "])) stop(); }", __LINE__);
		for (int ind_index = 0; ind_index < 10; ++ind_index)
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (size(i1.nearestInteractingNeighbors(ind[" + std::to_string(ind_index) + "], 100)) == sum(isFinite(i1.interactionDistance(ind[" + std::to_string(ind_index) + "])))) stop(); }", __LINE__);
		SLiMAssertScriptSuccess(gen1_setup_i1x_pop + "nn = i1.nearestInteractingNeighbors(ind, 100, returnDict=T); nc = i1.interactingNeighborCount(ind); for (i in 0:9) if (size(nn.getValue(i)) != nc[i]) stop(); }", __LINE__);
		
		if (!sex_seg_on)
		{
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.nearestInteractingNeighbors(ind, returnDict=T); stop(); } interaction(i1) { return 'foo'; }", __LINE__);	// doesn't raise because it doesn't use strengths
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.nearestInteractingNeighbors(ind, returnDict=T); stop(); } interaction(i1) { return 'foo'+'bar'; }", __LINE__);	// doesn't raise because it doesn't use strengths
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.interactingNeighborCount(ind); stop(); } interaction(i1) { return 'foo'; }", __LINE__);	// doesn't raise because it doesn't use strengths
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.interactingNeighborCount(ind); stop(); } interaction(i1) { return 'foo'+'bar'; }", __LINE__);	// doesn't raise because it doesn't use strengths
		}
		
		// Test InteractionType – (object<Individual>)nearestNeighborsOfPoint(float point, io<Subpopulation>$ subpop, [integer$ count = 1])
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "if (identical(i1.nearestNeighborsOfPoint(5.0, p1, -1), ind[integer(0)])) stop(); }", "requires count >= 0", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.nearestNeighborsOfPoint(5.0, p1, 0), ind[integer(0)])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.nearestNeighborsOfPoint(19.0, p1, 1), ind[8])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(sortBy(i1.nearestNeighborsOfPoint(19.0, p1, 3), 'index'), ind[c(7,8,9)])) stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "if (identical(i1.nearestNeighborsOfPoint(5.0, 1, -1), ind[integer(0)])) stop(); }", "requires count >= 0", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.nearestNeighborsOfPoint(5.0, 1, 0), ind[integer(0)])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.nearestNeighborsOfPoint(19.0, 1, 1), ind[8])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(sortBy(i1.nearestNeighborsOfPoint(19.0, 1, 3), 'index'), ind[c(7,8,9)])) stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "if (identical(sortBy(i1.nearestNeighborsOfPoint(19.0, 10, 3), 'index'), ind[c(7,8,9)])) stop(); }", "p10 not defined", __LINE__);
		
		// Test InteractionType – (void)setInteractionFunction(string$ functionType, ...)
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.setInteractionFunction('q', 10.0); i1.evaluate(p1); stop(); }", "while the interaction is being evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.setInteractionFunction('q', 10.0); i1.evaluate(p1); stop(); }", "functionType 'q' must be", __LINE__);
		if (max_dist_on)
		{
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.unevaluate(); i1.setInteractionFunction('f', 5.0); i1.evaluate(p1); stop(); }", __LINE__);
			SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.setInteractionFunction('f'); i1.evaluate(p1); stop(); }", "requires exactly", __LINE__);
			SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.setInteractionFunction('f', 5.0, 2.0); i1.evaluate(p1); stop(); }", "requires exactly", __LINE__);
		}
		else
			SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.setInteractionFunction('f', 5.0); i1.evaluate(p1); stop(); }", "finite maximum interaction distance", __LINE__);
		
		if (!max_dist_on)
			SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.setInteractionFunction('l', 5.0); i1.evaluate(p1); stop(); }", "finite maximum interaction distance", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('l', 5.0); i1.evaluate(p1); stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('l'); i1.evaluate(p1); stop(); }", "requires exactly", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('l', 5.0, 2.0); i1.evaluate(p1); stop(); }", "requires exactly", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('e', 5.0, 1.0); i1.evaluate(p1); stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('e', 5.0); i1.evaluate(p1); stop(); }", "requires exactly", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('e', 5.0, 2.0, 1.0); i1.evaluate(p1); stop(); }", "requires exactly", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('n', 5.0, 1.0); i1.evaluate(p1); stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('n', 5.0); i1.evaluate(p1); stop(); }", "requires exactly", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('n', 5.0, 2.0, 1.0); i1.evaluate(p1); stop(); }", "requires exactly", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('c', 5.0, 1.0); i1.evaluate(p1); stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('c', 5.0); i1.evaluate(p1); stop(); }", "requires exactly", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('c', 5.0, 2.0, 1.0); i1.evaluate(p1); stop(); }", "requires exactly", __LINE__);
		
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('n', 5.0, -1.0); stop(); }", "must have a standard deviation parameter >= 0", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('c', 5.0, 0.0); stop(); }", "must have a scale parameter > 0", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('c', 5.0, -1.0); stop(); }", "must have a scale parameter > 0", __LINE__);
		
		// Test InteractionType – (float)strength(object<Individual> individuals1, [No<Individual> individuals2 = NULL])
		if (!sex_seg_on)
		{
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (i1.strength(ind[0], ind[2]) == 1.0) stop(); }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.strength(ind[0], ind[2:3]), c(1.0, 1.0))) stop(); }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.strength(ind[5], ind[c(0, 5, 9, 8, 1)]), c(1.0, 0.0, 1.0, 1.0, 1.0))) stop(); }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.strength(ind[1], ind[integer(0)]), float(0))) stop(); }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.strength(ind[5]), c(1.0, 1.0, 1.0, 1.0, 1.0, 0.0, 1.0, 1.0, 1.0, 1.0))) stop(); }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.strength(ind[5], NULL), c(1.0, 1.0, 1.0, 1.0, 1.0, 0.0, 1.0, 1.0, 1.0, 1.0))) stop(); }", __LINE__);
			
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (i1.strength(ind[0], ind[2]) == 2.0) stop(); } interaction(i1) { return 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.strength(ind[0], ind[2:3]), c(2.0, 2.0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.strength(ind[5], ind[c(0, 5, 9, 8, 1)]), c(2.0, 0.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.strength(ind[1], ind[integer(0)]), float(0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.strength(ind[5]), c(2.0, 2.0, 2.0, 2.0, 2.0, 0.0, 2.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.strength(ind[5], NULL), c(2.0, 2.0, 2.0, 2.0, 2.0, 0.0, 2.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);
			
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (i1.strength(ind[0], ind[2]) == 2.0) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.strength(ind[0], ind[2:3]), c(2.0, 2.0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.strength(ind[5], ind[c(0, 5, 9, 8, 1)]), c(2.0, 0.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.strength(ind[1], ind[integer(0)]), float(0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.strength(ind[5]), c(2.0, 2.0, 2.0, 2.0, 2.0, 0.0, 2.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.strength(ind[5], NULL), c(2.0, 2.0, 2.0, 2.0, 2.0, 0.0, 2.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
			
			SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.strength(ind[0], ind[2]); stop(); } interaction(i1) { return 'foo'; }", "callbacks must provide", __LINE__);
			SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.strength(ind[0], ind[2]); stop(); } interaction(i1) { return 'foo'+'bar'; }", "callbacks must provide", __LINE__);
			SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.strength(ind[5], NULL); stop(); } interaction(i1) { return 'foo'; }", "callbacks must provide", __LINE__);
			SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.strength(ind[5], NULL); stop(); } interaction(i1) { return 'foo'+'bar'; }", "callbacks must provide", __LINE__);
		}
		
		// Test InteractionType – (float)totalOfNeighborStrengths(object<Individual> individuals)
		if (!sex_seg_on)
		{
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.totalOfNeighborStrengths(ind[integer(0)]), float(0))) stop(); }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.totalOfNeighborStrengths(ind[0]), 9.0)) stop(); }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.totalOfNeighborStrengths(ind[5]), 9.0)) stop(); }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.totalOfNeighborStrengths(ind[9]), 9.0)) stop(); }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.totalOfNeighborStrengths(ind[c(0, 5, 9)]), c(9.0, 9.0, 9.0))) stop(); }", __LINE__);
			
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.totalOfNeighborStrengths(ind[integer(0)]), float(0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.totalOfNeighborStrengths(ind[0]), 18.0)) stop(); } interaction(i1) { return 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.totalOfNeighborStrengths(ind[5]), 18.0)) stop(); } interaction(i1) { return 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.totalOfNeighborStrengths(ind[9]), 18.0)) stop(); } interaction(i1) { return 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.totalOfNeighborStrengths(ind[c(0, 5, 9)]), c(18.0, 18.0, 18.0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);
			
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.totalOfNeighborStrengths(ind[integer(0)]), float(0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.totalOfNeighborStrengths(ind[0]), 18.0)) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.totalOfNeighborStrengths(ind[5]), 18.0)) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.totalOfNeighborStrengths(ind[9]), 18.0)) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.totalOfNeighborStrengths(ind[c(0, 5, 9)]), c(18.0, 18.0, 18.0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
			
			SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.totalOfNeighborStrengths(ind[0]); stop(); } interaction(i1) { return 'foo'; }", "callbacks must provide", __LINE__);
			SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.totalOfNeighborStrengths(ind[c(0, 5, 9)]); stop(); } interaction(i1) { return 'foo'; }", "callbacks must provide", __LINE__);
			SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.totalOfNeighborStrengths(ind); stop(); } interaction(i1) { return 'foo'; }", "callbacks must provide", __LINE__);
			
			SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.totalOfNeighborStrengths(ind[0]); stop(); } interaction(i1) { return 'foo'+'bar'; }", "callbacks must provide", __LINE__);
			SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.totalOfNeighborStrengths(ind[c(0, 5, 9)]); stop(); } interaction(i1) { return 'foo'+'bar'; }", "callbacks must provide", __LINE__);
			SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.totalOfNeighborStrengths(ind); stop(); } interaction(i1) { return 'foo'+'bar'; }", "callbacks must provide", __LINE__);
		}
		
		// Test InteractionType – (void)unevaluate(void)
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.unevaluate(); i1.evaluate(p1); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.unevaluate(); i1.unevaluate(); stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.distance(ind[0], ind[2]); stop(); }", "must be evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.interactionDistance(ind[0], ind[2]); stop(); }", "must be evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.distanceFromPoint(1.0, ind[0]); stop(); }", "must be evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.drawByStrength(ind[0]); stop(); }", "must be evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.nearestNeighbors(ind[8], 1); stop(); }", "must be evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.nearestInteractingNeighbors(ind[8], 1); stop(); }", "must be evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.interactingNeighborCount(ind[8]); stop(); }", "must be evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.nearestNeighborsOfPoint(19.0, p1, 1); stop(); }", "must be evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.strength(ind[0], ind[2]); stop(); }", "must be evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.totalOfNeighborStrengths(ind[0]); stop(); }", "must be evaluated", __LINE__);
	}
	
	// *** 2D
	for (int i = 0; i < 6; ++i)
	{
		std::string gen1_setup_i1xy_pop;
		bool use_first_coordinate = (i < 3);
		std::string spatiality;
		
		// test spatiality 'xy' for x and y, 'xz' for x and z, and 'yz' for y and z 
		if (i == 0)
		{
			spatiality = "xy";
			gen1_setup_i1xy_pop = "initialize() { initializeSLiMOptions(dimensionality='xyz'); " + sex_string + "initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'xy', maxDistance=" + p_max_distance + ", sexSegregation='" + p_sex_segregation + "'); } 1 early() { sim.addSubpop('p1', 10); p1.individuals.x = c(-10.0, 0, 1, 2, 3, 5, 7, 8, 20, 25); p1.individuals.y = 0; p1.individuals.z = runif(10); i1.evaluate(p1); ind = p1.individuals; ";
		}
		else if (i == 1)
		{
			spatiality = "xz";
			gen1_setup_i1xy_pop = "initialize() { initializeSLiMOptions(dimensionality='xyz'); " + sex_string + "initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'xz', maxDistance=" + p_max_distance + ", sexSegregation='" + p_sex_segregation + "'); } 1 early() { sim.addSubpop('p1', 10); p1.individuals.x = c(-10.0, 0, 1, 2, 3, 5, 7, 8, 20, 25); p1.individuals.z = 0; p1.individuals.y = runif(10); i1.evaluate(p1); ind = p1.individuals; ";
		}
		else if (i == 2)
		{
			spatiality = "yz";
			gen1_setup_i1xy_pop = "initialize() { initializeSLiMOptions(dimensionality='xyz'); " + sex_string + "initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'yz', maxDistance=" + p_max_distance + ", sexSegregation='" + p_sex_segregation + "'); } 1 early() { sim.addSubpop('p1', 10); p1.individuals.y = c(-10.0, 0, 1, 2, 3, 5, 7, 8, 20, 25); p1.individuals.z = 0; p1.individuals.x = runif(10); i1.evaluate(p1); ind = p1.individuals; ";
		}
		else if (i == 3)
		{
			spatiality = "xy";
			gen1_setup_i1xy_pop = "initialize() { initializeSLiMOptions(dimensionality='xyz'); " + sex_string + "initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'xy', maxDistance=" + p_max_distance + ", sexSegregation='" + p_sex_segregation + "'); } 1 early() { sim.addSubpop('p1', 10); p1.individuals.y = c(-10.0, 0, 1, 2, 3, 5, 7, 8, 20, 25); p1.individuals.x = 0; p1.individuals.z = runif(10); i1.evaluate(p1); ind = p1.individuals; ";
		}
		else if (i == 4)
		{
			spatiality = "xz";
			gen1_setup_i1xy_pop = "initialize() { initializeSLiMOptions(dimensionality='xyz'); " + sex_string + "initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'xz', maxDistance=" + p_max_distance + ", sexSegregation='" + p_sex_segregation + "'); } 1 early() { sim.addSubpop('p1', 10); p1.individuals.z = c(-10.0, 0, 1, 2, 3, 5, 7, 8, 20, 25); p1.individuals.x = 0; p1.individuals.y = runif(10); i1.evaluate(p1); ind = p1.individuals; ";
		}
		else // if (i == 5)
		{
			spatiality = "yz";
			gen1_setup_i1xy_pop = "initialize() { initializeSLiMOptions(dimensionality='xyz'); " + sex_string + "initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'yz', maxDistance=" + p_max_distance + ", sexSegregation='" + p_sex_segregation + "'); } 1 early() { sim.addSubpop('p1', 10); p1.individuals.z = c(-10.0, 0, 1, 2, 3, 5, 7, 8, 20, 25); p1.individuals.y = 0; p1.individuals.x = runif(10); i1.evaluate(p1); ind = p1.individuals; ";
		}
		
		// Test InteractionType – (float)distance(object<Individual> individuals1, [No<Individual> individuals2 = NULL])
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (i1.distance(ind[0], ind[2]) == 11.0) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.distance(ind[2], ind[0:1]), c(11.0, 1.0))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.distance(ind[0], ind[2:3]), c(11.0, 12.0))) stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "if (i1.distance(ind[0:1], ind[2:3]) == 11.0) stop(); }", "must be a singleton", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.distance(ind[5], ind[c(0, 5, 9, 8, 1)]), c(15.0, 0, 20, 15, 5))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.distance(ind[1], ind[integer(0)]), float(0))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.distance(ind[5]), c(15.0, 5, 4, 3, 2, 0, 2, 3, 15, 20))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.distance(ind[5], NULL), c(15.0, 5, 4, 3, 2, 0, 2, 3, 15, 20))) stop(); }", __LINE__);
		
		// Test InteractionType – (float)interactionDistance(object<Individual>$ receiver, [No<Individual> exerters = NULL])
		if (!sex_seg_on)
		{
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (i1.interactionDistance(ind[0], ind[2]) == 11.0) stop(); }", __LINE__);
			SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "if (identical(i1.interactionDistance(ind[0:1], ind[2]), c(11.0, 1.0))) stop(); }", "must be a singleton", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.interactionDistance(ind[0], ind[2:3]), c(11.0, 12.0))) stop(); }", __LINE__);
			SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "if (i1.interactionDistance(ind[0:1], ind[2:3]) == 11.0) stop(); }", "must be a singleton", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.interactionDistance(ind[5], ind[c(0, 5, 9, 8, 1)]), c(15.0, INF, 20, 15, 5))) stop(); }", __LINE__);
			SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "if (identical(i1.interactionDistance(ind[integer(0)], ind[8]), float(0))) stop(); }", "must be a singleton", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.interactionDistance(ind[1], ind[integer(0)]), float(0))) stop(); }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.interactionDistance(ind[5]), c(15.0, 5, 4, 3, 2, INF, 2, 3, 15, 20))) stop(); }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.interactionDistance(ind[5], NULL), c(15.0, 5, 4, 3, 2, INF, 2, 3, 15, 20))) stop(); }", __LINE__);
		}
		else
		{
			// comprehensively testing all the different sex-seg cases is complicated, but we can at least test the two branches of the code against each other
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.interactionDistance(ind[5]), i1.interactionDistance(ind[5], NULL))) stop(); }", __LINE__);
		}
		
		// Test InteractionType – (float)distanceFromPoint(float point, object<Individual> individuals1)
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (i1.distanceFromPoint(c(" + (use_first_coordinate ? "1.0, 0.0" : "0.0, 1.0") + "), ind[0]) == 11.0) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.distanceFromPoint(c(" + (use_first_coordinate ? "1.0, 0.0" : "0.0, 1.0") + "), ind[0:1]), c(11.0, 1.0))) stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "if (i1.distanceFromPoint(1.0, ind[0:1]) == 11.0) stop(); }", "point is of length equal to", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.distanceFromPoint(c(" + (use_first_coordinate ? "5.0, 0.0" : "0.0, 5.0") + "), ind[c(0, 5, 9, 8, 1)]), c(15.0, 0, 20, 15, 5))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.distanceFromPoint(c(" + (use_first_coordinate ? "8.0, 0.0" : "0.0, 8.0") + "), ind[integer(0)]), float(0))) stop(); }", __LINE__);
		
		// Test InteractionType – (object<Individual>)drawByStrength(object<Individual>$ individual, [integer$ count = 1])
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.drawByStrength(ind[0]); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.drawByStrength(ind[0], 1); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.drawByStrength(ind[0], 50); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.drawByStrength(ind[0], 0), ind[integer(0)])) stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.drawByStrength(ind[0], -1); stop(); }", "requires count >= 0", __LINE__);
		
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.drawByStrength(ind[0]); stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.drawByStrength(ind[0], 1); stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.drawByStrength(ind[0], 50); stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.drawByStrength(ind[0], 0), ind[integer(0)])) stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.drawByStrength(ind[0], -1); stop(); } interaction(i1) { return 2.0; }", "requires count >= 0", __LINE__);
		
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.drawByStrength(ind[0]); stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.drawByStrength(ind[0], 1); stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.drawByStrength(ind[0], 50); stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.drawByStrength(ind[0], 0), ind[integer(0)])) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.drawByStrength(ind[0], -1); stop(); } interaction(i1) { return strength * 2.0; }", "requires count >= 0", __LINE__);
		
		// Test InteractionType – (void)evaluate(io<Subpopulation> subpops)
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.evaluate(p1); i1.evaluate(p1); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.evaluate(p1); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.evaluate(1); stop(); }", __LINE__);
		
		// Test InteractionType – (object<Individual>)nearestNeighbors(object<Individual>$ individual, [integer$ count = 1])
		// Test InteractionType – (integer)neighborCount(object<Individual> receivers, [No<Subpopulation>$ exerterSubpop = NULL])
		// Test InteractionType – (integer$)neighborCountOfPoint(float point, io<Subpopulation>$ exerterSubpop)
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "if (identical(i1.nearestNeighbors(ind[8], -1), ind[integer(0)])) stop(); }", "requires count >= 0", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.nearestNeighbors(ind[8], 0), ind[integer(0)])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.nearestNeighbors(ind[8], 1), ind[9])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(sortBy(i1.nearestNeighbors(ind[8], 3), 'index'), ind[c(6,7,9)])) stop(); }", __LINE__);
		for (int ind_index = 0; ind_index < 10; ++ind_index)
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (size(i1.nearestNeighbors(ind[" + std::to_string(ind_index) + "], 100)) == i1.neighborCount(ind[" + std::to_string(ind_index) + "])) stop(); }", __LINE__);
		for (int ind_index = 0; ind_index < 10; ++ind_index)
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (size(i1.nearestNeighbors(ind[" + std::to_string(ind_index) + "], 100)) + 1 == i1.neighborCountOfPoint(ind[" + std::to_string(ind_index) + "]." + spatiality + ", p1)) stop(); }", __LINE__);
		SLiMAssertScriptSuccess(gen1_setup_i1xy_pop + "nn = i1.nearestNeighbors(ind, 100, returnDict=T); nc = i1.neighborCount(ind); for (i in 0:9) if (size(nn.getValue(i)) != nc[i]) stop(); }", __LINE__);
		
		// Test InteractionType – (object<Individual>)nearestInteractingNeighbors(object<Individual>$ individual, [integer$ count = 1])
		// Test InteractionType – (object<Individual>)interactingNeighborCount(object<Individual>$ individual, [integer$ count = 1])
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "if (identical(i1.nearestInteractingNeighbors(ind[8], -1), ind[integer(0)])) stop(); }", "requires count >= 0", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.nearestInteractingNeighbors(ind[8], 0), ind[integer(0)])) stop(); }", __LINE__);
		for (int ind_index = 0; ind_index < 10; ++ind_index)
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (size(i1.nearestInteractingNeighbors(ind[" + std::to_string(ind_index) + "], 100)) == i1.interactingNeighborCount(ind[" + std::to_string(ind_index) + "])) stop(); }", __LINE__);
		for (int ind_index = 0; ind_index < 10; ++ind_index)
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (size(i1.nearestInteractingNeighbors(ind[" + std::to_string(ind_index) + "], 100)) == sum(isFinite(i1.interactionDistance(ind[" + std::to_string(ind_index) + "])))) stop(); }", __LINE__);
		SLiMAssertScriptSuccess(gen1_setup_i1xy_pop + "nn = i1.nearestInteractingNeighbors(ind, 100, returnDict=T); nc = i1.interactingNeighborCount(ind); for (i in 0:9) if (size(nn.getValue(i)) != nc[i]) stop(); }", __LINE__);
		
		// Test InteractionType – (object<Individual>)nearestNeighborsOfPoint(float point, io<Subpopulation>$ subpop, [integer$ count = 1])
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "if (identical(i1.nearestNeighborsOfPoint(c(5.0, 0.0), p1, -1), ind[integer(0)])) stop(); }", "requires count >= 0", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.nearestNeighborsOfPoint(c(5.0, 0.0), p1, 0), ind[integer(0)])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.nearestNeighborsOfPoint(c(" + (use_first_coordinate ? "19.0, 0.0" : "0.0, 19.0") + "), p1, 1), ind[8])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(sortBy(i1.nearestNeighborsOfPoint(c(" + (use_first_coordinate ? "19.0, 0.0" : "0.0, 19.0") + "), p1, 3), 'index'), ind[c(7,8,9)])) stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "if (identical(i1.nearestNeighborsOfPoint(c(5.0, 0.0), 1, -1), ind[integer(0)])) stop(); }", "requires count >= 0", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.nearestNeighborsOfPoint(c(5.0, 0.0), 1, 0), ind[integer(0)])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.nearestNeighborsOfPoint(c(" + (use_first_coordinate ? "19.0, 0.0" : "0.0, 19.0") + "), 1, 1), ind[8])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(sortBy(i1.nearestNeighborsOfPoint(c(" + (use_first_coordinate ? "19.0, 0.0" : "0.0, 19.0") + "), 1, 3), 'index'), ind[c(7,8,9)])) stop(); }", __LINE__);
		
		// Test InteractionType – (void)setInteractionFunction(string$ functionType, ...)
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.setInteractionFunction('q', 10.0); i1.evaluate(p1); stop(); }", "while the interaction is being evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.setInteractionFunction('q', 10.0); i1.evaluate(p1); stop(); }", "functionType 'q' must be", __LINE__);
		if (max_dist_on)
		{
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.setInteractionFunction('f', 5.0); i1.evaluate(p1); stop(); }", __LINE__);
			SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.setInteractionFunction('f'); i1.evaluate(p1); stop(); }", "requires exactly", __LINE__);
			SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.setInteractionFunction('f', 5.0, 2.0); i1.evaluate(p1); stop(); }", "requires exactly", __LINE__);
		}
		else
			SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.setInteractionFunction('f', 5.0); i1.evaluate(p1); stop(); }", "finite maximum interaction distance", __LINE__);
		
		if (!max_dist_on)
			SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.setInteractionFunction('l', 5.0); i1.evaluate(p1); stop(); }", "finite maximum interaction distance", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('l', 5.0); i1.evaluate(p1); stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('l'); i1.evaluate(p1); stop(); }", "requires exactly", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('l', 5.0, 2.0); i1.evaluate(p1); stop(); }", "requires exactly", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('e', 5.0, 1.0); i1.evaluate(p1); stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('e', 5.0); i1.evaluate(p1); stop(); }", "requires exactly", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('e', 5.0, 2.0, 1.0); i1.evaluate(p1); stop(); }", "requires exactly", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('n', 5.0, 1.0); i1.evaluate(p1); stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('n', 5.0); i1.evaluate(p1); stop(); }", "requires exactly", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('n', 5.0, 2.0, 1.0); i1.evaluate(p1); stop(); }", "requires exactly", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('c', 5.0, 1.0); i1.evaluate(p1); stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('c', 5.0); i1.evaluate(p1); stop(); }", "requires exactly", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('c', 5.0, 2.0, 1.0); i1.evaluate(p1); stop(); }", "requires exactly", __LINE__);
		
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('n', 5.0, -1.0); stop(); }", "must have a standard deviation parameter >= 0", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('c', 5.0, 0.0); stop(); }", "must have a scale parameter > 0", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('c', 5.0, -1.0); stop(); }", "must have a scale parameter > 0", __LINE__);
		
		// Test InteractionType – (float)strength(object<Individual> individuals1, [No<Individual> individuals2 = NULL])
		if (!sex_seg_on)
		{
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (i1.strength(ind[0], ind[2]) == 1.0) stop(); }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.strength(ind[0], ind[2:3]), c(1.0, 1.0))) stop(); }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.strength(ind[5], ind[c(0, 5, 9, 8, 1)]), c(1.0, 0.0, 1.0, 1.0, 1.0))) stop(); }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.strength(ind[1], ind[integer(0)]), float(0))) stop(); }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.strength(ind[5]), c(1.0, 1.0, 1.0, 1.0, 1.0, 0.0, 1.0, 1.0, 1.0, 1.0))) stop(); }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.strength(ind[5], NULL), c(1.0, 1.0, 1.0, 1.0, 1.0, 0.0, 1.0, 1.0, 1.0, 1.0))) stop(); }", __LINE__);
			
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (i1.strength(ind[0], ind[2]) == 2.0) stop(); } interaction(i1) { return 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.strength(ind[0], ind[2:3]), c(2.0, 2.0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.strength(ind[5], ind[c(0, 5, 9, 8, 1)]), c(2.0, 0.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.strength(ind[1], ind[integer(0)]), float(0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.strength(ind[5]), c(2.0, 2.0, 2.0, 2.0, 2.0, 0.0, 2.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.strength(ind[5], NULL), c(2.0, 2.0, 2.0, 2.0, 2.0, 0.0, 2.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);
			
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (i1.strength(ind[0], ind[2]) == 2.0) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.strength(ind[0], ind[2:3]), c(2.0, 2.0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.strength(ind[5], ind[c(0, 5, 9, 8, 1)]), c(2.0, 0.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.strength(ind[1], ind[integer(0)]), float(0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.strength(ind[5]), c(2.0, 2.0, 2.0, 2.0, 2.0, 0.0, 2.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.strength(ind[5], NULL), c(2.0, 2.0, 2.0, 2.0, 2.0, 0.0, 2.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		}
		
		// Test InteractionType – (float)totalOfNeighborStrengths(object<Individual> individuals)
		if (!sex_seg_on)
		{
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.totalOfNeighborStrengths(ind[integer(0)]), float(0))) stop(); }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.totalOfNeighborStrengths(ind[0]), 9.0)) stop(); }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.totalOfNeighborStrengths(ind[5]), 9.0)) stop(); }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.totalOfNeighborStrengths(ind[9]), 9.0)) stop(); }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.totalOfNeighborStrengths(ind[c(0, 5, 9)]), c(9.0, 9.0, 9.0))) stop(); }", __LINE__);
			
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.totalOfNeighborStrengths(ind[integer(0)]), float(0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.totalOfNeighborStrengths(ind[0]), 18.0)) stop(); } interaction(i1) { return 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.totalOfNeighborStrengths(ind[5]), 18.0)) stop(); } interaction(i1) { return 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.totalOfNeighborStrengths(ind[9]), 18.0)) stop(); } interaction(i1) { return 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.totalOfNeighborStrengths(ind[c(0, 5, 9)]), c(18.0, 18.0, 18.0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);
			
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.totalOfNeighborStrengths(ind[integer(0)]), float(0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.totalOfNeighborStrengths(ind[0]), 18.0)) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.totalOfNeighborStrengths(ind[5]), 18.0)) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.totalOfNeighborStrengths(ind[9]), 18.0)) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.totalOfNeighborStrengths(ind[c(0, 5, 9)]), c(18.0, 18.0, 18.0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		}
		
		// Test InteractionType – (void)unevaluate(void)
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.evaluate(p1); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.unevaluate(); stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.distance(ind[0], ind[2]); stop(); }", "must be evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.interactionDistance(ind[0], ind[2]); stop(); }", "must be evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.distanceFromPoint(c(1.0, 0.0), ind[0]); stop(); }", "must be evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.drawByStrength(ind[0]); stop(); }", "must be evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.nearestNeighbors(ind[8], 1); stop(); }", "must be evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.nearestInteractingNeighbors(ind[8], 1); stop(); }", "must be evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.interactingNeighborCount(ind[8]); stop(); }", "must be evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.nearestNeighborsOfPoint(19.0, p1, 1); stop(); }", "must be evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.strength(ind[0], ind[2]); stop(); }", "must be evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.totalOfNeighborStrengths(ind[0]); stop(); }", "must be evaluated", __LINE__);
	}
	
	// *** 3D with y and z zero
	std::string gen1_setup_i1xyz_pop("initialize() { initializeSLiMOptions(dimensionality='xyz'); " + sex_string + "initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'xyz', maxDistance=" + p_max_distance + ", sexSegregation='" + p_sex_segregation + "'); } 1 early() { sim.addSubpop('p1', 10); p1.individuals.x = c(-10.0, 0, 1, 2, 3, 5, 7, 8, 20, 25); p1.individuals.y = 0; p1.individuals.z = 0; i1.evaluate(p1); ind = p1.individuals; ");
	
	// Test InteractionType – (float)distance(object<Individual> individuals1, [No<Individual> individuals2 = NULL])
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (i1.distance(ind[0], ind[2]) == 11.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.distance(ind[2], ind[0:1]), c(11.0, 1.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.distance(ind[0], ind[2:3]), c(11.0, 12.0))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "if (i1.distance(ind[0:1], ind[2:3]) == 11.0) stop(); }", "must be a singleton", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.distance(ind[5], ind[c(0, 5, 9, 8, 1)]), c(15.0, 0, 20, 15, 5))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.distance(ind[1], ind[integer(0)]), float(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.distance(ind[5]), c(15.0, 5, 4, 3, 2, 0, 2, 3, 15, 20))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.distance(ind[5], NULL), c(15.0, 5, 4, 3, 2, 0, 2, 3, 15, 20))) stop(); }", __LINE__);
	
	// Test InteractionType – (float)interactionDistance(object<Individual>$ receiver, [No<Individual> exerters = NULL])
	if (!sex_seg_on)
	{
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (i1.interactionDistance(ind[0], ind[2]) == 11.0) stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "if (identical(i1.interactionDistance(ind[0:1], ind[2]), c(11.0, 1.0))) stop(); }", "must be a singleton", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.interactionDistance(ind[0], ind[2:3]), c(11.0, 12.0))) stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "if (i1.interactionDistance(ind[0:1], ind[2:3]) == 11.0) stop(); }", "must be a singleton", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.interactionDistance(ind[5], ind[c(0, 5, 9, 8, 1)]), c(15.0, INF, 20, 15, 5))) stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "if (identical(i1.interactionDistance(ind[integer(0)], ind[8]), float(0))) stop(); }", "must be a singleton", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.interactionDistance(ind[1], ind[integer(0)]), float(0))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.interactionDistance(ind[5]), c(15.0, 5, 4, 3, 2, INF, 2, 3, 15, 20))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.interactionDistance(ind[5], NULL), c(15.0, 5, 4, 3, 2, INF, 2, 3, 15, 20))) stop(); }", __LINE__);
	}
	else
	{
		// comprehensively testing all the different sex-seg cases is complicated, but we can at least test the two branches of the code against each other
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.interactionDistance(ind[5]), i1.interactionDistance(ind[5], NULL))) stop(); }", __LINE__);
	}
	
	// Test InteractionType – (float)distanceFromPoint(float point, object<Individual> individuals1)
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (i1.distanceFromPoint(c(1.0, 0.0, 0.0), ind[0]) == 11.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.distanceFromPoint(c(1.0, 0.0, 0.0), ind[0:1]), c(11.0, 1.0))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "if (i1.distanceFromPoint(1.0, ind[0:1]) == 11.0) stop(); }", "point is of length equal to", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.distanceFromPoint(c(5.0, 0.0, 0.0), ind[c(0, 5, 9, 8, 1)]), c(15.0, 0, 20, 15, 5))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.distanceFromPoint(c(8.0, 0.0, 0.0), ind[integer(0)]), float(0))) stop(); }", __LINE__);
	
	// Test InteractionType – (object<Individual>)drawByStrength(object<Individual>$ individual, [integer$ count = 1])
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.drawByStrength(ind[0]); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.drawByStrength(ind[0], 1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.drawByStrength(ind[0], 50); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.drawByStrength(ind[0], 0), ind[integer(0)])) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.drawByStrength(ind[0], -1); stop(); }", "requires count >= 0", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.drawByStrength(ind[0]); stop(); } interaction(i1) { return 2.0; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.drawByStrength(ind[0], 1); stop(); } interaction(i1) { return 2.0; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.drawByStrength(ind[0], 50); stop(); } interaction(i1) { return 2.0; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.drawByStrength(ind[0], 0), ind[integer(0)])) stop(); } interaction(i1) { return 2.0; }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.drawByStrength(ind[0], -1); stop(); } interaction(i1) { return 2.0; }", "requires count >= 0", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.drawByStrength(ind[0]); stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.drawByStrength(ind[0], 1); stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.drawByStrength(ind[0], 50); stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.drawByStrength(ind[0], 0), ind[integer(0)])) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.drawByStrength(ind[0], -1); stop(); } interaction(i1) { return strength * 2.0; }", "requires count >= 0", __LINE__);
	
	// Test InteractionType – (void)evaluate(io<Subpopulation> subpops)
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.evaluate(p1); i1.evaluate(p1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.evaluate(p1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.evaluate(1); stop(); }", __LINE__);
	
	// Test InteractionType – (object<Individual>)nearestNeighbors(object<Individual>$ individual, [integer$ count = 1])
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "if (identical(i1.nearestNeighbors(ind[8], -1), ind[integer(0)])) stop(); }", "requires count >= 0", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.nearestNeighbors(ind[8], 0), ind[integer(0)])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.nearestNeighbors(ind[8], 1), ind[9])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(sortBy(i1.nearestNeighbors(ind[8], 3), 'index'), ind[c(6,7,9)])) stop(); }", __LINE__);
	for (int ind_index = 0; ind_index < 10; ++ind_index)
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (size(i1.nearestNeighbors(ind[" + std::to_string(ind_index) + "], 100)) == i1.neighborCount(ind[" + std::to_string(ind_index) + "])) stop(); }", __LINE__);
	for (int ind_index = 0; ind_index < 10; ++ind_index)
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (size(i1.nearestNeighbors(ind[" + std::to_string(ind_index) + "], 100)) + 1 == i1.neighborCountOfPoint(ind[" + std::to_string(ind_index) + "].xyz, p1)) stop(); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_i1xyz_pop + "nn = i1.nearestNeighbors(ind, 100, returnDict=T); nc = i1.neighborCount(ind); for (i in 0:9) if (size(nn.getValue(i)) != nc[i]) stop(); }", __LINE__);
	
	// Test InteractionType – (object<Individual>)nearestInteractingNeighbors(object<Individual>$ individual, [integer$ count = 1])
	// Test InteractionType – (object<Individual>)interactingNeighborCount(object<Individual>$ individual, [integer$ count = 1])
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "if (identical(i1.nearestInteractingNeighbors(ind[8], -1), ind[integer(0)])) stop(); }", "requires count >= 0", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.nearestInteractingNeighbors(ind[8], 0), ind[integer(0)])) stop(); }", __LINE__);
	for (int ind_index = 0; ind_index < 10; ++ind_index)
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (size(i1.nearestInteractingNeighbors(ind[" + std::to_string(ind_index) + "], 100)) == i1.interactingNeighborCount(ind[" + std::to_string(ind_index) + "])) stop(); }", __LINE__);
	for (int ind_index = 0; ind_index < 10; ++ind_index)
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (size(i1.nearestInteractingNeighbors(ind[" + std::to_string(ind_index) + "], 100)) == sum(isFinite(i1.interactionDistance(ind[" + std::to_string(ind_index) + "])))) stop(); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_i1xyz_pop + "nn = i1.nearestInteractingNeighbors(ind, 100, returnDict=T); nc = i1.interactingNeighborCount(ind); for (i in 0:9) if (size(nn.getValue(i)) != nc[i]) stop(); }", __LINE__);
	
	// Test InteractionType – (object<Individual>)nearestNeighborsOfPoint(float point, io<Subpopulation>$ subpop, [integer$ count = 1])
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "if (identical(i1.nearestNeighborsOfPoint(c(5.0, 0.0, 0.0), p1, -1), ind[integer(0)])) stop(); }", "requires count >= 0", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.nearestNeighborsOfPoint(c(5.0, 0.0, 0.0), p1, 0), ind[integer(0)])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.nearestNeighborsOfPoint(c(19.0, 0.0, 0.0), p1, 1), ind[8])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(sortBy(i1.nearestNeighborsOfPoint(c(19.0, 0.0, 0.0), p1, 3), 'index'), ind[c(7,8,9)])) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "if (identical(i1.nearestNeighborsOfPoint(c(5.0, 0.0, 0.0), 1, -1), ind[integer(0)])) stop(); }", "requires count >= 0", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.nearestNeighborsOfPoint(c(5.0, 0.0, 0.0), 1, 0), ind[integer(0)])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.nearestNeighborsOfPoint(c(19.0, 0.0, 0.0), 1, 1), ind[8])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(sortBy(i1.nearestNeighborsOfPoint(c(19.0, 0.0, 0.0), 1, 3), 'index'), ind[c(7,8,9)])) stop(); }", __LINE__);
	
	// Test InteractionType – (void)setInteractionFunction(string$ functionType, ...)
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.setInteractionFunction('q', 10.0); i1.evaluate(p1); stop(); }", "while the interaction is being evaluated", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.setInteractionFunction('q', 10.0); i1.evaluate(p1); stop(); }", "functionType 'q' must be", __LINE__);
	if (max_dist_on)
	{
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.setInteractionFunction('f', 5.0); i1.evaluate(p1); stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.setInteractionFunction('f'); i1.evaluate(p1); stop(); }", "requires exactly", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.setInteractionFunction('f', 5.0, 2.0); i1.evaluate(p1); stop(); }", "requires exactly", __LINE__);
	}
	else
		SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.setInteractionFunction('f', 5.0); i1.evaluate(p1); stop(); }", "finite maximum interaction distance", __LINE__);
	
	if (!max_dist_on)
		SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.setInteractionFunction('l', 5.0); i1.evaluate(p1); stop(); }", "finite maximum interaction distance", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('l', 5.0); i1.evaluate(p1); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('l'); i1.evaluate(p1); stop(); }", "requires exactly", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('l', 5.0, 2.0); i1.evaluate(p1); stop(); }", "requires exactly", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('e', 5.0, 1.0); i1.evaluate(p1); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('e', 5.0); i1.evaluate(p1); stop(); }", "requires exactly", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('e', 5.0, 2.0, 1.0); i1.evaluate(p1); stop(); }", "requires exactly", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('n', 5.0, 1.0); i1.evaluate(p1); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('n', 5.0); i1.evaluate(p1); stop(); }", "requires exactly", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('n', 5.0, 2.0, 1.0); i1.evaluate(p1); stop(); }", "requires exactly", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('c', 5.0, 1.0); i1.evaluate(p1); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('c', 5.0); i1.evaluate(p1); stop(); }", "requires exactly", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('c', 5.0, 2.0, 1.0); i1.evaluate(p1); stop(); }", "requires exactly", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('n', 5.0, -1.0); stop(); }", "must have a standard deviation parameter >= 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('c', 5.0, 0.0); stop(); }", "must have a scale parameter > 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('c', 5.0, -1.0); stop(); }", "must have a scale parameter > 0", __LINE__);
	
	// Test InteractionType – (float)strength(object<Individual> individuals1, [No<Individual> individuals2 = NULL])
	if (!sex_seg_on)
	{
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (i1.strength(ind[0], ind[2]) == 1.0) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.strength(ind[0], ind[2:3]), c(1.0, 1.0))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.strength(ind[5], ind[c(0, 5, 9, 8, 1)]), c(1.0, 0.0, 1.0, 1.0, 1.0))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.strength(ind[1], ind[integer(0)]), float(0))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.strength(ind[5]), c(1.0, 1.0, 1.0, 1.0, 1.0, 0.0, 1.0, 1.0, 1.0, 1.0))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.strength(ind[5], NULL), c(1.0, 1.0, 1.0, 1.0, 1.0, 0.0, 1.0, 1.0, 1.0, 1.0))) stop(); }", __LINE__);

		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (i1.strength(ind[0], ind[2]) == 2.0) stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.strength(ind[0], ind[2:3]), c(2.0, 2.0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.strength(ind[5], ind[c(0, 5, 9, 8, 1)]), c(2.0, 0.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.strength(ind[1], ind[integer(0)]), float(0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.strength(ind[5]), c(2.0, 2.0, 2.0, 2.0, 2.0, 0.0, 2.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.strength(ind[5], NULL), c(2.0, 2.0, 2.0, 2.0, 2.0, 0.0, 2.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);

		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (i1.strength(ind[0], ind[2]) == 2.0) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.strength(ind[0], ind[2:3]), c(2.0, 2.0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.strength(ind[5], ind[c(0, 5, 9, 8, 1)]), c(2.0, 0.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.strength(ind[1], ind[integer(0)]), float(0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.strength(ind[5]), c(2.0, 2.0, 2.0, 2.0, 2.0, 0.0, 2.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.strength(ind[5], NULL), c(2.0, 2.0, 2.0, 2.0, 2.0, 0.0, 2.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
	}
	
	// Test InteractionType – (float)totalOfNeighborStrengths(object<Individual> individuals)
	if (!sex_seg_on)
	{
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.totalOfNeighborStrengths(ind[integer(0)]), float(0))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.totalOfNeighborStrengths(ind[0]), 9.0)) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.totalOfNeighborStrengths(ind[5]), 9.0)) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.totalOfNeighborStrengths(ind[9]), 9.0)) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.totalOfNeighborStrengths(ind[c(0, 5, 9)]), c(9.0, 9.0, 9.0))) stop(); }", __LINE__);

		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.totalOfNeighborStrengths(ind[integer(0)]), float(0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.totalOfNeighborStrengths(ind[0]), 18.0)) stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.totalOfNeighborStrengths(ind[5]), 18.0)) stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.totalOfNeighborStrengths(ind[9]), 18.0)) stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.totalOfNeighborStrengths(ind[c(0, 5, 9)]), c(18.0, 18.0, 18.0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);

		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.totalOfNeighborStrengths(ind[integer(0)]), float(0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.totalOfNeighborStrengths(ind[0]), 18.0)) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.totalOfNeighborStrengths(ind[5]), 18.0)) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.totalOfNeighborStrengths(ind[9]), 18.0)) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.totalOfNeighborStrengths(ind[c(0, 5, 9)]), c(18.0, 18.0, 18.0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
	}
	
	// Test InteractionType – (void)unevaluate(void)
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.evaluate(p1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.unevaluate(); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.distance(ind[0], ind[2]); stop(); }", "must be evaluated", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.interactionDistance(ind[0], ind[2]); stop(); }", "must be evaluated", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.distanceFromPoint(c(1.0, 0.0, 0.0), ind[0]); stop(); }", "must be evaluated", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.drawByStrength(ind[0]); stop(); }", "must be evaluated", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.nearestNeighbors(ind[8], 1); stop(); }", "must be evaluated", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.nearestInteractingNeighbors(ind[8], 1); stop(); }", "must be evaluated", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.interactingNeighborCount(ind[8]); stop(); }", "must be evaluated", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.nearestNeighborsOfPoint(19.0, p1, 1); stop(); }", "must be evaluated", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.strength(ind[0], ind[2]); stop(); }", "must be evaluated", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.totalOfNeighborStrengths(ind[0]); stop(); }", "must be evaluated", __LINE__);
	
	// *** 3D with full 3D coordinates; we skip the error-testing here since it's the same as before
	std::string gen1_setup_i1xyz_pop_full("initialize() { initializeSLiMOptions(dimensionality='xyz'); " + sex_string + "initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'xyz', maxDistance=" + p_max_distance + ", sexSegregation='" + p_sex_segregation + "'); } 1 early() { sim.addSubpop('p1', 10); p1.individuals.x = c(-10.0, 0, 1, 2, 3, 5, 7, 8, 20, 25); p1.individuals.y = c(12.0, 3, -2, 10, 8, 72, 0, -5, -13, 7); p1.individuals.z = c(0.0, 5, 9, -6, 6, -16, 2, 1, -1, 8); i1.evaluate(p1); ind = p1.individuals; ");
	
	// Test InteractionType – (float)distance(object<Individual> individuals1, [No<Individual> individuals2 = NULL])
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (i1.distance(ind[0], ind[2]) == sqrt(11^2 + 14^2 + 9^2)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(i1.distance(ind[2], ind[0:1]), c(sqrt(11^2 + 14^2 + 9^2), sqrt(1^2 + 5^2 + 4^2)))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(i1.distance(ind[0], ind[2:3]), c(sqrt(11^2 + 14^2 + 9^2), sqrt(12^2 + 2^2 + 6^2)))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (all(i1.distance(ind[5]) - c(63.882705, 72.2979, 78.2112, 62.8728, 67.7052,  0.0, 74.2428, 78.9113, 87.6070, 72.1179) < 0.001)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (all(i1.distance(ind[5], NULL) - c(63.882705, 72.2979, 78.2112, 62.8728, 67.7052,  0.0, 74.2428, 78.9113, 87.6070, 72.1179) < 0.001)) stop(); }", __LINE__);
	
	// Test InteractionType – (float)interactionDistance(object<Individual>$ receiver, [No<Individual> exerters = NULL])
	if (!sex_seg_on)
	{
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (i1.interactionDistance(ind[0], ind[2]) - sqrt(11^2 + 14^2 + 9^2) < 0.001) stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xyz_pop_full + "if (identical(i1.interactionDistance(ind[0:1], ind[2]), c(sqrt(11^2 + 14^2 + 9^2), sqrt(1^2 + 5^2 + 4^2)))) stop(); }", "must be a singleton", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (all(i1.interactionDistance(ind[0], ind[2:3]) - c(sqrt(11^2 + 14^2 + 9^2), sqrt(12^2 + 2^2 + 6^2)) < 0.001)) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (all(i1.interactionDistance(ind[5])[c(0:4,6:9)] - c(63.882705, 72.2979, 78.2112, 62.8728, 67.7052, 74.2428, 78.9113, 87.6070, 72.1179) < 0.001)) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (all(i1.interactionDistance(ind[5], NULL)[c(0:4,6:9)] - c(63.882705, 72.2979, 78.2112, 62.8728, 67.7052, 74.2428, 78.9113, 87.6070, 72.1179) < 0.001)) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (isInfinite(i1.interactionDistance(ind[5])[5])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (isInfinite(i1.interactionDistance(ind[5], NULL)[5])) stop(); }", __LINE__);
	}
	else
	{
		// comprehensively testing all the different sex-seg cases is complicated, but we can at least test the two branches of the code against each other
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(i1.interactionDistance(ind[5]), i1.interactionDistance(ind[5], NULL))) stop(); }", __LINE__);
	}
	
	// Test InteractionType – (float)distanceFromPoint(float point, object<Individual> individuals1)
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (i1.distanceFromPoint(c(-7.0, 12.0, 4.0), ind[0]) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(i1.distanceFromPoint(c(-7.0, 12.0, 4.0), ind[0:1]), c(5.0, sqrt(7^2 + 9^2 + 1^2)))) stop(); }", __LINE__);
	
	// Test InteractionType – (object<Individual>)drawByStrength(object<Individual>$ individual, [integer$ count = 1])
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.drawByStrength(ind[0]); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.drawByStrength(ind[0], 1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.drawByStrength(ind[0], 50); stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.drawByStrength(ind[0]); stop(); } interaction(i1) { return 2.0; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.drawByStrength(ind[0], 1); stop(); } interaction(i1) { return 2.0; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.drawByStrength(ind[0], 50); stop(); } interaction(i1) { return 2.0; }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.drawByStrength(ind[0]); stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.drawByStrength(ind[0], 1); stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.drawByStrength(ind[0], 50); stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
	
	// Test InteractionType – (void)evaluate(io<Subpopulation> subpops)
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.evaluate(p1); i1.evaluate(p1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.evaluate(p1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.evaluate(1); stop(); }", __LINE__);
	
	// Test InteractionType – (object<Individual>)nearestNeighbors(object<Individual>$ individual, [integer$ count = 1])
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(i1.nearestNeighbors(ind[8], 1), ind[7])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(sortBy(i1.nearestNeighbors(ind[8], 3), 'index'), ind[c(6,7,9)])) stop(); }", __LINE__);
	for (int ind_index = 0; ind_index < 10; ++ind_index)
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (size(i1.nearestNeighbors(ind[" + std::to_string(ind_index) + "], 100)) == i1.neighborCount(ind[" + std::to_string(ind_index) + "])) stop(); }", __LINE__);
	for (int ind_index = 0; ind_index < 10; ++ind_index)
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (size(i1.nearestNeighbors(ind[" + std::to_string(ind_index) + "], 100)) + 1 == i1.neighborCountOfPoint(ind[" + std::to_string(ind_index) + "].xyz, p1)) stop(); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_i1xyz_pop_full + "nn = i1.nearestNeighbors(ind, 100, returnDict=T); nc = i1.neighborCount(ind); for (i in 0:9) if (size(nn.getValue(i)) != nc[i]) stop(); }", __LINE__);
	
	// Test InteractionType – (object<Individual>)nearestInteractingNeighbors(object<Individual>$ individual, [integer$ count = 1])
	// Test InteractionType – (object<Individual>)interactingNeighborCount(object<Individual>$ individual, [integer$ count = 1])
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop_full + "if (identical(i1.nearestInteractingNeighbors(ind[8], -1), ind[integer(0)])) stop(); }", "requires count >= 0", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(i1.nearestInteractingNeighbors(ind[8], 0), ind[integer(0)])) stop(); }", __LINE__);
	for (int ind_index = 0; ind_index < 10; ++ind_index)
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (size(i1.nearestInteractingNeighbors(ind[" + std::to_string(ind_index) + "], 100)) == i1.interactingNeighborCount(ind[" + std::to_string(ind_index) + "])) stop(); }", __LINE__);
	for (int ind_index = 0; ind_index < 10; ++ind_index)
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (size(i1.nearestInteractingNeighbors(ind[" + std::to_string(ind_index) + "], 100)) == sum(isFinite(i1.interactionDistance(ind[" + std::to_string(ind_index) + "])))) stop(); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_i1xyz_pop_full + "nn = i1.nearestInteractingNeighbors(ind, 100, returnDict=T); nc = i1.interactingNeighborCount(ind); for (i in 0:9) if (size(nn.getValue(i)) != nc[i]) stop(); }", __LINE__);
	
	// Test InteractionType – (object<Individual>)nearestNeighborsOfPoint(float point, io<Subpopulation>$ subpop, [integer$ count = 1])
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(i1.nearestNeighborsOfPoint(c(-7.0, 12.0, 4.0), p1, 1), ind[0])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(i1.nearestNeighborsOfPoint(c(7.0, 3.0, 12.0), p1, 1), ind[2])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(sortBy(i1.nearestNeighborsOfPoint(c(19.0, -4.0, -2.0), p1, 3), 'index'), ind[c(6,7,8)])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(sortBy(i1.nearestNeighborsOfPoint(c(7.0, 3.0, 12.0), p1, 3), 'index'), ind[c(1,2,4)])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(i1.nearestNeighborsOfPoint(c(-7.0, 12.0, 4.0), 1, 1), ind[0])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(i1.nearestNeighborsOfPoint(c(7.0, 3.0, 12.0), 1, 1), ind[2])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(sortBy(i1.nearestNeighborsOfPoint(c(19.0, -4.0, -2.0), 1, 3), 'index'), ind[c(6,7,8)])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(sortBy(i1.nearestNeighborsOfPoint(c(7.0, 3.0, 12.0), 1, 3), 'index'), ind[c(1,2,4)])) stop(); }", __LINE__);
	
	// Test InteractionType – (void)setInteractionFunction(string$ functionType, ...)
	if (max_dist_on)
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.unevaluate(); i1.setInteractionFunction('f', 5.0); i1.evaluate(p1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('l', 5.0); i1.evaluate(p1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('e', 5.0, 1.0); i1.evaluate(p1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('n', 5.0, 1.0); i1.evaluate(p1); stop(); }", __LINE__);
	
	// Test InteractionType – (float)strength(object<Individual> individuals1, [No<Individual> individuals2 = NULL])
	if (!sex_seg_on)
	{
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (i1.strength(ind[0], ind[2]) == 1.0) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(i1.strength(ind[0], ind[2:3]), c(1.0, 1.0))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(i1.strength(ind[5]), c(1.0, 1.0, 1.0, 1.0, 1.0, 0.0, 1.0, 1.0, 1.0, 1.0))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(i1.strength(ind[5], NULL), c(1.0, 1.0, 1.0, 1.0, 1.0, 0.0, 1.0, 1.0, 1.0, 1.0))) stop(); }", __LINE__);

		SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (i1.strength(ind[0], ind[2]) == 2.0) stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(i1.strength(ind[0], ind[2:3]), c(2.0, 2.0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(i1.strength(ind[5]), c(2.0, 2.0, 2.0, 2.0, 2.0, 0.0, 2.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(i1.strength(ind[5], NULL), c(2.0, 2.0, 2.0, 2.0, 2.0, 0.0, 2.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);

		SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (i1.strength(ind[0], ind[2]) == 2.0) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(i1.strength(ind[0], ind[2:3]), c(2.0, 2.0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(i1.strength(ind[5]), c(2.0, 2.0, 2.0, 2.0, 2.0, 0.0, 2.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(i1.strength(ind[5], NULL), c(2.0, 2.0, 2.0, 2.0, 2.0, 0.0, 2.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
	}
	
	// Test InteractionType – (float)totalOfNeighborStrengths(object<Individual> individuals)
	if (!sex_seg_on)
	{
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(i1.totalOfNeighborStrengths(ind[0]), 9.0)) stop(); }", __LINE__);
		
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(i1.totalOfNeighborStrengths(ind[0]), 18.0)) stop(); } interaction(i1) { return 2.0; }", __LINE__);
		
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(i1.totalOfNeighborStrengths(ind[0]), 18.0)) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
	}
	
	// Test InteractionType – (void)unevaluate(void)
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.unevaluate(); i1.evaluate(p1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.unevaluate(); i1.unevaluate(); stop(); }", __LINE__);
	
	// *** Test all spatial queries with (1) empty receivers vector, (2) empty exerter subpop, (3) no qualified receivers, (4) no qualified exerters, all for (a) returnDict=F vs. (b) returnDict=T
	// We do this only for 2D at present; the logic is generally shared, for this level of functionality.  We use a nonWF model so we can have empty subpopulations; p1 has 10 male individuals,
	// p2 has 10 female individuals, and p3 is empty.  We randomize positions each time, unlike the tests above.  We don't look at results here at all; the goal is just to exercise the code paths
	// and make sure nothing crashes.
	for (int periodic = 0; periodic <= 1; ++periodic)
	{
		if ((periodic == 1) && (!max_dist_on))
			continue;
		
		std::string periodic_str = (periodic ? ", periodicity='xy'" : "");
		std::string max_distance = (periodic ? " 0.45 " : p_max_distance);
		std::string gen1_setup_i1xy_edge_cases;
		
		if (p_sex_enabled)
			gen1_setup_i1xy_edge_cases = std::string("initialize() { initializeSLiMModelType('nonWF'); initializeSLiMOptions(dimensionality='xy'" + periodic_str + "); " + sex_string + "initializeInteractionType('i1', 'xy', maxDistance=" + max_distance + ", sexSegregation='" + p_sex_segregation + "'); } 1 early() { sim.addSubpop('p1', 10, sexRatio=1.0); p1.individuals.setSpatialPosition(p1.pointUniform(10)); sim.addSubpop('p2', 10, sexRatio=0.0); p2.individuals.setSpatialPosition(p2.pointUniform(10)); sim.addSubpop('p3', 0); i1.evaluate(c(p1,p2,p3)); ind1 = p1.individuals; ind2 = p2.individuals; ind3 = p3.individuals; ");
		else
			gen1_setup_i1xy_edge_cases = std::string("initialize() { initializeSLiMModelType('nonWF'); initializeSLiMOptions(dimensionality='xy'" + periodic_str + "); initializeInteractionType('i1', 'xy', maxDistance=" + max_distance + "); } 1 early() { sim.addSubpop('p1', 10); p1.individuals.setSpatialPosition(p1.pointUniform(10)); sim.addSubpop('p2', 10); p2.individuals.setSpatialPosition(p2.pointUniform(10)); sim.addSubpop('p3', 0); i1.evaluate(c(p1,p2,p3)); ind1 = p1.individuals; ind2 = p2.individuals; ind3 = p3.individuals; ");
		
		// (float)distance(object<Individual>$ receiver, [No<Individual> exerters = NULL])
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.distance(ind1[0], ind2); stop(); }", __LINE__);										// sex-segregation effects
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.distance(ind1[0], ind3); stop(); }", __LINE__);										// empty exerter subpop
		
		// (float)distanceFromPoint(float point, object<Individual> exerters)
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.distanceFromPoint(ind1[0].xy, ind2); stop(); }", __LINE__);							// sex-segregation effects
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.distanceFromPoint(ind1[0].xy, ind3); stop(); }", __LINE__);							// empty exerter subpop
		
		// (object)drawByStrength(object<Individual> receiver, [integer$ count = 1], [No<Subpopulation>$ exerterSubpop = NULL], [logical$ returnDict = F])
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.drawByStrength(ind1[0], 0, p2); stop(); }", __LINE__);								// sex-segregation effects
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.drawByStrength(ind1[0], 1, p2); stop(); }", __LINE__);								// sex-segregation effects
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.drawByStrength(ind1[0], 100, p2); stop(); }", __LINE__);								// sex-segregation effects
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.drawByStrength(ind1[0], 0, p3); stop(); }", __LINE__);								// empty exerter subpop
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.drawByStrength(ind1[0], 1, p3); stop(); }", __LINE__);								// empty exerter subpop
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.drawByStrength(ind1[0], 100, p3); stop(); }", __LINE__);								// empty exerter subpop
		
		// drawByStrength(, returnDict=T)
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.drawByStrength(ind3, 0, p2, returnDict=T); stop(); }", __LINE__);						// empty receiver
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.drawByStrength(ind3, 1, p2, returnDict=T); stop(); }", __LINE__);						// empty receiver
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.drawByStrength(ind3, 100, p2, returnDict=T); stop(); }", __LINE__);					// empty receiver
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.drawByStrength(ind1[0], 0, p2, returnDict=T); stop(); }", __LINE__);					// sex-segregation effects
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.drawByStrength(ind1[0:1], 0, p2, returnDict=T); stop(); }", __LINE__);				// sex-segregation effects
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.drawByStrength(ind1[0], 1, p2, returnDict=T); stop(); }", __LINE__);					// sex-segregation effects
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.drawByStrength(ind1[0:1], 1, p2, returnDict=T); stop(); }", __LINE__);				// sex-segregation effects
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.drawByStrength(ind1[0], 100, p2, returnDict=T); stop(); }", __LINE__);				// sex-segregation effects
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.drawByStrength(ind1[0:1], 100, p2, returnDict=T); stop(); }", __LINE__);				// sex-segregation effects
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.drawByStrength(ind3, 0, p3, returnDict=T); stop(); }", __LINE__);						// empty receiver, empty exerter subpop
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.drawByStrength(ind3, 1, p3, returnDict=T); stop(); }", __LINE__);						// empty receiver, empty exerter subpop
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.drawByStrength(ind3, 1000, p3, returnDict=T); stop(); }", __LINE__);					// empty receiver, empty exerter subpop
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.drawByStrength(ind1[0], 0, p3, returnDict=T); stop(); }", __LINE__);					// empty exerter subpop
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.drawByStrength(ind1[0:1], 0, p3, returnDict=T); stop(); }", __LINE__);				// empty exerter subpop
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.drawByStrength(ind1[0], 1, p3, returnDict=T); stop(); }", __LINE__);					// empty exerter subpop
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.drawByStrength(ind1[0:1], 1, p3, returnDict=T); stop(); }", __LINE__);				// empty exerter subpop
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.drawByStrength(ind1[0], 100, p3, returnDict=T); stop(); }", __LINE__);				// empty exerter subpop
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.drawByStrength(ind1[0:1], 100, p3, returnDict=T); stop(); }", __LINE__);				// empty exerter subpop
		
		// (integer)interactingNeighborCount(object<Individual> receivers, [No<Subpopulation>$ exerterSubpop = NULL])
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.interactingNeighborCount(ind3, p2); stop(); }", __LINE__);							// empty receiver
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.interactingNeighborCount(ind1[0], p2); stop(); }", __LINE__);							// sex-segregation effects
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.interactingNeighborCount(ind1[0:1], p2); stop(); }", __LINE__);						// sex-segregation effects
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.interactingNeighborCount(ind3, p3); stop(); }", __LINE__);							// empty receiver, empty exerter subpop
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.interactingNeighborCount(ind1[0], p3); stop(); }", __LINE__);							// empty exerter subpop
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.interactingNeighborCount(ind1[0:1], p3); stop(); }", __LINE__);						// empty exerter subpop
		
		// (float)interactionDistance(object<Individual>$ receiver, [No<Individual> exerters = NULL])
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.interactionDistance(ind1[0], ind2); stop(); }", __LINE__);							// sex-segregation effects
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.interactionDistance(ind1[0], ind3); stop(); }", __LINE__);							// empty exerter subpop
		
		// (float)localPopulationDensity(object<Individual> receivers, [No<Subpopulation>$ exerterSubpop = NULL])
		// we do all of these in a single model because of the large first-time overhead
		if (max_dist_on)
		{
			std::string gen1_setup_i1xy_edge_cases_MAX;		// clippedIntegral() requires a short maximum distance; we use 0.45
			
			if (p_sex_enabled)
				gen1_setup_i1xy_edge_cases_MAX = std::string("initialize() { initializeSLiMModelType('nonWF'); initializeSLiMOptions(dimensionality='xy'); " + sex_string + "initializeInteractionType('i1', 'xy', maxDistance=0.45, sexSegregation='" + p_sex_segregation + "'); } 1 early() { sim.addSubpop('p1', 10, sexRatio=1.0); p1.individuals.setSpatialPosition(p1.pointUniform(10)); sim.addSubpop('p2', 10, sexRatio=0.0); p2.individuals.setSpatialPosition(p2.pointUniform(10)); sim.addSubpop('p3', 0); i1.evaluate(c(p1,p2,p3)); ind1 = p1.individuals; ind2 = p2.individuals; ind3 = p3.individuals; ");
			else
				gen1_setup_i1xy_edge_cases_MAX = std::string("initialize() { initializeSLiMModelType('nonWF'); initializeSLiMOptions(dimensionality='xy'); initializeInteractionType('i1', 'xy', maxDistance=0.45); } 1 early() { sim.addSubpop('p1', 10); p1.individuals.setSpatialPosition(p1.pointUniform(10)); sim.addSubpop('p2', 10); p2.individuals.setSpatialPosition(p2.pointUniform(10)); sim.addSubpop('p3', 0); i1.evaluate(c(p1,p2,p3)); ind1 = p1.individuals; ind2 = p2.individuals; ind3 = p3.individuals; ");
			
			SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases_MAX + "i1.localPopulationDensity(ind3, p2); "											// empty receiver
								 + "i1.localPopulationDensity(ind1[0], p2); "																		// sex-segregation effects
								 + "i1.localPopulationDensity(ind1[0:1], p2); "																		// sex-segregation effects
								 + "i1.localPopulationDensity(ind3, p3); "																			// empty receiver, empty exerter subpop
								 + "i1.localPopulationDensity(ind1[0], p3); "																		// empty exerter subpop
								 + "i1.localPopulationDensity(ind1[0:1], p3); stop(); }", __LINE__);												// empty exerter subpop
		}
		
		// (object)nearestInteractingNeighbors(object<Individual> receiver, [integer$ count = 1], [No<Subpopulation>$ exerterSubpop = NULL], [logical$ returnDict = F])
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestInteractingNeighbors(ind1[0], 0, p2); stop(); }", __LINE__);					// sex-segregation effects
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestInteractingNeighbors(ind1[0], 1, p2); stop(); }", __LINE__);					// sex-segregation effects
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestInteractingNeighbors(ind1[0], 100, p2); stop(); }", __LINE__);					// sex-segregation effects
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestInteractingNeighbors(ind1[0], 0, p3); stop(); }", __LINE__);					// empty exerter subpop
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestInteractingNeighbors(ind1[0], 1, p3); stop(); }", __LINE__);					// empty exerter subpop
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestInteractingNeighbors(ind1[0], 100, p3); stop(); }", __LINE__);					// empty exerter subpop
		
		// nearestInteractingNeighbors(, returnDict=T)
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestInteractingNeighbors(ind3, 0, p2, returnDict=T); stop(); }", __LINE__);		// empty receiver
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestInteractingNeighbors(ind3, 1, p2, returnDict=T); stop(); }", __LINE__);		// empty receiver
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestInteractingNeighbors(ind3, 100, p2, returnDict=T); stop(); }", __LINE__);		// empty receiver
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestInteractingNeighbors(ind1[0], 0, p2, returnDict=T); stop(); }", __LINE__);		// sex-segregation effects
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestInteractingNeighbors(ind1[0:1], 0, p2, returnDict=T); stop(); }", __LINE__);	// sex-segregation effects
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestInteractingNeighbors(ind1[0], 1, p2, returnDict=T); stop(); }", __LINE__);		// sex-segregation effects
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestInteractingNeighbors(ind1[0:1], 1, p2, returnDict=T); stop(); }", __LINE__);	// sex-segregation effects
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestInteractingNeighbors(ind1[0], 100, p2, returnDict=T); stop(); }", __LINE__);	// sex-segregation effects
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestInteractingNeighbors(ind1[0:1], 100, p2, returnDict=T); stop(); }", __LINE__);	// sex-segregation effects
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestInteractingNeighbors(ind3, 0, p3, returnDict=T); stop(); }", __LINE__);		// empty receiver, empty exerter subpop
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestInteractingNeighbors(ind3, 1, p3, returnDict=T); stop(); }", __LINE__);		// empty receiver, empty exerter subpop
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestInteractingNeighbors(ind3, 1000, p3, returnDict=T); stop(); }", __LINE__);		// empty receiver, empty exerter subpop
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestInteractingNeighbors(ind1[0], 0, p3, returnDict=T); stop(); }", __LINE__);		// empty exerter subpop
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestInteractingNeighbors(ind1[0:1], 0, p3, returnDict=T); stop(); }", __LINE__);	// empty exerter subpop
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestInteractingNeighbors(ind1[0], 1, p3, returnDict=T); stop(); }", __LINE__);		// empty exerter subpop
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestInteractingNeighbors(ind1[0:1], 1, p3, returnDict=T); stop(); }", __LINE__);	// empty exerter subpop
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestInteractingNeighbors(ind1[0], 100, p3, returnDict=T); stop(); }", __LINE__);	// empty exerter subpop
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestInteractingNeighbors(ind1[0:1], 100, p3, returnDict=T); stop(); }", __LINE__);	// empty exerter subpop
		
		// (object)nearestNeighbors(object<Individual> receiver, [integer$ count = 1], [No<Subpopulation>$ exerterSubpop = NULL], [logical$ returnDict = F])
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestNeighbors(ind1[0], 0, p2); stop(); }", __LINE__);								// sex-segregation effects
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestNeighbors(ind1[0], 1, p2); stop(); }", __LINE__);								// sex-segregation effects
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestNeighbors(ind1[0], 100, p2); stop(); }", __LINE__);							// sex-segregation effects
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestNeighbors(ind1[0], 0, p3); stop(); }", __LINE__);								// empty exerter subpop
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestNeighbors(ind1[0], 1, p3); stop(); }", __LINE__);								// empty exerter subpop
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestNeighbors(ind1[0], 100, p3); stop(); }", __LINE__);							// empty exerter subpop
		
		// nearestNeighbors(, returnDict=T)
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestNeighbors(ind3, 0, p2, returnDict=T); stop(); }", __LINE__);					// empty receiver
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestNeighbors(ind3, 1, p2, returnDict=T); stop(); }", __LINE__);					// empty receiver
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestNeighbors(ind3, 100, p2, returnDict=T); stop(); }", __LINE__);					// empty receiver
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestNeighbors(ind1[0], 0, p2, returnDict=T); stop(); }", __LINE__);				// sex-segregation effects
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestNeighbors(ind1[0:1], 0, p2, returnDict=T); stop(); }", __LINE__);				// sex-segregation effects
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestNeighbors(ind1[0], 1, p2, returnDict=T); stop(); }", __LINE__);				// sex-segregation effects
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestNeighbors(ind1[0:1], 1, p2, returnDict=T); stop(); }", __LINE__);				// sex-segregation effects
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestNeighbors(ind1[0], 100, p2, returnDict=T); stop(); }", __LINE__);				// sex-segregation effects
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestNeighbors(ind1[0:1], 100, p2, returnDict=T); stop(); }", __LINE__);			// sex-segregation effects
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestNeighbors(ind3, 0, p3, returnDict=T); stop(); }", __LINE__);					// empty receiver, empty exerter subpop
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestNeighbors(ind3, 1, p3, returnDict=T); stop(); }", __LINE__);					// empty receiver, empty exerter subpop
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestNeighbors(ind3, 1000, p3, returnDict=T); stop(); }", __LINE__);				// empty receiver, empty exerter subpop
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestNeighbors(ind1[0], 0, p3, returnDict=T); stop(); }", __LINE__);				// empty exerter subpop
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestNeighbors(ind1[0:1], 0, p3, returnDict=T); stop(); }", __LINE__);				// empty exerter subpop
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestNeighbors(ind1[0], 1, p3, returnDict=T); stop(); }", __LINE__);				// empty exerter subpop
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestNeighbors(ind1[0:1], 1, p3, returnDict=T); stop(); }", __LINE__);				// empty exerter subpop
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestNeighbors(ind1[0], 100, p3, returnDict=T); stop(); }", __LINE__);				// empty exerter subpop
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestNeighbors(ind1[0:1], 100, p3, returnDict=T); stop(); }", __LINE__);			// empty exerter subpop
		
		// (object<Individual>)nearestNeighborsOfPoint(float point, io<Subpopulation>$ exerterSubpop, [integer$ count = 1])
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestNeighborsOfPoint(ind1[0].xy, p2, count=0); stop(); }", __LINE__);				// sex-segregation effects
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestNeighborsOfPoint(ind1[0].xy, p2, count=1); stop(); }", __LINE__);				// sex-segregation effects
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestNeighborsOfPoint(ind1[0].xy, p2, count=100); stop(); }", __LINE__);			// sex-segregation effects
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestNeighborsOfPoint(ind1[0].xy, p2, count=0); stop(); }", __LINE__);				// empty exerter subpop
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestNeighborsOfPoint(ind1[0].xy, p2, count=1); stop(); }", __LINE__);				// empty exerter subpop
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.nearestNeighborsOfPoint(ind1[0].xy, p2, count=100); stop(); }", __LINE__);			// empty exerter subpop
		
		// (integer)neighborCount(object<Individual> receivers, [No<Subpopulation>$ exerterSubpop = NULL])
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.neighborCount(ind3, p2); stop(); }", __LINE__);										// empty receiver
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.neighborCount(ind1[0], p2); stop(); }", __LINE__);									// sex-segregation effects
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.neighborCount(ind1[0:1], p2); stop(); }", __LINE__);									// sex-segregation effects
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.neighborCount(ind3, p3); stop(); }", __LINE__);										// empty receiver, empty exerter subpop
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.neighborCount(ind1[0], p3); stop(); }", __LINE__);									// empty exerter subpop
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.neighborCount(ind1[0:1], p3); stop(); }", __LINE__);									// empty exerter subpop
		
		// (integer$)neighborCountOfPoint(float point, io<Subpopulation>$ exerterSubpop)
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.neighborCountOfPoint(ind1[0].xy, p2); stop(); }", __LINE__);							// sex-segregation effects
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.neighborCountOfPoint(ind1[0].xy, p3); stop(); }", __LINE__);							// empty exerter subpop
		
		// (float)strength(object<Individual>$ receiver, [No<Individual> exerters = NULL])
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.strength(ind1[0], ind2); stop(); }", __LINE__);										// sex-segregation effects
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.strength(ind1[0], ind3); stop(); }", __LINE__);										// empty exerter subpop
		
		// (float)totalOfNeighborStrengths(object<Individual> receivers, [No<Subpopulation>$ exerterSubpop = NULL])
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.totalOfNeighborStrengths(ind3, p2); stop(); }", __LINE__);							// empty receiver
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.totalOfNeighborStrengths(ind1[0], p2); stop(); }", __LINE__);							// sex-segregation effects
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.totalOfNeighborStrengths(ind1[0:1], p2); stop(); }", __LINE__);						// sex-segregation effects
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.totalOfNeighborStrengths(ind3, p3); stop(); }", __LINE__);							// empty receiver, empty exerter subpop
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.totalOfNeighborStrengths(ind1[0], p3); stop(); }", __LINE__);							// empty exerter subpop
		SLiMAssertScriptStop(gen1_setup_i1xy_edge_cases + "i1.totalOfNeighborStrengths(ind1[0:1], p3); stop(); }", __LINE__);						// empty exerter subpop
	}
}

void _RunInteractionTypeTests_LocalPopDensity()
{
	// Test InteractionType - localPopulationDensity()
	// FIXME for now we just make the calls, we don't test the results
	
	// *** 1D
	for (int i = 0; i < 6; ++i)
	{
		std::string gen1_setup_i1x_pop;
		
		if (i == 0)
			gen1_setup_i1x_pop = "initialize() { initializeSLiMOptions(dimensionality='xyz'); initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'x', reciprocal=T, maxDistance=10.0); } 1 early() { sim.addSubpop('p1', 10); p1.setSpatialBounds(c(-30, -30, -30, 30, 30, 30)); p1.individuals.x = c(-10.0, 0, 1, 2, 3, 5, 7, 8, 20, 25); p1.individuals.y = runif(10); p1.individuals.z = runif(10); i1.evaluate(p1); ind = p1.individuals; ";
		else if (i == 1)
			gen1_setup_i1x_pop = "initialize() { initializeSLiMOptions(dimensionality='xyz'); initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'y', reciprocal=T, maxDistance=10.0); } 1 early() { sim.addSubpop('p1', 10); p1.setSpatialBounds(c(-30, -30, -30, 30, 30, 30)); p1.individuals.y = c(-10.0, 0, 1, 2, 3, 5, 7, 8, 20, 25); p1.individuals.x = runif(10); p1.individuals.z = runif(10); i1.evaluate(p1); ind = p1.individuals; ";
		else if (i == 2)
			gen1_setup_i1x_pop = "initialize() { initializeSLiMOptions(dimensionality='xyz'); initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'z', reciprocal=T, maxDistance=10.0); } 1 early() { sim.addSubpop('p1', 10); p1.setSpatialBounds(c(-30, -30, -30, 30, 30, 30)); p1.individuals.z = c(-10.0, 0, 1, 2, 3, 5, 7, 8, 20, 25); p1.individuals.x = runif(10); p1.individuals.y = runif(10); i1.evaluate(p1); ind = p1.individuals; ";
		
		// go beyond type 'f', since that hits an optimized case
		else if (i == 3)
			gen1_setup_i1x_pop = "initialize() { initializeSLiMOptions(dimensionality='xyz'); initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'x', reciprocal=T, maxDistance=10.0); i1.setInteractionFunction('l', 1.0); } 1 early() { sim.addSubpop('p1', 10); p1.setSpatialBounds(c(-30, -30, -30, 30, 30, 30)); p1.individuals.x = c(-10.0, 0, 1, 2, 3, 5, 7, 8, 20, 25); p1.individuals.y = runif(10); p1.individuals.z = runif(10); i1.evaluate(p1); ind = p1.individuals; ";
		else if (i == 4)
			gen1_setup_i1x_pop = "initialize() { initializeSLiMOptions(dimensionality='xyz'); initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'y', reciprocal=T, maxDistance=10.0); i1.setInteractionFunction('l', 1.0); } 1 early() { sim.addSubpop('p1', 10); p1.setSpatialBounds(c(-30, -30, -30, 30, 30, 30)); p1.individuals.y = c(-10.0, 0, 1, 2, 3, 5, 7, 8, 20, 25); p1.individuals.x = runif(10); p1.individuals.z = runif(10); i1.evaluate(p1); ind = p1.individuals; ";
		else // if (i == 5)
			gen1_setup_i1x_pop = "initialize() { initializeSLiMOptions(dimensionality='xyz'); initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'z', reciprocal=T, maxDistance=10.0); i1.setInteractionFunction('l', 1.0); } 1 early() { sim.addSubpop('p1', 10); p1.setSpatialBounds(c(-30, -30, -30, 30, 30, 30)); p1.individuals.z = c(-10.0, 0, 1, 2, 3, 5, 7, 8, 20, 25); p1.individuals.x = runif(10); p1.individuals.y = runif(10); i1.evaluate(p1); ind = p1.individuals; ";

		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.localPopulationDensity(ind[integer(0)]); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.localPopulationDensity(ind[0]); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.localPopulationDensity(ind[c(0, 5, 9)]); stop(); }", __LINE__);
		
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.localPopulationDensity(ind[integer(0)]); stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.localPopulationDensity(ind[0]); stop(); } interaction(i1) { return 2.0; }", "interaction() callbacks", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.localPopulationDensity(ind[c(0, 5, 9)]); stop(); } interaction(i1) { return 2.0; }", "interaction() callbacks", __LINE__);
	}
	/*
	// *** 2D
	for (int i = 0; i < 6; ++i)
	{
		std::string gen1_setup_i1xy_pop;
		bool use_first_coordinate = (i < 3);
		
		if (i == 0)
			gen1_setup_i1xy_pop = "initialize() { initializeSLiMOptions(dimensionality='xyz'); " + sex_string + "initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'xy', " + reciprocal_string + ", maxDistance=" + p_max_distance + ", sexSegregation='" + p_sex_segregation + "'); } 1 early() { sim.addSubpop('p1', 10); p1.individuals.x = c(-10.0, 0, 1, 2, 3, 5, 7, 8, 20, 25); p1.individuals.y = 0; p1.individuals.z = runif(10); i1.evaluate(p1); ind = p1.individuals; ";
		else if (i == 1)
			gen1_setup_i1xy_pop = "initialize() { initializeSLiMOptions(dimensionality='xyz'); " + sex_string + "initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'xz', " + reciprocal_string + ", maxDistance=" + p_max_distance + ", sexSegregation='" + p_sex_segregation + "'); } 1 early() { sim.addSubpop('p1', 10); p1.individuals.x = c(-10.0, 0, 1, 2, 3, 5, 7, 8, 20, 25); p1.individuals.z = 0; p1.individuals.y = runif(10); i1.evaluate(p1); ind = p1.individuals; ";
		else if (i == 2)
			gen1_setup_i1xy_pop = "initialize() { initializeSLiMOptions(dimensionality='xyz'); " + sex_string + "initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'yz', " + reciprocal_string + ", maxDistance=" + p_max_distance + ", sexSegregation='" + p_sex_segregation + "'); } 1 early() { sim.addSubpop('p1', 10); p1.individuals.y = c(-10.0, 0, 1, 2, 3, 5, 7, 8, 20, 25); p1.individuals.z = 0; p1.individuals.x = runif(10); i1.evaluate(p1); ind = p1.individuals; ";
		else if (i == 3)
			gen1_setup_i1xy_pop = "initialize() { initializeSLiMOptions(dimensionality='xyz'); " + sex_string + "initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'xy', " + reciprocal_string + ", maxDistance=" + p_max_distance + ", sexSegregation='" + p_sex_segregation + "'); } 1 early() { sim.addSubpop('p1', 10); p1.individuals.y = c(-10.0, 0, 1, 2, 3, 5, 7, 8, 20, 25); p1.individuals.x = 0; p1.individuals.z = runif(10); i1.evaluate(p1); ind = p1.individuals; ";
		else if (i == 4)
			gen1_setup_i1xy_pop = "initialize() { initializeSLiMOptions(dimensionality='xyz'); " + sex_string + "initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'xz', " + reciprocal_string + ", maxDistance=" + p_max_distance + ", sexSegregation='" + p_sex_segregation + "'); } 1 early() { sim.addSubpop('p1', 10); p1.individuals.z = c(-10.0, 0, 1, 2, 3, 5, 7, 8, 20, 25); p1.individuals.x = 0; p1.individuals.y = runif(10); i1.evaluate(p1); ind = p1.individuals; ";
		else // if (i == 5)
			gen1_setup_i1xy_pop = "initialize() { initializeSLiMOptions(dimensionality='xyz'); " + sex_string + "initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'yz', " + reciprocal_string + ", maxDistance=" + p_max_distance + ", sexSegregation='" + p_sex_segregation + "'); } 1 early() { sim.addSubpop('p1', 10); p1.individuals.z = c(-10.0, 0, 1, 2, 3, 5, 7, 8, 20, 25); p1.individuals.y = 0; p1.individuals.x = runif(10); i1.evaluate(p1); ind = p1.individuals; ";
		
	}
	*/
	
	// 3D is not supported by clippedIntegral() at the moment
}

#pragma mark Continuous space tests
void _RunContinuousSpaceTests(void)
{
	// Since these tests are so different from others – spatiality has to be enabled, interactions have to be set up,
	// etc. – I decided to put them in their own test function, rather than wedging them into the class tests above.
	// Tests of the basic functionality of properties and methods remain in the class tests, however.
	
	// BCH 10/5/2023: I'm not sure what I intended to go here!  This function was empty until now.  But now I've added
	// the tests below, which test inheritance of position and pointDeviated().  Here's the full model that we test
	// variants of:
	
	/*
	 initialize() {
		 // periodic bounds enabled/disabled
		 initializeSLiMOptions(dimensionality="xy", periodicity="xy");
		 
		 // sex enabled/disabled
		 initializeSex("A");
	 }
	 1 early() {
		 sim.addSubpop("p1", 500);
		 if (sim.periodicity == "")
			 p1.setSpatialBounds(c(1.5, 3.8, 1.9, 6.2));
		 else
			 p1.setSpatialBounds(c(0.0, 0.0, 1.9, 6.2));
		 p1.individuals.setSpatialPosition(p1.pointUniform(p1.individualCount));
		 
		 // cloning and selfing enabled/disabled
		 p1.setCloningRate(0.2);
		 if (!sim.sexEnabled)
			 p1.setSelfingRate(0.2);
	 }
	 early() {
		 defineGlobal("PARENT_POS", p1.individuals.spatialPosition);
	 }
	 // callback present/absent
	 modifyChild() {
		 if ((child.x != parent1.x) | (child.y != parent1.y))
			 stop("child does not match parent!");
		 return T;
	 }
	 late() {
		 inds = p1.individuals;
		 pos = inds.spatialPosition;
		 if (any(match(pos, PARENT_POS) == -1))
			 stop("child does not match parent!");
		 
		 // different boundary conditions and kernels
		 inds.setSpatialPosition(p1.pointDeviated(inds.size(), pos, "reprising", INF, "n", 0.1));
		 if (!all(p1.pointInBounds(inds.spatialPosition)))
			 stop("position out of bounds!");
	 }
	 10 late() {}
	 */
	
	// This exercises most cases in WF models, although it does not test the code path with migration.
	
	for (int dimcount = 1; dimcount <= 3; ++dimcount)
	{
		for (int sex_enabled = 0; sex_enabled <= 1; ++sex_enabled)
		{
			for (int cloning_selfing = 0; cloning_selfing <= 1; ++cloning_selfing)
			{
				for (int periodic = 0; periodic <= 1; ++periodic)
				{
					for (int callbacks = 0; callbacks <= 1; ++callbacks)
					{
						for (int boundary = 0; boundary <= 3; boundary++)
						{
							for (int kernel = 0; kernel <= 4; kernel++)
							{
								if ((boundary == 3) && (!periodic))		// with periodic bounds, use only periodic boundary condition
									continue;
								if ((boundary != 3) && periodic)		// with non-periodic bounds, do not use periodic boundary condition
									continue;
								if ((dimcount == 3) && (kernel == 4))	// in 3D, do not use Student's t displacement; not implemented
									continue;
								
								std::string model_string = "initialize() { ";
								
								if (dimcount == 1)
								{
									if (periodic)
										model_string.append("initializeSLiMOptions(dimensionality='x', periodicity='x'); ");
									else
										model_string.append("initializeSLiMOptions(dimensionality='x'); ");
								}
								else if (dimcount == 2)
								{
									if (periodic)
										model_string.append("initializeSLiMOptions(dimensionality='xy', periodicity='xy'); ");
									else
										model_string.append("initializeSLiMOptions(dimensionality='xy'); ");
								}
								else
								{
									if (periodic)
										model_string.append("initializeSLiMOptions(dimensionality='xyz', periodicity='xyz'); ");
									else
										model_string.append("initializeSLiMOptions(dimensionality='xyz'); ");
								}
								
								if (sex_enabled)
									model_string.append("initializeSex('A'); ");
								
								model_string.append("} 1 early() { sim.addSubpop('p1', 500); ");
								
								if (dimcount == 1)
								{
									if (periodic)
										model_string.append("p1.setSpatialBounds(c(0.0, 6.2)); ");
									else
										model_string.append("p1.setSpatialBounds(c(1.8, 6.2)); ");
								}
								else if (dimcount == 2)
								{
									if (periodic)
										model_string.append("p1.setSpatialBounds(c(0.0, 0.0, 1.9, 6.2)); ");
									else
										model_string.append("p1.setSpatialBounds(c(1.5, 1.8, 1.9, 6.2)); ");
								}
								else
								{
									if (periodic)
										model_string.append("p1.setSpatialBounds(c(0.0, 0.0, 0.0, 1.9, 6.2, 11.4)); ");
									else
										model_string.append("p1.setSpatialBounds(c(1.5, 1.8, 0.7, 1.9, 6.2, 11.4)); ");
								}
								
								model_string.append("p1.individuals.setSpatialPosition(p1.pointUniform(p1.individualCount)); ");
								
								if (cloning_selfing)
									model_string.append("p1.setCloningRate(0.2); if (!sim.sexEnabled) p1.setSelfingRate(0.2); ");
								
								model_string.append("} early() { defineGlobal('PARENT_POS', p1.individuals.spatialPosition); } ");
								
								if (callbacks)
								{
									if (dimcount == 1)
										model_string.append("modifyChild() { if (child.x != parent1.x) stop('child does not match parent!'); return T; } ");
									else if (dimcount == 2)
										model_string.append("modifyChild() { if ((child.x != parent1.x) | (child.y != parent1.y)) stop('child does not match parent!'); return T; } ");
									else
										model_string.append("modifyChild() { if ((child.x != parent1.x) | (child.y != parent1.y) | (child.z != parent1.z)) stop('child does not match parent!'); return T; } ");
								}
								
								model_string.append("late() { inds = p1.individuals; pos = inds.spatialPosition; ");
								model_string.append("if (any(match(pos, PARENT_POS) == -1)) stop('child does not match parent!'); ");
								model_string.append("inds.setSpatialPosition(p1.pointDeviated(inds.size(), pos, ");
								
								switch (boundary)					// NOLINT(*-missing-default-case) : loop bounds
								{
									case 0: model_string.append("'stopping'"); break;
									case 1: model_string.append("'reflecting'"); break;
									case 2: model_string.append("'reprising'"); break;
									case 3: model_string.append("'periodic'"); break;
								}
								
								switch (kernel)						// NOLINT(*-missing-default-case) : loop bounds
								{
									case 0: model_string.append(", 0.1, 'f')); "); break;
									case 1: model_string.append(", 0.1, 'l')); "); break;
									case 2: model_string.append(", INF, 'e', 10.0)); "); break;
									case 3: model_string.append(", INF, 'n', 0.1)); "); break;
									case 4: model_string.append(", INF, 't', 2.0, 0.1)); "); break;
								}
								
								model_string.append("if (!all(p1.pointInBounds(inds.spatialPosition))) stop('position out of bounds!'); ");
								model_string.append("} 10 late() {} ");
								
								SLiMAssertScriptSuccess(model_string);
							}
						}
					}
				}
			}
		}
	}
	
	// For nonWF models we have a different test model.  This is simpler since there are not so many code paths to check.
	// Sex doesn't matter, callbacks present/absent doesn't matter, migration doesn't matter, cloning/selfing is tested
	// in every variant here:
	
	/*
	 initialize() {
		 initializeSLiMModelType("nonWF");
		 defineConstant("K", 100);	
		 
		 // periodic bounds enabled/disabled
		 initializeSLiMOptions(dimensionality="xy", periodicity="xy");
		 
		 // need genetics so we can use addRecombinant()
		 initializeMutationType("m1", 0.5, "f", 0.0);
		 initializeGenomicElementType("g1", m1, 1.0);
		 initializeGenomicElement(g1, 0, 99999);
		 initializeMutationRate(1e-7);
		 initializeRecombinationRate(1e-8);
	 }
	 reproduction() {
		 mate = subpop.sampleIndividuals(1);
		 o1 = subpop.addCrossed(individual, mate);
		 o2 = subpop.addCloned(individual);
		 o3 = subpop.addSelfed(individual);
		 ig = sample(individual.genomes, 2, F);
		 mg = sample(mate.genomes, 2, F);
		 o4 = subpop.addRecombinant(ig[0], ig[1], sim.chromosome.drawBreakpoints(),
					 mg[0], mg[1], sim.chromosome.drawBreakpoints(),
					 parent1=individual, parent2=mate);
		 for (o in c(o1, o2, o3, o4))
			 if ((o.x != individual.x) | (o.y != individual.y))
				 stop("child does not match parent!");
	 }
	 1 early() {
		 sim.addSubpop("p1", K);
		 if (sim.periodicity == "")
			 p1.setSpatialBounds(c(1.5, 3.8, 1.9, 6.2));
		 else
			 p1.setSpatialBounds(c(0.0, 0.0, 1.9, 6.2));
		 p1.individuals.setSpatialPosition(p1.pointUniform(p1.individualCount));
	 }
	 early() {
		 inds = p1.individuals;
		 pos = inds.spatialPosition;
		 
		 // different boundary conditions and kernels
		 inds.setSpatialPosition(p1.pointDeviated(inds.size(), pos, "reprising", INF, "n", 0.1));
		 if (!all(p1.pointInBounds(inds.spatialPosition)))
			 stop("position out of bounds!");
		 
		 p1.fitnessScaling = K / p1.individualCount;
	 }
	 10 late() {}
	 */
	
	for (int dimcount = 1; dimcount <= 3; ++dimcount)
	{
		for (int periodic = 0; periodic <= 1; ++periodic)
		{
			for (int boundary = 0; boundary <= 3; boundary++)
			{
				for (int kernel = 0; kernel <= 4; kernel++)
				{
					if ((boundary == 3) && (!periodic))		// with periodic bounds, use only periodic boundary condition
						continue;
					if ((boundary != 3) && periodic)		// with non-periodic bounds, do not use periodic boundary condition
						continue;
					if ((dimcount == 3) && (kernel == 4))	// in 3D, do not use Student's t displacement; not implemented
						continue;
					
					std::string model_string = "initialize() { initializeSLiMModelType('nonWF'); defineConstant('K', 100); ";
					
					if (dimcount == 1)
					{
						if (periodic)
							model_string.append("initializeSLiMOptions(dimensionality='x', periodicity='x'); ");
						else
							model_string.append("initializeSLiMOptions(dimensionality='x'); ");
					}
					else if (dimcount == 2)
					{
						if (periodic)
							model_string.append("initializeSLiMOptions(dimensionality='xy', periodicity='xy'); ");
						else
							model_string.append("initializeSLiMOptions(dimensionality='xy'); ");
					}
					else
					{
						if (periodic)
							model_string.append("initializeSLiMOptions(dimensionality='xyz', periodicity='xyz'); ");
						else
							model_string.append("initializeSLiMOptions(dimensionality='xyz'); ");
					}
					
					model_string.append("initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeMutationRate(1e-7); initializeRecombinationRate(1e-8); } ");
					
					model_string.append("reproduction() { mate = subpop.sampleIndividuals(1); o1 = subpop.addCrossed(individual, mate); o2 = subpop.addCloned(individual); o3 = subpop.addSelfed(individual); ");
					model_string.append("ig = sample(individual.genomes, 2, F); mg = sample(mate.genomes, 2, F); o4 = subpop.addRecombinant(ig[0], ig[1], sim.chromosome.drawBreakpoints(), mg[0], mg[1], sim.chromosome.drawBreakpoints(), parent1=individual, parent2=mate); ");
					
					if (dimcount == 1)
						model_string.append("for (o in c(o1, o2, o3, o4)) if (o.x != individual.x) stop('child does not match parent!'); }");
					else if (dimcount == 2)
						model_string.append("for (o in c(o1, o2, o3, o4)) if ((o.x != individual.x) | (o.y != individual.y)) stop('child does not match parent!'); }");
					else
						model_string.append("for (o in c(o1, o2, o3, o4)) if ((o.x != individual.x) | (o.y != individual.y) | (o.z != individual.z)) stop('child does not match parent!'); }");
					
					model_string.append("1 early() { sim.addSubpop('p1', K); ");
					
					if (dimcount == 1)
					{
						if (periodic)
							model_string.append("p1.setSpatialBounds(c(0.0, 6.2)); ");
						else
							model_string.append("p1.setSpatialBounds(c(1.8, 6.2)); ");
					}
					else if (dimcount == 2)
					{
						if (periodic)
							model_string.append("p1.setSpatialBounds(c(0.0, 0.0, 1.9, 6.2)); ");
						else
							model_string.append("p1.setSpatialBounds(c(1.5, 1.8, 1.9, 6.2)); ");
					}
					else
					{
						if (periodic)
							model_string.append("p1.setSpatialBounds(c(0.0, 0.0, 0.0, 1.9, 6.2, 11.4)); ");
						else
							model_string.append("p1.setSpatialBounds(c(1.5, 1.8, 0.7, 1.9, 6.2, 11.4)); ");
					}
					
					model_string.append("p1.individuals.setSpatialPosition(p1.pointUniform(p1.individualCount)); }");
					
					model_string.append("early() { inds = p1.individuals; pos = inds.spatialPosition; inds.setSpatialPosition(p1.pointDeviated(inds.size(), pos, ");
					
					switch (boundary)					// NOLINT(*-missing-default-case) : loop bounds
					{
						case 0: model_string.append("'stopping'"); break;
						case 1: model_string.append("'reflecting'"); break;
						case 2: model_string.append("'reprising'"); break;
						case 3: model_string.append("'periodic'"); break;
					}
					
					switch (kernel)						// NOLINT(*-missing-default-case) : loop bounds
					{
						case 0: model_string.append(", 0.1, 'f')); "); break;
						case 1: model_string.append(", 0.1, 'l')); "); break;
						case 2: model_string.append(", INF, 'e', 10.0)); "); break;
						case 3: model_string.append(", INF, 'n', 0.1)); "); break;
						case 4: model_string.append(", INF, 't', 2.0, 0.1)); "); break;
					}
					
					model_string.append("if (!all(p1.pointInBounds(inds.spatialPosition))) stop('position out of bounds!'); ");
					model_string.append("p1.fitnessScaling = K / p1.individualCount; } 10 late() {} ");
					
					SLiMAssertScriptSuccess(model_string);
				}
			}
		}
	}
	
	// Test different kernel types - other tests generally use only type "f"
	// Test different constraints - note that sex-segregation gets tested elsewhere
	for (int dimcount = 1; dimcount <= 3; ++dimcount)
	{
		std::string dimensionality;
		
		if (dimcount == 1)		dimensionality = "x";
		else if (dimcount == 2)	dimensionality = "xy";
		else					dimensionality = "xyz";
		
		for (int constraints = 0; constraints <= 1; ++constraints)
		{
			for (int periodic = 0; periodic <= 1; ++periodic)
			{
				for (int kernel = 0; kernel <= 5; kernel++)
				{
					std::string model_string = "initialize() { initializeSLiMModelType('nonWF'); defineConstant('K', 100); ";
					
					if (periodic)
						model_string.append("initializeSLiMOptions(dimensionality='" + dimensionality + "', periodicity='" + dimensionality + "'); ");
					else
						model_string.append("initializeSLiMOptions(dimensionality='" + dimensionality + "'); ");
					
					model_string.append("initializeSex('A'); ");
					
					model_string.append("initializeInteractionType(1, '" + dimensionality + "', maxDistance=0.2); ");
					
					switch (kernel)
					{
						case 0: model_string.append("i1.setInteractionFunction('f', 1.0); "); break;
						case 1: model_string.append("i1.setInteractionFunction('l', 1.0); "); break;
						case 2: model_string.append("i1.setInteractionFunction('n', 1.0, 0.1); "); break;
						case 3: model_string.append("i1.setInteractionFunction('e', 1.0, 10.0); "); break;
						case 4: model_string.append("i1.setInteractionFunction('c', 1.0, 0.1); "); break;
						case 5: model_string.append("i1.setInteractionFunction('t', 1.0, 3.0, 0.1); "); break;
					}
					
					if (constraints)
					{
						model_string.append("i1.setConstraints('receiver', sex='M', tagL2=T); ");
						model_string.append("i1.setConstraints('exerter', sex='F', tagL2=F); ");
					}
					
					model_string.append("} 1 early() { sim.addSubpop(1, K); inds = p1.individuals; inds.setSpatialPosition(p1.pointUniform(1)); ");
					
					if (constraints)
						model_string.append("inds.tagL2 = (runif(K) < 0.5); ");
					
					model_string.append("i1.evaluate(p1); ");
					model_string.append("i1.drawByStrength(inds[0], 1, p1); ");
					model_string.append("i1.drawByStrength(inds[0], 1000, p1); ");
					model_string.append("i1.drawByStrength(inds, 1, p1, returnDict=T); ");
					model_string.append("}");
					
					SLiMAssertScriptSuccess(model_string, __LINE__);
				}
			}
		}
	}
	
	// Test summarizeIndividuals() in a few simple ways
	for (int dimcount = 1; dimcount <= 3; ++dimcount)
	{
		std::string dimensionality;
		std::string dim_str;
		
		if (dimcount == 1)		{ dimensionality = "x"; dim_str = "10"; }
		else if (dimcount == 2)	{ dimensionality = "xy"; dim_str = "10, 10"; }
		else					{ dimensionality = "xyz"; dim_str = "5, 5, 5"; }
		
		for (int operation = 0; operation <= 2; operation++)
		{
			std::string model_string = "initialize() { initializeSLiMOptions(dimensionality='" + dimensionality + "'); defineConstant('K', 1000); } ";
			
			model_string.append("1 late() { sim.addSubpop('p1', K); p1.individuals.setSpatialPosition(p1.pointUniform(K)); ");
			model_string.append("density = summarizeIndividuals(p1.individuals, c(" + dim_str + "), p1.spatialBounds, ");
			
			if (operation == 0)
				model_string.append("operation='1;', empty=0.0, perUnitArea=F); ");
			else if (operation == 1)
				model_string.append("operation='individuals.size();', empty=0.0, perUnitArea=T); ");
			else
				model_string.append("operation='2;', empty=0.0, perUnitArea=F); ");
			
			model_string.append("}");
			
			SLiMAssertScriptSuccess(model_string, __LINE__);
		}
	}
}

#pragma mark Spatial map tests
void _RunSpatialMapTests(void)
{
	for (int periodic = 0; periodic <= 1; ++periodic)
	{
		//
		//	1D
		//
		std::string prefix_1D;
		
		if (periodic == 0)
			prefix_1D = "initialize() { initializeSLiMOptions(dimensionality='x'); } 1 early() { sim.addSubpop('p1', 10); mv1 = runif(11); mv2 = runif(11); m1 = p1.defineSpatialMap('map1', 'x', mv1); m2 = p1.defineSpatialMap('map2', 'x', mv2); ";
		else
			prefix_1D = "initialize() { initializeSLiMOptions(dimensionality='x', periodicity='x'); } 1 early() { sim.addSubpop('p1', 10); mv1 = runif(11); mv2 = runif(11); m1 = p1.defineSpatialMap('map1', 'x', mv1); m2 = p1.defineSpatialMap('map2', 'x', mv2); ";
		
		SLiMAssertScriptStop(prefix_1D + "f1 = m1.gridValues(); f2 = m2.gridValues(); if (identical(mv1, f1) & identical(mv2, f2)) stop(); } ");
		
		SLiMAssertScriptStop(prefix_1D + "m3 = SpatialMap('map3', m1); f3 = m3.gridValues(); if (identical(mv1, f3)) stop(); } ");
		
		SLiMAssertScriptStop(prefix_1D + "m1.add(17.3); if (identical(mv1 + 17.3, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_1D + "m1.add(mv2); if (identical(mv1 + mv2, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_1D + "m1.add(m2); if (identical(mv1 + mv2, m1.gridValues())) stop(); } ");
		
		SLiMAssertScriptStop(prefix_1D + "m1.blend(17.3, 0.0); if (identical(mv1, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_1D + "m1.blend(mv2, 0.0); if (identical(mv1, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_1D + "m1.blend(m2, 0.0); if (identical(mv1, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_1D + "m1.blend(17.3, 1.0); if (identical(rep(17.3, 11), m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_1D + "m1.blend(mv2, 1.0); if (identical(mv2, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_1D + "m1.blend(m2, 1.0); if (identical(mv2, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_1D + "m1.blend(17.3, 0.4); if (all(abs((mv1*0.6 + rep(17.3, 11)*0.4) - m1.gridValues()) < 1e-15)) stop(); } ");
		SLiMAssertScriptStop(prefix_1D + "m1.blend(mv2, 0.4); if (all(abs((mv1*0.6 + mv2*0.4) - m1.gridValues()) < 1e-15)) stop(); } ");
		SLiMAssertScriptStop(prefix_1D + "m1.blend(m2, 0.4); if (all(abs((mv1*0.6 + mv2*0.4) - m1.gridValues()) < 1e-15)) stop(); } ");
		
		SLiMAssertScriptStop(prefix_1D + "m1.multiply(0.25); if (identical(mv1 * 0.25, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_1D + "m1.multiply(mv2); if (identical(mv1 * mv2, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_1D + "m1.multiply(m2); if (identical(mv1 * mv2, m1.gridValues())) stop(); } ");
		
		SLiMAssertScriptStop(prefix_1D + "m1.subtract(0.25); if (identical(mv1 - 0.25, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_1D + "m1.subtract(mv2); if (identical(mv1 - mv2, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_1D + "m1.subtract(m2); if (identical(mv1 - mv2, m1.gridValues())) stop(); } ");
		
		SLiMAssertScriptStop(prefix_1D + "m1.divide(0.25); if (identical(mv1 / 0.25, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_1D + "m1.divide(mv2); if (identical(mv1 / mv2, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_1D + "m1.divide(m2); if (identical(mv1 / mv2, m1.gridValues())) stop(); } ");
		
		SLiMAssertScriptStop(prefix_1D + "m1.power(0.25); if (identical(mv1 ^ 0.25, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_1D + "m1.power(mv2); if (identical(mv1 ^ mv2, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_1D + "m1.power(m2); if (identical(mv1 ^ mv2, m1.gridValues())) stop(); } ");
		
		SLiMAssertScriptStop(prefix_1D + "m1.exp(); if (identical(exp(mv1), m1.gridValues())) stop(); } ");
		
		SLiMAssertScriptSuccess(prefix_1D + "m1.changeColors(c(0.0, 1.0), c('black', 'white')); } ");
		SLiMAssertScriptSuccess(prefix_1D + "m1.changeColors(c(0.0, 1.0), c('black', 'white')); m1.changeColors(c(0.5, 0.8), c('red', 'blue')); } ");
		
		SLiMAssertScriptRaise(prefix_1D + "m1.changeValues(17.3); }", "must be of size >= 2", __LINE__);
		SLiMAssertScriptStop(prefix_1D + "mx = rep(17.3, 10); m1.changeValues(mx); if (identical(mx, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_1D + "m1.changeValues(mv2); if (identical(mv2, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_1D + "m1.changeValues(m2); if (identical(mv2, m1.gridValues())) stop(); } ");
		
		SLiMAssertScriptStop(prefix_1D + "m1.interpolate(3, 'nearest'); if (identical(m1.gridDimensions, 31)) stop(); } ");
		SLiMAssertScriptStop(prefix_1D + "m1.interpolate(3, 'linear'); if (identical(m1.gridDimensions, 31)) stop(); } ");
		SLiMAssertScriptStop(prefix_1D + "m1.interpolate(3, 'cubic'); if (identical(m1.gridDimensions, 31)) stop(); } ");
		
		SLiMAssertScriptSuccess(prefix_1D + "m1.changeColors(c(0.0, 1.0), c('red', 'black')); m1.mapColor(rnorm(50)); } ");
		
		/* mapImage() only generates 2D images
		 SLiMAssertScriptSuccess(prefix_1D + "m1.mapImage(centers=F, color=F); } ");
		 SLiMAssertScriptSuccess(prefix_1D + "m1.mapImage(centers=T, color=F); } ");
		 SLiMAssertScriptSuccess(prefix_1D + "m1.changeColors(c(0.0, 1.0), c('red', 'black')); m1.mapImage(centers=F, color=T); } ");
		 SLiMAssertScriptSuccess(prefix_1D + "m1.changeColors(c(0.0, 1.0), c('red', 'black')); m1.mapImage(centers=T, color=T); } ");
		 SLiMAssertScriptSuccess(prefix_1D + "m1.mapImage(10, 15, centers=F, color=F); } ");
		 SLiMAssertScriptSuccess(prefix_1D + "m1.mapImage(10, 15, centers=T, color=F); } ");
		 SLiMAssertScriptSuccess(prefix_1D + "m1.changeColors(c(0.0, 1.0), c('red', 'black')); m1.mapImage(10, 15, centers=F, color=T); } ");
		 SLiMAssertScriptSuccess(prefix_1D + "m1.changeColors(c(0.0, 1.0), c('red', 'black')); m1.mapImage(10, 15, centers=T, color=T); } ");
		 
		 SLiMAssertScriptSuccess(prefix_1D + "p1.spatialMapImage('map1', centers=F, color=F); } ");
		 SLiMAssertScriptSuccess(prefix_1D + "p1.spatialMapImage('map1', centers=T, color=F); } ");
		 SLiMAssertScriptSuccess(prefix_1D + "p1.spatialMapImage('map1', 10, 15, centers=F, color=F); } ");
		 SLiMAssertScriptSuccess(prefix_1D + "p1.spatialMapImage('map1', 10, 15, centers=T, color=F); } ");*/
		
		SLiMAssertScriptSuccess(prefix_1D + "m1.mapValue(runif(0)); } ");
		SLiMAssertScriptSuccess(prefix_1D + "m1.mapValue(runif(1)); } ");
		SLiMAssertScriptSuccess(prefix_1D + "m1.mapValue(runif(10)); } ");
		
		SLiMAssertScriptSuccess(prefix_1D + "p1.spatialMapValue('map1', runif(0)); } ");
		SLiMAssertScriptSuccess(prefix_1D + "p1.spatialMapValue('map1', runif(1)); } ");
		SLiMAssertScriptSuccess(prefix_1D + "p1.spatialMapValue('map1', runif(10)); } ");
		
		SLiMAssertScriptStop(prefix_1D + "if (identical(range(mv1), m1.range()) & identical(range(mv2), m2.range())) stop(); } ");
		
		SLiMAssertScriptStop(prefix_1D + "m1.rescale(); if (identical(c(0.0, 1.0), m1.range())) stop(); } ");
		SLiMAssertScriptStop(prefix_1D + "m1.rescale(0.2, 1.7); if (identical(c(0.2, 1.7), m1.range())) stop(); } ");
		
		SLiMAssertScriptSuccess(prefix_1D + "m1.sampleImprovedNearbyPoint(runif(10), 0.2, 'f'); } ");
		SLiMAssertScriptSuccess(prefix_1D + "m1.sampleImprovedNearbyPoint(runif(10), 0.2, 'l'); } ");
		SLiMAssertScriptSuccess(prefix_1D + "m1.sampleImprovedNearbyPoint(runif(10), 0.2, 'e', 10.0); } ");
		SLiMAssertScriptSuccess(prefix_1D + "m1.sampleImprovedNearbyPoint(runif(10), 0.2, 'n', 0.1); } ");
		SLiMAssertScriptSuccess(prefix_1D + "m1.sampleImprovedNearbyPoint(runif(10), 0.2, 't', 2, 0.1); } ");
		
		SLiMAssertScriptSuccess(prefix_1D + "m1.sampleNearbyPoint(runif(10), 0.2, 'f'); } ");
		SLiMAssertScriptSuccess(prefix_1D + "m1.sampleNearbyPoint(runif(10), 0.2, 'l'); } ");
		SLiMAssertScriptSuccess(prefix_1D + "m1.sampleNearbyPoint(runif(10), 0.2, 'e', 10.0); } ");
		SLiMAssertScriptSuccess(prefix_1D + "m1.sampleNearbyPoint(runif(10), 0.2, 'n', 0.1); } ");
		SLiMAssertScriptSuccess(prefix_1D + "m1.sampleNearbyPoint(runif(10), 0.2, 't', 2, 0.1); } ");
		
		SLiMAssertScriptSuccess(prefix_1D + "m1.smooth(0.1, 'f'); } ");
		SLiMAssertScriptSuccess(prefix_1D + "m1.smooth(0.1, 'l'); } ");
		SLiMAssertScriptSuccess(prefix_1D + "m1.smooth(0.1, 'e', 10.0); } ");
		SLiMAssertScriptSuccess(prefix_1D + "m1.smooth(0.1, 'n', 0.1); } ");
		SLiMAssertScriptSuccess(prefix_1D + "m1.smooth(0.1, 'c', 0.1); } ");
		SLiMAssertScriptSuccess(prefix_1D + "m1.smooth(0.1, 't', 2, 0.1); } ");
		SLiMAssertScriptSuccess(prefix_1D + "m1.interpolate(3, 'cubic'); m1.smooth(0.1, 'f'); } ");
		SLiMAssertScriptSuccess(prefix_1D + "m1.interpolate(3, 'cubic'); m1.smooth(0.1, 'l'); } ");
		SLiMAssertScriptSuccess(prefix_1D + "m1.interpolate(3, 'cubic'); m1.smooth(0.1, 'e', 10.0); } ");
		SLiMAssertScriptSuccess(prefix_1D + "m1.interpolate(3, 'cubic'); m1.smooth(0.1, 'n', 0.1); } ");
		SLiMAssertScriptSuccess(prefix_1D + "m1.interpolate(3, 'cubic'); m1.smooth(0.1, 'c', 0.1); } ");
		SLiMAssertScriptSuccess(prefix_1D + "m1.interpolate(3, 'cubic'); m1.smooth(0.1, 't', 2, 0.1); } ");
		
		SLiMAssertScriptSuccess(prefix_1D + "defineConstant('M1', m1); defineGlobal('M2', m2); } 2 early() { sim.addSubpop('p2', 10); p2.addSpatialMap(M1); p2.addSpatialMap(M2); } 3 early() { p1.removeSpatialMap('map1'); p2.removeSpatialMap(M2); } 4 early() { if (!identical(p1.spatialMaps, M2)) stop(); if (!identical(p2.spatialMaps, M1)) stop(); p2.removeSpatialMap('map1'); p1.removeSpatialMap(M2); }");
		
		//
		//	2D
		//
		std::string prefix_2D;
		
		if (periodic == 0)
			prefix_2D = "initialize() { initializeSLiMOptions(dimensionality='xy'); } 1 early() { sim.addSubpop('p1', 10); mv1 = matrix(runif(30), ncol=5); mv2 = matrix(runif(30), ncol=5); m1 = p1.defineSpatialMap('map1', 'xy', mv1); m2 = p1.defineSpatialMap('map2', 'xy', mv2); ";
		else
			prefix_2D = "initialize() { initializeSLiMOptions(dimensionality='xy', periodicity='xy'); } 1 early() { sim.addSubpop('p1', 10); mv1 = matrix(runif(30), ncol=5); mv2 = matrix(runif(30), ncol=5); m1 = p1.defineSpatialMap('map1', 'xy', mv1); m2 = p1.defineSpatialMap('map2', 'xy', mv2); ";
		
		SLiMAssertScriptStop(prefix_2D + "f1 = m1.gridValues(); f2 = m2.gridValues(); if (identical(mv1, f1) & identical(mv2, f2)) stop(); } ");
		
		SLiMAssertScriptStop(prefix_2D + "m3 = SpatialMap('map3', m1); f3 = m3.gridValues(); if (identical(mv1, f3)) stop(); } ");
		
		SLiMAssertScriptStop(prefix_2D + "m1.add(17.3); if (identical(mv1 + 17.3, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_2D + "m1.add(mv2); if (identical(mv1 + mv2, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_2D + "m1.add(m2); if (identical(mv1 + mv2, m1.gridValues())) stop(); } ");
		
		SLiMAssertScriptStop(prefix_2D + "m1.blend(17.3, 0.0); if (identical(mv1, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_2D + "m1.blend(mv2, 0.0); if (identical(mv1, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_2D + "m1.blend(m2, 0.0); if (identical(mv1, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_2D + "m1.blend(17.3, 1.0); if (identical(matrix(rep(17.3, 30), ncol=5), m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_2D + "m1.blend(mv2, 1.0); if (identical(mv2, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_2D + "m1.blend(m2, 1.0); if (identical(mv2, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_2D + "m1.blend(17.3, 0.4); if (all(abs((mv1*0.6 + matrix(rep(17.3, 30), ncol=5)*0.4) - m1.gridValues()) < 1e-15)) stop(); } ");
		SLiMAssertScriptStop(prefix_2D + "m1.blend(mv2, 0.4); if (all(abs((mv1*0.6 + mv2*0.4) - m1.gridValues()) < 1e-15)) stop(); } ");
		SLiMAssertScriptStop(prefix_2D + "m1.blend(m2, 0.4); if (all(abs((mv1*0.6 + mv2*0.4) - m1.gridValues()) < 1e-15)) stop(); } ");
		
		SLiMAssertScriptStop(prefix_2D + "m1.multiply(0.25); if (identical(mv1 * 0.25, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_2D + "m1.multiply(mv2); if (identical(mv1 * mv2, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_2D + "m1.multiply(m2); if (identical(mv1 * mv2, m1.gridValues())) stop(); } ");
		
		SLiMAssertScriptStop(prefix_2D + "m1.subtract(0.25); if (identical(mv1 - 0.25, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_2D + "m1.subtract(mv2); if (identical(mv1 - mv2, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_2D + "m1.subtract(m2); if (identical(mv1 - mv2, m1.gridValues())) stop(); } ");
		
		SLiMAssertScriptStop(prefix_2D + "m1.divide(0.25); if (identical(mv1 / 0.25, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_2D + "m1.divide(mv2); if (identical(mv1 / mv2, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_2D + "m1.divide(m2); if (identical(mv1 / mv2, m1.gridValues())) stop(); } ");
		
		SLiMAssertScriptStop(prefix_2D + "m1.power(0.25); if (identical(mv1 ^ 0.25, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_2D + "m1.power(mv2); if (identical(mv1 ^ mv2, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_2D + "m1.power(m2); if (identical(mv1 ^ mv2, m1.gridValues())) stop(); } ");
		
		SLiMAssertScriptStop(prefix_2D + "m1.exp(); if (identical(exp(mv1), m1.gridValues())) stop(); } ");
		
		SLiMAssertScriptSuccess(prefix_2D + "m1.changeColors(c(0.0, 1.0), c('black', 'white')); } ");
		SLiMAssertScriptSuccess(prefix_2D + "m1.changeColors(c(0.0, 1.0), c('black', 'white')); m1.changeColors(c(0.5, 0.8), c('red', 'blue')); } ");
		
		SLiMAssertScriptRaise(prefix_2D + "m1.changeValues(17.3); }", "does not match the spatiality", __LINE__);
		SLiMAssertScriptStop(prefix_2D + "mx = matrix(rep(17.3, 30), ncol=5); m1.changeValues(mx); if (identical(mx, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_2D + "m1.changeValues(mv2); if (identical(mv2, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_2D + "m1.changeValues(m2); if (identical(mv2, m1.gridValues())) stop(); } ");
		
		SLiMAssertScriptStop(prefix_2D + "m1.interpolate(3, 'nearest'); if (identical(m1.gridDimensions, c(13, 16))) stop(); } ");
		SLiMAssertScriptStop(prefix_2D + "m1.interpolate(3, 'linear'); if (identical(m1.gridDimensions, c(13, 16))) stop(); } ");
		SLiMAssertScriptStop(prefix_2D + "m1.interpolate(3, 'cubic'); if (identical(m1.gridDimensions, c(13, 16))) stop(); } ");
		
		SLiMAssertScriptSuccess(prefix_2D + "m1.changeColors(c(0.0, 1.0), c('red', 'black')); m1.mapColor(rnorm(50)); } ");
		
		SLiMAssertScriptSuccess(prefix_2D + "m1.mapImage(centers=F, color=F); } ");
		SLiMAssertScriptSuccess(prefix_2D + "m1.mapImage(centers=T, color=F); } ");
		SLiMAssertScriptSuccess(prefix_2D + "m1.changeColors(c(0.0, 1.0), c('red', 'black')); m1.mapImage(centers=F, color=T); } ");
		SLiMAssertScriptSuccess(prefix_2D + "m1.changeColors(c(0.0, 1.0), c('red', 'black')); m1.mapImage(centers=T, color=T); } ");
		SLiMAssertScriptSuccess(prefix_2D + "m1.mapImage(10, 15, centers=F, color=F); } ");
		SLiMAssertScriptSuccess(prefix_2D + "m1.mapImage(10, 15, centers=T, color=F); } ");
		SLiMAssertScriptSuccess(prefix_2D + "m1.changeColors(c(0.0, 1.0), c('red', 'black')); m1.mapImage(10, 15, centers=F, color=T); } ");
		SLiMAssertScriptSuccess(prefix_2D + "m1.changeColors(c(0.0, 1.0), c('red', 'black')); m1.mapImage(10, 15, centers=T, color=T); } ");
		
		SLiMAssertScriptSuccess(prefix_2D + "p1.spatialMapImage('map1', centers=F, color=F); } ");
		SLiMAssertScriptSuccess(prefix_2D + "p1.spatialMapImage('map1', centers=T, color=F); } ");
		SLiMAssertScriptSuccess(prefix_2D + "p1.spatialMapImage('map1', 10, 15, centers=F, color=F); } ");
		SLiMAssertScriptSuccess(prefix_2D + "p1.spatialMapImage('map1', 10, 15, centers=T, color=F); } ");
		
		SLiMAssertScriptSuccess(prefix_2D + "m1.mapValue(runif(0)); } ");
		SLiMAssertScriptSuccess(prefix_2D + "m1.mapValue(runif(2)); } ");
		SLiMAssertScriptSuccess(prefix_2D + "m1.mapValue(runif(20)); } ");
		SLiMAssertScriptRaise(prefix_2D + "m1.mapValue(runif(21)); } ", "must match spatiality", __LINE__);
		
		SLiMAssertScriptSuccess(prefix_2D + "p1.spatialMapValue('map1', runif(0)); } ");
		SLiMAssertScriptSuccess(prefix_2D + "p1.spatialMapValue('map1', runif(2)); } ");
		SLiMAssertScriptSuccess(prefix_2D + "p1.spatialMapValue('map1', runif(20)); } ");
		SLiMAssertScriptRaise(prefix_2D + "p1.spatialMapValue('map1', runif(21)); } ", "must match spatiality", __LINE__);
		
		SLiMAssertScriptStop(prefix_2D + "if (identical(range(mv1), m1.range()) & identical(range(mv2), m2.range())) stop(); } ");
		
		SLiMAssertScriptStop(prefix_2D + "m1.rescale(); if (identical(c(0.0, 1.0), m1.range())) stop(); } ");
		SLiMAssertScriptStop(prefix_2D + "m1.rescale(0.2, 1.7); if (identical(c(0.2, 1.7), m1.range())) stop(); } ");
		
		SLiMAssertScriptSuccess(prefix_2D + "m1.sampleImprovedNearbyPoint(runif(20), 0.2, 'f'); } ");
		SLiMAssertScriptSuccess(prefix_2D + "m1.sampleImprovedNearbyPoint(runif(20), 0.2, 'l'); } ");
		SLiMAssertScriptSuccess(prefix_2D + "m1.sampleImprovedNearbyPoint(runif(20), 0.2, 'e', 10.0); } ");
		SLiMAssertScriptSuccess(prefix_2D + "m1.sampleImprovedNearbyPoint(runif(20), 0.2, 'n', 0.1); } ");
		SLiMAssertScriptSuccess(prefix_2D + "m1.sampleImprovedNearbyPoint(runif(20), 0.2, 't', 2, 0.1); } ");
		
		SLiMAssertScriptSuccess(prefix_2D + "m1.sampleNearbyPoint(runif(20), 0.2, 'f'); } ");
		SLiMAssertScriptSuccess(prefix_2D + "m1.sampleNearbyPoint(runif(20), 0.2, 'l'); } ");
		SLiMAssertScriptSuccess(prefix_2D + "m1.sampleNearbyPoint(runif(20), 0.2, 'e', 10.0); } ");
		SLiMAssertScriptSuccess(prefix_2D + "m1.sampleNearbyPoint(runif(20), 0.2, 'n', 0.1); } ");
		SLiMAssertScriptSuccess(prefix_2D + "m1.sampleNearbyPoint(runif(20), 0.2, 't', 2, 0.1); } ");
		
		SLiMAssertScriptSuccess(prefix_2D + "m1.smooth(0.1, 'f'); } ");
		SLiMAssertScriptSuccess(prefix_2D + "m1.smooth(0.1, 'l'); } ");
		SLiMAssertScriptSuccess(prefix_2D + "m1.smooth(0.1, 'e', 10.0); } ");
		SLiMAssertScriptSuccess(prefix_2D + "m1.smooth(0.1, 'n', 0.1); } ");
		SLiMAssertScriptSuccess(prefix_2D + "m1.smooth(0.1, 'c', 0.1); } ");
		SLiMAssertScriptSuccess(prefix_2D + "m1.smooth(0.1, 't', 2, 0.1); } ");
		SLiMAssertScriptSuccess(prefix_2D + "m1.interpolate(3, 'cubic'); m1.smooth(0.1, 'f'); } ");
		SLiMAssertScriptSuccess(prefix_2D + "m1.interpolate(3, 'cubic'); m1.smooth(0.1, 'l'); } ");
		SLiMAssertScriptSuccess(prefix_2D + "m1.interpolate(3, 'cubic'); m1.smooth(0.1, 'e', 10.0); } ");
		SLiMAssertScriptSuccess(prefix_2D + "m1.interpolate(3, 'cubic'); m1.smooth(0.1, 'n', 0.1); } ");
		SLiMAssertScriptSuccess(prefix_2D + "m1.interpolate(3, 'cubic'); m1.smooth(0.1, 'c', 0.1); } ");
		SLiMAssertScriptSuccess(prefix_2D + "m1.interpolate(3, 'cubic'); m1.smooth(0.1, 't', 2, 0.1); } ");
		
		SLiMAssertScriptSuccess(prefix_2D + "defineConstant('M1', m1); defineGlobal('M2', m2); } 2 early() { sim.addSubpop('p2', 10); p2.addSpatialMap(M1); p2.addSpatialMap(M2); } 3 early() { p1.removeSpatialMap('map1'); p2.removeSpatialMap(M2); } 4 early() { if (!identical(p1.spatialMaps, M2)) stop(); if (!identical(p2.spatialMaps, M1)) stop(); p2.removeSpatialMap('map1'); p1.removeSpatialMap(M2); }");
		
		//
		//	3D
		//
		std::string prefix_3D;
		
		if (periodic == 0)
			prefix_3D = "initialize() { initializeSLiMOptions(dimensionality='xyz'); } 1 early() { sim.addSubpop('p1', 10); mv1 = array(runif(120), dim=c(6, 5, 4)); mv2 = array(runif(120), dim=c(6, 5, 4)); m1 = p1.defineSpatialMap('map1', 'xyz', mv1); m2 = p1.defineSpatialMap('map2', 'xyz', mv2); ";
		else
			prefix_3D = "initialize() { initializeSLiMOptions(dimensionality='xyz', periodicity='xyz'); } 1 early() { sim.addSubpop('p1', 10); mv1 = array(runif(120), dim=c(6, 5, 4)); mv2 = array(runif(120), dim=c(6, 5, 4)); m1 = p1.defineSpatialMap('map1', 'xyz', mv1); m2 = p1.defineSpatialMap('map2', 'xyz', mv2); ";
		
		SLiMAssertScriptStop(prefix_3D + "f1 = m1.gridValues(); f2 = m2.gridValues(); if (identical(mv1, f1) & identical(mv2, f2)) stop(); } ");
		
		SLiMAssertScriptStop(prefix_3D + "m3 = SpatialMap('map3', m1); f3 = m3.gridValues(); if (identical(mv1, f3)) stop(); } ");
		
		SLiMAssertScriptStop(prefix_3D + "m1.add(17.3); if (identical(mv1 + 17.3, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_3D + "m1.add(mv2); if (identical(mv1 + mv2, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_3D + "m1.add(m2); if (identical(mv1 + mv2, m1.gridValues())) stop(); } ");
		
		SLiMAssertScriptStop(prefix_3D + "m1.blend(17.3, 0.0); if (identical(mv1, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_3D + "m1.blend(mv2, 0.0); if (identical(mv1, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_3D + "m1.blend(m2, 0.0); if (identical(mv1, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_3D + "m1.blend(17.3, 1.0); if (identical(array(rep(17.3, 120), dim=c(6, 5, 4)), m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_3D + "m1.blend(mv2, 1.0); if (identical(mv2, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_3D + "m1.blend(m2, 1.0); if (identical(mv2, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_3D + "m1.blend(17.3, 0.4); if (all(abs((mv1*0.6 + array(rep(17.3, 120), dim=c(6, 5, 4))*0.4) - m1.gridValues()) < 1e-15)) stop(); } ");
		SLiMAssertScriptStop(prefix_3D + "m1.blend(mv2, 0.4); if (all(abs((mv1*0.6 + mv2*0.4) - m1.gridValues()) < 1e-15)) stop(); } ");
		SLiMAssertScriptStop(prefix_3D + "m1.blend(m2, 0.4); if (all(abs((mv1*0.6 + mv2*0.4) - m1.gridValues()) < 1e-15)) stop(); } ");
		
		SLiMAssertScriptStop(prefix_3D + "m1.multiply(0.25); if (identical(mv1 * 0.25, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_3D + "m1.multiply(mv2); if (identical(mv1 * mv2, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_3D + "m1.multiply(m2); if (identical(mv1 * mv2, m1.gridValues())) stop(); } ");
		
		SLiMAssertScriptStop(prefix_3D + "m1.subtract(0.25); if (identical(mv1 - 0.25, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_3D + "m1.subtract(mv2); if (identical(mv1 - mv2, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_3D + "m1.subtract(m2); if (identical(mv1 - mv2, m1.gridValues())) stop(); } ");
		
		SLiMAssertScriptStop(prefix_3D + "m1.divide(0.25); if (identical(mv1 / 0.25, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_3D + "m1.divide(mv2); if (identical(mv1 / mv2, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_3D + "m1.divide(m2); if (identical(mv1 / mv2, m1.gridValues())) stop(); } ");
		
		SLiMAssertScriptStop(prefix_3D + "m1.power(0.25); if (identical(mv1 ^ 0.25, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_3D + "m1.power(mv2); if (identical(mv1 ^ mv2, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_3D + "m1.power(m2); if (identical(mv1 ^ mv2, m1.gridValues())) stop(); } ");
		
		SLiMAssertScriptStop(prefix_3D + "m1.exp(); if (identical(exp(mv1), m1.gridValues())) stop(); } ");
		
		SLiMAssertScriptSuccess(prefix_3D + "m1.changeColors(c(0.0, 1.0), c('black', 'white')); } ");
		SLiMAssertScriptSuccess(prefix_3D + "m1.changeColors(c(0.0, 1.0), c('black', 'white')); m1.changeColors(c(0.5, 0.8), c('red', 'blue')); } ");
		
		SLiMAssertScriptRaise(prefix_3D + "m1.changeValues(17.3); }", "does not match the spatiality", __LINE__);
		SLiMAssertScriptStop(prefix_3D + "mx = array(rep(17.3, 120), dim=c(6, 5, 4)); m1.changeValues(mx); if (identical(mx, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_3D + "m1.changeValues(mv2); if (identical(mv2, m1.gridValues())) stop(); } ");
		SLiMAssertScriptStop(prefix_3D + "m1.changeValues(m2); if (identical(mv2, m1.gridValues())) stop(); } ");
		
		SLiMAssertScriptStop(prefix_3D + "m1.interpolate(3, 'nearest'); if (identical(m1.gridDimensions, c(13, 16, 10))) stop(); } ");
		SLiMAssertScriptStop(prefix_3D + "m1.interpolate(3, 'linear'); if (identical(m1.gridDimensions, c(13, 16, 10))) stop(); } ");
		SLiMAssertScriptRaise(prefix_3D + "m1.interpolate(3, 'cubic'); if (identical(m1.gridDimensions, c(13, 16, 10))) stop(); } ", "not currently supported for 3D", __LINE__);
		
		SLiMAssertScriptSuccess(prefix_3D + "m1.changeColors(c(0.0, 1.0), c('red', 'black')); m1.mapColor(rnorm(50)); } ");
		
		/* mapImage() only generates 2D images
		 SLiMAssertScriptSuccess(prefix_3D + "m1.mapImage(centers=F, color=F); } ");
		 SLiMAssertScriptSuccess(prefix_3D + "m1.mapImage(centers=T, color=F); } ");
		 SLiMAssertScriptSuccess(prefix_3D + "m1.changeColors(c(0.0, 1.0), c('red', 'black')); m1.mapImage(centers=F, color=T); } ");
		 SLiMAssertScriptSuccess(prefix_3D + "m1.changeColors(c(0.0, 1.0), c('red', 'black')); m1.mapImage(centers=T, color=T); } ");
		 SLiMAssertScriptSuccess(prefix_3D + "m1.mapImage(10, 15, centers=F, color=F); } ");
		 SLiMAssertScriptSuccess(prefix_3D + "m1.mapImage(10, 15, centers=T, color=F); } ");
		 SLiMAssertScriptSuccess(prefix_3D + "m1.changeColors(c(0.0, 1.0), c('red', 'black')); m1.mapImage(10, 15, centers=F, color=T); } ");
		 SLiMAssertScriptSuccess(prefix_3D + "m1.changeColors(c(0.0, 1.0), c('red', 'black')); m1.mapImage(10, 15, centers=T, color=T); } ");
		 
		 SLiMAssertScriptSuccess(prefix_3D + "p1.spatialMapImage('map1', centers=F, color=F); } ");
		 SLiMAssertScriptSuccess(prefix_3D + "p1.spatialMapImage('map1', centers=T, color=F); } ");
		 SLiMAssertScriptSuccess(prefix_3D + "p1.spatialMapImage('map1', 10, 15, centers=F, color=F); } ");
		 SLiMAssertScriptSuccess(prefix_3D + "p1.spatialMapImage('map1', 10, 15, centers=T, color=F); } ");*/
		
		SLiMAssertScriptSuccess(prefix_3D + "m1.mapValue(runif(0)); } ");
		SLiMAssertScriptSuccess(prefix_3D + "m1.mapValue(runif(3)); } ");
		SLiMAssertScriptSuccess(prefix_3D + "m1.mapValue(runif(30)); } ");
		SLiMAssertScriptRaise(prefix_3D + "m1.mapValue(runif(31)); } ", "must match spatiality", __LINE__);
		
		SLiMAssertScriptSuccess(prefix_3D + "p1.spatialMapValue('map1', runif(0)); } ");
		SLiMAssertScriptSuccess(prefix_3D + "p1.spatialMapValue('map1', runif(3)); } ");
		SLiMAssertScriptSuccess(prefix_3D + "p1.spatialMapValue('map1', runif(30)); } ");
		SLiMAssertScriptRaise(prefix_3D + "p1.spatialMapValue('map1', runif(31)); } ", "must match spatiality", __LINE__);
		
		SLiMAssertScriptStop(prefix_3D + "if (identical(range(mv1), m1.range()) & identical(range(mv2), m2.range())) stop(); } ");
		
		SLiMAssertScriptStop(prefix_3D + "m1.rescale(); if (identical(c(0.0, 1.0), m1.range())) stop(); } ");
		SLiMAssertScriptStop(prefix_3D + "m1.rescale(0.2, 1.7); if (identical(c(0.2, 1.7), m1.range())) stop(); } ");
		
		SLiMAssertScriptSuccess(prefix_3D + "m1.sampleImprovedNearbyPoint(runif(30), 0.2, 'f'); } ");
		SLiMAssertScriptSuccess(prefix_3D + "m1.sampleImprovedNearbyPoint(runif(30), 0.2, 'l'); } ");
		SLiMAssertScriptSuccess(prefix_3D + "m1.sampleImprovedNearbyPoint(runif(30), 0.2, 'e', 10.0); } ");
		SLiMAssertScriptSuccess(prefix_3D + "m1.sampleImprovedNearbyPoint(runif(30), 0.2, 'n', 0.1); } ");
		SLiMAssertScriptRaise(prefix_3D + "m1.sampleImprovedNearbyPoint(runif(30), 0.2, 't', 3, 0.1); } ", "kernel type not supported", __LINE__);
		
		SLiMAssertScriptSuccess(prefix_3D + "m1.sampleNearbyPoint(runif(30), 0.2, 'f'); } ");
		SLiMAssertScriptSuccess(prefix_3D + "m1.sampleNearbyPoint(runif(30), 0.2, 'l'); } ");
		SLiMAssertScriptSuccess(prefix_3D + "m1.sampleNearbyPoint(runif(30), 0.2, 'e', 10.0); } ");
		SLiMAssertScriptSuccess(prefix_3D + "m1.sampleNearbyPoint(runif(30), 0.2, 'n', 0.1); } ");
		SLiMAssertScriptRaise(prefix_3D + "m1.sampleNearbyPoint(runif(30), 0.2, 't', 3, 0.1); } ", "kernel type not supported", __LINE__);
		
		SLiMAssertScriptSuccess(prefix_3D + "m1.smooth(0.1, 'f'); } ");
		SLiMAssertScriptSuccess(prefix_3D + "m1.smooth(0.1, 'l'); } ");
		SLiMAssertScriptSuccess(prefix_3D + "m1.smooth(0.1, 'e', 10.0); } ");
		SLiMAssertScriptSuccess(prefix_3D + "m1.smooth(0.1, 'n', 0.1); } ");
		SLiMAssertScriptSuccess(prefix_3D + "m1.smooth(0.1, 'c', 0.1); } ");
		SLiMAssertScriptSuccess(prefix_3D + "m1.smooth(0.1, 't', 3, 0.1); } ");
		SLiMAssertScriptSuccess(prefix_3D + "m1.interpolate(3, 'linear'); m1.smooth(0.1, 'f'); } ");	// linear not cubic, for 3D
		SLiMAssertScriptSuccess(prefix_3D + "m1.interpolate(3, 'linear'); m1.smooth(0.1, 'l'); } ");
		SLiMAssertScriptSuccess(prefix_3D + "m1.interpolate(3, 'linear'); m1.smooth(0.1, 'e', 10.0); } ");
		SLiMAssertScriptSuccess(prefix_3D + "m1.interpolate(3, 'linear'); m1.smooth(0.1, 'n', 0.1); } ");
		SLiMAssertScriptSuccess(prefix_3D + "m1.interpolate(3, 'linear'); m1.smooth(0.1, 'c', 0.1); } ");
		SLiMAssertScriptSuccess(prefix_3D + "m1.interpolate(3, 'linear'); m1.smooth(0.1, 't', 3, 0.1); } ");
		
		SLiMAssertScriptSuccess(prefix_3D + "defineConstant('M1', m1); defineGlobal('M2', m2); } 2 early() { sim.addSubpop('p2', 10); p2.addSpatialMap(M1); p2.addSpatialMap(M2); } 3 early() { p1.removeSpatialMap('map1'); p2.removeSpatialMap(M2); } 4 early() { if (!identical(p1.spatialMaps, M2)) stop(); if (!identical(p2.spatialMaps, M1)) stop(); p2.removeSpatialMap('map1'); p1.removeSpatialMap(M2); }");
	}
}

#pragma mark nonWF model tests
void _RunNonWFTests(void)
{
	// Test properties and methods that should be disabled in nonWF mode
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "1 early() { p1.setSubpopulationSize(500); } ", "not available in nonWF models", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "1 early() { p1.cloningRate; } ", "not available in nonWF models", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "1 early() { p1.setCloningRate(0.5); } ", "not available in nonWF models", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "1 early() { p1.selfingRate; } ", "not available in nonWF models", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "1 early() { p1.setSelfingRate(0.5); } ", "not available in nonWF models", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_sex_p1 + "1 early() { p1.sexRatio; } ", "not available in nonWF models", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_sex_p1 + "1 early() { p1.setSexRatio(0.5); } ", "not available in nonWF models", __LINE__);
	
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "1 early() { sim.addSubpopSplit(2, 100, p1); } ", "not available in nonWF models", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "1 early() { p1.immigrantSubpopFractions; } ", "not available in nonWF models", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "1 early() { p1.immigrantSubpopIDs; } ", "not available in nonWF models", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "1 early() { p1.setMigrationRates(2, 0.1); } ", "not available in nonWF models", __LINE__);
	
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "1 mateChoice() { return T; } ", "may not be defined in nonWF models", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "1 early() { sim.registerMateChoiceCallback(NULL, '{ return T; } '); } ", "not available in nonWF models", __LINE__);
	
	// Test properties and methods that should be disabled in WF mode
	SLiMAssertScriptRaise(WF_prefix + gen1_setup_p1 + "1 early() { p1.individuals.age; } ", "not available in WF models", __LINE__);
	
	SLiMAssertScriptRaise(WF_prefix + gen1_setup_p1 + "1 early() { p1.removeSubpopulation(); stop(); }", "not available in WF models", __LINE__);
	SLiMAssertScriptRaise(WF_prefix + gen1_setup_p1 + "1 early() { p1.takeMigrants(p1.individuals); stop(); }", "not available in WF models", __LINE__);
	SLiMAssertScriptRaise(WF_prefix + gen1_setup_p1 + "1 early() { p1.addCloned(p1.individuals[0]); stop(); }", "not available in WF models", __LINE__);
	SLiMAssertScriptRaise(WF_prefix + gen1_setup_p1 + "1 early() { p1.addCrossed(p1.individuals[0], p1.individuals[1]); stop(); }", "not available in WF models", __LINE__);
	SLiMAssertScriptRaise(WF_prefix + gen1_setup_p1 + "1 early() { p1.addEmpty(); stop(); }", "not available in WF models", __LINE__);
	SLiMAssertScriptRaise(WF_prefix + gen1_setup_p1 + "1 early() { p1.addSelfed(p1.individuals[0]); stop(); }", "not available in WF models", __LINE__);
	
	SLiMAssertScriptRaise(WF_prefix + gen1_setup_p1 + "1 reproduction() { return; } ", "may not be defined in WF models", __LINE__);
	
	// Community.modelType
	SLiMAssertScriptStop(gen1_setup + "1 early() { if (community.modelType == 'WF') stop(); } ", __LINE__);
	SLiMAssertScriptStop(nonWF_prefix + gen1_setup + "1 early() { if (community.modelType == 'nonWF') stop(); } ", __LINE__);
	SLiMAssertScriptStop(nonWF_prefix + gen1_setup_sex + "1 early() { if (community.modelType == 'nonWF') stop(); } ", __LINE__);
	
	// Individual.age
	SLiMAssertScriptStop(nonWF_prefix + gen1_setup_p1 + "1 early() { p1.individuals.age; stop(); } ", __LINE__);
	
	// Individual.meanParentAge
	SLiMAssertScriptStop(nonWF_prefix + pedigrees_prefix + gen1_setup_p1 + "1 early() { p1.individuals.meanParentAge; stop(); } ", __LINE__);
	
	// Subpopulation - (void)takeMigrants() and sampleIndividuals()
	SLiMAssertScriptStop(nonWF_prefix + gen1_setup_p1p2p3_100 + "2:10 first() { s = c(p2,p3).sampleIndividuals(1); p1.takeMigrants(s); stop(); } ", __LINE__);
	SLiMAssertScriptStop(nonWF_prefix + gen1_setup_p1p2p3_100 + "2:10 early() { s = c(p2,p3).sampleIndividuals(1); p1.takeMigrants(s); stop(); } ", __LINE__);
	SLiMAssertScriptStop(nonWF_prefix + gen1_setup_p1p2p3_100 + "2:10 late() { s = c(p2,p3).sampleIndividuals(1); p1.takeMigrants(s); stop(); } ", __LINE__);
	SLiMAssertScriptStop(nonWF_prefix + gen1_setup_p1p2p3_100 + "2:10 first() { s = c(p2,p3).sampleIndividuals(1, migrant=F); p1.takeMigrants(s); stop(); } ", __LINE__);
	SLiMAssertScriptStop(nonWF_prefix + gen1_setup_p1p2p3_100 + "2:10 early() { s = c(p2,p3).sampleIndividuals(1, migrant=F); p1.takeMigrants(s); stop(); } ", __LINE__);
	SLiMAssertScriptStop(nonWF_prefix + gen1_setup_p1p2p3_100 + "2:10 late() { s = c(p2,p3).sampleIndividuals(1, migrant=F); p1.takeMigrants(s); stop(); } ", __LINE__);
	SLiMAssertScriptStop(nonWF_prefix + gen1_setup_p1p2p3_100 + "2:10 first() { s = c(p2,p3).sampleIndividuals(40); p1.takeMigrants(s); stop(); } ", __LINE__);
	SLiMAssertScriptStop(nonWF_prefix + gen1_setup_p1p2p3_100 + "2:10 early() { s = c(p2,p3).sampleIndividuals(40); p1.takeMigrants(s); stop(); } ", __LINE__);
	SLiMAssertScriptStop(nonWF_prefix + gen1_setup_p1p2p3_100 + "2:10 late() { s = c(p2,p3).sampleIndividuals(40); p1.takeMigrants(s); stop(); } ", __LINE__);
	SLiMAssertScriptStop(nonWF_prefix + gen1_setup_p1p2p3_100 + "2:10 first() { s = c(p2,p3).sampleIndividuals(40, migrant=F); p1.takeMigrants(s); stop(); } ", __LINE__);
	SLiMAssertScriptStop(nonWF_prefix + gen1_setup_p1p2p3_100 + "2:10 early() { s = c(p2,p3).sampleIndividuals(40, migrant=F); p1.takeMigrants(s); stop(); } ", __LINE__);
	SLiMAssertScriptStop(nonWF_prefix + gen1_setup_p1p2p3_100 + "2:10 late() { s = c(p2,p3).sampleIndividuals(40, migrant=F); p1.takeMigrants(s); stop(); } ", __LINE__);
	
	// Subpopulation - (void)removeSubpopulation()
	SLiMAssertScriptStop(nonWF_prefix + gen1_setup_p1 + "1 early() { p1.removeSubpopulation(); stop(); }", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "1 early() { p1.removeSubpopulation(); if (p1.individualCount == 10) stop(); }", "undefined identifier", __LINE__);		// the symbol is undefined immediately
	SLiMAssertScriptStop(nonWF_prefix + gen1_setup_p1 + "1 early() { px=p1; p1.removeSubpopulation(); if (px.individualCount == 10) stop(); }", __LINE__);									// does not take visible effect until generating children
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "1 early() { p1.removeSubpopulation(); } 2 early() { if (p1.individualCount == 0) stop(); }", "undefined identifier", __LINE__);
	
	// Test that deferred generation of offspring genomes does not cause vulnerabilities in properties/methods
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "2 reproduction() { offspring = p1.addCloned(individual, defer=T); offspring.uniqueMutations; }", "deferred genomes", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_highmut_p1 + "2 reproduction() { offspring = p1.addCloned(individual, defer=T); offspring.containsMutations(sim.mutations); }", "deferred genomes", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "2 reproduction() { offspring = p1.addCloned(individual, defer=T); offspring.countOfMutationsOfType(m1); }", "deferred genomes", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "2 reproduction() { offspring = p1.addCloned(individual, defer=T); offspring.sumOfMutationsOfType(m1); }", "deferred genomes", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "2 reproduction() { offspring = p1.addCloned(individual, defer=T); offspring.uniqueMutationsOfType(m1); }", "deferred genomes", __LINE__);
	
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "2 reproduction() { offspring = p1.addCloned(individual, defer=T); offspring.genomes.mutations; }", "deferred genomes", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_highmut_p1 + "2 reproduction() { offspring = p1.addCloned(individual, defer=T); offspring.genomes.addMutations(sim.mutations); }", "deferred genomes", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "2 reproduction() { offspring = p1.addCloned(individual, defer=T); offspring.genomes.addNewDrawnMutation(m1, 10); }", "deferred genomes", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "2 reproduction() { offspring = p1.addCloned(individual, defer=T); offspring.genomes.addNewMutation(m1, 0.0, 10); }", "deferred genomes", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "2 reproduction() { offspring = p1.addCloned(individual, defer=T); offspring.genomes.containsMarkerMutation(m1, 10); }", "deferred genomes", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_highmut_p1 + "2 reproduction() { offspring = p1.addCloned(individual, defer=T); offspring.genomes.containsMutations(sim.mutations); }", "deferred genomes", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "2 reproduction() { offspring = p1.addCloned(individual, defer=T); offspring.genomes.countOfMutationsOfType(m1); }", "deferred genomes", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "2 reproduction() { offspring = p1.addCloned(individual, defer=T); offspring.genomes.mutationCountsInGenomes(); }", "deferred genomes", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "2 reproduction() { offspring = p1.addCloned(individual, defer=T); offspring.genomes.mutationFrequenciesInGenomes(); }", "deferred genomes", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "2 reproduction() { offspring = p1.addCloned(individual, defer=T); offspring.genomes.mutationsOfType(m1); }", "deferred genomes", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "2 reproduction() { offspring = p1.addCloned(individual, defer=T); offspring.genomes.nucleotides(); }", "deferred genomes", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "2 reproduction() { offspring = p1.addCloned(individual, defer=T); offspring.genomes.output(); }", "deferred genomes", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "2 reproduction() { offspring = p1.addCloned(individual, defer=T); offspring.genomes.outputMS(); }", "deferred genomes", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "2 reproduction() { offspring = p1.addCloned(individual, defer=T); offspring.genomes.outputVCF(); }", "deferred genomes", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "2 reproduction() { offspring = p1.addCloned(individual, defer=T); offspring.genomes.positionsOfMutationsOfType(m1); }", "deferred genomes", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "2 reproduction() { offspring = p1.addCloned(individual, defer=T); offspring.genomes.readFromMS('foo', m1); }", "deferred genomes", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "2 reproduction() { offspring = p1.addCloned(individual, defer=T); offspring.genomes.readFromVCF('foo'); }", "deferred genomes", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "2 reproduction() { offspring = p1.addCloned(individual, defer=T); offspring.genomes.removeMutations(); }", "deferred genomes", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "2 reproduction() { offspring = p1.addCloned(individual, defer=T); offspring.genomes.sumOfMutationsOfType(m1); }", "deferred genomes", __LINE__);
}

#pragma mark treeseq tests
void _RunTreeSeqTests(const std::string &temp_path)
{
	// initializeTreeSeq()
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(); } " + gen1_setup_p1 + "100 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=F, simplificationRatio=10.0, checkCoalescence=F, runCrosschecks=F); } " + gen1_setup_p1 + "100 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=T, simplificationRatio=10.0, checkCoalescence=F, runCrosschecks=F); } " + gen1_setup_p1 + "100 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=F, simplificationRatio=INF, checkCoalescence=F, runCrosschecks=F); } " + gen1_setup_p1 + "100 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=T, simplificationRatio=INF, checkCoalescence=F, runCrosschecks=F); } " + gen1_setup_p1 + "100 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=F, simplificationRatio=0.0, checkCoalescence=F, runCrosschecks=F); } " + gen1_setup_p1 + "100 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=T, simplificationRatio=0.0, checkCoalescence=F, runCrosschecks=F); } " + gen1_setup_p1 + "100 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=F, simplificationRatio=10.0, checkCoalescence=T, runCrosschecks=F); } " + gen1_setup_p1 + "100 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=T, simplificationRatio=10.0, checkCoalescence=T, runCrosschecks=F); } " + gen1_setup_p1 + "100 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=F, simplificationRatio=INF, checkCoalescence=T, runCrosschecks=F); } " + gen1_setup_p1 + "100 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=T, simplificationRatio=INF, checkCoalescence=T, runCrosschecks=F); } " + gen1_setup_p1 + "100 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=F, simplificationRatio=0.0, checkCoalescence=T, runCrosschecks=F); } " + gen1_setup_p1 + "100 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=T, simplificationRatio=0.0, checkCoalescence=T, runCrosschecks=F); } " + gen1_setup_p1 + "100 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=F, simplificationRatio=10.0, checkCoalescence=F, runCrosschecks=T); } " + gen1_setup_p1 + "100 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=T, simplificationRatio=10.0, checkCoalescence=F, runCrosschecks=T); } " + gen1_setup_p1 + "100 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=F, simplificationRatio=INF, checkCoalescence=F, runCrosschecks=T); } " + gen1_setup_p1 + "100 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=T, simplificationRatio=INF, checkCoalescence=F, runCrosschecks=T); } " + gen1_setup_p1 + "100 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=F, simplificationRatio=0.0, checkCoalescence=F, runCrosschecks=T); } " + gen1_setup_p1 + "100 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=T, simplificationRatio=0.0, checkCoalescence=F, runCrosschecks=T); } " + gen1_setup_p1 + "100 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=F, simplificationRatio=10.0, checkCoalescence=T, runCrosschecks=T); } " + gen1_setup_p1 + "100 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=T, simplificationRatio=10.0, checkCoalescence=T, runCrosschecks=T); } " + gen1_setup_p1 + "100 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=F, simplificationRatio=INF, checkCoalescence=T, runCrosschecks=T); } " + gen1_setup_p1 + "100 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=T, simplificationRatio=INF, checkCoalescence=T, runCrosschecks=T); } " + gen1_setup_p1 + "100 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=F, simplificationRatio=0.0, checkCoalescence=T, runCrosschecks=T); } " + gen1_setup_p1 + "100 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=T, simplificationRatio=0.0, checkCoalescence=T, runCrosschecks=T); } " + gen1_setup_p1 + "100 early() { stop(); }", __LINE__);
	
	// treeSeqCoalesced()
	SLiMAssertScriptRaise("initialize() { initializeTreeSeq(); } " + gen1_setup_p1 + "1: early() { sim.treeSeqCoalesced(); } 100 early() { stop(); }", "coalescence checking is enabled", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(checkCoalescence=T); } " + gen1_setup_p1 + "1: early() { sim.treeSeqCoalesced(); } 100 early() { stop(); }", __LINE__);
	
	// treeSeqSimplify()
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(); } " + gen1_setup_p1 + "50 early() { sim.treeSeqSimplify(); } 100 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(); } " + gen1_setup_p1 + "1: early() { sim.treeSeqSimplify(); } 100 early() { stop(); }", __LINE__);
	
	// treeSeqRememberIndividuals()
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(); } " + gen1_setup_p1 + "50 early() { sim.treeSeqRememberIndividuals(p1.individuals[integer(0)]); } 100 early() { sim.treeSeqSimplify(); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(); } " + gen1_setup_p1 + "50 early() { sim.treeSeqRememberIndividuals(p1.individuals); } 100 early() { sim.treeSeqSimplify(); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(); } " + gen1_setup_p1 + "1: early() { sim.treeSeqRememberIndividuals(p1.individuals); } 100 early() { sim.treeSeqSimplify(); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(); } " + gen1_setup_p1 + "50 early() { sim.treeSeqRememberIndividuals(p1.individuals, permanent=F); } 100 early() { sim.treeSeqSimplify(); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(); } " + gen1_setup_p1 + "1: early() { sim.treeSeqRememberIndividuals(p1.individuals, permanent=F); } 100 early() { sim.treeSeqSimplify(); stop(); }", __LINE__);
	
	// treeSeqOutput()
	if (Eidos_TemporaryDirectoryExists())
	{
		SLiMAssertScriptStop("initialize() { initializeTreeSeq(); } " + gen1_setup_p1 + "100 early() { sim.treeSeqOutput('" + temp_path + "/SLiM_treeSeq_1.trees', simplify=F, _binary=F); stop(); }", __LINE__);
		SLiMAssertScriptStop("initialize() { initializeTreeSeq(); } " + gen1_setup_p1 + "100 early() { sim.treeSeqOutput('" + temp_path + "/SLiM_treeSeq_2.trees', simplify=T, _binary=F); stop(); }", __LINE__);
		SLiMAssertScriptStop("initialize() { initializeTreeSeq(); } " + gen1_setup_p1 + "100 early() { sim.treeSeqOutput('" + temp_path + "/SLiM_treeSeq_3.trees', simplify=F, _binary=T); stop(); }", __LINE__);
		SLiMAssertScriptStop("initialize() { initializeTreeSeq(); } " + gen1_setup_p1 + "100 early() { sim.treeSeqOutput('" + temp_path + "/SLiM_treeSeq_4.trees', simplify=T, _binary=T); stop(); }", __LINE__);
		
		SLiMAssertScriptStop("initialize() { initializeTreeSeq(); } " + gen1_setup_p1 + "100 early() { sim.treeSeqOutput('" + temp_path + "/SLiM_treeSeq_1.trees', simplify=F, includeModel=F, _binary=F); stop(); }", __LINE__);
		SLiMAssertScriptStop("initialize() { initializeTreeSeq(); } " + gen1_setup_p1 + "100 early() { sim.treeSeqOutput('" + temp_path + "/SLiM_treeSeq_2.trees', simplify=T, includeModel=F, _binary=F); stop(); }", __LINE__);
		SLiMAssertScriptStop("initialize() { initializeTreeSeq(); } " + gen1_setup_p1 + "100 early() { sim.treeSeqOutput('" + temp_path + "/SLiM_treeSeq_3.trees', simplify=F, includeModel=F, _binary=T); stop(); }", __LINE__);
		SLiMAssertScriptStop("initialize() { initializeTreeSeq(); } " + gen1_setup_p1 + "100 early() { sim.treeSeqOutput('" + temp_path + "/SLiM_treeSeq_4.trees', simplify=T, includeModel=F, _binary=T); stop(); }", __LINE__);
	}
}

#pragma mark Nucleotide API tests
void _RunNucleotideFunctionTests(void)
{
	// nucleotidesToCodons()
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(nucleotidesToCodons(string(0)), integer(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(nucleotidesToCodons(integer(0)), integer(0))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { nucleotidesToCodons('A'); }", "multiple of three in length", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { nucleotidesToCodons(0); }", "multiple of three in length", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { nucleotidesToCodons('AA'); }", "multiple of three in length", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { nucleotidesToCodons(c(0,0)); }", "multiple of three in length", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (nucleotidesToCodons('AAA') == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (nucleotidesToCodons(c('A','A','A')) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (nucleotidesToCodons(c(0,0,0)) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (nucleotidesToCodons('AAC') == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (nucleotidesToCodons(c('A','A','C')) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (nucleotidesToCodons(c(0,0,1)) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (nucleotidesToCodons('AAG') == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (nucleotidesToCodons(c('A','A','G')) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (nucleotidesToCodons(c(0,0,2)) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (nucleotidesToCodons('AAT') == 3) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (nucleotidesToCodons(c('A','A','T')) == 3) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (nucleotidesToCodons(c(0,0,3)) == 3) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (nucleotidesToCodons('ACA') == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (nucleotidesToCodons(c('A','C','A')) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (nucleotidesToCodons(c(0,1,0)) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (nucleotidesToCodons('CAA') == 16) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (nucleotidesToCodons(c('C','A','A')) == 16) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (nucleotidesToCodons(c(1,0,0)) == 16) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (nucleotidesToCodons('TTT') == 63) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (nucleotidesToCodons(c('T','T','T')) == 63) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (nucleotidesToCodons(c(3,3,3)) == 63) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { if (nucleotidesToCodons('AAAA') == 0) stop(); }", "multiple of three in length", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { if (nucleotidesToCodons(c(0,0,0,0)) == 0) stop(); }", "multiple of three in length", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(nucleotidesToCodons('AAAAACAAGAATTTT'), c(0,1,2,3,63))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(nucleotidesToCodons(c('A','A','A','A','A','C','A','A','G','A','A','T','T','T','T')), c(0,1,2,3,63))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(nucleotidesToCodons(c(0,0,0,0,0,1,0,0,2,0,0,3,3,3,3)), c(0,1,2,3,63))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { nucleotidesToCodons('ADA'); }", "requires string sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { nucleotidesToCodons(c('A','D','A')); }", "requires string sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { nucleotidesToCodons(c(0,-1,0)); }", "requires integer sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { nucleotidesToCodons(c(0,4,0)); }", "requires integer sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { nucleotidesToCodons('AAAADA'); }", "requires string sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { nucleotidesToCodons(c('A','A','A','A','D','A')); }", "requires string sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { nucleotidesToCodons(c(0,0,0,0,-1,0)); }", "requires integer sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { nucleotidesToCodons(c(0,0,0,0,4,0)); }", "requires integer sequence values", __LINE__);
	
	// codonsToAminoAcids()
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToAminoAcids(integer(0), long=F, paste=T), '')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToAminoAcids(integer(0), long=T, paste=T), '')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToAminoAcids(integer(0), long=0, paste=T), integer(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToAminoAcids(integer(0), long=F, paste=F), string(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToAminoAcids(integer(0), long=T, paste=F), string(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToAminoAcids(integer(0), long=0, paste=F), integer(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToAminoAcids(0, long=F, paste=T), 'K')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToAminoAcids(0, long=T, paste=T), 'Lys')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToAminoAcids(0, long=0, paste=T), 12)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToAminoAcids(0, long=F, paste=F), 'K')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToAminoAcids(0, long=T, paste=F), 'Lys')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToAminoAcids(0, long=0, paste=F), 12)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToAminoAcids(c(0,1,63), long=F, paste=T), 'KNF')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToAminoAcids(c(0,1,63), long=T, paste=T), 'Lys-Asn-Phe')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToAminoAcids(c(0,1,63), long=0, paste=T), c(12, 3, 14))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToAminoAcids(c(0,1,63), long=F, paste=F), c('K','N','F'))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToAminoAcids(c(0,1,63), long=T, paste=F), c('Lys', 'Asn', 'Phe'))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToAminoAcids(c(0,1,63), long=0, paste=F), c(12, 3, 14))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { codonsToAminoAcids(-1, long=F, paste=T); }", "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { codonsToAminoAcids(-1, long=T, paste=T); }", "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { codonsToAminoAcids(-1, long=0, paste=T); }", "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { codonsToAminoAcids(-1, long=F, paste=F); }", "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { codonsToAminoAcids(-1, long=T, paste=F); }", "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { codonsToAminoAcids(-1, long=0, paste=F); }", "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { codonsToAminoAcids(64, long=F, paste=T); }", "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { codonsToAminoAcids(64, long=T, paste=T); }", "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { codonsToAminoAcids(64, long=0, paste=T); }", "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { codonsToAminoAcids(64, long=F, paste=F); }", "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { codonsToAminoAcids(64, long=T, paste=F); }", "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { codonsToAminoAcids(64, long=0, paste=F); }", "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { codonsToAminoAcids(c(0,-1), long=F, paste=T); }", "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { codonsToAminoAcids(c(0,-1), long=T, paste=T); }", "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { codonsToAminoAcids(c(0,-1), long=0, paste=T); }", "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { codonsToAminoAcids(c(0,-1), long=F, paste=F); }", "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { codonsToAminoAcids(c(0,-1), long=T, paste=F); }", "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { codonsToAminoAcids(c(0,-1), long=0, paste=F); }", "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { codonsToAminoAcids(c(0,64), long=F, paste=T); }", "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { codonsToAminoAcids(c(0,64), long=T, paste=T); }", "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { codonsToAminoAcids(c(0,64), long=0, paste=T); }", "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { codonsToAminoAcids(c(0,64), long=F, paste=F); }", "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { codonsToAminoAcids(c(0,64), long=T, paste=F); }", "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { codonsToAminoAcids(c(0,64), long=0, paste=F); }", "requires codons to be", __LINE__);
	
	// mm16To256()
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { mm16To256(rep(0.0,15)); }", "to be of length 16", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { mm16To256(rep(0.0,16)); }", "to be a 4x4 matrix", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(mm16To256(matrix(rep(0.0,16), ncol=4)), matrix(rep(0.0,256),ncol=4))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(mm16To256(matrix(rep(0.25,16), ncol=4)), matrix(rep(0.25,256),ncol=4))) stop(); }", __LINE__);
	
	// mmJukesCantor()
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { mmJukesCantor(-0.1); }", "requires alpha >= 0.0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { mmJukesCantor(0.35); }", "requires 3 * alpha <= 1.0", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(mmJukesCantor(0.0), matrix(rep(0.0,16),ncol=4))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(mmJukesCantor(0.25), matrix(c(0.0, 0.25, 0.25, 0.25, 0.25, 0.0, 0.25, 0.25, 0.25, 0.25, 0.0, 0.25, 0.25, 0.25, 0.25, 0.0),ncol=4))) stop(); }", __LINE__);
	
	// mmKimura()
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { mmKimura(-0.1, 0.5); }", "requires alpha to be in", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { mmKimura(1.1, 0.5); }", "requires alpha to be in", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { mmKimura(0.5, -0.1); }", "requires beta to be in", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { mmKimura(0.5, 1.1); }", "requires beta to be in", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(mmKimura(0.0, 0.0), matrix(rep(0.0,16),ncol=4))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(mmKimura(0.5, 0.25), matrix(c(0.0, 0.25, 0.5, 0.25, 0.25, 0.0, 0.25, 0.5, 0.5, 0.25, 0.0, 0.25, 0.25, 0.5, 0.25, 0.0),ncol=4))) stop(); }", __LINE__);
	
	// nucleotideCounts()
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(nucleotideCounts(string(0)), c(0,0,0,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(nucleotideCounts(integer(0)), c(0,0,0,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(nucleotideCounts('A'), c(1,0,0,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(nucleotideCounts('C'), c(0,1,0,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(nucleotideCounts('G'), c(0,0,1,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(nucleotideCounts('T'), c(0,0,0,1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(nucleotideCounts(0), c(1,0,0,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(nucleotideCounts(1), c(0,1,0,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(nucleotideCounts(2), c(0,0,1,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(nucleotideCounts(3), c(0,0,0,1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(nucleotideCounts('ACGT'), c(1,1,1,1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(nucleotideCounts(c('A','C','G','T')), c(1,1,1,1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(nucleotideCounts(c(0,1,2,3)), c(1,1,1,1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(nucleotideCounts('AACACGATCG'), c(4,3,2,1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(nucleotideCounts(c('A','A','C','A','C','G','A','T','C','G')), c(4,3,2,1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(nucleotideCounts(c(0,0,1,0,1,2,0,3,1,2)), c(4,3,2,1))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { nucleotideCounts('ADA'); }", "requires string sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { nucleotideCounts(c('A','D','A')); }", "requires string sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { nucleotideCounts(c(0,-1,0)); }", "requires integer sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { nucleotideCounts(c(0,4,0)); }", "requires integer sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { nucleotideCounts('AAAADA'); }", "requires string sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { nucleotideCounts(c('A','A','A','A','D','A')); }", "requires string sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { nucleotideCounts(c(0,0,0,0,-1,0)); }", "requires integer sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { nucleotideCounts(c(0,0,0,0,4,0)); }", "requires integer sequence values", __LINE__);
	
	// nucleotideFrequencies()
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (all(isNAN(nucleotideFrequencies(string(0))))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (all(isNAN(nucleotideFrequencies(integer(0))))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(nucleotideFrequencies('A'), c(1.0,0,0,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(nucleotideFrequencies('C'), c(0,1.0,0,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(nucleotideFrequencies('G'), c(0,0,1.0,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(nucleotideFrequencies('T'), c(0,0,0,1.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(nucleotideFrequencies(0), c(1.0,0,0,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(nucleotideFrequencies(1), c(0,1.0,0,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(nucleotideFrequencies(2), c(0,0,1.0,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(nucleotideFrequencies(3), c(0,0,0,1.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(nucleotideFrequencies('ACGT'), c(0.25,0.25,0.25,0.25))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(nucleotideFrequencies(c('A','C','G','T')), c(0.25,0.25,0.25,0.25))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(nucleotideFrequencies(c(0,1,2,3)), c(0.25,0.25,0.25,0.25))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(nucleotideFrequencies('AACACGATCG'), c(0.4,0.3,0.2,0.1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(nucleotideFrequencies(c('A','A','C','A','C','G','A','T','C','G')), c(0.4,0.3,0.2,0.1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(nucleotideFrequencies(c(0,0,1,0,1,2,0,3,1,2)), c(0.4,0.3,0.2,0.1))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { nucleotideFrequencies('ADA'); }", "requires string sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { nucleotideFrequencies(c('A','D','A')); }", "requires string sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { nucleotideFrequencies(c(0,-1,0)); }", "requires integer sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { nucleotideFrequencies(c(0,4,0)); }", "requires integer sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { nucleotideFrequencies('AAAADA'); }", "requires string sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { nucleotideFrequencies(c('A','A','A','A','D','A')); }", "requires string sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { nucleotideFrequencies(c(0,0,0,0,-1,0)); }", "requires integer sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { nucleotideFrequencies(c(0,0,0,0,4,0)); }", "requires integer sequence values", __LINE__);
	
	// randomNucleotides()
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(randomNucleotides(0, format='string'), string(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(randomNucleotides(0, format='char'), string(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(randomNucleotides(0, format='integer'), integer(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { setSeed(0); if (identical(randomNucleotides(1, format='string'), 'A')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { setSeed(1); if (identical(randomNucleotides(1, format='char'), 'T')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { setSeed(2); if (identical(randomNucleotides(1, format='integer'), 2)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { setSeed(3); if (identical(randomNucleotides(10, format='string'), 'ACACATATGA')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { setSeed(4); if (identical(randomNucleotides(10, format='char'), c('A','G','C','A','C','T','C','G','C','T'))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { setSeed(5); if (identical(randomNucleotides(10, format='integer'), c(2,2,0,1,2,2,0,2,1,3))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { setSeed(0); if (identical(randomNucleotides(1, basis=c(1.0,0,0,0), format='string'), 'A')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { setSeed(0); if (identical(randomNucleotides(1, basis=c(1.0,0,0,0), format='char'), 'A')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { setSeed(0); if (identical(randomNucleotides(1, basis=c(1.0,0,0,0), format='integer'), 0)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { setSeed(0); if (identical(randomNucleotides(1, basis=c(0,1.0,0,0), format='string'), 'C')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { setSeed(0); if (identical(randomNucleotides(1, basis=c(0,1.0,0,0), format='char'), 'C')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { setSeed(0); if (identical(randomNucleotides(1, basis=c(0,1.0,0,0), format='integer'), 1)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { setSeed(0); if (identical(randomNucleotides(1, basis=c(0,0,1.0,0), format='string'), 'G')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { setSeed(0); if (identical(randomNucleotides(1, basis=c(0,0,1.0,0), format='char'), 'G')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { setSeed(0); if (identical(randomNucleotides(1, basis=c(0,0,1.0,0), format='integer'), 2)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { setSeed(0); if (identical(randomNucleotides(1, basis=c(0,0,0,1.0), format='string'), 'T')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { setSeed(0); if (identical(randomNucleotides(1, basis=c(0,0,0,1.0), format='char'), 'T')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { setSeed(0); if (identical(randomNucleotides(1, basis=c(0,0,0,1.0), format='integer'), 3)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { setSeed(0); if (identical(randomNucleotides(10, basis=c(1.0,0,0,0), format='string'), 'AAAAAAAAAA')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { setSeed(0); if (identical(randomNucleotides(10, basis=c(1.0,0,0,0), format='char'), rep('A',10))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { setSeed(0); if (identical(randomNucleotides(10, basis=c(1.0,0,0,0), format='integer'), rep(0,10))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { setSeed(0); if (identical(randomNucleotides(10, basis=c(0,1.0,0,0), format='string'), 'CCCCCCCCCC')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { setSeed(0); if (identical(randomNucleotides(10, basis=c(0,1.0,0,0), format='char'), rep('C',10))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { setSeed(0); if (identical(randomNucleotides(10, basis=c(0,1.0,0,0), format='integer'), rep(1,10))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { setSeed(0); if (identical(randomNucleotides(10, basis=c(0,0,1.0,0), format='string'), 'GGGGGGGGGG')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { setSeed(0); if (identical(randomNucleotides(10, basis=c(0,0,1.0,0), format='char'), rep('G',10))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { setSeed(0); if (identical(randomNucleotides(10, basis=c(0,0,1.0,0), format='integer'), rep(2,10))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { setSeed(0); if (identical(randomNucleotides(10, basis=c(0,0,0,1.0), format='string'), 'TTTTTTTTTT')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { setSeed(0); if (identical(randomNucleotides(10, basis=c(0,0,0,1.0), format='char'), rep('T',10))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { setSeed(0); if (identical(randomNucleotides(10, basis=c(0,0,0,1.0), format='integer'), rep(3,10))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { setSeed(0); if (identical(randomNucleotides(100, basis=c(10.0,1.0,2.0,3.0), format='string'), 'ATAAAAAAAGAAATAAACTATGAATATCATAAAATACAAAATAAAATAATTTGTAAGAGTAAATTATTAGTATGAATCTAACATAATAAAAAATAATATA')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { setSeed(0); if (identical(randomNucleotides(100, basis=c(10.0,1.0,2.0,3.0), format='char'), c('A','T','A','A','A','A','A','A','A','G','A','A','A','T','A','A','A','C','T','A','T','G','A','A','T','A','T','C','A','T','A','A','A','A','T','A','C','A','A','A','A','T','A','A','A','A','T','A','A','T','T','T','G','T','A','A','G','A','G','T','A','A','A','T','T','A','T','T','A','G','T','A','T','G','A','A','T','C','T','A','A','C','A','T','A','A','T','A','A','A','A','A','A','T','A','A','T','A','T','A'))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { setSeed(0); if (identical(randomNucleotides(100, basis=c(10.0,1.0,2.0,3.0), format='integer'), c(0,3,0,0,0,0,0,0,0,2,0,0,0,3,0,0,0,1,3,0,3,2,0,0,3,0,3,1,0,3,0,0,0,0,3,0,1,0,0,0,0,3,0,0,0,0,3,0,0,3,3,3,2,3,0,0,2,0,2,3,0,0,0,3,3,0,3,3,0,2,3,0,3,2,0,0,3,1,3,0,0,1,0,3,0,0,3,0,0,0,0,0,0,3,0,0,3,0,3,0))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { randomNucleotides(-1); }", "requires length to be in [0, 2e9]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { randomNucleotides(0, basis=3.0); }", "requires basis to be either", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { randomNucleotides(0, basis=c(0.0,0.0,0.0,0.0)); }", "requires at least one basis value to be > 0.0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { randomNucleotides(0, basis=c(0.0,0.0,0.2,-0.1)); }", "requires basis values to be finite and >= 0.0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { randomNucleotides(0, basis=c(0.0,0.0,0.2,INF)); }", "requires basis values to be finite and >= 0.0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { randomNucleotides(0, basis=c(0.0,0.0,0.2,NAN)); }", "requires basis values to be finite and >= 0.0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { randomNucleotides(0, basis=c(0.0,0.0,0.2,0.0), format='foo'); }", "requires a format of", __LINE__);
	
	// codonsToNucleotides()
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToNucleotides(integer(0), format='string'), '')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToNucleotides(integer(0), format='char'), string(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToNucleotides(integer(0), format='integer'), integer(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToNucleotides(0, format='string'), 'AAA')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToNucleotides(1, format='string'), 'AAC')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToNucleotides(2, format='string'), 'AAG')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToNucleotides(3, format='string'), 'AAT')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToNucleotides(4, format='string'), 'ACA')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToNucleotides(8, format='string'), 'AGA')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToNucleotides(12, format='string'), 'ATA')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToNucleotides(16, format='string'), 'CAA')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToNucleotides(32, format='string'), 'GAA')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToNucleotides(48, format='string'), 'TAA')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToNucleotides(63, format='string'), 'TTT')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToNucleotides(0, format='char'), c('A','A','A'))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToNucleotides(1, format='char'), c('A','A','C'))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToNucleotides(2, format='char'), c('A','A','G'))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToNucleotides(3, format='char'), c('A','A','T'))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToNucleotides(4, format='char'), c('A','C','A'))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToNucleotides(8, format='char'), c('A','G','A'))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToNucleotides(12, format='char'), c('A','T','A'))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToNucleotides(16, format='char'), c('C','A','A'))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToNucleotides(32, format='char'), c('G','A','A'))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToNucleotides(48, format='char'), c('T','A','A'))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToNucleotides(63, format='char'), c('T','T','T'))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToNucleotides(0, format='integer'), c(0,0,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToNucleotides(1, format='integer'), c(0,0,1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToNucleotides(2, format='integer'), c(0,0,2))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToNucleotides(3, format='integer'), c(0,0,3))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToNucleotides(4, format='integer'), c(0,1,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToNucleotides(8, format='integer'), c(0,2,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToNucleotides(12, format='integer'), c(0,3,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToNucleotides(16, format='integer'), c(1,0,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToNucleotides(32, format='integer'), c(2,0,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToNucleotides(48, format='integer'), c(3,0,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToNucleotides(63, format='integer'), c(3,3,3))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToNucleotides(0:5, format='string'), 'AAAAACAAGAATACAACC')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToNucleotides(0:5, format='char'), c('A','A','A','A','A','C','A','A','G','A','A','T','A','C','A','A','C','C'))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(codonsToNucleotides(0:5, format='integer'), c(0,0,0,0,0,1,0,0,2,0,0,3,0,1,0,0,1,1))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { codonsToNucleotides(-1, format='string'); }", "requires codon values to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { codonsToNucleotides(-1, format='char'); }", "requires codon values to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { codonsToNucleotides(-1, format='integer'); }", "requires codon values to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { codonsToNucleotides(64, format='string'); }", "requires codon values to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { codonsToNucleotides(64, format='char'); }", "requires codon values to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { codonsToNucleotides(64, format='integer'); }", "requires codon values to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { codonsToNucleotides(0, format='foo'); }", "requires a format of", __LINE__);
}

void _RunNucleotideMethodTests(void)
{
	// Test that various nucleotide-based APIs behave as they ought to when used in a non-nucleotide model
	SLiMAssertScriptRaise("initialize() { initializeAncestralNucleotides('ACGT'); } ", "only be called in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeHotspotMap(1.0); } ", "only be called in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationTypeNuc(1, 0.5, 'f', 0.0); } ", "only be called in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0, mutationMatrix=mmJukesCantor(1e-7)); } ", "to be NULL in non-nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { sim.chromosome.hotspotEndPositions; }", "only defined in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { sim.chromosome.hotspotEndPositionsM; }", "only defined in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { sim.chromosome.hotspotEndPositionsF; }", "only defined in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { sim.chromosome.hotspotMultipliers; }", "only defined in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { sim.chromosome.hotspotMultipliersM; }", "only defined in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { sim.chromosome.hotspotMultipliersF; }", "only defined in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { sim.chromosome.ancestralNucleotides(); }", "only be called in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { sim.chromosome.setHotspotMap(1.0); }", "only be called in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.genomes[0].nucleotides(); }", "only be called in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { g1.mutationMatrix; }", "only defined in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { g1.setMutationMatrix(mmJukesCantor(1e-7)); }", "only be called in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 early() { mut = sim.mutations[0]; mut.nucleotide; }", "only defined for nucleotide-based mutations", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 early() { mut = sim.mutations[0]; mut.nucleotideValue; }", "only defined for nucleotide-based mutations", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_fixmut_p1 + "30 early() { sub = sim.substitutions[0]; sub.nucleotide; }", "only defined for nucleotide-based mutations", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_fixmut_p1 + "30 early() { sub = sim.substitutions[0]; sub.nucleotideValue; }", "only defined for nucleotide-based mutations", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (sim.nucleotideBased == F) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (m1.nucleotideBased == F) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(m1, 5000, nucleotide='A'); stop(); }", "NULL in non-nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { gen = p1.genomes[0]; mut = gen.addNewMutation(m1, 0.0, 5000, nucleotide='A'); stop(); }", "NULL in non-nucleotide-based models", __LINE__);
	
	// Test that some APIs are correctly disabled in nucleotide-based models
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(nucleotideBased=T); initializeMutationRate(1e-7); } ", "may not be called in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(nucleotideBased=T); initializeMutationTypeNuc(1, 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); }", "non-NULL in nucleotide-based models", __LINE__);
	
	std::string nuc_model_start("initialize() { initializeSLiMOptions(nucleotideBased=T); ");
	std::string nuc_model_init(nuc_model_start + "initializeAncestralNucleotides(randomNucleotides(1e2)); initializeMutationTypeNuc(1, 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0, mmJukesCantor(1e-7)); initializeGenomicElement(g1, 0, 1e2-1); initializeRecombinationRate(1e-8); } ");
	
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { sim.chromosome.mutationEndPositions; }", "not defined in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { sim.chromosome.mutationEndPositionsF; }", "not defined in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { sim.chromosome.mutationEndPositionsM; }", "not defined in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { sim.chromosome.mutationRates; }", "not defined in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { sim.chromosome.mutationRatesF; }", "not defined in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { sim.chromosome.mutationRatesM; }", "not defined in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { sim.chromosome.overallMutationRate; }", "not defined in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { sim.chromosome.overallMutationRateF; }", "not defined in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { sim.chromosome.overallMutationRateM; }", "not defined in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { sim.chromosome.setMutationRate(1e-7); }", "may not be called in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { sim.addSubpop('p1', 10); gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(m1, 50); }", "requires nucleotide to be non-NULL", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { sim.addSubpop('p1', 10); gen = p1.genomes[0]; mut = gen.addNewMutation(m1, 0.0, 50); }", "requires nucleotide to be non-NULL", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { m1.mutationStackGroup = 2; }", "for nucleotide-based mutation types", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { m1.mutationStackPolicy = 'f'; }", "for nucleotide-based mutation types", __LINE__);
	
	// initializeAncestralNucleotides()
	SLiMAssertScriptRaise(nuc_model_start + "initializeAncestralNucleotides(integer(0)); } ", "requires a sequence of length >= 1", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeAncestralNucleotides(-1); } ", "integer nucleotide values", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeAncestralNucleotides(4); } ", "integer nucleotide values", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeAncestralNucleotides('AACAGTACGTTACAGGTACAD'); } ", "could not be opened or does not exist", __LINE__);	// file path!
	SLiMAssertScriptRaise(nuc_model_start + "initializeAncestralNucleotides(c(0,-1,2)); } ", "integer nucleotide value", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeAncestralNucleotides(c(0,4,2)); } ", "integer nucleotide value", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeAncestralNucleotides(c('A','D','T')); } ", "string nucleotide character", __LINE__);
	SLiMAssertScriptStop(nuc_model_start + "if (initializeAncestralNucleotides('A') == 1) stop(); } ", __LINE__);
	SLiMAssertScriptStop(nuc_model_start + "if (initializeAncestralNucleotides(0) == 1) stop(); } ", __LINE__);
	SLiMAssertScriptStop(nuc_model_start + "if (initializeAncestralNucleotides('ACGTACGT') == 8) stop(); } ", __LINE__);
	SLiMAssertScriptStop(nuc_model_start + "if (initializeAncestralNucleotides(c(0,1,2,3,0,1,2,3)) == 8) stop(); } ", __LINE__);
	
	// initializeHotspotMap()
	SLiMAssertScriptRaise(nuc_model_start + "initializeHotspotMap(float(0)); } ", "to be a singleton", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeHotspotMap(1.0, integer(0)); } ", "of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeHotspotMap(float(0), 1e2-1); } ", "of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeHotspotMap(float(0), integer(0)); } ", "of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeHotspotMap(1.0, sex='A'); } ", "requested sex 'A' unsupported", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeHotspotMap(1.0, sex='M'); } ", "supplied in non-sexual simulation", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeSex('A'); initializeHotspotMap(1.0, sex='A'); } ", "requested sex 'A' unsupported", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeSex('A'); initializeHotspotMap(1.0, sex='M'); initializeHotspotMap(1.0, sex='F'); initializeHotspotMap(1.0, sex='M'); } ", "may be called only once", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeHotspotMap(1.0); initializeHotspotMap(1.0); } ", "may be called only once", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeHotspotMap(c(1.0, 1.2)); } ", "multipliers to be a singleton", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeHotspotMap(-0.1); } ", "multipliers to be >= 0", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeHotspotMap(0.1, c(10, 20)); } ", "of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeHotspotMap(c(1.0, 1.2), 10); } ", "of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeHotspotMap(c(1.0, 1.2), c(20, 10)); } ", "in strictly ascending order", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeHotspotMap(c(1.0, -1.2), c(10, 20)); } ", "multipliers to be >= 0", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "initialize() { initializeHotspotMap(c(1.0, 1.2), c(10, 20)); } 1 early() {}", "do not cover the full chromosome", __LINE__, false);
	SLiMAssertScriptStop(nuc_model_start + "initializeHotspotMap(2.0); stop(); } ", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "initialize() { initializeHotspotMap(c(1.0, 1.2), c(10, 1e2-1)); } 1 early() { stop(); } ", __LINE__);
	
	// initializeMutationTypeNuc() (copied from initializeMutationType())
	SLiMAssertScriptStop(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'f', 0.0); stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_start + "initializeMutationTypeNuc(1, 0.5, 'f', 0.0); stop(); }", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc(-1, 0.5, 'f', 0.0); stop(); }", "identifier value is out of range", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('p2', 0.5, 'f', 0.0); stop(); }", "identifier prefix 'm' was expected", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('mm1', 0.5, 'f', 0.0); stop(); }", "must be a simple integer", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'f'); stop(); }", "requires exactly 1 DFE parameter", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'f', 0.0, 0.0); stop(); }", "requires exactly 1 DFE parameter", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'g', 0.0); stop(); }", "requires exactly 2 DFE parameters", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'e', 0.0, 0.0); stop(); }", "requires exactly 1 DFE parameter", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'n', 0.0); stop(); }", "requires exactly 2 DFE parameters", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'p', 0.0); stop(); }", "requires exactly 2 DFE parameters", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'w', 0.0); stop(); }", "requires exactly 2 DFE parameters", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'f', 'foo'); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'g', 'foo', 0.0); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'g', 0.0, 'foo'); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'e', 'foo'); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'n', 'foo', 0.0); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'n', 0.0, 'foo'); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'p', 'foo', 0.0); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'p', 0.0, 'foo'); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'w', 'foo', 0.0); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'w', 0.0, 'foo'); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'f', '1'); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'g', '1', 0.0); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'g', 0.0, '1'); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'e', '1'); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'n', '1', 0.0); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'n', 0.0, '1'); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'p', '1', 0.0); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'p', 0.0, '1'); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'w', '1', 0.0); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'w', 0.0, '1'); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'x', 0.0); stop(); }", "must be 'f', 'g', 'e', 'n', 'w', or 's'", __LINE__);
	SLiMAssertScriptStop(nuc_model_start + "x = initializeMutationTypeNuc('m7', 0.5, 'f', 0.0); if (x == m7) stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_start + "x = initializeMutationTypeNuc(7, 0.5, 'f', 0.0); if (x == m7) stop(); }", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "m7 = 15; initializeMutationTypeNuc(7, 0.5, 'f', 0.0); stop(); }", "already defined", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'f', 0.0); initializeMutationTypeNuc('m1', 0.5, 'f', 0.0); stop(); }", "already defined", __LINE__);
	
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'g', 3.1, 0.0); stop(); }", "must have a shape parameter > 0", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'g', 3.1, -1.0); stop(); }", "must have a shape parameter > 0", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'n', 3.1, -1.0); stop(); }", "must have a standard deviation parameter >= 0", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'p', 3.1, 0.0); stop(); }", "must have a scale parameter > 0", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'p', 3.1, -1.0); stop(); }", "must have a scale parameter > 0", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'w', 0.0, 7.5); stop(); }", "must have a scale parameter > 0", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'w', -1.0, 7.5); stop(); }", "must have a scale parameter > 0", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'w', 3.1, 0.0); stop(); }", "must have a shape parameter > 0", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'w', 3.1, -7.5); stop(); }", "must have a shape parameter > 0", __LINE__);
	
	// initializeGenomicElementType()
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0, mutationMatrix=mmJukesCantor(1e-7)); } ", "requires all mutation types for", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "initialize() { initializeGenomicElementType('g2', m1, 1.0); } ", "non-NULL in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "initialize() { initializeGenomicElementType('g2', m1, 1.0, float(0)); } ", "a 4x4 or 64x4 matrix", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "initialize() { initializeGenomicElementType('g2', m1, 1.0, rep(1.0, 16)); } ", "a 4x4 or 64x4 matrix", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "initialize() { initializeGenomicElementType('g2', m1, 1.0, rep(1.0, 256)); } ", "a 4x4 or 64x4 matrix", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "initialize() { initializeGenomicElementType('g2', m1, 1.0, matrix(rep(1.0, 16), ncol=2)); } ", "a 4x4 or 64x4 matrix", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "initialize() { initializeGenomicElementType('g2', m1, 1.0, matrix(rep(1.0, 256), ncol=2)); } ", "a 4x4 or 64x4 matrix", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "initialize() { initializeGenomicElementType('g2', m1, 1.0, matrix(rep(1.0, 16), ncol=4)); } ", "must contain 0.0 for all", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "initialize() { initializeGenomicElementType('g2', m1, 1.0, matrix(rep(1.0, 256), ncol=4)); } ", "must contain 0.0 for all", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "initialize() { initializeGenomicElementType('g2', m1, 1.0, mmJukesCantor(0.25)*2); } ", "requires the sum of each mutation matrix row", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "initialize() { initializeGenomicElementType('g2', m1, 1.0, mm16To256(mmJukesCantor(0.25))*2); } ", "requires the sum of each mutation matrix row", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "initialize() { mm = mmJukesCantor(0.25); mm[0,1] = -0.1; initializeGenomicElementType('g2', m1, 1.0, mm); } ", "to be finite and >= 0.0", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "initialize() { mm = mm16To256(mmJukesCantor(0.25)); mm[0,1] = -0.1; initializeGenomicElementType('g2', m1, 1.0, mm); } ", "to be finite and >= 0.0", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "initialize() { initializeGenomicElementType('g2', m1, 1.0, mmJukesCantor(0.25)); stop(); } ", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "initialize() { initializeGenomicElementType('g2', m1, 1.0, mm16To256(mmJukesCantor(0.25))); stop(); } ", __LINE__);
	
	// hotspotEndPositions, hotspotEndPositionsM, hotspotEndPositionsF
	SLiMAssertScriptStop(nuc_model_init + "1 early() { if (sim.chromosome.hotspotEndPositions == 1e2-1) stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "initialize() { initializeHotspotMap(2.0); } 1 early() { if (sim.chromosome.hotspotEndPositions == 1e2-1) stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "initialize() { initializeHotspotMap(c(1.0, 1.2), c(10, 1e2-1)); } 1 early() { if (identical(sim.chromosome.hotspotEndPositions, c(10, 1e2-1))) stop(); }", __LINE__);
	
	// hotspotMultipliers, hotspotMultipliersM, hotspotMultipliersF
	SLiMAssertScriptStop(nuc_model_init + "1 early() { if (sim.chromosome.hotspotMultipliers == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "initialize() { initializeHotspotMap(2.0); } 1 early() { if (sim.chromosome.hotspotMultipliers == 2.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "initialize() { initializeHotspotMap(c(1.0, 1.2), c(10, 1e2-1)); } 1 early() { if (identical(sim.chromosome.hotspotMultipliers, c(1.0, 1.2))) stop(); }", __LINE__);
	
	// ancestralNucleotides()
	std::string ances_setup_string = "initialize() { initializeSLiMOptions(nucleotideBased=T); defineConstant('AS', randomNucleotides(1e2, format='string')); initializeAncestralNucleotides(AS); initializeMutationTypeNuc(1, 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0, mmJukesCantor(1e-7)); initializeGenomicElement(g1, 0, 1e2-1); initializeRecombinationRate(1e-8); } ";
	std::string ances_setup_char = "initialize() { initializeSLiMOptions(nucleotideBased=T); defineConstant('AS', randomNucleotides(1e2, format='char')); initializeAncestralNucleotides(AS); initializeMutationTypeNuc(1, 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0, mmJukesCantor(1e-7)); initializeGenomicElement(g1, 0, 1e2-1); initializeRecombinationRate(1e-8); } ";
	std::string ances_setup_integer = "initialize() { initializeSLiMOptions(nucleotideBased=T); defineConstant('AS', randomNucleotides(1e2, format='integer')); initializeAncestralNucleotides(AS); initializeMutationTypeNuc(1, 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0, mmJukesCantor(1e-7)); initializeGenomicElement(g1, 0, 1e2-1); initializeRecombinationRate(1e-8); } ";
	
	SLiMAssertScriptStop(ances_setup_string + "1 early() { if (identical(sim.chromosome.ancestralNucleotides(format='string'), AS)) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_string + "1 early() { if (identical(sim.chromosome.ancestralNucleotides(end=49, format='string'), substr(AS, 0, 49))) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_string + "1 early() { if (identical(sim.chromosome.ancestralNucleotides(start=50, format='string'), substr(AS, 50, 99))) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_string + "1 early() { if (identical(sim.chromosome.ancestralNucleotides(start=25, end=69, format='string'), substr(AS, 25, 69))) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_string + "1 early() { if (identical(sim.chromosome.ancestralNucleotides(start=10, end=39, format='codon'), nucleotidesToCodons(substr(AS, 10, 39)))) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(ances_setup_char + "1 early() { if (identical(sim.chromosome.ancestralNucleotides(format='char'), AS)) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_char + "1 early() { if (identical(sim.chromosome.ancestralNucleotides(end=49, format='char'), AS[0:49])) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_char + "1 early() { if (identical(sim.chromosome.ancestralNucleotides(start=50, format='char'), AS[50:99])) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_char + "1 early() { if (identical(sim.chromosome.ancestralNucleotides(start=25, end=69, format='char'), AS[25:69])) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_char + "1 early() { if (identical(sim.chromosome.ancestralNucleotides(start=10, end=39, format='codon'), nucleotidesToCodons(AS[10:39]))) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(ances_setup_integer + "1 early() { if (identical(sim.chromosome.ancestralNucleotides(format='integer'), AS)) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_integer + "1 early() { if (identical(sim.chromosome.ancestralNucleotides(end=49, format='integer'), AS[0:49])) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_integer + "1 early() { if (identical(sim.chromosome.ancestralNucleotides(start=50, format='integer'), AS[50:99])) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_integer + "1 early() { if (identical(sim.chromosome.ancestralNucleotides(start=25, end=69, format='integer'), AS[25:69])) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_integer + "1 early() { if (identical(sim.chromosome.ancestralNucleotides(start=10, end=39, format='codon'), nucleotidesToCodons(AS[10:39]))) stop(); }", __LINE__);
	
	SLiMAssertScriptRaise(ances_setup_integer + "1 early() { sim.chromosome.ancestralNucleotides(start=-1, end=50, format='integer'); }", "within the chromosome's extent", __LINE__);
	SLiMAssertScriptRaise(ances_setup_integer + "1 early() { sim.chromosome.ancestralNucleotides(start=50, end=100, format='integer'); }", "within the chromosome's extent", __LINE__);
	SLiMAssertScriptRaise(ances_setup_integer + "1 early() { sim.chromosome.ancestralNucleotides(start=75, end=25, format='integer'); }", "start must be <= end", __LINE__);
	SLiMAssertScriptRaise(ances_setup_integer + "1 early() { sim.chromosome.ancestralNucleotides(format='foo'); }", "format must be either", __LINE__);
	
	// setHotspotMap()
	std::string nuc_w_hotspot = nuc_model_init + "initialize() { initializeHotspotMap(c(1.0, 1.2), c(10, 1e2-1)); } ";
	
	SLiMAssertScriptRaise(nuc_w_hotspot + "1 early() { sim.chromosome.setHotspotMap(float(0)); }", "to be a singleton if", __LINE__);
	SLiMAssertScriptRaise(nuc_w_hotspot + "1 early() { sim.chromosome.setHotspotMap(1.0, integer(0)); }", "equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(nuc_w_hotspot + "1 early() { sim.chromosome.setHotspotMap(float(0), 1e2-1); }", "equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(nuc_w_hotspot + "1 early() { sim.chromosome.setHotspotMap(float(0), integer(0)); }", "equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(nuc_w_hotspot + "1 early() { sim.chromosome.setHotspotMap(1.0, sex='A'); }", "sex 'A' unsupported", __LINE__);
	SLiMAssertScriptRaise(nuc_w_hotspot + "1 early() { sim.chromosome.setHotspotMap(1.0, sex='M'); }", "original configuration", __LINE__);
	SLiMAssertScriptRaise(nuc_w_hotspot + "1 early() { sim.chromosome.setHotspotMap(c(1.0, 1.2)); }", "to be a singleton if", __LINE__);
	SLiMAssertScriptRaise(nuc_w_hotspot + "1 early() { sim.chromosome.setHotspotMap(-0.1); }", "multipliers must be >= 0", __LINE__);
	SLiMAssertScriptRaise(nuc_w_hotspot + "1 early() { sim.chromosome.setHotspotMap(0.1, c(10, 20)); }", "equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(nuc_w_hotspot + "1 early() { sim.chromosome.setHotspotMap(c(1.0, 1.2), 10); }", "equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(nuc_w_hotspot + "1 early() { sim.chromosome.setHotspotMap(c(1.0, 1.2), c(20, 10)); }", "strictly ascending order", __LINE__);
	SLiMAssertScriptRaise(nuc_w_hotspot + "1 early() { sim.chromosome.setHotspotMap(c(1.0, -1.2), c(10, 20)); }", "multipliers must be >= 0", __LINE__);
	SLiMAssertScriptRaise(nuc_w_hotspot + "1 early() { sim.chromosome.setHotspotMap(c(1.0, 1.2), c(10, 20)); }", "must end at the last position", __LINE__);
	SLiMAssertScriptStop(nuc_w_hotspot + "1 early() { sim.chromosome.setHotspotMap(1.2); stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_w_hotspot + "1 early() { sim.chromosome.setHotspotMap(c(1.0, 1.2), c(10, 1e2-1)); stop(); }", __LINE__);
	
	// nucleotides()
	SLiMAssertScriptStop(ances_setup_string + "1 early() { sim.addSubpop(1, 10); if (identical(p1.genomes[0].nucleotides(format='string'), AS)) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_string + "1 early() { sim.addSubpop(1, 10); if (identical(p1.genomes[0].nucleotides(end=49, format='string'), substr(AS, 0, 49))) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_string + "1 early() { sim.addSubpop(1, 10); if (identical(p1.genomes[0].nucleotides(start=50, format='string'), substr(AS, 50, 99))) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_string + "1 early() { sim.addSubpop(1, 10); if (identical(p1.genomes[0].nucleotides(start=25, end=69, format='string'), substr(AS, 25, 69))) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_string + "1 early() { sim.addSubpop(1, 10); if (identical(p1.genomes[0].nucleotides(start=10, end=39, format='codon'), nucleotidesToCodons(substr(AS, 10, 39)))) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(ances_setup_char + "1 early() { sim.addSubpop(1, 10); if (identical(p1.genomes[0].nucleotides(format='char'), AS)) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_char + "1 early() { sim.addSubpop(1, 10); if (identical(p1.genomes[0].nucleotides(end=49, format='char'), AS[0:49])) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_char + "1 early() { sim.addSubpop(1, 10); if (identical(p1.genomes[0].nucleotides(start=50, format='char'), AS[50:99])) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_char + "1 early() { sim.addSubpop(1, 10); if (identical(p1.genomes[0].nucleotides(start=25, end=69, format='char'), AS[25:69])) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_char + "1 early() { sim.addSubpop(1, 10); if (identical(p1.genomes[0].nucleotides(start=10, end=39, format='codon'), nucleotidesToCodons(AS[10:39]))) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(ances_setup_integer + "1 early() { sim.addSubpop(1, 10); if (identical(p1.genomes[0].nucleotides(format='integer'), AS)) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_integer + "1 early() { sim.addSubpop(1, 10); if (identical(p1.genomes[0].nucleotides(end=49, format='integer'), AS[0:49])) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_integer + "1 early() { sim.addSubpop(1, 10); if (identical(p1.genomes[0].nucleotides(start=50, format='integer'), AS[50:99])) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_integer + "1 early() { sim.addSubpop(1, 10); if (identical(p1.genomes[0].nucleotides(start=25, end=69, format='integer'), AS[25:69])) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_integer + "1 early() { sim.addSubpop(1, 10); if (identical(p1.genomes[0].nucleotides(start=10, end=39, format='codon'), nucleotidesToCodons(AS[10:39]))) stop(); }", __LINE__);
	
	SLiMAssertScriptRaise(ances_setup_integer + "1 early() { sim.addSubpop(1, 10); p1.genomes[0].nucleotides(start=-1, end=50, format='integer'); }", "within the chromosome's extent", __LINE__);
	SLiMAssertScriptRaise(ances_setup_integer + "1 early() { sim.addSubpop(1, 10); p1.genomes[0].nucleotides(start=50, end=100, format='integer'); }", "within the chromosome's extent", __LINE__);
	SLiMAssertScriptRaise(ances_setup_integer + "1 early() { sim.addSubpop(1, 10); p1.genomes[0].nucleotides(start=75, end=25, format='integer'); }", "start must be <= end", __LINE__);
	SLiMAssertScriptRaise(ances_setup_integer + "1 early() { sim.addSubpop(1, 10); p1.genomes[0].nucleotides(format='foo'); }", "format must be either", __LINE__);
	
	// mutationMatrix()
	SLiMAssertScriptStop(nuc_model_init + "1 early() { if (identical(g1.mutationMatrix, mmJukesCantor(1e-7))) stop(); }", __LINE__);
	
	// setMutationMatrix()
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { g1.setMutationMatrix(NULL); } ", "cannot be type NULL", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { g1.setMutationMatrix(float(0)); } ", "a 4x4 or 64x4 matrix", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { g1.setMutationMatrix(rep(1.0, 16)); } ", "a 4x4 or 64x4 matrix", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { g1.setMutationMatrix(rep(1.0, 256)); } ", "a 4x4 or 64x4 matrix", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { g1.setMutationMatrix(matrix(rep(1.0, 16), ncol=2)); } ", "a 4x4 or 64x4 matrix", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { g1.setMutationMatrix(matrix(rep(1.0, 256), ncol=2)); } ", "a 4x4 or 64x4 matrix", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { g1.setMutationMatrix(matrix(rep(1.0, 16), ncol=4)); } ", "must contain 0.0 for all", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { g1.setMutationMatrix(matrix(rep(1.0, 256), ncol=4)); } ", "must contain 0.0 for all", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { g1.setMutationMatrix(mmJukesCantor(0.25)*2); } ", "requires the sum of each mutation matrix row", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { g1.setMutationMatrix(mm16To256(mmJukesCantor(0.25))*2); } ", "requires the sum of each mutation matrix row", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { mm = mmJukesCantor(0.25); mm[0,1] = -0.1; g1.setMutationMatrix(mm); } ", "to be finite and >= 0.0", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { mm = mm16To256(mmJukesCantor(0.25)); mm[0,1] = -0.1; g1.setMutationMatrix(mm); } ", "to be finite and >= 0.0", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "1 early() { g1.setMutationMatrix(mmJukesCantor(0.25)); stop(); } ", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "1 early() { g1.setMutationMatrix(mm16To256(mmJukesCantor(0.25))); stop(); } ", __LINE__);
	
	// nucleotide & nucleotideValue
	std::string nuc_highmut("initialize() { initializeSLiMOptions(nucleotideBased=T); initializeAncestralNucleotides(randomNucleotides(1e2)); initializeMutationTypeNuc('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0, mmJukesCantor(1e-2)); initializeGenomicElement(g1, 0, 1e2-1); initializeRecombinationRate(1e-8); } 1 early() { sim.addSubpop('p1', 10); } ");
	std::string nuc_fixmut("initialize() { initializeSLiMOptions(nucleotideBased=T); initializeAncestralNucleotides(randomNucleotides(1e2)); initializeMutationTypeNuc('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0, mmJukesCantor(1e-2)); initializeGenomicElement(g1, 0, 1e2-1); initializeRecombinationRate(1e-8); } 1 early() { sim.addSubpop('p1', 10); } 10 early() { sim.mutations[0].setSelectionCoeff(500.0); sim.recalculateFitness(); } ");
	
	SLiMAssertScriptStop(nuc_highmut + "10 early() { mut = sim.mutations[0]; mut.nucleotide; stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_highmut + "10 early() { mut = sim.mutations[0]; mut.nucleotideValue; stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_fixmut + "30 early() { sub = sim.substitutions[0]; sub.nucleotide; stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_fixmut + "30 early() { sub = sim.substitutions[0]; sub.nucleotideValue; stop(); }", __LINE__);
	
	// addNewDrawnMutation()
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { sim.addSubpop(1, 10); p1.genomes[0].addNewDrawnMutation(m1, 10); }", "nucleotide to be non-NULL", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { sim.addSubpop(1, 10); p1.genomes[0].addNewDrawnMutation(m1, 10, nucleotide=NULL); }", "nucleotide to be non-NULL", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { sim.addSubpop(1, 10); p1.genomes[0].addNewDrawnMutation(m1, 10, nucleotide='D'); }", "string nucleotide values", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { sim.addSubpop(1, 10); p1.genomes[0].addNewDrawnMutation(m1, 10, nucleotide=-1); }", "integer nucleotide values", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { sim.addSubpop(1, 10); p1.genomes[0].addNewDrawnMutation(m1, 10, nucleotide=4); }", "integer nucleotide values", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "1 early() { sim.addSubpop(1, 10); p1.genomes[0].addNewDrawnMutation(m1, 10, nucleotide='A'); stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "1 early() { sim.addSubpop(1, 10); p1.genomes[0].addNewDrawnMutation(m1, 10, nucleotide=0); stop(); }", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { sim.addSubpop(1, 10); p1.genomes[0:3].addNewDrawnMutation(m1, 10); }", "nucleotide to be non-NULL", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { sim.addSubpop(1, 10); p1.genomes[0:3].addNewDrawnMutation(m1, 10, nucleotide=NULL); }", "nucleotide to be non-NULL", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { sim.addSubpop(1, 10); p1.genomes[0:3].addNewDrawnMutation(m1, 10, nucleotide=c('A','D','G','C')); }", "string nucleotide values", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { sim.addSubpop(1, 10); p1.genomes[0:3].addNewDrawnMutation(m1, 10, nucleotide=c(0,-1,2,3)); }", "integer nucleotide values", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { sim.addSubpop(1, 10); p1.genomes[0:3].addNewDrawnMutation(m1, 10, nucleotide=c(0,4,2,3)); }", "integer nucleotide values", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "1 early() { sim.addSubpop(1, 10); p1.genomes[0:3].addNewDrawnMutation(m1, 10, nucleotide=c('A','C','G','T')); stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "1 early() { sim.addSubpop(1, 10); p1.genomes[0:3].addNewDrawnMutation(m1, 10, nucleotide=0:3); stop(); }", __LINE__);
	
	// addNewMutation()
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { sim.addSubpop(1, 10); p1.genomes[0].addNewMutation(m1, 0.5, 10); }", "nucleotide to be non-NULL", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { sim.addSubpop(1, 10); p1.genomes[0].addNewMutation(m1, 0.5, 10, nucleotide=NULL); }", "nucleotide to be non-NULL", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { sim.addSubpop(1, 10); p1.genomes[0].addNewMutation(m1, 0.5, 10, nucleotide='D'); }", "string nucleotide values", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { sim.addSubpop(1, 10); p1.genomes[0].addNewMutation(m1, 0.5, 10, nucleotide=-1); }", "integer nucleotide values", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { sim.addSubpop(1, 10); p1.genomes[0].addNewMutation(m1, 0.5, 10, nucleotide=4); }", "integer nucleotide values", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "1 early() { sim.addSubpop(1, 10); p1.genomes[0].addNewMutation(m1, 0.5, 10, nucleotide='A'); stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "1 early() { sim.addSubpop(1, 10); p1.genomes[0].addNewMutation(m1, 0.5, 10, nucleotide=0); stop(); }", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { sim.addSubpop(1, 10); p1.genomes[0:3].addNewMutation(m1, 0.5, 10); }", "nucleotide to be non-NULL", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { sim.addSubpop(1, 10); p1.genomes[0:3].addNewMutation(m1, 0.5, 10, nucleotide=NULL); }", "nucleotide to be non-NULL", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { sim.addSubpop(1, 10); p1.genomes[0:3].addNewMutation(m1, 0.5, 10, nucleotide=c('A','D','G','C')); }", "string nucleotide values", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { sim.addSubpop(1, 10); p1.genomes[0:3].addNewMutation(m1, 0.5, 10, nucleotide=c(0,-1,2,3)); }", "integer nucleotide values", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { sim.addSubpop(1, 10); p1.genomes[0:3].addNewMutation(m1, 0.5, 10, nucleotide=c(0,4,2,3)); }", "integer nucleotide values", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "1 early() { sim.addSubpop(1, 10); p1.genomes[0:3].addNewMutation(m1, 0.5, 10, nucleotide=c('A','C','G','T')); stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "1 early() { sim.addSubpop(1, 10); p1.genomes[0:3].addNewMutation(m1, 0.5, 10, nucleotide=0:3); stop(); }", __LINE__);
	
	// Species.nucleotideBased
	SLiMAssertScriptStop(nuc_model_init + "1 early() { if (sim.nucleotideBased == T) stop(); }", __LINE__);
	
	// MutationType.nucleotideBased
	SLiMAssertScriptStop(nuc_model_init + "1 early() { if (m1.nucleotideBased == T) stop(); }", __LINE__);
	
	// initializeGeneConversion() tests using GC bias != 0
	SLiMAssertScriptStop(nuc_model_init + "initialize() { initializeGeneConversion(0.2, 1234.5, 0.75, -0.01); } 1 early() { if (sim.chromosome.geneConversionEnabled == T) stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "initialize() { initializeGeneConversion(0.2, 1234.5, 0.75, -0.01); } 1 early() { if (sim.chromosome.geneConversionNonCrossoverFraction == 0.2) stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "initialize() { initializeGeneConversion(0.2, 1234.5, 0.75, -0.01); } 1 early() { if (sim.chromosome.geneConversionMeanLength == 1234.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "initialize() { initializeGeneConversion(0.2, 1234.5, 0.75, -0.01); } 1 early() { if (sim.chromosome.geneConversionSimpleConversionFraction == 0.75) stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "initialize() { initializeGeneConversion(0.2, 1234.5, 0.75, -0.01); } 1 early() { if (sim.chromosome.geneConversionGCBias == -0.01) stop(); }", __LINE__);
	
	// Chromosome.setGeneConversion() tests using GC bias != 0
	SLiMAssertScriptStop(nuc_model_init + "1 early() { sim.chromosome.setGeneConversion(0.2, 1234.5, 0.75, -0.01); if (sim.chromosome.geneConversionEnabled == T) stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "1 early() { sim.chromosome.setGeneConversion(0.2, 1234.5, 0.75, -0.01); if (sim.chromosome.geneConversionNonCrossoverFraction == 0.2) stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "1 early() { sim.chromosome.setGeneConversion(0.2, 1234.5, 0.75, -0.01); if (sim.chromosome.geneConversionMeanLength == 1234.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "1 early() { sim.chromosome.setGeneConversion(0.2, 1234.5, 0.75, -0.01); if (sim.chromosome.geneConversionSimpleConversionFraction == 0.75) stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "1 early() { sim.chromosome.setGeneConversion(0.2, 1234.5, 0.75, -0.01); if (sim.chromosome.geneConversionGCBias == -0.01) stop(); }", __LINE__);
	
	// Chromosome.setGeneConversion() bounds tests
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { sim.chromosome.setGeneConversion(-0.001, 10000000000000, 0.0); stop(); }", "nonCrossoverFraction must be between 0.0 and 1.0", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { sim.chromosome.setGeneConversion(1.001, 10000000000000, 0.0); stop(); }", "nonCrossoverFraction must be between 0.0 and 1.0", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { sim.chromosome.setGeneConversion(0.5, -0.01, 0.0); stop(); }", "meanLength must be >= 0.0", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { sim.chromosome.setGeneConversion(0.5, 1000, -0.001); stop(); }", "simpleConversionFraction must be between 0.0 and 1.0", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { sim.chromosome.setGeneConversion(0.5, 1000, 1.001); stop(); }", "simpleConversionFraction must be between 0.0 and 1.0", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { sim.chromosome.setGeneConversion(0.5, 1000, 0.0, -1.001); stop(); }", "bias must be between -1.0 and 1.0", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 early() { sim.chromosome.setGeneConversion(0.5, 1000, 0.0, 1.001); stop(); }", "bias must be between -1.0 and 1.0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { sim.chromosome.setGeneConversion(0.5, 1000, 0.0, 0.1); stop(); }", "must be 0.0 in non-nucleotide-based models", __LINE__);
}

































