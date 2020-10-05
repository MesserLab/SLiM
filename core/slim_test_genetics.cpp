//
//  slim_test_genetics.cpp
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


#pragma mark MutationType tests
void _RunMutationTypeTests(void)
{	
	// ************************************************************************************
	//
	//	Gen 1+ tests: MutationType
	//
	
	// Test MutationType properties
	SLiMAssertScriptStop(gen1_setup + "1 { if (m1.color == '') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { if (m1.colorSubstitution == '') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { if (m1.convertToSubstitution == T) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { if (m1.mutationStackGroup == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { if (m1.mutationStackPolicy == 's') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { if (m1.distributionParams == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { if (m1.distributionType == 'f') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { if (m1.dominanceCoeff == 0.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { if (m1.id == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { m1.color = ''; } 2 { if (m1.color == '') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { m1.color = 'red'; } 2 { if (m1.color == 'red') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { m1.color = '#FF0000'; } 2 { if (m1.color == '#FF0000') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { m1.colorSubstitution = ''; } 2 { if (m1.colorSubstitution == '') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { m1.colorSubstitution = 'red'; } 2 { if (m1.colorSubstitution == 'red') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { m1.colorSubstitution = '#FF0000'; } 2 { if (m1.colorSubstitution == '#FF0000') stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.tag; }", 1, 219, "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { c(m1,m1).tag; }", 1, 225, "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { m1.tag = 17; } 2 { if (m1.tag == 17) stop(); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 { m1.convertToSubstitution = F; }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 { m1.mutationStackGroup = -17; }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 { m1.mutationStackPolicy = 's'; }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 { m1.mutationStackPolicy = 'f'; }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 { m1.mutationStackPolicy = 'l'; }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.mutationStackPolicy = 'z'; }", 1, 239, "property mutationStackPolicy must be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.distributionParams = 0.1; }", 1, 238, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.distributionType = 'g'; }", 1, 236, "read-only property", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 { m1.dominanceCoeff = 0.3; }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.id = 2; }", 1, 222, "read-only property", __LINE__);

	SLiMAssertScriptStop(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); c(m1,m2).mutationStackGroup = 3; c(m1,m2).mutationStackPolicy = 'f'; } 1 { stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); c(m1,m2).mutationStackGroup = 3; m1.mutationStackPolicy = 'f'; m2.mutationStackPolicy = 'l'; } 1 { stop(); }", -1, -1, "inconsistent mutationStackPolicy", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); c(m1,m2).mutationStackGroup = 3; c(m1,m2).mutationStackPolicy = 'f'; } 1 { m2.mutationStackPolicy = 'l'; }", -1, -1, "inconsistent mutationStackPolicy", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); m1.mutationStackPolicy = 'f'; m2.mutationStackPolicy = 'l'; } 1 { stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); m1.mutationStackPolicy = 'f'; m2.mutationStackPolicy = 'l'; } 1 { c(m1,m2).mutationStackGroup = 3; }", -1, -1, "inconsistent mutationStackPolicy", __LINE__);
	
	// Test MutationType - (void)setDistribution(string$ distributionType, ...)
	SLiMAssertScriptStop(gen1_setup + "1 { m1.setDistribution('f', 2.2); if (m1.distributionType == 'f' & m1.distributionParams == 2.2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { m1.setDistribution('g', 3.1, 7.5); if (m1.distributionType == 'g' & identical(m1.distributionParams, c(3.1, 7.5))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { m1.setDistribution('e', -3); if (m1.distributionType == 'e' & m1.distributionParams == -3) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { m1.setDistribution('n', 3.1, 7.5); if (m1.distributionType == 'n' & identical(m1.distributionParams, c(3.1, 7.5))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { m1.setDistribution('w', 3.1, 7.5); if (m1.distributionType == 'w' & identical(m1.distributionParams, c(3.1, 7.5))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { m1.setDistribution('s', 'return 1;'); if (m1.distributionType == 's' & identical(m1.distributionParams, 'return 1;')) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('x', 1.5); stop(); }", 1, 219, "must be \"f\", \"g\", \"e\", \"n\", \"w\", or \"s\"", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('f', 'foo'); stop(); }", 1, 219, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('g', 'foo', 7.5); stop(); }", 1, 219, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('g', 3.1, 'foo'); stop(); }", 1, 219, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('e', 'foo'); stop(); }", 1, 219, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('n', 'foo', 7.5); stop(); }", 1, 219, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('n', 3.1, 'foo'); stop(); }", 1, 219, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('w', 'foo', 7.5); stop(); }", 1, 219, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('w', 3.1, 'foo'); stop(); }", 1, 219, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('s', 3); stop(); }", 1, 219, "must be of type string", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('f', '1'); stop(); }", 1, 219, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('g', '1', 7.5); stop(); }", 1, 219, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('g', 3.1, '1'); stop(); }", 1, 219, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('e', '1'); stop(); }", 1, 219, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('n', '1', 7.5); stop(); }", 1, 219, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('n', 3.1, '1'); stop(); }", 1, 219, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('w', '1', 7.5); stop(); }", 1, 219, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('w', 3.1, '1'); stop(); }", 1, 219, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('s', 3.1); stop(); }", 1, 219, "must be of type string", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('f', T); stop(); }", 1, 219, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('g', T, 7.5); stop(); }", 1, 219, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('g', 3.1, T); stop(); }", 1, 219, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('e', T); stop(); }", 1, 219, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('n', T, 7.5); stop(); }", 1, 219, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('n', 3.1, T); stop(); }", 1, 219, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('w', T, 7.5); stop(); }", 1, 219, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('w', 3.1, T); stop(); }", 1, 219, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('s', T); stop(); }", 1, 219, "must be of type string", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('g', 3.1, 0.0); }", 1, 219, "must have a shape parameter > 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('g', 3.1, -1.0); }", 1, 219, "must have a shape parameter > 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('n', 3.1, -1.0); }", 1, 219, "must have a standard deviation parameter >= 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('w', 0.0, 7.5); }", 1, 219, "must have a scale parameter > 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('w', -1.0, 7.5); }", 1, 219, "must have a scale parameter > 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('w', 3.1, 0.0); }", 1, 219, "must have a shape parameter > 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('w', 3.1, -7.5); }", 1, 219, "must have a shape parameter > 0", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { m1.setDistribution('s', 'return foo;'); } 100 { stop(); }", -1, -1, "undefined identifier foo", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { m1.setDistribution('s', 'x >< 5;'); } 100 { stop(); }", -1, -1, "tokenize/parse error in type 's' DFE callback script", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { m1.setDistribution('s', 'x $ 5;'); } 100 { stop(); }", -1, -1, "tokenize/parse error in type 's' DFE callback script", __LINE__);
	
	// Test MutationType - (float)drawSelectionCoefficient([integer$ n = 1])
	// the parameters here are chosen so that these tests should fail extremely rarely
	SLiMAssertScriptStop(gen1_setup + "1 { m1.setDistribution('f', 2.2); if (m1.drawSelectionCoefficient() == 2.2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { m1.setDistribution('f', 2.2); if (identical(m1.drawSelectionCoefficient(10), rep(2.2, 10))) stop(); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 { m1.setDistribution('g', 3.1, 7.5); m1.drawSelectionCoefficient(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { m1.setDistribution('g', 3.1, 7.5); if (abs(mean(m1.drawSelectionCoefficient(5000)) - 3.1) < 0.1) stop(); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 { m1.setDistribution('e', -3.0); m1.drawSelectionCoefficient(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { m1.setDistribution('e', -3.0); if (abs(mean(m1.drawSelectionCoefficient(30000)) + 3.0) < 0.1) stop(); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 { m1.setDistribution('n', 3.1, 0.5); m1.drawSelectionCoefficient(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { m1.setDistribution('n', 3.1, 0.5); if (abs(mean(m1.drawSelectionCoefficient(2000)) - 3.1) < 0.1) stop(); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 { m1.setDistribution('w', 3.1, 7.5); m1.drawSelectionCoefficient(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { m1.setDistribution('w', 3.1, 7.5); if (abs(mean(m1.drawSelectionCoefficient(2000)) - 2.910106) < 0.1) stop(); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 { m1.setDistribution('s', 'rbinom(1, 4, 0.5);'); m1.drawSelectionCoefficient(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { m1.setDistribution('s', 'rbinom(1, 4, 0.5);'); if (abs(mean(m1.drawSelectionCoefficient(5000)) - 2.0) < 0.1) stop(); }", __LINE__);
}

#pragma mark GenomicElementType tests
void _RunGenomicElementTypeTests(void)
{
	// ************************************************************************************
	//
	//	Gen 1+ tests: GenomicElementType
	//
	
	// Test GenomicElementType properties
	SLiMAssertScriptStop(gen1_setup + "1 { if (g1.color == '') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { if (g1.id == 1) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { g1.id = 2; }", 1, 222, "read-only property", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { if (g1.mutationFractions == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { if (g1.mutationTypes == m1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { g1.color = ''; } 2 { if (g1.color == '') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { g1.color = 'red'; } 2 { if (g1.color == 'red') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { g1.color = '#FF0000'; } 2 { if (g1.color == '#FF0000') stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { g1.tag; }", 1, 219, "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { c(g1,g1).tag; }", 1, 225, "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { g1.tag = 17; } 2 { if (g1.tag == 17) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { g1.mutationFractions = 1.0; }", 1, 237, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { g1.mutationTypes = m1; }", 1, 233, "read-only property", __LINE__);
	
	// Test GenomicElementType - (void)setMutationFractions(io<MutationType> mutationTypes, numeric proportions)
	SLiMAssertScriptStop(gen1_setup + "1 { g1.setMutationFractions(object(), integer(0)); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { g1.setMutationFractions(m1, 0.0); if (g1.mutationTypes == m1 & g1.mutationFractions == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { g1.setMutationFractions(1, 0.0); if (g1.mutationTypes == m1 & g1.mutationFractions == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { g1.setMutationFractions(m1, 0.3); if (g1.mutationTypes == m1 & g1.mutationFractions == 0.3) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { g1.setMutationFractions(1, 0.3); if (g1.mutationTypes == m1 & g1.mutationFractions == 0.3) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); } 1 { g1.setMutationFractions(c(m1,m2), c(0.3, 0.7)); if (identical(g1.mutationTypes, c(m1,m2)) & identical(g1.mutationFractions, c(0.3,0.7))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); } 1 { g1.setMutationFractions(c(1,2), c(0.3, 0.7)); if (identical(g1.mutationTypes, c(m1,m2)) & identical(g1.mutationFractions, c(0.3,0.7))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); } 1 { g1.setMutationFractions(c(m1,m2)); stop(); }", 1, 281, "missing required argument", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); } 1 { g1.setMutationFractions(c(m1,m2), 0.3); stop(); }", 1, 281, "requires the sizes", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); } 1 { g1.setMutationFractions(c(m1,m2), c(-1, 2)); stop(); }", 1, 281, "must be greater than or equal to zero", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); } 1 { g1.setMutationFractions(c(2,3), c(1, 2)); stop(); }", 1, 281, "not defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); } 1 { g1.setMutationFractions(c(m2,m2), c(1, 2)); stop(); }", 1, 281, "used more than once", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); } 1 { g1.setMutationFractions(c(2,2), c(1, 2)); stop(); }", 1, 281, "used more than once", __LINE__);
}

#pragma mark GenomicElement tests
void _RunGenomicElementTests(void)
{	
	// ************************************************************************************
	//
	//	Gen 1+ tests: GenomicElement
	//
	
	std::string gen1_setup_2ge("initialize() { initializeMutationRate(1e-7); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 999); initializeGenomicElement(g1, 1000, 99999); initializeRecombinationRate(1e-8); } ");
	
	// Test GenomicElement properties
	SLiMAssertScriptStop(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; if (ge.endPosition == 999) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; if (ge.startPosition == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; if (ge.genomicElementType == g1) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; ge.tag; }", 1, 300, "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; ge.tag = -12; if (ge.tag == -12) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; ge.endPosition = 999; stop(); }", 1, 312, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; ge.startPosition = 0; stop(); }", 1, 314, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; ge.genomicElementType = g1; stop(); }", 1, 319, "read-only property", __LINE__);
	SLiMAssertScriptStop(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[1]; if (ge.endPosition == 99999) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[1]; if (ge.startPosition == 1000) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[1]; if (ge.genomicElementType == g1) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[1]; ge.tag; }", 1, 300, "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[1]; ge.tag = -17; if (ge.tag == -17) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[1]; ge.endPosition = 99999; stop(); }", 1, 312, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[1]; ge.startPosition = 1000; stop(); }", 1, 314, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[1]; ge.genomicElementType = g1; stop(); }", 1, 319, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements; ge.tag; }", 1, 297, "before being set", __LINE__);
	
	// Test GenomicElement - (void)setGenomicElementType(io<GenomicElementType>$ genomicElementType)
	SLiMAssertScriptStop(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; ge.setGenomicElementType(g1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; ge.setGenomicElementType(1); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; ge.setGenomicElementType(); stop(); }", 1, 300, "missing required argument", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; ge.setGenomicElementType(object()); stop(); }", 1, 300, "must be a singleton", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; ge.setGenomicElementType(2); stop(); }", 1, 300, "not defined", __LINE__);
	
	// Test GenomicElement position testing
	SLiMAssertScriptStop(gen1_setup_2ge + "initialize() { initializeGenomicElement(g1, 100000, 100000); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_2ge + "initialize() { initializeGenomicElement(g1, 99999, 100000); stop(); }", 1, 268, "overlaps existing genomic element", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_2ge + "initialize() { initializeGenomicElement(g1, -2, -1); stop(); }", 1, 268, "chromosome position or length is out of range", __LINE__);
}

#pragma mark Chromosome tests
void _RunChromosomeTests(void)
{
	// ************************************************************************************
	//
	//	Gen 1+ tests: Chromosome
	//
	
	// Test Chromosome properties
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; if (ch.colorSubstitution == '#3333FF') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; if (ch.geneConversionEnabled == F) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; if (ch.geneConversionGCBias == 0.0) stop(); }", 1, 244, "not defined since the DSB", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; if (ch.geneConversionNonCrossoverFraction == 0.0) stop(); }", 1, 244, "not defined since the DSB", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; if (ch.geneConversionMeanLength == 0.0) stop(); }", 1, 244, "not defined since the DSB", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; if (ch.geneConversionSimpleConversionFraction == 0.0) stop(); }", 1, 244, "not defined since the DSB", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; if (ch.genomicElements[0].genomicElementType == g1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; if (ch.lastPosition == 99999) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; if (ch.overallRecombinationRate == 1e-8 * 99999) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; if (isNULL(ch.overallRecombinationRateM)) stop(); }", 1, 251, "sex-specific recombination rate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; if (isNULL(ch.overallRecombinationRateF)) stop(); }", 1, 251, "sex-specific recombination rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; if (ch.recombinationEndPositions == 99999) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; if (isNULL(ch.recombinationEndPositionsM)) stop(); }", 1, 251, "sex-specific recombination rate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; if (isNULL(ch.recombinationEndPositionsF)) stop(); }", 1, 251, "sex-specific recombination rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; if (ch.recombinationRates == 1e-8) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; if (isNULL(ch.recombinationRatesM)) stop(); }", 1, 251, "sex-specific recombination rate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; if (isNULL(ch.recombinationRatesF)) stop(); }", 1, 251, "sex-specific recombination rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; if (ch.overallMutationRate == 1e-7 * 100000) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; if (isNULL(ch.overallMutationRateM)) stop(); }", 1, 251, "sex-specific mutation rate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; if (isNULL(ch.overallMutationRateF)) stop(); }", 1, 251, "sex-specific mutation rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; if (ch.mutationEndPositions == 99999) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; if (isNULL(ch.mutationEndPositionsM)) stop(); }", 1, 251, "sex-specific mutation rate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; if (isNULL(ch.mutationEndPositionsF)) stop(); }", 1, 251, "sex-specific mutation rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; if (ch.mutationRates == 1e-7) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; if (isNULL(ch.mutationRatesM)) stop(); }", 1, 251, "sex-specific mutation rate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; if (isNULL(ch.mutationRatesF)) stop(); }", 1, 251, "sex-specific mutation rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.colorSubstitution = ''; if (ch.colorSubstitution == '') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.colorSubstitution = 'red'; if (ch.colorSubstitution == 'red') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.colorSubstitution = '#FF0000'; if (ch.colorSubstitution == '#FF0000') stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.tag; }", 1, 240, "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; c(ch,ch).tag; }", 1, 246, "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.tag = 3294; if (ch.tag == 3294) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.genomicElements = ch.genomicElements; stop(); }", 1, 256, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.lastPosition = 99999; stop(); }", 1, 253, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.overallRecombinationRate = 1e-2; stop(); }", 1, 265, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.overallRecombinationRateM = 1e-2; stop(); }", 1, 266, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.overallRecombinationRateF = 1e-2; stop(); }", 1, 266, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.recombinationEndPositions = 99999; stop(); }", 1, 266, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.recombinationEndPositionsM = 99999; stop(); }", 1, 267, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.recombinationEndPositionsF = 99999; stop(); }", 1, 267, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.recombinationRates = 1e-8; stop(); }", 1, 259, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.recombinationRatesM = 1e-8; stop(); }", 1, 260, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.recombinationRatesF = 1e-8; stop(); }", 1, 260, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.overallMutationRate = 1e-2; stop(); }", 1, 260, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.overallMutationRateM = 1e-2; stop(); }", 1, 261, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.overallMutationRateF = 1e-2; stop(); }", 1, 261, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.mutationEndPositions = 99999; stop(); }", 1, 261, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.mutationEndPositionsM = 99999; stop(); }", 1, 262, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.mutationEndPositionsF = 99999; stop(); }", 1, 262, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.mutationRates = 1e-8; stop(); }", 1, 254, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.mutationRatesM = 1e-8; stop(); }", 1, 255, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.mutationRatesF = 1e-8; stop(); }", 1, 255, "read-only property", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; if (ch.colorSubstitution == '#3333FF') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; if (ch.geneConversionEnabled == F) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; if (ch.geneConversionGCBias == 0.0) stop(); }", 1, 264, "not defined since the DSB", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; if (ch.geneConversionNonCrossoverFraction == 0.0) stop(); }", 1, 264, "not defined since the DSB", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; if (ch.geneConversionMeanLength == 0.0) stop(); }", 1, 264, "not defined since the DSB", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; if (ch.geneConversionSimpleConversionFraction == 0.0) stop(); }", 1, 264, "not defined since the DSB", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; if (ch.genomicElements[0].genomicElementType == g1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; if (ch.lastPosition == 99999) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; if (ch.overallRecombinationRate == 1e-8 * 99999) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; if (isNULL(ch.overallRecombinationRateM)) stop(); }", 1, 271, "sex-specific recombination rate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; if (isNULL(ch.overallRecombinationRateF)) stop(); }", 1, 271, "sex-specific recombination rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; if (ch.recombinationEndPositions == 99999) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; if (isNULL(ch.recombinationEndPositionsM)) stop(); }", 1, 271, "sex-specific recombination rate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; if (isNULL(ch.recombinationEndPositionsF)) stop(); }", 1, 271, "sex-specific recombination rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; if (ch.recombinationRates == 1e-8) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; if (isNULL(ch.recombinationRatesM)) stop(); }", 1, 271, "sex-specific recombination rate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; if (isNULL(ch.recombinationRatesF)) stop(); }", 1, 271, "sex-specific recombination rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; if (ch.overallMutationRate == 1e-7 * 100000) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; if (isNULL(ch.overallMutationRateM)) stop(); }", 1, 271, "sex-specific mutation rate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; if (isNULL(ch.overallMutationRateF)) stop(); }", 1, 271, "sex-specific mutation rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; if (ch.mutationEndPositions == 99999) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; if (isNULL(ch.mutationEndPositionsM)) stop(); }", 1, 271, "sex-specific mutation rate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; if (isNULL(ch.mutationEndPositionsF)) stop(); }", 1, 271, "sex-specific mutation rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; if (ch.mutationRates == 1e-7) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; if (isNULL(ch.mutationRatesM)) stop(); }", 1, 271, "sex-specific mutation rate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; if (isNULL(ch.mutationRatesF)) stop(); }", 1, 271, "sex-specific mutation rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; ch.colorSubstitution = ''; if (ch.colorSubstitution == '') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; ch.colorSubstitution = 'red'; if (ch.colorSubstitution == 'red') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; ch.colorSubstitution = '#FF0000'; if (ch.colorSubstitution == '#FF0000') stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.tag; }", 1, 260, "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; c(ch,ch).tag; }", 1, 266, "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; ch.tag = 3294; if (ch.tag == 3294) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.genomicElements = ch.genomicElements; stop(); }", 1, 276, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.lastPosition = 99999; stop(); }", 1, 273, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.overallRecombinationRate = 1e-2; stop(); }", 1, 285, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.overallRecombinationRateM = 1e-2; stop(); }", 1, 286, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.overallRecombinationRateF = 1e-2; stop(); }", 1, 286, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.recombinationEndPositions = 99999; stop(); }", 1, 286, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.recombinationEndPositionsM = 99999; stop(); }", 1, 287, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.recombinationEndPositionsF = 99999; stop(); }", 1, 287, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.recombinationRates = 1e-8; stop(); }", 1, 279, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.recombinationRatesM = 1e-8; stop(); }", 1, 280, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.recombinationRatesF = 1e-8; stop(); }", 1, 280, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.overallMutationRate = 1e-2; stop(); }", 1, 280, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.overallMutationRateM = 1e-2; stop(); }", 1, 281, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.overallMutationRateF = 1e-2; stop(); }", 1, 281, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.mutationEndPositions = 99999; stop(); }", 1, 281, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.mutationEndPositionsM = 99999; stop(); }", 1, 282, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.mutationEndPositionsF = 99999; stop(); }", 1, 282, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.mutationRates = 1e-8; stop(); }", 1, 274, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.mutationRatesM = 1e-8; stop(); }", 1, 275, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.mutationRatesF = 1e-8; stop(); }", 1, 275, "read-only property", __LINE__);
	
	std::string gen1_setup_sex_2rates("initialize() { initializeSex('X'); initializeMutationRate(1e-7, sex='M'); initializeMutationRate(1e-8, sex='F'); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8, 99999, 'M'); initializeRecombinationRate(1e-7, 99999, 'F'); } ");
	
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.colorSubstitution == '#3333FF') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.geneConversionEnabled == F) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.geneConversionGCBias == 0.0) stop(); }", 1, 371, "not defined since the DSB", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.geneConversionNonCrossoverFraction == 0.0) stop(); }", 1, 371, "not defined since the DSB", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.geneConversionMeanLength == 0.0) stop(); }", 1, 371, "not defined since the DSB", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.geneConversionSimpleConversionFraction == 0.0) stop(); }", 1, 371, "not defined since the DSB", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.genomicElements[0].genomicElementType == g1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.lastPosition == 99999) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (isNULL(ch.overallRecombinationRate)) stop(); }", 1, 378, "sex-specific recombination rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.overallRecombinationRateM == 1e-8 * 99999) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.overallRecombinationRateF == 1e-7 * 99999) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (isNULL(ch.recombinationEndPositions)) stop(); }", 1, 378, "sex-specific recombination rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.recombinationEndPositionsM == 99999) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.recombinationEndPositionsF == 99999) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (isNULL(ch.recombinationRates)) stop(); }", 1, 378, "sex-specific recombination rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.recombinationRatesM == 1e-8) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.recombinationRatesF == 1e-7) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (isNULL(ch.overallMutationRate)) stop(); }", 1, 378, "sex-specific mutation rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.overallMutationRateM == 1e-7 * 100000) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.overallMutationRateF == 1e-8 * 100000) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (isNULL(ch.mutationEndPositions)) stop(); }", 1, 378, "sex-specific mutation rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.mutationEndPositionsM == 99999) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.mutationEndPositionsF == 99999) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (isNULL(ch.mutationRates)) stop(); }", 1, 378, "sex-specific mutation rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.mutationRatesM == 1e-7) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.mutationRatesF == 1e-8) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.colorSubstitution = ''; if (ch.colorSubstitution == '') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.colorSubstitution = 'red'; if (ch.colorSubstitution == 'red') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.colorSubstitution = '#FF0000'; if (ch.colorSubstitution == '#FF0000') stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.tag; }", 1, 367, "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; c(ch,ch).tag; }", 1, 373, "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.tag = 3294; if (ch.tag == 3294) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.genomicElements = ch.genomicElements; stop(); }", 1, 383, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.lastPosition = 99999; stop(); }", 1, 380, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.overallRecombinationRate = 1e-2; stop(); }", 1, 392, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.overallRecombinationRateM = 1e-2; stop(); }", 1, 393, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.overallRecombinationRateF = 1e-2; stop(); }", 1, 393, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.recombinationEndPositions = 99999; stop(); }", 1, 393, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.recombinationEndPositionsM = 99999; stop(); }", 1, 394, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.recombinationEndPositionsF = 99999; stop(); }", 1, 394, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.recombinationRates = 1e-8; stop(); }", 1, 386, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.recombinationRatesM = 1e-8; stop(); }", 1, 387, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.recombinationRatesF = 1e-8; stop(); }", 1, 387, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.overallMutationRate = 1e-2; stop(); }", 1, 387, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.overallMutationRateM = 1e-2; stop(); }", 1, 388, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.overallMutationRateF = 1e-2; stop(); }", 1, 388, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.mutationEndPositions = 99999; stop(); }", 1, 388, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.mutationEndPositionsM = 99999; stop(); }", 1, 389, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.mutationEndPositionsF = 99999; stop(); }", 1, 389, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.mutationRates = 1e-8; stop(); }", 1, 381, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.mutationRatesM = 1e-8; stop(); }", 1, 382, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.mutationRatesF = 1e-8; stop(); }", 1, 382, "read-only property", __LINE__);
	
	// Test Chromosome - (void)setMutationRate(numeric rates, [integer ends])
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.setMutationRate(0.0); stop(); }", __LINE__);														// legal: singleton rate, no end
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setMutationRate(); stop(); }", 1, 240, "missing required argument", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setMutationRate(-0.00001); stop(); }", 1, 240, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setMutationRate(10000); stop(); }", 1, 240, "rate is >= 1.0", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), c(1000, 99999)); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.001), c(1000, 99999)); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1)); stop(); }", 1, 240, "to be a singleton if", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setMutationRate(integer(0), integer(0)); stop(); }", 1, 240, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), 99999); stop(); }", 1, 240, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), 99997:99999); stop(); }", 1, 240, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), c(99999, 1000)); stop(); }", 1, 240, "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), c(99999, 99999)); stop(); }", 1, 240, "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, -0.001), c(1000, 99999)); stop(); }", 1, 240, "must be >= 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, -0.001), c(1000, 2000)); stop(); }", 1, 240, "must be >= 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, -0.001), c(1000, 100000)); stop(); }", 1, 240, "must be >= 0", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), c(1000, 99999), '*'); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.001), c(1000, 99999), '*'); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setMutationRate(integer(0), integer(0), '*'); stop(); }", 1, 240, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), 99999, '*'); stop(); }", 1, 240, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), 99997:99999, '*'); stop(); }", 1, 240, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), c(99999, 1000), '*'); stop(); }", 1, 240, "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), c(99999, 99999), '*'); stop(); }", 1, 240, "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, -0.001), c(1000, 99999), '*'); stop(); }", 1, 240, "must be >= 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, -0.001), c(1000, 2000), '*'); stop(); }", 1, 240, "must be >= 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, -0.001), c(1000, 100000), '*'); stop(); }", 1, 240, "must be >= 0", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(0.0); stop(); }", __LINE__);														// legal: singleton rate, no end
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(); stop(); }", 1, 260, "missing required argument", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(-0.00001); stop(); }", 1, 260, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(10000); stop(); }", 1, 260, "rate is >= 1.0", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), c(1000, 99999)); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.001), c(1000, 99999)); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1)); stop(); }", 1, 260, "to be a singleton if", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(integer(0), integer(0)); stop(); }", 1, 260, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), 99999); stop(); }", 1, 260, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), 99997:99999); stop(); }", 1, 260, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), c(99999, 1000)); stop(); }", 1, 260, "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), c(99999, 99999)); stop(); }", 1, 260, "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, -0.001), c(1000, 99999)); stop(); }", 1, 260, "must be >= 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, -0.001), c(1000, 2000)); stop(); }", 1, 260, "must be >= 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, -0.001), c(1000, 100000)); stop(); }", 1, 260, "must be >= 0", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), c(1000, 99999), '*'); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.001), c(1000, 99999), '*'); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(integer(0), integer(0), '*'); stop(); }", 1, 260, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), 99999, '*'); stop(); }", 1, 260, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), 99997:99999, '*'); stop(); }", 1, 260, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), c(99999, 1000), '*'); stop(); }", 1, 260, "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), c(99999, 99999), '*'); stop(); }", 1, 260, "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, -0.001), c(1000, 99999), '*'); stop(); }", 1, 260, "must be >= 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, -0.001), c(1000, 2000), '*'); stop(); }", 1, 260, "must be >= 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, -0.001), c(1000, 100000), '*'); stop(); }", 1, 260, "must be >= 0", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), c(1000, 99999), 'M'); stop(); }", 1, 260, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(integer(0), integer(0), 'M'); stop(); }", 1, 260, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), 99999, 'M'); stop(); }", 1, 260, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), 99997:99999, 'M'); stop(); }", 1, 260, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), c(99999, 1000), 'M'); stop(); }", 1, 260, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), c(99999, 99999), 'M'); stop(); }", 1, 260, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, -0.001), c(1000, 99999), 'M'); stop(); }", 1, 260, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, -0.001), c(1000, 2000), 'M'); stop(); }", 1, 260, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, -0.001), c(1000, 100000), 'M'); stop(); }", 1, 260, "single map versus separate maps", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(0.0); stop(); }", 1, 367, "single map versus separate maps", __LINE__);														// legal: singleton rate, no end
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(); stop(); }", 1, 367, "missing required argument", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(-0.00001); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(10000); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), c(1000, 99999)); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.001), c(1000, 99999)); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1)); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(integer(0), integer(0)); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), 99999); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), 99997:99999); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), c(99999, 1000)); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), c(99999, 99999)); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, -0.001), c(1000, 99999)); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, -0.001), c(1000, 2000)); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, -0.001), c(1000, 100000)); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), c(1000, 99999), '*'); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.001), c(1000, 99999), '*'); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(integer(0), integer(0), '*'); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), 99999, '*'); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), 99997:99999, '*'); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), c(99999, 1000), '*'); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), c(99999, 99999), '*'); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, -0.001), c(1000, 99999), '*'); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, -0.001), c(1000, 2000), '*'); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, -0.001), c(1000, 100000), '*'); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), c(1000, 99999), 'M'); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.001), c(1000, 99999), 'M'); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(integer(0), integer(0), 'M'); stop(); }", 1, 367, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), 99999, 'M'); stop(); }", 1, 367, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), 99997:99999, 'M'); stop(); }", 1, 367, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), c(99999, 1000), 'M'); stop(); }", 1, 367, "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), c(99999, 99999), 'M'); stop(); }", 1, 367, "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, -0.001), c(1000, 99999), 'M'); stop(); }", 1, 367, "must be >= 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, -0.001), c(1000, 2000), 'M'); stop(); }", 1, 367, "must be >= 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, -0.001), c(1000, 100000), 'M'); stop(); }", 1, 367, "must be >= 0", __LINE__);
	
	// Test Chromosome - (void)setRecombinationRate(numeric rates, [integer ends])
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(0.0); stop(); }", __LINE__);														// legal: singleton rate, no end
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(); stop(); }", 1, 240, "missing required argument", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(-0.00001); stop(); }", 1, 240, "out of range", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(0.5); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(0.6); stop(); }", 1, 240, "rates must be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(10000); stop(); }", 1, 240, "rates must be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(1000, 99999)); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.001), c(1000, 99999)); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1)); stop(); }", 1, 240, "to be a singleton if", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(integer(0), integer(0)); stop(); }", 1, 240, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99999); stop(); }", 1, 240, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99997:99999); stop(); }", 1, 240, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 1000)); stop(); }", 1, 240, "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 99999)); stop(); }", 1, 240, "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 99999)); stop(); }", 1, 240, "rates must be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 2000)); stop(); }", 1, 240, "rates must be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 100000)); stop(); }", 1, 240, "rates must be in [0.0, 0.5]", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(1000, 99999), '*'); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.001), c(1000, 99999), '*'); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(integer(0), integer(0), '*'); stop(); }", 1, 240, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99999, '*'); stop(); }", 1, 240, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99997:99999, '*'); stop(); }", 1, 240, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 1000), '*'); stop(); }", 1, 240, "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 99999), '*'); stop(); }", 1, 240, "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 99999), '*'); stop(); }", 1, 240, "rates must be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 2000), '*'); stop(); }", 1, 240, "rates must be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 100000), '*'); stop(); }", 1, 240, "rates must be in [0.0, 0.5]", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(0.0); stop(); }", __LINE__);														// legal: singleton rate, no end
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(); stop(); }", 1, 260, "missing required argument", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(-0.00001); stop(); }", 1, 260, "out of range", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(0.5); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(0.6); stop(); }", 1, 260, "rates must be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(10000); stop(); }", 1, 260, "rates must be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(1000, 99999)); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.001), c(1000, 99999)); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1)); stop(); }", 1, 260, "to be a singleton if", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(integer(0), integer(0)); stop(); }", 1, 260, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99999); stop(); }", 1, 260, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99997:99999); stop(); }", 1, 260, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 1000)); stop(); }", 1, 260, "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 99999)); stop(); }", 1, 260, "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 99999)); stop(); }", 1, 260, "rates must be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 2000)); stop(); }", 1, 260, "rates must be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 100000)); stop(); }", 1, 260, "rates must be in [0.0, 0.5]", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(1000, 99999), '*'); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.001), c(1000, 99999), '*'); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(integer(0), integer(0), '*'); stop(); }", 1, 260, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99999, '*'); stop(); }", 1, 260, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99997:99999, '*'); stop(); }", 1, 260, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 1000), '*'); stop(); }", 1, 260, "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 99999), '*'); stop(); }", 1, 260, "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 99999), '*'); stop(); }", 1, 260, "rates must be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 2000), '*'); stop(); }", 1, 260, "rates must be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 100000), '*'); stop(); }", 1, 260, "rates must be in [0.0, 0.5]", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(1000, 99999), 'M'); stop(); }", 1, 260, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(integer(0), integer(0), 'M'); stop(); }", 1, 260, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99999, 'M'); stop(); }", 1, 260, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99997:99999, 'M'); stop(); }", 1, 260, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 1000), 'M'); stop(); }", 1, 260, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 99999), 'M'); stop(); }", 1, 260, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 99999), 'M'); stop(); }", 1, 260, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 2000), 'M'); stop(); }", 1, 260, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 100000), 'M'); stop(); }", 1, 260, "single map versus separate maps", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(0.0); stop(); }", 1, 367, "single map versus separate maps", __LINE__);														// legal: singleton rate, no end
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(); stop(); }", 1, 367, "missing required argument", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(-0.00001); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(10000); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(1000, 99999)); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.001), c(1000, 99999)); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1)); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(integer(0), integer(0)); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99999); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99997:99999); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 1000)); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 99999)); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 99999)); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 2000)); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 100000)); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(1000, 99999), '*'); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.001), c(1000, 99999), '*'); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(integer(0), integer(0), '*'); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99999, '*'); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99997:99999, '*'); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 1000), '*'); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 99999), '*'); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 99999), '*'); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 2000), '*'); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 100000), '*'); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(1000, 99999), 'M'); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.001), c(1000, 99999), 'M'); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(integer(0), integer(0), 'M'); stop(); }", 1, 367, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99999, 'M'); stop(); }", 1, 367, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99997:99999, 'M'); stop(); }", 1, 367, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 1000), 'M'); stop(); }", 1, 367, "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 99999), 'M'); stop(); }", 1, 367, "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 99999), 'M'); stop(); }", 1, 367, "rates must be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 2000), 'M'); stop(); }", 1, 367, "rates must be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 100000), 'M'); stop(); }", 1, 367, "rates must be in [0.0, 0.5]", __LINE__);
	
	// initializeGeneConversion() tests
	SLiMAssertScriptStop(gen1_setup + "initialize() { initializeGeneConversion(0.2, 1234.5, 0.75); } 1 { if (sim.chromosome.geneConversionEnabled == T) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "initialize() { initializeGeneConversion(0.2, 1234.5, 0.75); } 1 { if (sim.chromosome.geneConversionNonCrossoverFraction == 0.2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "initialize() { initializeGeneConversion(0.2, 1234.5, 0.75); } 1 { if (sim.chromosome.geneConversionMeanLength == 1234.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "initialize() { initializeGeneConversion(0.2, 1234.5, 0.75); } 1 { if (sim.chromosome.geneConversionSimpleConversionFraction == 0.75) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "initialize() { initializeGeneConversion(0.2, 1234.5, 0.75); } 1 { if (sim.chromosome.geneConversionGCBias == 0.0) stop(); }", __LINE__);
	
	// setGeneConversion() tests
	SLiMAssertScriptStop(gen1_setup + "1 { sim.chromosome.setGeneConversion(0.2, 1234.5, 0.75); if (sim.chromosome.geneConversionEnabled == T) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { sim.chromosome.setGeneConversion(0.2, 1234.5, 0.75); if (sim.chromosome.geneConversionNonCrossoverFraction == 0.2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { sim.chromosome.setGeneConversion(0.2, 1234.5, 0.75); if (sim.chromosome.geneConversionMeanLength == 1234.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { sim.chromosome.setGeneConversion(0.2, 1234.5, 0.75); if (sim.chromosome.geneConversionSimpleConversionFraction == 0.75) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { sim.chromosome.setGeneConversion(0.2, 1234.5, 0.75); if (sim.chromosome.geneConversionGCBias == 0.0) stop(); }", __LINE__);
}

#pragma mark Mutation tests
void _RunMutationTests(void)
{
	// ************************************************************************************
	//
	//	Gen 1+ tests: Mutation
	//
	
	// Test Mutation properties
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; if (mut.mutationType == m1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; if ((mut.originGeneration >= 1) & (mut.originGeneration < 10)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; if ((mut.position >= 0) & (mut.position < 100000)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; if (mut.selectionCoeff == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; if (mut.subpopID == 1) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; mut.mutationType = m1; stop(); }", 1, 289, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; mut.originGeneration = 1; stop(); }", 1, 293, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; mut.position = 0; stop(); }", 1, 285, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; mut.selectionCoeff = 0.1; stop(); }", 1, 291, "read-only property", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; mut.subpopID = 237; if (mut.subpopID == 237) stop(); }", __LINE__);						// legal; this field may be used as a user tag
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; mut.tag; }", 1, 276, "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; c(mut,mut).tag; }", 1, 283, "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; mut.tag = 278; if (mut.tag == 278) stop(); }", __LINE__);
	
	// Test Mutation - (void)setMutationType(io<MutationType>$ mutType)
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; mut.setMutationType(m1); if (mut.mutationType == m1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; mut.setMutationType(m1); if (mut.mutationType == m1) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; mut.setMutationType(2); if (mut.mutationType == m1) stop(); }", 1, 276, "mutation type m2 not defined", __LINE__);
	
	// Test Mutation - (void)setSelectionCoeff(float$ selectionCoeff)
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; mut.setSelectionCoeff(0.5); if (mut.selectionCoeff == 0.5) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; mut.setSelectionCoeff(1); if (mut.selectionCoeff == 1) stop(); }", 1, 276, "cannot be type integer", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; mut.setSelectionCoeff(-500.0); if (mut.selectionCoeff == -500.0) stop(); }", __LINE__);	// legal; no lower bound
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; mut.setSelectionCoeff(500.0); if (mut.selectionCoeff == 500.0) stop(); }", __LINE__);		// legal; no upper bound
}

#pragma mark Substitution tests
void _RunSubstitutionTests(void)
{
	// ************************************************************************************
	//
	//	Gen 1+ tests: Substitution
	//
	
	// Test Substitution properties
	SLiMAssertScriptStop(gen1_setup_fixmut_p1 + "30 { if (size(sim.substitutions) > 0) stop(); }", __LINE__);										// check that our script generates substitutions fast enough
	SLiMAssertScriptStop(gen1_setup_fixmut_p1 + "30 { sub = sim.substitutions[0]; if (sub.fixationGeneration > 0 & sub.fixationGeneration <= 30) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_fixmut_p1 + "30 { sub = sim.substitutions[0]; if (sub.mutationType == m1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_fixmut_p1 + "30 { sub = sim.substitutions[0]; if (sub.originGeneration > 0 & sub.originGeneration <= 10) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_fixmut_p1 + "30 { sub = sim.substitutions[0]; if (sub.position >= 0 & sub.position <= 99999) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_fixmut_p1 + "30 { if (sum(sim.substitutions.selectionCoeff == 500.0) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_fixmut_p1 + "30 { sub = sim.substitutions[0]; if (sub.subpopID == 1) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_fixmut_p1 + "30 { sub = sim.substitutions[0]; sub.fixationGeneration = 10; stop(); }", 1, 375, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_fixmut_p1 + "30 { sub = sim.substitutions[0]; sub.mutationType = m1; stop(); }", 1, 369, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_fixmut_p1 + "30 { sub = sim.substitutions[0]; sub.originGeneration = 10; stop(); }", 1, 373, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_fixmut_p1 + "30 { sub = sim.substitutions[0]; sub.position = 99999; stop(); }", 1, 365, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_fixmut_p1 + "30 { sub = sim.substitutions[0]; sub.selectionCoeff = 50.0; stop(); }", 1, 371, "read-only property", __LINE__);
	SLiMAssertScriptStop(gen1_setup_fixmut_p1 + "30 { sub = sim.substitutions[0]; sub.subpopID = 237; if (sub.subpopID == 237) stop(); }", __LINE__);						// legal; this field may be used as a user tag
}

#pragma mark Genome tests
void _RunGenomeTests(std::string temp_path)
{
	// ************************************************************************************
	//
	//	Gen 1+ tests: Genome
	//
	
	// Test Genome properties
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; if (gen.genomeType == 'A') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; if (gen.isNullGenome == F) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { gen = p1.genomes[0]; if (gen.mutations[0].mutationType == m1) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; gen.tag; }", 1, 272, "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; c(gen,gen).tag; }", 1, 279, "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; gen.tag = 278; if (gen.tag == 278) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; gen.genomeType = 'A'; stop(); }", 1, 283, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; gen.isNullGenome = F; stop(); }", 1, 285, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 { gen = p1.genomes[0]; gen.mutations[0].mutationType = m1; stop(); }", 1, 299, "read-only property", __LINE__);
	
	// Test Genome + (void)addMutations(object<Mutation> mutations)
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; gen.addMutations(object()); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { gen = p1.genomes[0]; gen.addMutations(gen.mutations[0]); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { gen = p1.genomes[0]; gen.addMutations(p1.genomes[1].mutations[0]); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { gen = p1.genomes[0]; mut = p1.genomes[1].mutations[0]; gen.addMutations(rep(mut, 10)); if (sum(gen.mutations == mut) == 1) stop(); }", __LINE__);
	
	// Test Genome + (object<Mutation>)addNewDrawnMutation(io<MutationType> mutationType, integer position, [Ni originGeneration], [Nio<Subpopulation> originSubpop])
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(m1, 5000, 1, p1); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(m1, 5000, 1, 1); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(m1, 5000, 1); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(m1, 5000); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(1, 5000, 1, p1); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(1, 5000, 1, 1); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(1, 5000, 1); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(1, 5000); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(1, 5000, NULL, 1); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(1, 5000, NULL); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(1, 5000, NULL, NULL); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(m1, 5000:5003, 1, p1); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(m1, 5000:5003, 10:13, 1); p1.genomes.addMutations(mut); stop(); }", 1, 278, "scratch space", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(m1, 5000:5003, 1, 0:3); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(m1, 5000:5003); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(7, 5000, NULL, 1); stop(); }", 1, 278, "not defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(1, 5000, 0, 1); stop(); }", 1, 278, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(1, -1, NULL, 1); stop(); }", 1, 278, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(1, 100000, NULL, 1); stop(); }", 1, 278, "past the end", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(1, 5000, NULL, 237); stop(); }", __LINE__);											// bad subpop, but this is legal to allow "tagging" of mutations
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(1, 5000, NULL, -1); stop(); }", 1, 278, "out of range", __LINE__);					// however, such tags must be within range
	
	// Test Genome + (object<Mutation>)addNewMutation(io<MutationType> mutationType, numeric selectionCoeff, integer position, [Ni originGeneration], [Nio<Subpopulation> originSubpop])
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(m1, 0.1, 5000, 1, p1); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(m1, 0.1, 5000, 1, 1); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(m1, 0.1, 5000, 1); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(m1, 0.1, 5000); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(1, 0.1, 5000, 1, p1); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(1, 0.1, 5000, 1, 1); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(1, 0.1, 5000, 1); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(1, 0.1, 5000); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(1, 0.1, 5000, NULL, 1); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(1, 0.1, 5000, NULL); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(1, 0.1, 5000, NULL, NULL); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(m1, 0.1, 5000:5003, 1, p1); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(m1, 0.1, 5000:5003, 10:13, 1); p1.genomes.addMutations(mut); stop(); }", 1, 278, "scratch space", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(m1, 0.1, 5000:5003, 1, 0:3); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(m1, 0.1, 5000:5003); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(m1, (0:3)/10, 5000:5003); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(m1, (0:3)/10, 5000:5003, 1, 0:3); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(7, 0.1, 5000, NULL, 1); p1.genomes.addMutations(mut); stop(); }", 1, 278, "not defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(1, 0.1, 5000, 0, 1); p1.genomes.addMutations(mut); stop(); }", 1, 278, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(1, 0.1, -1, NULL, 1); p1.genomes.addMutations(mut); stop(); }", 1, 278, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(1, 0.1, 100000, NULL, 1); p1.genomes.addMutations(mut); stop(); }", 1, 278, "past the end", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(1, 0.1, 5000, NULL, 237); p1.genomes.addMutations(mut); stop(); }", __LINE__);							// bad subpop, but this is legal to allow "tagging" of mutations
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(1, 0.1, 5000, NULL, -1); p1.genomes.addMutations(mut); stop(); }", 1, 278, "out of range", __LINE__);	// however, such tags must be within range
	
	// Test Genome + (object<Mutation>)addNewDrawnMutation(io<MutationType> mutationType, integer position, [Ni originGeneration], [io<Subpopulation> originSubpop]) with new class method non-multiplex behavior
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewDrawnMutation(m1, 5000, 1, p1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewDrawnMutation(m1, 5000, 1, 1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewDrawnMutation(m1, 5000, 1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewDrawnMutation(m1, 5000); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewDrawnMutation(1, 5000, 1, p1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewDrawnMutation(1, 5000, 1, 1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewDrawnMutation(1, 5000, 1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewDrawnMutation(1, 5000); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewDrawnMutation(1, 5000, NULL, 1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewDrawnMutation(1, 5000, NULL); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewDrawnMutation(1, 5000, NULL, NULL); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewDrawnMutation(m1, 5000:5003, 1, p1); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.genomes.addNewDrawnMutation(m1, 5000:5003, 10:13, 1); stop(); }", 1, 258, "scratch space", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewDrawnMutation(m1, 5000:5003, 1, 0:3); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewDrawnMutation(m1, 5000:5003); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.genomes.addNewDrawnMutation(7, 5000, NULL, 1); stop(); }", 1, 258, "not defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.genomes.addNewDrawnMutation(1, 5000, 0, 1); stop(); }", 1, 258, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.genomes.addNewDrawnMutation(1, -1, NULL, 1); stop(); }", 1, 258, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.genomes.addNewDrawnMutation(1, 100000, NULL, 1); stop(); }", 1, 258, "past the end", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewDrawnMutation(1, 5000, NULL, 237); stop(); }", __LINE__);											// bad subpop, but this is legal to allow "tagging" of mutations
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.genomes.addNewDrawnMutation(1, 5000, NULL, -1); stop(); }", 1, 258, "out of range", __LINE__);					// however, such tags must be within range
	
	// Test Genome + (object<Mutation>)addNewMutation(io<MutationType> mutationType, numeric selectionCoeff, integer position, [Ni originGeneration], [io<Subpopulation> originSubpop]) with new class method non-multiplex behavior
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(m1, 0.1, 5000, 1, p1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(m1, 0.1, 5000, 1, 1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(m1, 0.1, 5000, 1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(m1, 0.1, 5000); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(1, 0.1, 5000, 1, p1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(1, 0.1, 5000, 1, 1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(1, 0.1, 5000, 1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(1, 0.1, 5000); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(1, 0.1, 5000, NULL, 1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(1, 0.1, 5000, NULL); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(1, 0.1, 5000, NULL, NULL); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(m1, 0.1, 5000:5003, 1, p1); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(m1, 0.1, 5000:5003, 10:13, 1); stop(); }", 1, 258, "scratch space", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(m1, 0.1, 5000:5003, 1, 0:3); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(m1, 0.1, 5000:5003); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(m1, (0:3)/10, 5000:5003); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(m1, (0:3)/10, 5000:5003, 1, 0:3); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(7, 0.1, 5000, NULL, 1); stop(); }", 1, 258, "not defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(1, 0.1, 5000, 0, 1); stop(); }", 1, 258, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(1, 0.1, -1, NULL, 1); stop(); }", 1, 258, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(1, 0.1, 100000, NULL, 1); stop(); }", 1, 258, "past the end", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(1, 0.1, 5000, NULL, 237); stop(); }", __LINE__);							// bad subpop, but this is legal to allow "tagging" of mutations
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(1, 0.1, 5000, NULL, -1); stop(); }", 1, 258, "out of range", __LINE__);	// however, such tags must be within range
	
	// Test Genome - (logical$)containsMarkerMutation(io<MutationType>$ mutType, integer$ position, [logical$ returnMutation = F])
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { p1.genomes[0].containsMarkerMutation(m1, 1000); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { p1.genomes[0].containsMarkerMutation(1, 1000); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { p1.genomes[0:1].containsMarkerMutation(1, 1000); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "10 { p1.genomes[0].containsMarkerMutation(m1, -1); stop(); }", 1, 262, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "10 { p1.genomes[0].containsMarkerMutation(m1, 1000000); stop(); }", 1, 262, "past the end", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "10 { p1.genomes[0].containsMarkerMutation(10, 1000); stop(); }", 1, 262, "mutation type m10 not defined", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { p1.genomes[0].containsMarkerMutation(m1, 1000, returnMutation=T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { p1.genomes[0].containsMarkerMutation(1, 1000, returnMutation=T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { p1.genomes[0:1].containsMarkerMutation(1, 1000, returnMutation=T); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "10 { p1.genomes[0].containsMarkerMutation(m1, -1, returnMutation=T); stop(); }", 1, 262, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "10 { p1.genomes[0].containsMarkerMutation(m1, 1000000, returnMutation=T); stop(); }", 1, 262, "past the end", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "10 { p1.genomes[0].containsMarkerMutation(10, 1000, returnMutation=T); stop(); }", 1, 262, "mutation type m10 not defined", __LINE__);
	
	// Test Genome - (logical)containsMutations(object<Mutation> mutations)
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { p1.genomes[0].containsMutations(object()); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { p1.genomes[0].containsMutations(sim.mutations); stop(); }", __LINE__);
	
	// Test Genome - (integer$)countOfMutationsOfType(io<MutationType>$ mutType)
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { p1.genomes[0].countOfMutationsOfType(m1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { p1.genomes[0].countOfMutationsOfType(1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { p1.genomes[0:1].countOfMutationsOfType(1); stop(); }", __LINE__);
	
	// Test Genome - (integer$)positionsOfMutationsOfType(io<MutationType>$ mutType)
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { p1.genomes[0].positionsOfMutationsOfType(m1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { p1.genomes[0].positionsOfMutationsOfType(1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { p1.genomes[0:1].positionsOfMutationsOfType(1); stop(); }", __LINE__);
	
	// Test Genome - (float$)sumOfMutationsOfType(io<MutationType>$ mutType)
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { p1.genomes[0].sumOfMutationsOfType(m1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { p1.genomes[0].sumOfMutationsOfType(1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { p1.genomes[0:1].sumOfMutationsOfType(1); stop(); }", __LINE__);
	
	// Test Genome - (object<Mutation>)mutationsOfType(io<MutationType>$ mutType)
	SLiMAssertScriptSuccess(gen1_setup_p1 + "10 { p1.genomes[0].mutationsOfType(m1); } ", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1 + "10 { p1.genomes[0].mutationsOfType(1); } ", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1 + "10 { p1.genomes[0:1].mutationsOfType(1); } ", __LINE__);
	
	// Test Genome + (void)removeMutations(object<Mutation> mutations, [logical$ substitute])
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(m1, 0.1, 5000); gen.removeMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(m1, 0.1, 5000); gen.removeMutations(mut); gen.removeMutations(mut); stop(); }", __LINE__);	// legal to remove a mutation that is not present
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; gen.removeMutations(object()); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { gen = p1.genomes[0]; gen.removeMutations(gen.mutations); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(m1, 0.1, 5000); gen.removeMutations(mut, T); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(m1, 0.1, 5000); gen.removeMutations(mut, T); gen.removeMutations(mut, T); stop(); }", 1, 342, "not currently segregating", __LINE__);	// not legal to remove a mutation that has been substituted
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; gen.removeMutations(object(), T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { gen = p1.genomes[0]; gen.removeMutations(gen.mutations, T); stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(m1, 0.1, 5000); gen.removeMutations(); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(m1, 0.1, 5000); gen.removeMutations(); gen.removeMutations(NULL); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { gen = p1.genomes[0]; gen.removeMutations(NULL); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(m1, 0.1, 5000); gen.removeMutations(NULL, T); }", 1, 313, "substitute may not be T if", __LINE__);
	
	// Test Genome + (void)outputMS([Ns$ filePath], [logical$ append = F], [logical$ filterMonomorphic = F])
	SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.genomes, 0, T).outputMS(); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.genomes, 100, T).outputMS(); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.genomes, 0, T).outputMS(NULL); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.genomes, 100, T).outputMS(NULL); stop(); }", __LINE__);
	if (Eidos_SlashTmpExists())
	{
		SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.genomes, 0, T).outputMS('" + temp_path + "/slimOutputMSTest1.txt'); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.genomes, 100, T).outputMS('" + temp_path + "/slimOutputMSTest2.txt'); stop(); }", __LINE__);
	}
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "10 late() { sample(p1.genomes, 0, T).outputMS(NULL); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "10 late() { sample(p1.genomes, 100, T).outputMS(NULL); stop(); }", 1, 302, "cannot output null genomes", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "10 late() { sample(p1.genomes[!p1.genomes.isNullGenome], 100, T).outputMS(NULL); stop(); }", __LINE__);
	if (Eidos_SlashTmpExists())
	{
		SLiMAssertScriptStop(gen1_setup_sex_p1 + "10 late() { sample(p1.genomes, 0, T).outputMS('" + temp_path + "/slimOutputMSTest3.txt'); stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_sex_p1 + "10 late() { sample(p1.genomes, 100, T).outputMS('" + temp_path + "/slimOutputMSTest4.txt'); stop(); }", 1, 302, "cannot output null genomes", __LINE__);
		SLiMAssertScriptStop(gen1_setup_sex_p1 + "10 late() { sample(p1.genomes[!p1.genomes.isNullGenome], 100, T).outputMS('" + temp_path + "/slimOutputMSTest5.txt'); stop(); }", __LINE__);
	}
	
	// Test Genome + (void)output([Ns$ filePath])
	SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.genomes, 0, T).output(); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.genomes, 100, T).output(); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.genomes, 0, T).output(NULL); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.genomes, 100, T).output(NULL); stop(); }", __LINE__);
	if (Eidos_SlashTmpExists())
	{
		SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.genomes, 0, T).output('" + temp_path + "/slimOutputTest1.txt'); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.genomes, 100, T).output('" + temp_path + "/slimOutputTest2.txt'); stop(); }", __LINE__);
	}
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "10 late() { sample(p1.genomes, 0, T).output(NULL); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "10 late() { sample(p1.genomes, 100, T).output(NULL); stop(); }", 1, 302, "cannot output null genomes", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "10 late() { sample(p1.genomes[!p1.genomes.isNullGenome], 100, T).output(NULL); stop(); }", __LINE__);
	if (Eidos_SlashTmpExists())
	{
		SLiMAssertScriptStop(gen1_setup_sex_p1 + "10 late() { sample(p1.genomes, 0, T).output('" + temp_path + "/slimOutputTest3.txt'); stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_sex_p1 + "10 late() { sample(p1.genomes, 100, T).output('" + temp_path + "/slimOutputTest4.txt'); stop(); }", 1, 302, "cannot output null genomes", __LINE__);
		SLiMAssertScriptStop(gen1_setup_sex_p1 + "10 late() { sample(p1.genomes[!p1.genomes.isNullGenome], 100, T).output('" + temp_path + "/slimOutputTest5.txt'); stop(); }", __LINE__);
	}
	
	// Test Genome + (void)outputVCF([Ns$ filePath], [logical$ outputMultiallelics])
	SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.individuals, 0, T).genomes.outputVCF(); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.individuals, 100, T).genomes.outputVCF(); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.individuals, 0, T).genomes.outputVCF(NULL); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.individuals, 100, T).genomes.outputVCF(NULL); stop(); }", __LINE__);
	if (Eidos_SlashTmpExists())
	{
		SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.individuals, 0, T).genomes.outputVCF('" + temp_path + "/slimOutputVCFTest1.txt'); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.individuals, 100, T).genomes.outputVCF('" + temp_path + "/slimOutputVCFTest2.txt'); stop(); }", __LINE__);
	}
	SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.individuals, 0, T).genomes.outputVCF(NULL, F); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.individuals, 100, T).genomes.outputVCF(NULL, F); stop(); }", __LINE__);
	if (Eidos_SlashTmpExists())
	{
		SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.individuals, 0, T).genomes.outputVCF('" + temp_path + "/slimOutputVCFTest3.txt', F); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.individuals, 100, T).genomes.outputVCF('" + temp_path + "/slimOutputVCFTest4.txt', F); stop(); }", __LINE__);
	}
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "10 late() { sample(p1.individuals, 0, T).genomes.outputVCF(NULL); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "10 late() { sample(p1.individuals, 100, T).genomes.outputVCF(NULL); stop(); }", __LINE__);
	if (Eidos_SlashTmpExists())
	{
		SLiMAssertScriptStop(gen1_setup_sex_p1 + "10 late() { sample(p1.individuals, 0, T).genomes.outputVCF('" + temp_path + "/slimOutputVCFTest5.txt'); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_sex_p1 + "10 late() { sample(p1.individuals, 100, T).genomes.outputVCF('" + temp_path + "/slimOutputVCFTest6.txt'); stop(); }", __LINE__);
	}
		SLiMAssertScriptStop(gen1_setup_sex_p1 + "10 late() { sample(p1.individuals, 0, T).genomes.outputVCF(NULL, F); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_sex_p1 + "10 late() { sample(p1.individuals, 100, T).genomes.outputVCF(NULL, F); stop(); }", __LINE__);
	if (Eidos_SlashTmpExists())
	{
		SLiMAssertScriptStop(gen1_setup_sex_p1 + "10 late() { sample(p1.individuals, 0, T).genomes.outputVCF('" + temp_path + "/slimOutputVCFTest7.txt', F); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_sex_p1 + "10 late() { sample(p1.individuals, 100, T).genomes.outputVCF('" + temp_path + "/slimOutputVCFTest8.txt', F); stop(); }", __LINE__);
	}
}






























