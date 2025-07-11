//
//  slim_test_genetics.cpp
//  SLiM
//
//  Created by Ben Haller on 7/11/20.
//  Copyright (c) 2020-2025 Benjamin C. Haller.  All rights reserved.
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
	SLiMAssertScriptStop(gen1_setup + "1 early() { if (m1.color == '') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { if (m1.colorSubstitution == '') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { if (m1.convertToSubstitution == T) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { if (m1.mutationStackGroup == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { if (m1.mutationStackPolicy == 's') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { if (m1.distributionParams == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { if (m1.distributionType == 'f') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { if (m1.dominanceCoeff == 0.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { if (m1.id == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { m1.color = ''; } 2 early() { if (m1.color == '') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { m1.color = 'red'; } 2 early() { if (m1.color == 'red') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { m1.color = '#FF0000'; } 2 early() { if (m1.color == '#FF0000') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { m1.colorSubstitution = ''; } 2 early() { if (m1.colorSubstitution == '') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { m1.colorSubstitution = 'red'; } 2 early() { if (m1.colorSubstitution == 'red') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { m1.colorSubstitution = '#FF0000'; } 2 early() { if (m1.colorSubstitution == '#FF0000') stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.tag; }", "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { c(m1,m1).tag; }", "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { m1.tag = 17; } 2 early() { if (m1.tag == 17) stop(); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 early() { m1.convertToSubstitution = F; }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 early() { m1.mutationStackGroup = -17; }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 early() { m1.mutationStackPolicy = 's'; }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 early() { m1.mutationStackPolicy = 'f'; }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 early() { m1.mutationStackPolicy = 'l'; }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.mutationStackPolicy = 'z'; }", "property mutationStackPolicy must be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.distributionParams = 0.1; }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.distributionType = 'g'; }", "read-only property", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 early() { m1.dominanceCoeff = 0.3; }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.id = 2; }", "read-only property", __LINE__);

	SLiMAssertScriptStop(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); c(m1,m2).mutationStackGroup = 3; c(m1,m2).mutationStackPolicy = 'f'; } 1 early() { stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); c(m1,m2).mutationStackGroup = 3; m1.mutationStackPolicy = 'f'; m2.mutationStackPolicy = 'l'; } 1 early() { stop(); }", "inconsistent mutationStackPolicy", __LINE__, false);
	SLiMAssertScriptRaise(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); c(m1,m2).mutationStackGroup = 3; c(m1,m2).mutationStackPolicy = 'f'; } 1 early() { m2.mutationStackPolicy = 'l'; }", "inconsistent mutationStackPolicy", __LINE__, false);
	SLiMAssertScriptStop(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); m1.mutationStackPolicy = 'f'; m2.mutationStackPolicy = 'l'; } 1 early() { stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); m1.mutationStackPolicy = 'f'; m2.mutationStackPolicy = 'l'; } 1 early() { c(m1,m2).mutationStackGroup = 3; }", "inconsistent mutationStackPolicy", __LINE__, false);
	
	// Test MutationType - (void)setDistribution(string$ distributionType, ...)
	SLiMAssertScriptStop(gen1_setup + "1 early() { m1.setDistribution('f', 2.2); if (m1.distributionType == 'f' & m1.distributionParams == 2.2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { m1.setDistribution('g', 3.1, 7.5); if (m1.distributionType == 'g' & identical(m1.distributionParams, c(3.1, 7.5))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { m1.setDistribution('e', -3); if (m1.distributionType == 'e' & m1.distributionParams == -3) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { m1.setDistribution('n', 3.1, 7.5); if (m1.distributionType == 'n' & identical(m1.distributionParams, c(3.1, 7.5))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { m1.setDistribution('p', 3.1, 7.5); if (m1.distributionType == 'p' & identical(m1.distributionParams, c(3.1, 7.5))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { m1.setDistribution('w', 3.1, 7.5); if (m1.distributionType == 'w' & identical(m1.distributionParams, c(3.1, 7.5))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { m1.setDistribution('s', 'return 1;'); if (m1.distributionType == 's' & identical(m1.distributionParams, 'return 1;')) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setDistribution('x', 1.5); stop(); }", "must be 'f', 'g', 'e', 'n', 'w', or 's'", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setDistribution('f', 'foo'); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setDistribution('g', 'foo', 7.5); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setDistribution('g', 3.1, 'foo'); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setDistribution('e', 'foo'); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setDistribution('n', 'foo', 7.5); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setDistribution('n', 3.1, 'foo'); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setDistribution('p', 'foo', 7.5); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setDistribution('p', 3.1, 'foo'); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setDistribution('w', 'foo', 7.5); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setDistribution('w', 3.1, 'foo'); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setDistribution('s', 3); stop(); }", "must be of type string", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setDistribution('f', '1'); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setDistribution('g', '1', 7.5); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setDistribution('g', 3.1, '1'); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setDistribution('e', '1'); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setDistribution('n', '1', 7.5); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setDistribution('n', 3.1, '1'); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setDistribution('p', '1', 7.5); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setDistribution('p', 3.1, '1'); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setDistribution('w', '1', 7.5); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setDistribution('w', 3.1, '1'); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setDistribution('s', 3.1); stop(); }", "must be of type string", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setDistribution('f', T); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setDistribution('g', T, 7.5); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setDistribution('g', 3.1, T); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setDistribution('e', T); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setDistribution('n', T, 7.5); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setDistribution('n', 3.1, T); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setDistribution('p', T, 7.5); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setDistribution('p', 3.1, T); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setDistribution('w', T, 7.5); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setDistribution('w', 3.1, T); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setDistribution('s', T); stop(); }", "must be of type string", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setDistribution('g', 3.1, 0.0); }", "must have a shape parameter > 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setDistribution('g', 3.1, -1.0); }", "must have a shape parameter > 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setDistribution('n', 3.1, -1.0); }", "must have a standard deviation parameter >= 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setDistribution('p', 3.1, 0.0); }", "must have a scale parameter > 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setDistribution('p', 3.1, -1.0); }", "must have a scale parameter > 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setDistribution('w', 0.0, 7.5); }", "must have a scale parameter > 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setDistribution('w', -1.0, 7.5); }", "must have a scale parameter > 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setDistribution('w', 3.1, 0.0); }", "must have a shape parameter > 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setDistribution('w', 3.1, -7.5); }", "must have a shape parameter > 0", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 early() { m1.setDistribution('s', 'return foo;'); } 100 early() { stop(); }", "undefined identifier foo", __LINE__, false);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 early() { m1.setDistribution('s', 'x >< 5;'); } 100 early() { stop(); }", "tokenize/parse error in type 's' DFE callback script", __LINE__, false);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 early() { m1.setDistribution('s', 'x $ 5;'); } 100 early() { stop(); }", "tokenize/parse error in type 's' DFE callback script", __LINE__, false);
	
	// Test MutationType - (float)drawSelectionCoefficient([integer$ n = 1])
	// the parameters here are chosen so that these tests should fail extremely rarely
	SLiMAssertScriptStop(gen1_setup + "1 early() { m1.setDistribution('f', 2.2); if (m1.drawSelectionCoefficient() == 2.2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { m1.setDistribution('f', 2.2); if (identical(m1.drawSelectionCoefficient(10), rep(2.2, 10))) stop(); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 early() { m1.setDistribution('g', 3.1, 7.5); m1.drawSelectionCoefficient(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { m1.setDistribution('g', 3.1, 7.5); if (abs(mean(m1.drawSelectionCoefficient(5000)) - 3.1) < 0.1) stop(); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 early() { m1.setDistribution('e', -3.0); m1.drawSelectionCoefficient(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { m1.setDistribution('e', -3.0); if (abs(mean(m1.drawSelectionCoefficient(30000)) + 3.0) < 0.1) stop(); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 early() { m1.setDistribution('n', 3.1, 0.5); m1.drawSelectionCoefficient(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { m1.setDistribution('n', 3.1, 0.5); if (abs(mean(m1.drawSelectionCoefficient(2000)) - 3.1) < 0.1) stop(); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 early() { m1.setDistribution('p', 3.1, 7.5); m1.drawSelectionCoefficient(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { m1.setDistribution('p', 3.1, 0.01); if (abs(mean(m1.drawSelectionCoefficient(2000)) - 3.1) < 0.1) stop(); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 early() { m1.setDistribution('w', 3.1, 7.5); m1.drawSelectionCoefficient(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { m1.setDistribution('w', 3.1, 7.5); if (abs(mean(m1.drawSelectionCoefficient(2000)) - 2.910106) < 0.1) stop(); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 early() { m1.setDistribution('s', 'rbinom(1, 4, 0.5);'); m1.drawSelectionCoefficient(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { m1.setDistribution('s', 'rbinom(1, 4, 0.5);'); if (abs(mean(m1.drawSelectionCoefficient(5000)) - 2.0) < 0.1) stop(); }", __LINE__);
}

#pragma mark GenomicElementType tests
void _RunGenomicElementTypeTests(void)
{
	// ************************************************************************************
	//
	//	Gen 1+ tests: GenomicElementType
	//
	
	// Test GenomicElementType properties
	SLiMAssertScriptStop(gen1_setup + "1 early() { if (g1.color == '') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { if (g1.id == 1) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { g1.id = 2; }", "read-only property", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { if (g1.mutationFractions == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { if (g1.mutationTypes == m1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { g1.color = ''; } 2 early() { if (g1.color == '') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { g1.color = 'red'; } 2 early() { if (g1.color == 'red') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { g1.color = '#FF0000'; } 2 early() { if (g1.color == '#FF0000') stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { g1.tag; }", "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { c(g1,g1).tag; }", "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { g1.tag = 17; } 2 early() { if (g1.tag == 17) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { g1.mutationFractions = 1.0; }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { g1.mutationTypes = m1; }", "read-only property", __LINE__);
	
	// Test GenomicElementType - (void)setMutationFractions(io<MutationType> mutationTypes, numeric proportions)
	SLiMAssertScriptStop(gen1_setup + "1 early() { g1.setMutationFractions(object(), integer(0)); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { g1.setMutationFractions(m1, 0.0); if (g1.mutationTypes == m1 & g1.mutationFractions == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { g1.setMutationFractions(1, 0.0); if (g1.mutationTypes == m1 & g1.mutationFractions == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { g1.setMutationFractions(m1, 0.3); if (g1.mutationTypes == m1 & g1.mutationFractions == 0.3) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { g1.setMutationFractions(1, 0.3); if (g1.mutationTypes == m1 & g1.mutationFractions == 0.3) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); } 1 early() { g1.setMutationFractions(c(m1,m2), c(0.3, 0.7)); if (identical(g1.mutationTypes, c(m1,m2)) & identical(g1.mutationFractions, c(0.3,0.7))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); } 1 early() { g1.setMutationFractions(c(1,2), c(0.3, 0.7)); if (identical(g1.mutationTypes, c(m1,m2)) & identical(g1.mutationFractions, c(0.3,0.7))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); } 1 early() { g1.setMutationFractions(c(m1,m2)); stop(); }", "missing required argument", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); } 1 early() { g1.setMutationFractions(c(m1,m2), 0.3); stop(); }", "requires the sizes", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); } 1 early() { g1.setMutationFractions(c(m1,m2), c(-1, 2)); stop(); }", "must be greater than or equal to zero", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); } 1 early() { g1.setMutationFractions(c(2,3), c(1, 2)); stop(); }", "not defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); } 1 early() { g1.setMutationFractions(c(m2,m2), c(1, 2)); stop(); }", "used more than once", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); } 1 early() { g1.setMutationFractions(c(2,2), c(1, 2)); stop(); }", "used more than once", __LINE__);
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
	SLiMAssertScriptStop(gen1_setup_2ge + "1 early() { ge = sim.chromosomes.genomicElements[0]; if (ge.endPosition == 999) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_2ge + "1 early() { ge = sim.chromosomes.genomicElements[0]; if (ge.startPosition == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_2ge + "1 early() { ge = sim.chromosomes.genomicElements[0]; if (ge.genomicElementType == g1) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 early() { ge = sim.chromosomes.genomicElements[0]; ge.tag; }", "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup_2ge + "1 early() { ge = sim.chromosomes.genomicElements[0]; ge.tag = -12; if (ge.tag == -12) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 early() { ge = sim.chromosomes.genomicElements[0]; ge.endPosition = 999; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 early() { ge = sim.chromosomes.genomicElements[0]; ge.startPosition = 0; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 early() { ge = sim.chromosomes.genomicElements[0]; ge.genomicElementType = g1; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptStop(gen1_setup_2ge + "1 early() { ge = sim.chromosomes.genomicElements[1]; if (ge.endPosition == 99999) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_2ge + "1 early() { ge = sim.chromosomes.genomicElements[1]; if (ge.startPosition == 1000) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_2ge + "1 early() { ge = sim.chromosomes.genomicElements[1]; if (ge.genomicElementType == g1) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 early() { ge = sim.chromosomes.genomicElements[1]; ge.tag; }", "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup_2ge + "1 early() { ge = sim.chromosomes.genomicElements[1]; ge.tag = -17; if (ge.tag == -17) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 early() { ge = sim.chromosomes.genomicElements[1]; ge.endPosition = 99999; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 early() { ge = sim.chromosomes.genomicElements[1]; ge.startPosition = 1000; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 early() { ge = sim.chromosomes.genomicElements[1]; ge.genomicElementType = g1; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 early() { ge = sim.chromosomes.genomicElements; ge.tag; }", "before being set", __LINE__);
	
	// Test GenomicElement - (void)setGenomicElementType(io<GenomicElementType>$ genomicElementType)
	SLiMAssertScriptStop(gen1_setup_2ge + "1 early() { ge = sim.chromosomes.genomicElements[0]; ge.setGenomicElementType(g1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_2ge + "1 early() { ge = sim.chromosomes.genomicElements[0]; ge.setGenomicElementType(1); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 early() { ge = sim.chromosomes.genomicElements[0]; ge.setGenomicElementType(); stop(); }", "missing required argument", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 early() { ge = sim.chromosomes.genomicElements[0]; ge.setGenomicElementType(object()); stop(); }", "must be a singleton", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 early() { ge = sim.chromosomes.genomicElements[0]; ge.setGenomicElementType(2); stop(); }", "not defined", __LINE__);
	
	// Test GenomicElement position testing
	SLiMAssertScriptStop(gen1_setup_2ge + "initialize() { initializeGenomicElement(g1, 100000, 100000); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_2ge + "initialize() { initializeGenomicElement(g1, 99999, 100000); stop(); }", "overlaps existing genomic element", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_2ge + "initialize() { initializeGenomicElement(g1, -2, -1); stop(); }", "chromosome position or length is out of range", __LINE__);
}

#pragma mark Chromosome tests
void _RunChromosomeTests(void)
{
	// ************************************************************************************
	//
	//	Gen 1+ tests: Chromosome
	//
	
	// Test Chromosome properties
	SLiMAssertScriptStop(gen1_setup + "1 early() { ch = sim.chromosomes; if (ch.colorSubstitution == '#3333FF') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { ch = sim.chromosomes; if (ch.geneConversionEnabled == F) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; if (ch.geneConversionGCBias == 0.0) stop(); }", "not defined since the DSB", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; if (ch.geneConversionNonCrossoverFraction == 0.0) stop(); }", "not defined since the DSB", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; if (ch.geneConversionMeanLength == 0.0) stop(); }", "not defined since the DSB", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; if (ch.geneConversionSimpleConversionFraction == 0.0) stop(); }", "not defined since the DSB", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { ch = sim.chromosomes; if (ch.genomicElements[0].genomicElementType == g1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { ch = sim.chromosomes; if (ch.lastPosition == 99999) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { ch = sim.chromosomes; if (ch.overallRecombinationRate == 1e-8 * 99999) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; if (isNULL(ch.overallRecombinationRateM)) stop(); }", "sex-specific recombination rate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; if (isNULL(ch.overallRecombinationRateF)) stop(); }", "sex-specific recombination rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { ch = sim.chromosomes; if (ch.recombinationEndPositions == 99999) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; if (isNULL(ch.recombinationEndPositionsM)) stop(); }", "sex-specific recombination rate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; if (isNULL(ch.recombinationEndPositionsF)) stop(); }", "sex-specific recombination rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { ch = sim.chromosomes; if (ch.recombinationRates == 1e-8) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; if (isNULL(ch.recombinationRatesM)) stop(); }", "sex-specific recombination rate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; if (isNULL(ch.recombinationRatesF)) stop(); }", "sex-specific recombination rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { ch = sim.chromosomes; if (ch.overallMutationRate == 1e-7 * 100000) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; if (isNULL(ch.overallMutationRateM)) stop(); }", "sex-specific mutation rate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; if (isNULL(ch.overallMutationRateF)) stop(); }", "sex-specific mutation rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { ch = sim.chromosomes; if (ch.mutationEndPositions == 99999) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; if (isNULL(ch.mutationEndPositionsM)) stop(); }", "sex-specific mutation rate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; if (isNULL(ch.mutationEndPositionsF)) stop(); }", "sex-specific mutation rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { ch = sim.chromosomes; if (ch.mutationRates == 1e-7) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; if (isNULL(ch.mutationRatesM)) stop(); }", "sex-specific mutation rate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; if (isNULL(ch.mutationRatesF)) stop(); }", "sex-specific mutation rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { ch = sim.chromosomes; ch.colorSubstitution = ''; if (ch.colorSubstitution == '') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { ch = sim.chromosomes; ch.colorSubstitution = 'red'; if (ch.colorSubstitution == 'red') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { ch = sim.chromosomes; ch.colorSubstitution = '#FF0000'; if (ch.colorSubstitution == '#FF0000') stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.tag; }", "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; c(ch,ch).tag; }", "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { ch = sim.chromosomes; ch.tag = 3294; if (ch.tag == 3294) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.genomicElements = ch.genomicElements; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.lastPosition = 99999; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.overallRecombinationRate = 1e-2; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.overallRecombinationRateM = 1e-2; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.overallRecombinationRateF = 1e-2; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.recombinationEndPositions = 99999; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.recombinationEndPositionsM = 99999; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.recombinationEndPositionsF = 99999; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.recombinationRates = 1e-8; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.recombinationRatesM = 1e-8; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.recombinationRatesF = 1e-8; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.overallMutationRate = 1e-2; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.overallMutationRateM = 1e-2; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.overallMutationRateF = 1e-2; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.mutationEndPositions = 99999; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.mutationEndPositionsM = 99999; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.mutationEndPositionsF = 99999; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.mutationRates = 1e-8; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.mutationRatesM = 1e-8; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.mutationRatesF = 1e-8; stop(); }", "read-only property", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex + "1 early() { ch = sim.chromosomes; if (ch.colorSubstitution == '#3333FF') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 early() { ch = sim.chromosomes; if (ch.geneConversionEnabled == F) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; if (ch.geneConversionGCBias == 0.0) stop(); }", "not defined since the DSB", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; if (ch.geneConversionNonCrossoverFraction == 0.0) stop(); }", "not defined since the DSB", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; if (ch.geneConversionMeanLength == 0.0) stop(); }", "not defined since the DSB", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; if (ch.geneConversionSimpleConversionFraction == 0.0) stop(); }", "not defined since the DSB", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 early() { ch = sim.chromosomes; if (ch.genomicElements[0].genomicElementType == g1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 early() { ch = sim.chromosomes; if (ch.lastPosition == 99999) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 early() { ch = sim.chromosomes; if (ch.overallRecombinationRate == 1e-8 * 99999) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; if (isNULL(ch.overallRecombinationRateM)) stop(); }", "sex-specific recombination rate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; if (isNULL(ch.overallRecombinationRateF)) stop(); }", "sex-specific recombination rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 early() { ch = sim.chromosomes; if (ch.recombinationEndPositions == 99999) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; if (isNULL(ch.recombinationEndPositionsM)) stop(); }", "sex-specific recombination rate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; if (isNULL(ch.recombinationEndPositionsF)) stop(); }", "sex-specific recombination rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 early() { ch = sim.chromosomes; if (ch.recombinationRates == 1e-8) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; if (isNULL(ch.recombinationRatesM)) stop(); }", "sex-specific recombination rate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; if (isNULL(ch.recombinationRatesF)) stop(); }", "sex-specific recombination rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 early() { ch = sim.chromosomes; if (ch.overallMutationRate == 1e-7 * 100000) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; if (isNULL(ch.overallMutationRateM)) stop(); }", "sex-specific mutation rate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; if (isNULL(ch.overallMutationRateF)) stop(); }", "sex-specific mutation rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 early() { ch = sim.chromosomes; if (ch.mutationEndPositions == 99999) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; if (isNULL(ch.mutationEndPositionsM)) stop(); }", "sex-specific mutation rate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; if (isNULL(ch.mutationEndPositionsF)) stop(); }", "sex-specific mutation rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 early() { ch = sim.chromosomes; if (ch.mutationRates == 1e-7) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; if (isNULL(ch.mutationRatesM)) stop(); }", "sex-specific mutation rate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; if (isNULL(ch.mutationRatesF)) stop(); }", "sex-specific mutation rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.colorSubstitution = ''; if (ch.colorSubstitution == '') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.colorSubstitution = 'red'; if (ch.colorSubstitution == 'red') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.colorSubstitution = '#FF0000'; if (ch.colorSubstitution == '#FF0000') stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.tag; }", "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; c(ch,ch).tag; }", "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.tag = 3294; if (ch.tag == 3294) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.genomicElements = ch.genomicElements; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.lastPosition = 99999; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.overallRecombinationRate = 1e-2; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.overallRecombinationRateM = 1e-2; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.overallRecombinationRateF = 1e-2; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.recombinationEndPositions = 99999; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.recombinationEndPositionsM = 99999; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.recombinationEndPositionsF = 99999; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.recombinationRates = 1e-8; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.recombinationRatesM = 1e-8; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.recombinationRatesF = 1e-8; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.overallMutationRate = 1e-2; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.overallMutationRateM = 1e-2; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.overallMutationRateF = 1e-2; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.mutationEndPositions = 99999; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.mutationEndPositionsM = 99999; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.mutationEndPositionsF = 99999; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.mutationRates = 1e-8; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.mutationRatesM = 1e-8; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.mutationRatesF = 1e-8; stop(); }", "read-only property", __LINE__);
	
	std::string gen1_setup_sex_2rates("initialize() { initializeSex('X'); initializeMutationRate(1e-7, sex='M'); initializeMutationRate(1e-8, sex='F'); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8, 99999, 'M'); initializeRecombinationRate(1e-7, 99999, 'F'); } ");
	
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; if (ch.colorSubstitution == '#3333FF') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; if (ch.geneConversionEnabled == F) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; if (ch.geneConversionGCBias == 0.0) stop(); }", "not defined since the DSB", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; if (ch.geneConversionNonCrossoverFraction == 0.0) stop(); }", "not defined since the DSB", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; if (ch.geneConversionMeanLength == 0.0) stop(); }", "not defined since the DSB", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; if (ch.geneConversionSimpleConversionFraction == 0.0) stop(); }", "not defined since the DSB", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; if (ch.genomicElements[0].genomicElementType == g1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; if (ch.lastPosition == 99999) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; if (isNULL(ch.overallRecombinationRate)) stop(); }", "sex-specific recombination rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; if (ch.overallRecombinationRateM == 1e-8 * 99999) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; if (ch.overallRecombinationRateF == 1e-7 * 99999) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; if (isNULL(ch.recombinationEndPositions)) stop(); }", "sex-specific recombination rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; if (ch.recombinationEndPositionsM == 99999) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; if (ch.recombinationEndPositionsF == 99999) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; if (isNULL(ch.recombinationRates)) stop(); }", "sex-specific recombination rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; if (ch.recombinationRatesM == 1e-8) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; if (ch.recombinationRatesF == 1e-7) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; if (isNULL(ch.overallMutationRate)) stop(); }", "sex-specific mutation rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; if (ch.overallMutationRateM == 1e-7 * 100000) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; if (ch.overallMutationRateF == 1e-8 * 100000) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; if (isNULL(ch.mutationEndPositions)) stop(); }", "sex-specific mutation rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; if (ch.mutationEndPositionsM == 99999) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; if (ch.mutationEndPositionsF == 99999) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; if (isNULL(ch.mutationRates)) stop(); }", "sex-specific mutation rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; if (ch.mutationRatesM == 1e-7) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; if (ch.mutationRatesF == 1e-8) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.colorSubstitution = ''; if (ch.colorSubstitution == '') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.colorSubstitution = 'red'; if (ch.colorSubstitution == 'red') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.colorSubstitution = '#FF0000'; if (ch.colorSubstitution == '#FF0000') stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.tag; }", "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; c(ch,ch).tag; }", "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.tag = 3294; if (ch.tag == 3294) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.genomicElements = ch.genomicElements; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.lastPosition = 99999; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.overallRecombinationRate = 1e-2; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.overallRecombinationRateM = 1e-2; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.overallRecombinationRateF = 1e-2; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.recombinationEndPositions = 99999; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.recombinationEndPositionsM = 99999; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.recombinationEndPositionsF = 99999; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.recombinationRates = 1e-8; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.recombinationRatesM = 1e-8; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.recombinationRatesF = 1e-8; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.overallMutationRate = 1e-2; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.overallMutationRateM = 1e-2; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.overallMutationRateF = 1e-2; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.mutationEndPositions = 99999; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.mutationEndPositionsM = 99999; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.mutationEndPositionsF = 99999; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.mutationRates = 1e-8; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.mutationRatesM = 1e-8; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.mutationRatesF = 1e-8; stop(); }", "read-only property", __LINE__);
	
	// Test Chromosome - (void)setMutationRate(numeric rates, [integer ends])
	SLiMAssertScriptStop(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setMutationRate(0.0); stop(); }", __LINE__);														// legal: singleton rate, no end
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setMutationRate(); stop(); }", "missing required argument", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setMutationRate(-0.00001); stop(); }", "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setMutationRate(10000); stop(); }", "rate is >= 1.0", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, 0.1), c(1000, 99999)); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, 0.001), c(1000, 99999)); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, 0.1)); stop(); }", "to be a singleton if", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setMutationRate(integer(0), integer(0)); stop(); }", "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, 0.1), 99999); stop(); }", "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, 0.1), 99997:99999); stop(); }", "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, 0.1), c(99999, 1000)); stop(); }", "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, 0.1), c(99999, 99999)); stop(); }", "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, -0.001), c(1000, 99999)); stop(); }", "must be >= 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, -0.001), c(1000, 2000)); stop(); }", "must be >= 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, -0.001), c(1000, 100000)); stop(); }", "must be >= 0", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, 0.1), c(1000, 99999), '*'); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, 0.001), c(1000, 99999), '*'); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setMutationRate(integer(0), integer(0), '*'); stop(); }", "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, 0.1), 99999, '*'); stop(); }", "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, 0.1), 99997:99999, '*'); stop(); }", "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, 0.1), c(99999, 1000), '*'); stop(); }", "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, 0.1), c(99999, 99999), '*'); stop(); }", "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, -0.001), c(1000, 99999), '*'); stop(); }", "must be >= 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, -0.001), c(1000, 2000), '*'); stop(); }", "must be >= 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, -0.001), c(1000, 100000), '*'); stop(); }", "must be >= 0", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setMutationRate(0.0); stop(); }", __LINE__);														// legal: singleton rate, no end
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setMutationRate(); stop(); }", "missing required argument", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setMutationRate(-0.00001); stop(); }", "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setMutationRate(10000); stop(); }", "rate is >= 1.0", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, 0.1), c(1000, 99999)); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, 0.001), c(1000, 99999)); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, 0.1)); stop(); }", "to be a singleton if", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setMutationRate(integer(0), integer(0)); stop(); }", "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, 0.1), 99999); stop(); }", "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, 0.1), 99997:99999); stop(); }", "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, 0.1), c(99999, 1000)); stop(); }", "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, 0.1), c(99999, 99999)); stop(); }", "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, -0.001), c(1000, 99999)); stop(); }", "must be >= 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, -0.001), c(1000, 2000)); stop(); }", "must be >= 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, -0.001), c(1000, 100000)); stop(); }", "must be >= 0", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, 0.1), c(1000, 99999), '*'); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, 0.001), c(1000, 99999), '*'); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setMutationRate(integer(0), integer(0), '*'); stop(); }", "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, 0.1), 99999, '*'); stop(); }", "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, 0.1), 99997:99999, '*'); stop(); }", "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, 0.1), c(99999, 1000), '*'); stop(); }", "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, 0.1), c(99999, 99999), '*'); stop(); }", "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, -0.001), c(1000, 99999), '*'); stop(); }", "must be >= 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, -0.001), c(1000, 2000), '*'); stop(); }", "must be >= 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, -0.001), c(1000, 100000), '*'); stop(); }", "must be >= 0", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, 0.1), c(1000, 99999), 'M'); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setMutationRate(integer(0), integer(0), 'M'); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, 0.1), 99999, 'M'); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, 0.1), 99997:99999, 'M'); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, 0.1), c(99999, 1000), 'M'); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, 0.1), c(99999, 99999), 'M'); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, -0.001), c(1000, 99999), 'M'); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, -0.001), c(1000, 2000), 'M'); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, -0.001), c(1000, 100000), 'M'); stop(); }", "single map versus separate maps", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setMutationRate(0.0); stop(); }", "single map versus separate maps", __LINE__);														// legal: singleton rate, no end
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setMutationRate(); stop(); }", "missing required argument", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setMutationRate(-0.00001); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setMutationRate(10000); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, 0.1), c(1000, 99999)); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, 0.001), c(1000, 99999)); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, 0.1)); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setMutationRate(integer(0), integer(0)); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, 0.1), 99999); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, 0.1), 99997:99999); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, 0.1), c(99999, 1000)); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, 0.1), c(99999, 99999)); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, -0.001), c(1000, 99999)); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, -0.001), c(1000, 2000)); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, -0.001), c(1000, 100000)); stop(); }", "single map versus separate maps", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, 0.1), c(1000, 99999), '*'); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, 0.001), c(1000, 99999), '*'); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setMutationRate(integer(0), integer(0), '*'); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, 0.1), 99999, '*'); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, 0.1), 99997:99999, '*'); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, 0.1), c(99999, 1000), '*'); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, 0.1), c(99999, 99999), '*'); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, -0.001), c(1000, 99999), '*'); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, -0.001), c(1000, 2000), '*'); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, -0.001), c(1000, 100000), '*'); stop(); }", "single map versus separate maps", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, 0.1), c(1000, 99999), 'M'); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, 0.001), c(1000, 99999), 'M'); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setMutationRate(integer(0), integer(0), 'M'); stop(); }", "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, 0.1), 99999, 'M'); stop(); }", "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, 0.1), 99997:99999, 'M'); stop(); }", "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, 0.1), c(99999, 1000), 'M'); stop(); }", "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, 0.1), c(99999, 99999), 'M'); stop(); }", "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, -0.001), c(1000, 99999), 'M'); stop(); }", "must be >= 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, -0.001), c(1000, 2000), 'M'); stop(); }", "must be >= 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setMutationRate(c(0.0, -0.001), c(1000, 100000), 'M'); stop(); }", "must be >= 0", __LINE__);
	
	// Test Chromosome - (void)setRecombinationRate(numeric rates, [integer ends])
	SLiMAssertScriptStop(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(0.0); stop(); }", __LINE__);														// legal: singleton rate, no end
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(); stop(); }", "missing required argument", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(-0.00001); stop(); }", "out of range", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(0.5); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(0.6); stop(); }", "rates must be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(10000); stop(); }", "rates must be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, 0.1), c(1000, 99999)); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, 0.001), c(1000, 99999)); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, 0.1)); stop(); }", "to be a singleton if", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(integer(0), integer(0)); stop(); }", "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, 0.1), 99999); stop(); }", "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, 0.1), 99997:99999); stop(); }", "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 1000)); stop(); }", "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 99999)); stop(); }", "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 99999)); stop(); }", "rates must be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 2000)); stop(); }", "rates must be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 100000)); stop(); }", "rates must be in [0.0, 0.5]", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, 0.1), c(1000, 99999), '*'); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, 0.001), c(1000, 99999), '*'); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(integer(0), integer(0), '*'); stop(); }", "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, 0.1), 99999, '*'); stop(); }", "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, 0.1), 99997:99999, '*'); stop(); }", "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 1000), '*'); stop(); }", "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 99999), '*'); stop(); }", "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 99999), '*'); stop(); }", "rates must be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 2000), '*'); stop(); }", "rates must be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 100000), '*'); stop(); }", "rates must be in [0.0, 0.5]", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(0.0); stop(); }", __LINE__);														// legal: singleton rate, no end
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(); stop(); }", "missing required argument", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(-0.00001); stop(); }", "out of range", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(0.5); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(0.6); stop(); }", "rates must be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(10000); stop(); }", "rates must be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, 0.1), c(1000, 99999)); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, 0.001), c(1000, 99999)); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, 0.1)); stop(); }", "to be a singleton if", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(integer(0), integer(0)); stop(); }", "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, 0.1), 99999); stop(); }", "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, 0.1), 99997:99999); stop(); }", "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 1000)); stop(); }", "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 99999)); stop(); }", "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 99999)); stop(); }", "rates must be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 2000)); stop(); }", "rates must be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 100000)); stop(); }", "rates must be in [0.0, 0.5]", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, 0.1), c(1000, 99999), '*'); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, 0.001), c(1000, 99999), '*'); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(integer(0), integer(0), '*'); stop(); }", "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, 0.1), 99999, '*'); stop(); }", "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, 0.1), 99997:99999, '*'); stop(); }", "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 1000), '*'); stop(); }", "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 99999), '*'); stop(); }", "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 99999), '*'); stop(); }", "rates must be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 2000), '*'); stop(); }", "rates must be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 100000), '*'); stop(); }", "rates must be in [0.0, 0.5]", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, 0.1), c(1000, 99999), 'M'); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(integer(0), integer(0), 'M'); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, 0.1), 99999, 'M'); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, 0.1), 99997:99999, 'M'); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 1000), 'M'); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 99999), 'M'); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 99999), 'M'); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 2000), 'M'); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 100000), 'M'); stop(); }", "single map versus separate maps", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(0.0); stop(); }", "single map versus separate maps", __LINE__);														// legal: singleton rate, no end
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(); stop(); }", "missing required argument", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(-0.00001); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(10000); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, 0.1), c(1000, 99999)); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, 0.001), c(1000, 99999)); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, 0.1)); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(integer(0), integer(0)); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, 0.1), 99999); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, 0.1), 99997:99999); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 1000)); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 99999)); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 99999)); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 2000)); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 100000)); stop(); }", "single map versus separate maps", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, 0.1), c(1000, 99999), '*'); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, 0.001), c(1000, 99999), '*'); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(integer(0), integer(0), '*'); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, 0.1), 99999, '*'); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, 0.1), 99997:99999, '*'); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 1000), '*'); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 99999), '*'); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 99999), '*'); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 2000), '*'); stop(); }", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 100000), '*'); stop(); }", "single map versus separate maps", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, 0.1), c(1000, 99999), 'M'); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, 0.001), c(1000, 99999), 'M'); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(integer(0), integer(0), 'M'); stop(); }", "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, 0.1), 99999, 'M'); stop(); }", "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, 0.1), 99997:99999, 'M'); stop(); }", "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 1000), 'M'); stop(); }", "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 99999), 'M'); stop(); }", "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 99999), 'M'); stop(); }", "rates must be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 2000), 'M'); stop(); }", "rates must be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 early() { ch = sim.chromosomes; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 100000), 'M'); stop(); }", "rates must be in [0.0, 0.5]", __LINE__);
	
	// initializeGeneConversion() tests
	SLiMAssertScriptStop(gen1_setup + "initialize() { initializeGeneConversion(0.2, 1234.5, 0.75); } 1 early() { if (sim.chromosomes.geneConversionEnabled == T) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "initialize() { initializeGeneConversion(0.2, 1234.5, 0.75); } 1 early() { if (sim.chromosomes.geneConversionNonCrossoverFraction == 0.2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "initialize() { initializeGeneConversion(0.2, 1234.5, 0.75); } 1 early() { if (sim.chromosomes.geneConversionMeanLength == 1234.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "initialize() { initializeGeneConversion(0.2, 1234.5, 0.75); } 1 early() { if (sim.chromosomes.geneConversionSimpleConversionFraction == 0.75) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "initialize() { initializeGeneConversion(0.2, 1234.5, 0.75); } 1 early() { if (sim.chromosomes.geneConversionGCBias == 0.0) stop(); }", __LINE__);
	
	// setGeneConversion() tests
	SLiMAssertScriptStop(gen1_setup + "1 early() { sim.chromosomes.setGeneConversion(0.2, 1234.5, 0.75); if (sim.chromosomes.geneConversionEnabled == T) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { sim.chromosomes.setGeneConversion(0.2, 1234.5, 0.75); if (sim.chromosomes.geneConversionNonCrossoverFraction == 0.2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { sim.chromosomes.setGeneConversion(0.2, 1234.5, 0.75); if (sim.chromosomes.geneConversionMeanLength == 1234.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { sim.chromosomes.setGeneConversion(0.2, 1234.5, 0.75); if (sim.chromosomes.geneConversionSimpleConversionFraction == 0.75) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { sim.chromosomes.setGeneConversion(0.2, 1234.5, 0.75); if (sim.chromosomes.geneConversionGCBias == 0.0) stop(); }", __LINE__);
	
	// some basic tests of initializeChromosome(), especially length checks and length determination with and without a length of NULL
	std::string short_init = "initialize() { initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); ";
	
	SLiMAssertScriptRaise(short_init + "initializeChromosome(1, length=1e4); initializeGenomicElement(g1, 0, 1e4); }", "extent of the chromosome", __LINE__);
	SLiMAssertScriptRaise(short_init + "initializeChromosome(1, length=1e4); initializeRecombinationRate(1e-7, 1e4); }", "extent of the chromosome", __LINE__);
	SLiMAssertScriptRaise(short_init + "initializeChromosome(1, length=1e4); initializeMutationRate(1e-7, 1e4); }", "extent of the chromosome", __LINE__);
	
	SLiMAssertScriptStop(short_init + "initializeChromosome(1, length=1e4); initializeGenomicElement(g1, 0, 1e4-1); stop(); }");
	SLiMAssertScriptStop(short_init + "initializeChromosome(1, length=1e4); initializeRecombinationRate(1e-7, 1e4-1); stop(); }");
	SLiMAssertScriptStop(short_init + "initializeChromosome(1, length=1e4); initializeMutationRate(1e-7, 1e4-1); stop(); }");
	
	SLiMAssertScriptStop(short_init + "initializeChromosome(1); initializeGenomicElement(g1, 0, 1e4-1); initializeRecombinationRate(1e-7, 1e4-1); initializeMutationRate(1e-7, 1e4-1); } 1 early() { if (sim.chromosome.length == 1e4) stop(); }");
	SLiMAssertScriptStop(short_init + "initializeChromosome(1); initializeGenomicElement(g1, 0, 1e4-1); initializeRecombinationRate(1e-7, 1e4-1); initializeMutationRate(1e-7, 1e4-1); } 1 early() { if (sim.chromosome.lastPosition == 1e4-1) stop(); }");
	SLiMAssertScriptRaise(short_init + "initializeChromosome(1); initializeGenomicElement(g1, 0, 1e5-1); initializeRecombinationRate(1e-7, 1e5-1); initializeMutationRate(1e-7, 1e4-1); }", "do not cover the full chromosome", __LINE__, false);
	SLiMAssertScriptRaise(short_init + "initializeChromosome(1); initializeGenomicElement(g1, 0, 1e5-1); initializeRecombinationRate(1e-7, 1e4-1); initializeMutationRate(1e-7, 1e5-1); }", "do not cover the full chromosome", __LINE__, false);
	SLiMAssertScriptStop(short_init + "initializeChromosome(1); initializeGenomicElement(g1, 0, 1e4-1); initializeRecombinationRate(1e-7, 1e5-1); initializeMutationRate(1e-7, 1e5-1); } 1 early() { if (sim.chromosome.length == 1e5) stop(); }");
	SLiMAssertScriptStop(short_init + "initializeChromosome(1); initializeGenomicElement(g1, 0, 1e4-1); initializeRecombinationRate(1e-7, 1e5-1); initializeMutationRate(1e-7, 1e5-1); } 1 early() { if (sim.chromosome.lastPosition == 1e5-1) stop(); }");
	SLiMAssertScriptRaise(short_init + "initializeChromosome(1); initializeGenomicElement(g1, 0, 1e4-1); initializeRecombinationRate(1e-7, 1e5-1); initializeMutationRate(1e-7, 1e4-1); }", "do not cover the full chromosome", __LINE__, false);
	SLiMAssertScriptRaise(short_init + "initializeChromosome(1); initializeGenomicElement(g1, 0, 1e4-1); initializeRecombinationRate(1e-7, 1e4-1); initializeMutationRate(1e-7, 1e5-1); }", "do not cover the full chromosome", __LINE__, false);
}

#pragma mark Mutation tests
void _RunMutationTests(void)
{
	// ************************************************************************************
	//
	//	Gen 1+ tests: Mutation
	//
	
	// Test Mutation properties
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 early() { mut = sim.mutations[0]; if (mut.mutationType == m1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 early() { mut = sim.mutations[0]; if ((mut.originTick >= 1) & (mut.originTick < 10)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 early() { mut = sim.mutations[0]; if ((mut.position >= 0) & (mut.position < 100000)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 early() { mut = sim.mutations[0]; if (mut.selectionCoeff == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 early() { mut = sim.mutations[0]; if (mut.subpopID == 1) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 early() { mut = sim.mutations[0]; mut.mutationType = m1; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 early() { mut = sim.mutations[0]; mut.originTick = 1; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 early() { mut = sim.mutations[0]; mut.position = 0; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 early() { mut = sim.mutations[0]; mut.selectionCoeff = 0.1; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 early() { mut = sim.mutations[0]; mut.subpopID = 237; if (mut.subpopID == 237) stop(); }", __LINE__);						// legal; this field may be used as a user tag
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 early() { mut = sim.mutations[0]; mut.tag; }", "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 early() { mut = sim.mutations[0]; c(mut,mut).tag; }", "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 early() { mut = sim.mutations[0]; mut.tag = 278; if (mut.tag == 278) stop(); }", __LINE__);
	
	// Test Mutation - (void)setMutationType(io<MutationType>$ mutType)
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 early() { mut = sim.mutations[0]; mut.setMutationType(m1); if (mut.mutationType == m1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 early() { mut = sim.mutations[0]; mut.setMutationType(m1); if (mut.mutationType == m1) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 early() { mut = sim.mutations[0]; mut.setMutationType(2); if (mut.mutationType == m1) stop(); }", "mutation type m2 not defined", __LINE__);
	
	// Test Mutation - (void)setSelectionCoeff(float$ selectionCoeff)
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 early() { mut = sim.mutations[0]; mut.setSelectionCoeff(0.5); if (mut.selectionCoeff == 0.5) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 early() { mut = sim.mutations[0]; mut.setSelectionCoeff(1); if (mut.selectionCoeff == 1) stop(); }", "cannot be type integer", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 early() { mut = sim.mutations[0]; mut.setSelectionCoeff(-500.0); if (mut.selectionCoeff == -500.0) stop(); }", __LINE__);	// legal; no lower bound
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 early() { mut = sim.mutations[0]; mut.setSelectionCoeff(500.0); if (mut.selectionCoeff == 500.0) stop(); }", __LINE__);		// legal; no upper bound
}

#pragma mark Substitution tests
void _RunSubstitutionTests(void)
{
	// ************************************************************************************
	//
	//	Gen 1+ tests: Substitution
	//
	
	// Test Substitution properties
	SLiMAssertScriptStop(gen1_setup_fixmut_p1 + "30 early() { if (size(sim.substitutions) > 0) stop(); }", __LINE__);										// check that our script generates substitutions fast enough
	SLiMAssertScriptStop(gen1_setup_fixmut_p1 + "30 early() { sub = sim.substitutions[0]; if (sub.fixationTick > 0 & sub.fixationTick <= 30) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_fixmut_p1 + "30 early() { sub = sim.substitutions[0]; if (sub.mutationType == m1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_fixmut_p1 + "30 early() { sub = sim.substitutions[0]; if (sub.originTick > 0 & sub.originTick <= 10) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_fixmut_p1 + "30 early() { sub = sim.substitutions[0]; if (sub.position >= 0 & sub.position <= 99999) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_fixmut_p1 + "30 early() { if (sum(sim.substitutions.selectionCoeff == 500.0) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_fixmut_p1 + "30 early() { sub = sim.substitutions[0]; if (sub.subpopID == 1) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_fixmut_p1 + "30 early() { sub = sim.substitutions[0]; sub.fixationTick = 10; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_fixmut_p1 + "30 early() { sub = sim.substitutions[0]; sub.mutationType = m1; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_fixmut_p1 + "30 early() { sub = sim.substitutions[0]; sub.originTick = 10; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_fixmut_p1 + "30 early() { sub = sim.substitutions[0]; sub.position = 99999; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_fixmut_p1 + "30 early() { sub = sim.substitutions[0]; sub.selectionCoeff = 50.0; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptStop(gen1_setup_fixmut_p1 + "30 early() { sub = sim.substitutions[0]; sub.subpopID = 237; if (sub.subpopID == 237) stop(); }", __LINE__);						// legal; this field may be used as a user tag
}

#pragma mark Haplosome tests
void _RunHaplosomeTests(const std::string &temp_path)
{
	// ************************************************************************************
	//
	//	Gen 1+ tests: Haplosome
	//
	
	// Test Haplosome properties
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; if (gen.chromosome.type == 'A') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[c(0,1)]; if (identical(gen.chromosomeSubposition, c(0,1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; if (gen.isNullHaplosome == F) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 early() { gen = p1.haplosomes[0]; if (gen.mutations[0].mutationType == m1) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; gen.tag; }", "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; c(gen,gen).tag; }", "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; gen.tag = 278; if (gen.tag == 278) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; gen.chromosome.type = 'A'; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; gen.chromosomeSubposition = 0; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; gen.isNullHaplosome = F; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 early() { gen = p1.haplosomes[0]; gen.mutations[0].mutationType = m1; stop(); }", "read-only property", __LINE__);
	
	// Test Haplosome + (void)addMutations(object<Mutation> mutations)
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; gen.addMutations(object()); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 early() { gen = p1.haplosomes[0]; gen.addMutations(gen.mutations[0]); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 early() { gen = p1.haplosomes[0]; gen.addMutations(p1.haplosomes[1].mutations[0]); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 early() { gen = p1.haplosomes[0]; mut = p1.haplosomes[1].mutations[0]; gen.addMutations(rep(mut, 10)); if (sum(gen.mutations == mut) == 1) stop(); }", __LINE__);
	
	// Test Haplosome + (object<Mutation>)addNewDrawnMutation(io<MutationType> mutationType, integer position, [Nio<Subpopulation> originSubpop])
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; mut = gen.addNewDrawnMutation(m1, 5000, p1); p1.haplosomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; mut = gen.addNewDrawnMutation(m1, 5000, 1); p1.haplosomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; mut = gen.addNewDrawnMutation(m1, 5000); p1.haplosomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; mut = gen.addNewDrawnMutation(1, 5000, p1); p1.haplosomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; mut = gen.addNewDrawnMutation(1, 5000, 1); p1.haplosomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; mut = gen.addNewDrawnMutation(1, 5000); p1.haplosomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; mut = gen.addNewDrawnMutation(1, 5000, NULL); p1.haplosomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; mut = gen.addNewDrawnMutation(1, 5000, NULL, NULL); p1.haplosomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; mut = gen.addNewDrawnMutation(m1, 5000:5003, p1); p1.haplosomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; mut = gen.addNewDrawnMutation(m1, 5000:5003, 1); p1.haplosomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; mut = gen.addNewDrawnMutation(m1, 5000:5003); p1.haplosomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; mut = gen.addNewDrawnMutation(7, 5000, NULL); stop(); }", "not defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; mut = gen.addNewDrawnMutation(1, -1, 1); stop(); }", "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; mut = gen.addNewDrawnMutation(1, 100000, 1); stop(); }", "past the end", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; mut = gen.addNewDrawnMutation(1, 5000, 237); stop(); }", __LINE__);											// bad subpop, but this is legal to allow "tagging" of mutations
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; mut = gen.addNewDrawnMutation(1, 5000, -1); stop(); }", "out of range", __LINE__);					// however, such tags must be within range
	
	// Test Haplosome + (object<Mutation>)addNewMutation(io<MutationType> mutationType, numeric selectionCoeff, integer position, [Nio<Subpopulation> originSubpop])
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; mut = gen.addNewMutation(m1, 0.1, 5000, p1); p1.haplosomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; mut = gen.addNewMutation(m1, 0.1, 5000, 1); p1.haplosomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; mut = gen.addNewMutation(m1, 0.1, 5000); p1.haplosomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; mut = gen.addNewMutation(1, 0.1, 5000, p1); p1.haplosomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; mut = gen.addNewMutation(1, 0.1, 5000, 1); p1.haplosomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; mut = gen.addNewMutation(1, 0.1, 5000); p1.haplosomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; mut = gen.addNewMutation(1, 0.1, 5000, 1); p1.haplosomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; mut = gen.addNewMutation(1, 0.1, 5000); p1.haplosomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; mut = gen.addNewMutation(1, 0.1, 5000, NULL); p1.haplosomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; mut = gen.addNewMutation(m1, 0.1, 5000:5000, p1); p1.haplosomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; mut = gen.addNewMutation(m1, 0.1, 5000:5003, 0:3); p1.haplosomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; mut = gen.addNewMutation(m1, 0.1, 5000:5003); p1.haplosomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; mut = gen.addNewMutation(m1, (0:3)/10, 5000:5003); p1.haplosomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; mut = gen.addNewMutation(m1, (0:3)/10, 5000:5003, 0:3); p1.haplosomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; mut = gen.addNewMutation(7, 0.1, 5000, 1); p1.haplosomes.addMutations(mut); stop(); }", "not defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; mut = gen.addNewMutation(1, 0.1, -1, 1); p1.haplosomes.addMutations(mut); stop(); }", "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; mut = gen.addNewMutation(1, 0.1, 100000, 1); p1.haplosomes.addMutations(mut); stop(); }", "past the end", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; mut = gen.addNewMutation(1, 0.1, 5000, 237); p1.haplosomes.addMutations(mut); stop(); }", __LINE__);							// bad subpop, but this is legal to allow "tagging" of mutations
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; mut = gen.addNewMutation(1, 0.1, 5000, -1); p1.haplosomes.addMutations(mut); stop(); }", "out of range", __LINE__);	// however, such tags must be within range
	
	// Test Haplosome + (object<Mutation>)addNewDrawnMutation(io<MutationType> mutationType, integer position, [io<Subpopulation> originSubpop]) with new class method non-multiplex behavior
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.haplosomes.addNewDrawnMutation(m1, 5000, p1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.haplosomes.addNewDrawnMutation(m1, 5000, 1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.haplosomes.addNewDrawnMutation(m1, 5000); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.haplosomes.addNewDrawnMutation(1, 5000, p1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.haplosomes.addNewDrawnMutation(1, 5000, 1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.haplosomes.addNewDrawnMutation(1, 5000); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.haplosomes.addNewDrawnMutation(1, 5000, 1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.haplosomes.addNewDrawnMutation(1, 5000); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.haplosomes.addNewDrawnMutation(1, 5000, NULL); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.haplosomes.addNewDrawnMutation(m1, 5000:5003, p1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.haplosomes.addNewDrawnMutation(m1, 5000:5003, 0:3); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.haplosomes.addNewDrawnMutation(m1, 5000:5003); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.haplosomes.addNewDrawnMutation(7, 5000, 1); stop(); }", "not defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.haplosomes.addNewDrawnMutation(1, -1, 1); stop(); }", "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.haplosomes.addNewDrawnMutation(1, 100000, 1); stop(); }", "past the end", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.haplosomes.addNewDrawnMutation(1, 5000, 237); stop(); }", __LINE__);											// bad subpop, but this is legal to allow "tagging" of mutations
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.haplosomes.addNewDrawnMutation(1, 5000, -1); stop(); }", "out of range", __LINE__);					// however, such tags must be within range
	
	// Test Haplosome + (object<Mutation>)addNewMutation(io<MutationType> mutationType, numeric selectionCoeff, integer position, [io<Subpopulation> originSubpop]) with new class method non-multiplex behavior
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.haplosomes.addNewMutation(m1, 0.1, 5000, p1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.haplosomes.addNewMutation(m1, 0.1, 5000, 1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.haplosomes.addNewMutation(m1, 0.1, 5000); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.haplosomes.addNewMutation(1, 0.1, 5000, p1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.haplosomes.addNewMutation(1, 0.1, 5000, 1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.haplosomes.addNewMutation(1, 0.1, 5000); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.haplosomes.addNewMutation(1, 0.1, 5000, 1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.haplosomes.addNewMutation(1, 0.1, 5000); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.haplosomes.addNewMutation(1, 0.1, 5000, NULL); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.haplosomes.addNewMutation(m1, 0.1, 5000:5003, p1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.haplosomes.addNewMutation(m1, 0.1, 5000:5003, 0:3); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.haplosomes.addNewMutation(m1, 0.1, 5000:5003); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.haplosomes.addNewMutation(m1, (0:3)/10, 5000:5003); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.haplosomes.addNewMutation(m1, (0:3)/10, 5000:5003, 0:3); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.haplosomes.addNewMutation(7, 0.1, 5000, 1); stop(); }", "not defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.haplosomes.addNewMutation(1, 0.1, -1, 1); stop(); }", "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.haplosomes.addNewMutation(1, 0.1, 100000, 1); stop(); }", "past the end", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.haplosomes.addNewMutation(1, 0.1, 5000, 237); stop(); }", __LINE__);							// bad subpop, but this is legal to allow "tagging" of mutations
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.haplosomes.addNewMutation(1, 0.1, 5000, -1); stop(); }", "out of range", __LINE__);	// however, such tags must be within range
	
	// Test Haplosome - (logical$)containsMarkerMutation(io<MutationType>$ mutType, integer$ position, [logical$ returnMutation = F])
	SLiMAssertScriptStop(gen1_setup_p1 + "10 early() { p1.haplosomes[0].containsMarkerMutation(m1, 1000); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 early() { p1.haplosomes[0].containsMarkerMutation(1, 1000); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 early() { p1.haplosomes[0:1].containsMarkerMutation(1, 1000); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "10 early() { p1.haplosomes[0].containsMarkerMutation(m1, -1); stop(); }", "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "10 early() { p1.haplosomes[0].containsMarkerMutation(m1, 1000000); stop(); }", "past the end", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "10 early() { p1.haplosomes[0].containsMarkerMutation(10, 1000); stop(); }", "mutation type m10 not defined", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_p1 + "10 early() { p1.haplosomes[0].containsMarkerMutation(m1, 1000, returnMutation=T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 early() { p1.haplosomes[0].containsMarkerMutation(1, 1000, returnMutation=T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 early() { p1.haplosomes[0:1].containsMarkerMutation(1, 1000, returnMutation=T); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "10 early() { p1.haplosomes[0].containsMarkerMutation(m1, -1, returnMutation=T); stop(); }", "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "10 early() { p1.haplosomes[0].containsMarkerMutation(m1, 1000000, returnMutation=T); stop(); }", "past the end", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "10 early() { p1.haplosomes[0].containsMarkerMutation(10, 1000, returnMutation=T); stop(); }", "mutation type m10 not defined", __LINE__);
	
	// Test Haplosome - (logical)containsMutations(object<Mutation> mutations)
	SLiMAssertScriptStop(gen1_setup_p1 + "10 early() { p1.haplosomes[0].containsMutations(object()); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 early() { p1.haplosomes[0].containsMutations(sim.mutations); stop(); }", __LINE__);
	
	// Test Haplosome - (integer$)countOfMutationsOfType(io<MutationType>$ mutType)
	SLiMAssertScriptStop(gen1_setup_p1 + "10 early() { p1.haplosomes[0].countOfMutationsOfType(m1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 early() { p1.haplosomes[0].countOfMutationsOfType(1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 early() { p1.haplosomes[0:1].countOfMutationsOfType(1); stop(); }", __LINE__);
	
	// Test Haplosome + (float)mutationFrequenciesInHaplosomes([No<Mutation> mutations = NULL])
	SLiMAssertScriptRaise(gen1_setup_p1 + "10 early() { f = p1.haplosomes[integer(0)].mutationFrequenciesInHaplosomes(); stop(); }", "zero-length Haplosome vector", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 early() { f = p1.haplosomes[0].mutationFrequenciesInHaplosomes(); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 early() { f = p1.haplosomes.mutationFrequenciesInHaplosomes(); if (identical(f, sim.mutationFrequencies(NULL))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "10 early() { f = p1.haplosomes[integer(0)].mutationFrequenciesInHaplosomes(object()); stop(); }", "zero-length Haplosome vector", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 early() { f = p1.haplosomes[0].mutationFrequenciesInHaplosomes(object()); if (size(f) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 early() { f = p1.haplosomes.mutationFrequenciesInHaplosomes(object()); if (size(f) == 0) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "10 early() { f = p1.haplosomes[integer(0)].mutationFrequenciesInHaplosomes(sim.mutations); stop(); }", "zero-length Haplosome vector", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 early() { f = p1.haplosomes[0].mutationFrequenciesInHaplosomes(sim.mutations); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 early() { f = p1.haplosomes.mutationFrequenciesInHaplosomes(sim.mutations); if (identical(f, sim.mutationFrequencies(NULL))) stop(); }", __LINE__);
	
	// Test Haplosome + (integer)mutationCountsInHaplosomes([No<Mutation> mutations = NULL])
	SLiMAssertScriptRaise(gen1_setup_p1 + "10 early() { f = p1.haplosomes[integer(0)].mutationCountsInHaplosomes(); stop(); }", "zero-length Haplosome vector", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 early() { f = p1.haplosomes[0].mutationCountsInHaplosomes(); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 early() { f = p1.haplosomes.mutationCountsInHaplosomes(); if (identical(f, sim.mutationCounts(NULL))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "10 early() { f = p1.haplosomes[integer(0)].mutationCountsInHaplosomes(object()); stop(); }", "zero-length Haplosome vector", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 early() { f = p1.haplosomes[0].mutationCountsInHaplosomes(object()); if (size(f) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 early() { f = p1.haplosomes.mutationCountsInHaplosomes(object()); if (size(f) == 0) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "10 early() { f = p1.haplosomes[integer(0)].mutationCountsInHaplosomes(sim.mutations); stop(); }", "zero-length Haplosome vector", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 early() { f = p1.haplosomes[0].mutationCountsInHaplosomes(sim.mutations); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 early() { f = p1.haplosomes.mutationCountsInHaplosomes(sim.mutations); if (identical(f, sim.mutationCounts(NULL))) stop(); }", __LINE__);
	
	// Test Haplosome - (integer$)positionsOfMutationsOfType(io<MutationType>$ mutType)
	SLiMAssertScriptStop(gen1_setup_p1 + "10 early() { p1.haplosomes[0].positionsOfMutationsOfType(m1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 early() { p1.haplosomes[0].positionsOfMutationsOfType(1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 early() { p1.haplosomes[0:1].positionsOfMutationsOfType(1); stop(); }", __LINE__);
	
	// Test Haplosome - (float$)sumOfMutationsOfType(io<MutationType>$ mutType)
	SLiMAssertScriptStop(gen1_setup_p1 + "10 early() { p1.haplosomes[0].sumOfMutationsOfType(m1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 early() { p1.haplosomes[0].sumOfMutationsOfType(1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 early() { p1.haplosomes[0:1].sumOfMutationsOfType(1); stop(); }", __LINE__);
	
	// Test Haplosome - (object<Mutation>)mutationsOfType(io<MutationType>$ mutType)
	SLiMAssertScriptSuccess(gen1_setup_p1 + "10 early() { p1.haplosomes[0].mutationsOfType(m1); } ", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1 + "10 early() { p1.haplosomes[0].mutationsOfType(1); } ", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1 + "10 early() { p1.haplosomes[0:1].mutationsOfType(1); } ", __LINE__);
	
	// Test Haplosome + (void)removeMutations(object<Mutation> mutations, [logical$ substitute])
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; mut = gen.addNewMutation(m1, 0.1, 5000); gen.removeMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; mut = gen.addNewMutation(m1, 0.1, 5000); gen.removeMutations(mut); gen.removeMutations(mut); stop(); }", __LINE__);	// legal to remove a mutation that is not present
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; gen.removeMutations(object()); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 early() { gen = p1.haplosomes[0]; gen.removeMutations(gen.mutations); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; mut = gen.addNewMutation(m1, 0.1, 5000); gen.removeMutations(mut, T); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; mut = gen.addNewMutation(m1, 0.1, 5000); gen.removeMutations(mut, T); gen.removeMutations(mut, T); stop(); }", "not currently segregating", __LINE__);	// not legal to remove a mutation that has been substituted
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; gen.removeMutations(object(), T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 early() { gen = p1.haplosomes[0]; gen.removeMutations(gen.mutations, T); stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; mut = gen.addNewMutation(m1, 0.1, 5000); gen.removeMutations(); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; mut = gen.addNewMutation(m1, 0.1, 5000); gen.removeMutations(); gen.removeMutations(NULL); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 early() { gen = p1.haplosomes[0]; gen.removeMutations(NULL); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { gen = p1.haplosomes[0]; mut = gen.addNewMutation(m1, 0.1, 5000); gen.removeMutations(NULL, T); }", "substitute may not be T if", __LINE__);
	
	// Test Haplosome + (void)outputHaplosomesToMS([Ns$ filePath], [logical$ append = F], [logical$ filterMonomorphic = F])
	SLiMAssertScriptRaise(gen1_setup_p1 + "10 late() { sample(p1.haplosomes, 0, T).outputHaplosomesToMS(); stop(); }", "at least one haplosome is required", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.haplosomes, 100, T).outputHaplosomesToMS(); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.haplosomes, 100, T).outputHaplosomesToMS(NULL); stop(); }", __LINE__);
	if (Eidos_TemporaryDirectoryExists())
	{
		SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.haplosomes, 100, T).outputHaplosomesToMS('" + temp_path + "/slimOutputMSTest2.txt'); stop(); }", __LINE__);
	}
	
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "10 late() { sample(p1.haplosomes, 100, T).outputHaplosomesToMS(NULL); stop(); }", "cannot output null haplosomes", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "10 late() { sample(p1.haplosomes[!p1.haplosomes.isNullHaplosome], 100, T).outputHaplosomesToMS(NULL); stop(); }", __LINE__);
	if (Eidos_TemporaryDirectoryExists())
	{
		SLiMAssertScriptRaise(gen1_setup_sex_p1 + "10 late() { sample(p1.haplosomes, 100, T).outputHaplosomesToMS('" + temp_path + "/slimOutputMSTest4.txt'); stop(); }", "cannot output null haplosomes", __LINE__);
		SLiMAssertScriptStop(gen1_setup_sex_p1 + "10 late() { sample(p1.haplosomes[!p1.haplosomes.isNullHaplosome], 100, T).outputHaplosomesToMS('" + temp_path + "/slimOutputMSTest5.txt'); stop(); }", __LINE__);
	}
	
	// Test Haplosome + (void)outputHaplosomes([Ns$ filePath])
	SLiMAssertScriptRaise(gen1_setup_p1 + "10 late() { sample(p1.haplosomes, 0, T).outputHaplosomes(); stop(); }", "at least one haplosome is required", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.haplosomes, 100, T).outputHaplosomes(); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.haplosomes, 100, T).outputHaplosomes(NULL); stop(); }", __LINE__);
	if (Eidos_TemporaryDirectoryExists())
	{
		SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.haplosomes, 100, T).outputHaplosomes('" + temp_path + "/slimOutputTest2.txt'); stop(); }", __LINE__);
	}
	
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "10 late() { sample(p1.haplosomes, 100, T).outputHaplosomes(NULL); stop(); }", "cannot output null haplosomes", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "10 late() { sample(p1.haplosomes[!p1.haplosomes.isNullHaplosome], 100, T).outputHaplosomes(NULL); stop(); }", __LINE__);
	if (Eidos_TemporaryDirectoryExists())
	{
		SLiMAssertScriptRaise(gen1_setup_sex_p1 + "10 late() { sample(p1.haplosomes, 100, T).outputHaplosomes('" + temp_path + "/slimOutputTest4.txt'); stop(); }", "cannot output null haplosomes", __LINE__);
		SLiMAssertScriptStop(gen1_setup_sex_p1 + "10 late() { sample(p1.haplosomes[!p1.haplosomes.isNullHaplosome], 100, T).outputHaplosomes('" + temp_path + "/slimOutputTest5.txt'); stop(); }", __LINE__);
	}
	
	// Test Haplosome + (void)outputHaplosomesToVCF([Ns$ filePath], [logical$ outputMultiallelics])
	SLiMAssertScriptRaise(gen1_setup_p1 + "10 late() { sample(p1.individuals, 0, T).haplosomes.outputHaplosomesToVCF(); stop(); }", "at least one haplosome is required", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.individuals, 100, T).haplosomes.outputHaplosomesToVCF(); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.individuals, 100, T).haplosomes.outputHaplosomesToVCF(NULL); stop(); }", __LINE__);
	if (Eidos_TemporaryDirectoryExists())
	{
		SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.individuals, 100, T).haplosomes.outputHaplosomesToVCF('" + temp_path + "/slimOutputVCFTest2.txt'); stop(); }", __LINE__);
	}
	SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.individuals, 100, T).haplosomes.outputHaplosomesToVCF(NULL, F); stop(); }", __LINE__);
	if (Eidos_TemporaryDirectoryExists())
	{
		SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.individuals, 100, T).haplosomes.outputHaplosomesToVCF('" + temp_path + "/slimOutputVCFTest4.txt', F); stop(); }", __LINE__);
	}
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "10 late() { sample(p1.individuals, 100, T).haplosomes.outputHaplosomesToVCF(NULL); stop(); }", __LINE__);
	if (Eidos_TemporaryDirectoryExists())
	{
		SLiMAssertScriptStop(gen1_setup_sex_p1 + "10 late() { sample(p1.individuals, 100, T).haplosomes.outputHaplosomesToVCF('" + temp_path + "/slimOutputVCFTest6.txt'); stop(); }", __LINE__);
	}
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "10 late() { sample(p1.individuals, 100, T).haplosomes.outputHaplosomesToVCF(NULL, F); stop(); }", __LINE__);
	if (Eidos_TemporaryDirectoryExists())
	{
		SLiMAssertScriptStop(gen1_setup_sex_p1 + "10 late() { sample(p1.individuals, 100, T).haplosomes.outputHaplosomesToVCF('" + temp_path + "/slimOutputVCFTest8.txt', F); stop(); }", __LINE__);
	}
}






























