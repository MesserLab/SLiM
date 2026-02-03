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
	SLiMAssertScriptStop(gen1_setup + "1 early() { if (m1.effectSizeDistributionParamsForTrait() == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { if (m1.effectSizeDistributionTypeForTrait() == 'f') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { if (m1.defaultDominanceForTrait() == 0.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { if (m1.defaultHemizygousDominanceForTrait() == 1.0) stop(); }", __LINE__);
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
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.id = 2; }", "read-only property", __LINE__);

	SLiMAssertScriptStop(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); c(m1,m2).mutationStackGroup = 3; c(m1,m2).mutationStackPolicy = 'f'; } 1 early() { stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); c(m1,m2).mutationStackGroup = 3; m1.mutationStackPolicy = 'f'; m2.mutationStackPolicy = 'l'; } 1 early() { stop(); }", "inconsistent mutationStackPolicy", __LINE__, false);
	SLiMAssertScriptRaise(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); c(m1,m2).mutationStackGroup = 3; c(m1,m2).mutationStackPolicy = 'f'; } 1 early() { m2.mutationStackPolicy = 'l'; }", "inconsistent mutationStackPolicy", __LINE__, false);
	SLiMAssertScriptStop(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); m1.mutationStackPolicy = 'f'; m2.mutationStackPolicy = 'l'; } 1 early() { stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); m1.mutationStackPolicy = 'f'; m2.mutationStackPolicy = 'l'; } 1 early() { c(m1,m2).mutationStackGroup = 3; }", "inconsistent mutationStackPolicy", __LINE__, false);
	
	// Test MutationType - (void)setDistribution(string$ distributionType, ...)
	SLiMAssertScriptSuccess(gen1_setup + "1 early() { m1.setDefaultDominanceForTrait(NULL, 0.3); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 early() { m1.setDefaultDominanceForTrait(0, 0.3); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 early() { m1.setDefaultDominanceForTrait(sim.traits, 0.3); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 early() { m1.setDefaultHemizygousDominanceForTrait(NULL, 0.3); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 early() { m1.setDefaultHemizygousDominanceForTrait(0, 0.3); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 early() { m1.setDefaultHemizygousDominanceForTrait(sim.traits, 0.3); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'f', 2.2); if (m1.effectSizeDistributionTypeForTrait() == 'f' & m1.effectSizeDistributionParamsForTrait() == 2.2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'g', 3.1, 7.5); if (m1.effectSizeDistributionTypeForTrait() == 'g' & identical(m1.effectSizeDistributionParamsForTrait(), c(3.1, 7.5))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'e', -3); if (m1.effectSizeDistributionTypeForTrait() == 'e' & m1.effectSizeDistributionParamsForTrait() == -3) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'n', 3.1, 7.5); if (m1.effectSizeDistributionTypeForTrait() == 'n' & identical(m1.effectSizeDistributionParamsForTrait(), c(3.1, 7.5))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'p', 3.1, 7.5); if (m1.effectSizeDistributionTypeForTrait() == 'p' & identical(m1.effectSizeDistributionParamsForTrait(), c(3.1, 7.5))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'w', 3.1, 7.5); if (m1.effectSizeDistributionTypeForTrait() == 'w' & identical(m1.effectSizeDistributionParamsForTrait(), c(3.1, 7.5))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 's', 'return 1;'); if (m1.effectSizeDistributionTypeForTrait() == 's' & identical(m1.effectSizeDistributionParamsForTrait(), 'return 1;')) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'x', 1.5); stop(); }", "must be 'f', 'g', 'e', 'n', 'w', or 's'", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'f', 'foo'); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'g', 'foo', 7.5); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'g', 3.1, 'foo'); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'e', 'foo'); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'n', 'foo', 7.5); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'n', 3.1, 'foo'); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'p', 'foo', 7.5); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'p', 3.1, 'foo'); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'w', 'foo', 7.5); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'w', 3.1, 'foo'); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 's', 3); stop(); }", "must be of type string", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'f', '1'); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'g', '1', 7.5); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'g', 3.1, '1'); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'e', '1'); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'n', '1', 7.5); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'n', 3.1, '1'); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'p', '1', 7.5); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'p', 3.1, '1'); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'w', '1', 7.5); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'w', 3.1, '1'); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 's', 3.1); stop(); }", "must be of type string", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'f', T); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'g', T, 7.5); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'g', 3.1, T); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'e', T); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'n', T, 7.5); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'n', 3.1, T); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'p', T, 7.5); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'p', 3.1, T); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'w', T, 7.5); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'w', 3.1, T); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 's', T); stop(); }", "must be of type string", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'g', 3.1, 0.0); }", "must have a shape parameter > 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'g', 3.1, -1.0); }", "must have a shape parameter > 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'n', 3.1, -1.0); }", "must have a standard deviation parameter >= 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'p', 3.1, 0.0); }", "must have a scale parameter > 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'p', 3.1, -1.0); }", "must have a scale parameter > 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'w', 0.0, 7.5); }", "must have a scale parameter > 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'w', -1.0, 7.5); }", "must have a scale parameter > 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'w', 3.1, 0.0); }", "must have a shape parameter > 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'w', 3.1, -7.5); }", "must have a shape parameter > 0", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 's', 'return foo;'); } 100 early() { stop(); }", "undefined identifier foo", __LINE__, false);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 's', 'x >< 5;'); } 100 early() { stop(); }", "tokenize/parse error in type 's' DES callback script", __LINE__, false);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 's', 'x $ 5;'); } 100 early() { stop(); }", "tokenize/parse error in type 's' DES callback script", __LINE__, false);
	
	// Test MutationType - (float)drawEffectSizeForTrait([integer$ n = 1])
	// the parameters here are chosen so that these tests should fail extremely rarely
	SLiMAssertScriptStop(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'f', 2.2); if (abs(m1.drawEffectSizeForTrait() - 2.2) < 1e-6) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'f', 2.2); if (all(abs(m1.drawEffectSizeForTrait(NULL, 10) - rep(2.2, 10)) < 1e-6)) stop(); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'g', 3.1, 7.5); m1.drawEffectSizeForTrait(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'g', 3.1, 7.5); if (abs(mean(m1.drawEffectSizeForTrait(NULL, 5000)) - 3.1) < 0.1) stop(); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'e', -3.0); m1.drawEffectSizeForTrait(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'e', -3.0); if (abs(mean(m1.drawEffectSizeForTrait(NULL, 30000)) + 3.0) < 0.1) stop(); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'n', 3.1, 0.5); m1.drawEffectSizeForTrait(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'n', 3.1, 0.5); if (abs(mean(m1.drawEffectSizeForTrait(NULL, 2000)) - 3.1) < 0.1) stop(); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'p', 3.1, 7.5); m1.drawEffectSizeForTrait(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'p', 3.1, 0.01); if (abs(mean(m1.drawEffectSizeForTrait(NULL, 2000)) - 3.1) < 0.1) stop(); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'w', 3.1, 7.5); m1.drawEffectSizeForTrait(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 'w', 3.1, 7.5); if (abs(mean(m1.drawEffectSizeForTrait(NULL, 2000)) - 2.910106) < 0.1) stop(); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 's', 'rbinom(1, 4, 0.5);'); m1.drawEffectSizeForTrait(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { m1.setEffectSizeDistributionForTrait(NULL, 's', 'rbinom(1, 4, 0.5);'); if (abs(mean(m1.drawEffectSizeForTrait(NULL, 5000)) - 2.0) < 0.1) stop(); }", __LINE__);
	
	// test mutation data recording with logMutationData() and loggedData()
	std::string data_recording =
	R"V0G0N(
		initialize() {
			initializeMutationRate(1e-7);
			initializeMutationType("m1", 0.5, "f", 0.0);
			m1.logMutationData(T, chromosomeID=T, position=T);
			initializeGenomicElementType("g1", m1, 1.0);
			initializeGenomicElement(g1, 0, 999999);
			initializeRecombinationRate(1e-8);
		}
		1 late() {
			sim.addSubpop("p1", 50);
			if (m1.loggedData(kind="count") != 0)
				stop("MutationType data recording initial state incorrect");
			df1 = m1.loggedData(kind="values");         // get all logged columns
			if ((df1.nrow != 0) | (df1.ncol != 2))
				stop("MutationType data recording initial state incorrect");
			df2 = m1.loggedData(kind="values", chromosomeID=T);
			if ((df2.nrow != 0) | (df2.ncol != 1))
				stop("MutationType data recording initial state incorrect");
			df3 = m1.loggedData(kind="values", chromosomeID=T, position=T, originTick=T);
			if ((df3.nrow != 0) | (df3.ncol != 2))
				stop("MutationType data recording initial state incorrect");
		}
		100 late() {
			count = m1.loggedData(kind="count");
			if ((count == 0))
				stop("MutationType data recording recorded no mutations");
			df1 = m1.loggedData(kind="values");         // get all logged columns
			if ((df1.nrow != count) | (df1.ncol != 2))
				stop("MutationType data recording problem");
			df2 = m1.loggedData(kind="values", position=T);
			if ((df2.nrow != count) | (df2.ncol != 1))
				stop("MutationType data recording problem");
			df3 = m1.loggedData(kind="values", chromosomeID=T, position=T, originTick=T);
			if ((df3.nrow != count) | (df3.ncol != 2))
				stop("MutationType data recording problem");
		}
	)V0G0N";
	
	SLiMAssertScriptSuccess(data_recording);
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
	SLiMAssertScriptStop(gen1_setup + "1 early() { g1.setMutationFractions(m1, 0.0); if (g1.mutationTypes.size() == 0 & size(g1.mutationFractions) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { g1.setMutationFractions(1, 0.0); if (g1.mutationTypes.size() == 0 & size(g1.mutationFractions) == 0) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { sim.addSubpop('p1', 10); } 1 early() { g1.setMutationFractions(m1, 0.0); } 100 early() {}", "empty mutation type vector", __LINE__, false);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { sim.addSubpop('p1', 10); } 1 early() { g1.setMutationFractions(1, 0.0); } 100 early() {}", "empty mutation type vector", __LINE__, false);
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
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 early() { mut = sim.mutations[0]; if (mut.effectSize == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 early() { mut = sim.mutations[0]; if (mut.subpopID == 1) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 early() { mut = sim.mutations[0]; mut.mutationType = m1; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 early() { mut = sim.mutations[0]; mut.originTick = 1; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 early() { mut = sim.mutations[0]; mut.position = 0; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 early() { mut = sim.mutations[0]; mut.effectSize = 0.1; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 early() { mut = sim.mutations[0]; mut.subpopID = 237; if (mut.subpopID == 237) stop(); }", __LINE__);						// legal; this field may be used as a user tag
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 early() { mut = sim.mutations[0]; mut.tag; }", "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 early() { mut = sim.mutations[0]; c(mut,mut).tag; }", "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 early() { mut = sim.mutations[0]; mut.tag = 278; if (mut.tag == 278) stop(); }", __LINE__);
	
	// Test Mutation - (void)setMutationType(io<MutationType>$ mutType)
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 early() { mut = sim.mutations[0]; mut.setMutationType(m1); if (mut.mutationType == m1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 early() { mut = sim.mutations[0]; mut.setMutationType(m1); if (mut.mutationType == m1) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 early() { mut = sim.mutations[0]; mut.setMutationType(2); if (mut.mutationType == m1) stop(); }", "mutation type m2 not defined", __LINE__);
}

#pragma mark Substitution tests
void _RunSubstitutionTests(void)
{
	// ************************************************************************************
	//
	//	Gen 1+ tests: Substitution
	//
	
	// Test Substitution properties
	SLiMAssertScriptStop(gen1_setup_fixmut_p1 + "50 early() { if (size(sim.substitutions) > 0) stop(); }", __LINE__);										// check that our script generates substitutions fast enough
	SLiMAssertScriptStop(gen1_setup_fixmut_p1 + "50 early() { sub = sim.substitutions[0]; if (sub.fixationTick > 0 & sub.fixationTick <= 30) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_fixmut_p1 + "50 early() { sub = sim.substitutions[0]; if (sub.mutationType == m1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_fixmut_p1 + "50 early() { sub = sim.substitutions[0]; if (sub.originTick > 0 & sub.originTick <= 10) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_fixmut_p1 + "50 early() { sub = sim.substitutions[0]; if (sub.position >= 0 & sub.position <= 99999) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_fixmut_p1 + "50 early() { if (sum(sim.substitutions.effectSize == 500.0) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_fixmut_p1 + "50 early() { sub = sim.substitutions[0]; if (sub.subpopID == 1) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_fixmut_p1 + "50 early() { sub = sim.substitutions[0]; sub.fixationTick = 10; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_fixmut_p1 + "50 early() { sub = sim.substitutions[0]; sub.mutationType = m1; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_fixmut_p1 + "50 early() { sub = sim.substitutions[0]; sub.originTick = 10; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_fixmut_p1 + "50 early() { sub = sim.substitutions[0]; sub.position = 99999; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_fixmut_p1 + "50 early() { sub = sim.substitutions[0]; sub.effectSize = 50.0; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptStop(gen1_setup_fixmut_p1 + "50 early() { sub = sim.substitutions[0]; sub.subpopID = 237; if (sub.subpopID == 237) stop(); }", __LINE__);						// legal; this field may be used as a user tag
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
	
	// Test Haplosome + (object<Mutation>)addNewMutation(io<MutationType> mutationType, numeric effectSize, integer position, [Nio<Subpopulation> originSubpop])
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
	
	// Test Haplosome + (object<Mutation>)addNewMutation(io<MutationType> mutationType, numeric effectSize, integer position, [io<Subpopulation> originSubpop]) with new class method non-multiplex behavior
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

#pragma mark Multitrait tests
void _RunMultitraitTests(void)
{
	// two-trait base model implemented in WF and nonWF -- one trait multiplicative, one trait additive
	const std::string mt_base_p1_WF = 
R"V0G0N(
initialize() {
	defineConstant("T_height", initializeTrait("height", "multiplicative", 2.0));
	defineConstant("T_weight", initializeTrait("weight", "additive", 186.0));
	initializeMutationRate(1e-5);
	initializeMutationType("m1", 0.5, "f", 0.0);
	initializeGenomicElementType("g1", m1, 1.0);
	initializeGenomicElement(g1, 0, 99999);
	initializeRecombinationRate(1e-8);
}
1 late() { sim.addSubpop("p1", 5); }
5 late() { }
)V0G0N";
	
	const std::string mt_base_p1_nonWF = 
R"V0G0N(
initialize() {
	initializeSLiMModelType("nonWF");
	defineConstant("T_height", initializeTrait("height", "multiplicative", 2.0));
	defineConstant("T_weight", initializeTrait("weight", "additive", 186.0));
	initializeMutationRate(1e-5);
	initializeMutationType("m1", 0.5, "f", 0.0).convertToSubstitution = T;
	initializeGenomicElementType("g1", m1, 1.0);
	initializeGenomicElement(g1, 0, 99999);
	initializeRecombinationRate(1e-8);
}
reproduction() { p1.addCrossed(individual, p1.sampleIndividuals(1)); }
1 late() { sim.addSubpop("p1", 5); }
late() { sim.killIndividuals(p1.subsetIndividuals(minAge=1)); }
5 late() { }
)V0G0N";
	
	for (int model = 0; model <= 1; ++model)
	{
		std::string mt_base_p1 = ((model == 0) ? mt_base_p1_WF : mt_base_p1_nonWF);
		
		SLiMAssertScriptSuccess(mt_base_p1);
		
		// initializeTrait() requirements
		SLiMAssertScriptRaise("initialize() { initializeTrait('', 'multiplicative', 2.0); }", "non-empty string", __LINE__);
		SLiMAssertScriptRaise("initialize() { initializeTrait('human height', 'multiplicative', 2.0); }", "valid Eidos identifier", __LINE__);
		SLiMAssertScriptRaise("initialize() { initializeTrait('migrant', 'multiplicative', 2.0); }", "existing property on Individual", __LINE__);
		SLiMAssertScriptRaise("initialize() { initializeTrait('avatar', 'multiplicative', 2.0); }", "existing property on Species", __LINE__);
		SLiMAssertScriptRaise("initialize() { initializeTrait('height', 'multiplicative', 2.0); initializeTrait('height', 'multiplicative', 2.0); }", "already a trait in this species", __LINE__);
		SLiMAssertScriptRaise("initialize() { initializeTrait('height', 'multi', 2.0); }", "requires type to be either", __LINE__);
		SLiMAssertScriptRaise("initialize() { initializeTrait('height', 'multiplicative', baselineOffset=INF); }", "baselineOffset to be a finite value", __LINE__);
		SLiMAssertScriptRaise("initialize() { initializeTrait('height', 'multiplicative', baselineOffset=NAN); }", "baselineOffset to be a finite value", __LINE__);
		SLiMAssertScriptRaise("initialize() { initializeTrait('height', 'multiplicative', individualOffsetMean=INF, individualOffsetSD=0.0); }", "individualOffsetMean to be a finite value", __LINE__);
		SLiMAssertScriptRaise("initialize() { initializeTrait('height', 'multiplicative', individualOffsetMean=NAN, individualOffsetSD=0.0); }", "individualOffsetMean to be a finite value", __LINE__);
		SLiMAssertScriptRaise("initialize() { initializeTrait('height', 'multiplicative', individualOffsetMean=1.0, individualOffsetSD=INF); }", "individualOffsetSD to be a nonnegative finite value", __LINE__);
		SLiMAssertScriptRaise("initialize() { initializeTrait('height', 'multiplicative', individualOffsetMean=1.0, individualOffsetSD=NAN); }", "individualOffsetSD to be a nonnegative finite value", __LINE__);
		SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'f', 0.0); initializeTrait('height', 'multiplicative'); }", "already been implicitly defined", __LINE__);
		SLiMAssertScriptRaise("initialize() { initializeTrait('height', 'multiplicative'); initializeMutationType('m1', 0.5, 'f', 0.0); initializeTrait('weight', 'multiplicative'); }", "before a mutation type is created", __LINE__);
		SLiMAssertScriptRaise("initialize() { for (i in 1:257) initializeTrait('height' + i, 'multiplicative'); }", "maximum number of traits", __LINE__);
		
		// trait defines, trait lookup
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { if (!identical(c(T_height, T_weight), sim.traits)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { if (!identical(c(T_height, T_weight), community.allTraits)) stop(); }");
		
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { if (!identical(T_height, sim.traitsWithIndices(0))) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { if (!identical(T_weight, sim.traitsWithIndices(1))) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { if (!identical(c(T_height, T_weight), sim.traitsWithIndices(0:1))) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { if (!identical(c(T_weight, T_height), sim.traitsWithIndices(1:0))) stop(); }");
		SLiMAssertScriptRaise(mt_base_p1 + "1 late() { sim.traitsWithIndices(2); }", "out-of-range index (2)", __LINE__);
		
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { if (!identical(T_height, sim.traitsWithNames('height'))) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { if (!identical(T_weight, sim.traitsWithNames('weight'))) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { if (!identical(c(T_height, T_weight), sim.traitsWithNames(c('height', 'weight')))) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { if (!identical(c(T_weight, T_height), sim.traitsWithNames(c('weight', 'height')))) stop(); }");
		SLiMAssertScriptRaise(mt_base_p1 + "1 late() { sim.traitsWithNames('typo'); }", "trait with the given name (typo)", __LINE__);
		
		// basic trait properties: baselineOffset, directFitnessEffect, index, individualOffsetMean, individualOffsetSD, name, species, tag, type
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { if (!identical(T_height.baselineOffset, 2.0)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { if (!identical(T_weight.baselineOffset, 186.0)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { T_height.baselineOffset = 12.5; if (!identical(T_height.baselineOffset, 12.5)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { T_weight.baselineOffset = 17.25; if (!identical(T_weight.baselineOffset, 17.25)) stop(); }");
		SLiMAssertScriptRaise(mt_base_p1 + "1 late() { T_height.baselineOffset = NAN; }", "requires a finite value", __LINE__);
		SLiMAssertScriptRaise(mt_base_p1 + "1 late() { T_height.baselineOffset = INF; }", "requires a finite value", __LINE__);
		
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { if (!identical(T_height.directFitnessEffect, F)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { if (!identical(T_weight.directFitnessEffect, F)) stop(); }");
		
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { if (!identical(T_height.index, 0)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { if (!identical(T_weight.index, 1)) stop(); }");
		
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { if (!identical(T_height.individualOffsetMean, 0.0)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { if (!identical(T_weight.individualOffsetMean, 0.0)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { T_height.individualOffsetMean = 3.5; if (!identical(T_height.individualOffsetMean, 3.5)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { T_weight.individualOffsetMean = 2.5; if (!identical(T_weight.individualOffsetMean, 2.5)) stop(); }");
		SLiMAssertScriptRaise(mt_base_p1 + "1 late() { T_height.individualOffsetMean = NAN; }", "requires a finite value", __LINE__);
		SLiMAssertScriptRaise(mt_base_p1 + "1 late() { T_height.individualOffsetMean = INF; }", "requires a finite value", __LINE__);
		
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { if (!identical(T_height.individualOffsetSD, 0.0)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { if (!identical(T_weight.individualOffsetSD, 0.0)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { T_height.individualOffsetSD = 3.5; if (!identical(T_height.individualOffsetSD, 3.5)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { T_weight.individualOffsetSD = 2.5; if (!identical(T_weight.individualOffsetSD, 2.5)) stop(); }");
		SLiMAssertScriptRaise(mt_base_p1 + "1 late() { T_height.individualOffsetSD = NAN; }", "requires a nonnegative finite value", __LINE__);
		SLiMAssertScriptRaise(mt_base_p1 + "1 late() { T_height.individualOffsetSD = INF; }", "requires a nonnegative finite value", __LINE__);
		
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { if (!identical(T_height.name, 'height')) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { if (!identical(T_weight.name, 'weight')) stop(); }");
		
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { if (!identical(T_height.species, sim)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { if (!identical(T_weight.species, sim)) stop(); }");
		
		SLiMAssertScriptRaise(mt_base_p1 + "1 late() { if (!identical(T_height.tag, 12)) stop(); }", "before being set", __LINE__);
		SLiMAssertScriptRaise(mt_base_p1 + "1 late() { if (!identical(T_weight.tag, 3)) stop(); }", "before being set", __LINE__);
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { T_height.tag = 12; if (!identical(T_height.tag, 12)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { T_weight.tag = 3; if (!identical(T_weight.tag, 3)) stop(); }");
		
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { if (!identical(T_height.type, 'multiplicative')) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { if (!identical(T_weight.type, 'additive')) stop(); }");
		
		// individual offset
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { if (!identical(p1.individuals.offsetForTrait(T_height), rep(1.0, 5))) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { if (!identical(p1.individuals.offsetForTrait(T_weight), rep(0.0, 5))) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { if (!identical(p1.individuals.offsetForTrait(NULL), rep(c(1.0, 0.0), 5))) stop(); }");
		
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { p1.individuals.setOffsetForTrait(0, 3); p1.individuals.setOffsetForTrait(1, 4.5); if (!identical(p1.individuals.offsetForTrait(NULL), rep(c(3, 4.5), 5))) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { p1.individuals.setOffsetForTrait(0, 1:5 * 2 - 1); p1.individuals.setOffsetForTrait(1, 1:5 * 2); if (!identical(p1.individuals.offsetForTrait(NULL), 1.0:10)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { p1.individuals.setOffsetForTrait(NULL, 1:10); if (!identical(p1.individuals.offsetForTrait(NULL), 1.0:10)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { p1.individuals.setOffsetForTrait(NULL, 1:10 + 0.5); if (!identical(p1.individuals.offsetForTrait(NULL), 1:10 + 0.5)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { p1.individuals.setOffsetForTrait(c(0,1), 1:10); if (!identical(p1.individuals.offsetForTrait(NULL), 1.0:10)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { p1.individuals.setOffsetForTrait(c(0,1), 1:10 + 0.5); if (!identical(p1.individuals.offsetForTrait(NULL), 1:10 + 0.5)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { p1.individuals.setOffsetForTrait(c(1,0), 1:10); if (!identical(p1.individuals.offsetForTrait(c(1,0)), 1.0:10)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { p1.individuals.setOffsetForTrait(c(1,0), 1:10 + 0.5); if (!identical(p1.individuals.offsetForTrait(c(1,0)), 1:10 + 0.5)) stop(); }");
		SLiMAssertScriptRaise(mt_base_p1 + "1 late() { p1.individuals.setOffsetForTrait(0, NAN); }", "offset values to be finite", __LINE__);
		SLiMAssertScriptRaise(mt_base_p1 + "1 late() { p1.individuals.setOffsetForTrait(0, c(1,NAN,3,NAN,5)); }", "offset values to be finite", __LINE__);
		SLiMAssertScriptRaise(mt_base_p1 + "1 late() { p1.individuals.setOffsetForTrait(0, INF); }", "offset values to be finite", __LINE__);
		SLiMAssertScriptRaise(mt_base_p1 + "1 late() { p1.individuals.setOffsetForTrait(0, c(1,INF,3,INF,5)); }", "offset values to be finite", __LINE__);
		
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { T_height.individualOffsetMean = 3.5; } 2 late() { if (!allClose(p1.individuals.offsetForTrait(T_height), rep(exp(3.5), 5))) stop(); }");	// note exp() post-transform
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { T_weight.individualOffsetMean = 2.5; } 2 late() { if (!identical(p1.individuals.offsetForTrait(T_weight), rep(2.5, 5))) stop(); }");
		
		// individual phenotype
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { if (!identical(p1.individuals.phenotypeForTrait(T_height), p1.individuals.height)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { if (!identical(p1.individuals.phenotypeForTrait(T_weight), p1.individuals.weight)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { if (!identical(p1.individuals.phenotypeForTrait(NULL), asVector(cbind(p1.individuals.height, p1.individuals.weight)))) stop(); }");
		
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { p1.individuals.setPhenotypeForTrait(0, 3); p1.individuals.setPhenotypeForTrait(1, 4.5); if (!identical(p1.individuals.phenotypeForTrait(NULL), rep(c(3, 4.5), 5))) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { p1.individuals.setPhenotypeForTrait(0, 1:5 * 2 - 1); p1.individuals.setPhenotypeForTrait(1, 1:5 * 2); if (!identical(p1.individuals.phenotypeForTrait(NULL), 1.0:10)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { p1.individuals.setPhenotypeForTrait(NULL, 1:10); if (!identical(p1.individuals.phenotypeForTrait(NULL), 1.0:10)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { p1.individuals.setPhenotypeForTrait(NULL, 1:10 + 0.5); if (!identical(p1.individuals.phenotypeForTrait(NULL), 1:10 + 0.5)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { p1.individuals.setPhenotypeForTrait(c(0,1), 1:10); if (!identical(p1.individuals.phenotypeForTrait(NULL), 1.0:10)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { p1.individuals.setPhenotypeForTrait(c(0,1), 1:10 + 0.5); if (!identical(p1.individuals.phenotypeForTrait(NULL), 1:10 + 0.5)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { p1.individuals.setPhenotypeForTrait(c(1,0), 1:10); if (!identical(p1.individuals.phenotypeForTrait(c(1,0)), 1.0:10)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { p1.individuals.setPhenotypeForTrait(c(1,0), 1:10 + 0.5); if (!identical(p1.individuals.phenotypeForTrait(c(1,0)), 1:10 + 0.5)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { p1.individuals.setPhenotypeForTrait(0, NAN); p1.individuals.setPhenotypeForTrait(1, 4.5); if (!identical(p1.individuals.phenotypeForTrait(NULL), rep(c(NAN, 4.5), 5))) stop(); }");
		SLiMAssertScriptRaise(mt_base_p1 + "1 late() { p1.individuals.setPhenotypeForTrait(0, INF); }", "phenotypes to be finite or NAN", __LINE__);
		
		// Species <trait-name> property access
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { if (!identical(sim.height, sim.traitsWithNames('height'))) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { if (!identical(sim.weight, sim.traitsWithNames('weight'))) stop(); }");
		SLiMAssertScriptRaise(mt_base_p1 + "1 late() { sim.height = sim.traitsWithNames('height'); }", "new value for read-only property", __LINE__);
		SLiMAssertScriptRaise(mt_base_p1 + "1 late() { sim.weight = sim.traitsWithNames('weight'); }", "new value for read-only property", __LINE__);
		
		// Individual <trait-name> property access
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { if (!identical(p1.individuals.height, rep(NAN, 5))) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { if (!identical(p1.individuals.weight, rep(NAN, 5))) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { p1.individuals.height = 10.0; if (!identical(p1.individuals.height, rep(10.0, 5))) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { p1.individuals.weight = 10.0; if (!identical(p1.individuals.weight, rep(10.0, 5))) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { p1.individuals.height = 10.0:14; if (!identical(p1.individuals.height, 10.0:14)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { p1.individuals.weight = 11.0:15; if (!identical(p1.individuals.weight, 11.0:15)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { p1.individuals.height = NAN; if (!identical(p1.individuals.height, rep(NAN, 5))) stop(); }");								// NAN means uncalculated; you can set that
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { p1.individuals.height = c(10.0,NAN,12,NAN,14); if (!identical(p1.individuals.height, c(10.0,NAN,12,NAN,14))) stop(); }");
		SLiMAssertScriptRaise(mt_base_p1 + "1 late() { p1.individuals.height = INF; }", "required to be finite or NAN", __LINE__);
		SLiMAssertScriptRaise(mt_base_p1 + "1 late() { p1.individuals.height = c(10.0,INF,12,INF,14); }", "required to be finite or NAN", __LINE__);
		
		// Individual <trait-name>Offset property access
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { if (!identical(p1.individuals.heightOffset, rep(1.0, 5))) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { if (!identical(p1.individuals.weightOffset, rep(0.0, 5))) stop(); }");
		
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { p1.individuals.heightOffset = 3.0; p1.individuals.weightOffset = 4.5; if (!identical(p1.individuals.offsetForTrait(NULL), rep(c(3, 4.5), 5))) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "1 late() { p1.individuals.heightOffset = 1.0:5 * 2 - 1; p1.individuals.weightOffset = 1.0:5 * 2; if (!identical(p1.individuals.offsetForTrait(NULL), 1.0:10)) stop(); }");
		SLiMAssertScriptRaise(mt_base_p1 + "1 late() { p1.individuals.heightOffset = NAN; }", "required to be a finite value (not INF or NAN)", __LINE__);
		SLiMAssertScriptRaise(mt_base_p1 + "1 late() { p1.individuals.heightOffset = c(1,NAN,3,NAN,5); }", "required to be a finite value (not INF or NAN)", __LINE__);
		SLiMAssertScriptRaise(mt_base_p1 + "1 late() { p1.individuals.heightOffset = INF; }", "required to be a finite value (not INF or NAN)", __LINE__);
		SLiMAssertScriptRaise(mt_base_p1 + "1 late() { p1.individuals.heightOffset = c(1,INF,3,INF,5); }", "required to be a finite value (not INF or NAN)", __LINE__);
		
		// Mutation effectSizeForTrait()
		SLiMAssertScriptSuccess(mt_base_p1 + "5 late() { mut = sim.mutations[0]; if (!identical(mut.effectSizeForTrait(0), 0.0)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "5 late() { mut = sim.mutations[0]; if (!identical(mut.effectSizeForTrait(1), 0.0)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "5 late() { mut = sim.mutations[0]; if (!identical(mut.effectSizeForTrait(NULL), c(0.0, 0.0))) stop(); }");
		
		// Mutation setEffectSizeForTrait()
		SLiMAssertScriptSuccess(mt_base_p1 + "5 late() { mut = sim.mutations[0:4]; mut.setEffectSizeForTrait(0, 3); mut.setEffectSizeForTrait(1, 4.5); if (!identical(mut.effectSizeForTrait(NULL), rep(c(3, 4.5), 5))) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "5 late() { mut = sim.mutations[0:4]; mut.setEffectSizeForTrait(0, 1:5 * 2 - 1); mut.setEffectSizeForTrait(1, 1:5 * 2); if (!identical(mut.effectSizeForTrait(NULL), 1.0:10)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "5 late() { mut = sim.mutations[0:4]; mut.setEffectSizeForTrait(NULL, 1:10); if (!identical(mut.effectSizeForTrait(NULL), 1.0:10)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "5 late() { mut = sim.mutations[0:4]; mut.setEffectSizeForTrait(NULL, 1:10 + 0.5); if (!identical(mut.effectSizeForTrait(NULL), 1:10 + 0.5)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "5 late() { mut = sim.mutations[0:4]; mut.setEffectSizeForTrait(c(0,1), 1:10); if (!identical(mut.effectSizeForTrait(NULL), 1.0:10)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "5 late() { mut = sim.mutations[0:4]; mut.setEffectSizeForTrait(c(0,1), 1:10 + 0.5); if (!identical(mut.effectSizeForTrait(NULL), 1:10 + 0.5)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "5 late() { mut = sim.mutations[0:4]; mut.setEffectSizeForTrait(c(1,0), 1:10); if (!identical(mut.effectSizeForTrait(c(1,0)), 1.0:10)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "5 late() { mut = sim.mutations[0:4]; mut.setEffectSizeForTrait(c(1,0), 1:10 + 0.5); if (!identical(mut.effectSizeForTrait(c(1,0)), 1:10 + 0.5)) stop(); }");
		SLiMAssertScriptRaise(mt_base_p1 + "5 late() { mut = sim.mutations[0:4]; mut.setEffectSizeForTrait(0, NAN); }", "non-finite after setEffectSizeForTrait()", __LINE__);
		SLiMAssertScriptRaise(mt_base_p1 + "5 late() { mut = sim.mutations[0:4]; mut.setEffectSizeForTrait(0, c(1,NAN,3,NAN,5)); }", "non-finite after setEffectSizeForTrait()", __LINE__);
		SLiMAssertScriptRaise(mt_base_p1 + "5 late() { mut = sim.mutations[0:4]; mut.setEffectSizeForTrait(0, INF); }", "non-finite after setEffectSizeForTrait()", __LINE__);
		SLiMAssertScriptRaise(mt_base_p1 + "5 late() { mut = sim.mutations[0:4]; mut.setEffectSizeForTrait(0, c(1,INF,3,INF,5)); }", "non-finite after setEffectSizeForTrait()", __LINE__);
		
		// Mutation dominanceForTrait()
		SLiMAssertScriptSuccess(mt_base_p1 + "5 late() { mut = sim.mutations[0]; if (!identical(mut.dominanceForTrait(0), 0.5)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "5 late() { mut = sim.mutations[0]; if (!identical(mut.dominanceForTrait(1), 0.5)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "5 late() { mut = sim.mutations[0]; if (!identical(mut.dominanceForTrait(NULL), c(0.5, 0.5))) stop(); }");
		
		// Mutation setDominanceForTrait()
		SLiMAssertScriptSuccess(mt_base_p1 + "5 late() { mut = sim.mutations[0:4]; mut.setDominanceForTrait(0, 3); mut.setDominanceForTrait(1, 4.5); if (!identical(mut.dominanceForTrait(NULL), rep(c(3, 4.5), 5))) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "5 late() { mut = sim.mutations[0:4]; mut.setDominanceForTrait(0, 1:5 * 2 - 1); mut.setDominanceForTrait(1, 1:5 * 2); if (!identical(mut.dominanceForTrait(NULL), 1.0:10)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "5 late() { mut = sim.mutations[0:4]; mut.setDominanceForTrait(NULL, 1:10); if (!identical(mut.dominanceForTrait(NULL), 1.0:10)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "5 late() { mut = sim.mutations[0:4]; mut.setDominanceForTrait(NULL, 1:10 + 0.5); if (!identical(mut.dominanceForTrait(NULL), 1:10 + 0.5)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "5 late() { mut = sim.mutations[0:4]; mut.setDominanceForTrait(c(0,1), 1:10); if (!identical(mut.dominanceForTrait(NULL), 1.0:10)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "5 late() { mut = sim.mutations[0:4]; mut.setDominanceForTrait(c(0,1), 1:10 + 0.5); if (!identical(mut.dominanceForTrait(NULL), 1:10 + 0.5)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "5 late() { mut = sim.mutations[0:4]; mut.setDominanceForTrait(c(1,0), 1:10); if (!identical(mut.dominanceForTrait(c(1,0)), 1.0:10)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "5 late() { mut = sim.mutations[0:4]; mut.setDominanceForTrait(c(1,0), 1:10 + 0.5); if (!identical(mut.dominanceForTrait(c(1,0)), 1:10 + 0.5)) stop(); }");
		// NAN for dominance means independent dominance, so it is legal; tested below
		SLiMAssertScriptRaise(mt_base_p1 + "5 late() { mut = sim.mutations[0:4]; mut.setDominanceForTrait(0, INF); }", "infinite after setDominanceForTrait()", __LINE__);
		SLiMAssertScriptRaise(mt_base_p1 + "5 late() { mut = sim.mutations[0:4]; mut.setDominanceForTrait(0, c(1,INF,3,INF,5)); }", "infinite after setDominanceForTrait()", __LINE__);
		
		// MutationType defaultDominanceForTrait() and setDefaultDominanceForTrait()
		SLiMAssertScriptSuccess(mt_base_p1 + "initialize() { if (!identical(m1.defaultDominanceForTrait(0), 0.5)) stop(); } 5 late() { }");
		SLiMAssertScriptSuccess(mt_base_p1 + "initialize() { if (!identical(m1.defaultDominanceForTrait(1), 0.5)) stop(); } 5 late() { }");
		SLiMAssertScriptSuccess(mt_base_p1 + "initialize() { if (!identical(m1.defaultDominanceForTrait(c(0,1)), c(0.5, 0.5))) stop(); } 5 late() { }");
		SLiMAssertScriptSuccess(mt_base_p1 + "initialize() { m1.setDefaultDominanceForTrait(0, 0.25); } 5 late() { mut = sim.mutations[0]; if (!identical(mut.dominanceForTrait(NULL), c(0.25, 0.5))) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "initialize() { m1.setDefaultDominanceForTrait(1, 0.25); } 5 late() { mut = sim.mutations[0]; if (!identical(mut.dominanceForTrait(NULL), c(0.5, 0.25))) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "initialize() { m1.setDefaultDominanceForTrait(NULL, c(0.25, 1.0)); } 5 late() { mut = sim.mutations[0]; if (!identical(mut.dominanceForTrait(NULL), c(0.25, 1.0))) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "initialize() { m1.setDefaultDominanceForTrait(c(0,1), c(0.25, 1.0)); } 5 late() { mut = sim.mutations[0]; if (!identical(mut.dominanceForTrait(c(0,1)), c(0.25, 1.0))) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "initialize() { m1.setDefaultDominanceForTrait(c(1,0), c(0.25, 1.0)); } 5 late() { mut = sim.mutations[0]; if (!identical(mut.dominanceForTrait(c(1,0)), c(0.25, 1.0))) stop(); }");
		SLiMAssertScriptStop(mt_base_p1 + "initialize() { m1.setDefaultDominanceForTrait(c(1,0), c(0.25, 1.0)); } 5 late() { mut = sim.mutations[0]; if (!identical(mut.dominanceForTrait(c(0,1)), c(0.25, 1.0))) stop(); }");
		// NAN for dominance means independent dominance, so it is legal; tested below
		SLiMAssertScriptRaise(mt_base_p1 + "initialize() { m1.setDefaultDominanceForTrait(0, INF); }", "infinite after setDefaultDominanceForTrait()", __LINE__);
		SLiMAssertScriptRaise(mt_base_p1 + "initialize() { m1.setDefaultDominanceForTrait(NULL, c(0.25, INF)); }", "infinite after setDefaultDominanceForTrait()", __LINE__);
		
		// Mutation hemizygousDominanceForTrait()
		SLiMAssertScriptSuccess(mt_base_p1 + "5 late() { mut = sim.mutations[0]; if (!identical(mut.hemizygousDominanceForTrait(0), 1.0)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "5 late() { mut = sim.mutations[0]; if (!identical(mut.hemizygousDominanceForTrait(1), 1.0)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "5 late() { mut = sim.mutations[0]; if (!identical(mut.hemizygousDominanceForTrait(NULL), c(1.0, 1.0))) stop(); }");
		
		// Mutation setHemizygousDominanceForTrait()
		SLiMAssertScriptSuccess(mt_base_p1 + "5 late() { mut = sim.mutations[0:4]; mut.setHemizygousDominanceForTrait(0, 3); mut.setHemizygousDominanceForTrait(1, 4.5); if (!identical(mut.hemizygousDominanceForTrait(NULL), rep(c(3, 4.5), 5))) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "5 late() { mut = sim.mutations[0:4]; mut.setHemizygousDominanceForTrait(0, 1:5 * 2 - 1); mut.setHemizygousDominanceForTrait(1, 1:5 * 2); if (!identical(mut.hemizygousDominanceForTrait(NULL), 1.0:10)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "5 late() { mut = sim.mutations[0:4]; mut.setHemizygousDominanceForTrait(NULL, 1:10); if (!identical(mut.hemizygousDominanceForTrait(NULL), 1.0:10)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "5 late() { mut = sim.mutations[0:4]; mut.setHemizygousDominanceForTrait(NULL, 1:10 + 0.5); if (!identical(mut.hemizygousDominanceForTrait(NULL), 1:10 + 0.5)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "5 late() { mut = sim.mutations[0:4]; mut.setHemizygousDominanceForTrait(c(0,1), 1:10); if (!identical(mut.hemizygousDominanceForTrait(NULL), 1.0:10)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "5 late() { mut = sim.mutations[0:4]; mut.setHemizygousDominanceForTrait(c(0,1), 1:10 + 0.5); if (!identical(mut.hemizygousDominanceForTrait(NULL), 1:10 + 0.5)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "5 late() { mut = sim.mutations[0:4]; mut.setHemizygousDominanceForTrait(c(1,0), 1:10); if (!identical(mut.hemizygousDominanceForTrait(c(1,0)), 1.0:10)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "5 late() { mut = sim.mutations[0:4]; mut.setHemizygousDominanceForTrait(c(1,0), 1:10 + 0.5); if (!identical(mut.hemizygousDominanceForTrait(c(1,0)), 1:10 + 0.5)) stop(); }");
		// hemizygous dominance cannot be NAN, since independent dominance has no meaning for it
		SLiMAssertScriptRaise(mt_base_p1 + "5 late() { mut = sim.mutations[0:4]; mut.setHemizygousDominanceForTrait(0, NAN); }", "non-finite after setHemizygousDominanceForTrait()", __LINE__);
		SLiMAssertScriptRaise(mt_base_p1 + "5 late() { mut = sim.mutations[0:4]; mut.setHemizygousDominanceForTrait(0, c(1,NAN,3,NAN,5)); }", "non-finite after setHemizygousDominanceForTrait()", __LINE__);
		SLiMAssertScriptRaise(mt_base_p1 + "5 late() { mut = sim.mutations[0:4]; mut.setHemizygousDominanceForTrait(0, INF); }", "non-finite after setHemizygousDominanceForTrait()", __LINE__);
		SLiMAssertScriptRaise(mt_base_p1 + "5 late() { mut = sim.mutations[0:4]; mut.setHemizygousDominanceForTrait(0, c(1,INF,3,INF,5)); }", "non-finite after setHemizygousDominanceForTrait()", __LINE__);
		
		// MutationType defaultHemizygousDominanceForTrait() and setDefaultHemizygousDominanceForTrait()
		// note that `m1.convertToSubstitution = F` is needed in some cases, because baseline accumulation will raise an error if substitution occurs with a hemizygous dominance coefficient != 1.0
		SLiMAssertScriptSuccess(mt_base_p1 + "initialize() { if (!identical(m1.defaultHemizygousDominanceForTrait(0), 1.0)) stop(); } 5 late() { }");
		SLiMAssertScriptSuccess(mt_base_p1 + "initialize() { if (!identical(m1.defaultHemizygousDominanceForTrait(1), 1.0)) stop(); } 5 late() { }");
		SLiMAssertScriptSuccess(mt_base_p1 + "initialize() { if (!identical(m1.defaultHemizygousDominanceForTrait(c(0,1)), c(1.0, 1.0))) stop(); } 5 late() { }");
		SLiMAssertScriptSuccess(mt_base_p1 + "initialize() { m1.convertToSubstitution = F; m1.setDefaultHemizygousDominanceForTrait(0, 0.5); } 5 late() { mut = sim.mutations[0]; if (!identical(mut.hemizygousDominanceForTrait(NULL), c(0.5, 1.0))) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "initialize() { m1.convertToSubstitution = F; m1.setDefaultHemizygousDominanceForTrait(1, 0.5); } 5 late() { mut = sim.mutations[0]; if (!identical(mut.hemizygousDominanceForTrait(NULL), c(1.0, 0.5))) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "initialize() { m1.convertToSubstitution = F; m1.setDefaultHemizygousDominanceForTrait(NULL, c(0.25, 0.5)); } 5 late() { mut = sim.mutations[0]; if (!identical(mut.hemizygousDominanceForTrait(NULL), c(0.25, 0.5))) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "initialize() { m1.convertToSubstitution = F; m1.setDefaultHemizygousDominanceForTrait(c(0,1), c(0.25, 0.5)); } 5 late() { mut = sim.mutations[0]; if (!identical(mut.hemizygousDominanceForTrait(c(0,1)), c(0.25, 0.5))) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "initialize() { m1.convertToSubstitution = F; m1.setDefaultHemizygousDominanceForTrait(c(1,0), c(0.25, 0.5)); } 5 late() { mut = sim.mutations[0]; if (!identical(mut.hemizygousDominanceForTrait(c(1,0)), c(0.25, 0.5))) stop(); }");
		SLiMAssertScriptStop(mt_base_p1 + "initialize() { m1.convertToSubstitution = F; m1.setDefaultHemizygousDominanceForTrait(c(1,0), c(0.25, 0.5)); } 5 late() { mut = sim.mutations[0]; if (!identical(mut.hemizygousDominanceForTrait(c(0,1)), c(0.25, 0.5))) stop(); }");
		// hemizygous dominance cannot be NAN, since independent dominance has no meaning for it
		SLiMAssertScriptRaise(mt_base_p1 + "initialize() { m1.setDefaultHemizygousDominanceForTrait(0, NAN); }", "non-finite after setDefaultHemizygousDominanceForTrait()", __LINE__);
		SLiMAssertScriptRaise(mt_base_p1 + "initialize() { m1.setDefaultHemizygousDominanceForTrait(NULL, c(0.25, NAN)); }", "non-finite after setDefaultHemizygousDominanceForTrait()", __LINE__);
		SLiMAssertScriptRaise(mt_base_p1 + "initialize() { m1.setDefaultHemizygousDominanceForTrait(0, INF); }", "non-finite after setDefaultHemizygousDominanceForTrait()", __LINE__);
		SLiMAssertScriptRaise(mt_base_p1 + "initialize() { m1.setDefaultHemizygousDominanceForTrait(NULL, c(0.25, INF)); }", "non-finite after setDefaultHemizygousDominanceForTrait()", __LINE__);
		
		// Substitution effectSizeForTrait()
		SLiMAssertScriptSuccess(mt_base_p1 + "200 late() { sub = sim.substitutions[0]; if (!identical(sub.effectSizeForTrait(0), 0.0)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "200 late() { sub = sim.substitutions[0]; if (!identical(sub.effectSizeForTrait(1), 0.0)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "200 late() { sub = sim.substitutions[0]; if (!identical(sub.effectSizeForTrait(NULL), c(0.0, 0.0))) stop(); }");
		
		// Substitution dominanceForTrait()
		SLiMAssertScriptSuccess(mt_base_p1 + "200 late() { sub = sim.substitutions[0]; if (!identical(sub.dominanceForTrait(0), 0.5)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "200 late() { sub = sim.substitutions[0]; if (!identical(sub.dominanceForTrait(1), 0.5)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "200 late() { sub = sim.substitutions[0]; if (!identical(sub.dominanceForTrait(NULL), c(0.5, 0.5))) stop(); }");
		
		// Substitution hemizygousDominanceForTrait()
		SLiMAssertScriptSuccess(mt_base_p1 + "200 late() { sub = sim.substitutions[0]; if (!identical(sub.hemizygousDominanceForTrait(0), 1.0)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "200 late() { sub = sim.substitutions[0]; if (!identical(sub.hemizygousDominanceForTrait(1), 1.0)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "200 late() { sub = sim.substitutions[0]; if (!identical(sub.hemizygousDominanceForTrait(NULL), c(1.0, 1.0))) stop(); }");
		
		// Mutation <trait-name>EffectSize property
		SLiMAssertScriptSuccess(mt_base_p1 + "5 late() { mut = sim.mutations[0]; if (!identical(mut.heightEffectSize, 0.0)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "5 late() { mut = sim.mutations[0]; if (!identical(mut.weightEffectSize, 0.0)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "5 late() { mut = sim.mutations[0]; mut.heightEffectSize = 0.25; if (!identical(mut.heightEffectSize, 0.25)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "5 late() { mut = sim.mutations[0]; mut.weightEffectSize = 0.25; if (!identical(mut.weightEffectSize, 0.25)) stop(); }");
		SLiMAssertScriptRaise(mt_base_p1 + "5 late() { mut = sim.mutations[0]; mut.heightEffectSize = NAN; }", "required to be a finite value (not INF or NAN)", __LINE__);
		SLiMAssertScriptRaise(mt_base_p1 + "5 late() { mut = sim.mutations[0]; mut.heightEffectSize = INF; }", "required to be a finite value (not INF or NAN)", __LINE__);
		
		// Mutation <trait-name>Dominance property
		SLiMAssertScriptSuccess(mt_base_p1 + "5 late() { mut = sim.mutations[0]; if (!identical(mut.heightDominance, 0.5)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "5 late() { mut = sim.mutations[0]; if (!identical(mut.weightDominance, 0.5)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "5 late() { mut = sim.mutations[0]; mut.heightDominance = 0.25; if (!identical(mut.heightDominance, 0.25)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "5 late() { mut = sim.mutations[0]; mut.weightDominance = 0.25; if (!identical(mut.weightDominance, 0.25)) stop(); }");
		// NAN for dominance means independent dominance, so it is legal; tested below
		SLiMAssertScriptRaise(mt_base_p1 + "5 late() { mut = sim.mutations[0]; mut.heightDominance = INF; }", "required to be finite or NAN", __LINE__);
		
		// Mutation <trait-name>HemizygousDominance property
		SLiMAssertScriptSuccess(mt_base_p1 + "5 late() { mut = sim.mutations[0]; if (!identical(mut.heightHemizygousDominance, 1.0)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "5 late() { mut = sim.mutations[0]; if (!identical(mut.weightHemizygousDominance, 1.0)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "5 late() { mut = sim.mutations[0]; mut.heightHemizygousDominance = 0.25; if (!identical(mut.heightHemizygousDominance, 0.25)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "5 late() { mut = sim.mutations[0]; mut.weightHemizygousDominance = 0.25; if (!identical(mut.weightHemizygousDominance, 0.25)) stop(); }");
		// hemizygous dominance cannot be NAN, since independent dominance has no meaning for it
		SLiMAssertScriptRaise(mt_base_p1 + "5 late() { mut = sim.mutations[0]; mut.heightHemizygousDominance = NAN; }", "required to be finite or NAN", __LINE__);
		SLiMAssertScriptRaise(mt_base_p1 + "5 late() { mut = sim.mutations[0]; mut.heightHemizygousDominance = INF; }", "required to be finite or NAN", __LINE__);
		
		// Substitution <trait-name>EffectSize property
		SLiMAssertScriptSuccess(mt_base_p1 + "200 late() { sub = sim.substitutions[0]; if (!identical(sub.heightEffectSize, 0.0)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "200 late() { sub = sim.substitutions[0]; if (!identical(sub.weightEffectSize, 0.0)) stop(); }");
		
		// Substitution <trait-name>Dominance property
		SLiMAssertScriptSuccess(mt_base_p1 + "200 late() { sub = sim.substitutions[0]; if (!identical(sub.heightDominance, 0.5)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "200 late() { sub = sim.substitutions[0]; if (!identical(sub.weightDominance, 0.5)) stop(); }");
		
		// Substitution <trait-name>HemizygousDominance property
		SLiMAssertScriptSuccess(mt_base_p1 + "200 late() { sub = sim.substitutions[0]; if (!identical(sub.heightHemizygousDominance, 1.0)) stop(); }");
		SLiMAssertScriptSuccess(mt_base_p1 + "200 late() { sub = sim.substitutions[0]; if (!identical(sub.weightHemizygousDominance, 1.0)) stop(); }");
	}
	
	// Test independent dominance and new Mutation and MutationType APIs
	SLiMAssertScriptStop("initialize() { initializeMutationType('m1', 0.5); if (m1.defaultDominanceForTrait(0) == 0.5) stop(); }");
	SLiMAssertScriptStop("initialize() { initializeMutationType('m1', NAN); if (isNAN(m1.defaultDominanceForTrait(0))) stop(); }");
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', INF); }", "requires defaultDominance to be finite", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeMutationType('m1', 0.5); m1.setDefaultDominanceForTrait(0, NAN); if (isNAN(m1.defaultDominanceForTrait(0))) stop(); }");
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5); m1.setDefaultDominanceForTrait(0, INF); }", "default dominance is infinite", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5); m1.setDefaultHemizygousDominanceForTrait(0, NAN); }", "hemizygous dominance is non-finite", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5); m1.setDefaultHemizygousDominanceForTrait(0, INF); }", "hemizygous dominance is non-finite", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTrait('A', 'm'); initializeTrait('B', 'm'); initializeMutationType('m1', 0.5); if (m1.defaultDominanceForTrait('A') == 0.5) stop(); }");
	SLiMAssertScriptStop("initialize() { initializeTrait('A', 'm'); initializeTrait('B', 'm'); initializeMutationType('m1', NAN); if (isNAN(m1.defaultDominanceForTrait('A'))) stop(); }");
	SLiMAssertScriptStop("initialize() { initializeTrait('A', 'm'); initializeTrait('B', 'm'); initializeMutationType('m1', 0.5); if (identical(m1.defaultDominanceForTrait(), c(0.5,0.5))) stop(); }");
	SLiMAssertScriptStop("initialize() { initializeTrait('A', 'm'); initializeTrait('B', 'm'); initializeMutationType('m1', NAN); if (identical(m1.defaultDominanceForTrait(), c(NAN,NAN))) stop(); }");
	SLiMAssertScriptStop("initialize() { initializeTrait('A', 'm'); initializeTrait('B', 'm'); initializeMutationType('m1', 0.5); m1.setDefaultDominanceForTrait(c('A','B'), c(0.25, 0.75)); if (identical(m1.defaultDominanceForTrait(), c(0.25, 0.75))) stop(); }");
	SLiMAssertScriptStop("initialize() { initializeTrait('A', 'm'); initializeTrait('B', 'm'); initializeMutationType('m1', 0.5); m1.setDefaultDominanceForTrait(c('B','A'), c(0.25, 0.75)); if (identical(m1.defaultDominanceForTrait(), c(0.75, 0.25))) stop(); }");
	SLiMAssertScriptStop("initialize() { initializeTrait('A', 'm'); initializeTrait('B', 'm'); initializeMutationType('m1', 0.5); m1.setDefaultDominanceForTrait(c('B','A'), c(0.25, 0.75)); if (identical(m1.defaultDominanceForTrait(c('B','A')), c(0.25, 0.75))) stop(); }");
	SLiMAssertScriptStop("initialize() { initializeTrait('A', 'm'); initializeTrait('B', 'm'); initializeMutationType('m1', 0.5); m1.setDefaultDominanceForTrait(c('A','B'), c(NAN, NAN)); if (identical(m1.defaultDominanceForTrait(), c(NAN, NAN))) stop(); }");
	SLiMAssertScriptStop("initialize() { initializeTrait('A', 'm'); initializeTrait('B', 'm'); initializeMutationType('m1', 0.5); m1.setDefaultDominanceForTrait(c('A'), NAN); if (identical(m1.defaultDominanceForTrait(), c(NAN, 0.5))) stop(); }");
	SLiMAssertScriptStop("initialize() { initializeTrait('A', 'm'); initializeTrait('B', 'm'); initializeMutationType('m1', 0.5); m1.setDefaultDominanceForTrait(c('A','B'), c(0.5, NAN)); if (identical(m1.defaultDominanceForTrait(), c(0.5, NAN))) stop(); }");
	SLiMAssertScriptStop("initialize() { initializeTrait('A', 'm'); initializeTrait('B', 'm'); initializeMutationType('m1', NAN); m1.setDefaultDominanceForTrait(c('A','B'), c(0.5, 0.5)); if (identical(m1.defaultDominanceForTrait(), c(0.5, 0.5))) stop(); }");
	SLiMAssertScriptStop("initialize() { initializeTrait('A', 'm'); initializeTrait('B', 'm'); initializeMutationType('m1', NAN); m1.setDefaultDominanceForTrait(c('A'), 0.5); if (identical(m1.defaultDominanceForTrait(), c(0.5, NAN))) stop(); }");
	SLiMAssertScriptStop("initialize() { initializeTrait('A', 'm'); initializeTrait('B', 'm'); initializeMutationType('m1', NAN); m1.setDefaultDominanceForTrait(c('A','B'), c(0.5, NAN)); if (identical(m1.defaultDominanceForTrait(), c(0.5, NAN))) stop(); }");
	
	std::string middle = " initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeMutationRate(1e-4); } 1 late() { sim.addSubpop('p1', 10); } 2 late() { muts = sim.mutations; ";
	
	SLiMAssertScriptStop("initialize() { initializeMutationType('m1', 0.5, 'f', 0.0);" + middle + "if (all(muts.isNeutral == T)) stop(); }");
	SLiMAssertScriptStop("initialize() { initializeMutationType('m1', 0.5, 'f', 0.0);" + middle + "if (all(muts.isIndependentDominanceForTrait(0) == F)) stop(); }");
	SLiMAssertScriptStop("initialize() { initializeMutationType('m1', 0.5, 'f', 0.0);" + middle + "if (all(muts.dominance == 0.5)) stop(); }");
	SLiMAssertScriptStop("initialize() { initializeMutationType('m1', 0.5, 'f', 0.0);" + middle + "if (all(muts.effectSize == 0.0)) stop(); }");
	SLiMAssertScriptStop("initialize() { initializeMutationType('m1', 0.5, 'f', 0.0001);" + middle + "if (all(muts.isNeutral == F)) stop(); }");
	SLiMAssertScriptStop("initialize() { initializeMutationType('m1', 0.5, 'f', 0.0001);" + middle + "if (all(muts.isIndependentDominanceForTrait(0) == F)) stop(); }");
	SLiMAssertScriptStop("initialize() { initializeMutationType('m1', 0.5, 'f', 0.0001);" + middle + "if (all(muts.dominance == 0.5)) stop(); }");
	SLiMAssertScriptStop("initialize() { initializeMutationType('m1', 0.5, 'f', 0.0001);" + middle + "if (allClose(muts.effectSize, 0.0001)) stop(); }");
	SLiMAssertScriptStop("initialize() { initializeMutationType('m1', NAN, 'f', 0.0);" + middle + "if (all(muts.isNeutral == T)) stop(); }");
	SLiMAssertScriptStop("initialize() { initializeMutationType('m1', NAN, 'f', 0.0);" + middle + "if (all(muts.isIndependentDominanceForTrait(0) == T)) stop(); }");
	SLiMAssertScriptStop("initialize() { initializeMutationType('m1', NAN, 'f', 0.0);" + middle + "if (all(muts.dominance == 0.5)) stop(); }");
	SLiMAssertScriptStop("initialize() { initializeMutationType('m1', NAN, 'f', 0.0);" + middle + "if (all(muts.effectSize == 0.0)) stop(); }");
	SLiMAssertScriptStop("initialize() { initializeMutationType('m1', NAN, 'f', 0.0001);" + middle + "if (all(muts.isNeutral == F)) stop(); }");
	SLiMAssertScriptStop("initialize() { initializeMutationType('m1', NAN, 'f', 0.0001);" + middle + "if (all(muts.isIndependentDominanceForTrait(0) == T)) stop(); }");
	SLiMAssertScriptStop("initialize() { initializeMutationType('m1', NAN, 'f', 0.0001);" + middle + "if (allClose(muts.dominance, 0.4999875)) stop(); }");		// h = (sqrt(1+s)-1)/s
	SLiMAssertScriptStop("initialize() { initializeMutationType('m1', NAN, 'f', 0.0001);" + middle + "if (allClose(muts.effectSize, 0.0001)) stop(); }");
	
	SLiMAssertScriptStop("initialize() { initializeMutationType('m1', NAN, 'f', 0.0001);" + middle + "muts.setDominanceForTrait(0, 0.5); if (all(muts.dominance == 0.5)) stop(); }");
	SLiMAssertScriptStop("initialize() { initializeTrait('height', 'm'); initializeMutationType('m1', NAN, 'f', 0.0001);" + middle + "muts.heightDominance = 0.5; if (all(muts.dominance == 0.5)) stop(); }");
	SLiMAssertScriptStop("initialize() { initializeTrait('height', 'm'); initializeMutationType('m1', NAN, 'f', 0.0001);" + middle + "muts.heightDominance = 0.5; if (all(muts.heightDominance == 0.5)) stop(); }");
	SLiMAssertScriptStop("initialize() { initializeMutationType('m1', 0.5, 'f', 0.0001);" + middle + "muts.setDominanceForTrait(0, NAN); if (allClose(muts.dominance, 0.4999875)) stop(); }");
	SLiMAssertScriptStop("initialize() { initializeTrait('height', 'm'); initializeMutationType('m1', 0.5, 'f', 0.0001);" + middle + "muts.heightDominance = NAN; if (allClose(muts.dominance, 0.4999875)) stop(); }");
	SLiMAssertScriptStop("initialize() { initializeTrait('height', 'm'); initializeMutationType('m1', 0.5, 'f', 0.0001);" + middle + "muts.heightDominance = NAN; if (allClose(muts.heightDominance, 0.4999875)) stop(); }");
	
	// Test the new Individual cachedFitness property and crosscheck it against the Subpopulation cachedFitness() method
	std::string cachedFitness1 =	// neutral but with fitnessScaling
	R"V0G0N(
		initialize() {
			initializeMutationRate(1e-7);
			initializeMutationType("m1", 0.5, "f", 0.0);
			initializeGenomicElementType("g1", m1, 1.0);
			initializeGenomicElement(g1, 0, 9999999);
			initializeRecombinationRate(1e-8);
		}
		1 early() {
			sim.addSubpop("p1", 50);
		}
		1:10 early() {
			// will affect the next generation, not this one
			p1.fitnessScaling = runif(1, 0.1, 1.0);
			p1.individuals.fitnessScaling = runif(50, 0.1, 1.0);
			
			f1 = p1.cachedFitness(NULL);
			f2 = p1.cachedFitness(0:49);
			f3 = p1.cachedFitness(p1.individuals);
			f4 = p1.individuals.cachedFitness;
			
			if (!identical(f1, f2, f3, f4))
				stop();
		}
	)V0G0N";
	
	SLiMAssertScriptSuccess(cachedFitness1);
	
	std::string cachedFitness2 =	// non-neutral
	R"V0G0N(
		initialize() {
			initializeMutationRate(1e-7);
			initializeMutationType("m1", 0.5, "n", 0.0, 0.01);
			initializeGenomicElementType("g1", m1, 1.0);
			initializeGenomicElement(g1, 0, 9999999);
			initializeRecombinationRate(1e-8);
		}
		1 early() {
			sim.addSubpop("p1", 50);
		}
		1:10 early() {
			f1 = p1.cachedFitness(NULL);
			f2 = p1.cachedFitness(0:49);
			f3 = p1.cachedFitness(p1.individuals);
			f4 = p1.individuals.cachedFitness;
			
			if (!identical(f1, f2, f3, f4))
				stop();
		}
	)V0G0N";
	
	SLiMAssertScriptSuccess(cachedFitness2);
	
	std::string cachedFitness3 =	// non-neutral with independent dominance
	R"V0G0N(
		initialize() {
			initializeMutationRate(1e-7);
			initializeMutationType("m1", NAN, "n", 0.0, 0.01);
			initializeGenomicElementType("g1", m1, 1.0);
			initializeGenomicElement(g1, 0, 9999999);
			initializeRecombinationRate(1e-8);
		}
		1 early() {
			sim.addSubpop("p1", 50);
		}
		1:10 early() {
			f1 = p1.individuals.cachedFitness;
			f2 = sapply(p1.individuals, "muts = applyValue.haplosomes.mutations; product(1.0 + muts.effectSize * muts.dominance);");
			if (!allClose(f1, f2))
				stop();
		}
	)V0G0N";
	
	SLiMAssertScriptSuccess(cachedFitness3);
	
	// test the new zygosityOfMutations() method
	std::string test_zygosity1 =	// simple one-chromosome diploid test
	R"V0G0N(
		initialize() {
			initializeMutationRate(1e-7);
			initializeMutationType("m1", 0.5, "n", 0.0, 0.01);
			initializeGenomicElementType("g1", m1, 1.0);
			initializeGenomicElement(g1, 0, 9999999);
			initializeRecombinationRate(1e-8);
		}
		1 early() {
			sim.addSubpop("p1", 10);
		}
		20 late() {
			inds = p1.individuals;
			muts = sim.mutations;
			
			// calculate zygosity
			z = inds.zygosityOfMutations(NULL);
			
			// cross-check row sums: occurrence counts of each mutation
			mutCounts1 = rowSums(z);
			mutCounts2 = sim.mutationCounts(p1, NULL);
			if (!identical(mutCounts1, mutCounts2))
				stop("individual mutation counts do not match");
			
			// cross-check column sums: the number of mutations per individual
			indCounts1 = colSums(z);
			indCounts2 = sapply(inds, "applyValue.haplosomes.mutations.size();");
			if (!identical(indCounts1, indCounts2))
				stop("individual mutation counts do not match");
			
			// cross-check against a zygosity matrix from containsMutations()
			m = NULL;
			for (ind in inds)
			{
				counts_h0 = ind.haplosomes[0].containsMutations(muts);
				counts_h1 = ind.haplosomes[1].containsMutations(muts);
				zygosity = counts_h0 + counts_h1;
				m = cbind(m, zygosity);
			}
			if (!identical(z, m))
				stop("zygosity matrices do not match");
		}
	)V0G0N";
	
	SLiMAssertScriptSuccess(test_zygosity1);

	std::string test_zygosity2 =	// multiple chromosomes of different types
	R"V0G0N(
		initialize() {
			initializeSex();
			initializeMutationType("m1", 0.5, "n", 0.0, 0.01);
			initializeGenomicElementType("g1", m1, 1.0);
			
			ids = 1:6;
			symbols = c(1:3, "X", "Y", "MT");
			lengths = rdunif(6, 2e7, 4e7);
			types = c(rep("A", 3), "X", "Y", "H");
			
			for (id in ids, symbol in symbols, length in lengths, type in types)
			{
				initializeChromosome(id, length, type, symbol);
				initializeMutationRate(1e-7);
				initializeRecombinationRate(1e-8);   // not used for the Y
				initializeGenomicElement(g1);
			}
		}
		1 early() {
			sim.addSubpop("p1", 10);
		}
		20 late() {
			inds = p1.individuals;
			muts = sim.mutations;
			
			// calculate zygosity
			z = inds.zygosityOfMutations(NULL);
			
			// cross-check row sums: occurrence counts of each mutation
			mutCounts1 = rowSums(z);
			mutCounts2 = sim.mutationCounts(p1, NULL);
			if (!identical(mutCounts1, mutCounts2))
				stop("individual mutation counts do not match");
			
			// cross-check column sums: the number of mutations per individual
			indCounts1 = colSums(z);
			indCounts2 = sapply(inds, "applyValue.haplosomesNonNull.mutations.size();");
			if (!identical(indCounts1, indCounts2))
				stop("individual mutation counts do not match");
			
			// cross-check against a zygosity matrix from containsMutations()
			m = NULL;
			for (ind in inds)
			{
				zygosity = 0;
				
				for (chromosome in sim.chromosomes)
				{
					chr_haplosomes = ind.haplosomesForChromosomes(chromosome, includeNulls=F);
					
					if (length(chr_haplosomes) == 0)
						next;
					
					chr_muts_indices = which(muts.chromosome == chromosome);
					chr_muts = muts[chr_muts_indices];
					
					for (hap in chr_haplosomes)
					{
						chr_muts_counts = hap.containsMutations(chr_muts);
						all_muts_counts = rep(0, length(muts));
						all_muts_counts[chr_muts_indices] = chr_muts_counts;
						zygosity = zygosity + all_muts_counts;
					}
				}
				
				m = cbind(m, zygosity);
			}
			if (!identical(z, m))
				stop("zygosity matrices do not match");
		}
	)V0G0N";
	
	SLiMAssertScriptSuccess(test_zygosity2);

	std::string test_zygosity3 =	// same but passing mutations=mut explicitly, different code path
	R"V0G0N(
		initialize() {
			initializeSex();
			initializeMutationType("m1", 0.5, "n", 0.0, 0.01);
			initializeGenomicElementType("g1", m1, 1.0);
			
			ids = 1:6;
			symbols = c(1:3, "X", "Y", "MT");
			lengths = rdunif(6, 2e7, 4e7);
			types = c(rep("A", 3), "X", "Y", "H");
			
			for (id in ids, symbol in symbols, length in lengths, type in types)
			{
				initializeChromosome(id, length, type, symbol);
				initializeMutationRate(1e-7);
				initializeRecombinationRate(1e-8);   // not used for the Y
				initializeGenomicElement(g1);
			}
		}
		1 early() {
			sim.addSubpop("p1", 10);
		}
		20 late() {
			inds = p1.individuals;
			muts = sim.mutations;
			
			// calculate zygosity
			z = inds.zygosityOfMutations(NULL);
			
			// cross-check row sums: occurrence counts of each mutation
			mutCounts1 = rowSums(z);
			mutCounts2 = sim.mutationCounts(p1, NULL);
			if (!identical(mutCounts1, mutCounts2))
				stop("individual mutation counts do not match");
			
			// cross-check column sums: the number of mutations per individual
			indCounts1 = colSums(z);
			indCounts2 = sapply(inds, "applyValue.haplosomesNonNull.mutations.size();");
			if (!identical(indCounts1, indCounts2))
				stop("individual mutation counts do not match");
			
			// cross-check against a zygosity matrix from containsMutations()
			m = NULL;
			for (ind in inds)
			{
				zygosity = 0;
				
				for (chromosome in sim.chromosomes)
				{
					chr_haplosomes = ind.haplosomesForChromosomes(chromosome, includeNulls=F);
					
					if (length(chr_haplosomes) == 0)
						next;
					
					chr_muts_indices = which(muts.chromosome == chromosome);
					chr_muts = muts[chr_muts_indices];
					
					for (hap in chr_haplosomes)
					{
						chr_muts_counts = hap.containsMutations(chr_muts);
						all_muts_counts = rep(0, length(muts));
						all_muts_counts[chr_muts_indices] = chr_muts_counts;
						zygosity = zygosity + all_muts_counts;
					}
				}
				
				m = cbind(m, zygosity);
			}
			if (!identical(z, m))
				stop("zygosity matrices do not match");
		}
	)V0G0N";
	
	SLiMAssertScriptSuccess(test_zygosity3);
	
	std::string test_zygosity4 =	// test specified values for hemizygosity and haploidy
	R"V0G0N(
		initialize() {
			initializeSex();
			initializeMutationType("m1", 0.5, "n", 0.0, 0.01);
			initializeGenomicElementType("g1", m1, 1.0);
			
			ids = 1:6;
			symbols = c(1:3, "X", "Y", "MT");
			lengths = rdunif(6, 2e7, 4e7);
			types = c(rep("A", 3), "X", "Y", "H");
			
			for (id in ids, symbol in symbols, length in lengths, type in types)
			{
				initializeChromosome(id, length, type, symbol);
				initializeMutationRate(1e-7);
				initializeRecombinationRate(1e-8);   // not used for the Y
				initializeGenomicElement(g1);
			}
		}
		1 early() {
			sim.addSubpop("p1", 10);
		}
		20 late() {
			inds = p1.individuals;
			muts = sample(sim.mutations, 1000, replace=T);
			
			// calculate zygosity
			z = inds.zygosityOfMutations(muts, hemizygousValue=3, haploidValue=4);
			
			// check that the correct values were used for each mutation
			for (mut in muts, index in seqAlong(muts))
			{
				chr = mut.chromosome;
				col = z[index,];
				u = sort(unique(col, preserveOrder=F));
				if (chr.type == "A")
					expected = c(0, 1, 2);
				else if (chr.type == "X")
					expected = c(0, 1, 2, 3);
				else if (chr.type == "Y")
					expected = c(0, 4);
				else if (chr.type == "H")
					expected = c(0, 4);
				
				if (any(match(u, expected) == -1))
					stop("for chromosome type " + chr.type + " expected (" +
						paste(expected, sep=", ") + ") but saw (" +
						paste(u, sep=", ") + ")");
			}
		}
	)V0G0N";
	
	SLiMAssertScriptSuccess(test_zygosity4);
	
	
	// =========================================================================================================
	//
	// Below is a set of test models that will (hopefully) exercise all the different code paths for trait
	// evaluation -- callbacks, different DES combinations, neutral and non-neutral, etc.  These models are
	// intended both to test themselves to some extent, especially in DEBUG builds using internal properties
	// for diagnostics, and also to be tested by _CheckPhenotypeForTrait()'s cross-checks in DEBUG builds.
	//
	// =========================================================================================================
	
	// a completely neutral model using the default trait (no initializeTrait() call)
	// - fitness values should be 1.0 for all individuals
	// - the trait should be "super-pure-neutral" and thus skipped and uncalculated (and thus NAN)
	// - this model should not allocate any non-neutral caches, and should be in the kPureNeutral regime
	std::string multitrait_DEFAULT_NEUTRAL =
		R"V0G0N(
// multitrait_DEFAULT_NEUTRAL
initialize() {
	initializeMutationType("m1", 0.5, "f", 0.0);
	initializeGenomicElementType("g1", m1, 1.0);
	initializeGenomicElement(g1, 0, 999999);
	initializeMutationRate(1e-7);
	initializeRecombinationRate(1e-8);
}
1 early() { sim.addSubpop("p1", 50); }
2: first() {
	inds = p1.individuals;
	for (ind in inds) {
		ind_t1 = ind.phenotypeForTrait("simT");   // dynamic property not defined for the default trait
		ind_fitness = ind.cachedFitness;
		
		if (!isNAN(ind_t1))
			stop("t1 value mismatch (NAN expected): " + ind_t1);
		if (ind_fitness != 1.0)
			stop("fitness mismatch (1.0 expected): " + ind_fitness);
	}
	if (sim._debugBuild) {
		if (sim._allocatedNonneutralCacheCount != 0)
			stop("nonneutral caches were allocated despite all neutral genetics");
		if (sim._traitCalculationRegimeName != "kPureNeutral")
			stop("unexpected trait calculation regime");
	}
}
100 late() { }
		)V0G0N";
	
	SLiMAssertScriptSuccess(multitrait_DEFAULT_NEUTRAL);
	
	
	// a completely neutral model using the default trait (no initializeTrait() call), with a subpopulation
	// fitnessScaling value and several tricky mutationEffect() callbacks to obfuscate inference.
	// - fitness values should be 1.0 for all individuals
	// - the trait should be "super-pure-neutral" and thus skipped and uncalculated (and thus NAN)
	// - this model should not allocate any non-neutral caches, and should be in the kPureNeutral regime
	std::string multitrait_DEFAULT_NEUTRAL_OBFUSCATED =
		R"V0G0N(
// multitrait_DEFAULT_NEUTRAL_OBFUSCATED
initialize() {
	initializeMutationType("m1", 0.5, "f", 0.0);
	initializeGenomicElementType("g1", m1, 1.0);
	initializeGenomicElement(g1, 0, 999999);
	initializeMutationRate(1e-7);
	initializeRecombinationRate(1e-8);
}
1 early() { sim.addSubpop("p1", 50); }
late() { p1.fitnessScaling = 1.25; }
mutationEffect(m1) { return NULL; }
mutationEffect(m1) { return 1.0; }
mutationEffect(m1, p1) { return 1.0; }
mutationEffect(m1, p2) { return effect; }   // considered non-neutral, but subpop nonexistent
2: first() {
	inds = p1.individuals;
	for (ind in inds) {
		ind_t1 = ind.phenotypeForTrait("simT");   // dynamic property not defined for the default trait
		ind_fitness = ind.cachedFitness;
		
		if (!isNAN(ind_t1))
			stop("t1 value mismatch (NAN expected): " + ind_t1);
		if (ind_fitness != 1.25)
			stop("fitness mismatch (1.25 expected): " + ind_fitness);
	}
	if (sim._debugBuild) {
		if (sim._allocatedNonneutralCacheCount != 0)
			stop("nonneutral caches were allocated despite all neutral genetics");
		if (sim._traitCalculationRegimeName != "kPureNeutral")
			stop("unexpected trait calculation regime");
	}
}
100 late() { }
		)V0G0N";
	
	SLiMAssertScriptSuccess(multitrait_DEFAULT_NEUTRAL_OBFUSCATED);
	
	
	// a quick test model for what happens if a non-neutral muttype is defined but not used in a neutral model
	// the goal was to provoke a conflict between muttype and trait ideas of whether the model is neutral
	std::string multitrait_DEFAULT_NEUTRAL_NNMUTTYPE =
		R"V0G0N(
// multitrait_DEFAULT_NEUTRAL_NNMUTTYPE
initialize() {
	initializeMutationRate(1e-7);
	initializeMutationType("m1", 0.5, "f", 0.0);
	initializeMutationType("m2", 0.5, "f", 0.1);   // nonneutral
	initializeGenomicElementType("g1", m1, 1.0);
	initializeGenomicElementType("g2", m2, 1.0);   // nonneutral but unused
	initializeGenomicElement(g1, 0, 99999);
	initializeRecombinationRate(1e-8);
}
1 early() { sim.addSubpop("p1", 50); }
100 late() { }
		)V0G0N";
	
	SLiMAssertScriptSuccess(multitrait_DEFAULT_NEUTRAL_NNMUTTYPE);
	
	
	// a completely neutral model with constant baseline and individual offsets, with direct fitness effects
	// - fitness values should be 1.0 for all individuals
	// - traits should be "super-pure-neutral" and thus skipped and uncalculated (and thus NAN)
	// - this model should not allocate any non-neutral caches, and should be in the kPureNeutral regime
	std::string multitrait_COMPLETE_NEUTRAL =
		R"V0G0N(
// multitrait_COMPLETE_NEUTRAL
initialize() {
	defineConstant("t1", initializeTrait("t1", "a", 1.0, directFitnessEffect=T));
	defineConstant("t2", initializeTrait("t2", "m", directFitnessEffect=T));
	initializeMutationType("m1", 0.5, "f", 0.0);
	initializeGenomicElementType("g1", m1, 1.0);
	initializeGenomicElement(g1, 0, 999999);
	initializeMutationRate(1e-7);
	initializeRecombinationRate(1e-8);
}
1 early() { sim.addSubpop("p1", 50); }
2: first() {
	inds = p1.individuals;
	for (ind in inds) {
		ind_t1 = ind.t1;
		ind_t2 = ind.t2;
		ind_fitness = ind.cachedFitness;
		
		if (!isNAN(ind_t1))
			stop("t1 value mismatch (NAN expected): " + ind_t1);
		if (!isNAN(ind_t2))
			stop("t2 value mismatch (NAN expected): " + ind_t2);
		if (ind_fitness != 1.0)
			stop("fitness mismatch (1.0 expected): " + ind_fitness);
	}
	if (sim._debugBuild) {
		if (sim._allocatedNonneutralCacheCount != 0)
			stop("nonneutral caches were allocated despite all neutral genetics");
		if (sim._traitCalculationRegimeName != "kPureNeutral")
			stop("unexpected trait calculation regime");
	}
}
100 late() { }
		)V0G0N";
	
	SLiMAssertScriptSuccess(multitrait_COMPLETE_NEUTRAL);
	
	
	// two traits, both genetically neutral but with non-zero fitness effects
	// the additive trait is NOT configured for independent dominance
	// - fitness values should be predictable from baseline offsets
	// - traits should be "super-pure-neutral" and thus skipped and uncalculated (and thus NAN)
	// - this model should not allocate any non-neutral caches, and should be in the kPureNeutral regime
	std::string multitrait_NEUTRAL_GENETICS =
		R"V0G0N(
// multitrait_NEUTRAL_GENETICS
initialize() {
	defineConstant("t1", initializeTrait("t1", "a", 1.01, directFitnessEffect=T));
	defineConstant("t2", initializeTrait("t2", "m", 1.01, directFitnessEffect=T));
	initializeMutationType("m1", 0.5, "f", 0.0);
	initializeGenomicElementType("g1", m1, 1.0);
	initializeGenomicElement(g1, 0, 999999);
	initializeMutationRate(1e-7);
	initializeRecombinationRate(1e-8);
}
1 early() { sim.addSubpop("p1", 50); }
late() { p1.fitnessScaling = runif(1); defineGlobal("lastFitnessScaling", p1.fitnessScaling); }
2: first() {
	inds = p1.individuals;
	for (ind in inds) {
		ind_fitness = ind.cachedFitness;
		expected_fitness = t1.baselineOffset * t2.baselineOffset * lastFitnessScaling;
		
		if (!isNAN(ind.t1))
			stop("t1 value mismatch (NAN expected): " + ind.t1);
		if (!isNAN(ind.t2))
			stop("t2 value mismatch (NAN expected): " + ind.t2);
		if (!isClose(ind_fitness, expected_fitness))
			stop("fitness mismatch: " + ind_fitness + ", " + expected_fitness);
	}
	if (sim._debugBuild) {
		if (sim._allocatedNonneutralCacheCount != 0)
			stop("nonneutral caches were allocated despite all neutral genetics");
		if (sim._traitCalculationRegimeName != "kPureNeutral")
			stop("unexpected trait calculation regime");
	}
}
100 late() { }
		)V0G0N";
	
	SLiMAssertScriptSuccess(multitrait_NEUTRAL_GENETICS);
	
	
	// two traits, both genetically neutral but with non-zero fitness effects and non-uniform offsets
	// the additive trait is NOT configured for independent dominance
	// - fitness values should be predictable from baseline/individual offsets
	// - this model should not allocate any non-neutral caches, and should be in the kPureNeutral regime
	std::string multitrait_NEUTRAL_GENETICS_WITH_OFFSETS =
		R"V0G0N(
// multitrait_NEUTRAL_GENETICS_WITH_OFFSETS
initialize() {
	defineConstant("t1", initializeTrait("t1", "a", 1.01, 0.0, 0.01, directFitnessEffect=T));
	defineConstant("t2", initializeTrait("t2", "m", 1.01, 0.0, 0.01, directFitnessEffect=T));
	initializeMutationType("m1", 0.5, "f", 0.0);
	initializeGenomicElementType("g1", m1, 1.0);
	initializeGenomicElement(g1, 0, 999999);
	initializeMutationRate(1e-7);
	initializeRecombinationRate(1e-8);
}
1 early() { sim.addSubpop("p1", 50); }
late() { p1.fitnessScaling = runif(1); defineGlobal("lastFitnessScaling", p1.fitnessScaling); }
2: first() {
	inds = p1.individuals;
	for (ind in inds) {
		ind_t1 = ind.t1;
		ind_t2 = ind.t2;
		ind_fitness = ind.cachedFitness;
		expected_t1 = t1.baselineOffset + ind.t1Offset;
		expected_t2 = t2.baselineOffset * ind.t2Offset;
		expected_fitness = ind_t1 * ind_t2 * lastFitnessScaling;
		
		if (!isClose(ind_t1, expected_t1))
			stop("t1 value mismatch: " + ind_t1 + ", " + expected_t1);
		if (!isClose(ind_t2, expected_t2))
			stop("t2 value mismatch: " + ind_t2 + ", " + expected_t2);
		if (!isClose(ind_fitness, expected_fitness))
			stop("fitness mismatch: " + ind_fitness + ", " + expected_fitness);
	}
	if (sim._debugBuild) {
		if (sim._allocatedNonneutralCacheCount != 0)
			stop("nonneutral caches were allocated despite all neutral genetics");
		if (sim._traitCalculationRegimeName != "kPureNeutral")
			stop("unexpected trait calculation regime");
	}
}
100 late() { }
		)V0G0N";
	
	SLiMAssertScriptSuccess(multitrait_NEUTRAL_GENETICS_WITH_OFFSETS);
	
	
	// default trait, with a nonneutral DES so all mutations are nonneutral; independent dominance is not used
	// - fitness values are unpredictable and not tested
	// - this model should not allocate any non-neutral caches, and should be in the kAllNonNeutralNoIndDomCaches regime
	std::string multitrait_ALL_NONNEUTRAL_NO_INDDOM =
		R"V0G0N(
// multitrait_ALL_NONNEUTRAL_NO_INDDOM
initialize() {
	initializeMutationRate(1e-7);
	initializeMutationType("m1", 0.5, "e", 0.01);
	initializeGenomicElementType("g1", m1, 1.0);
	initializeGenomicElement(g1, 0, 999999);
	initializeRecombinationRate(1e-8);
}
1 early() {
	sim.addSubpop("p1", 50);
}
2: first() {
	inds = p1.individuals;
	for (ind in inds) {
		ind_t1 = ind.phenotypeForTrait("simT");   // dynamic property not defined for the default trait
		ind_fitness = ind.cachedFitness;
		
		if (isNAN(ind_t1))
			stop("t1 value mismatch (NAN not expected): " + ind_t1);
	}
	if (sim._debugBuild) {
		if (sim._allocatedNonneutralCacheCount != 0)
			stop("nonneutral caches were allocated despite all nonneutral genetics");
		if (sim._traitCalculationRegimeName != "kAllNonNeutralNoIndDomCaches")
			stop("unexpected trait calculation regime");
	}
}
100 late() { }
		)V0G0N";
	
	SLiMAssertScriptSuccess(multitrait_ALL_NONNEUTRAL_NO_INDDOM);
	
	
	// default trait, with a nonneutral DES so all mutations are nonneutral; independent dominance is used
	// - fitness values are unpredictable and not tested
	// - this model should not allocate any non-neutral caches, and should be in the kAllNonNeutralWithIndDomCaches regime
	std::string multitrait_ALL_NONNEUTRAL_INDDOM =
		R"V0G0N(
// multitrait_ALL_NONNEUTRAL_INDDOM
initialize() {
	initializeMutationRate(1e-7);
	initializeMutationType("m1", NAN, "e", 0.01);
	initializeGenomicElementType("g1", m1, 1.0);
	initializeGenomicElement(g1, 0, 999999);
	initializeRecombinationRate(1e-8);
}
1 early() {
	sim.addSubpop("p1", 50);
}
2: first() {
	inds = p1.individuals;
	for (ind in inds) {
		ind_t1 = ind.phenotypeForTrait("simT");   // dynamic property not defined for the default trait
		ind_fitness = ind.cachedFitness;
		
		if (isNAN(ind_t1))
			stop("t1 value mismatch (NAN not expected): " + ind_t1);
	}
	if (sim._debugBuild) {
		if (sim._allocatedNonneutralCacheCount == 0)
			stop("nonneutral caches were unallocated despite independent dominance");
		if (sim._allocatedNonneutralMutationBufferCount > 0)
			stop("nonneutral mutation buffers were in use despite all nonneutral genetics");
		if (sim._traitCalculationRegimeName != "kAllNonNeutralWithIndDomCaches")
			stop("unexpected trait calculation regime");
	}
}
100 late() { }
		)V0G0N";
	
	SLiMAssertScriptSuccess(multitrait_ALL_NONNEUTRAL_INDDOM);
	
	
	// this is a SLiM 5.1-style quantitative trait model using sumOfMutationsOfType()
	// - fitness values are unpredictable and not tested
	// - the default trait should be inferred to be pure neutral, since the callback makes it neutral
	// - the default trait's phenotypes should never be calculated, since they are never demanded
	// - this model should not allocate any non-neutral caches, and should be in the kPureNeutral regime
	std::string multitrait_OLD_STYLE_QUANTITATIVE =
		R"V0G0N(
// multitrait_OLD_STYLE_QUANTITATIVE
initialize() {
	initializeMutationRate(1e-7);
	initializeMutationType("m1", 0.5, "f", 0.0);       // neutral
	initializeMutationType("m2", 0.5, "n", 0.0, 0.5);  // QTLs
	m2.convertToSubstitution = F;
	initializeGenomicElementType("g1", m1, 1);
	initializeGenomicElementType("g2", m2, 1);
	initializeGenomicElement(g1, 0, 20000);
	initializeGenomicElement(g2, 20001, 30000);
	initializeGenomicElement(g1, 30001, 99999);
	initializeRecombinationRate(1e-8);
}
mutationEffect(m2) { return 1.0; }
1 early() { sim.addSubpop("p1", 100); }
2: first() {
	inds = p1.individuals;
	for (ind in inds) {
		ind_t1 = ind.phenotypeForTrait("simT");   // dynamic property not defined for the default trait
		
		if (!isNAN(ind_t1))
			stop("t1 value mismatch (NAN expected): " + ind_t1);
	}
	if (sim._debugBuild) {
		if (sim._allocatedNonneutralCacheCount != 0)
			stop("nonneutral caches were allocated despite no demand");
		if (sim._traitCalculationRegimeName != "kPureNeutral")
			stop("unexpected trait calculation regime");
	}
}
1:2000 late() {
	inds = sim.subpopulations.individuals;
	phenotypes = inds.sumOfMutationsOfType(m2);
	inds.fitnessScaling = 1.5 - (phenotypes - 10.0)^2 * 0.005;
}
		)V0G0N";
	
	SLiMAssertScriptSuccess(multitrait_OLD_STYLE_QUANTITATIVE);
	
	
	// This is a particularly complex multitrait model intended to test many different things at once,
	// including pleiotropy, independent dominance, direct and indirect effects, and so forth.
	// This is an abbreviated version of test script complex_multi_test_1.slim
	std::string multitrait_COMPLEX_1 =
		R"V0G0N(
// multitrait_COMPLEX_1
initialize() {
	defineConstant("I1", 5.0);
	defineConstant("I2", -5.0);
	defineConstant("OPT1", 10.0);
	defineConstant("OPT2", 10.0);
	defineConstant("SD1", 2.0);
	defineConstant("SD2", 2.0);
	
	initializeSex();
	
	// multiplicative traits
	popgen1T = initializeTrait("popgen1T", "m", 1.0, 0.0, 0.01, directFitnessEffect=T);       // will have a mix of dominance
	popgen2T = initializeTrait("popgen2T", "m", 1.0, 0.0, 0.01, directFitnessEffect=T);       // will be independent dominance
	n1T = initializeTrait("n1T", "m", directFitnessEffect=T);                                 // neutral with direct effect
	n2T = initializeTrait("n2T", "m", directFitnessEffect=F);                                 // neutral with no direct effect
	
	// additive traits
	quant1T = initializeTrait("quant1T", "a", I1, 0.0, 0.01, directFitnessEffect=F);          // will have a mix of dominance
	quant2T = initializeTrait("quant2T", "a", I2, 0.0, 0.01, directFitnessEffect=F);          // will be independent dominance
	n3T = initializeTrait("n3T", "a", directFitnessEffect=F, baselineAccumulation=F);         // non-neutral with no direct effect
	
	// logistic trait
	logistic1T = initializeTrait("logistic1T", "l", 0.0, 0.01, 0.01, directFitnessEffect=T);  // will have a mix of dominance
	
	// quant1T / quant2T will be demanded in script; popgen1T / popgen2T / n1T / logistic1T are direct-effect and generate demand
	// calculation of popgen2T and quant2T should be extremely efficient since they are independent dominance
	// calculation of n1T should be omitted entirely; SLiM should detect that it is neutral, and not even set phenotype values
	// n2T and n3T should not be demanded, and should thus never be calculated by SLiM, which we can check in script
	
	// mutation types
	initializeMutationType("m1", 0.4, "f", 0.0);                                  // neutral for all traits
	
	initializeMutationType("m2", 0.4, "e", 0.05);                                 // beneficial for the popgen traits
	m2.setEffectSizeDistributionForTrait(c(n1T, n2T), "f", 0.0);                  // neutral DES for the neutral traits
	m2.setEffectSizeDistributionForTrait(c(quant1T, quant2T), "n", 0.0, 0.1);     // unbiased normal DES for the additive traits
	m2.setEffectSizeDistributionForTrait(c(logistic1T), "n", -0.05, 0.1);         // biased, wide normal DES for the logistic trait
	
	initializeMutationType("m3", 0.4, "g", -0.05, 1.0);                           // deleterious for the popgen traits
	m3.setEffectSizeDistributionForTrait(c(n1T, n2T), "f", 0.0);                  // neutral DES for the neutral traits
	m3.setEffectSizeDistributionForTrait(c(quant1T, quant2T), "n", 0.0, 0.1);     // unbiased normal DES for the additive traits
	m3.setEffectSizeDistributionForTrait(c(logistic1T), "n", -0.05, 0.1);         // biased, wide normal DES for the logistic trait
	
	c(m2,m3).setEffectSizeDistributionForTrait(n3T, "n", -5.0, 0.5);              // very biased and wide DES for n3T
	
	// set up independent dominance for popgen2T and quant2T; note that setting this for m1 is unnecessary (it is neutral)
	c(m2,m3).setDefaultDominanceForTrait(c(popgen2T, quant2T), NAN);
	
	// log information about m2 and m3 mutations, for comparison of initial versus final distributions of trait metrics
	c(m2,m3).logMutationData(T, trait=NULL, effectSize=T, dominance=T);
	
	initializeGenomicElementType("g1", m1, 1.0);          // neutral
	initializeGenomicElementType("g2", 1:3, c(3, 1, 2));  // mixture
	
	ids = 1:5;
	symbols = c(1, 2, "X", "Y", "MT");
	lengths = rdunif(5, 1e7, 2e7);
	types = c("A", "A", "X", "Y", "H");
	names = c("A1", "A2", "X", "Y", "MT");
	
	for (id in ids, symbol in symbols, length in lengths, type in types, name in names)
	{
		initializeChromosome(id, length, type, symbol, name);
		initializeMutationRate(1e-7);
		initializeRecombinationRate(1e-8);
		
		if (id == 1)
			initializeGenomicElement(g1);  // autosome 1 is pure-neutral, using only m1
		else
			initializeGenomicElement(g2);  // autosome 2 is a mix, using m1 / m2 / m3
	}
}

mutation(m2) {
	// set random dominance effects for the popgen1T and quant1T and logistic1TDominance traits
	// other effects are generated as specified by the mutation type DES
	mut.popgen1TDominance = runif(1);
	mut.quant1TDominance = runif(1);
	mut.logistic1TDominance = runif(1);
	return T;
}
mutation(m3) {
	// set random dominance effects for the popgen1T and quant1T and logistic1TDominance traits
	// other effects are generated as specified by the mutation type DES
	mut.popgen1TDominance = runif(1);
	mut.quant1TDominance = runif(1);
	mut.logistic1TDominance = runif(1);
	return T;
}

1 late() {
	sim.addSubpop("p1", 20);
}

// tick 7: m2 mutations are completely neutral, m3 are normal
// tick 8: m2 is completely neutral, m3 is neutral for popgen1T and popgen2T, and QTL demand and selection are off
// tick 9: m3 mutations are neutral for popgen1T only

7:8 mutationEffect(m2)
{
	return NULL;	// make neutral
}
8:9 mutationEffect(m3, NULL, "popgen1T")
{
	return NULL;	// make neutral
}
8 mutationEffect(m3, NULL, "popgen2T")
{
	return NULL;	// make neutral
}

1: late() {
	// make tick 8 neutral
	if (sim.cycle == 8)
		return;
	
	// stabilizing selection on quant1T and quant2T, before fitness calculation takes place
	inds = sim.subpopulations.individuals;
	
	if (community.tick % 2 == 0)
		sim.demandPhenotype(NULL, c(sim.quant1T, sim.quant2T));	// the fast way
	else
		sim.subpopulations.individuals.demandPhenotypeForIndividuals(c(sim.quant1T, sim.quant2T));	// the slow way
	
	phenotypes_q1 = inds.quant1T;
	phenotypes_q2 = inds.quant2T;
	
	fitnessEffect_q1 = dnorm(phenotypes_q1, OPT1, SD1) / dnorm(0.0, 0.0, SD1);
	fitnessEffect_q2 = dnorm(phenotypes_q2, OPT2, SD2) / dnorm(0.0, 0.0, SD2);
	
	inds.fitnessScaling = fitnessEffect_q1 * fitnessEffect_q2;
}

2: first() {
	inds = sim.subpopulations.individuals;
	
	// check that traits were calculated correctly, or left uncalculated as appropriate
	if (!all(isNAN(inds.n1T))) stop("n1T was calculated unnecessarily (super-pure-neutral trait)");
	if (!all(isNAN(inds.n2T))) stop("n2T was calculated unnecessarily (no direct fitness effect)");
	if (!all(isNAN(inds.n3T))) stop("n3T was calculated unnecessarily (no direct fitness effect)");
	if (!all(!isNAN(inds.logistic1T))) stop("logistic1T is NAN");
	if (!all((inds.logistic1T >= 0.0) & (inds.logistic1T <= 1.0))) stop("logistic1T is out of range");
	
	// check baseline accumulation, which in on for all traits except n3T
	// each substitution shifts the baseline by 1+s (multiplicatively) or 2a (additively)
	p1t_subs = product(1 + sim.substitutions.popgen1TEffectSize);
	p2t_subs = product(1 + sim.substitutions.popgen2TEffectSize);
	n1t_subs = product(1 + sim.substitutions.n1TEffectSize);
	n2t_subs = product(1 + sim.substitutions.n2TEffectSize);
	q1t_subs = sum(2 * sim.substitutions.quant1TEffectSize);
	q2t_subs = sum(2 * sim.substitutions.quant2TEffectSize);
	//n3t_subs = sum(2 * sim.substitutions.n3TEffectSize);
	l1t_subs = sum(2 * sim.substitutions.logistic1TEffectSize);
	
	if (!isClose(p1t_subs * 1.0, sim.popgen1T.baselineOffset)) stop("popgen1T baseline is wrong");
	if (!isClose(p2t_subs * 1.0, sim.popgen2T.baselineOffset)) stop("popgen2T baseline is wrong");
	if (!isClose(n1t_subs * 1.0, sim.n1T.baselineOffset)) stop("n1T baseline is wrong");
	if (!isClose(n2t_subs * 1.0, sim.n2T.baselineOffset)) stop("n2T baseline is wrong");
	if (!isClose(q1t_subs + I1, sim.quant1T.baselineOffset)) stop("quant1T baseline is wrong");
	if (!isClose(q2t_subs + I2, sim.quant2T.baselineOffset)) stop("quant2T baseline is wrong");
	if (!(sim.n3T.baselineOffset == 0.0)) stop("n3T baseline is wrong");
	if (!isClose(l1t_subs + 0.0, sim.logistic1T.baselineOffset)) stop("logistic1T baseline is wrong");
}

100 late() { }
		)V0G0N";
	
	SLiMAssertScriptSuccess(multitrait_COMPLEX_1);
	
	std::cout << "_RunMultitraitTests() done" << std::endl;
}






























