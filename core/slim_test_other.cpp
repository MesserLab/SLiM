//
//  slim_test_other.cpp
//  SLiM
//
//  Created by Ben Haller on 7/11/20.
//  Copyright (c) 2020 Philipp Messer.  All rights reserved.
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
static void _RunInteractionTypeTests_Nonspatial(bool p_reciprocal, bool p_immediate, bool p_sex_enabled, std::string p_sex_segregation);
static void _RunInteractionTypeTests_Spatial(std::string p_max_distance, bool p_reciprocal, bool p_immediate, bool p_sex_enabled, std::string p_sex_segregation);

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
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { if (i1.id == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { if (isInfinite(i1.maxDistance)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { if (i1.reciprocal == F) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { if (i1.sexSegregation == '**') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { if (i1.spatiality == 'x') stop(); }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 { i1.id = 2; }", 1, 427, "read-only property", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { i1.maxDistance = 0.5; if (i1.maxDistance == 0.5) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 { i1.reciprocal = F; }", 1, 435, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 { i1.sexSegregation = '**'; }", 1, 439, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 { i1.spatiality = 'x'; }", 1, 435, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 { i1.tag; }", 1, 424, "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 { c(i1,i1).tag; }", 1, 430, "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { i1.tag = 17; } 2 { if (i1.tag == 17) stop(); }", __LINE__);
	
	// Run tests in a variety of combinations
	_RunInteractionTypeTests_Nonspatial(false, false, false, "**");
	_RunInteractionTypeTests_Nonspatial(true, false, false, "**");
	_RunInteractionTypeTests_Nonspatial(false, true, false, "**");
	_RunInteractionTypeTests_Nonspatial(true, true, false, "**");
	
	_RunInteractionTypeTests_Spatial(" INF ", false, false, false, "**");
	_RunInteractionTypeTests_Spatial("999.0", false, false, false, "**");
	_RunInteractionTypeTests_Spatial(" INF ", true, false, false, "**");
	_RunInteractionTypeTests_Spatial("999.0", true, false, false, "**");
	_RunInteractionTypeTests_Spatial(" INF ", false, true, false, "**");
	_RunInteractionTypeTests_Spatial("999.0", false, true, false, "**");
	_RunInteractionTypeTests_Spatial(" INF ", true, true, false, "**");
	_RunInteractionTypeTests_Spatial("999.0", true, true, false, "**");
	
	for (int sex_seg_index = 0; sex_seg_index <= 8; ++sex_seg_index)
	{
		// For a full test, change the condition to <= 8; that makes for a long test runtime, but it works.
		// Note that the tests are throttled down when sexSegregation != "**" anyway, because the results
		// will vary, and it's too much work to figure out the right answer for every test in every
		// combination; we just test for a crash or error.
		std::string seg_str;
		
		switch (sex_seg_index)
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
		}
		
		_RunInteractionTypeTests_Nonspatial(false, false, true, seg_str);
		_RunInteractionTypeTests_Nonspatial(true, false, true, seg_str);
		_RunInteractionTypeTests_Nonspatial(false, true, true, seg_str);
		_RunInteractionTypeTests_Nonspatial(true, true, true, seg_str);
		
		_RunInteractionTypeTests_Spatial(" INF ", false, false, true, seg_str);
		_RunInteractionTypeTests_Spatial("999.0", false, false, true, seg_str);
		_RunInteractionTypeTests_Spatial(" INF ", true, false, true, seg_str);
		_RunInteractionTypeTests_Spatial("999.0", true, false, true, seg_str);
		_RunInteractionTypeTests_Spatial(" INF ", false, true, true, seg_str);
		_RunInteractionTypeTests_Spatial("999.0", false, true, true, seg_str);
		_RunInteractionTypeTests_Spatial(" INF ", true, true, true, seg_str);
		_RunInteractionTypeTests_Spatial("999.0", true, true, true, seg_str);
	}
}

void _RunInteractionTypeTests_Nonspatial(bool p_reciprocal, bool p_immediate, bool p_sex_enabled, std::string p_sex_segregation)
{
	std::string reciprocal_string = p_reciprocal ? "reciprocal=T" : "reciprocal=F";
	std::string immediate_string = p_immediate ? "immediate=T" : "immediate=F";
	std::string sex_string = p_sex_enabled ? "initializeSex('A'); " : "                    ";
	bool sex_seg_on = (p_sex_segregation != "**");
	
	std::string gen1_setup_i1_pop("initialize() { initializeMutationRate(1e-5); " + sex_string + "initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', '', " + reciprocal_string + ", sexSegregation='" + p_sex_segregation + "'); } 1 { sim.addSubpop('p1', 10); } 1:10 late() { i1.evaluate(" + immediate_string + "); i1.strength(p1.individuals[0]); } 1 late() { ind = p1.individuals; ");
	
	SLiMAssertScriptStop(gen1_setup_i1_pop + "i1.unevaluate(); i1.evaluate(); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1_pop + "i1.unevaluate(); i1.unevaluate(); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1_pop + "i1.distance(ind[0], ind[2]); stop(); }", 1, 445, "interaction be spatial", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1_pop + "i1.interactionDistance(ind[0], ind[2]); stop(); }", 1, 445, "interaction be spatial", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1_pop + "i1.distanceToPoint(ind[0], 1.0); stop(); }", 1, 445, "interaction be spatial", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1_pop + "i1.drawByStrength(ind[0]); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1_pop + "i1.drawByStrength(ind[0]); stop(); } interaction(i1) { return 2.0; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1_pop + "i1.drawByStrength(ind[0]); stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1_pop + "i1.nearestNeighbors(ind[8], 1); stop(); }", 1, 445, "interaction be spatial", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1_pop + "i1.nearestInteractingNeighbors(ind[8], 1); stop(); }", 1, 445, "interaction be spatial", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1_pop + "i1.interactingNeighborCount(ind[8]); stop(); }", 1, 445, "interaction be spatial", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1_pop + "i1.nearestNeighborsOfPoint(p1, 19.0, 1); stop(); }", 1, 445, "interaction be spatial", __LINE__);
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
	}
	SLiMAssertScriptRaise(gen1_setup_i1_pop + "i1.totalOfNeighborStrengths(ind[0]); stop(); }", 1, 445, "interaction be spatial", __LINE__);
}

void _RunInteractionTypeTests_Spatial(std::string p_max_distance, bool p_reciprocal, bool p_immediate, bool p_sex_enabled, std::string p_sex_segregation)
{
	std::string reciprocal_string = p_reciprocal ? "reciprocal=T" : "reciprocal=F";
	std::string immediate_string = p_immediate ? "immediate=T" : "immediate=F";
	std::string sex_string = p_sex_enabled ? "initializeSex('A'); " : "                    ";
	bool sex_seg_on = (p_sex_segregation != "**");
	bool max_dist_on = (p_max_distance != "INF");
	
	// *** 1D
	for (int i = 0; i < 3; ++i)
	{
		std::string gen1_setup_i1x_pop;
		
		if (i == 0)
			gen1_setup_i1x_pop = "initialize() { initializeSLiMOptions(dimensionality='xyz'); " + sex_string + "initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'x', " + reciprocal_string + ", maxDistance=" + p_max_distance + ", sexSegregation='" + p_sex_segregation + "'); } 1 { sim.addSubpop('p1', 10); p1.individuals.x = c(-10.0, 0, 1, 2, 3, 5, 7, 8, 20, 25); p1.individuals.y = runif(10); p1.individuals.z = runif(10); i1.evaluate(" + immediate_string + "); ind = p1.individuals; ";
		else if (i == 1)
			gen1_setup_i1x_pop = "initialize() { initializeSLiMOptions(dimensionality='xyz'); " + sex_string + "initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'y', " + reciprocal_string + ", maxDistance=" + p_max_distance + ", sexSegregation='" + p_sex_segregation + "'); } 1 { sim.addSubpop('p1', 10); p1.individuals.y = c(-10.0, 0, 1, 2, 3, 5, 7, 8, 20, 25); p1.individuals.x = runif(10); p1.individuals.z = runif(10); i1.evaluate(" + immediate_string + "); ind = p1.individuals; ";
		else // if (i == 2)
			gen1_setup_i1x_pop = "initialize() { initializeSLiMOptions(dimensionality='xyz'); " + sex_string + "initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'z', " + reciprocal_string + ", maxDistance=" + p_max_distance + ", sexSegregation='" + p_sex_segregation + "'); } 1 { sim.addSubpop('p1', 10); p1.individuals.z = c(-10.0, 0, 1, 2, 3, 5, 7, 8, 20, 25); p1.individuals.x = runif(10); p1.individuals.y = runif(10); i1.evaluate(" + immediate_string + "); ind = p1.individuals; ";
		
		// Test InteractionType – (float)distance(object<Individual> individuals1, [No<Individual> individuals2 = NULL])
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (i1.distance(ind[0], ind[2]) == 11.0) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.distance(ind[0:1], ind[2]), c(11.0, 1.0))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.distance(ind[0], ind[2:3]), c(11.0, 12.0))) stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "if (i1.distance(ind[0:1], ind[2:3]) == 11.0) stop(); }", 1, 571, "either individuals1 or individuals2 be", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.distance(ind[5], ind[c(0, 5, 9, 8, 1)]), c(15.0, 0, 20, 15, 5))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.distance(ind[integer(0)], ind[8]), float(0))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.distance(ind[1], ind[integer(0)]), float(0))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.distance(ind[5]), c(15.0, 5, 4, 3, 2, 0, 2, 3, 15, 20))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.distance(ind[5], NULL), c(15.0, 5, 4, 3, 2, 0, 2, 3, 15, 20))) stop(); }", __LINE__);
		
		// Test InteractionType – (float)interactionDistance(object<Individual>$ receiver, [No<Individual> exerters = NULL])
		if (!sex_seg_on)
		{
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (i1.interactionDistance(ind[0], ind[2]) == 11.0) stop(); }", __LINE__);
			SLiMAssertScriptRaise(gen1_setup_i1x_pop + "if (identical(i1.interactionDistance(ind[0:1], ind[2]), c(11.0, 1.0))) stop(); }", 1, 581, "must be a singleton", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.interactionDistance(ind[0], ind[2:3]), c(11.0, 12.0))) stop(); }", __LINE__);
			SLiMAssertScriptRaise(gen1_setup_i1x_pop + "if (i1.interactionDistance(ind[0:1], ind[2:3]) == 11.0) stop(); }", 1, 571, "must be a singleton", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.interactionDistance(ind[5], ind[c(0, 5, 9, 8, 1)]), c(15.0, INF, 20, 15, 5))) stop(); }", __LINE__);
			SLiMAssertScriptRaise(gen1_setup_i1x_pop + "if (identical(i1.interactionDistance(ind[integer(0)], ind[8]), float(0))) stop(); }", 1, 581, "must be a singleton", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.interactionDistance(ind[1], ind[integer(0)]), float(0))) stop(); }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.interactionDistance(ind[5]), c(15.0, 5, 4, 3, 2, INF, 2, 3, 15, 20))) stop(); }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.interactionDistance(ind[5], NULL), c(15.0, 5, 4, 3, 2, INF, 2, 3, 15, 20))) stop(); }", __LINE__);
		}
		else
		{
			// comprehensively testing all the different sex-seg cases is complicated, but we can at least test the two branches of the code against each other
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.interactionDistance(ind[5]), i1.interactionDistance(ind[5], NULL))) stop(); }", __LINE__);
		}
		
		// Test InteractionType – (float)distanceToPoint(object<Individual> individuals1, float point)
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (i1.distanceToPoint(ind[0], 1.0) == 11.0) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.distanceToPoint(ind[0:1], 1.0), c(11.0, 1.0))) stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "if (i1.distanceToPoint(ind[0:1], 1.0:2.0) == 11.0) stop(); }", 1, 571, "point is of length equal to", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.distanceToPoint(ind[c(0, 5, 9, 8, 1)], 5.0), c(15.0, 0, 20, 15, 5))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.distanceToPoint(ind[integer(0)], 8.0), float(0))) stop(); }", __LINE__);
		
		// Test InteractionType – (object<Individual>)drawByStrength(object<Individual>$ individual, [integer$ count = 1])
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.drawByStrength(ind[0]); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.drawByStrength(ind[0], 1); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.drawByStrength(ind[0], 50); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.drawByStrength(ind[0], 0), ind[integer(0)])) stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.drawByStrength(ind[0], -1); stop(); }", 1, 567, "requires count >= 0", __LINE__);
		
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.drawByStrength(ind[0]); stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.drawByStrength(ind[0], 1); stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.drawByStrength(ind[0], 50); stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.drawByStrength(ind[0], 0), ind[integer(0)])) stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.drawByStrength(ind[0], -1); stop(); } interaction(i1) { return 2.0; }", 1, 567, "requires count >= 0", __LINE__);
		
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.drawByStrength(ind[0]); stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.drawByStrength(ind[0], 1); stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.drawByStrength(ind[0], 50); stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.drawByStrength(ind[0], 0), ind[integer(0)])) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.drawByStrength(ind[0], -1); stop(); } interaction(i1) { return strength * 2.0; }", 1, 567, "requires count >= 0", __LINE__);
		
		// Test InteractionType – (void)evaluate([No<Subpopulation> subpops = NULL], [logical$ immediate = F])
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.evaluate(); i1.evaluate(); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.evaluate(p1); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.evaluate(NULL); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.evaluate(immediate=T); i1.evaluate(); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.evaluate(p1, immediate=T); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.evaluate(NULL, immediate=T); stop(); }", __LINE__);
		
		// Test InteractionType – (object<Individual>)nearestNeighbors(object<Individual>$ individual, [integer$ count = 1])
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "if (identical(i1.nearestNeighbors(ind[8], -1), ind[integer(0)])) stop(); }", 1, 581, "requires count >= 0", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.nearestNeighbors(ind[8], 0), ind[integer(0)])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.nearestNeighbors(ind[8], 1), ind[9])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(sortBy(i1.nearestNeighbors(ind[8], 3), 'index'), ind[c(6,7,9)])) stop(); }", __LINE__);
		
		// Test InteractionType – (object<Individual>)nearestInteractingNeighbors(object<Individual>$ individual, [integer$ count = 1])
		// Test InteractionType – (object<Individual>)interactingNeighborCount(object<Individual>$ individual, [integer$ count = 1])
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "if (identical(i1.nearestInteractingNeighbors(ind[8], -1), ind[integer(0)])) stop(); }", 1, 581, "requires count >= 0", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.nearestInteractingNeighbors(ind[8], 0), ind[integer(0)])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (size(i1.nearestInteractingNeighbors(ind[0], 100)) == i1.interactingNeighborCount(ind[0])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (size(i1.nearestInteractingNeighbors(ind[1], 100)) == i1.interactingNeighborCount(ind[1])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (size(i1.nearestInteractingNeighbors(ind[2], 100)) == i1.interactingNeighborCount(ind[2])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (size(i1.nearestInteractingNeighbors(ind[3], 100)) == i1.interactingNeighborCount(ind[3])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (size(i1.nearestInteractingNeighbors(ind[4], 100)) == i1.interactingNeighborCount(ind[4])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (size(i1.nearestInteractingNeighbors(ind[5], 100)) == i1.interactingNeighborCount(ind[5])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (size(i1.nearestInteractingNeighbors(ind[6], 100)) == i1.interactingNeighborCount(ind[6])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (size(i1.nearestInteractingNeighbors(ind[7], 100)) == i1.interactingNeighborCount(ind[7])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (size(i1.nearestInteractingNeighbors(ind[8], 100)) == i1.interactingNeighborCount(ind[8])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (size(i1.nearestInteractingNeighbors(ind[9], 100)) == i1.interactingNeighborCount(ind[9])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (size(i1.nearestInteractingNeighbors(ind[0], 100)) == sum(isFinite(i1.interactionDistance(ind[0])))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (size(i1.nearestInteractingNeighbors(ind[1], 100)) == sum(isFinite(i1.interactionDistance(ind[1])))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (size(i1.nearestInteractingNeighbors(ind[2], 100)) == sum(isFinite(i1.interactionDistance(ind[2])))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (size(i1.nearestInteractingNeighbors(ind[3], 100)) == sum(isFinite(i1.interactionDistance(ind[3])))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (size(i1.nearestInteractingNeighbors(ind[4], 100)) == sum(isFinite(i1.interactionDistance(ind[4])))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (size(i1.nearestInteractingNeighbors(ind[5], 100)) == sum(isFinite(i1.interactionDistance(ind[5])))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (size(i1.nearestInteractingNeighbors(ind[6], 100)) == sum(isFinite(i1.interactionDistance(ind[6])))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (size(i1.nearestInteractingNeighbors(ind[7], 100)) == sum(isFinite(i1.interactionDistance(ind[7])))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (size(i1.nearestInteractingNeighbors(ind[8], 100)) == sum(isFinite(i1.interactionDistance(ind[8])))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (size(i1.nearestInteractingNeighbors(ind[9], 100)) == sum(isFinite(i1.interactionDistance(ind[9])))) stop(); }", __LINE__);
		
		// Test InteractionType – (object<Individual>)nearestNeighborsOfPoint(object<Subpopulation>$ subpop, float point, [integer$ count = 1])
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "if (identical(i1.nearestNeighborsOfPoint(p1, 5.0, -1), ind[integer(0)])) stop(); }", 1, 581, "requires count >= 0", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.nearestNeighborsOfPoint(p1, 5.0, 0), ind[integer(0)])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.nearestNeighborsOfPoint(p1, 19.0, 1), ind[8])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(sortBy(i1.nearestNeighborsOfPoint(p1, 19.0, 3), 'index'), ind[c(7,8,9)])) stop(); }", __LINE__);
		
		// Test InteractionType – (void)setInteractionFunction(string$ functionType, ...)
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.setInteractionFunction('q', 10.0); i1.evaluate(immediate=T); stop(); }", 1, 567, "while the interaction is being evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.setInteractionFunction('q', 10.0); i1.evaluate(immediate=T); stop(); }", 1, 584, "functionType \"q\" must be", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.unevaluate(); i1.setInteractionFunction('f', 5.0); i1.evaluate(immediate=T); stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.setInteractionFunction('f'); i1.evaluate(immediate=T); stop(); }", 1, 584, "requires exactly", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.setInteractionFunction('f', 5.0, 2.0); i1.evaluate(immediate=T); stop(); }", 1, 584, "requires exactly", __LINE__);
		
		if (!max_dist_on)
			SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.setInteractionFunction('l', 5.0); i1.evaluate(immediate=T); stop(); }", 1, 584, "finite maximum interaction distance", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('l', 5.0); i1.evaluate(immediate=T); stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('l'); i1.evaluate(immediate=T); stop(); }", 1, 604, "requires exactly", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('l', 5.0, 2.0); i1.evaluate(immediate=T); stop(); }", 1, 604, "requires exactly", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('e', 5.0, 1.0); i1.evaluate(immediate=T); stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('e', 5.0); i1.evaluate(immediate=T); stop(); }", 1, 604, "requires exactly", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('e', 5.0, 2.0, 1.0); i1.evaluate(immediate=T); stop(); }", 1, 604, "requires exactly", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('n', 5.0, 1.0); i1.evaluate(immediate=T); stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('n', 5.0); i1.evaluate(immediate=T); stop(); }", 1, 604, "requires exactly", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('n', 5.0, 2.0, 1.0); i1.evaluate(immediate=T); stop(); }", 1, 604, "requires exactly", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('c', 5.0, 1.0); i1.evaluate(immediate=T); stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('c', 5.0); i1.evaluate(immediate=T); stop(); }", 1, 604, "requires exactly", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('c', 5.0, 2.0, 1.0); i1.evaluate(immediate=T); stop(); }", 1, 604, "requires exactly", __LINE__);
		
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('n', 5.0, -1.0); stop(); }", 1, 604, "must have a standard deviation parameter >= 0", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('c', 5.0, 0.0); stop(); }", 1, 604, "must have a scale parameter > 0", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('c', 5.0, -1.0); stop(); }", 1, 604, "must have a scale parameter > 0", __LINE__);
		
		// Test InteractionType – (float)strength(object<Individual> individuals1, [No<Individual> individuals2 = NULL])
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
		}
		
		// Test InteractionType – (float)totalOfNeighborStrengths(object<Individual> individuals)
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
		}
		
		// Test InteractionType – (void)unevaluate(void)
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.unevaluate(); i1.evaluate(); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.unevaluate(); i1.unevaluate(); stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.distance(ind[0], ind[2]); stop(); }", 1, 584, "has been evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.interactionDistance(ind[0], ind[2]); stop(); }", 1, 584, "has been evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.distanceToPoint(ind[0], 1.0); stop(); }", 1, 584, "has been evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.drawByStrength(ind[0]); stop(); }", 1, 584, "has been evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.nearestNeighbors(ind[8], 1); stop(); }", 1, 584, "has been evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.nearestInteractingNeighbors(ind[8], 1); stop(); }", 1, 584, "has been evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.interactingNeighborCount(ind[8]); stop(); }", 1, 584, "has been evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.nearestNeighborsOfPoint(p1, 19.0, 1); stop(); }", 1, 584, "has been evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.strength(ind[0], ind[2]); stop(); }", 1, 584, "has been evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.totalOfNeighborStrengths(ind[0]); stop(); }", 1, 584, "has been evaluated", __LINE__);
	}
	
	// *** 2D
	for (int i = 0; i < 6; ++i)
	{
		std::string gen1_setup_i1xy_pop;
		bool use_first_coordinate = (i < 3);
		
		if (i == 0)
			gen1_setup_i1xy_pop = "initialize() { initializeSLiMOptions(dimensionality='xyz'); " + sex_string + "initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'xy', " + reciprocal_string + ", maxDistance=" + p_max_distance + ", sexSegregation='" + p_sex_segregation + "'); } 1 { sim.addSubpop('p1', 10); p1.individuals.x = c(-10.0, 0, 1, 2, 3, 5, 7, 8, 20, 25); p1.individuals.y = 0; p1.individuals.z = runif(10); i1.evaluate(" + immediate_string + "); ind = p1.individuals; ";
		else if (i == 1)
			gen1_setup_i1xy_pop = "initialize() { initializeSLiMOptions(dimensionality='xyz'); " + sex_string + "initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'xz', " + reciprocal_string + ", maxDistance=" + p_max_distance + ", sexSegregation='" + p_sex_segregation + "'); } 1 { sim.addSubpop('p1', 10); p1.individuals.x = c(-10.0, 0, 1, 2, 3, 5, 7, 8, 20, 25); p1.individuals.z = 0; p1.individuals.y = runif(10); i1.evaluate(" + immediate_string + "); ind = p1.individuals; ";
		else if (i == 2)
			gen1_setup_i1xy_pop = "initialize() { initializeSLiMOptions(dimensionality='xyz'); " + sex_string + "initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'yz', " + reciprocal_string + ", maxDistance=" + p_max_distance + ", sexSegregation='" + p_sex_segregation + "'); } 1 { sim.addSubpop('p1', 10); p1.individuals.y = c(-10.0, 0, 1, 2, 3, 5, 7, 8, 20, 25); p1.individuals.z = 0; p1.individuals.x = runif(10); i1.evaluate(" + immediate_string + "); ind = p1.individuals; ";
		else if (i == 3)
			gen1_setup_i1xy_pop = "initialize() { initializeSLiMOptions(dimensionality='xyz'); " + sex_string + "initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'xy', " + reciprocal_string + ", maxDistance=" + p_max_distance + ", sexSegregation='" + p_sex_segregation + "'); } 1 { sim.addSubpop('p1', 10); p1.individuals.y = c(-10.0, 0, 1, 2, 3, 5, 7, 8, 20, 25); p1.individuals.x = 0; p1.individuals.z = runif(10); i1.evaluate(" + immediate_string + "); ind = p1.individuals; ";
		else if (i == 4)
			gen1_setup_i1xy_pop = "initialize() { initializeSLiMOptions(dimensionality='xyz'); " + sex_string + "initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'xz', " + reciprocal_string + ", maxDistance=" + p_max_distance + ", sexSegregation='" + p_sex_segregation + "'); } 1 { sim.addSubpop('p1', 10); p1.individuals.z = c(-10.0, 0, 1, 2, 3, 5, 7, 8, 20, 25); p1.individuals.x = 0; p1.individuals.y = runif(10); i1.evaluate(" + immediate_string + "); ind = p1.individuals; ";
		else // if (i == 5)
			gen1_setup_i1xy_pop = "initialize() { initializeSLiMOptions(dimensionality='xyz'); " + sex_string + "initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'yz', " + reciprocal_string + ", maxDistance=" + p_max_distance + ", sexSegregation='" + p_sex_segregation + "'); } 1 { sim.addSubpop('p1', 10); p1.individuals.z = c(-10.0, 0, 1, 2, 3, 5, 7, 8, 20, 25); p1.individuals.y = 0; p1.individuals.x = runif(10); i1.evaluate(" + immediate_string + "); ind = p1.individuals; ";
		
		// Test InteractionType – (float)distance(object<Individual> individuals1, [No<Individual> individuals2 = NULL])
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (i1.distance(ind[0], ind[2]) == 11.0) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.distance(ind[0:1], ind[2]), c(11.0, 1.0))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.distance(ind[0], ind[2:3]), c(11.0, 12.0))) stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "if (i1.distance(ind[0:1], ind[2:3]) == 11.0) stop(); }", 1, 564, "either individuals1 or individuals2 be", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.distance(ind[5], ind[c(0, 5, 9, 8, 1)]), c(15.0, 0, 20, 15, 5))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.distance(ind[integer(0)], ind[8]), float(0))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.distance(ind[1], ind[integer(0)]), float(0))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.distance(ind[5]), c(15.0, 5, 4, 3, 2, 0, 2, 3, 15, 20))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.distance(ind[5], NULL), c(15.0, 5, 4, 3, 2, 0, 2, 3, 15, 20))) stop(); }", __LINE__);
		
		// Test InteractionType – (float)interactionDistance(object<Individual>$ receiver, [No<Individual> exerters = NULL])
		if (!sex_seg_on)
		{
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (i1.interactionDistance(ind[0], ind[2]) == 11.0) stop(); }", __LINE__);
			SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "if (identical(i1.interactionDistance(ind[0:1], ind[2]), c(11.0, 1.0))) stop(); }", 1, 574, "must be a singleton", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.interactionDistance(ind[0], ind[2:3]), c(11.0, 12.0))) stop(); }", __LINE__);
			SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "if (i1.interactionDistance(ind[0:1], ind[2:3]) == 11.0) stop(); }", 1, 564, "must be a singleton", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.interactionDistance(ind[5], ind[c(0, 5, 9, 8, 1)]), c(15.0, INF, 20, 15, 5))) stop(); }", __LINE__);
			SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "if (identical(i1.interactionDistance(ind[integer(0)], ind[8]), float(0))) stop(); }", 1, 574, "must be a singleton", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.interactionDistance(ind[1], ind[integer(0)]), float(0))) stop(); }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.interactionDistance(ind[5]), c(15.0, 5, 4, 3, 2, INF, 2, 3, 15, 20))) stop(); }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.interactionDistance(ind[5], NULL), c(15.0, 5, 4, 3, 2, INF, 2, 3, 15, 20))) stop(); }", __LINE__);
		}
		else
		{
			// comprehensively testing all the different sex-seg cases is complicated, but we can at least test the two branches of the code against each other
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.interactionDistance(ind[5]), i1.interactionDistance(ind[5], NULL))) stop(); }", __LINE__);
		}
		
		// Test InteractionType – (float)distanceToPoint(object<Individual> individuals1, float point)
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (i1.distanceToPoint(ind[0], c(" + (use_first_coordinate ? "1.0, 0.0" : "0.0, 1.0") + ")) == 11.0) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.distanceToPoint(ind[0:1], c(" + (use_first_coordinate ? "1.0, 0.0" : "0.0, 1.0") + ")), c(11.0, 1.0))) stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "if (i1.distanceToPoint(ind[0:1], 1.0) == 11.0) stop(); }", 1, 564, "point is of length equal to", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.distanceToPoint(ind[c(0, 5, 9, 8, 1)], c(" + (use_first_coordinate ? "5.0, 0.0" : "0.0, 5.0") + ")), c(15.0, 0, 20, 15, 5))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.distanceToPoint(ind[integer(0)], c(" + (use_first_coordinate ? "8.0, 0.0" : "0.0, 8.0") + ")), float(0))) stop(); }", __LINE__);
		
		// Test InteractionType – (object<Individual>)drawByStrength(object<Individual>$ individual, [integer$ count = 1])
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.drawByStrength(ind[0]); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.drawByStrength(ind[0], 1); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.drawByStrength(ind[0], 50); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.drawByStrength(ind[0], 0), ind[integer(0)])) stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.drawByStrength(ind[0], -1); stop(); }", 1, 560, "requires count >= 0", __LINE__);
		
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.drawByStrength(ind[0]); stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.drawByStrength(ind[0], 1); stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.drawByStrength(ind[0], 50); stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.drawByStrength(ind[0], 0), ind[integer(0)])) stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.drawByStrength(ind[0], -1); stop(); } interaction(i1) { return 2.0; }", 1, 560, "requires count >= 0", __LINE__);
		
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.drawByStrength(ind[0]); stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.drawByStrength(ind[0], 1); stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.drawByStrength(ind[0], 50); stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.drawByStrength(ind[0], 0), ind[integer(0)])) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.drawByStrength(ind[0], -1); stop(); } interaction(i1) { return strength * 2.0; }", 1, 560, "requires count >= 0", __LINE__);
		
		// Test InteractionType – (void)evaluate([No<Subpopulation> subpops = NULL], [logical$ immediate = F])
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.evaluate(); i1.evaluate(); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.evaluate(p1); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.evaluate(NULL); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.evaluate(immediate=T); i1.evaluate(); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.evaluate(p1, immediate=T); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.evaluate(NULL, immediate=T); stop(); }", __LINE__);
		
		// Test InteractionType – (object<Individual>)nearestNeighbors(object<Individual>$ individual, [integer$ count = 1])
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "if (identical(i1.nearestNeighbors(ind[8], -1), ind[integer(0)])) stop(); }", 1, 574, "requires count >= 0", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.nearestNeighbors(ind[8], 0), ind[integer(0)])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.nearestNeighbors(ind[8], 1), ind[9])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(sortBy(i1.nearestNeighbors(ind[8], 3), 'index'), ind[c(6,7,9)])) stop(); }", __LINE__);
		
		// Test InteractionType – (object<Individual>)nearestInteractingNeighbors(object<Individual>$ individual, [integer$ count = 1])
		// Test InteractionType – (object<Individual>)interactingNeighborCount(object<Individual>$ individual, [integer$ count = 1])
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "if (identical(i1.nearestInteractingNeighbors(ind[8], -1), ind[integer(0)])) stop(); }", 1, 574, "requires count >= 0", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.nearestInteractingNeighbors(ind[8], 0), ind[integer(0)])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (size(i1.nearestInteractingNeighbors(ind[0], 100)) == i1.interactingNeighborCount(ind[0])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (size(i1.nearestInteractingNeighbors(ind[1], 100)) == i1.interactingNeighborCount(ind[1])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (size(i1.nearestInteractingNeighbors(ind[2], 100)) == i1.interactingNeighborCount(ind[2])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (size(i1.nearestInteractingNeighbors(ind[3], 100)) == i1.interactingNeighborCount(ind[3])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (size(i1.nearestInteractingNeighbors(ind[4], 100)) == i1.interactingNeighborCount(ind[4])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (size(i1.nearestInteractingNeighbors(ind[5], 100)) == i1.interactingNeighborCount(ind[5])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (size(i1.nearestInteractingNeighbors(ind[6], 100)) == i1.interactingNeighborCount(ind[6])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (size(i1.nearestInteractingNeighbors(ind[7], 100)) == i1.interactingNeighborCount(ind[7])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (size(i1.nearestInteractingNeighbors(ind[8], 100)) == i1.interactingNeighborCount(ind[8])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (size(i1.nearestInteractingNeighbors(ind[9], 100)) == i1.interactingNeighborCount(ind[9])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (size(i1.nearestInteractingNeighbors(ind[0], 100)) == sum(isFinite(i1.interactionDistance(ind[0])))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (size(i1.nearestInteractingNeighbors(ind[1], 100)) == sum(isFinite(i1.interactionDistance(ind[1])))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (size(i1.nearestInteractingNeighbors(ind[2], 100)) == sum(isFinite(i1.interactionDistance(ind[2])))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (size(i1.nearestInteractingNeighbors(ind[3], 100)) == sum(isFinite(i1.interactionDistance(ind[3])))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (size(i1.nearestInteractingNeighbors(ind[4], 100)) == sum(isFinite(i1.interactionDistance(ind[4])))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (size(i1.nearestInteractingNeighbors(ind[5], 100)) == sum(isFinite(i1.interactionDistance(ind[5])))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (size(i1.nearestInteractingNeighbors(ind[6], 100)) == sum(isFinite(i1.interactionDistance(ind[6])))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (size(i1.nearestInteractingNeighbors(ind[7], 100)) == sum(isFinite(i1.interactionDistance(ind[7])))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (size(i1.nearestInteractingNeighbors(ind[8], 100)) == sum(isFinite(i1.interactionDistance(ind[8])))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (size(i1.nearestInteractingNeighbors(ind[9], 100)) == sum(isFinite(i1.interactionDistance(ind[9])))) stop(); }", __LINE__);
		
		// Test InteractionType – (object<Individual>)nearestNeighborsOfPoint(object<Subpopulation>$ subpop, float point, [integer$ count = 1])
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "if (identical(i1.nearestNeighborsOfPoint(p1, c(5.0, 0.0), -1), ind[integer(0)])) stop(); }", 1, 574, "requires count >= 0", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.nearestNeighborsOfPoint(p1, c(5.0, 0.0), 0), ind[integer(0)])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.nearestNeighborsOfPoint(p1, c(" + (use_first_coordinate ? "19.0, 0.0" : "0.0, 19.0") + "), 1), ind[8])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(sortBy(i1.nearestNeighborsOfPoint(p1, c(" + (use_first_coordinate ? "19.0, 0.0" : "0.0, 19.0") + "), 3), 'index'), ind[c(7,8,9)])) stop(); }", __LINE__);
		
		// Test InteractionType – (void)setInteractionFunction(string$ functionType, ...)
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.setInteractionFunction('q', 10.0); i1.evaluate(immediate=T); stop(); }", 1, 560, "while the interaction is being evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.setInteractionFunction('q', 10.0); i1.evaluate(immediate=T); stop(); }", 1, 577, "functionType \"q\" must be", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.setInteractionFunction('f', 5.0); i1.evaluate(immediate=T); stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.setInteractionFunction('f'); i1.evaluate(immediate=T); stop(); }", 1, 577, "requires exactly", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.setInteractionFunction('f', 5.0, 2.0); i1.evaluate(immediate=T); stop(); }", 1, 577, "requires exactly", __LINE__);
		
		if (!max_dist_on)
			SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.setInteractionFunction('l', 5.0); i1.evaluate(immediate=T); stop(); }", 1, 577, "finite maximum interaction distance", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('l', 5.0); i1.evaluate(immediate=T); stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('l'); i1.evaluate(immediate=T); stop(); }", 1, 597, "requires exactly", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('l', 5.0, 2.0); i1.evaluate(immediate=T); stop(); }", 1, 597, "requires exactly", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('e', 5.0, 1.0); i1.evaluate(immediate=T); stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('e', 5.0); i1.evaluate(immediate=T); stop(); }", 1, 597, "requires exactly", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('e', 5.0, 2.0, 1.0); i1.evaluate(immediate=T); stop(); }", 1, 597, "requires exactly", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('n', 5.0, 1.0); i1.evaluate(immediate=T); stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('n', 5.0); i1.evaluate(immediate=T); stop(); }", 1, 597, "requires exactly", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('n', 5.0, 2.0, 1.0); i1.evaluate(immediate=T); stop(); }", 1, 597, "requires exactly", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('c', 5.0, 1.0); i1.evaluate(immediate=T); stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('c', 5.0); i1.evaluate(immediate=T); stop(); }", 1, 597, "requires exactly", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('c', 5.0, 2.0, 1.0); i1.evaluate(immediate=T); stop(); }", 1, 597, "requires exactly", __LINE__);
		
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('n', 5.0, -1.0); stop(); }", 1, 597, "must have a standard deviation parameter >= 0", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('c', 5.0, 0.0); stop(); }", 1, 597, "must have a scale parameter > 0", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('c', 5.0, -1.0); stop(); }", 1, 597, "must have a scale parameter > 0", __LINE__);
		
		// Test InteractionType – (float)strength(object<Individual> individuals1, [No<Individual> individuals2 = NULL])
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
		
		// Test InteractionType – (float)totalOfNeighborStrengths(object<Individual> individuals)
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
		
		// Test InteractionType – (void)unevaluate(void)
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.evaluate(); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.unevaluate(); stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.distance(ind[0], ind[2]); stop(); }", 1, 577, "has been evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.interactionDistance(ind[0], ind[2]); stop(); }", 1, 577, "has been evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.distanceToPoint(ind[0], c(1.0, 0.0)); stop(); }", 1, 577, "has been evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.drawByStrength(ind[0]); stop(); }", 1, 577, "has been evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.nearestNeighbors(ind[8], 1); stop(); }", 1, 577, "has been evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.nearestInteractingNeighbors(ind[8], 1); stop(); }", 1, 577, "has been evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.interactingNeighborCount(ind[8]); stop(); }", 1, 577, "has been evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.nearestNeighborsOfPoint(p1, 19.0, 1); stop(); }", 1, 577, "has been evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.strength(ind[0], ind[2]); stop(); }", 1, 577, "has been evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.totalOfNeighborStrengths(ind[0]); stop(); }", 1, 577, "has been evaluated", __LINE__);
	}
	
	// *** 3D with y and z zero
	std::string gen1_setup_i1xyz_pop("initialize() { initializeSLiMOptions(dimensionality='xyz'); " + sex_string + "initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'xyz', " + reciprocal_string + ", maxDistance=" + p_max_distance + ", sexSegregation='" + p_sex_segregation + "'); } 1 { sim.addSubpop('p1', 10); p1.individuals.x = c(-10.0, 0, 1, 2, 3, 5, 7, 8, 20, 25); p1.individuals.y = 0; p1.individuals.z = 0; i1.evaluate(" + immediate_string + "); ind = p1.individuals; ");
	
	// Test InteractionType – (float)distance(object<Individual> individuals1, [No<Individual> individuals2 = NULL])
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (i1.distance(ind[0], ind[2]) == 11.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.distance(ind[0:1], ind[2]), c(11.0, 1.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.distance(ind[0], ind[2:3]), c(11.0, 12.0))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "if (i1.distance(ind[0:1], ind[2:3]) == 11.0) stop(); }", 1, 557, "either individuals1 or individuals2 be", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.distance(ind[5], ind[c(0, 5, 9, 8, 1)]), c(15.0, 0, 20, 15, 5))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.distance(ind[integer(0)], ind[8]), float(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.distance(ind[1], ind[integer(0)]), float(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.distance(ind[5]), c(15.0, 5, 4, 3, 2, 0, 2, 3, 15, 20))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.distance(ind[5], NULL), c(15.0, 5, 4, 3, 2, 0, 2, 3, 15, 20))) stop(); }", __LINE__);
	
	// Test InteractionType – (float)interactionDistance(object<Individual>$ receiver, [No<Individual> exerters = NULL])
	if (!sex_seg_on)
	{
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (i1.interactionDistance(ind[0], ind[2]) == 11.0) stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "if (identical(i1.interactionDistance(ind[0:1], ind[2]), c(11.0, 1.0))) stop(); }", 1, 567, "must be a singleton", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.interactionDistance(ind[0], ind[2:3]), c(11.0, 12.0))) stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "if (i1.interactionDistance(ind[0:1], ind[2:3]) == 11.0) stop(); }", 1, 557, "must be a singleton", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.interactionDistance(ind[5], ind[c(0, 5, 9, 8, 1)]), c(15.0, INF, 20, 15, 5))) stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "if (identical(i1.interactionDistance(ind[integer(0)], ind[8]), float(0))) stop(); }", 1, 567, "must be a singleton", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.interactionDistance(ind[1], ind[integer(0)]), float(0))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.interactionDistance(ind[5]), c(15.0, 5, 4, 3, 2, INF, 2, 3, 15, 20))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.interactionDistance(ind[5], NULL), c(15.0, 5, 4, 3, 2, INF, 2, 3, 15, 20))) stop(); }", __LINE__);
	}
	else
	{
		// comprehensively testing all the different sex-seg cases is complicated, but we can at least test the two branches of the code against each other
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.interactionDistance(ind[5]), i1.interactionDistance(ind[5], NULL))) stop(); }", __LINE__);
	}
	
	// Test InteractionType – (float)distanceToPoint(object<Individual> individuals1, float point)
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (i1.distanceToPoint(ind[0], c(1.0, 0.0, 0.0)) == 11.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.distanceToPoint(ind[0:1], c(1.0, 0.0, 0.0)), c(11.0, 1.0))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "if (i1.distanceToPoint(ind[0:1], 1.0) == 11.0) stop(); }", 1, 557, "point is of length equal to", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.distanceToPoint(ind[c(0, 5, 9, 8, 1)], c(5.0, 0.0, 0.0)), c(15.0, 0, 20, 15, 5))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.distanceToPoint(ind[integer(0)], c(8.0, 0.0, 0.0)), float(0))) stop(); }", __LINE__);
	
	// Test InteractionType – (object<Individual>)drawByStrength(object<Individual>$ individual, [integer$ count = 1])
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.drawByStrength(ind[0]); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.drawByStrength(ind[0], 1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.drawByStrength(ind[0], 50); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.drawByStrength(ind[0], 0), ind[integer(0)])) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.drawByStrength(ind[0], -1); stop(); }", 1, 553, "requires count >= 0", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.drawByStrength(ind[0]); stop(); } interaction(i1) { return 2.0; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.drawByStrength(ind[0], 1); stop(); } interaction(i1) { return 2.0; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.drawByStrength(ind[0], 50); stop(); } interaction(i1) { return 2.0; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.drawByStrength(ind[0], 0), ind[integer(0)])) stop(); } interaction(i1) { return 2.0; }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.drawByStrength(ind[0], -1); stop(); } interaction(i1) { return 2.0; }", 1, 553, "requires count >= 0", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.drawByStrength(ind[0]); stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.drawByStrength(ind[0], 1); stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.drawByStrength(ind[0], 50); stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.drawByStrength(ind[0], 0), ind[integer(0)])) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.drawByStrength(ind[0], -1); stop(); } interaction(i1) { return strength * 2.0; }", 1, 553, "requires count >= 0", __LINE__);
	
	// Test InteractionType – (void)evaluate([No<Subpopulation> subpops = NULL], [logical$ immediate = F])
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.evaluate(); i1.evaluate(); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.evaluate(p1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.evaluate(NULL); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.evaluate(immediate=T); i1.evaluate(); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.evaluate(p1, immediate=T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.evaluate(NULL, immediate=T); stop(); }", __LINE__);
	
	// Test InteractionType – (object<Individual>)nearestNeighbors(object<Individual>$ individual, [integer$ count = 1])
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "if (identical(i1.nearestNeighbors(ind[8], -1), ind[integer(0)])) stop(); }", 1, 567, "requires count >= 0", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.nearestNeighbors(ind[8], 0), ind[integer(0)])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.nearestNeighbors(ind[8], 1), ind[9])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(sortBy(i1.nearestNeighbors(ind[8], 3), 'index'), ind[c(6,7,9)])) stop(); }", __LINE__);
	
	// Test InteractionType – (object<Individual>)nearestInteractingNeighbors(object<Individual>$ individual, [integer$ count = 1])
	// Test InteractionType – (object<Individual>)interactingNeighborCount(object<Individual>$ individual, [integer$ count = 1])
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "if (identical(i1.nearestInteractingNeighbors(ind[8], -1), ind[integer(0)])) stop(); }", 1, 567, "requires count >= 0", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.nearestInteractingNeighbors(ind[8], 0), ind[integer(0)])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (size(i1.nearestInteractingNeighbors(ind[0], 100)) == i1.interactingNeighborCount(ind[0])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (size(i1.nearestInteractingNeighbors(ind[1], 100)) == i1.interactingNeighborCount(ind[1])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (size(i1.nearestInteractingNeighbors(ind[2], 100)) == i1.interactingNeighborCount(ind[2])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (size(i1.nearestInteractingNeighbors(ind[3], 100)) == i1.interactingNeighborCount(ind[3])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (size(i1.nearestInteractingNeighbors(ind[4], 100)) == i1.interactingNeighborCount(ind[4])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (size(i1.nearestInteractingNeighbors(ind[5], 100)) == i1.interactingNeighborCount(ind[5])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (size(i1.nearestInteractingNeighbors(ind[6], 100)) == i1.interactingNeighborCount(ind[6])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (size(i1.nearestInteractingNeighbors(ind[7], 100)) == i1.interactingNeighborCount(ind[7])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (size(i1.nearestInteractingNeighbors(ind[8], 100)) == i1.interactingNeighborCount(ind[8])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (size(i1.nearestInteractingNeighbors(ind[9], 100)) == i1.interactingNeighborCount(ind[9])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (size(i1.nearestInteractingNeighbors(ind[0], 100)) == sum(isFinite(i1.interactionDistance(ind[0])))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (size(i1.nearestInteractingNeighbors(ind[1], 100)) == sum(isFinite(i1.interactionDistance(ind[1])))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (size(i1.nearestInteractingNeighbors(ind[2], 100)) == sum(isFinite(i1.interactionDistance(ind[2])))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (size(i1.nearestInteractingNeighbors(ind[3], 100)) == sum(isFinite(i1.interactionDistance(ind[3])))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (size(i1.nearestInteractingNeighbors(ind[4], 100)) == sum(isFinite(i1.interactionDistance(ind[4])))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (size(i1.nearestInteractingNeighbors(ind[5], 100)) == sum(isFinite(i1.interactionDistance(ind[5])))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (size(i1.nearestInteractingNeighbors(ind[6], 100)) == sum(isFinite(i1.interactionDistance(ind[6])))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (size(i1.nearestInteractingNeighbors(ind[7], 100)) == sum(isFinite(i1.interactionDistance(ind[7])))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (size(i1.nearestInteractingNeighbors(ind[8], 100)) == sum(isFinite(i1.interactionDistance(ind[8])))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (size(i1.nearestInteractingNeighbors(ind[9], 100)) == sum(isFinite(i1.interactionDistance(ind[9])))) stop(); }", __LINE__);
	
	// Test InteractionType – (object<Individual>)nearestNeighborsOfPoint(object<Subpopulation>$ subpop, float point, [integer$ count = 1])
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "if (identical(i1.nearestNeighborsOfPoint(p1, c(5.0, 0.0, 0.0), -1), ind[integer(0)])) stop(); }", 1, 567, "requires count >= 0", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.nearestNeighborsOfPoint(p1, c(5.0, 0.0, 0.0), 0), ind[integer(0)])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.nearestNeighborsOfPoint(p1, c(19.0, 0.0, 0.0), 1), ind[8])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(sortBy(i1.nearestNeighborsOfPoint(p1, c(19.0, 0.0, 0.0), 3), 'index'), ind[c(7,8,9)])) stop(); }", __LINE__);
	
	// Test InteractionType – (void)setInteractionFunction(string$ functionType, ...)
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.setInteractionFunction('q', 10.0); i1.evaluate(immediate=T); stop(); }", 1, 553, "while the interaction is being evaluated", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.setInteractionFunction('q', 10.0); i1.evaluate(immediate=T); stop(); }", 1, 570, "functionType \"q\" must be", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.setInteractionFunction('f', 5.0); i1.evaluate(immediate=T); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.setInteractionFunction('f'); i1.evaluate(immediate=T); stop(); }", 1, 570, "requires exactly", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.setInteractionFunction('f', 5.0, 2.0); i1.evaluate(immediate=T); stop(); }", 1, 570, "requires exactly", __LINE__);
	
	if (!max_dist_on)
		SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.setInteractionFunction('l', 5.0); i1.evaluate(immediate=T); stop(); }", 1, 570, "finite maximum interaction distance", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('l', 5.0); i1.evaluate(immediate=T); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('l'); i1.evaluate(immediate=T); stop(); }", 1, 590, "requires exactly", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('l', 5.0, 2.0); i1.evaluate(immediate=T); stop(); }", 1, 590, "requires exactly", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('e', 5.0, 1.0); i1.evaluate(immediate=T); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('e', 5.0); i1.evaluate(immediate=T); stop(); }", 1, 590, "requires exactly", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('e', 5.0, 2.0, 1.0); i1.evaluate(immediate=T); stop(); }", 1, 590, "requires exactly", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('n', 5.0, 1.0); i1.evaluate(immediate=T); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('n', 5.0); i1.evaluate(immediate=T); stop(); }", 1, 590, "requires exactly", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('n', 5.0, 2.0, 1.0); i1.evaluate(immediate=T); stop(); }", 1, 590, "requires exactly", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('c', 5.0, 1.0); i1.evaluate(immediate=T); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('c', 5.0); i1.evaluate(immediate=T); stop(); }", 1, 590, "requires exactly", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('c', 5.0, 2.0, 1.0); i1.evaluate(immediate=T); stop(); }", 1, 590, "requires exactly", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('n', 5.0, -1.0); stop(); }", 1, 590, "must have a standard deviation parameter >= 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('c', 5.0, 0.0); stop(); }", 1, 590, "must have a scale parameter > 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('c', 5.0, -1.0); stop(); }", 1, 590, "must have a scale parameter > 0", __LINE__);
	
	// Test InteractionType – (float)strength(object<Individual> individuals1, [No<Individual> individuals2 = NULL])
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
	
	// Test InteractionType – (float)totalOfNeighborStrengths(object<Individual> individuals)
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
	
	// Test InteractionType – (void)unevaluate(void)
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.evaluate(); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.unevaluate(); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.distance(ind[0], ind[2]); stop(); }", 1, 570, "has been evaluated", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.interactionDistance(ind[0], ind[2]); stop(); }", 1, 570, "has been evaluated", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.distanceToPoint(ind[0], c(1.0, 0.0, 0.0)); stop(); }", 1, 570, "has been evaluated", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.drawByStrength(ind[0]); stop(); }", 1, 570, "has been evaluated", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.nearestNeighbors(ind[8], 1); stop(); }", 1, 570, "has been evaluated", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.nearestInteractingNeighbors(ind[8], 1); stop(); }", 1, 570, "has been evaluated", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.interactingNeighborCount(ind[8]); stop(); }", 1, 570, "has been evaluated", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.nearestNeighborsOfPoint(p1, 19.0, 1); stop(); }", 1, 570, "has been evaluated", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.strength(ind[0], ind[2]); stop(); }", 1, 570, "has been evaluated", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.totalOfNeighborStrengths(ind[0]); stop(); }", 1, 570, "has been evaluated", __LINE__);
	
	// *** 3D with full 3D coordinates; we skip the error-testing here since it's the same as before
	std::string gen1_setup_i1xyz_pop_full("initialize() { initializeSLiMOptions(dimensionality='xyz'); " + sex_string + "initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'xyz', " + reciprocal_string + ", maxDistance=" + p_max_distance + ", sexSegregation='" + p_sex_segregation + "'); } 1 { sim.addSubpop('p1', 10); p1.individuals.x = c(-10.0, 0, 1, 2, 3, 5, 7, 8, 20, 25); p1.individuals.y = c(12.0, 3, -2, 10, 8, 72, 0, -5, -13, 7); p1.individuals.z = c(0.0, 5, 9, -6, 6, -16, 2, 1, -1, 8); i1.evaluate(" + immediate_string + "); ind = p1.individuals; ");
	
	// Test InteractionType – (float)distance(object<Individual> individuals1, [No<Individual> individuals2 = NULL])
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (i1.distance(ind[0], ind[2]) == sqrt(11^2 + 14^2 + 9^2)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(i1.distance(ind[0:1], ind[2]), c(sqrt(11^2 + 14^2 + 9^2), sqrt(1^2 + 5^2 + 4^2)))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(i1.distance(ind[0], ind[2:3]), c(sqrt(11^2 + 14^2 + 9^2), sqrt(12^2 + 2^2 + 6^2)))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (all(i1.distance(ind[5]) - c(63.882705, 72.2979, 78.2112, 62.8728, 67.7052,  0.0, 74.2428, 78.9113, 87.6070, 72.1179) < 0.001)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (all(i1.distance(ind[5], NULL) - c(63.882705, 72.2979, 78.2112, 62.8728, 67.7052,  0.0, 74.2428, 78.9113, 87.6070, 72.1179) < 0.001)) stop(); }", __LINE__);
	
	// Test InteractionType – (float)interactionDistance(object<Individual>$ receiver, [No<Individual> exerters = NULL])
	if (!sex_seg_on)
	{
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (i1.interactionDistance(ind[0], ind[2]) - sqrt(11^2 + 14^2 + 9^2) < 0.001) stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xyz_pop_full + "if (identical(i1.interactionDistance(ind[0:1], ind[2]), c(sqrt(11^2 + 14^2 + 9^2), sqrt(1^2 + 5^2 + 4^2)))) stop(); }", 1, 642, "must be a singleton", __LINE__);
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
	
	// Test InteractionType – (float)distanceToPoint(object<Individual> individuals1, float point)
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (i1.distanceToPoint(ind[0], c(-7.0, 12.0, 4.0)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(i1.distanceToPoint(ind[0:1], c(-7.0, 12.0, 4.0)), c(5.0, sqrt(7^2 + 9^2 + 1^2)))) stop(); }", __LINE__);
	
	// Test InteractionType – (object<Individual>)drawByStrength(object<Individual>$ individual, [integer$ count = 1])
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.drawByStrength(ind[0]); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.drawByStrength(ind[0], 1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.drawByStrength(ind[0], 50); stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.drawByStrength(ind[0]); stop(); } interaction(i1) { return 2.0; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.drawByStrength(ind[0], 1); stop(); } interaction(i1) { return 2.0; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.drawByStrength(ind[0], 50); stop(); } interaction(i1) { return 2.0; }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.drawByStrength(ind[0]); stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.drawByStrength(ind[0], 1); stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.drawByStrength(ind[0], 50); stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
	
	// Test InteractionType – (void)evaluate([No<Subpopulation> subpops = NULL], [logical$ immediate = F])
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.evaluate(); i1.evaluate(); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.evaluate(p1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.evaluate(NULL); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.evaluate(immediate=T); i1.evaluate(); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.evaluate(p1, immediate=T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.evaluate(NULL, immediate=T); stop(); }", __LINE__);
	
	// Test InteractionType – (object<Individual>)nearestNeighbors(object<Individual>$ individual, [integer$ count = 1])
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(i1.nearestNeighbors(ind[8], 1), ind[7])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(sortBy(i1.nearestNeighbors(ind[8], 3), 'index'), ind[c(6,7,9)])) stop(); }", __LINE__);
	
	// Test InteractionType – (object<Individual>)nearestInteractingNeighbors(object<Individual>$ individual, [integer$ count = 1])
	// Test InteractionType – (object<Individual>)interactingNeighborCount(object<Individual>$ individual, [integer$ count = 1])
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop_full + "if (identical(i1.nearestInteractingNeighbors(ind[8], -1), ind[integer(0)])) stop(); }", 1, 642, "requires count >= 0", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(i1.nearestInteractingNeighbors(ind[8], 0), ind[integer(0)])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (size(i1.nearestInteractingNeighbors(ind[0], 100)) == i1.interactingNeighborCount(ind[0])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (size(i1.nearestInteractingNeighbors(ind[1], 100)) == i1.interactingNeighborCount(ind[1])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (size(i1.nearestInteractingNeighbors(ind[2], 100)) == i1.interactingNeighborCount(ind[2])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (size(i1.nearestInteractingNeighbors(ind[3], 100)) == i1.interactingNeighborCount(ind[3])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (size(i1.nearestInteractingNeighbors(ind[4], 100)) == i1.interactingNeighborCount(ind[4])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (size(i1.nearestInteractingNeighbors(ind[5], 100)) == i1.interactingNeighborCount(ind[5])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (size(i1.nearestInteractingNeighbors(ind[6], 100)) == i1.interactingNeighborCount(ind[6])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (size(i1.nearestInteractingNeighbors(ind[7], 100)) == i1.interactingNeighborCount(ind[7])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (size(i1.nearestInteractingNeighbors(ind[8], 100)) == i1.interactingNeighborCount(ind[8])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (size(i1.nearestInteractingNeighbors(ind[9], 100)) == i1.interactingNeighborCount(ind[9])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (size(i1.nearestInteractingNeighbors(ind[0], 100)) == sum(isFinite(i1.interactionDistance(ind[0])))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (size(i1.nearestInteractingNeighbors(ind[1], 100)) == sum(isFinite(i1.interactionDistance(ind[1])))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (size(i1.nearestInteractingNeighbors(ind[2], 100)) == sum(isFinite(i1.interactionDistance(ind[2])))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (size(i1.nearestInteractingNeighbors(ind[3], 100)) == sum(isFinite(i1.interactionDistance(ind[3])))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (size(i1.nearestInteractingNeighbors(ind[4], 100)) == sum(isFinite(i1.interactionDistance(ind[4])))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (size(i1.nearestInteractingNeighbors(ind[5], 100)) == sum(isFinite(i1.interactionDistance(ind[5])))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (size(i1.nearestInteractingNeighbors(ind[6], 100)) == sum(isFinite(i1.interactionDistance(ind[6])))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (size(i1.nearestInteractingNeighbors(ind[7], 100)) == sum(isFinite(i1.interactionDistance(ind[7])))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (size(i1.nearestInteractingNeighbors(ind[8], 100)) == sum(isFinite(i1.interactionDistance(ind[8])))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (size(i1.nearestInteractingNeighbors(ind[9], 100)) == sum(isFinite(i1.interactionDistance(ind[9])))) stop(); }", __LINE__);
	
	// Test InteractionType – (object<Individual>)nearestNeighborsOfPoint(object<Subpopulation>$ subpop, float point, [integer$ count = 1])
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(i1.nearestNeighborsOfPoint(p1, c(-7.0, 12.0, 4.0), 1), ind[0])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(i1.nearestNeighborsOfPoint(p1, c(7.0, 3.0, 12.0), 1), ind[2])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(sortBy(i1.nearestNeighborsOfPoint(p1, c(19.0, -4.0, -2.0), 3), 'index'), ind[c(6,7,8)])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(sortBy(i1.nearestNeighborsOfPoint(p1, c(7.0, 3.0, 12.0), 3), 'index'), ind[c(1,2,4)])) stop(); }", __LINE__);
	
	// Test InteractionType – (void)setInteractionFunction(string$ functionType, ...)
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.unevaluate(); i1.setInteractionFunction('f', 5.0); i1.evaluate(immediate=T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('l', 5.0); i1.evaluate(immediate=T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('e', 5.0, 1.0); i1.evaluate(immediate=T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('n', 5.0, 1.0); i1.evaluate(immediate=T); stop(); }", __LINE__);
	
	// Test InteractionType – (float)strength(object<Individual> individuals1, [No<Individual> individuals2 = NULL])
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
	
	// Test InteractionType – (float)totalOfNeighborStrengths(object<Individual> individuals)
	if (!sex_seg_on)
	{
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(i1.totalOfNeighborStrengths(ind[0]), 9.0)) stop(); }", __LINE__);
		
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(i1.totalOfNeighborStrengths(ind[0]), 18.0)) stop(); } interaction(i1) { return 2.0; }", __LINE__);
		
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(i1.totalOfNeighborStrengths(ind[0]), 18.0)) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
	}
	
	// Test InteractionType – (void)unevaluate(void)
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.unevaluate(); i1.evaluate(); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.unevaluate(); i1.unevaluate(); stop(); }", __LINE__);
}

#pragma mark Continuous space tests
void _RunContinuousSpaceTests(void)
{
	// Since these tests are so different from others – spatiality has to be enabled, interactions have to be set up,
	// etc. – I decided to put them in their own test function, rather than wedging them into the class tests above.
	// Tests of the basic functionality of properties and methods remain in the class tests, however.
}

#pragma mark nonWF model tests
void _RunNonWFTests(void)
{
	// Test properties and methods that should be disabled in nonWF mode
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "1 { p1.setSubpopulationSize(500); } ", 1, 301, "not available in nonWF models", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "1 { p1.cloningRate; } ", 1, 301, "not available in nonWF models", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "1 { p1.setCloningRate(0.5); } ", 1, 301, "not available in nonWF models", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "1 { p1.selfingRate; } ", 1, 301, "not available in nonWF models", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "1 { p1.setSelfingRate(0.5); } ", 1, 301, "not available in nonWF models", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_sex_p1 + "1 { p1.sexRatio; } ", 1, 321, "not available in nonWF models", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_sex_p1 + "1 { p1.setSexRatio(0.5); } ", 1, 321, "not available in nonWF models", __LINE__);
	
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "1 { sim.addSubpopSplit(2, 100, p1); } ", 1, 302, "not available in nonWF models", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "1 { p1.immigrantSubpopFractions; } ", 1, 301, "not available in nonWF models", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "1 { p1.immigrantSubpopIDs; } ", 1, 301, "not available in nonWF models", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "1 { p1.setMigrationRates(2, 0.1); } ", 1, 301, "not available in nonWF models", __LINE__);
	
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "1 mateChoice() { return T; } ", 1, 296, "may not be defined in nonWF models", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "1 { sim.registerMateChoiceCallback(NULL, '{ return T; } '); } ", 1, 302, "not available in nonWF models", __LINE__);
	
	// Test properties and methods that should be disabled in WF mode
	SLiMAssertScriptRaise(WF_prefix + gen1_setup_p1 + "1 { p1.individuals.age; } ", 1, 310, "not available in WF models", __LINE__);
	
	SLiMAssertScriptRaise(WF_prefix + gen1_setup_p1 + "1 { p1.removeSubpopulation(); stop(); }", 1, 298, "not available in WF models", __LINE__);
	SLiMAssertScriptRaise(WF_prefix + gen1_setup_p1 + "1 { p1.takeMigrants(p1.individuals); stop(); }", 1, 298, "not available in WF models", __LINE__);
	SLiMAssertScriptRaise(WF_prefix + gen1_setup_p1 + "1 { p1.addCloned(p1.individuals[0]); stop(); }", 1, 298, "not available in WF models", __LINE__);
	SLiMAssertScriptRaise(WF_prefix + gen1_setup_p1 + "1 { p1.addCrossed(p1.individuals[0], p1.individuals[1]); stop(); }", 1, 298, "not available in WF models", __LINE__);
	SLiMAssertScriptRaise(WF_prefix + gen1_setup_p1 + "1 { p1.addEmpty(); stop(); }", 1, 298, "not available in WF models", __LINE__);
	SLiMAssertScriptRaise(WF_prefix + gen1_setup_p1 + "1 { p1.addSelfed(p1.individuals[0]); stop(); }", 1, 298, "not available in WF models", __LINE__);
	
	SLiMAssertScriptRaise(WF_prefix + gen1_setup_p1 + "1 reproduction() { return; } ", 1, 293, "may not be defined in WF models", __LINE__);
	
	// SLiMSim.modelType
	SLiMAssertScriptStop(nonWF_prefix + gen1_setup + "1 { if (sim.modelType == 'nonWF') stop(); } ", __LINE__);
	SLiMAssertScriptStop(nonWF_prefix + gen1_setup_sex + "1 { if (sim.modelType == 'nonWF') stop(); } ", __LINE__);
	
	// Individual.age
	SLiMAssertScriptStop(nonWF_prefix + gen1_setup_p1 + "1 { p1.individuals.age; stop(); } ", __LINE__);
	
	// Subpopulation - (void)removeSubpopulation()
	SLiMAssertScriptStop(nonWF_prefix + gen1_setup_p1 + "1 { p1.removeSubpopulation(); stop(); }", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "1 { p1.removeSubpopulation(); if (p1.individualCount == 10) stop(); }", 1, 328, "undefined identifier", __LINE__);		// the symbol is undefined immediately
	SLiMAssertScriptStop(nonWF_prefix + gen1_setup_p1 + "1 { px=p1; p1.removeSubpopulation(); if (px.individualCount == 10) stop(); }", __LINE__);									// does not take visible effect until child generation
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "1 { p1.removeSubpopulation(); } 2 { if (p1.individualCount == 0) stop(); }", 1, 334, "undefined identifier", __LINE__);
}

#pragma mark treeseq tests
void _RunTreeSeqTests(std::string temp_path)
{
	// initializeTreeSeq()
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(); } " + gen1_setup_p1 + "100 { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=F, simplificationRatio=10.0, checkCoalescence=F, runCrosschecks=F); } " + gen1_setup_p1 + "100 { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=T, simplificationRatio=10.0, checkCoalescence=F, runCrosschecks=F); } " + gen1_setup_p1 + "100 { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=F, simplificationRatio=INF, checkCoalescence=F, runCrosschecks=F); } " + gen1_setup_p1 + "100 { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=T, simplificationRatio=INF, checkCoalescence=F, runCrosschecks=F); } " + gen1_setup_p1 + "100 { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=F, simplificationRatio=0.0, checkCoalescence=F, runCrosschecks=F); } " + gen1_setup_p1 + "100 { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=T, simplificationRatio=0.0, checkCoalescence=F, runCrosschecks=F); } " + gen1_setup_p1 + "100 { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=F, simplificationRatio=10.0, checkCoalescence=T, runCrosschecks=F); } " + gen1_setup_p1 + "100 { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=T, simplificationRatio=10.0, checkCoalescence=T, runCrosschecks=F); } " + gen1_setup_p1 + "100 { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=F, simplificationRatio=INF, checkCoalescence=T, runCrosschecks=F); } " + gen1_setup_p1 + "100 { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=T, simplificationRatio=INF, checkCoalescence=T, runCrosschecks=F); } " + gen1_setup_p1 + "100 { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=F, simplificationRatio=0.0, checkCoalescence=T, runCrosschecks=F); } " + gen1_setup_p1 + "100 { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=T, simplificationRatio=0.0, checkCoalescence=T, runCrosschecks=F); } " + gen1_setup_p1 + "100 { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=F, simplificationRatio=10.0, checkCoalescence=F, runCrosschecks=T); } " + gen1_setup_p1 + "100 { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=T, simplificationRatio=10.0, checkCoalescence=F, runCrosschecks=T); } " + gen1_setup_p1 + "100 { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=F, simplificationRatio=INF, checkCoalescence=F, runCrosschecks=T); } " + gen1_setup_p1 + "100 { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=T, simplificationRatio=INF, checkCoalescence=F, runCrosschecks=T); } " + gen1_setup_p1 + "100 { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=F, simplificationRatio=0.0, checkCoalescence=F, runCrosschecks=T); } " + gen1_setup_p1 + "100 { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=T, simplificationRatio=0.0, checkCoalescence=F, runCrosschecks=T); } " + gen1_setup_p1 + "100 { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=F, simplificationRatio=10.0, checkCoalescence=T, runCrosschecks=T); } " + gen1_setup_p1 + "100 { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=T, simplificationRatio=10.0, checkCoalescence=T, runCrosschecks=T); } " + gen1_setup_p1 + "100 { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=F, simplificationRatio=INF, checkCoalescence=T, runCrosschecks=T); } " + gen1_setup_p1 + "100 { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=T, simplificationRatio=INF, checkCoalescence=T, runCrosschecks=T); } " + gen1_setup_p1 + "100 { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=F, simplificationRatio=0.0, checkCoalescence=T, runCrosschecks=T); } " + gen1_setup_p1 + "100 { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=T, simplificationRatio=0.0, checkCoalescence=T, runCrosschecks=T); } " + gen1_setup_p1 + "100 { stop(); }", __LINE__);
	
	// treeSeqCoalesced()
	SLiMAssertScriptRaise("initialize() { initializeTreeSeq(); } " + gen1_setup_p1 + "1: { sim.treeSeqCoalesced(); } 100 { stop(); }", 1, 290, "coalescence checking is enabled", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(checkCoalescence=T); } " + gen1_setup_p1 + "1: { sim.treeSeqCoalesced(); } 100 { stop(); }", __LINE__);
	
	// treeSeqSimplify()
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(); } " + gen1_setup_p1 + "50 { sim.treeSeqSimplify(); } 100 { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(); } " + gen1_setup_p1 + "1: { sim.treeSeqSimplify(); } 100 { stop(); }", __LINE__);
	
	// treeSeqRememberIndividuals()
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(); } " + gen1_setup_p1 + "50 { sim.treeSeqRememberIndividuals(p1.individuals); } 100 { sim.treeSeqSimplify(); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(); } " + gen1_setup_p1 + "1: { sim.treeSeqRememberIndividuals(p1.individuals); } 100 { sim.treeSeqSimplify(); stop(); }", __LINE__);
	
	// treeSeqOutput()
	if (Eidos_SlashTmpExists())
	{
		SLiMAssertScriptStop("initialize() { initializeTreeSeq(); } " + gen1_setup_p1 + "100 { sim.treeSeqOutput('" + temp_path + "/SLiM_treeSeq_1.trees', simplify=F, _binary=F); stop(); }", __LINE__);
		SLiMAssertScriptStop("initialize() { initializeTreeSeq(); } " + gen1_setup_p1 + "100 { sim.treeSeqOutput('" + temp_path + "/SLiM_treeSeq_2.trees', simplify=T, _binary=F); stop(); }", __LINE__);
		SLiMAssertScriptStop("initialize() { initializeTreeSeq(); } " + gen1_setup_p1 + "100 { sim.treeSeqOutput('" + temp_path + "/SLiM_treeSeq_3.trees', simplify=F, _binary=T); stop(); }", __LINE__);
		SLiMAssertScriptStop("initialize() { initializeTreeSeq(); } " + gen1_setup_p1 + "100 { sim.treeSeqOutput('" + temp_path + "/SLiM_treeSeq_4.trees', simplify=T, _binary=T); stop(); }", __LINE__);
		
		SLiMAssertScriptStop("initialize() { initializeTreeSeq(); } " + gen1_setup_p1 + "100 { sim.treeSeqOutput('" + temp_path + "/SLiM_treeSeq_1.trees', simplify=F, includeModel=F, _binary=F); stop(); }", __LINE__);
		SLiMAssertScriptStop("initialize() { initializeTreeSeq(); } " + gen1_setup_p1 + "100 { sim.treeSeqOutput('" + temp_path + "/SLiM_treeSeq_2.trees', simplify=T, includeModel=F, _binary=F); stop(); }", __LINE__);
		SLiMAssertScriptStop("initialize() { initializeTreeSeq(); } " + gen1_setup_p1 + "100 { sim.treeSeqOutput('" + temp_path + "/SLiM_treeSeq_3.trees', simplify=F, includeModel=F, _binary=T); stop(); }", __LINE__);
		SLiMAssertScriptStop("initialize() { initializeTreeSeq(); } " + gen1_setup_p1 + "100 { sim.treeSeqOutput('" + temp_path + "/SLiM_treeSeq_4.trees', simplify=T, includeModel=F, _binary=T); stop(); }", __LINE__);
	}
}

#pragma mark Nucleotide API tests
void _RunNucleotideFunctionTests(void)
{
	// nucleotidesToCodons()
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotidesToCodons(string(0)), integer(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotidesToCodons(integer(0)), integer(0))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotidesToCodons('A'); }", 1, 247, "multiple of three in length", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotidesToCodons(0); }", 1, 247, "multiple of three in length", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotidesToCodons('AA'); }", 1, 247, "multiple of three in length", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotidesToCodons(c(0,0)); }", 1, 247, "multiple of three in length", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (nucleotidesToCodons('AAA') == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (nucleotidesToCodons(c('A','A','A')) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (nucleotidesToCodons(c(0,0,0)) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (nucleotidesToCodons('AAC') == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (nucleotidesToCodons(c('A','A','C')) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (nucleotidesToCodons(c(0,0,1)) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (nucleotidesToCodons('AAG') == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (nucleotidesToCodons(c('A','A','G')) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (nucleotidesToCodons(c(0,0,2)) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (nucleotidesToCodons('AAT') == 3) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (nucleotidesToCodons(c('A','A','T')) == 3) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (nucleotidesToCodons(c(0,0,3)) == 3) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (nucleotidesToCodons('ACA') == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (nucleotidesToCodons(c('A','C','A')) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (nucleotidesToCodons(c(0,1,0)) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (nucleotidesToCodons('CAA') == 16) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (nucleotidesToCodons(c('C','A','A')) == 16) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (nucleotidesToCodons(c(1,0,0)) == 16) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (nucleotidesToCodons('TTT') == 63) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (nucleotidesToCodons(c('T','T','T')) == 63) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (nucleotidesToCodons(c(3,3,3)) == 63) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { if (nucleotidesToCodons('AAAA') == 0) stop(); }", 1, 251, "multiple of three in length", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { if (nucleotidesToCodons(c(0,0,0,0)) == 0) stop(); }", 1, 251, "multiple of three in length", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotidesToCodons('AAAAACAAGAATTTT'), c(0,1,2,3,63))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotidesToCodons(c('A','A','A','A','A','C','A','A','G','A','A','T','T','T','T')), c(0,1,2,3,63))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotidesToCodons(c(0,0,0,0,0,1,0,0,2,0,0,3,3,3,3)), c(0,1,2,3,63))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotidesToCodons('ADA'); }", 1, 247, "requires string sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotidesToCodons(c('A','D','A')); }", 1, 247, "requires string sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotidesToCodons(c(0,-1,0)); }", 1, 247, "requires integer sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotidesToCodons(c(0,4,0)); }", 1, 247, "requires integer sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotidesToCodons('AAAADA'); }", 1, 247, "requires string sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotidesToCodons(c('A','A','A','A','D','A')); }", 1, 247, "requires string sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotidesToCodons(c(0,0,0,0,-1,0)); }", 1, 247, "requires integer sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotidesToCodons(c(0,0,0,0,4,0)); }", 1, 247, "requires integer sequence values", __LINE__);
	
	// codonsToAminoAcids()
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToAminoAcids(integer(0), long=F, paste=T), '')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToAminoAcids(integer(0), long=T, paste=T), '')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToAminoAcids(integer(0), long=0, paste=T), integer(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToAminoAcids(integer(0), long=F, paste=F), string(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToAminoAcids(integer(0), long=T, paste=F), string(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToAminoAcids(integer(0), long=0, paste=F), integer(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToAminoAcids(0, long=F, paste=T), 'K')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToAminoAcids(0, long=T, paste=T), 'Lys')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToAminoAcids(0, long=0, paste=T), 12)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToAminoAcids(0, long=F, paste=F), 'K')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToAminoAcids(0, long=T, paste=F), 'Lys')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToAminoAcids(0, long=0, paste=F), 12)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToAminoAcids(c(0,1,63), long=F, paste=T), 'KNF')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToAminoAcids(c(0,1,63), long=T, paste=T), 'Lys-Asn-Phe')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToAminoAcids(c(0,1,63), long=0, paste=T), c(12, 3, 14))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToAminoAcids(c(0,1,63), long=F, paste=F), c('K','N','F'))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToAminoAcids(c(0,1,63), long=T, paste=F), c('Lys', 'Asn', 'Phe'))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToAminoAcids(c(0,1,63), long=0, paste=F), c(12, 3, 14))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToAminoAcids(-1, long=F, paste=T); }", 1, 247, "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToAminoAcids(-1, long=T, paste=T); }", 1, 247, "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToAminoAcids(-1, long=0, paste=T); }", 1, 247, "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToAminoAcids(-1, long=F, paste=F); }", 1, 247, "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToAminoAcids(-1, long=T, paste=F); }", 1, 247, "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToAminoAcids(-1, long=0, paste=F); }", 1, 247, "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToAminoAcids(64, long=F, paste=T); }", 1, 247, "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToAminoAcids(64, long=T, paste=T); }", 1, 247, "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToAminoAcids(64, long=0, paste=T); }", 1, 247, "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToAminoAcids(64, long=F, paste=F); }", 1, 247, "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToAminoAcids(64, long=T, paste=F); }", 1, 247, "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToAminoAcids(64, long=0, paste=F); }", 1, 247, "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToAminoAcids(c(0,-1), long=F, paste=T); }", 1, 247, "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToAminoAcids(c(0,-1), long=T, paste=T); }", 1, 247, "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToAminoAcids(c(0,-1), long=0, paste=T); }", 1, 247, "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToAminoAcids(c(0,-1), long=F, paste=F); }", 1, 247, "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToAminoAcids(c(0,-1), long=T, paste=F); }", 1, 247, "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToAminoAcids(c(0,-1), long=0, paste=F); }", 1, 247, "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToAminoAcids(c(0,64), long=F, paste=T); }", 1, 247, "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToAminoAcids(c(0,64), long=T, paste=T); }", 1, 247, "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToAminoAcids(c(0,64), long=0, paste=T); }", 1, 247, "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToAminoAcids(c(0,64), long=F, paste=F); }", 1, 247, "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToAminoAcids(c(0,64), long=T, paste=F); }", 1, 247, "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToAminoAcids(c(0,64), long=0, paste=F); }", 1, 247, "requires codons to be", __LINE__);
	
	// mm16To256()
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { mm16To256(rep(0.0,15)); }", 1, 247, "to be of length 16", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { mm16To256(rep(0.0,16)); }", 1, 247, "to be a 4x4 matrix", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(mm16To256(matrix(rep(0.0,16), ncol=4)), matrix(rep(0.0,256),ncol=4))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(mm16To256(matrix(rep(0.25,16), ncol=4)), matrix(rep(0.25,256),ncol=4))) stop(); }", __LINE__);
	
	// mmJukesCantor()
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { mmJukesCantor(-0.1); }", 1, 247, "requires alpha >= 0.0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { mmJukesCantor(0.35); }", 1, 247, "requires 3 * alpha <= 1.0", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(mmJukesCantor(0.0), matrix(rep(0.0,16),ncol=4))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(mmJukesCantor(0.25), matrix(c(0.0, 0.25, 0.25, 0.25, 0.25, 0.0, 0.25, 0.25, 0.25, 0.25, 0.0, 0.25, 0.25, 0.25, 0.25, 0.0),ncol=4))) stop(); }", __LINE__);
	
	// mmKimura()
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { mmKimura(-0.1, 0.5); }", 1, 247, "requires alpha to be in", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { mmKimura(1.1, 0.5); }", 1, 247, "requires alpha to be in", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { mmKimura(0.5, -0.1); }", 1, 247, "requires beta to be in", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { mmKimura(0.5, 1.1); }", 1, 247, "requires beta to be in", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(mmKimura(0.0, 0.0), matrix(rep(0.0,16),ncol=4))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(mmKimura(0.5, 0.25), matrix(c(0.0, 0.25, 0.5, 0.25, 0.25, 0.0, 0.25, 0.5, 0.5, 0.25, 0.0, 0.25, 0.25, 0.5, 0.25, 0.0),ncol=4))) stop(); }", __LINE__);
	
	// nucleotideCounts()
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideCounts(string(0)), c(0,0,0,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideCounts(integer(0)), c(0,0,0,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideCounts('A'), c(1,0,0,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideCounts('C'), c(0,1,0,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideCounts('G'), c(0,0,1,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideCounts('T'), c(0,0,0,1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideCounts(0), c(1,0,0,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideCounts(1), c(0,1,0,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideCounts(2), c(0,0,1,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideCounts(3), c(0,0,0,1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideCounts('ACGT'), c(1,1,1,1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideCounts(c('A','C','G','T')), c(1,1,1,1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideCounts(c(0,1,2,3)), c(1,1,1,1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideCounts('AACACGATCG'), c(4,3,2,1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideCounts(c('A','A','C','A','C','G','A','T','C','G')), c(4,3,2,1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideCounts(c(0,0,1,0,1,2,0,3,1,2)), c(4,3,2,1))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotideCounts('ADA'); }", 1, 247, "requires string sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotideCounts(c('A','D','A')); }", 1, 247, "requires string sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotideCounts(c(0,-1,0)); }", 1, 247, "requires integer sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotideCounts(c(0,4,0)); }", 1, 247, "requires integer sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotideCounts('AAAADA'); }", 1, 247, "requires string sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotideCounts(c('A','A','A','A','D','A')); }", 1, 247, "requires string sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotideCounts(c(0,0,0,0,-1,0)); }", 1, 247, "requires integer sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotideCounts(c(0,0,0,0,4,0)); }", 1, 247, "requires integer sequence values", __LINE__);
	
	// nucleotideFrequencies()
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (all(isNAN(nucleotideFrequencies(string(0))))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (all(isNAN(nucleotideFrequencies(integer(0))))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideFrequencies('A'), c(1.0,0,0,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideFrequencies('C'), c(0,1.0,0,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideFrequencies('G'), c(0,0,1.0,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideFrequencies('T'), c(0,0,0,1.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideFrequencies(0), c(1.0,0,0,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideFrequencies(1), c(0,1.0,0,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideFrequencies(2), c(0,0,1.0,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideFrequencies(3), c(0,0,0,1.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideFrequencies('ACGT'), c(0.25,0.25,0.25,0.25))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideFrequencies(c('A','C','G','T')), c(0.25,0.25,0.25,0.25))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideFrequencies(c(0,1,2,3)), c(0.25,0.25,0.25,0.25))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideFrequencies('AACACGATCG'), c(0.4,0.3,0.2,0.1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideFrequencies(c('A','A','C','A','C','G','A','T','C','G')), c(0.4,0.3,0.2,0.1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideFrequencies(c(0,0,1,0,1,2,0,3,1,2)), c(0.4,0.3,0.2,0.1))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotideFrequencies('ADA'); }", 1, 247, "requires string sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotideFrequencies(c('A','D','A')); }", 1, 247, "requires string sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotideFrequencies(c(0,-1,0)); }", 1, 247, "requires integer sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotideFrequencies(c(0,4,0)); }", 1, 247, "requires integer sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotideFrequencies('AAAADA'); }", 1, 247, "requires string sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotideFrequencies(c('A','A','A','A','D','A')); }", 1, 247, "requires string sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotideFrequencies(c(0,0,0,0,-1,0)); }", 1, 247, "requires integer sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotideFrequencies(c(0,0,0,0,4,0)); }", 1, 247, "requires integer sequence values", __LINE__);
	
	// randomNucleotides()
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(randomNucleotides(0, format='string'), string(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(randomNucleotides(0, format='char'), string(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(randomNucleotides(0, format='integer'), integer(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(1, format='string'), 'A')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(1); if (identical(randomNucleotides(1, format='char'), 'T')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(2); if (identical(randomNucleotides(1, format='integer'), 2)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(3); if (identical(randomNucleotides(10, format='string'), 'ACACATATGA')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(4); if (identical(randomNucleotides(10, format='char'), c('A','G','C','A','C','T','C','G','C','T'))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(5); if (identical(randomNucleotides(10, format='integer'), c(2,2,0,1,2,2,0,2,1,3))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(1, basis=c(1.0,0,0,0), format='string'), 'A')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(1, basis=c(1.0,0,0,0), format='char'), 'A')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(1, basis=c(1.0,0,0,0), format='integer'), 0)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(1, basis=c(0,1.0,0,0), format='string'), 'C')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(1, basis=c(0,1.0,0,0), format='char'), 'C')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(1, basis=c(0,1.0,0,0), format='integer'), 1)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(1, basis=c(0,0,1.0,0), format='string'), 'G')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(1, basis=c(0,0,1.0,0), format='char'), 'G')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(1, basis=c(0,0,1.0,0), format='integer'), 2)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(1, basis=c(0,0,0,1.0), format='string'), 'T')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(1, basis=c(0,0,0,1.0), format='char'), 'T')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(1, basis=c(0,0,0,1.0), format='integer'), 3)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(10, basis=c(1.0,0,0,0), format='string'), 'AAAAAAAAAA')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(10, basis=c(1.0,0,0,0), format='char'), rep('A',10))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(10, basis=c(1.0,0,0,0), format='integer'), rep(0,10))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(10, basis=c(0,1.0,0,0), format='string'), 'CCCCCCCCCC')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(10, basis=c(0,1.0,0,0), format='char'), rep('C',10))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(10, basis=c(0,1.0,0,0), format='integer'), rep(1,10))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(10, basis=c(0,0,1.0,0), format='string'), 'GGGGGGGGGG')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(10, basis=c(0,0,1.0,0), format='char'), rep('G',10))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(10, basis=c(0,0,1.0,0), format='integer'), rep(2,10))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(10, basis=c(0,0,0,1.0), format='string'), 'TTTTTTTTTT')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(10, basis=c(0,0,0,1.0), format='char'), rep('T',10))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(10, basis=c(0,0,0,1.0), format='integer'), rep(3,10))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(100, basis=c(10.0,1.0,2.0,3.0), format='string'), 'ATAAAAAAAGAAATAAACTATGAATATCATAAAATACAAAATAAAATAATTTGTAAGAGTAAATTATTAGTATGAATCTAACATAATAAAAAATAATATA')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(100, basis=c(10.0,1.0,2.0,3.0), format='char'), c('A','T','A','A','A','A','A','A','A','G','A','A','A','T','A','A','A','C','T','A','T','G','A','A','T','A','T','C','A','T','A','A','A','A','T','A','C','A','A','A','A','T','A','A','A','A','T','A','A','T','T','T','G','T','A','A','G','A','G','T','A','A','A','T','T','A','T','T','A','G','T','A','T','G','A','A','T','C','T','A','A','C','A','T','A','A','T','A','A','A','A','A','A','T','A','A','T','A','T','A'))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(100, basis=c(10.0,1.0,2.0,3.0), format='integer'), c(0,3,0,0,0,0,0,0,0,2,0,0,0,3,0,0,0,1,3,0,3,2,0,0,3,0,3,1,0,3,0,0,0,0,3,0,1,0,0,0,0,3,0,0,0,0,3,0,0,3,3,3,2,3,0,0,2,0,2,3,0,0,0,3,3,0,3,3,0,2,3,0,3,2,0,0,3,1,3,0,0,1,0,3,0,0,3,0,0,0,0,0,0,3,0,0,3,0,3,0))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { randomNucleotides(-1); }", 1, 247, "requires length to be in [0, 2e9]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { randomNucleotides(0, basis=3.0); }", 1, 247, "requires basis to be either", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { randomNucleotides(0, basis=c(0.0,0.0,0.0,0.0)); }", 1, 247, "requires at least one basis value to be > 0.0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { randomNucleotides(0, basis=c(0.0,0.0,0.2,-0.1)); }", 1, 247, "requires basis values to be finite and >= 0.0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { randomNucleotides(0, basis=c(0.0,0.0,0.2,INF)); }", 1, 247, "requires basis values to be finite and >= 0.0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { randomNucleotides(0, basis=c(0.0,0.0,0.2,NAN)); }", 1, 247, "requires basis values to be finite and >= 0.0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { randomNucleotides(0, basis=c(0.0,0.0,0.2,0.0), format='foo'); }", 1, 247, "requires a format of", __LINE__);
	
	// codonsToNucleotides()
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(integer(0), format='string'), '')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(integer(0), format='char'), string(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(integer(0), format='integer'), integer(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(0, format='string'), 'AAA')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(1, format='string'), 'AAC')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(2, format='string'), 'AAG')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(3, format='string'), 'AAT')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(4, format='string'), 'ACA')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(8, format='string'), 'AGA')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(12, format='string'), 'ATA')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(16, format='string'), 'CAA')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(32, format='string'), 'GAA')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(48, format='string'), 'TAA')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(63, format='string'), 'TTT')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(0, format='char'), c('A','A','A'))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(1, format='char'), c('A','A','C'))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(2, format='char'), c('A','A','G'))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(3, format='char'), c('A','A','T'))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(4, format='char'), c('A','C','A'))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(8, format='char'), c('A','G','A'))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(12, format='char'), c('A','T','A'))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(16, format='char'), c('C','A','A'))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(32, format='char'), c('G','A','A'))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(48, format='char'), c('T','A','A'))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(63, format='char'), c('T','T','T'))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(0, format='integer'), c(0,0,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(1, format='integer'), c(0,0,1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(2, format='integer'), c(0,0,2))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(3, format='integer'), c(0,0,3))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(4, format='integer'), c(0,1,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(8, format='integer'), c(0,2,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(12, format='integer'), c(0,3,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(16, format='integer'), c(1,0,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(32, format='integer'), c(2,0,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(48, format='integer'), c(3,0,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(63, format='integer'), c(3,3,3))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(0:5, format='string'), 'AAAAACAAGAATACAACC')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(0:5, format='char'), c('A','A','A','A','A','C','A','A','G','A','A','T','A','C','A','A','C','C'))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(0:5, format='integer'), c(0,0,0,0,0,1,0,0,2,0,0,3,0,1,0,0,1,1))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToNucleotides(-1, format='string'); }", 1, 247, "requires codon values to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToNucleotides(-1, format='char'); }", 1, 247, "requires codon values to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToNucleotides(-1, format='integer'); }", 1, 247, "requires codon values to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToNucleotides(64, format='string'); }", 1, 247, "requires codon values to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToNucleotides(64, format='char'); }", 1, 247, "requires codon values to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToNucleotides(64, format='integer'); }", 1, 247, "requires codon values to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToNucleotides(0, format='foo'); }", 1, 247, "requires a format of", __LINE__);
}

void _RunNucleotideMethodTests(void)
{
	// Test that various nucleotide-based APIs behave as they ought to when used in a non-nucleotide model
	SLiMAssertScriptRaise("initialize() { initializeAncestralNucleotides('ACGT'); } ", 1, 15, "only be called in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeHotspotMap(1.0); } ", 1, 15, "only be called in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationTypeNuc(1, 0.5, 'f', 0.0); } ", 1, 15, "only be called in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0, mutationMatrix=mmJukesCantor(1e-7)); } ", 1, 60, "to be NULL in non-nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.chromosome.hotspotEndPositions; }", 1, 262, "only defined in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.chromosome.hotspotEndPositionsM; }", 1, 262, "only defined in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.chromosome.hotspotEndPositionsF; }", 1, 262, "only defined in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.chromosome.hotspotMultipliers; }", 1, 262, "only defined in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.chromosome.hotspotMultipliersM; }", 1, 262, "only defined in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.chromosome.hotspotMultipliersF; }", 1, 262, "only defined in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.chromosome.ancestralNucleotides(); }", 1, 262, "only be called in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.chromosome.setHotspotMap(1.0); }", 1, 262, "only be called in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.genomes[0].nucleotides(); }", 1, 261, "only be called in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { g1.mutationMatrix; }", 1, 250, "only defined in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { g1.setMutationMatrix(mmJukesCantor(1e-7)); }", 1, 250, "only be called in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; mut.nucleotide; }", 1, 276, "only defined for nucleotide-based mutations", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; mut.nucleotideValue; }", 1, 276, "only defined for nucleotide-based mutations", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_fixmut_p1 + "30 { sub = sim.substitutions[0]; sub.nucleotide; }", 1, 356, "only defined for nucleotide-based mutations", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_fixmut_p1 + "30 { sub = sim.substitutions[0]; sub.nucleotideValue; }", 1, 356, "only defined for nucleotide-based mutations", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (sim.nucleotideBased == F) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (m1.nucleotideBased == F) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(m1, 5000, nucleotide='A'); stop(); }", 1, 278, "NULL in non-nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(m1, 0.0, 5000, nucleotide='A'); stop(); }", 1, 278, "NULL in non-nucleotide-based models", __LINE__);
	
	// Test that some APIs are correctly disabled in nucleotide-based models
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(nucleotideBased=T); initializeMutationRate(1e-7); } ", 1, 57, "may not be called in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(nucleotideBased=T); initializeMutationTypeNuc(1, 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); }", 1, 102, "non-NULL in nucleotide-based models", __LINE__);
	
	std::string nuc_model_start("initialize() { initializeSLiMOptions(nucleotideBased=T); ");
	std::string nuc_model_init(nuc_model_start + "initializeAncestralNucleotides(randomNucleotides(1e2)); initializeMutationTypeNuc(1, 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0, mmJukesCantor(1e-7)); initializeGenomicElement(g1, 0, 1e2-1); initializeRecombinationRate(1e-8); } ");
	
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.chromosome.mutationEndPositions; }", 1, 320, "not defined in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.chromosome.mutationEndPositionsF; }", 1, 320, "not defined in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.chromosome.mutationEndPositionsM; }", 1, 320, "not defined in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.chromosome.mutationRates; }", 1, 320, "not defined in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.chromosome.mutationRatesF; }", 1, 320, "not defined in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.chromosome.mutationRatesM; }", 1, 320, "not defined in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.chromosome.overallMutationRate; }", 1, 320, "not defined in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.chromosome.overallMutationRateF; }", 1, 320, "not defined in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.chromosome.overallMutationRateM; }", 1, 320, "not defined in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.chromosome.setMutationRate(1e-7); }", 1, 320, "may not be called in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.addSubpop('p1', 10); gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(m1, 50); }", 1, 361, "requires nucleotide to be non-NULL", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.addSubpop('p1', 10); gen = p1.genomes[0]; mut = gen.addNewMutation(m1, 0.0, 50); }", 1, 361, "requires nucleotide to be non-NULL", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { m1.mutationStackGroup = 2; }", 1, 327, "for nucleotide-based mutation types", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { m1.mutationStackPolicy = 'f'; }", 1, 328, "for nucleotide-based mutation types", __LINE__);
	
	// initializeAncestralNucleotides()
	SLiMAssertScriptRaise(nuc_model_start + "initializeAncestralNucleotides(integer(0)); } ", 1, 57, "requires a sequence of length >= 1", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeAncestralNucleotides(-1); } ", 1, 57, "integer nucleotide values", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeAncestralNucleotides(4); } ", 1, 57, "integer nucleotide values", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeAncestralNucleotides('AACAGTACGTTACAGGTACAD'); } ", 1, 57, "could not be opened or does not exist", __LINE__);	// file path!
	SLiMAssertScriptRaise(nuc_model_start + "initializeAncestralNucleotides(c(0,-1,2)); } ", 1, 57, "integer nucleotide values", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeAncestralNucleotides(c(0,4,2)); } ", 1, 57, "integer nucleotide values", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeAncestralNucleotides(c('A','D','T')); } ", 1, 57, "string nucleotide values", __LINE__);
	SLiMAssertScriptStop(nuc_model_start + "if (initializeAncestralNucleotides('A') == 1) stop(); } ", __LINE__);
	SLiMAssertScriptStop(nuc_model_start + "if (initializeAncestralNucleotides(0) == 1) stop(); } ", __LINE__);
	SLiMAssertScriptStop(nuc_model_start + "if (initializeAncestralNucleotides('ACGTACGT') == 8) stop(); } ", __LINE__);
	SLiMAssertScriptStop(nuc_model_start + "if (initializeAncestralNucleotides(c(0,1,2,3,0,1,2,3)) == 8) stop(); } ", __LINE__);
	
	// initializeHotspotMap()
	SLiMAssertScriptRaise(nuc_model_start + "initializeHotspotMap(float(0)); } ", 1, 57, "to be a singleton", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeHotspotMap(1.0, integer(0)); } ", 1, 57, "of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeHotspotMap(float(0), 1e2-1); } ", 1, 57, "of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeHotspotMap(float(0), integer(0)); } ", 1, 57, "of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeHotspotMap(1.0, sex='A'); } ", 1, 57, "requested sex \"A\" unsupported", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeHotspotMap(1.0, sex='M'); } ", 1, 57, "supplied in non-sexual simulation", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeSex('A'); initializeHotspotMap(1.0, sex='A'); } ", 1, 77, "requested sex \"A\" unsupported", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeSex('A'); initializeHotspotMap(1.0, sex='M'); initializeHotspotMap(1.0, sex='F'); initializeHotspotMap(1.0, sex='M'); } ", 1, 149, "may be called only once", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeHotspotMap(1.0); initializeHotspotMap(1.0); } ", 1, 84, "may be called only once", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeHotspotMap(c(1.0, 1.2)); } ", 1, 57, "multipliers to be a singleton", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeHotspotMap(-0.1); } ", 1, 57, "multipliers to be >= 0", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeHotspotMap(0.1, c(10, 20)); } ", 1, 57, "of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeHotspotMap(c(1.0, 1.2), 10); } ", 1, 57, "of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeHotspotMap(c(1.0, 1.2), c(20, 10)); } ", 1, 57, "in strictly ascending order", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeHotspotMap(c(1.0, -1.2), c(10, 20)); } ", 1, 57, "multipliers to be >= 0", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "initialize() { initializeHotspotMap(c(1.0, 1.2), c(10, 20)); } 1 {}", -1, -1, "do not cover the full chromosome", __LINE__);
	SLiMAssertScriptStop(nuc_model_start + "initializeHotspotMap(2.0); stop(); } ", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "initialize() { initializeHotspotMap(c(1.0, 1.2), c(10, 1e2-1)); } 1 { stop(); } ", __LINE__);
	
	// initializeMutationTypeNuc() (copied from initializeMutationType())
	SLiMAssertScriptStop(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'f', 0.0); stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_start + "initializeMutationTypeNuc(1, 0.5, 'f', 0.0); stop(); }", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc(-1, 0.5, 'f', 0.0); stop(); }", 1, 57, "identifier value is out of range", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('p2', 0.5, 'f', 0.0); stop(); }", 1, 57, "identifier prefix \"m\" was expected", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('mm1', 0.5, 'f', 0.0); stop(); }", 1, 57, "must be a simple integer", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'f'); stop(); }", 1, 57, "requires exactly 1 DFE parameter", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'f', 0.0, 0.0); stop(); }", 1, 57, "requires exactly 1 DFE parameter", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'g', 0.0); stop(); }", 1, 57, "requires exactly 2 DFE parameters", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'e', 0.0, 0.0); stop(); }", 1, 57, "requires exactly 1 DFE parameter", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'n', 0.0); stop(); }", 1, 57, "requires exactly 2 DFE parameters", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'w', 0.0); stop(); }", 1, 57, "requires exactly 2 DFE parameters", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'f', 'foo'); stop(); }", 1, 57, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'g', 'foo', 0.0); stop(); }", 1, 57, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'g', 0.0, 'foo'); stop(); }", 1, 57, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'e', 'foo'); stop(); }", 1, 57, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'n', 'foo', 0.0); stop(); }", 1, 57, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'n', 0.0, 'foo'); stop(); }", 1, 57, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'w', 'foo', 0.0); stop(); }", 1, 57, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'w', 0.0, 'foo'); stop(); }", 1, 57, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'f', '1'); stop(); }", 1, 57, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'g', '1', 0.0); stop(); }", 1, 57, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'g', 0.0, '1'); stop(); }", 1, 57, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'e', '1'); stop(); }", 1, 57, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'n', '1', 0.0); stop(); }", 1, 57, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'n', 0.0, '1'); stop(); }", 1, 57, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'w', '1', 0.0); stop(); }", 1, 57, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'w', 0.0, '1'); stop(); }", 1, 57, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'x', 0.0); stop(); }", 1, 57, "must be \"f\", \"g\", \"e\", \"n\", \"w\", or \"s\"", __LINE__);
	SLiMAssertScriptStop(nuc_model_start + "x = initializeMutationTypeNuc('m7', 0.5, 'f', 0.0); if (x == m7) stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_start + "x = initializeMutationTypeNuc(7, 0.5, 'f', 0.0); if (x == m7) stop(); }", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "m7 = 15; initializeMutationTypeNuc(7, 0.5, 'f', 0.0); stop(); }", 1, 66, "already defined", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'f', 0.0); initializeMutationTypeNuc('m1', 0.5, 'f', 0.0); stop(); }", 1, 105, "already defined", __LINE__);
	
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'g', 3.1, 0.0); stop(); }", 1, 57, "must have a shape parameter > 0", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'g', 3.1, -1.0); stop(); }", 1, 57, "must have a shape parameter > 0", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'n', 3.1, -1.0); stop(); }", 1, 57, "must have a standard deviation parameter >= 0", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'w', 0.0, 7.5); stop(); }", 1, 57, "must have a scale parameter > 0", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'w', -1.0, 7.5); stop(); }", 1, 57, "must have a scale parameter > 0", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'w', 3.1, 0.0); stop(); }", 1, 57, "must have a shape parameter > 0", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'w', 3.1, -7.5); stop(); }", 1, 57, "must have a shape parameter > 0", __LINE__);
	
	// initializeGenomicElementType()
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0, mutationMatrix=mmJukesCantor(1e-7)); } ", 1, 102, "requires all mutation types for", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "initialize() { initializeGenomicElementType('g2', m1, 1.0); } ", 1, 316, "non-NULL in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "initialize() { initializeGenomicElementType('g2', m1, 1.0, float(0)); } ", 1, 316, "a 4x4 or 64x4 matrix", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "initialize() { initializeGenomicElementType('g2', m1, 1.0, rep(1.0, 16)); } ", 1, 316, "a 4x4 or 64x4 matrix", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "initialize() { initializeGenomicElementType('g2', m1, 1.0, rep(1.0, 256)); } ", 1, 316, "a 4x4 or 64x4 matrix", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "initialize() { initializeGenomicElementType('g2', m1, 1.0, matrix(rep(1.0, 16), ncol=2)); } ", 1, 316, "a 4x4 or 64x4 matrix", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "initialize() { initializeGenomicElementType('g2', m1, 1.0, matrix(rep(1.0, 256), ncol=2)); } ", 1, 316, "a 4x4 or 64x4 matrix", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "initialize() { initializeGenomicElementType('g2', m1, 1.0, matrix(rep(1.0, 16), ncol=4)); } ", 1, 316, "must contain 0.0 for all", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "initialize() { initializeGenomicElementType('g2', m1, 1.0, matrix(rep(1.0, 256), ncol=4)); } ", 1, 316, "must contain 0.0 for all", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "initialize() { initializeGenomicElementType('g2', m1, 1.0, mmJukesCantor(0.25)*2); } ", 1, 316, "requires the sum of each mutation matrix row", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "initialize() { initializeGenomicElementType('g2', m1, 1.0, mm16To256(mmJukesCantor(0.25))*2); } ", 1, 316, "requires the sum of each mutation matrix row", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "initialize() { mm = mmJukesCantor(0.25); mm[0,1] = -0.1; initializeGenomicElementType('g2', m1, 1.0, mm); } ", 1, 358, "to be finite and >= 0.0", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "initialize() { mm = mm16To256(mmJukesCantor(0.25)); mm[0,1] = -0.1; initializeGenomicElementType('g2', m1, 1.0, mm); } ", 1, 369, "to be finite and >= 0.0", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "initialize() { initializeGenomicElementType('g2', m1, 1.0, mmJukesCantor(0.25)); stop(); } ", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "initialize() { initializeGenomicElementType('g2', m1, 1.0, mm16To256(mmJukesCantor(0.25))); stop(); } ", __LINE__);
	
	// hotspotEndPositions, hotspotEndPositionsM, hotspotEndPositionsF
	SLiMAssertScriptStop(nuc_model_init + "1 { if (sim.chromosome.hotspotEndPositions == 1e2-1) stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "initialize() { initializeHotspotMap(2.0); } 1 { if (sim.chromosome.hotspotEndPositions == 1e2-1) stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "initialize() { initializeHotspotMap(c(1.0, 1.2), c(10, 1e2-1)); } 1 { if (identical(sim.chromosome.hotspotEndPositions, c(10, 1e2-1))) stop(); }", __LINE__);
	
	// hotspotMultipliers, hotspotMultipliersM, hotspotMultipliersF
	SLiMAssertScriptStop(nuc_model_init + "1 { if (sim.chromosome.hotspotMultipliers == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "initialize() { initializeHotspotMap(2.0); } 1 { if (sim.chromosome.hotspotMultipliers == 2.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "initialize() { initializeHotspotMap(c(1.0, 1.2), c(10, 1e2-1)); } 1 { if (identical(sim.chromosome.hotspotMultipliers, c(1.0, 1.2))) stop(); }", __LINE__);
	
	// ancestralNucleotides()
	std::string ances_setup_string = "initialize() { initializeSLiMOptions(nucleotideBased=T); defineConstant('AS', randomNucleotides(1e2, format='string')); initializeAncestralNucleotides(AS); initializeMutationTypeNuc(1, 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0, mmJukesCantor(1e-7)); initializeGenomicElement(g1, 0, 1e2-1); initializeRecombinationRate(1e-8); } ";
	std::string ances_setup_char = "initialize() { initializeSLiMOptions(nucleotideBased=T); defineConstant('AS', randomNucleotides(1e2, format='char')); initializeAncestralNucleotides(AS); initializeMutationTypeNuc(1, 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0, mmJukesCantor(1e-7)); initializeGenomicElement(g1, 0, 1e2-1); initializeRecombinationRate(1e-8); } ";
	std::string ances_setup_integer = "initialize() { initializeSLiMOptions(nucleotideBased=T); defineConstant('AS', randomNucleotides(1e2, format='integer')); initializeAncestralNucleotides(AS); initializeMutationTypeNuc(1, 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0, mmJukesCantor(1e-7)); initializeGenomicElement(g1, 0, 1e2-1); initializeRecombinationRate(1e-8); } ";
	
	SLiMAssertScriptStop(ances_setup_string + "1 { if (identical(sim.chromosome.ancestralNucleotides(format='string'), AS)) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_string + "1 { if (identical(sim.chromosome.ancestralNucleotides(end=49, format='string'), substr(AS, 0, 49))) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_string + "1 { if (identical(sim.chromosome.ancestralNucleotides(start=50, format='string'), substr(AS, 50, 99))) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_string + "1 { if (identical(sim.chromosome.ancestralNucleotides(start=25, end=69, format='string'), substr(AS, 25, 69))) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_string + "1 { if (identical(sim.chromosome.ancestralNucleotides(start=10, end=39, format='codon'), nucleotidesToCodons(substr(AS, 10, 39)))) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(ances_setup_char + "1 { if (identical(sim.chromosome.ancestralNucleotides(format='char'), AS)) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_char + "1 { if (identical(sim.chromosome.ancestralNucleotides(end=49, format='char'), AS[0:49])) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_char + "1 { if (identical(sim.chromosome.ancestralNucleotides(start=50, format='char'), AS[50:99])) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_char + "1 { if (identical(sim.chromosome.ancestralNucleotides(start=25, end=69, format='char'), AS[25:69])) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_char + "1 { if (identical(sim.chromosome.ancestralNucleotides(start=10, end=39, format='codon'), nucleotidesToCodons(AS[10:39]))) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(ances_setup_integer + "1 { if (identical(sim.chromosome.ancestralNucleotides(format='integer'), AS)) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_integer + "1 { if (identical(sim.chromosome.ancestralNucleotides(end=49, format='integer'), AS[0:49])) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_integer + "1 { if (identical(sim.chromosome.ancestralNucleotides(start=50, format='integer'), AS[50:99])) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_integer + "1 { if (identical(sim.chromosome.ancestralNucleotides(start=25, end=69, format='integer'), AS[25:69])) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_integer + "1 { if (identical(sim.chromosome.ancestralNucleotides(start=10, end=39, format='codon'), nucleotidesToCodons(AS[10:39]))) stop(); }", __LINE__);
	
	SLiMAssertScriptRaise(ances_setup_integer + "1 { sim.chromosome.ancestralNucleotides(start=-1, end=50, format='integer'); }", 1, 364, "within the chromosome's extent", __LINE__);
	SLiMAssertScriptRaise(ances_setup_integer + "1 { sim.chromosome.ancestralNucleotides(start=50, end=100, format='integer'); }", 1, 364, "within the chromosome's extent", __LINE__);
	SLiMAssertScriptRaise(ances_setup_integer + "1 { sim.chromosome.ancestralNucleotides(start=75, end=25, format='integer'); }", 1, 364, "start must be <= end", __LINE__);
	SLiMAssertScriptRaise(ances_setup_integer + "1 { sim.chromosome.ancestralNucleotides(format='foo'); }", 1, 364, "format must be either", __LINE__);
	
	// setHotspotMap()
	std::string nuc_w_hotspot = nuc_model_init + "initialize() { initializeHotspotMap(c(1.0, 1.2), c(10, 1e2-1)); } ";
	
	SLiMAssertScriptRaise(nuc_w_hotspot + "1 { sim.chromosome.setHotspotMap(float(0)); }", 1, 386, "to be a singleton if", __LINE__);
	SLiMAssertScriptRaise(nuc_w_hotspot + "1 { sim.chromosome.setHotspotMap(1.0, integer(0)); }", 1, 386, "equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(nuc_w_hotspot + "1 { sim.chromosome.setHotspotMap(float(0), 1e2-1); }", 1, 386, "equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(nuc_w_hotspot + "1 { sim.chromosome.setHotspotMap(float(0), integer(0)); }", 1, 386, "equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(nuc_w_hotspot + "1 { sim.chromosome.setHotspotMap(1.0, sex='A'); }", 1, 386, "sex \"A\" unsupported", __LINE__);
	SLiMAssertScriptRaise(nuc_w_hotspot + "1 { sim.chromosome.setHotspotMap(1.0, sex='M'); }", 1, 386, "original configuration", __LINE__);
	SLiMAssertScriptRaise(nuc_w_hotspot + "1 { sim.chromosome.setHotspotMap(c(1.0, 1.2)); }", 1, 386, "to be a singleton if", __LINE__);
	SLiMAssertScriptRaise(nuc_w_hotspot + "1 { sim.chromosome.setHotspotMap(-0.1); }", 1, 386, "multipliers must be >= 0", __LINE__);
	SLiMAssertScriptRaise(nuc_w_hotspot + "1 { sim.chromosome.setHotspotMap(0.1, c(10, 20)); }", 1, 386, "equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(nuc_w_hotspot + "1 { sim.chromosome.setHotspotMap(c(1.0, 1.2), 10); }", 1, 386, "equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(nuc_w_hotspot + "1 { sim.chromosome.setHotspotMap(c(1.0, 1.2), c(20, 10)); }", 1, 386, "strictly ascending order", __LINE__);
	SLiMAssertScriptRaise(nuc_w_hotspot + "1 { sim.chromosome.setHotspotMap(c(1.0, -1.2), c(10, 20)); }", 1, 386, "multipliers must be >= 0", __LINE__);
	SLiMAssertScriptRaise(nuc_w_hotspot + "1 { sim.chromosome.setHotspotMap(c(1.0, 1.2), c(10, 20)); }", 1, 386, "must end at the last position", __LINE__);
	SLiMAssertScriptStop(nuc_w_hotspot + "1 { sim.chromosome.setHotspotMap(1.2); stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_w_hotspot + "1 { sim.chromosome.setHotspotMap(c(1.0, 1.2), c(10, 1e2-1)); stop(); }", __LINE__);
	
	// nucleotides()
	SLiMAssertScriptStop(ances_setup_string + "1 { sim.addSubpop(1, 10); if (identical(p1.genomes[0].nucleotides(format='string'), AS)) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_string + "1 { sim.addSubpop(1, 10); if (identical(p1.genomes[0].nucleotides(end=49, format='string'), substr(AS, 0, 49))) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_string + "1 { sim.addSubpop(1, 10); if (identical(p1.genomes[0].nucleotides(start=50, format='string'), substr(AS, 50, 99))) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_string + "1 { sim.addSubpop(1, 10); if (identical(p1.genomes[0].nucleotides(start=25, end=69, format='string'), substr(AS, 25, 69))) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_string + "1 { sim.addSubpop(1, 10); if (identical(p1.genomes[0].nucleotides(start=10, end=39, format='codon'), nucleotidesToCodons(substr(AS, 10, 39)))) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(ances_setup_char + "1 { sim.addSubpop(1, 10); if (identical(p1.genomes[0].nucleotides(format='char'), AS)) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_char + "1 { sim.addSubpop(1, 10); if (identical(p1.genomes[0].nucleotides(end=49, format='char'), AS[0:49])) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_char + "1 { sim.addSubpop(1, 10); if (identical(p1.genomes[0].nucleotides(start=50, format='char'), AS[50:99])) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_char + "1 { sim.addSubpop(1, 10); if (identical(p1.genomes[0].nucleotides(start=25, end=69, format='char'), AS[25:69])) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_char + "1 { sim.addSubpop(1, 10); if (identical(p1.genomes[0].nucleotides(start=10, end=39, format='codon'), nucleotidesToCodons(AS[10:39]))) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(ances_setup_integer + "1 { sim.addSubpop(1, 10); if (identical(p1.genomes[0].nucleotides(format='integer'), AS)) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_integer + "1 { sim.addSubpop(1, 10); if (identical(p1.genomes[0].nucleotides(end=49, format='integer'), AS[0:49])) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_integer + "1 { sim.addSubpop(1, 10); if (identical(p1.genomes[0].nucleotides(start=50, format='integer'), AS[50:99])) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_integer + "1 { sim.addSubpop(1, 10); if (identical(p1.genomes[0].nucleotides(start=25, end=69, format='integer'), AS[25:69])) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_integer + "1 { sim.addSubpop(1, 10); if (identical(p1.genomes[0].nucleotides(start=10, end=39, format='codon'), nucleotidesToCodons(AS[10:39]))) stop(); }", __LINE__);
	
	SLiMAssertScriptRaise(ances_setup_integer + "1 { sim.addSubpop(1, 10); p1.genomes[0].nucleotides(start=-1, end=50, format='integer'); }", 1, 385, "within the chromosome's extent", __LINE__);
	SLiMAssertScriptRaise(ances_setup_integer + "1 { sim.addSubpop(1, 10); p1.genomes[0].nucleotides(start=50, end=100, format='integer'); }", 1, 385, "within the chromosome's extent", __LINE__);
	SLiMAssertScriptRaise(ances_setup_integer + "1 { sim.addSubpop(1, 10); p1.genomes[0].nucleotides(start=75, end=25, format='integer'); }", 1, 385, "start must be <= end", __LINE__);
	SLiMAssertScriptRaise(ances_setup_integer + "1 { sim.addSubpop(1, 10); p1.genomes[0].nucleotides(format='foo'); }", 1, 385, "format must be either", __LINE__);
	
	// mutationMatrix()
	SLiMAssertScriptStop(nuc_model_init + "1 { if (identical(g1.mutationMatrix, mmJukesCantor(1e-7))) stop(); }", __LINE__);
	
	// setMutationMatrix()
	SLiMAssertScriptRaise(nuc_model_init + "1 { g1.setMutationMatrix(NULL); } ", 1, 308, "cannot be type NULL", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { g1.setMutationMatrix(float(0)); } ", 1, 308, "a 4x4 or 64x4 matrix", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { g1.setMutationMatrix(rep(1.0, 16)); } ", 1, 308, "a 4x4 or 64x4 matrix", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { g1.setMutationMatrix(rep(1.0, 256)); } ", 1, 308, "a 4x4 or 64x4 matrix", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { g1.setMutationMatrix(matrix(rep(1.0, 16), ncol=2)); } ", 1, 308, "a 4x4 or 64x4 matrix", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { g1.setMutationMatrix(matrix(rep(1.0, 256), ncol=2)); } ", 1, 308, "a 4x4 or 64x4 matrix", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { g1.setMutationMatrix(matrix(rep(1.0, 16), ncol=4)); } ", 1, 308, "must contain 0.0 for all", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { g1.setMutationMatrix(matrix(rep(1.0, 256), ncol=4)); } ", 1, 308, "must contain 0.0 for all", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { g1.setMutationMatrix(mmJukesCantor(0.25)*2); } ", 1, 308, "requires the sum of each mutation matrix row", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { g1.setMutationMatrix(mm16To256(mmJukesCantor(0.25))*2); } ", 1, 308, "requires the sum of each mutation matrix row", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { mm = mmJukesCantor(0.25); mm[0,1] = -0.1; g1.setMutationMatrix(mm); } ", 1, 350, "to be finite and >= 0.0", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { mm = mm16To256(mmJukesCantor(0.25)); mm[0,1] = -0.1; g1.setMutationMatrix(mm); } ", 1, 361, "to be finite and >= 0.0", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "1 { g1.setMutationMatrix(mmJukesCantor(0.25)); stop(); } ", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "1 { g1.setMutationMatrix(mm16To256(mmJukesCantor(0.25))); stop(); } ", __LINE__);
	
	// nucleotide & nucleotideValue
	std::string nuc_highmut("initialize() { initializeSLiMOptions(nucleotideBased=T); initializeAncestralNucleotides(randomNucleotides(1e2)); initializeMutationTypeNuc('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0, mmJukesCantor(1e-2)); initializeGenomicElement(g1, 0, 1e2-1); initializeRecombinationRate(1e-8); } 1 { sim.addSubpop('p1', 10); } ");
	std::string nuc_fixmut("initialize() { initializeSLiMOptions(nucleotideBased=T); initializeAncestralNucleotides(randomNucleotides(1e2)); initializeMutationTypeNuc('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0, mmJukesCantor(1e-2)); initializeGenomicElement(g1, 0, 1e2-1); initializeRecombinationRate(1e-8); } 1 { sim.addSubpop('p1', 10); } 10 { sim.mutations[0].setSelectionCoeff(500.0); sim.recalculateFitness(); } ");
	
	SLiMAssertScriptStop(nuc_highmut + "10 { mut = sim.mutations[0]; mut.nucleotide; stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_highmut + "10 { mut = sim.mutations[0]; mut.nucleotideValue; stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_fixmut + "30 { sub = sim.substitutions[0]; sub.nucleotide; stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_fixmut + "30 { sub = sim.substitutions[0]; sub.nucleotideValue; stop(); }", __LINE__);
	
	// addNewDrawnMutation()
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0].addNewDrawnMutation(m1, 10); }", 1, 341, "nucleotide to be non-NULL", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0].addNewDrawnMutation(m1, 10, nucleotide=NULL); }", 1, 341, "nucleotide to be non-NULL", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0].addNewDrawnMutation(m1, 10, nucleotide='D'); }", 1, 341, "string nucleotide values", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0].addNewDrawnMutation(m1, 10, nucleotide=-1); }", 1, 341, "integer nucleotide values", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0].addNewDrawnMutation(m1, 10, nucleotide=4); }", 1, 341, "integer nucleotide values", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0].addNewDrawnMutation(m1, 10, nucleotide='A'); stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0].addNewDrawnMutation(m1, 10, nucleotide=0); stop(); }", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0:3].addNewDrawnMutation(m1, 10); }", 1, 343, "nucleotide to be non-NULL", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0:3].addNewDrawnMutation(m1, 10, nucleotide=NULL); }", 1, 343, "nucleotide to be non-NULL", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0:3].addNewDrawnMutation(m1, 10, nucleotide=c('A','D','G','C')); }", 1, 343, "string nucleotide values", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0:3].addNewDrawnMutation(m1, 10, nucleotide=c(0,-1,2,3)); }", 1, 343, "integer nucleotide values", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0:3].addNewDrawnMutation(m1, 10, nucleotide=c(0,4,2,3)); }", 1, 343, "integer nucleotide values", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0:3].addNewDrawnMutation(m1, 10, nucleotide=c('A','C','G','T')); stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0:3].addNewDrawnMutation(m1, 10, nucleotide=0:3); stop(); }", __LINE__);
	
	// addNewMutation()
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0].addNewMutation(m1, 0.5, 10); }", 1, 341, "nucleotide to be non-NULL", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0].addNewMutation(m1, 0.5, 10, nucleotide=NULL); }", 1, 341, "nucleotide to be non-NULL", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0].addNewMutation(m1, 0.5, 10, nucleotide='D'); }", 1, 341, "string nucleotide values", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0].addNewMutation(m1, 0.5, 10, nucleotide=-1); }", 1, 341, "integer nucleotide values", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0].addNewMutation(m1, 0.5, 10, nucleotide=4); }", 1, 341, "integer nucleotide values", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0].addNewMutation(m1, 0.5, 10, nucleotide='A'); stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0].addNewMutation(m1, 0.5, 10, nucleotide=0); stop(); }", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0:3].addNewMutation(m1, 0.5, 10); }", 1, 343, "nucleotide to be non-NULL", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0:3].addNewMutation(m1, 0.5, 10, nucleotide=NULL); }", 1, 343, "nucleotide to be non-NULL", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0:3].addNewMutation(m1, 0.5, 10, nucleotide=c('A','D','G','C')); }", 1, 343, "string nucleotide values", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0:3].addNewMutation(m1, 0.5, 10, nucleotide=c(0,-1,2,3)); }", 1, 343, "integer nucleotide values", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0:3].addNewMutation(m1, 0.5, 10, nucleotide=c(0,4,2,3)); }", 1, 343, "integer nucleotide values", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0:3].addNewMutation(m1, 0.5, 10, nucleotide=c('A','C','G','T')); stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0:3].addNewMutation(m1, 0.5, 10, nucleotide=0:3); stop(); }", __LINE__);
	
	// SLiMSim.nucleotideBased
	SLiMAssertScriptStop(nuc_model_init + "1 { if (sim.nucleotideBased == T) stop(); }", __LINE__);
	
	// MutationType.nucleotideBased
	SLiMAssertScriptStop(nuc_model_init + "1 { if (m1.nucleotideBased == T) stop(); }", __LINE__);
	
	// initializeGeneConversion() tests using GC bias != 0
	SLiMAssertScriptStop(nuc_model_init + "initialize() { initializeGeneConversion(0.2, 1234.5, 0.75, -0.01); } 1 { if (sim.chromosome.geneConversionEnabled == T) stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "initialize() { initializeGeneConversion(0.2, 1234.5, 0.75, -0.01); } 1 { if (sim.chromosome.geneConversionNonCrossoverFraction == 0.2) stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "initialize() { initializeGeneConversion(0.2, 1234.5, 0.75, -0.01); } 1 { if (sim.chromosome.geneConversionMeanLength == 1234.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "initialize() { initializeGeneConversion(0.2, 1234.5, 0.75, -0.01); } 1 { if (sim.chromosome.geneConversionSimpleConversionFraction == 0.75) stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "initialize() { initializeGeneConversion(0.2, 1234.5, 0.75, -0.01); } 1 { if (sim.chromosome.geneConversionGCBias == -0.01) stop(); }", __LINE__);
	
	// Chromosome.setGeneConversion() tests using GC bias != 0
	SLiMAssertScriptStop(nuc_model_init + "1 { sim.chromosome.setGeneConversion(0.2, 1234.5, 0.75, -0.01); if (sim.chromosome.geneConversionEnabled == T) stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "1 { sim.chromosome.setGeneConversion(0.2, 1234.5, 0.75, -0.01); if (sim.chromosome.geneConversionNonCrossoverFraction == 0.2) stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "1 { sim.chromosome.setGeneConversion(0.2, 1234.5, 0.75, -0.01); if (sim.chromosome.geneConversionMeanLength == 1234.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "1 { sim.chromosome.setGeneConversion(0.2, 1234.5, 0.75, -0.01); if (sim.chromosome.geneConversionSimpleConversionFraction == 0.75) stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "1 { sim.chromosome.setGeneConversion(0.2, 1234.5, 0.75, -0.01); if (sim.chromosome.geneConversionGCBias == -0.01) stop(); }", __LINE__);
	
	// Chromosome.setGeneConversion() bounds tests
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.chromosome.setGeneConversion(-0.001, 10000000000000, 0.0); stop(); }", 1, 320, "nonCrossoverFraction must be between 0.0 and 1.0", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.chromosome.setGeneConversion(1.001, 10000000000000, 0.0); stop(); }", 1, 320, "nonCrossoverFraction must be between 0.0 and 1.0", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.chromosome.setGeneConversion(0.5, -0.01, 0.0); stop(); }", 1, 320, "meanLength must be >= 0.0", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.chromosome.setGeneConversion(0.5, 1000, -0.001); stop(); }", 1, 320, "simpleConversionFraction must be between 0.0 and 1.0", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.chromosome.setGeneConversion(0.5, 1000, 1.001); stop(); }", 1, 320, "simpleConversionFraction must be between 0.0 and 1.0", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.chromosome.setGeneConversion(0.5, 1000, 0.0, -1.001); stop(); }", 1, 320, "bias must be between -1.0 and 1.0", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.chromosome.setGeneConversion(0.5, 1000, 0.0, 1.001); stop(); }", 1, 320, "bias must be between -1.0 and 1.0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.chromosome.setGeneConversion(0.5, 1000, 0.0, 0.1); stop(); }", 1, 231, "must be 0.0 in non-nucleotide-based models", __LINE__);
}

































