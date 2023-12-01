//
//  slim_test_core.cpp
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


#pragma mark initialize() tests
void _RunInitTests(void)
{	
	// ************************************************************************************
	//
	//	Initialization function tests
	//
	
	// Test (void)initializeGeneConversion(numeric$ conversionFraction, numeric$ meanLength)
	SLiMAssertScriptStop("initialize() { initializeGeneConversion(0.5, 10000000000000, 0.0); stop(); }", __LINE__);										// legal; no max for meanLength
	SLiMAssertScriptRaise("initialize() { initializeGeneConversion(-0.001, 10000000000000, 0.0); stop(); }", "nonCrossoverFraction must be between 0.0 and 1.0", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeGeneConversion(1.001, 10000000000000, 0.0); stop(); }", "nonCrossoverFraction must be between 0.0 and 1.0", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeGeneConversion(0.5, -0.01, 0.0); stop(); }", "meanLength must be >= 0.0", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeGeneConversion(0.5, 1000, -0.001); stop(); }", "simpleConversionFraction must be between 0.0 and 1.0", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeGeneConversion(0.5, 1000, 1.001); stop(); }", "simpleConversionFraction must be between 0.0 and 1.0", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeGeneConversion(0.5, 1000, 0.0, -1.001); stop(); }", "bias must be between -1.0 and 1.0", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeGeneConversion(0.5, 1000, 0.0, 1.001); stop(); }", "bias must be between -1.0 and 1.0", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeGeneConversion(0.5, 1000, 0.0, 0.1); stop(); }", "must be 0.0 in non-nucleotide-based models", __LINE__);
	
	// Test (object<MutationType>$)initializeMutationType(is$ id, numeric$ dominanceCoeff, string$ distributionType, ...)
	SLiMAssertScriptStop("initialize() { initializeMutationType('m1', 0.5, 'f', 0.0); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeMutationType(1, 0.5, 'f', 0.0); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType(-1, 0.5, 'f', 0.0); stop(); }", "identifier value is out of range", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('p2', 0.5, 'f', 0.0); stop(); }", "identifier prefix 'm' was expected", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('mm1', 0.5, 'f', 0.0); stop(); }", "must be a simple integer", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'f'); stop(); }", "requires exactly 1 DFE parameter", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'f', 0.0, 0.0); stop(); }", "requires exactly 1 DFE parameter", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'g', 0.0); stop(); }", "requires exactly 2 DFE parameters", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'e', 0.0, 0.0); stop(); }", "requires exactly 1 DFE parameter", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'n', 0.0); stop(); }", "requires exactly 2 DFE parameters", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'p', 0.0); stop(); }", "requires exactly 2 DFE parameters", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'w', 0.0); stop(); }", "requires exactly 2 DFE parameters", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'f', 'foo'); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'g', 'foo', 0.0); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'g', 0.0, 'foo'); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'e', 'foo'); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'n', 'foo', 0.0); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'n', 0.0, 'foo'); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'p', 'foo', 0.0); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'p', 0.0, 'foo'); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'w', 'foo', 0.0); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'w', 0.0, 'foo'); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'f', '1'); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'g', '1', 0.0); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'g', 0.0, '1'); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'e', '1'); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'n', '1', 0.0); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'n', 0.0, '1'); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'p', '1', 0.0); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'p', 0.0, '1'); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'w', '1', 0.0); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'w', 0.0, '1'); stop(); }", "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'x', 0.0); stop(); }", "must be 'f', 'g', 'e', 'n', 'w', or 's'", __LINE__);
	SLiMAssertScriptStop("initialize() { x = initializeMutationType('m7', 0.5, 'f', 0.0); if (x == m7) stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { x = initializeMutationType(7, 0.5, 'f', 0.0); if (x == m7) stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { m7 = 15; initializeMutationType(7, 0.5, 'f', 0.0); stop(); }", "already defined", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'f', 0.0); initializeMutationType('m1', 0.5, 'f', 0.0); stop(); }", "already defined", __LINE__);
	
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'g', 3.1, 0.0); stop(); }", "must have a shape parameter > 0", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'g', 3.1, -1.0); stop(); }", "must have a shape parameter > 0", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'n', 3.1, -1.0); stop(); }", "must have a standard deviation parameter >= 0", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'p', 3.1, 0.0); stop(); }", "must have a scale parameter > 0", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'p', 3.1, -1.0); stop(); }", "must have a scale parameter > 0", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'w', 0.0, 7.5); stop(); }", "must have a scale parameter > 0", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'w', -1.0, 7.5); stop(); }", "must have a scale parameter > 0", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'w', 3.1, 0.0); stop(); }", "must have a shape parameter > 0", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'w', 3.1, -7.5); stop(); }", "must have a shape parameter > 0", __LINE__);
	
	// Test (object<GenomicElementType>$)initializeGenomicElementType(is$ id, io<MutationType> mutationTypes, numeric proportions)
	std::string define_m12(" initializeMutationType('m1', 0.5, 'f', 0.0); initializeMutationType('m2', 0.5, 'f', 0.5); ");
	
	SLiMAssertScriptStop("initialize() {" + define_m12 + "initializeGenomicElementType('g1', object(), integer(0)); stop(); }", __LINE__);			// legal: genomic element with no mutations
	SLiMAssertScriptStop("initialize() {" + define_m12 + "initializeGenomicElementType('g1', integer(0), float(0)); stop(); }", __LINE__);			// legal: genomic element with no mutations
	SLiMAssertScriptStop("initialize() {" + define_m12 + "initializeGenomicElementType('g1', c(m1,m2), c(0,0)); stop(); }", __LINE__);				// legal: genomic element with all zero proportions (must be fixed later...)
	SLiMAssertScriptStop("initialize() {" + define_m12 + "initializeGenomicElementType('g1', c(m1,m2), 1:2); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() {" + define_m12 + "initializeGenomicElementType(1, c(m1,m2), 1:2); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() {" + define_m12 + "initializeGenomicElementType('g1', 1:2, 1:2); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_m12 + "initializeGenomicElementType('g1', c(m1,m2)); stop(); }", "missing required argument", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_m12 + "initializeGenomicElementType('g1', c(m1,m2), 1); stop(); }", "requires the sizes", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_m12 + "initializeGenomicElementType('g1', c(m1,m2), c(-1,2)); stop(); }", "must be greater than or equal to zero", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_m12 + "initializeGenomicElementType('g1', 2:3, 1:2); stop(); }", "not defined", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_m12 + "initializeGenomicElementType('g1', c(2,2), 1:2); stop(); }", "used more than once", __LINE__);
	SLiMAssertScriptStop("initialize() {" + define_m12 + "x = initializeGenomicElementType('g7', c(m1,m2), 1:2); if (x == g7) stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() {" + define_m12 + "x = initializeGenomicElementType(7, c(m1,m2), 1:2); if (x == g7) stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_m12 + "g7 = 17; initializeGenomicElementType(7, c(m1,m2), 1:2); stop(); }", "already defined", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_m12 + "initializeGenomicElementType('g1', c(m1,m2), 1:2); initializeGenomicElementType('g1', c(m1,m2), c(0,0)); stop(); }", "already defined", __LINE__);
	
	// Test (void)initializeGenomicElement(io<GenomicElementType>$ genomicElementType, integer$ start, integer$ end)
	std::string define_g1(define_m12 + " initializeGenomicElementType('g1', c(m1,m2), 1:2); ");
	
	SLiMAssertScriptStop("initialize() {" + define_g1 + "initializeGenomicElement(g1, 0, 1000000000); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() {" + define_g1 + "initializeGenomicElement(1, 0, 1000000000); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_g1 + "initializeGenomicElement(g1, 0); stop(); }", "missing required argument", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_g1 + "initializeGenomicElement(2, 0, 1000000000); stop(); }", "not defined", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_g1 + "initializeGenomicElement(g1, -1, 1000000000); stop(); }", "out of range", __LINE__);
	//SLiMAssertScriptRaise("initialize() {" + define_g1 + "initializeGenomicElement(g1, 0, 1000000001); stop(); }", "out of range", __LINE__);		// now legal!
	SLiMAssertScriptStop("initialize() {" + define_g1 + "initializeGenomicElement(g1, 0, 1000000000000000); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_g1 + "initializeGenomicElement(g1, 0, 1000000000000001); stop(); }", "out of range", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_g1 + "initializeGenomicElement(g1, 100, 99); stop(); }", "is less than start position", __LINE__);
	
	// Test (void)initializeMutationRate(numeric$ rate)
	SLiMAssertScriptStop("initialize() { initializeMutationRate(0.0); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationRate(); stop(); }", "missing required argument", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationRate(-0.0000001); stop(); }", "requires rates to be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationRate(10000000); stop(); }", "requires rates to be", __LINE__);	// no longer legal, in SLiM 3.5
	
	// Test (void)initializeRecombinationRate(numeric rates, [integer ends])
	SLiMAssertScriptStop("initialize() { initializeRecombinationRate(0.0); stop(); }", __LINE__);														// legal: singleton rate, no end
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(); stop(); }", "missing required argument", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(-0.00001); stop(); }", "requires rates to be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeRecombinationRate(0.5); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(0.6); stop(); }", "requires rates to be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(10000); stop(); }", "requires rates to be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeRecombinationRate(c(0.0, 0.1), c(1000, 2000)); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(c(0.0, 0.1)); stop(); }", "requires rates to be a singleton if", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(integer(0), integer(0)); stop(); }", "ends and rates to be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(c(0.0, 0.1), 1000); stop(); }", "ends and rates to be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(c(0.0, 0.1), 1:3); stop(); }", "ends and rates to be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(c(0.0, 0.1), c(2000, 1000)); stop(); }", "ascending order", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(c(0.0, 0.1), c(1000, 1000)); stop(); }", "ascending order", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(c(0.0, -0.001), c(1000, 2000)); stop(); }", "requires rates to be in [0.0, 0.5]", __LINE__);
	
	SLiMAssertScriptStop("initialize() { initializeSex('A'); initializeRecombinationRate(0.0); stop(); }", __LINE__);														// legal: singleton rate, no end
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(); stop(); }", "missing required argument", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(-0.00001); stop(); }", "requires rates to be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSex('A'); initializeRecombinationRate(0.5); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(0.6); stop(); }", "requires rates to be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(10000); stop(); }", "requires rates to be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(1000, 2000)); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1)); stop(); }", "requires rates to be a singleton if", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(integer(0), integer(0)); stop(); }", "ends and rates to be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), 1000); stop(); }", "ends and rates to be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), 1:3); stop(); }", "ends and rates to be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(2000, 1000)); stop(); }", "ascending order", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(1000, 1000)); stop(); }", "ascending order", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, -0.001), c(1000, 2000)); stop(); }", "requires rates to be in [0.0, 0.5]", __LINE__);
	
	SLiMAssertScriptStop("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(1000, 2000), '*'); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(integer(0), integer(0), '*'); stop(); }", "ends and rates to be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), 1000, '*'); stop(); }", "ends and rates to be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), 1:3, '*'); stop(); }", "ends and rates to be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(2000, 1000), '*'); stop(); }", "ascending order", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(1000, 1000), '*'); stop(); }", "ascending order", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, -0.001), c(1000, 2000), '*'); stop(); }", "requires rates to be in [0.0, 0.5]", __LINE__);
	
	SLiMAssertScriptStop("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(1000, 2000), 'M'); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(integer(0), integer(0), 'M'); stop(); }", "ends and rates to be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), 1000, 'M'); stop(); }", "ends and rates to be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), 1:3, 'M'); stop(); }", "ends and rates to be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(2000, 1000), 'M'); stop(); }", "ascending order", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(1000, 1000), 'M'); stop(); }", "ascending order", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, -0.001), c(1000, 2000), 'M'); stop(); }", "requires rates to be in [0.0, 0.5]", __LINE__);
	
	SLiMAssertScriptStop("initialize() {" + define_g1 + "initializeMutationRate(0.0); initializeGenomicElement(g1, 0, 2000); initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(1000, 2000), 'M'); initializeRecombinationRate(0.0, 2000, 'F'); stop(); } 1 early() {}", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_g1 + "initializeMutationRate(0.0); initializeGenomicElement(g1, 0, 3000); initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(1000, 2000), 'M'); initializeRecombinationRate(0.0, 2000, 'F'); } 1 early() {}", "do not cover the full chromosome", __LINE__, false);
	SLiMAssertScriptStop("initialize() {" + define_g1 + "initializeMutationRate(0.0); initializeGenomicElement(g1, 0, 1000); initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(1000, 2000), 'M'); initializeRecombinationRate(0.0, 2000, 'F'); } 1 early() { stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_g1 + "initializeMutationRate(0.0); initializeGenomicElement(g1, 0, 2000); initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(1000, 2000), 'M'); initializeRecombinationRate(0.0, 1999, 'F'); } 1 early() {}", "do not cover the full chromosome", __LINE__, false);
	SLiMAssertScriptRaise("initialize() {" + define_g1 + "initializeMutationRate(0.0); initializeGenomicElement(g1, 0, 2000); initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(1000, 2000), 'M'); initializeRecombinationRate(0.0, 2001, 'F'); } 1 early() { stop(); }", "do not cover the full chromosome", __LINE__, false);
	SLiMAssertScriptRaise("initialize() {" + define_g1 + "initializeMutationRate(0.0); initializeGenomicElement(g1, 0, 2000); initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(1000, 2000), 'M'); initializeRecombinationRate(0.0, 2000, '*'); } 1 early() {}", "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_g1 + "initializeMutationRate(0.0); initializeGenomicElement(g1, 0, 2000); initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(1000, 2000), '*'); initializeRecombinationRate(0.0, 2000, 'F'); } 1 early() {}", "single map versus separate maps", __LINE__);
	
	// Test (void)initializeSex(string$ chromosomeType)
	SLiMAssertScriptStop("initialize() { initializeSex('A'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSex('X'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSex('Y'); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('Z'); stop(); }", "requires a chromosomeType of", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex(); stop(); }", "missing required argument", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeSex('A'); stop(); }", "may be called only once", __LINE__);
	
	// Test (void)initializeSLiMModelType(string$ modelType)
	SLiMAssertScriptRaise("initialize() { initializeSLiMModelType(); stop(); }", "missing required argument modelType", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMModelType('WF'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMModelType('nonWF'); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMModelType('foo'); stop(); }", "legal values", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(); initializeSLiMModelType('WF'); stop(); }", "must be called before", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationRate(0.0); initializeSLiMModelType('WF'); stop(); }", "must be called before", __LINE__);
	
	// Test (void)initializeSLiMOptions([logical$ keepPedigrees = F], [string$ dimensionality = ""], [string$ periodicity = ""], [integer$ mutationRuns = 0], [logical$ preventIncidentalSelfing = F])
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(F); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(T); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(F, ''); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(T, ''); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(F, 'xyz'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(T, 'xyz'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(keepPedigrees=T); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality=''); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='x'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='xy'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='xyz'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='', periodicity=''); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='x', periodicity=''); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='x', periodicity='x'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='xy', periodicity=''); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='xy', periodicity='x'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='xy', periodicity='y'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='xy', periodicity='xy'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='xyz', periodicity=''); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='xyz', periodicity='x'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='xyz', periodicity='y'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='xyz', periodicity='z'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='xyz', periodicity='xy'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='xyz', periodicity='xz'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='xyz', periodicity='yz'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='xyz', periodicity='xyz'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(mutationRuns=0); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(mutationRuns=1); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(mutationRuns=100); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(preventIncidentalSelfing=F); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(preventIncidentalSelfing=T); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(keepPedigrees=NULL); stop(); }", "cannot be type NULL", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality=NULL); stop(); }", "cannot be type NULL", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(mutationRuns=NULL); stop(); }", "cannot be type NULL", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(preventIncidentalSelfing=NULL); stop(); }", "cannot be type NULL", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='foo'); stop(); }", "legal non-empty values", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='y'); stop(); }", "legal non-empty values", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='z'); stop(); }", "legal non-empty values", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='xz'); stop(); }", "legal non-empty values", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='yz'); stop(); }", "legal non-empty values", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='zyx'); stop(); }", "legal non-empty values", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='', periodicity='x'); stop(); }", "may not be set in non-spatial simulations", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='x', periodicity='y'); stop(); }", "cannot utilize spatial dimensions beyond", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='x', periodicity='z'); stop(); }", "cannot utilize spatial dimensions beyond", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='xy', periodicity='z'); stop(); }", "cannot utilize spatial dimensions beyond", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='xyz', periodicity='foo'); stop(); }", "legal non-empty values", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='xyz', periodicity='xzy'); stop(); }", "legal non-empty values", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(); initializeSLiMOptions(); stop(); }", "may be called only once", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationRate(0.0); initializeSLiMOptions(); stop(); }", "must be called before", __LINE__);
	
	// Test (object<InteractionType>$)initializeInteractionType(is$ id, string$ spatiality, [logical$ reciprocal = F], [numeric$ maxDistance = INF], [string$ sexSegregation = "**"])
	SLiMAssertScriptRaise("initialize() { initializeInteractionType(-1, ''); stop(); }", "identifier value is out of range", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeInteractionType(0, ''); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeInteractionType('i0', ''); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeInteractionType(0, 'x'); stop(); }", "spatial dimensions beyond those set", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeInteractionType('i0', 'x'); stop(); }", "spatial dimensions beyond those set", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeInteractionType(0, 'w'); stop(); }", "spatiality 'w' must be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeInteractionType('i0', 'w'); stop(); }", "spatiality 'w' must be", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeInteractionType(0, '', T); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeInteractionType(0, '', T, 0.1); stop(); }", "must be INF for non-spatial interactions", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeInteractionType(0, '', T, INF, '**'); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeInteractionType(0, '', T, INF, '*M'); stop(); }", "unsupported in non-sexual simulation", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSex('A'); initializeInteractionType(0, '', T, INF, '**'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSex('A'); initializeInteractionType(0, '', T, INF, '*M'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSex('A'); initializeInteractionType(0, '', T, INF, '*F'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSex('A'); initializeInteractionType(0, '', T, INF, 'M*'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSex('A'); initializeInteractionType(0, '', T, INF, 'MM'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSex('A'); initializeInteractionType(0, '', T, INF, 'MF'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSex('A'); initializeInteractionType(0, '', T, INF, 'F*'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSex('A'); initializeInteractionType(0, '', T, INF, 'FM'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSex('A'); initializeInteractionType(0, '', T, INF, 'FF'); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeInteractionType(0, '', T, INF, 'W*'); stop(); }", "unsupported sexSegregation value", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeInteractionType(0, '', T, INF, '*W'); stop(); }", "unsupported sexSegregation value", __LINE__);

	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='x'); initializeInteractionType(0, 'w'); stop(); }", "spatiality 'w' must be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='x'); initializeInteractionType('i0', 'w'); stop(); }", "spatiality 'w' must be", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='x'); initializeInteractionType(0, '', T); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='x'); initializeInteractionType(0, '', T, 0.1); stop(); }", "must be INF for non-spatial interactions", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='x'); initializeInteractionType(0, '', T, INF, '**'); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='x'); initializeInteractionType(0, '', T, INF, '*M'); stop(); }", "unsupported in non-sexual simulation", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='x'); initializeSex('A'); initializeInteractionType(0, '', T, INF, '*M'); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='x'); initializeInteractionType(0, '', T, INF, 'W*'); stop(); }", "unsupported sexSegregation value", __LINE__);
	
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='x'); initializeInteractionType(0, 'x'); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='x'); initializeInteractionType(0, 'y'); stop(); }", "spatial dimensions beyond those set", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='x'); initializeInteractionType(0, 'x', F); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='x'); initializeInteractionType(0, 'x', T); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='x'); initializeInteractionType(0, 'x', F, 0.1); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='x'); initializeInteractionType(0, 'x', T, 0.1); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='x'); initializeInteractionType(0, 'x', T, 0.0); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='x'); initializeInteractionType(0, 'x', T, -0.1); stop(); }", "maxDistance must be >= 0.0", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='x'); initializeInteractionType(0, 'x', T, 0.1, '*M'); stop(); }", "unsupported in non-sexual simulation", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='x'); initializeSex('A'); initializeInteractionType(0, 'x', T, 0.1, '*M'); stop(); }", __LINE__);
	
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='xyz'); initializeInteractionType(0, 'x'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='xyz'); initializeInteractionType(0, 'y'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='xyz'); initializeInteractionType(0, 'z'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='xyz'); initializeInteractionType(0, 'xy'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='xyz'); initializeInteractionType(0, 'yz'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='xyz'); initializeInteractionType(0, 'xz'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='xyz'); initializeInteractionType(0, 'xyz'); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='xyz'); initializeInteractionType(0, 'w'); stop(); }", "spatiality 'w' must be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='xyz'); initializeInteractionType(0, 'yx'); stop(); }", "spatiality 'yx' must be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='xyz'); initializeInteractionType(0, 'zyx'); stop(); }", "spatiality 'zyx' must be", __LINE__);
}

#pragma mark Community tests
void _RunCommunityTests(void)
{
	// Note that _RunSpeciesTests() also does some Community tests, for historical reasons...
	SLiMAssertScriptStop(gen1_setup_p1 + "2 early() { community.outputUsage(); } " + gen2_stop, __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "2 early() { community.usage(); } " + gen2_stop, __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_p1 + "2 early() { if (identical(community.allGenomicElementTypes, g1)) stop(); } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "2 early() { if (community.allInteractionTypes.size() == 0) stop(); } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "2 early() { if (identical(community.allMutationTypes, m1)) stop(); } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "2 early() { if (community.allScriptBlocks.size() == 3) stop(); } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "2 early() { if (identical(community.allSpecies, sim)) stop(); } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "2 early() { if (identical(community.allSubpopulations, p1)) stop(); } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "2 early() { if (community.logFiles.size() == 0) stop(); } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "2 early() { if (community.tick == 2) stop(); } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "2 early() { community.tag = 10; if (community.tag == 10) stop(); } ", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_p1 + "2 early() { if (identical(community.genomicElementTypesWithIDs(1), g1)) stop(); } ", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "2 early() { community.genomicElementTypesWithIDs(2); } ", "did not find", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "2 early() { community.genomicElementTypesWithIDs(c(2,3)); } ", "did not find", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "2 early() { if (identical(community.genomicElementTypesWithIDs(c(1,1)), c(g1,g1))) stop(); } ", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1 + "2 early() { if (identical(community.interactionTypesWithIDs(1), i1)) stop(); } ", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1 + "2 early() { community.interactionTypesWithIDs(2); } ", "did not find", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1 + "2 early() { community.interactionTypesWithIDs(c(2,3)); } ", "did not find", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1 + "2 early() { if (identical(community.interactionTypesWithIDs(c(1,1)), c(i1,i1))) stop(); } ", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_p1 + "2 early() { if (identical(community.mutationTypesWithIDs(1), m1)) stop(); } ", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "2 early() { community.mutationTypesWithIDs(2); } ", "did not find", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "2 early() { community.mutationTypesWithIDs(c(2,3)); } ", "did not find", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "2 early() { if (identical(community.mutationTypesWithIDs(c(1,1)), c(m1,m1))) stop(); } ", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_p1 + "s1 2 early() { if (identical(community.scriptBlocksWithIDs(1), self)) stop(); } ", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "s1 2 early() { community.scriptBlocksWithIDs(2); } ", "did not find", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "s1 2 early() { community.scriptBlocksWithIDs(c(2,3)); } ", "did not find", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "s1 2 early() { if (identical(community.scriptBlocksWithIDs(c(1,1)), c(self,self))) stop(); } ", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_p1 + "2 early() { if (identical(community.speciesWithIDs(0), sim)) stop(); } ", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "2 early() { community.speciesWithIDs(1); } ", "did not find", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "2 early() { community.speciesWithIDs(c(1,2)); } ", "did not find", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "2 early() { if (identical(community.speciesWithIDs(c(0,0)), c(sim,sim))) stop(); } ", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_p1 + "2 early() { if (identical(community.subpopulationsWithIDs(1), p1)) stop(); } ", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "2 early() { community.subpopulationsWithIDs(2); } ", "did not find", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "2 early() { community.subpopulationsWithIDs(c(2,3)); } ", "did not find", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "2 early() { if (identical(community.subpopulationsWithIDs(c(1,1)), c(p1,p1))) stop(); } ", __LINE__);
}

#pragma mark Species tests
void _RunSpeciesTests(const std::string &temp_path)
{
	// ************************************************************************************
	//
	//	Gen 1+ tests: Species & Community
	//
	
	// Test sim properties
	SLiMAssertScriptStop(gen1_setup + "1 first() { } " + gen2_stop, __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { } " + gen2_stop, __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { sim.chromosome; } " + gen2_stop, __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { sim.chromosome = sim.chromosome; } " + gen2_stop, "read-only property", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { if (sim.chromosomeType == 'A') stop(); } ", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { sim.chromosomeType = 'A'; } " + gen2_stop, "read-only property", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 early() { if (sim.chromosomeType == 'X') stop(); } ", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 early() { sim.chromosomeType = 'X'; } " + gen2_stop, "read-only property", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 early() { sim.cycle; } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { sim.cycle = 7; } " + gen2_stop, __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 early() { community.tick = 7; } " + gen2_stop, __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { if (community.cycleStage == 'early') stop(); } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { if (community.cycleStage == 'early') stop(); } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 late() { if (community.cycleStage == 'late') stop(); } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "modifyChild(p1) { if (community.cycleStage == 'reproduction') stop(); } 2 early() {}", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "mutationEffect(m1) { if (community.cycleStage == 'fitness') stop(); } 100 early() {}", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { community.cycleStage = 'early'; } ", "read-only property", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { if (sim.genomicElementTypes == g1) stop(); } ", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { sim.genomicElementTypes = g1; } ", "read-only property", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { if (community.modelType == 'WF') stop(); } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 early() { if (community.modelType == 'WF') stop(); } ", __LINE__);
	SLiMAssertScriptStop(WF_prefix + gen1_setup + "1 early() { if (community.modelType == 'WF') stop(); } ", __LINE__);
	SLiMAssertScriptStop(WF_prefix + gen1_setup_sex + "1 early() { if (community.modelType == 'WF') stop(); } ", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { community.modelType = 'foo'; } ", "read-only property", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { if (sim.mutationTypes == m1) stop(); } ", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { sim.mutationTypes = m1; } ", "read-only property", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 early() { sim.mutations; } ", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { sim.mutations = _Test(7); } ", "cannot be object element type", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 early() { sim.scriptBlocks; } ", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { sim.scriptBlocks = sim.scriptBlocks[0]; } ", "read-only property", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 early() { community.allScriptBlocks; } ", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { community.allScriptBlocks = community.allScriptBlocks[0]; } ", "read-only property", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { if (sim.sexEnabled == F) stop(); } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 early() { if (sim.sexEnabled == T) stop(); } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { if (size(sim.subpopulations) == 0) stop(); } ", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { sim.subpopulations = _Test(7); } ", "cannot be object element type", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { if (size(sim.substitutions) == 0) stop(); } ", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { sim.substitutions = _Test(7); } ", "cannot be object element type", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { sim.tag; } ", "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { c(sim,sim).tag; } ", "before being set", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 early() { sim.tag = -17; } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { sim.tag = -17; } 2 early() { if (sim.tag == -17) stop(); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 early() { community.verbosity; } ", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 early() { community.verbosity = -17; } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { community.verbosity = -17; } 2 early() { if (community.verbosity == -17) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { if (sim.dimensionality == '') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1 + "1 early() { if (sim.dimensionality == '') stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1 + "1 early() { sim.dimensionality = 'x'; }", "read-only property", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 early() { if (sim.dimensionality == 'x') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { if (sim.periodicity == '') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1 + "1 early() { if (sim.periodicity == '') stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1 + "1 early() { sim.periodicity = 'x'; }", "read-only property", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 early() { if (sim.periodicity == '') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyzPxz + "1 early() { if (sim.periodicity == 'xz') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { if (sim.id == 0) stop(); } ", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { sim.id = 2; } " + gen2_stop, "read-only property", __LINE__);
	
	// Test sim - (object<Subpopulation>)addSubpop(is$ subpopID, integer$ size, [float$ sexRatio])
	SLiMAssertScriptStop(gen1_setup + "1 early() { sim.addSubpop('p1', 10); } " + gen2_stop, __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { sim.addSubpop(1, 10); } " + gen2_stop, __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { sim.addSubpop('p1', 10, 0.5); } " + gen2_stop, __LINE__);	// default value
	SLiMAssertScriptStop(gen1_setup + "1 early() { sim.addSubpop(1, 10, 0.5); } " + gen2_stop, __LINE__);	// default value
	SLiMAssertScriptRaise(gen1_setup + "1 early() { sim.addSubpop('p1', 10, 0.4); } " + gen2_stop, "non-sexual simulation", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { sim.addSubpop(1, 10, 0.4); } " + gen2_stop, "non-sexual simulation", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 early() { sim.addSubpop('p1', 10, 0.5); } " + gen2_stop, __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 early() { sim.addSubpop(1, 10, 0.5); } " + gen2_stop, __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { x = sim.addSubpop('p7', 10); if (x == p7) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 early() { x = sim.addSubpop(7, 10); if (x == p7) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { p7 = 17; sim.addSubpop('p7', 10); stop(); }", "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 early() { sim.addSubpop('p7', 10); sim.addSubpop(7, 10); stop(); }", "used already", __LINE__);
	
	// Test sim - (object<Subpopulation>)addSubpopSplit(is$ subpopID, integer$ size, io<Subpopulation>$ sourceSubpop, [float$ sexRatio])
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { sim.addSubpopSplit('p2', 10, p1); } " + gen2_stop, __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { sim.addSubpopSplit('p2', 10, 1); } " + gen2_stop, __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { sim.addSubpopSplit(2, 10, p1); } " + gen2_stop, __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { sim.addSubpopSplit(2, 10, 1); } " + gen2_stop, __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { sim.addSubpopSplit(2, 10, 7); } " + gen2_stop, "not defined", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { sim.addSubpopSplit('p2', 10, p1, 0.5); } " + gen2_stop, __LINE__);	// default value
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { sim.addSubpopSplit(2, 10, p1, 0.5); } " + gen2_stop, __LINE__);	// default value
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { sim.addSubpopSplit('p2', 10, p1, 0.4); } " + gen2_stop, "non-sexual simulation", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { sim.addSubpopSplit(2, 10, p1, 0.4); } " + gen2_stop, "non-sexual simulation", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { sim.addSubpopSplit('p2', 10, p1, 0.5); } " + gen2_stop, __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { sim.addSubpopSplit(2, 10, p1, 0.5); } " + gen2_stop, __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { x = sim.addSubpopSplit('p7', 10, p1); if (x == p7) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { x = sim.addSubpopSplit(7, 10, p1); if (x == p7) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p7 = 17; sim.addSubpopSplit('p7', 10, p1); stop(); }", "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { sim.addSubpopSplit('p7', 10, p1); sim.addSubpopSplit(7, 10, p1); stop(); }", "used already", __LINE__);
	
	// Test sim - (void)deregisterScriptBlock(io<SLiMEidosBlock> scriptBlocks)
	SLiMAssertScriptSuccess(gen1_setup_p1 + "1 early() { community.deregisterScriptBlock(s1); } s1 2 early() { stop(); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1 + "1 early() { community.deregisterScriptBlock(1); } s1 2 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { community.deregisterScriptBlock(object()); } s1 2 early() { stop(); }", __LINE__);									// legal: deregister nothing
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { community.deregisterScriptBlock(c(s1, s1)); } s1 2 early() { stop(); }", "same script block", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { community.deregisterScriptBlock(c(1, 1)); } s1 2 early() { stop(); }", "same script block", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { community.deregisterScriptBlock(s1); community.deregisterScriptBlock(s1); } s1 2 early() { stop(); }", "same script block", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { community.deregisterScriptBlock(1); community.deregisterScriptBlock(1); } s1 2 early() { stop(); }", "same script block", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1 + "1 early() { community.deregisterScriptBlock(c(s1, s2)); } s1 2 early() { stop(); } s2 3 early() { stop(); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1 + "1 early() { community.deregisterScriptBlock(c(1, 2)); } s1 2 early() { stop(); } s2 3 early() { stop(); }", __LINE__);
	
	// Test sim - (object<Individual>)individualsWithPedigreeIDs(integer pedigreeIDs, [Nio<Subpopulation> subpops = NULL])
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { sim.individualsWithPedigreeIDs(1); }", "when pedigree recording", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(keepPedigrees=T); }" + gen1_setup_p1 + "1 early() { i = sim.individualsWithPedigreeIDs(integer(0)); if (identical(i, p1.individuals[integer(0)])) stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(keepPedigrees=T); }" + gen1_setup_p1 + "1 early() { i = sim.individualsWithPedigreeIDs(100000); if (identical(i, p1.individuals[integer(0)])) stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(keepPedigrees=T); }" + gen1_setup_p1 + "1 early() { i = sim.individualsWithPedigreeIDs(rep(100000, 1000)); if (identical(i, p1.individuals[integer(0)])) stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(keepPedigrees=T); }" + gen1_setup_p1 + "10 early() { i1 = p1.individuals[0]; ids = i1.pedigreeID; i2 = sim.individualsWithPedigreeIDs(ids); if (identical(i1, i2)) stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(keepPedigrees=T); }" + gen1_setup_p1 + "10 early() { i1 = p1.individuals; ids = i1.pedigreeID; i2 = sim.individualsWithPedigreeIDs(ids); if (identical(i1, i2)) stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(keepPedigrees=T); }" + gen1_setup_p1 + "10 early() { i1 = sample(p1.individuals, 1000, replace=T); ids = i1.pedigreeID; i2 = sim.individualsWithPedigreeIDs(ids); if (identical(i1, i2)) stop(); }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { sim.individualsWithPedigreeIDs(1, p1); }", "when pedigree recording", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(keepPedigrees=T); }" + gen1_setup_p1 + "1 early() { i = sim.individualsWithPedigreeIDs(integer(0), p1); if (identical(i, p1.individuals[integer(0)])) stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(keepPedigrees=T); }" + gen1_setup_p1 + "1 early() { i = sim.individualsWithPedigreeIDs(100000, p1); if (identical(i, p1.individuals[integer(0)])) stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(keepPedigrees=T); }" + gen1_setup_p1 + "1 early() { i = sim.individualsWithPedigreeIDs(rep(100000, 1000), p1); if (identical(i, p1.individuals[integer(0)])) stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(keepPedigrees=T); }" + gen1_setup_p1 + "10 early() { i1 = p1.individuals[0]; ids = i1.pedigreeID; i2 = sim.individualsWithPedigreeIDs(ids, p1); if (identical(i1, i2)) stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(keepPedigrees=T); }" + gen1_setup_p1 + "10 early() { i1 = p1.individuals; ids = i1.pedigreeID; i2 = sim.individualsWithPedigreeIDs(ids, p1); if (identical(i1, i2)) stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(keepPedigrees=T); }" + gen1_setup_p1 + "10 early() { i1 = sample(p1.individuals, 1000, replace=T); ids = i1.pedigreeID; i2 = sim.individualsWithPedigreeIDs(ids, p1); if (identical(i1, i2)) stop(); }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { sim.individualsWithPedigreeIDs(1, 1); }", "when pedigree recording", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(keepPedigrees=T); }" + gen1_setup_p1 + "1 early() { sim.individualsWithPedigreeIDs(1, 10); }", "p10 not defined", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(keepPedigrees=T); }" + gen1_setup_p1 + "1 early() { i = sim.individualsWithPedigreeIDs(integer(0), 1); if (identical(i, p1.individuals[integer(0)])) stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(keepPedigrees=T); }" + gen1_setup_p1 + "1 early() { i = sim.individualsWithPedigreeIDs(100000, 1); if (identical(i, p1.individuals[integer(0)])) stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(keepPedigrees=T); }" + gen1_setup_p1 + "1 early() { i = sim.individualsWithPedigreeIDs(rep(100000, 1000), 1); if (identical(i, p1.individuals[integer(0)])) stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(keepPedigrees=T); }" + gen1_setup_p1 + "10 early() { i1 = p1.individuals[0]; ids = i1.pedigreeID; i2 = sim.individualsWithPedigreeIDs(ids, 1); if (identical(i1, i2)) stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(keepPedigrees=T); }" + gen1_setup_p1 + "10 early() { i1 = p1.individuals; ids = i1.pedigreeID; i2 = sim.individualsWithPedigreeIDs(ids, 1); if (identical(i1, i2)) stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(keepPedigrees=T); }" + gen1_setup_p1 + "10 early() { i1 = sample(p1.individuals, 1000, replace=T); ids = i1.pedigreeID; i2 = sim.individualsWithPedigreeIDs(ids, 1); if (identical(i1, i2)) stop(); }", __LINE__);
	
	// Test sim - (void)killIndividuals(object<Individual> individuals)
	// this is also done in the test script killIndividuals_test.slim, in Miscellaneous
	SLiMAssertScriptSuccess(nonWF_prefix + gen1_setup_p1_100 + "2:10 first() { p1.individuals.tag = 0; s = p1.sampleIndividuals(3); s.tag = 1; sim.killIndividuals(s); if (sum(p1.individuals.tag) != 0) stop(); }", __LINE__);
	SLiMAssertScriptSuccess(nonWF_prefix + gen1_setup_p1_100 + "2:10 early() { p1.individuals.tag = 0; s = p1.sampleIndividuals(3); s.tag = 1; sim.killIndividuals(s); if (sum(p1.individuals.tag) != 0) stop(); }", __LINE__);
	SLiMAssertScriptSuccess(nonWF_prefix + gen1_setup_p1_100 + "2:10 late() { p1.individuals.tag = 0; s = p1.sampleIndividuals(3); s.tag = 1; sim.killIndividuals(s); if (sum(p1.individuals.tag) != 0) stop(); }", __LINE__);
	SLiMAssertScriptSuccess(nonWF_prefix + gen1_setup_sex_p1_100 + "2:10 first() { p1.individuals.tag = 0; s = p1.sampleIndividuals(3); s.tag = 1; sim.killIndividuals(s); if (sum(p1.individuals.tag) != 0) stop(); }", __LINE__);
	SLiMAssertScriptSuccess(nonWF_prefix + gen1_setup_sex_p1_100 + "2:10 early() { p1.individuals.tag = 0; s = p1.sampleIndividuals(3); s.tag = 1; sim.killIndividuals(s); if (sum(p1.individuals.tag) != 0) stop(); }", __LINE__);
	SLiMAssertScriptSuccess(nonWF_prefix + gen1_setup_sex_p1_100 + "2:10 late() { p1.individuals.tag = 0; s = p1.sampleIndividuals(3); s.tag = 1; sim.killIndividuals(s); if (sum(p1.individuals.tag) != 0) stop(); }", __LINE__);
	
	// Test sim - (float)mutationFrequencies(Nio<Subpopulation> subpops, [object<Mutation> mutations])
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 early() { sim.mutationFrequencies(p1); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 early() { sim.mutationFrequencies(c(p1, p2)); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 early() { sim.mutationFrequencies(NULL); }", __LINE__);												// legal, requests population-wide frequencies
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 early() { sim.mutationFrequencies(sim.subpopulations); }", __LINE__);								// legal, requests population-wide frequencies
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 early() { sim.mutationFrequencies(object()); }", __LINE__);											// legal to specify an empty object vector
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 early() { sim.mutationFrequencies(10); }", "p10 not defined", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 early() { sim.mutationFrequencies(1); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 early() { sim.mutationFrequencies(1:2); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 early() { sim.mutationFrequencies(sim.subpopulations.id); }", __LINE__);								// legal, requests population-wide frequencies
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 early() { sim.mutationFrequencies(integer(0)); }", __LINE__);										// legal to specify an empty integer vector
	
	// Test sim - (integer)mutationCounts(Nio<Subpopulation> subpops, [object<Mutation> mutations])
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 early() { sim.mutationCounts(p1); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 early() { sim.mutationCounts(c(p1, p2)); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 early() { sim.mutationCounts(NULL); }", __LINE__);													// legal, requests population-wide frequencies
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 early() { sim.mutationCounts(sim.subpopulations); }", __LINE__);										// legal, requests population-wide frequencies
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 early() { sim.mutationCounts(object()); }", __LINE__);												// legal to specify an empty object vector
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 early() { sim.mutationCounts(10); }", "p10 not defined", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 early() { sim.mutationCounts(1); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 early() { sim.mutationCounts(1:2); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 early() { sim.mutationCounts(sim.subpopulations.id); }", __LINE__);									// legal, requests population-wide frequencies
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 early() { sim.mutationCounts(integer(0)); }", __LINE__);												// legal to specify an empty integer vector
	
	// Test sim - (object<Mutation>)mutationsOfType(io<MutationType>$ mutType)
	SLiMAssertScriptSuccess(gen1_setup_p1 + "10 early() { sim.mutationsOfType(m1); } ", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1 + "10 early() { sim.mutationsOfType(1); } ", __LINE__);
	
	// Test sim - (object<Mutation>)countOfMutationsOfType(io<MutationType>$ mutType)
	SLiMAssertScriptSuccess(gen1_setup_p1 + "10 early() { sim.countOfMutationsOfType(m1); } ", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1 + "10 early() { sim.countOfMutationsOfType(1); } ", __LINE__);
	
	// Test sim - (void)outputFixedMutations(void)
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 late() { sim.outputFixedMutations(); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 late() { sim.outputFixedMutations(NULL); }", __LINE__);
	if (Eidos_TemporaryDirectoryExists())
		SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 late() { sim.outputFixedMutations('" + temp_path + "/slimOutputFixedTest.txt'); }", __LINE__);
	
	// Test sim - (void)outputFull([string$ filePath])
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 late() { sim.outputFull(); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 late() { sim.outputFull(NULL); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 late() { sim.outputFull(spatialPositions=T); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 late() { sim.outputFull(spatialPositions=F); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 late() { sim.outputFull(ages=T); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 late() { sim.outputFull(ages=F); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_i1x + "1 late() { sim.outputFull(spatialPositions=T); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_i1x + "1 late() { sim.outputFull(spatialPositions=F); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_i1x + "1 late() { sim.outputFull(ages=T); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_i1x + "1 late() { sim.outputFull(ages=F); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 late() { sim.outputFull(NULL, T); }", "cannot output in binary format", __LINE__);
	if (Eidos_TemporaryDirectoryExists())
	{
		SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 late() { sim.outputFull('" + temp_path + "/slimOutputFullTest.txt'); }", __LINE__);								// legal, output to file path; this test might work only on Un*x systems
		SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 late() { sim.outputFull('" + temp_path + "/slimOutputFullTest.slimbinary', T); }", __LINE__);						// legal, output to file path; this test might work only on Un*x systems
		SLiMAssertScriptSuccess(gen1_setup_i1x + "1 late() { p1.individuals.x = runif(10); sim.outputFull('" + temp_path + "/slimOutputFullTest_POSITIONS.txt'); }", __LINE__);
		SLiMAssertScriptSuccess(gen1_setup_i1x + "1 late() { p1.individuals.x = runif(10); sim.outputFull('" + temp_path + "/slimOutputFullTest_POSITIONS.slimbinary', T); }", __LINE__);
	}
	
	// Test sim - (void)outputMutations(object<Mutation> mutations)
	SLiMAssertScriptSuccess(gen1_setup_highmut_p1 + "5 late() { sim.outputMutations(sim.mutations); }", __LINE__);											// legal; should have some mutations by gen 5
	SLiMAssertScriptSuccess(gen1_setup_highmut_p1 + "5 late() { sim.outputMutations(sim.mutations[0]); }", __LINE__);										// legal; output just one mutation
	SLiMAssertScriptSuccess(gen1_setup_highmut_p1 + "5 late() { sim.outputMutations(sim.mutations[integer(0)]); }", __LINE__);								// legal to specify an empty object vector
	SLiMAssertScriptSuccess(gen1_setup_highmut_p1 + "5 late() { sim.outputMutations(object()); }", __LINE__);												// legal to specify an empty object vector
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "5 late() { sim.outputMutations(NULL); }", "cannot be type NULL", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_highmut_p1 + "5 late() { sim.outputMutations(sim.mutations, NULL); }", __LINE__);
	if (Eidos_TemporaryDirectoryExists())
		SLiMAssertScriptSuccess(gen1_setup_highmut_p1 + "5 late() { sim.outputMutations(sim.mutations, '" + temp_path + "/slimOutputMutationsTest.txt'); }", __LINE__);
	
	// Test sim - (void)readFromPopulationFile(string$ filePath)
	if (Eidos_TemporaryDirectoryExists())
	{
		SLiMAssertScriptSuccess(gen1_setup + "1 early() { sim.readFromPopulationFile('" + temp_path + "/slimOutputFullTest.txt'); }", __LINE__);												// legal, read from file path; depends on the outputFull() test above
		SLiMAssertScriptSuccess(gen1_setup + "1 early() { sim.readFromPopulationFile('" + temp_path + "/slimOutputFullTest.slimbinary'); }", __LINE__);										// legal, read from file path; depends on the outputFull() test above
		SLiMAssertScriptRaise(gen1_setup + "1 early() { sim.readFromPopulationFile('" + temp_path + "/slimOutputFullTest_POSITIONS.txt'); }", "spatial dimension or age information", __LINE__);
		SLiMAssertScriptRaise(gen1_setup + "1 early() { sim.readFromPopulationFile('" + temp_path + "/slimOutputFullTest_POSITIONS.slimbinary'); }", "output spatial dimensionality does not match", __LINE__);
		SLiMAssertScriptSuccess(gen1_setup_i1x + "1 early() { sim.readFromPopulationFile('" + temp_path + "/slimOutputFullTest_POSITIONS.txt'); }", __LINE__);
		SLiMAssertScriptSuccess(gen1_setup_i1x + "1 early() { sim.readFromPopulationFile('" + temp_path + "/slimOutputFullTest_POSITIONS.slimbinary'); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup + "1 early() { sim.readFromPopulationFile('" + temp_path + "/notAFile.foo'); }", "does not exist or is empty", __LINE__);
		SLiMAssertScriptSuccess(gen1_setup_p1 + "1 early() { sim.readFromPopulationFile('" + temp_path + "/slimOutputFullTest.txt'); if (size(sim.subpopulations) != 3) stop(); }", __LINE__);			// legal; should wipe previous state
		SLiMAssertScriptSuccess(gen1_setup_p1 + "1 early() { sim.readFromPopulationFile('" + temp_path + "/slimOutputFullTest.slimbinary'); if (size(sim.subpopulations) != 3) stop(); }", __LINE__);	// legal; should wipe previous state
	}
	
	// Test sim - (object<SLiMEidosBlock>)registerFirstEvent(Nis$ id, string$ source, [integer$ start], [integer$ end])
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { community.registerFirstEvent(NULL, '{ stop(); }', 2, 2); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { community.registerFirstEvent('s1', '{ stop(); }', 2, 2); } s1 early() { }", "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { s1 = 7; community.registerFirstEvent('s1', '{ stop(); }', 2, 2); }", "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { s1 = 7; community.registerFirstEvent(1, '{ stop(); }', 2, 2); }", "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { community.registerFirstEvent(1, '{ stop(); }', 2, 2); community.registerFirstEvent(1, '{ stop(); }', 2, 2); }", "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { community.registerFirstEvent(1, '{ stop(); }', 3, 2); }", "requires start <= end", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { community.registerFirstEvent(1, '{ stop(); }', -1, -1); }", "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { community.registerFirstEvent(1, '{ stop(); }', 0, 0); }", "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { community.registerFirstEvent(1, '{ $; }', 2, 2); }", "unexpected token '$'", __LINE__);
	
	// Test sim - (object<SLiMEidosBlock>)registerEarlyEvent(Nis$ id, string$ source, [integer$ start], [integer$ end])
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { community.registerEarlyEvent(NULL, '{ stop(); }', 2, 2); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { community.registerEarlyEvent('s1', '{ stop(); }', 2, 2); } s1 early() { }", "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { s1 = 7; community.registerEarlyEvent('s1', '{ stop(); }', 2, 2); }", "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { s1 = 7; community.registerEarlyEvent(1, '{ stop(); }', 2, 2); }", "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { community.registerEarlyEvent(1, '{ stop(); }', 2, 2); community.registerEarlyEvent(1, '{ stop(); }', 2, 2); }", "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { community.registerEarlyEvent(1, '{ stop(); }', 3, 2); }", "requires start <= end", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { community.registerEarlyEvent(1, '{ stop(); }', -1, -1); }", "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { community.registerEarlyEvent(1, '{ stop(); }', 0, 0); }", "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { community.registerEarlyEvent(1, '{ $; }', 2, 2); }", "unexpected token '$'", __LINE__);
	
	// Test sim - (object<SLiMEidosBlock>)registerLateEvent(Nis$ id, string$ source, [integer$ start], [integer$ end])
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { community.registerLateEvent(NULL, '{ stop(); }', 2, 2); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { community.registerLateEvent('s1', '{ stop(); }', 2, 2); } s1 early() { }", "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { s1 = 7; community.registerLateEvent('s1', '{ stop(); }', 2, 2); }", "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { s1 = 7; community.registerLateEvent(1, '{ stop(); }', 2, 2); }", "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { community.registerLateEvent(1, '{ stop(); }', 2, 2); community.registerLateEvent(1, '{ stop(); }', 2, 2); }", "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { community.registerLateEvent(1, '{ stop(); }', 3, 2); }", "requires start <= end", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { community.registerLateEvent(1, '{ stop(); }', -1, -1); }", "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { community.registerLateEvent(1, '{ stop(); }', 0, 0); }", "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { community.registerLateEvent(1, '{ $; }', 2, 2); }", "unexpected token '$'", __LINE__);
	
	// Test sim - (object<SLiMEidosBlock>)registerFitnessEffectCallback(Nis$ id, string$ source, [Nio<Subpopulation>$ subpop], [integer$ start], [integer$ end])
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "1 early() { sim.registerFitnessEffectCallback(NULL, '{ stop(); }', NULL, 5, 10); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "1 early() { sim.registerFitnessEffectCallback(NULL, '{ stop(); }', p1, 5, 10); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "1 early() { sim.registerFitnessEffectCallback(NULL, '{ stop(); }'); } 10 early() { ; }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 early() { sim.registerFitnessEffectCallback('s1', '{ stop(); }', NULL, 2, 2); } s1 early() { }", "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 early() { s1 = 7; sim.registerFitnessEffectCallback('s1', '{ stop(); }', NULL, 2, 2); }", "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 early() { s1 = 7; sim.registerFitnessEffectCallback(1, '{ stop(); }', NULL, 2, 2); }", "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 early() { sim.registerFitnessEffectCallback(1, '{ stop(); }', NULL, 2, 2); sim.registerFitnessEffectCallback(1, '{ stop(); }', NULL, 2, 2); }", "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 early() { sim.registerFitnessEffectCallback(1, '{ stop(); }', NULL, 3, 2); }", "requires start <= end", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 early() { sim.registerFitnessEffectCallback(1, '{ stop(); }', NULL, -1, -1); }", "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 early() { sim.registerFitnessEffectCallback(1, '{ stop(); }', NULL, 0, 0); }", "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 early() { sim.registerFitnessEffectCallback(1, '{ $; }', NULL, 2, 2); }", "unexpected token '$'", __LINE__);
	
	// Test sim - (object<SLiMEidosBlock>)registerMutationEffectCallback(Nis$ id, string$ source, io<MutationType>$ mutType, [Nio<Subpopulation>$ subpop], [integer$ start], [integer$ end])
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "1 early() { sim.registerMutationEffectCallback(NULL, '{ stop(); }', 1, NULL, 5, 10); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "1 early() { sim.registerMutationEffectCallback(NULL, '{ stop(); }', m1, NULL, 5, 10); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "1 early() { sim.registerMutationEffectCallback(NULL, '{ stop(); }', 1, 1, 5, 10); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "1 early() { sim.registerMutationEffectCallback(NULL, '{ stop(); }', m1, p1, 5, 10); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "1 early() { sim.registerMutationEffectCallback(NULL, '{ stop(); }', 1); } 10 early() { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "1 early() { sim.registerMutationEffectCallback(NULL, '{ stop(); }', m1); } 10 early() { ; }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 early() { sim.registerMutationEffectCallback(NULL, '{ stop(); }'); }", "missing required argument", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 early() { sim.registerMutationEffectCallback('s1', '{ stop(); }', m1, NULL, 2, 2); } s1 early() { }", "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 early() { s1 = 7; sim.registerMutationEffectCallback('s1', '{ stop(); }', m1, NULL, 2, 2); }", "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 early() { s1 = 7; sim.registerMutationEffectCallback(1, '{ stop(); }', m1, NULL, 2, 2); }", "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 early() { sim.registerMutationEffectCallback(1, '{ stop(); }', m1, NULL, 2, 2); sim.registerMutationEffectCallback(1, '{ stop(); }', m1, NULL, 2, 2); }", "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 early() { sim.registerMutationEffectCallback(1, '{ stop(); }', m1, NULL, 3, 2); }", "requires start <= end", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 early() { sim.registerMutationEffectCallback(1, '{ stop(); }', m1, NULL, -1, -1); }", "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 early() { sim.registerMutationEffectCallback(1, '{ stop(); }', m1, NULL, 0, 0); }", "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 early() { sim.registerMutationEffectCallback(1, '{ $; }', m1, NULL, 2, 2); }", "unexpected token '$'", __LINE__);
	
	// Test community - (object<SLiMEidosBlock>)registerInteractionCallback(Nis$ id, string$ source, io<InteractionType>$ intType, [Nio<Subpopulation>$ subpop], [integer$ start], [integer$ end])
	SLiMAssertScriptStop(gen1_setup_i1 + "late() { i1.evaluate(p1); i1.strength(p1.individuals[0]); } 1 early() { community.registerInteractionCallback(NULL, '{ stop(); }', 1, NULL, 5, 10); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1 + "late() { i1.evaluate(p1); i1.strength(p1.individuals[0]); } 1 early() { community.registerInteractionCallback(NULL, '{ stop(); }', i1, NULL, 5, 10); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1 + "late() { i1.evaluate(p1); i1.strength(p1.individuals[0]); } 1 early() { community.registerInteractionCallback(NULL, '{ stop(); }', 1, 1, 5, 10); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1 + "late() { i1.evaluate(p1); i1.strength(p1.individuals[0]); } 1 early() { community.registerInteractionCallback(NULL, '{ stop(); }', i1, p1, 5, 10); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1 + "late() { i1.evaluate(p1); i1.strength(p1.individuals[0]); } 1 early() { community.registerInteractionCallback(NULL, '{ stop(); }', 1); } 10 early() { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1 + "late() { i1.evaluate(p1); i1.strength(p1.individuals[0]); } 1 early() { community.registerInteractionCallback(NULL, '{ stop(); }', i1); } 10 early() { ; }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1 + "late() { i1.evaluate(p1); i1.strength(p1.individuals[0]); } 1 early() { community.registerInteractionCallback(NULL, '{ stop(); }'); }", "missing required argument", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1 + "late() { i1.evaluate(p1); i1.strength(p1.individuals[0]); } 1 early() { community.registerInteractionCallback('s1', '{ stop(); }', i1, NULL, 2, 2); } s1 early() { }", "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1 + "late() { i1.evaluate(p1); i1.strength(p1.individuals[0]); } 1 early() { s1 = 7; community.registerInteractionCallback('s1', '{ stop(); }', i1, NULL, 2, 2); }", "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1 + "late() { i1.evaluate(p1); i1.strength(p1.individuals[0]); } 1 early() { s1 = 7; community.registerInteractionCallback(1, '{ stop(); }', i1, NULL, 2, 2); }", "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1 + "late() { i1.evaluate(p1); i1.strength(p1.individuals[0]); } 1 early() { community.registerInteractionCallback(1, '{ stop(); }', i1, NULL, 2, 2); community.registerInteractionCallback(1, '{ stop(); }', i1, NULL, 2, 2); }", "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1 + "late() { i1.evaluate(p1); i1.strength(p1.individuals[0]); } 1 early() { community.registerInteractionCallback(1, '{ stop(); }', i1, NULL, 3, 2); }", "requires start <= end", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1 + "late() { i1.evaluate(p1); i1.strength(p1.individuals[0]); } 1 early() { community.registerInteractionCallback(1, '{ stop(); }', i1, NULL, -1, -1); }", "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1 + "late() { i1.evaluate(p1); i1.strength(p1.individuals[0]); } 1 early() { community.registerInteractionCallback(1, '{ stop(); }', i1, NULL, 0, 0); }", "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1 + "late() { i1.evaluate(p1); i1.strength(p1.individuals[0]); } 1 early() { community.registerInteractionCallback(1, '{ $; }', i1, NULL, 2, 2); }", "unexpected token '$'", __LINE__);
	
	// Test sim - (object<SLiMEidosBlock>)registerMateChoiceCallback(Nis$ id, string$ source, [Nio<Subpopulation>$ subpop], [integer$ start], [integer$ end])
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { sim.registerMateChoiceCallback(NULL, '{ stop(); }', NULL, 2, 2); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { sim.registerMateChoiceCallback(NULL, '{ stop(); }', NULL, 2, 2); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { sim.registerMateChoiceCallback(NULL, '{ stop(); }', 1, 2, 2); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { sim.registerMateChoiceCallback(NULL, '{ stop(); }', p1, 2, 2); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { sim.registerMateChoiceCallback(NULL, '{ stop(); }'); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { sim.registerMateChoiceCallback(NULL, '{ stop(); }'); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { sim.registerMateChoiceCallback(NULL); }", "missing required argument", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { sim.registerMateChoiceCallback('s1', '{ stop(); }', NULL, 2, 2); } s1 early() { }", "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { s1 = 7; sim.registerMateChoiceCallback('s1', '{ stop(); }', NULL, 2, 2); }", "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { s1 = 7; sim.registerMateChoiceCallback(1, '{ stop(); }', NULL, 2, 2); }", "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { sim.registerMateChoiceCallback(1, '{ stop(); }', NULL, 2, 2); sim.registerMateChoiceCallback(1, '{ stop(); }', NULL, 2, 2); }", "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { sim.registerMateChoiceCallback(1, '{ stop(); }', NULL, 3, 2); }", "requires start <= end", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { sim.registerMateChoiceCallback(1, '{ stop(); }', NULL, -1, -1); }", "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { sim.registerMateChoiceCallback(1, '{ stop(); }', NULL, 0, 0); }", "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { sim.registerMateChoiceCallback(1, '{ $; }', NULL, 2, 2); }", "unexpected token '$'", __LINE__);
	
	// Test sim - (object<SLiMEidosBlock>)registerModifyChildCallback(Nis$ id, string$ source, [Nio<Subpopulation>$ subpop], [integer$ start], [integer$ end])
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { sim.registerModifyChildCallback(NULL, '{ stop(); }', NULL, 2, 2); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { sim.registerModifyChildCallback(NULL, '{ stop(); }', NULL, 2, 2); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { sim.registerModifyChildCallback(NULL, '{ stop(); }', 1, 2, 2); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { sim.registerModifyChildCallback(NULL, '{ stop(); }', p1, 2, 2); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { sim.registerModifyChildCallback(NULL, '{ stop(); }'); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { sim.registerModifyChildCallback(NULL, '{ stop(); }'); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { sim.registerModifyChildCallback(NULL); }", "missing required argument", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { sim.registerModifyChildCallback('s1', '{ stop(); }', NULL, 2, 2); } s1 early() { }", "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { s1 = 7; sim.registerModifyChildCallback('s1', '{ stop(); }', NULL, 2, 2); }", "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { s1 = 7; sim.registerModifyChildCallback(1, '{ stop(); }', NULL, 2, 2); }", "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { sim.registerModifyChildCallback(1, '{ stop(); }', NULL, 2, 2); sim.registerModifyChildCallback(1, '{ stop(); }', NULL, 2, 2); }", "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { sim.registerModifyChildCallback(1, '{ stop(); }', NULL, 3, 2); }", "requires start <= end", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { sim.registerModifyChildCallback(1, '{ stop(); }', NULL, -1, -1); }", "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { sim.registerModifyChildCallback(1, '{ stop(); }', NULL, 0, 0); }", "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { sim.registerModifyChildCallback(1, '{ $; }', NULL, 2, 2); }", "unexpected token '$'", __LINE__);
	
	// Test sim – (object<SLiMEidosBlock>)rescheduleScriptBlock(io<SLiMEidosBlock>$ block, [Ni$ start = NULL], [Ni$ end = NULL], [Ni ticks = NULL])
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { b = community.rescheduleScriptBlock(s1, start=10, end=9); stop(); } s1 10 early() { }", "requires start <= end", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { b = community.rescheduleScriptBlock(s1, ticks=integer(0)); stop(); } s1 10 early() { }", "requires at least one tick", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { b = community.rescheduleScriptBlock(s1, ticks=c(25, 25)); stop(); } s1 10 early() { }", "same tick cannot be used twice", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { b = community.rescheduleScriptBlock(s1, start=25, end=25, ticks=25); stop(); } s1 10 early() { }", "either start/end or ticks", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { b = community.rescheduleScriptBlock(s1, start=25, end=NULL, ticks=25); stop(); } s1 10 early() { }", "either start/end or ticks", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { b = community.rescheduleScriptBlock(s1, start=NULL, end=25, ticks=25); stop(); } s1 10 early() { }", "either start/end or ticks", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { b = community.rescheduleScriptBlock(s1); stop(); } s1 10 early() { }", "either start/end or ticks", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { b = community.rescheduleScriptBlock(s1, start=25, end=25); r = sapply(b, 'applyValue.start:applyValue.end;'); if (identical(r, 25)) stop(); } s1 10 early() { }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { b = community.rescheduleScriptBlock(s1, start=25, end=29); r = sapply(b, 'applyValue.start:applyValue.end;'); if (identical(r, 25:29)) stop(); } s1 10 early() { }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { b = community.rescheduleScriptBlock(s1, start=NULL, end=29); r = sapply(b, 'applyValue.start:applyValue.end;'); if (identical(r, 1:29)) stop(); } s1 10 early() { }", "for the currently executing", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { b = community.rescheduleScriptBlock(s1, end=29); r = sapply(b, 'applyValue.start:applyValue.end;'); if (identical(r, 1:29)) stop(); } s1 10 early() { }", "for the currently executing", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "2 early() { b = community.rescheduleScriptBlock(s1, start=NULL, end=29); r = sapply(b, 'applyValue.start:applyValue.end;'); if (identical(r, 1:29)) stop(); } s1 10 early() { }", "scheduled for a past tick", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "2 early() { b = community.rescheduleScriptBlock(s1, end=29); r = sapply(b, 'applyValue.start:applyValue.end;'); if (identical(r, 1:29)) stop(); } s1 10 early() { }", "scheduled for a past tick", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { b = community.rescheduleScriptBlock(s1, start=25, end=NULL); if (b.start == 25 & b.end == 1000000001) stop(); } s1 10 early() { }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { b = community.rescheduleScriptBlock(s1, start=25); if (b.start == 25 & b.end == 1000000001) stop(); } s1 10 early() { }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { b = community.rescheduleScriptBlock(s1, ticks=25); r = sapply(b, 'applyValue.start:applyValue.end;'); if (identical(r, 25)) stop(); } s1 10 early() { }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { b = community.rescheduleScriptBlock(s1, ticks=25:28); r = sapply(b, 'applyValue.start:applyValue.end;'); if (identical(r, 25:28)) stop(); } s1 10 early() { }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { b = community.rescheduleScriptBlock(s1, ticks=c(25:28, 35)); r = sapply(b, 'applyValue.start:applyValue.end;'); if (identical(r, c(25:28, 35))) stop(); } s1 10 early() { }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { b = community.rescheduleScriptBlock(s1, ticks=c(13, 25:28)); r = sapply(b, 'applyValue.start:applyValue.end;'); if (identical(r, c(13, 25:28))) stop(); } s1 10 early() { }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { b = community.rescheduleScriptBlock(2, start=10, end=9); stop(); } s1 10 early() { }", "s2 not defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { b = community.rescheduleScriptBlock(1, start=10, end=9); stop(); } s1 10 early() { }", "requires start <= end", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { b = community.rescheduleScriptBlock(1, ticks=integer(0)); stop(); } s1 10 early() { }", "requires at least one tick", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { b = community.rescheduleScriptBlock(1, ticks=c(25, 25)); stop(); } s1 10 early() { }", "same tick cannot be used twice", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { b = community.rescheduleScriptBlock(1, start=25, end=25, ticks=25); stop(); } s1 10 early() { }", "either start/end or ticks", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { b = community.rescheduleScriptBlock(1, start=25, end=NULL, ticks=25); stop(); } s1 10 early() { }", "either start/end or ticks", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { b = community.rescheduleScriptBlock(1, start=NULL, end=25, ticks=25); stop(); } s1 10 early() { }", "either start/end or ticks", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { b = community.rescheduleScriptBlock(1); stop(); } s1 10 early() { }", "either start/end or ticks", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { b = community.rescheduleScriptBlock(1, start=25, end=25); r = sapply(b, 'applyValue.start:applyValue.end;'); if (identical(r, 25)) stop(); } s1 10 early() { }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { b = community.rescheduleScriptBlock(1, start=25, end=29); r = sapply(b, 'applyValue.start:applyValue.end;'); if (identical(r, 25:29)) stop(); } s1 10 early() { }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { b = community.rescheduleScriptBlock(1, start=NULL, end=29); r = sapply(b, 'applyValue.start:applyValue.end;'); if (identical(r, 1:29)) stop(); } s1 10 early() { }", "for the currently executing", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { b = community.rescheduleScriptBlock(1, end=29); r = sapply(b, 'applyValue.start:applyValue.end;'); if (identical(r, 1:29)) stop(); } s1 10 early() { }", "for the currently executing", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "2 early() { b = community.rescheduleScriptBlock(1, start=NULL, end=29); r = sapply(b, 'applyValue.start:applyValue.end;'); if (identical(r, 1:29)) stop(); } s1 10 early() { }", "scheduled for a past tick", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "2 early() { b = community.rescheduleScriptBlock(1, end=29); r = sapply(b, 'applyValue.start:applyValue.end;'); if (identical(r, 1:29)) stop(); } s1 10 early() { }", "scheduled for a past tick", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { b = community.rescheduleScriptBlock(1, start=25, end=NULL); if (b.start == 25 & b.end == 1000000001) stop(); } s1 10 early() { }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { b = community.rescheduleScriptBlock(1, start=25); if (b.start == 25 & b.end == 1000000001) stop(); } s1 10 early() { }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { b = community.rescheduleScriptBlock(1, ticks=25); r = sapply(b, 'applyValue.start:applyValue.end;'); if (identical(r, 25)) stop(); } s1 10 early() { }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { b = community.rescheduleScriptBlock(1, ticks=25:28); r = sapply(b, 'applyValue.start:applyValue.end;'); if (identical(r, 25:28)) stop(); } s1 10 early() { }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { b = community.rescheduleScriptBlock(1, ticks=c(25:28, 35)); r = sapply(b, 'applyValue.start:applyValue.end;'); if (identical(r, c(25:28, 35))) stop(); } s1 10 early() { }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { b = community.rescheduleScriptBlock(1, ticks=c(13, 25:28)); r = sapply(b, 'applyValue.start:applyValue.end;'); if (identical(r, c(13, 25:28))) stop(); } s1 10 early() { }", __LINE__);
	
	// Test Community - (object<LogFile>$)createLogFile(string$ filePath, [Ns initialContents = NULL], [logical$ append = F], [logical$ compress = F], [string$ sep = ","], [Ni$ logInterval = NULL], [Ni$ flushInterval = NULL])
	if (Eidos_TemporaryDirectoryExists())
		SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 late() { path = '" + temp_path + "/slimLogFileTest.txt'; log = community.createLogFile(path, initialContents='# HEADER COMMENT', logInterval=1); log.addTick(); log.addCycle(); log.addSubpopulationSize(p1); } 10 late() { }", __LINE__);
	
	// Test Community - (void)simulationFinished(void)
	SLiMAssertScriptStop(gen1_setup_p1 + "11 early() { stop(); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1 + "10 early() { community.simulationFinished(); } 11 early() { stop(); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1 + "10 early() { sim.simulationFinished(); } 11 early() { stop(); }", __LINE__);
	
	// Test sim - (object<Mutation>)subsetMutations([No<Mutation>$ exclude = NULL], [Nio<MutationType>$ mutationType = NULL], [Ni$ position = NULL], [Nis$ nucleotide = NULL], [Ni$ tag = NULL], [Ni$ id = NULL])
	// unusually, we do this with custom SLiM scripts that check the API stochastically, since it would be difficult
	// to test all the possible parameter combinations otherwise; we do a non-nucleotide test and a nucleotide test
	SLiMAssertScriptSuccess(R"V0G0N(
	initialize() {
		initializeMutationRate(1e-2);
		initializeMutationType('m1', 0.5, 'f', 0.0);
		initializeMutationType('m2', 0.5, 'f', 0.0);
		initializeGenomicElementType('g1', c(m1,m2), c(1,1));
		initializeGenomicElement(g1, 0, 99);
		initializeRecombinationRate(1e-8);
		m2.color="red";
	}
	1 early() { sim.addSubpop('p1', 10); }
	50 early() {
		m=sim.mutations;
		m.tag=rdunif(m.size(), max=5);
		for (i in 1:10000) {
			ex=(runif(1)<0.8) ? NULL else sample(m,1);
			mt=(runif(1)<0.8) ? NULL else ((runif(1) < 0.5) ? m1 else m2);
			pos=(runif(1)<0.8) ? NULL else rdunif(1, max=99);
			tag=(runif(1)<0.8) ? NULL else rdunif(1, max=5);
			id=(runif(1)<0.8) ? NULL else sample(m.id, 1);
			method1=sim.subsetMutations(exclude=ex, mutType=mt, position=pos,
				tag=tag, id=id);
			method2=m;
			if (!isNULL(ex)) method2=method2[method2!=ex];
			if (!isNULL(mt)) method2=method2[method2.mutationType==mt];
			if (!isNULL(pos)) method2=method2[method2.position==pos];
			if (!isNULL(tag)) method2=method2[method2.tag==tag];
			if (!isNULL(id)) method2=method2[method2.id==id];
			
			if (!identical(method1,method2)) stop();
		}
	}
	)V0G0N", __LINE__);
	SLiMAssertScriptSuccess(R"V0G0N(
	initialize() {
		initializeSLiMOptions(nucleotideBased=T);
		initializeAncestralNucleotides(randomNucleotides(100));
		initializeMutationTypeNuc('m1', 0.5, 'f', 0.0);
		initializeMutationTypeNuc('m2', 0.5, 'f', 0.0);
		initializeGenomicElementType('g1', c(m1,m2), c(1,1), mmJukesCantor(1e-2 / 3));
		initializeGenomicElement(g1, 0, 99);
		initializeRecombinationRate(1e-8);
		m2.color="red";
	}
	1 early() { sim.addSubpop('p1', 10); }
	50 early() {
		m=sim.mutations;
		m.tag=rdunif(m.size(), max=5);
		for (i in 1:10000) {
			ex=(runif(1)<0.8) ? NULL else sample(m,1);
			mt=(runif(1)<0.8) ? NULL else ((runif(1) < 0.5) ? m1 else m2);
			pos=(runif(1)<0.8) ? NULL else rdunif(1, max=99);
			nuc=(runif(1)<0.8) ? NULL else rdunif(1, max=3);
			tag=(runif(1)<0.8) ? NULL else rdunif(1, max=5);
			id=(runif(1)<0.8) ? NULL else sample(m.id, 1);
			method1=sim.subsetMutations(exclude=ex, mutType=mt, position=pos,
				nucleotide=nuc, tag=tag, id=id);
			method2=m;
			if (!isNULL(ex)) method2=method2[method2!=ex];
			if (!isNULL(mt)) method2=method2[method2.mutationType==mt];
			if (!isNULL(pos)) method2=method2[method2.position==pos];
			if (!isNULL(nuc)) method2=method2[method2.nucleotideValue==nuc];
			if (!isNULL(tag)) method2=method2[method2.tag==tag];
			if (!isNULL(id)) method2=method2[method2.id==id];
			
			if (!identical(method1,method2)) stop();
		}
	}
	)V0G0N", __LINE__);
	
	// Test sim EidosDictionaryUnretained functionality: - (+)getValue(is$ key) and - (void)setValue(is$ key, + value)
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { sim.setValue('foo', 7:9); sim.setValue('bar', 'baz'); } 10 early() { if (identical(sim.getValue('foo'), 7:9) & identical(sim.getValue('bar'), 'baz')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { sim.setValue('foo', 3:5); sim.setValue('foo', 'foobar'); } 10 early() { if (identical(sim.getValue('foo'), 'foobar')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { sim.setValue('foo', 3:5); sim.setValue('foo', NULL); } 10 early() { if (isNULL(sim.getValue('foo'))) stop(); }", __LINE__);
}

#pragma mark Subpopulation tests
void _RunSubpopulationTests(void)
{
	// ************************************************************************************
	//
	//	Gen 1+ tests: Subpopulation
	//
	
	// Test Subpopulation properties
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (p1.cloningRate == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (p1.firstMaleIndex == p1.firstMaleIndex) stop(); }", __LINE__);					// legal but undefined value in non-sexual sims
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (size(p1.genomes) == 20) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (size(p1.genomesNonNull) == 20) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (size(p1.individuals) == 10) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (p1.id == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(p1.immigrantSubpopFractions, float(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(p1.immigrantSubpopIDs, integer(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (p1.selfingRate == 0.0) stop(); }", __LINE__);									// legal but always 0.0 in non-sexual sims
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (p1.sexRatio == 0.0) stop(); }", __LINE__);										// legal but always 0.0 in non-sexual sims
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(p1.species, sim)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (p1.individualCount == 10) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.tag; }", "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { c(p1,p1).tag; }", "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.tag = 135; if (p1.tag == 135) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.fitnessScaling = 135.0; if (p1.fitnessScaling == 135.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.fitnessScaling = 0.0; if (p1.fitnessScaling == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.fitnessScaling = -0.01; }", "must be >= 0.0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.fitnessScaling = NAN; }", "must be >= 0.0", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (p1.name == 'p1') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.name = 'p1'; if (p1.name == 'p1') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.name = 'foo'; if (p1.name == 'foo') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.name = 'foo'; p1.name = 'bar'; if (p1.name == 'bar') stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.name = 'p2'; }", "subpopulation symbol", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.name = 'foo'; p1.name = 'bar'; p1.name = 'foo'; }", "must be unique", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (p1.description == '') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.description = 'this is groovy'; if (p1.description == 'this is groovy') stop(); }", __LINE__);

	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.cloningRate = 0.0; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.firstMaleIndex = p1.firstMaleIndex; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.genomes = p1.genomes[0]; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.genomesNonNull = p1.genomesNonNull[0]; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.individuals = p1.individuals[0]; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.id = 1; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.immigrantSubpopFractions = 1.0; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.immigrantSubpopIDs = 1; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.selfingRate = 0.0; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.sexRatio = 0.5; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.species = sim; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.individualCount = 10; stop(); }", "read-only property", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (identical(p1.cloningRate, c(0.0,0.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (p1.firstMaleIndex == 5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.genomes) == 20) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.genomesNonNull) == 15) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.individuals) == 10) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (p1.id == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (identical(p1.immigrantSubpopFractions, float(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (identical(p1.immigrantSubpopIDs, integer(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (p1.selfingRate == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (p1.sexRatio == 0.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (identical(p1.species, sim)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (p1.individualCount == 10) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 early() { p1.tag; }", "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 early() { c(p1,p1).tag; }", "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { p1.tag = 135; if (p1.tag == 135) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { p1.fitnessScaling = 135.0; if (p1.fitnessScaling == 135.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { p1.fitnessScaling = 0.0; if (p1.fitnessScaling == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 early() { p1.fitnessScaling = -0.01; }", "must be >= 0.0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 early() { p1.fitnessScaling = NAN; }", "must be >= 0.0", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (p1.name == 'p1') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { p1.name = 'p1'; if (p1.name == 'p1') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { p1.name = 'foo'; if (p1.name == 'foo') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { p1.name = 'foo'; p1.name = 'bar'; if (p1.name == 'bar') stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 early() { p1.name = 'p2'; }", "subpopulation symbol", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 early() { p1.name = 'foo'; p1.name = 'bar'; p1.name = 'foo'; }", "must be unique", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (p1.description == '') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { p1.description = 'this is groovy'; if (p1.description == 'this is groovy') stop(); }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 early() { p1.cloningRate = 0.0; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 early() { p1.firstMaleIndex = p1.firstMaleIndex; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 early() { p1.genomes = p1.genomes[0]; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 early() { p1.genomesNonNull = p1.genomesNonNull[0]; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 early() { p1.individuals = p1.individuals[0]; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 early() { p1.id = 1; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 early() { p1.immigrantSubpopFractions = 1.0; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 early() { p1.immigrantSubpopIDs = 1; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 early() { p1.selfingRate = 0.0; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 early() { p1.sexRatio = 0.5; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 early() { p1.species = sim; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 early() { p1.individualCount = 10; stop(); }", "read-only property", __LINE__);
	
	// Test Subpopulation - (float)cachedFitness(Ni indices)
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(p1.cachedFitness(NULL), rep(1.0, 10))) stop(); }", __LINE__);				// legal (after subpop construction)
	SLiMAssertScriptStop(gen1_setup_p1 + "2 early() { if (identical(p1.cachedFitness(NULL), rep(1.0, 10))) stop(); }", __LINE__);				// legal (after generating children)
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(p1.cachedFitness(0), 1.0)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(p1.cachedFitness(0:3), rep(1.0, 4))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { identical(p1.cachedFitness(-1), rep(1.0, 10)); stop(); }", "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { identical(p1.cachedFitness(10), rep(1.0, 10)); stop(); }", "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { identical(p1.cachedFitness(c(-1,5)), rep(1.0, 10)); stop(); }", "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { identical(p1.cachedFitness(c(5,10)), rep(1.0, 10)); stop(); }", "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "2 early() { identical(p1.cachedFitness(-1), rep(1.0, 10)); stop(); }", "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "2 early() { identical(p1.cachedFitness(10), rep(1.0, 10)); stop(); }", "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "2 early() { identical(p1.cachedFitness(c(-1,5)), rep(1.0, 10)); stop(); }", "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "2 early() { identical(p1.cachedFitness(c(5,10)), rep(1.0, 10)); stop(); }", "out of range", __LINE__);
	
	// Test Subpopulation – (object<Individual>)sampleIndividuals(integer$ size, [logical$ replace = F], [No<Individual>$ exclude = NULL], [Ns$ sex = NULL], [Ni$ tag = NULL], [Ni$ minAge = NULL], [Ni$ maxAge = NULL], [Nl$ migrant = NULL])
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (size(p1.sampleIndividuals(0)) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (size(p1.sampleIndividuals(1)) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (size(p1.sampleIndividuals(2)) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (size(p1.sampleIndividuals(4)) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (size(p1.sampleIndividuals(0, exclude=p1.individuals[2])) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (size(p1.sampleIndividuals(1, exclude=p1.individuals[2])) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (size(p1.sampleIndividuals(2, exclude=p1.individuals[2])) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (size(p1.sampleIndividuals(4, exclude=p1.individuals[2])) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (size(p1.sampleIndividuals(0, replace=T)) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (size(p1.sampleIndividuals(1, replace=T)) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (size(p1.sampleIndividuals(2, replace=T)) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (size(p1.sampleIndividuals(4, replace=T)) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (size(p1.sampleIndividuals(0, replace=T, exclude=p1.individuals[2])) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (size(p1.sampleIndividuals(1, replace=T, exclude=p1.individuals[2])) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (size(p1.sampleIndividuals(2, replace=T, exclude=p1.individuals[2])) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (size(p1.sampleIndividuals(4, replace=T, exclude=p1.individuals[2])) == 4) stop(); }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.sampleIndividuals(-1); }", "requires a sample size", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.sampleIndividuals(15, replace=F); if (size(i) == 10) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.sampleIndividuals(1, sex='M'); }", "in non-sexual models", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 early() { p1.sampleIndividuals(1, sex='W'); }", "unrecognized value for sex", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { p1.individuals.tag = rep(c(0,1),5); if (p1.sampleIndividuals(3, replace=T, exclude=p1.individuals[5], sex='M', tag=1).size() == 3) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { p1.individuals.tag = rep(c(0,1),5); if (p1.sampleIndividuals(3, replace=F, exclude=p1.individuals[5], sex='M', tag=1).size() == 2) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.sampleIndividuals(0, tag=1).tag, integer(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.sampleIndividuals(1, tag=1).tag, c(1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.sampleIndividuals(2, tag=1).tag, c(1,1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.sampleIndividuals(4, tag=1).tag, c(1,1,1,1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.sampleIndividuals(0, exclude=p1.individuals[2], tag=1).tag, integer(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.sampleIndividuals(1, exclude=p1.individuals[2], tag=1).tag, c(1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.sampleIndividuals(2, exclude=p1.individuals[2], tag=1).tag, c(1,1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.sampleIndividuals(4, exclude=p1.individuals[2], tag=1).tag, c(1,1,1,1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.sampleIndividuals(0, replace=T, tag=1).tag, integer(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.sampleIndividuals(1, replace=T, tag=1).tag, c(1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.sampleIndividuals(2, replace=T, tag=1).tag, c(1,1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.sampleIndividuals(4, replace=T, tag=1).tag, c(1,1,1,1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.sampleIndividuals(0, replace=T, exclude=p1.individuals[2], tag=1).tag, integer(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.sampleIndividuals(1, replace=T, exclude=p1.individuals[2], tag=1).tag, c(1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.sampleIndividuals(2, replace=T, exclude=p1.individuals[2], tag=1).tag, c(1,1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.sampleIndividuals(4, replace=T, exclude=p1.individuals[2], tag=1).tag, c(1,1,1,1))) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(0)) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(1)) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(2)) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(4)) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(0, exclude=p1.individuals[2])) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(1, exclude=p1.individuals[2])) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(2, exclude=p1.individuals[2])) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(4, exclude=p1.individuals[2])) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(0, replace=T)) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(1, replace=T)) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(2, replace=T)) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(4, replace=T)) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(0, replace=T, exclude=p1.individuals[2])) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(1, replace=T, exclude=p1.individuals[2])) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(2, replace=T, exclude=p1.individuals[2])) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(4, replace=T, exclude=p1.individuals[2])) == 4) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(0, sex='M')) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(1, sex='M')) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(2, sex='M')) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(4, sex='M')) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(0, exclude=p1.individuals[2], sex='M')) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(1, exclude=p1.individuals[2], sex='M')) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(2, exclude=p1.individuals[2], sex='M')) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(4, exclude=p1.individuals[2], sex='M')) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(0, replace=T, sex='M')) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(1, replace=T, sex='M')) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(2, replace=T, sex='M')) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(4, replace=T, sex='M')) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(0, replace=T, exclude=p1.individuals[2], sex='M')) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(1, replace=T, exclude=p1.individuals[2], sex='M')) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(2, replace=T, exclude=p1.individuals[2], sex='M')) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(4, replace=T, exclude=p1.individuals[2], sex='M')) == 4) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(0, sex='F')) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(1, sex='F')) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(2, sex='F')) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(4, sex='F')) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(0, exclude=p1.individuals[2], sex='F')) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(1, exclude=p1.individuals[2], sex='F')) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(2, exclude=p1.individuals[2], sex='F')) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(4, exclude=p1.individuals[2], sex='F')) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(0, replace=T, sex='F')) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(1, replace=T, sex='F')) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(2, replace=T, sex='F')) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(4, replace=T, sex='F')) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(0, replace=T, exclude=p1.individuals[2], sex='F')) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(1, replace=T, exclude=p1.individuals[2], sex='F')) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(2, replace=T, exclude=p1.individuals[2], sex='F')) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(4, replace=T, exclude=p1.individuals[2], sex='F')) == 4) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(0, migrant=F)) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(1, migrant=F)) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(2, migrant=F)) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(4, migrant=F)) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(0, exclude=p1.individuals[2], migrant=F)) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(1, exclude=p1.individuals[2], migrant=F)) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(2, exclude=p1.individuals[2], migrant=F)) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(4, exclude=p1.individuals[2], migrant=F)) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(0, replace=T, migrant=F)) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(1, replace=T, migrant=F)) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(2, replace=T, migrant=F)) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(4, replace=T, migrant=F)) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(0, replace=T, exclude=p1.individuals[2], migrant=F)) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(1, replace=T, exclude=p1.individuals[2], migrant=F)) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(2, replace=T, exclude=p1.individuals[2], migrant=F)) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(4, replace=T, exclude=p1.individuals[2], migrant=F)) == 4) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(0, migrant=T)) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(1, migrant=T)) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.sampleIndividuals(1, exclude=p1.individuals[2], migrant=T)) == 0) stop(); }", __LINE__);
	
	// Test Subpopulation – (object<Individual>)subsetIndividuals([No<Individual>$ exclude = NULL], [Ns$ sex = NULL], [Ni$ tag = NULL], [Ni$ minAge = NULL], [Ni$ maxAge = NULL], [Nl$ migrant = NULL])
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (size(p1.subsetIndividuals()) == 10) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (size(p1.subsetIndividuals(exclude=p1.individuals[2])) == 9) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.subsetIndividuals(sex='M'); }", "in non-sexual models", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 early() { p1.subsetIndividuals(sex='W'); }", "unrecognized value for sex", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.subsetIndividuals(tag=1).tag, c(1,1,1,1,1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.subsetIndividuals(exclude=p1.individuals[3], tag=1).tag, c(1,1,1,1))) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.subsetIndividuals()) == 10) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.subsetIndividuals(exclude=p1.individuals[2])) == 9) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.subsetIndividuals(sex='M')) == 5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.subsetIndividuals(exclude=p1.individuals[2], sex='M')) == 5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.subsetIndividuals(sex='F')) == 5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.subsetIndividuals(exclude=p1.individuals[2], sex='F')) == 4) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.subsetIndividuals(migrant=F)) == 10) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.subsetIndividuals(exclude=p1.individuals[2], migrant=F)) == 9) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.subsetIndividuals(migrant=T)) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { if (size(p1.subsetIndividuals(exclude=p1.individuals[2], migrant=T)) == 0) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { p1.individuals.tag = rep(c(0,1),5); if (size(p1.subsetIndividuals(sex='F', tag=1)) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { p1.individuals.tag = rep(c(0,1),5); if (size(p1.subsetIndividuals(exclude=p1.individuals[3], sex='F', tag=1)) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { p1.individuals.tag = rep(c(0,1),5); if (size(p1.subsetIndividuals(sex='F', tag=0)) == 3) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { p1.individuals.tag = rep(c(0,1),5); if (size(p1.subsetIndividuals(exclude=p1.individuals[3], sex='F', tag=0)) == 3) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { p1.individuals.tag = rep(c(0,1),5); if (size(p1.subsetIndividuals(tag=1, migrant=F)) == 5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { p1.individuals.tag = rep(c(0,1),5); if (size(p1.subsetIndividuals(exclude=p1.individuals[3], tag=1, migrant=F)) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { p1.individuals.tag = rep(c(0,1),5); if (size(p1.subsetIndividuals(tag=0, migrant=F)) == 5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { p1.individuals.tag = rep(c(0,1),5); if (size(p1.subsetIndividuals(exclude=p1.individuals[3], tag=0, migrant=F)) == 5) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { p1.individuals.tag = rep(c(0,1),5); if (size(p1.subsetIndividuals(tag=1, migrant=T)) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { p1.individuals.tag = rep(c(0,1),5); if (size(p1.subsetIndividuals(exclude=p1.individuals[3], tag=1, migrant=T)) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { p1.individuals.tag = rep(c(0,1),5); if (size(p1.subsetIndividuals(tag=0, migrant=T)) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { p1.individuals.tag = rep(c(0,1),5); if (size(p1.subsetIndividuals(exclude=p1.individuals[3], tag=0, migrant=T)) == 0) stop(); }", __LINE__);
	
	// Test Subpopulation - (void)outputMSSample(integer$ sampleSize, [logical$ replace], [string$ requestedSex], [Ns$ filePath = NULL], [logical$ append = F], [logical$ filterMonomorphic = F])
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputMSSample(1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputMSSample(1, F); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputMSSample(1, T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputMSSample(5); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputMSSample(5, F); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputMSSample(5, T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputMSSample(10); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputMSSample(20); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputMSSample(30); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 late() { p1.outputMSSample(30, F); stop(); }", "not enough eligible genomes", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputMSSample(30, T); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 late() { p1.outputMSSample(1, F, 'M'); stop(); }", "non-sexual simulation", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 late() { p1.outputMSSample(1, F, 'F'); stop(); }", "non-sexual simulation", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputMSSample(1, F, '*'); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 late() { p1.outputMSSample(1, F, 'Z'); stop(); }", "requested sex", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputMSSample(1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputMSSample(1, F); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputMSSample(1, T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputMSSample(5); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputMSSample(5, F); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputMSSample(5, T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputMSSample(10); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputMSSample(20); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputMSSample(30); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 late() { p1.outputMSSample(30, F); stop(); }", "not enough eligible genomes", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputMSSample(30, T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputMSSample(1, F, 'M'); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputMSSample(1, F, 'F'); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputMSSample(1, F, '*'); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 late() { p1.outputMSSample(1, F, 'Z'); stop(); }", "requested sex", __LINE__);
	
	// Test Subpopulation - (void)outputSample(integer$ sampleSize, [logical$ replace], [string$ requestedSex])
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputSample(1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputSample(1, F); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputSample(1, T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputSample(5); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputSample(5, F); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputSample(5, T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputSample(10); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputSample(20); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputSample(30); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 late() { p1.outputSample(30, F); stop(); }", "not enough eligible genomes", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputSample(30, T); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 late() { p1.outputSample(1, F, 'M'); stop(); }", "non-sexual simulation", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 late() { p1.outputSample(1, F, 'F'); stop(); }", "non-sexual simulation", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputSample(1, F, '*'); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 late() { p1.outputSample(1, F, 'Z'); stop(); }", "requested sex", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputSample(1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputSample(1, F); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputSample(1, T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputSample(5); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputSample(5, F); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputSample(5, T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputSample(10); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputSample(20); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputSample(30); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 late() { p1.outputSample(30, F); stop(); }", "not enough eligible genomes", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputSample(30, T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputSample(1, F, 'M'); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputSample(1, F, 'F'); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputSample(1, F, '*'); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 late() { p1.outputSample(1, F, 'Z'); stop(); }", "requested sex", __LINE__);
	
	// Test Subpopulation - (void)outputVCFSample(integer$ sampleSize, [logical$ replace], [string$ requestedSex], [logical$ outputMultiallelics)
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputVCFSample(1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputVCFSample(1, F); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputVCFSample(1, T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputVCFSample(5); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputVCFSample(5, F); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputVCFSample(5, T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputVCFSample(10); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputVCFSample(20); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputVCFSample(30); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 late() { p1.outputVCFSample(30, F); stop(); }", "not enough eligible individuals", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputVCFSample(30, T); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 late() { p1.outputVCFSample(1, F, 'M'); stop(); }", "non-sexual simulation", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 late() { p1.outputVCFSample(1, F, 'F'); stop(); }", "non-sexual simulation", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputVCFSample(1, F, '*'); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 late() { p1.outputVCFSample(1, F, 'Z'); stop(); }", "requested sex", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 late() { p1.outputVCFSample(5, F, 'M', F); stop(); }", "non-sexual simulation", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 late() { p1.outputVCFSample(5, F, 'F', F); stop(); }", "non-sexual simulation", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputVCFSample(5, F, '*', F); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 late() { p1.outputVCFSample(5, F, 'M', T); stop(); }", "non-sexual simulation", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 late() { p1.outputVCFSample(5, F, 'F', T); stop(); }", "non-sexual simulation", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputVCFSample(5, F, '*', T); stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(1, F); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(1, T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(5); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(5, F); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(5, T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(10); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(20); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(30); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(30, F); stop(); }", "not enough eligible individuals", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(30, T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(1, F, 'M'); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(1, F, 'F'); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(1, F, '*'); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(1, F, 'Z'); stop(); }", "requested sex", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(5, F, 'M', F); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(5, F, 'F', F); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(5, F, '*', F); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(5, F, 'M', T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(5, F, 'F', T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(5, F, '*', T); stop(); }", __LINE__);
	
	// Test Subpopulation - (void)setCloningRate(numeric rate)
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.setCloningRate(0.0); } 10 early() { if (p1.cloningRate == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.setCloningRate(0.5); } 10 early() { if (p1.cloningRate == 0.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.setCloningRate(1.0); } 10 early() { if (p1.cloningRate == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.setCloningRate(-0.001); stop(); }", "within [0,1]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.setCloningRate(1.001); stop(); }", "within [0,1]", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { p1.setCloningRate(0.0); } 10 early() { if (identical(p1.cloningRate, c(0.0, 0.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { p1.setCloningRate(0.5); } 10 early() { if (identical(p1.cloningRate, c(0.5, 0.5))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { p1.setCloningRate(1.0); } 10 early() { if (identical(p1.cloningRate, c(1.0, 1.0))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 early() { p1.setCloningRate(-0.001); stop(); }", "within [0,1]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 early() { p1.setCloningRate(1.001); stop(); }", "within [0,1]", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { p1.setCloningRate(c(0.0, 0.1)); } 10 early() { if (identical(p1.cloningRate, c(0.0, 0.1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { p1.setCloningRate(c(0.5, 0.1)); } 10 early() { if (identical(p1.cloningRate, c(0.5, 0.1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { p1.setCloningRate(c(1.0, 0.1)); } 10 early() { if (identical(p1.cloningRate, c(1.0, 0.1))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 early() { p1.setCloningRate(c(0.0, -0.001)); stop(); }", "within [0,1]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 early() { p1.setCloningRate(c(0.0, 1.001)); stop(); }", "within [0,1]", __LINE__);
	
	// Test Subpopulation - (void)setMigrationRates(io<Subpopulation> sourceSubpops, numeric rates)
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "1 early() { p1.setMigrationRates(2, 0.1); } 10 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "1 early() { p1.setMigrationRates(3, 0.1); } 10 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "1 early() { p1.setMigrationRates(c(2, 3), c(0.1, 0.1)); } 10 early() { stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 early() { p1.setMigrationRates(1, 0.1); } 10 early() { stop(); }", "self-referential", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 early() { p1.setMigrationRates(4, 0.1); } 10 early() { stop(); }", "not defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 early() { p1.setMigrationRates(c(2, 1), c(0.1, 0.1)); } 10 early() { stop(); }", "self-referential", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 early() { p1.setMigrationRates(c(2, 4), c(0.1, 0.1)); } 10 early() { stop(); }", "not defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 early() { p1.setMigrationRates(c(2, 2), c(0.1, 0.1)); } 10 early() { stop(); }", "two rates set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 early() { p1.setMigrationRates(c(p2, p2), c(0.1, 0.1)); } 10 early() { stop(); }", "two rates set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 early() { p1.setMigrationRates(c(2, 3), 0.1); } 10 early() { stop(); }", "to be equal in size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 early() { p1.setMigrationRates(2, c(0.1, 0.1)); } 10 early() { stop(); }", "to be equal in size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 early() { p1.setMigrationRates(2, -0.0001); } 10 early() { stop(); }", "within [0,1]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 early() { p1.setMigrationRates(2, 1.0001); } 10 early() { stop(); }", "within [0,1]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 early() { p1.setMigrationRates(c(2, 3), c(0.6, 0.6)); } 10 early() { stop(); }", "must sum to <= 1.0", __LINE__, false);	// raise is from EvolveSubpopulation(); we don't force constraints prematurely
	
	// Test Subpopulation - (void)setSelfingRate(numeric$ rate)
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.setSelfingRate(0.0); } 10 early() { if (p1.selfingRate == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.setSelfingRate(0.5); } 10 early() { if (p1.selfingRate == 0.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.setSelfingRate(1.0); } 10 early() { if (p1.selfingRate == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.setSelfingRate(-0.001); }", "within [0,1]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.setSelfingRate(1.001); }", "within [0,1]", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { p1.setSelfingRate(0.0); stop(); }", __LINE__);													// we permit this, since a rate of 0.0 makes sense even in sexual sims
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 early() { p1.setSelfingRate(0.1); stop(); }", "cannot be called in sexual simulations", __LINE__);
	
	// Test Subpopulation - (void)setSexRatio(float$ sexRatio)
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.setSexRatio(0.0); stop(); }", "cannot be called in asexual simulations", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.setSexRatio(0.1); stop(); }", "cannot be called in asexual simulations", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 early() { p1.setSexRatio(0.0); } 10 early() { if (p1.sexRatio == 0.0) stop(); }", "produced no males", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { p1.setSexRatio(0.1); } 10 early() { if (p1.sexRatio == 0.1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { p1.setSexRatio(0.5); } 10 early() { if (p1.sexRatio == 0.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { p1.setSexRatio(0.9); } 10 early() { if (p1.sexRatio == 0.9) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 early() { p1.setSexRatio(1.0); } 10 early() { if (p1.sexRatio == 1.0) stop(); }", "produced no females", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 early() { p1.setSexRatio(-0.001); }", "within [0,1]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 early() { p1.setSexRatio(1.001); }", "within [0,1]", __LINE__);
	
	// Test Subpopulation - (void)setSubpopulationSize(integer$ size)
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.setSubpopulationSize(0); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.setSubpopulationSize(0); if (p1.individualCount == 10) stop(); }", "undefined identifier", __LINE__);		// the symbol is undefined immediately
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { px=p1; p1.setSubpopulationSize(0); if (px.individualCount == 10) stop(); }", __LINE__);									// does not take visible effect until generating children
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.setSubpopulationSize(0); } 2 early() { if (p1.individualCount == 0) stop(); }", "undefined identifier", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.setSubpopulationSize(20); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.setSubpopulationSize(20); if (p1.individualCount == 10) stop(); }", __LINE__);					// does not take visible effect until generating children
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.setSubpopulationSize(20); } 2 early() { if (p1.individualCount == 20) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.setSubpopulationSize(-1); stop(); }", "out of range", __LINE__);
	
	// Test Subpopulation EidosDictionaryUnretained functionality: - (+)getValue(is$ key) and - (void)setValue(is$ key, + value)
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.setValue('foo', 7:9); p1.setValue('bar', 'baz'); } 10 early() { if (identical(p1.getValue('foo'), 7:9) & identical(p1.getValue('bar'), 'baz')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.setValue('foo', 3:5); p1.setValue('foo', 'foobar'); } 10 early() { if (identical(p1.getValue('foo'), 'foobar')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { p1.setValue('foo', 3:5); p1.setValue('foo', NULL); } 10 early() { if (isNULL(p1.getValue('foo'))) stop(); }", __LINE__);
	
	// Test spatial stuff including spatialBounds, setSpatialBounds(), pointInBounds(), pointPeriodic(), pointReflected(), pointStopped(), pointUniform()
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (identical(p1.spatialBounds, float(0))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.spatialBounds = 0.0; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.setSpatialBounds(-2.0); stop(); }", "setSpatialBounds() cannot be called in non-spatial simulations", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.pointInBounds(-2.0); stop(); }", "pointInBounds() cannot be called in non-spatial simulations", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.pointPeriodic(-2.0); stop(); }", "pointPeriodic() cannot be called in non-spatial simulations", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.pointReflected(-2.0); stop(); }", "pointReflected() cannot be called in non-spatial simulations", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.pointStopped(-2.0); stop(); }", "pointStopped() cannot be called in non-spatial simulations", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.pointUniform(); stop(); }", "pointUniform() cannot be called in non-spatial simulations", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1x + "1 early() { p1.setSpatialBounds(c(-2.0, 7.5)); if (identical(p1.pointInBounds(float(0)), logical(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 early() { p1.setSpatialBounds(c(-2.0, 7.5)); if (identical(p1.pointReflected(float(0)), float(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 early() { p1.setSpatialBounds(c(-2.0, 7.5)); if (identical(p1.pointStopped(float(0)), float(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xPx + "1 early() { p1.setSpatialBounds(c(0.0, 7.5)); if (identical(p1.pointPeriodic(float(0)), float(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 early() { p1.setSpatialBounds(c(-2.0, 7.5)); if (identical(p1.pointUniform(0), float(0))) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1x + "1 early() { if (identical(p1.spatialBounds, c(0.0, 1.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 early() { p1.setSpatialBounds(c(-2.0, 7.5)); if (identical(p1.spatialBounds, c(-2.0, 7.5))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 early() { p1.setSpatialBounds(-2.0); stop(); }", "requires twice as many coordinates", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 early() { p1.setSpatialBounds(c(0.0, 0.0, 1.0, 1.0)); stop(); }", "requires twice as many coordinates", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 early() { p1.setSpatialBounds(c(-2.0, 7.5)); if (p1.pointInBounds(-2.1) == F) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 early() { p1.setSpatialBounds(c(-2.0, 7.5)); if (p1.pointInBounds(-2.0) == T) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 early() { p1.setSpatialBounds(c(-2.0, 7.5)); if (p1.pointInBounds(0.0) == T) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 early() { p1.setSpatialBounds(c(-2.0, 7.5)); if (p1.pointInBounds(7.5) == T) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 early() { p1.setSpatialBounds(c(-2.0, 7.5)); if (p1.pointInBounds(7.6) == F) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 early() { p1.setSpatialBounds(c(-2.0, 7.5)); if (p1.pointInBounds(11.0, 0.0) == F) stop(); }", "too many arguments supplied", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 early() { p1.setSpatialBounds(c(-2.0, 7.5)); if (identical(p1.pointInBounds(c(11.0, 0.0)), c(F,T))) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1x + "1 early() { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointReflected(-15.5) == -0.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 early() { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointReflected(-5.5) == -4.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 early() { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointReflected(-5.0) == -5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 early() { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointReflected(2.0) == 2.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 early() { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointReflected(2.5) == 2.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 early() { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointReflected(3.5) == 1.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 early() { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointReflected(11.0) == -4.0) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 early() { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointReflected(11.0, 0.0) == -4.0) stop(); }", "too many arguments supplied", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 early() { p1.setSpatialBounds(c(-5.0, 2.5)); if (identical(p1.pointReflected(c(-15.5, -5.5, 2.0, 3.5)), c(-0.5, -4.5, 2.0, 1.5))) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1x + "1 early() { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointStopped(-15.5) == -5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 early() { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointStopped(-5.5) == -5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 early() { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointStopped(-5.0) == -5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 early() { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointStopped(2.0) == 2.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 early() { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointStopped(2.5) == 2.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 early() { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointStopped(3.5) == 2.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 early() { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointStopped(11.0) == 2.5) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 early() { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointStopped(11.0, 0.0) == -4.0) stop(); }", "too many arguments supplied", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 early() { p1.setSpatialBounds(c(-5.0, 2.5)); if (identical(p1.pointStopped(c(-15.5, -5.5, 2.0, 3.5)), c(-5.0, -5.0, 2.0, 2.5))) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1x + "1 early() { p1.setSpatialBounds(c(-5.0, 2.5)); if (size(p1.pointUniform()) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 early() { p1.setSpatialBounds(c(-5.0, 2.5)); if (size(p1.pointUniform(1)) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 early() { p1.setSpatialBounds(c(-5.0, 2.5)); if (size(p1.pointUniform(5)) == 5) stop(); }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 early() { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointPeriodic(-15.5) == -0.5) stop(); }", "no periodic spatial dimension", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xPx + "1 early() { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointPeriodic(-15.5) == -0.5) stop(); }", "requires min coordinates to be 0.0", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xPx + "1 early() { p1.setSpatialBounds(c(0.0, 2.5)); if (p1.pointPeriodic(-0.5) == 2.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xPx + "1 early() { p1.setSpatialBounds(c(0.0, 2.5)); if (p1.pointPeriodic(-5.5) == 2.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xPx + "1 early() { p1.setSpatialBounds(c(0.0, 2.5)); if (p1.pointPeriodic(0.0) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xPx + "1 early() { p1.setSpatialBounds(c(0.0, 2.5)); if (p1.pointPeriodic(2.0) == 2.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xPx + "1 early() { p1.setSpatialBounds(c(0.0, 2.5)); if (p1.pointPeriodic(2.5) == 2.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xPx + "1 early() { p1.setSpatialBounds(c(0.0, 2.5)); if (p1.pointPeriodic(3.5) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xPx + "1 early() { p1.setSpatialBounds(c(0.0, 2.5)); if (p1.pointPeriodic(11.0) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xPx + "1 early() { p1.setSpatialBounds(c(0.0, 2.5)); if (p1.pointPeriodic(11.0, 0.0) == -4.0) stop(); }", "too many arguments supplied", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xPx + "1 early() { p1.setSpatialBounds(c(0.0, 2.5)); if (identical(p1.pointPeriodic(c(-0.5, -5.5, 0.0, 2.5, 3.5)), c(2.0, 2.0, 0.0, 2.5, 1.0))) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1xy + "1 early() { p1.setSpatialBounds(c(-2.0, 1.5, 7.5, 4.5)); if (identical(p1.pointInBounds(float(0)), logical(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xy + "1 early() { p1.setSpatialBounds(c(-2.0, 1.5, 7.5, 4.5)); if (identical(p1.pointReflected(float(0)), float(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xy + "1 early() { p1.setSpatialBounds(c(-2.0, 1.5, 7.5, 4.5)); if (identical(p1.pointStopped(float(0)), float(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyPxy + "1 early() { p1.setSpatialBounds(c(0.0, 0.0, 7.5, 4.5)); if (identical(p1.pointPeriodic(float(0)), float(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xy + "1 early() { p1.setSpatialBounds(c(-2.0, 1.5, 7.5, 4.5)); if (identical(p1.pointUniform(0), float(0))) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1xy + "1 early() { if (identical(p1.spatialBounds, c(0.0, 0.0, 1.0, 1.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xy + "1 early() { p1.setSpatialBounds(c(-2.0, 1.5, 7.5, 4.5)); if (identical(p1.spatialBounds, c(-2.0, 1.5, 7.5, 4.5))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xy + "1 early() { p1.setSpatialBounds(c(-2.0, 1.5)); stop(); }", "requires twice as many coordinates", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xy + "1 early() { p1.setSpatialBounds(c(0.0, 0.0, 0.0, 1.0, 1.0, 1.0)); stop(); }", "requires twice as many coordinates", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xy + "1 early() { p1.setSpatialBounds(c(-2.0, 1.5, 7.5, 4.5)); if (p1.pointInBounds(c(-2.1, 2.0)) == F) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xy + "1 early() { p1.setSpatialBounds(c(-2.0, 1.5, 7.5, 4.5)); if (p1.pointInBounds(c(-2.0, 2.0)) == T) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xy + "1 early() { p1.setSpatialBounds(c(-2.0, 1.5, 7.5, 4.5)); if (p1.pointInBounds(c(0.0, 1.0)) == F) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xy + "1 early() { p1.setSpatialBounds(c(-2.0, 1.5, 7.5, 4.5)); if (p1.pointInBounds(c(0.0, 1.5)) == T) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xy + "1 early() { p1.setSpatialBounds(c(-2.0, 1.5, 7.5, 4.5)); if (p1.pointInBounds(c(0.0, 2.0)) == T) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xy + "1 early() { p1.setSpatialBounds(c(-2.0, 1.5, 7.5, 4.5)); if (p1.pointInBounds(c(0.0, 4.5)) == T) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xy + "1 early() { p1.setSpatialBounds(c(-2.0, 1.5, 7.5, 4.5)); if (p1.pointInBounds(c(0.0, 4.6)) == F) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xy + "1 early() { p1.setSpatialBounds(c(-2.0, 1.5, 7.5, 4.5)); if (p1.pointInBounds(c(7.5, 2.0)) == T) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xy + "1 early() { p1.setSpatialBounds(c(-2.0, 1.5, 7.5, 4.5)); if (p1.pointInBounds(c(7.6, 2.0)) == F) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xy + "1 early() { p1.setSpatialBounds(c(-2.0, 1.5, 7.5, 4.5)); if (p1.pointInBounds(c(11.0, 0.0, 0.0)) == F) stop(); }", "exact multiple", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xy + "1 early() { p1.setSpatialBounds(c(-2.0, 1.5, 7.5, 4.5)); if (identical(p1.pointInBounds(c(-2.1, 2.0, 7.5, 2.0)), c(F,T))) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1xy + "1 early() { p1.setSpatialBounds(c(-5.0, 10.5, 2.5, 12.0)); if (identical(p1.pointReflected(c(-15.5, 11)), c(-0.5, 11))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xy + "1 early() { p1.setSpatialBounds(c(-5.0, 10.5, 2.5, 12.0)); if (identical(p1.pointReflected(c(-5.5, 11)), c(-4.5, 11))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xy + "1 early() { p1.setSpatialBounds(c(-5.0, 10.5, 2.5, 12.0)); if (identical(p1.pointReflected(c(-5.0, 11)), c(-5.0, 11))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xy + "1 early() { p1.setSpatialBounds(c(-5.0, 10.5, 2.5, 12.0)); if (identical(p1.pointReflected(c(2.0, 9.5)), c(2.0, 11.5))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xy + "1 early() { p1.setSpatialBounds(c(-5.0, 10.5, 2.5, 12.0)); if (identical(p1.pointReflected(c(2.0, 10.5)), c(2.0, 10.5))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xy + "1 early() { p1.setSpatialBounds(c(-5.0, 10.5, 2.5, 12.0)); if (identical(p1.pointReflected(c(2.0, 11)), c(2.0, 11))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xy + "1 early() { p1.setSpatialBounds(c(-5.0, 10.5, 2.5, 12.0)); if (identical(p1.pointReflected(c(2.0, 12.0)), c(2.0, 12.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xy + "1 early() { p1.setSpatialBounds(c(-5.0, 10.5, 2.5, 12.0)); if (identical(p1.pointReflected(c(2.0, 13.25)), c(2.0, 10.75))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xy + "1 early() { p1.setSpatialBounds(c(-5.0, 10.5, 2.5, 12.0)); if (identical(p1.pointReflected(c(2.5, 11)), c(2.5, 11))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xy + "1 early() { p1.setSpatialBounds(c(-5.0, 10.5, 2.5, 12.0)); if (identical(p1.pointReflected(c(3.5, 11)), c(1.5, 11))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xy + "1 early() { p1.setSpatialBounds(c(-5.0, 10.5, 2.5, 12.0)); if (identical(p1.pointReflected(c(11.0, 11)), c(-4.0, 11))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xy + "1 early() { p1.setSpatialBounds(c(-5.0, 10.5, 2.5, 12.0)); if (identical(p1.pointReflected(c(11.0, 0.0, 0.0)), c(-4.0, 11))) stop(); }", "exact multiple", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xy + "1 early() { p1.setSpatialBounds(c(-5.0, 10.5, 2.5, 12.0)); if (identical(p1.pointReflected(c(-15.5, 11, 2.0, 13.25)), c(-0.5, 11, 2.0, 10.75))) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1xy + "1 early() { p1.setSpatialBounds(c(-5.0, 10.5, 2.5, 12.0)); if (identical(p1.pointStopped(c(-15.5, 11)), c(-5.0, 11))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xy + "1 early() { p1.setSpatialBounds(c(-5.0, 10.5, 2.5, 12.0)); if (identical(p1.pointStopped(c(-5.5, 11)), c(-5.0, 11))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xy + "1 early() { p1.setSpatialBounds(c(-5.0, 10.5, 2.5, 12.0)); if (identical(p1.pointStopped(c(-5.0, 11)), c(-5.0, 11))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xy + "1 early() { p1.setSpatialBounds(c(-5.0, 10.5, 2.5, 12.0)); if (identical(p1.pointStopped(c(2.0, 9.5)), c(2.0, 10.5))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xy + "1 early() { p1.setSpatialBounds(c(-5.0, 10.5, 2.5, 12.0)); if (identical(p1.pointStopped(c(2.0, 10.5)), c(2.0, 10.5))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xy + "1 early() { p1.setSpatialBounds(c(-5.0, 10.5, 2.5, 12.0)); if (identical(p1.pointStopped(c(2.0, 11)), c(2.0, 11))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xy + "1 early() { p1.setSpatialBounds(c(-5.0, 10.5, 2.5, 12.0)); if (identical(p1.pointStopped(c(2.0, 12.0)), c(2.0, 12.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xy + "1 early() { p1.setSpatialBounds(c(-5.0, 10.5, 2.5, 12.0)); if (identical(p1.pointStopped(c(2.0, 13.25)), c(2.0, 12.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xy + "1 early() { p1.setSpatialBounds(c(-5.0, 10.5, 2.5, 12.0)); if (identical(p1.pointStopped(c(2.5, 11)), c(2.5, 11))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xy + "1 early() { p1.setSpatialBounds(c(-5.0, 10.5, 2.5, 12.0)); if (identical(p1.pointStopped(c(3.5, 11)), c(2.5, 11))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xy + "1 early() { p1.setSpatialBounds(c(-5.0, 10.5, 2.5, 12.0)); if (identical(p1.pointStopped(c(11.0, 11)), c(2.5, 11))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xy + "1 early() { p1.setSpatialBounds(c(-5.0, 10.5, 2.5, 12.0)); if (identical(p1.pointStopped(c(11.0, 0.0, 0.0)), c(-4.0, 11))) stop(); }", "exact multiple", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xy + "1 early() { p1.setSpatialBounds(c(-5.0, 10.5, 2.5, 12.0)); if (identical(p1.pointStopped(c(-15.5, 11, 2.0, 13.25)), c(-5.0, 11, 2.0, 12.0))) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1xy + "1 early() { p1.setSpatialBounds(c(-5.0, 10.5, 2.5, 12.0)); if (size(p1.pointUniform()) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xy + "1 early() { p1.setSpatialBounds(c(-5.0, 10.5, 2.5, 12.0)); if (size(p1.pointUniform(1)) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xy + "1 early() { p1.setSpatialBounds(c(-5.0, 10.5, 2.5, 12.0)); if (size(p1.pointUniform(5)) == 10) stop(); }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_i1xy + "1 early() { p1.setSpatialBounds(c(-5.0, 10.5, 2.5, 12.0)); if (identical(p1.pointPeriodic(c(-15.5, 0.0)), c(-0.5, 11))) stop(); }", "no periodic spatial dimension", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyPxy + "1 early() { p1.setSpatialBounds(c(-5.0, 10.5, 2.5, 12.0)); if (identical(p1.pointPeriodic(c(-15.5, 0.0)), c(-0.5, 11))) stop(); }", "requires min coordinates to be 0.0", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyPxy + "1 early() { p1.setSpatialBounds(c(0.0, 0.0, 2.5, 12.0)); if (identical(p1.pointPeriodic(c(-0.5, 11)), c(2.0, 11))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyPxy + "1 early() { p1.setSpatialBounds(c(0.0, 0.0, 2.5, 12.0)); if (identical(p1.pointPeriodic(c(-5.5, 11)), c(2.0, 11))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyPxy + "1 early() { p1.setSpatialBounds(c(0.0, 0.0, 2.5, 12.0)); if (identical(p1.pointPeriodic(c(0.0, 11)), c(0.0, 11))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyPxy + "1 early() { p1.setSpatialBounds(c(0.0, 0.0, 2.5, 12.0)); if (identical(p1.pointPeriodic(c(2.0, -1.5)), c(2.0, 10.5))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyPxy + "1 early() { p1.setSpatialBounds(c(0.0, 0.0, 2.5, 12.0)); if (identical(p1.pointPeriodic(c(2.0, 11)), c(2.0, 11))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyPxy + "1 early() { p1.setSpatialBounds(c(0.0, 0.0, 2.5, 12.0)); if (identical(p1.pointPeriodic(c(2.0, 14.25)), c(2.0, 2.25))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyPxy + "1 early() { p1.setSpatialBounds(c(0.0, 0.0, 2.5, 12.0)); if (identical(p1.pointPeriodic(c(2.5, 11)), c(2.5, 11))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyPxy + "1 early() { p1.setSpatialBounds(c(0.0, 0.0, 2.5, 12.0)); if (identical(p1.pointPeriodic(c(3.5, 11)), c(1.0, 11))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyPxy + "1 early() { p1.setSpatialBounds(c(0.0, 0.0, 2.5, 12.0)); if (identical(p1.pointPeriodic(c(11.0, 11)), c(1.0, 11))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyPxy + "1 early() { p1.setSpatialBounds(c(0.0, 0.0, 2.5, 12.0)); if (identical(p1.pointPeriodic(c(11.0, 0.0, 0.0)), c(-4.0, 11))) stop(); }", "exact multiple", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyPxy + "1 early() { p1.setSpatialBounds(c(0.0, 0.0, 2.5, 12.0)); if (identical(p1.pointPeriodic(c(-0.5, 11, 2.0, -1.5)), c(2.0, 11, 2.0, 10.5))) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 early() { if (identical(p1.spatialBounds, c(0.0, 0.0, 0.0, 1.0, 1.0, 1.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 early() { p1.setSpatialBounds(c(-2.0, -100, 10.0, 7.5, -99.5, 12.0)); if (identical(p1.spatialBounds, c(-2.0, -100, 10.0, 7.5, -99.5, 12.0))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 early() { p1.setSpatialBounds(-2.0); stop(); }", "requires twice as many coordinates", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 early() { p1.setSpatialBounds(c(0.0, 0.0, 1.0, 1.0)); stop(); }", "requires twice as many coordinates", __LINE__);
	
	std::string gen1_setup_i1xyz_bounds(gen1_setup_i1xyz + "1 early() { p1.setSpatialBounds(c(-10.0, 0.0, 10.0,    -9.0, 2.0, 13.0)); ");
	std::string gen1_setup_i1xyzPxz_bounds(gen1_setup_i1xyzPxz + "1 early() { p1.setSpatialBounds(c(0.0, 0.0, 0.0,    9.0, 2.0, 13.0)); ");
	
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (p1.pointInBounds(c(-10.1, 1.0, 11.0)) == F) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (p1.pointInBounds(c(-9.5, 1.0, 11.0)) == T) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (p1.pointInBounds(c(-8.0, 1.0, 11.0)) == F) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (p1.pointInBounds(c(-9.5, -1.0, 11.0)) == F) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (p1.pointInBounds(c(-9.5, 1.0, 11.0)) == T) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (p1.pointInBounds(c(-9.5, 3.0, 11.0)) == F) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (p1.pointInBounds(c(-9.5, 1.0, 9.0)) == F) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (p1.pointInBounds(c(-9.5, 1.0, 11.0)) == T) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (p1.pointInBounds(c(-9.5, 1.0, 14.0)) == F) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_bounds + "if (p1.pointInBounds(11.0, 0.0) == F) stop(); }", "too many arguments supplied", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_bounds + "if (p1.pointInBounds(c(11.0, 0.0)) == F) stop(); }", "requires the length of point", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (identical(p1.pointInBounds(c(-10.1, 1.0, 11.0, -9.5, 1.0, 11.0)), c(F, T))) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (identical(p1.pointReflected(c(-10.5, 1.0, 11.0)), c(-9.5, 1.0, 11.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (identical(p1.pointReflected(c(-9.5, 1.0, 11.0)), c(-9.5, 1.0, 11.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (identical(p1.pointReflected(c(-8.0, 1.0, 11.0)), c(-10.0, 1.0, 11.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (identical(p1.pointReflected(c(-9.5, -1.0, 11.0)), c(-9.5, 1.0, 11.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (identical(p1.pointReflected(c(-9.5, 1.0, 11.0)), c(-9.5, 1.0, 11.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (identical(p1.pointReflected(c(-9.5, 2.5, 11.0)), c(-9.5, 1.5, 11.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (identical(p1.pointReflected(c(-9.5, 1.0, 4.5)), c(-9.5, 1.0, 10.5))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (identical(p1.pointReflected(c(-9.5, 1.0, 11.0)), c(-9.5, 1.0, 11.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (identical(p1.pointReflected(c(-9.5, 1.0, 14.5)), c(-9.5, 1.0, 11.5))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_bounds + "if (p1.pointReflected(11.0, 0.0) == -4.0) stop(); }", "too many arguments supplied", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_bounds + "if (p1.pointReflected(c(11.0, 0.0)) == -4.0) stop(); }", "requires the length of point", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (identical(p1.pointReflected(c(-10.5, -1.0, 4.5, -8.0, 2.5, 14.5)), c(-9.5, 1.0, 10.5, -10.0, 1.5, 11.5))) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (identical(p1.pointStopped(c(-10.5, 1.0, 11.0)), c(-10.0, 1.0, 11.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (identical(p1.pointStopped(c(-9.5, 1.0, 11.0)), c(-9.5, 1.0, 11.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (identical(p1.pointStopped(c(-8.0, 1.0, 11.0)), c(-9.0, 1.0, 11.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (identical(p1.pointStopped(c(-9.5, -1.0, 11.0)), c(-9.5, 0.0, 11.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (identical(p1.pointStopped(c(-9.5, 1.0, 11.0)), c(-9.5, 1.0, 11.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (identical(p1.pointStopped(c(-9.5, 2.5, 11.0)), c(-9.5, 2.0, 11.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (identical(p1.pointStopped(c(-9.5, 1.0, 4.5)), c(-9.5, 1.0, 10.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (identical(p1.pointStopped(c(-9.5, 1.0, 11.0)), c(-9.5, 1.0, 11.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (identical(p1.pointStopped(c(-9.5, 1.0, 14.5)), c(-9.5, 1.0, 13.0))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_bounds + "if (p1.pointStopped(11.0, 0.0) == -4.0) stop(); }", "too many arguments supplied", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_bounds + "if (p1.pointStopped(c(11.0, 0.0)) == -4.0) stop(); }", "requires the length of point", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (identical(p1.pointStopped(c(-10.5, -1.0, 4.5, -8.0, 2.5, 14.5)), c(-10.0, 0.0, 10.0, -9.0, 2.0, 13.0))) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (size(p1.pointUniform()) == 3) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (size(p1.pointUniform(1)) == 3) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (size(p1.pointUniform(5)) == 15) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1xyzPxz_bounds + "if (identical(p1.pointPeriodic(c(-10.5, 1.0, 11.0)), c(7.5, 1.0, 11.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyzPxz_bounds + "if (identical(p1.pointPeriodic(c(-9.5, 1.0, 11.0)), c(8.5, 1.0, 11.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyzPxz_bounds + "if (identical(p1.pointPeriodic(c(-8.0, 1.0, 11.0)), c(1.0, 1.0, 11.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyzPxz_bounds + "if (identical(p1.pointPeriodic(c(-9.5, -1.0, 11.0)), c(8.5, -1.0, 11.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyzPxz_bounds + "if (identical(p1.pointPeriodic(c(-9.5, 1.0, 11.0)), c(8.5, 1.0, 11.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyzPxz_bounds + "if (identical(p1.pointPeriodic(c(-9.5, 2.5, 11.0)), c(8.5, 2.5, 11.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyzPxz_bounds + "if (identical(p1.pointPeriodic(c(-9.5, 1.0, 4.5)), c(8.5, 1.0, 4.5))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyzPxz_bounds + "if (identical(p1.pointPeriodic(c(-9.5, 1.0, 11.0)), c(8.5, 1.0, 11.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyzPxz_bounds + "if (identical(p1.pointPeriodic(c(-9.5, 1.0, 14.5)), c(8.5, 1.0, 1.5))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyzPxz_bounds + "if (p1.pointPeriodic(11.0, 0.0) == -4.0) stop(); }", "too many arguments supplied", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyzPxz_bounds + "if (p1.pointPeriodic(c(11.0, 0.0)) == -4.0) stop(); }", "requires the length of point", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyzPxz_bounds + "if (identical(p1.pointPeriodic(c(-10.5, -1.0, 4.5, -8.0, 2.5, 14.5)), c(7.5, -1.0, 4.5, 1.0, 2.5, 1.5))) stop(); }", __LINE__);
	
	// Test spatial stuff including defineSpatialMap(), spatialMapColor(), and spatialMapValue()
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.defineSpatialMap('map', '', float(0)); stop(); }", "spatiality '' must be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.defineSpatialMap('map', 'x', c(0.0, 1.0)); stop(); }", "spatial dimensions beyond those set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.spatialMapColor('m', 0.5); stop(); }", "could not find map", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.spatialMapValue('m', float(0)); stop(); }", "could not find map", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.spatialMapValue('m', 0.0); stop(); }", "could not find map", __LINE__);
	
	// Test that permutations of defineSpatialMap() that include the old gridSize parameter get a helpful error message
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.defineSpatialMap('map', 'x', gridSize=2, c(0.0, 1.0)); stop(); }", "changed in SLiM 3.5", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.defineSpatialMap('map', 'x', NULL, c(0.0, 1.0)); stop(); }", "changed in SLiM 3.5", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.defineSpatialMap('map', 'x', 2, values=c(0.0, 1.0)); stop(); }", "changed in SLiM 3.5", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.defineSpatialMap('map', 'x', 2, c(0.0, 1.0)); stop(); }", "changed in SLiM 3.5", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.defineSpatialMap('map', 'x', 2, c(0.0, 1.0), interpolate=T); stop(); }", "changed in SLiM 3.5", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.defineSpatialMap('map', 'x', 2, c(0.0, 1.0), T); stop(); }", "changed in SLiM 3.5", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.defineSpatialMap('map', 'x', 2, c(0.0, 1.0), T, valueRange=c(0.0, 1.0)); stop(); }", "changed in SLiM 3.5", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.defineSpatialMap('map', 'x', 2, c(0.0, 1.0), T, c(0.0, 1.0)); stop(); }", "changed in SLiM 3.5", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.defineSpatialMap('map', 'x', 2, c(0.0, 1.0), T, c(0.0, 1.0), colors=c('red','blue')); stop(); }", "changed in SLiM 3.5", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { p1.defineSpatialMap('map', 'x', 2, c(0.0, 1.0), T, c(0.0, 1.0), c('red','blue')); stop(); }", "changed in SLiM 3.5", __LINE__);
	
	// a few tests supplying a matrix/array spatial map instead of a vector; no need to test spatialMapValue() etc. with these,
	// since it all funnels into the same map definition code anyway, so we just need to be sure the pre-funnel code is good...
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 early() { p1.defineSpatialMap('map', 'xy', matrix(1.0:4, nrow=2)); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 early() { p1.defineSpatialMap('map', 'xy', matrix(1.0:9, nrow=3)); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 early() { p1.defineSpatialMap('map', 'xy', matrix(1.0:6, nrow=2)); stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 early() { p1.defineSpatialMap('map', 'xz', matrix(1.0:4, nrow=2)); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 early() { p1.defineSpatialMap('map', 'xz', matrix(1.0:9, nrow=3)); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 early() { p1.defineSpatialMap('map', 'xz', matrix(1.0:6, nrow=2)); stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 early() { p1.defineSpatialMap('map', 'xyz', array(1.0:8, c(2,2,2))); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 early() { p1.defineSpatialMap('map', 'xyz', array(1.0:27, c(3,3,3))); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 early() { p1.defineSpatialMap('map', 'xyz', array(1.0:12, c(2,3,2))); stop(); }", __LINE__);
	
	// 1D sim with 1D x map
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 early() { p1.defineSpatialMap('map', '', float(0)); stop(); }", "spatiality '' must be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 early() { p1.defineSpatialMap('map', 'xy', c(0.0, 1.0)); stop(); }", "does not match the spatiality defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 early() { p1.defineSpatialMap('map', 'xy', matrix(1.0:4, nrow=2)); stop(); }", "spatial dimensions beyond those set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 early() { p1.defineSpatialMap('map', 'x', 0.0); stop(); }", "must be of size >= 2", __LINE__);
	
	std::string gen1_setup_i1x_mapNI(gen1_setup_i1x + "1 early() { p1.defineSpatialMap('map', 'x', c(0.0, 1.0, 3.0), interpolate=F, valueRange=c(-5.0, 5.0), colors=c('black', 'white')); ");
	
	SLiMAssertScriptStop(gen1_setup_i1x_mapNI + "if (p1.spatialMapValue('map', -9.0) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x_mapNI + "if (p1.spatialMapValue('map', 0.0) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x_mapNI + "if (p1.spatialMapValue('map', 0.2) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x_mapNI + "if (p1.spatialMapValue('map', 0.3) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x_mapNI + "if (p1.spatialMapValue('map', 0.5) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x_mapNI + "if (p1.spatialMapValue('map', 0.7) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x_mapNI + "if (p1.spatialMapValue('map', 0.8) == 3.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x_mapNI + "if (p1.spatialMapValue('map', 1.0) == 3.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x_mapNI + "if (p1.spatialMapValue('map', 9.0) == 3.0) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1x_mapNI + "if (p1.spatialMapColor('map', -5.0) == '#000000') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x_mapNI + "if (p1.spatialMapColor('map', -2.5) == '#404040') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x_mapNI + "if (p1.spatialMapColor('map', 0.0001) == '#808080') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x_mapNI + "if (p1.spatialMapColor('map', 2.5) == '#BFBFBF') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x_mapNI + "if (p1.spatialMapColor('map', 5.0) == '#FFFFFF') stop(); }", __LINE__);
	
	std::string gen1_setup_i1x_mapI(gen1_setup_i1x + "1 early() { p1.defineSpatialMap('map', 'x', c(0.0, 1.0, 3.0), interpolate=T, valueRange=c(-5.0, 5.0), colors=c('#FF003F', '#007F00', '#00FFFF')); ");
	
	SLiMAssertScriptStop(gen1_setup_i1x_mapI + "if (p1.spatialMapValue('map', -9.0) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x_mapI + "if (p1.spatialMapValue('map', 0.0) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x_mapI + "if (p1.spatialMapValue('map', 0.25) == 0.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x_mapI + "if (p1.spatialMapValue('map', 0.5) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x_mapI + "if (p1.spatialMapValue('map', 0.75) == 2.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x_mapI + "if (p1.spatialMapValue('map', 1.0) == 3.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x_mapI + "if (p1.spatialMapValue('map', 9.0) == 3.0) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1x_mapI + "if (p1.spatialMapColor('map', -5.0) == '#FF003F') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x_mapI + "if (p1.spatialMapColor('map', -2.5) == '#804020') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x_mapI + "if (p1.spatialMapColor('map', 0.0001) == '#007F00') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x_mapI + "if (p1.spatialMapColor('map', 2.5) == '#00BF80') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x_mapI + "if (p1.spatialMapColor('map', 5.0) == '#00FFFF') stop(); }", __LINE__);
	
	// 3D sim with 1D x map
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 early() { p1.defineSpatialMap('map', '', float(0)); stop(); }", "spatiality '' must be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 early() { p1.defineSpatialMap('map', 'x', 0.0); stop(); }", "must be of size >= 2", __LINE__);
	
	std::string gen1_setup_i1xyz_mapNIx(gen1_setup_i1xyz + "1 early() { p1.defineSpatialMap('map', 'x', c(0.0, 1.0, 3.0), interpolate=F, valueRange=c(-5.0, 5.0), colors=c('black', 'white')); ");
	
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIx + "if (p1.spatialMapValue('map', -9.0) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIx + "if (p1.spatialMapValue('map', 0.0) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIx + "if (p1.spatialMapValue('map', 0.2) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIx + "if (p1.spatialMapValue('map', 0.3) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIx + "if (p1.spatialMapValue('map', 0.5) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIx + "if (p1.spatialMapValue('map', 0.7) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIx + "if (p1.spatialMapValue('map', 0.8) == 3.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIx + "if (p1.spatialMapValue('map', 1.0) == 3.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIx + "if (p1.spatialMapValue('map', 9.0) == 3.0) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIx + "if (p1.spatialMapColor('map', -5.0) == '#000000') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIx + "if (p1.spatialMapColor('map', -2.5) == '#404040') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIx + "if (p1.spatialMapColor('map', 0.0001) == '#808080') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIx + "if (p1.spatialMapColor('map', 2.5) == '#BFBFBF') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIx + "if (p1.spatialMapColor('map', 5.0) == '#FFFFFF') stop(); }", __LINE__);
	
	std::string gen1_setup_i1xyz_mapIx(gen1_setup_i1xyz + "1 early() { p1.defineSpatialMap('map', 'x', c(0.0, 1.0, 3.0), interpolate=T, valueRange=c(-5.0, 5.0), colors=c('#FF003F', '#007F00', '#00FFFF')); ");
	
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIx + "if (p1.spatialMapValue('map', -9.0) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIx + "if (p1.spatialMapValue('map', 0.0) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIx + "if (p1.spatialMapValue('map', 0.25) == 0.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIx + "if (p1.spatialMapValue('map', 0.5) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIx + "if (p1.spatialMapValue('map', 0.75) == 2.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIx + "if (p1.spatialMapValue('map', 1.0) == 3.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIx + "if (p1.spatialMapValue('map', 9.0) == 3.0) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIx + "if (p1.spatialMapColor('map', -5.0) == '#FF003F') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIx + "if (p1.spatialMapColor('map', -2.5) == '#804020') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIx + "if (p1.spatialMapColor('map', 0.0001) == '#007F00') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIx + "if (p1.spatialMapColor('map', 2.5) == '#00BF80') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIx + "if (p1.spatialMapColor('map', 5.0) == '#00FFFF') stop(); }", __LINE__);
	
	// 3D sim with 1D z map
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 early() { p1.defineSpatialMap('map', '', float(0)); stop(); }", "spatiality '' must be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 early() { p1.defineSpatialMap('map', 'z', 0.0); stop(); }", "must be of size >= 2", __LINE__);
	
	std::string gen1_setup_i1xyz_mapNIz(gen1_setup_i1xyz + "1 early() { p1.defineSpatialMap('map', 'z', c(0.0, 1.0, 3.0), interpolate=F, valueRange=c(-5.0, 5.0), colors=c('black', 'white')); ");
	
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIz + "if (p1.spatialMapValue('map', -9.0) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIz + "if (p1.spatialMapValue('map', 0.0) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIz + "if (p1.spatialMapValue('map', 0.2) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIz + "if (p1.spatialMapValue('map', 0.3) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIz + "if (p1.spatialMapValue('map', 0.5) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIz + "if (p1.spatialMapValue('map', 0.7) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIz + "if (p1.spatialMapValue('map', 0.8) == 3.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIz + "if (p1.spatialMapValue('map', 1.0) == 3.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIz + "if (p1.spatialMapValue('map', 9.0) == 3.0) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIz + "if (p1.spatialMapColor('map', -5.0) == '#000000') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIz + "if (p1.spatialMapColor('map', -2.5) == '#404040') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIz + "if (p1.spatialMapColor('map', 0.0001) == '#808080') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIz + "if (p1.spatialMapColor('map', 2.5) == '#BFBFBF') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIz + "if (p1.spatialMapColor('map', 5.0) == '#FFFFFF') stop(); }", __LINE__);
	
	std::string gen1_setup_i1xyz_mapIz(gen1_setup_i1xyz + "1 early() { p1.defineSpatialMap('map', 'z', c(0.0, 1.0, 3.0), interpolate=T, valueRange=c(-5.0, 5.0), colors=c('#FF003F', '#007F00', '#00FFFF')); ");
	
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIz + "if (p1.spatialMapValue('map', -9.0) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIz + "if (p1.spatialMapValue('map', 0.0) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIz + "if (p1.spatialMapValue('map', 0.25) == 0.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIz + "if (p1.spatialMapValue('map', 0.5) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIz + "if (p1.spatialMapValue('map', 0.75) == 2.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIz + "if (p1.spatialMapValue('map', 1.0) == 3.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIz + "if (p1.spatialMapValue('map', 9.0) == 3.0) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIz + "if (p1.spatialMapColor('map', -5.0) == '#FF003F') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIz + "if (p1.spatialMapColor('map', -2.5) == '#804020') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIz + "if (p1.spatialMapColor('map', 0.0001) == '#007F00') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIz + "if (p1.spatialMapColor('map', 2.5) == '#00BF80') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIz + "if (p1.spatialMapColor('map', 5.0) == '#00FFFF') stop(); }", __LINE__);
	
	// 3D sim with 2D xz map; note that these tests were designed with the old matrix interpretation, so now a transpose/flip is needed to make them match
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 early() { p1.defineSpatialMap('map', '', float(0)); stop(); }", "spatiality '' must be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 early() { p1.defineSpatialMap('map', 'xz', 0.0); stop(); }", "does not match the spatiality defined for the map", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 early() { p1.defineSpatialMap('map', 'xz', matrix(0.0)); stop(); }", "must be of size >= 2", __LINE__);
	
	std::string gen1_setup_i1xyz_mapNIxz(gen1_setup_i1xyz + "1 early() { p1.defineSpatialMap('map', 'xz', matrix(c(0.0, 1, 3, 5, 5, 5), ncol=3, byrow=T)[1:0,], interpolate=F, valueRange=c(-5.0, 5.0), colors=c('black', 'white')); ");
	
	SLiMAssertScriptRaise(gen1_setup_i1xyz_mapNIxz + "p1.spatialMapValue('map', 0.0); stop(); }", "must match spatiality of map", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(-9.0, 0.0)) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(0.0, 0.0)) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(0.2, 0.0)) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(0.3, 0.0)) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(0.5, 0.0)) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(0.7, 0.0)) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(0.8, 0.0)) == 3.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(1.0, 0.0)) == 3.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(9.0, 0.0)) == 3.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(-9.0, 0.2)) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(0.0, 0.2)) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(0.2, 0.2)) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(0.3, 0.2)) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(0.5, 0.2)) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(0.7, 0.2)) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(0.8, 0.2)) == 3.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(1.0, 0.2)) == 3.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(9.0, 0.2)) == 3.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(-9.0, 0.8)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(0.0, 0.8)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(0.2, 0.8)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(0.3, 0.8)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(0.5, 0.8)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(0.7, 0.8)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(0.8, 0.8)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(1.0, 0.8)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(9.0, 0.8)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(-9.0, 1.0)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(0.0, 1.0)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(0.2, 1.0)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(0.3, 1.0)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(0.5, 1.0)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(0.7, 1.0)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(0.8, 1.0)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(1.0, 1.0)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(9.0, 1.0)) == 5.0) stop(); }", __LINE__);

	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapColor('map', -5.0) == '#000000') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapColor('map', -2.5) == '#404040') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapColor('map', 0.0001) == '#808080') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapColor('map', 2.5) == '#BFBFBF') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapColor('map', 5.0) == '#FFFFFF') stop(); }", __LINE__);
	
	std::string gen1_setup_i1xyz_mapIxz(gen1_setup_i1xyz + "1 early() { p1.defineSpatialMap('map', 'xz', matrix(c(0.0, 1, 3, 5, 5, 5), ncol=3, byrow=T)[1:0,], interpolate=T, valueRange=c(-5.0, 5.0), colors=c('#FF003F', '#007F00', '#00FFFF')); ");
	
	SLiMAssertScriptRaise(gen1_setup_i1xyz_mapIxz + "p1.spatialMapValue('map', 0.0); stop(); }", "must match spatiality of map", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapValue('map', c(-9.0, 0.0)) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapValue('map', c(0.0, 0.0)) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapValue('map', c(0.25, 0.0)) == 0.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapValue('map', c(0.5, 0.0)) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapValue('map', c(0.75, 0.0)) == 2.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapValue('map', c(1.0, 0.0)) == 3.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapValue('map', c(9.0, 0.0)) == 3.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapValue('map', c(-9.0, 0.5)) == 2.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapValue('map', c(0.0, 0.5)) == 2.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapValue('map', c(0.25, 0.5)) == 2.75) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapValue('map', c(0.5, 0.5)) == 3.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapValue('map', c(0.75, 0.5)) == 3.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapValue('map', c(1.0, 0.5)) == 4.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapValue('map', c(9.0, 0.5)) == 4.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapValue('map', c(-9.0, 1.0)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapValue('map', c(0.0, 1.0)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapValue('map', c(0.25, 1.0)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapValue('map', c(0.5, 1.0)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapValue('map', c(0.75, 1.0)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapValue('map', c(1.0, 1.0)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapValue('map', c(9.0, 1.0)) == 5.0) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapColor('map', -5.0) == '#FF003F') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapColor('map', -2.5) == '#804020') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapColor('map', 0.0001) == '#007F00') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapColor('map', 2.5) == '#00BF80') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapColor('map', 5.0) == '#00FFFF') stop(); }", __LINE__);
	
	// 3D sim with 3D xyz map; note that these tests were designed with the old matrix interpretation, so now a transpose/flip is needed to make them match
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 early() { p1.defineSpatialMap('map', '', float(0)); stop(); }", "spatiality '' must be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 early() { p1.defineSpatialMap('map', 'xyz', 0.0); stop(); }", "does not match the spatiality defined for the map", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 early() { p1.defineSpatialMap('map', 'xyz', array(0.0, c(1,1,1))); stop(); }", "must be of size >= 2", __LINE__);
	
	std::string gen1_setup_i1xyz_mapNIxyz(gen1_setup_i1xyz + "1 early() { p1.defineSpatialMap('map', 'xyz', array(c(3.0, 0, 4, 1, 5, 2, 9, 6, 10, 7, 11, 8), c(2,3,2)), interpolate=F, valueRange=c(-5.0, 5.0), colors=c('black', 'white')); ");
	
	SLiMAssertScriptRaise(gen1_setup_i1xyz_mapNIxyz + "p1.spatialMapValue('map', 0.0); stop(); }", "must match spatiality of map", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(0.0, 0.0, 0.0)) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(0.5, 0.0, 0.0)) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(1.0, 0.0, 0.0)) == 2.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(0.0, 0.8, 0.0)) == 3.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(0.5, 0.8, 0.0)) == 4.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(1.0, 0.8, 0.0)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(0.0, 1.0, 0.0)) == 3.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(0.5, 1.0, 0.0)) == 4.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(1.0, 1.0, 0.0)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(0.0, 0.0, 0.6)) == 6.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(0.5, 0.0, 0.6)) == 7.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(1.0, 0.0, 0.6)) == 8.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(0.0, 0.2, 0.6)) == 6.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(0.5, 0.2, 0.6)) == 7.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(1.0, 0.2, 0.6)) == 8.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(0.0, 1.0, 0.6)) == 9.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(0.5, 1.0, 0.6)) == 10.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(1.0, 1.0, 0.6)) == 11.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(0.0, 0.0, 1.0)) == 6.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(0.5, 0.0, 1.0)) == 7.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(1.0, 0.0, 1.0)) == 8.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(0.0, 0.2, 1.0)) == 6.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(0.5, 0.2, 1.0)) == 7.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(1.0, 0.2, 1.0)) == 8.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(0.0, 1.0, 1.0)) == 9.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(0.5, 1.0, 1.0)) == 10.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(1.0, 1.0, 1.0)) == 11.0) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapColor('map', -5.0) == '#000000') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapColor('map', -2.5) == '#404040') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapColor('map', 0.0001) == '#808080') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapColor('map', 2.5) == '#BFBFBF') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapColor('map', 5.0) == '#FFFFFF') stop(); }", __LINE__);
	
	std::string gen1_setup_i1xyz_mapIxyz(gen1_setup_i1xyz + "1 early() { p1.defineSpatialMap('map', 'xyz', array(c(3.0, 0, 4, 1, 5, 2, 9, 6, 10, 7, 11, 8), c(2,3,2)), interpolate=T, valueRange=c(-5.0, 5.0), colors=c('#FF003F', '#007F00', '#00FFFF')); ");
	
	SLiMAssertScriptRaise(gen1_setup_i1xyz_mapIxyz + "p1.spatialMapValue('map', 0.0); stop(); }", "must match spatiality of map", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(0.0, 0.0, 0.0)) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(0.5, 0.0, 0.0)) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(1.0, 0.0, 0.0)) == 2.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(0.0, 0.5, 0.0)) == 1.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(0.5, 0.5, 0.0)) == 2.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(1.0, 0.5, 0.0)) == 3.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(0.0, 1.0, 0.0)) == 3.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(0.5, 1.0, 0.0)) == 4.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(1.0, 1.0, 0.0)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(0.0, 0.0, 0.5)) == 3.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(0.5, 0.0, 0.5)) == 4.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(1.0, 0.0, 0.5)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(0.0, 0.5, 0.5)) == 4.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(0.5, 0.5, 0.5)) == 5.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(1.0, 0.5, 0.5)) == 6.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(0.0, 1.0, 0.5)) == 6.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(0.5, 1.0, 0.5)) == 7.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(1.0, 1.0, 0.5)) == 8.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(0.0, 0.0, 1.0)) == 6.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(0.5, 0.0, 1.0)) == 7.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(1.0, 0.0, 1.0)) == 8.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(0.0, 0.5, 1.0)) == 7.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(0.5, 0.5, 1.0)) == 8.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(1.0, 0.5, 1.0)) == 9.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(0.0, 1.0, 1.0)) == 9.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(0.5, 1.0, 1.0)) == 10.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(1.0, 1.0, 1.0)) == 11.0) stop(); }", __LINE__);

	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapColor('map', -5.0) == '#FF003F') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapColor('map', -2.5) == '#804020') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapColor('map', 0.0001) == '#007F00') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapColor('map', 2.5) == '#00BF80') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapColor('map', 5.0) == '#00FFFF') stop(); }", __LINE__);
}

#pragma mark Individual tests
void _RunIndividualTests(void)
{	
	// ************************************************************************************
	//
	//	Gen 1+ tests: Individual
	//
	
	// Test Individual properties
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; if (all(i.color == '')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; if (size(i.genome1) == 10) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; if (size(i.genome2) == 10) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; if (size(i.genomes) == 20) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; if (size(i.genomesNonNull) == 20) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; if (identical(i.genome1, i.genomes[0:9 * 2])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; if (identical(i.genome2, i.genomes[0:9 * 2 + 1])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; if (all(i.index == (0:9))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; if (all(i.subpopulation == rep(p1, 10))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; if (all(i.sex == rep('H', 10))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; i.color = 'red'; if (all(i.color == '#FF0000')) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { i = p1.individuals; i[0].tag; }", "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { i = p1.individuals; i.tag; }", "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; i.tag = 135; if (all(i.tag == 135)) stop(); }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { i = p1.individuals; i[0].tagF; }", "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { i = p1.individuals; i.tagF; }", "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; i.tagF = 135.0; if (all(i.tagF == 135.0)) stop(); }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { i = p1.individuals; i[0].tagL0; }", "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { i = p1.individuals; i.tagL0; }", "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; i.tagL0 = T; if (all(i.tagL0 == T)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; i.tagL0 = F; if (all(i.tagL0 == F)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; i.tagL0 = rep(c(T,F),5); if (sum(i.tagL0) == 5) stop(); }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { i = p1.individuals; i[0].tagL1; }", "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { i = p1.individuals; i.tagL1; }", "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; i.tagL1 = T; if (all(i.tagL1 == T)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; i.tagL1 = F; if (all(i.tagL1 == F)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; i.tagL1 = rep(c(T,F),5); if (sum(i.tagL1) == 5) stop(); }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { i = p1.individuals; i[0].tagL2; }", "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { i = p1.individuals; i.tagL2; }", "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; i.tagL2 = T; if (all(i.tagL2 == T)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; i.tagL2 = F; if (all(i.tagL2 == F)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; i.tagL2 = rep(c(T,F),5); if (sum(i.tagL2) == 5) stop(); }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { i = p1.individuals; i[0].tagL3; }", "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { i = p1.individuals; i.tagL3; }", "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; i.tagL3 = T; if (all(i.tagL3 == T)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; i.tagL3 = F; if (all(i.tagL3 == F)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; i.tagL3 = rep(c(T,F),5); if (sum(i.tagL3) == 5) stop(); }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { i = p1.individuals; i[0].tagL4; }", "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { i = p1.individuals; i.tagL4; }", "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; i.tagL4 = T; if (all(i.tagL4 == T)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; i.tagL4 = F; if (all(i.tagL4 == F)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; i.tagL4 = rep(c(T,F),5); if (sum(i.tagL4) == 5) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; if (size(i.migrant) == 10) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; if (all(i.migrant == F)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; i.fitnessScaling = 135.0; if (all(i.fitnessScaling == 135.0)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; i.fitnessScaling = 0.0; if (all(i.fitnessScaling == 0.0)) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { i = p1.individuals; i.fitnessScaling = -0.01; }", "must be >= 0.0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { i = p1.individuals; i.fitnessScaling = NAN; }", "must be >= 0.0", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; i.x = 135.0; if (all(i.x == 135.0)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; i.y = 135.0; if (all(i.y == 135.0)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; i.z = 135.0; if (all(i.z == 135.0)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 early() { i = p1.individuals; i.uniqueMutations; stop(); }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { i = p1.individuals; i.genome1 = i[0].genomes[0]; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { i = p1.individuals; i.genome2 = i[0].genomes[0]; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { i = p1.individuals; i.genomes = i[0].genomes[0]; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { i = p1.individuals; i.genomesNonNull = i[0].genomes[0]; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { i = p1.individuals; i.index = i[0].index; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { i = p1.individuals; i.subpopulation = i[0].subpopulation; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { i = p1.individuals; i.sex = i[0].sex; stop(); }", "read-only property", __LINE__);
	//SLiMAssertScriptRaise(gen1_setup_p1 + "10 early() { i = p1.individuals; i.uniqueMutations = sim.mutations[0]; stop(); }", "read-only property", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { i = p1.individuals; if (all(i.color == '')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { i = p1.individuals; if (size(i.genome1) == 10) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { i = p1.individuals; if (size(i.genome2) == 10) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { i = p1.individuals; if (size(i.genomes) == 20) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { i = p1.individuals; if (size(i.genomesNonNull) == 15) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { i = p1.individuals; if (identical(i.genome1, i.genomes[0:9 * 2])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { i = p1.individuals; if (identical(i.genome2, i.genomes[0:9 * 2 + 1])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { i = p1.individuals; if (all(i.index == (0:9))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { i = p1.individuals; if (all(i.subpopulation == rep(p1, 10))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { i = p1.individuals; if (all(i.sex == repEach(c('F','M'), 5))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { i = p1.individuals; i.color = 'red'; if (all(i.color == '#FF0000')) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 early() { i = p1.individuals; i[0].tag; }", "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 early() { i = p1.individuals; i.tag; }", "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { i = p1.individuals; i.tag = 135; if (all(i.tag == 135)) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 early() { i = p1.individuals; i[0].tagF; }", "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 early() { i = p1.individuals; i.tagF; }", "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { i = p1.individuals; i.tagF = 135.0; if (all(i.tagF == 135.0)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { i = p1.individuals; if (size(i.migrant) == 10) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { i = p1.individuals; if (all(i.migrant == F)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { i = p1.individuals; i.fitnessScaling = 135.0; if (all(i.fitnessScaling == 135.0)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { i = p1.individuals; i.fitnessScaling = 0.0; if (all(i.fitnessScaling == 0.0)) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 early() { i = p1.individuals; i.fitnessScaling = -0.01; }", "must be >= 0.0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 early() { i = p1.individuals; i.fitnessScaling = NAN; }", "must be >= 0.0", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { i = p1.individuals; i.x = 135.0; if (all(i.x == 135.0)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { i = p1.individuals; i.y = 135.0; if (all(i.y == 135.0)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 early() { i = p1.individuals; i.z = 135.0; if (all(i.z == 135.0)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "10 early() { i = p1.individuals; i.uniqueMutations; stop(); }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 early() { i = p1.individuals; i.genome1 = i[0].genomes[0]; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 early() { i = p1.individuals; i.genome2 = i[0].genomes[0]; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 early() { i = p1.individuals; i.genomes = i[0].genomes[0]; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 early() { i = p1.individuals; i.genomesNonNull = i[0].genomes[0]; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 early() { i = p1.individuals; i.index = i[0].index; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 early() { i = p1.individuals; i.subpopulation = i[0].subpopulation; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 early() { i = p1.individuals; i.sex = i[0].sex; stop(); }", "read-only property", __LINE__);
	//SLiMAssertScriptRaise(gen1_setup_sex_p1 + "10 early() { i = p1.individuals; i.uniqueMutations = sim.mutations[0]; stop(); }", "read-only property", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { i = p1.individuals; i.x = 0.5; if (identical(i.spatialPosition, rep(0.5, 10))) stop(); }", "position cannot be accessed", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 early() { i = p1.individuals; i.x = 0.5; if (identical(i.spatialPosition, rep(0.5, 10))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xy + "1 early() { i = p1.individuals; i.x = 0.5; i.y = 0.6; if (identical(i.spatialPosition, rep(c(0.5, 0.6), 10))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 early() { i = p1.individuals; i.x = 0.5; i.y = 0.6; i.z = 0.7; if (identical(i.spatialPosition, rep(c(0.5, 0.6, 0.7), 10))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { i = p1.individuals; i.spatialPosition = 0.5; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 early() { i = p1.individuals; i.spatialPosition = 0.5; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xy + "1 early() { i = p1.individuals; i.spatialPosition = 0.5; stop(); }", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 early() { i = p1.individuals; i.spatialPosition = 0.5; stop(); }", "read-only property", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { i = p1.individuals; i.setSpatialPosition(0.5); stop(); }", "cannot be called in non-spatial simulations", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 early() { i = p1.individuals; i[0].setSpatialPosition(float(0)); }", "requires at least as many coordinates", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 early() { i = p1.individuals; i[0].setSpatialPosition(0.5); if (identical(i[0].spatialPosition, 0.5)) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 early() { i = p1.individuals; i[0].setSpatialPosition(c(0.5, 0.6)); }", "position parameter to contain", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 early() { i = p1.individuals; i.setSpatialPosition(float(0)); }", "requires at least as many coordinates", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 early() { i = p1.individuals; i.setSpatialPosition(0.5); if (identical(i.spatialPosition, rep(0.5, 10))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 early() { i = p1.individuals; i.setSpatialPosition(c(0.5, 0.6)); }", "position parameter to contain", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 early() { i = p1.individuals; i.setSpatialPosition((1:10) / 10.0); if (identical(i.spatialPosition, (1:10) / 10.0)) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xy + "1 early() { i = p1.individuals; i[0].setSpatialPosition(0.5); }", "requires at least as many coordinates", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xy + "1 early() { i = p1.individuals; i[0].setSpatialPosition(c(0.5, 0.6)); if (identical(i[0].spatialPosition, c(0.5, 0.6))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xy + "1 early() { i = p1.individuals; i[0].setSpatialPosition(c(0.5, 0.6, 0.7)); }", "position parameter to contain", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xy + "1 early() { i = p1.individuals; i.setSpatialPosition(0.5); }", "requires at least as many coordinates", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xy + "1 early() { i = p1.individuals; i.setSpatialPosition(c(0.5, 0.6)); if (identical(i.spatialPosition, rep(c(0.5, 0.6), 10))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xy + "1 early() { i = p1.individuals; i.setSpatialPosition(c(0.5, 0.6, 0.7)); }", "position parameter to contain", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xy + "1 early() { i = p1.individuals; i.setSpatialPosition(1.0:20); if (identical(i.spatialPosition, 1.0:20)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xy + "1 early() { i = p1.individuals; i.setSpatialPosition(1.0:20); if (identical(i.y, (1.0:10)*2)) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 early() { i = p1.individuals; i[0].setSpatialPosition(0.5); }", "requires at least as many coordinates", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 early() { i = p1.individuals; i[0].setSpatialPosition(c(0.5, 0.6, 0.7)); if (identical(i[0].spatialPosition, c(0.5, 0.6, 0.7))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 early() { i = p1.individuals; i[0].setSpatialPosition(c(0.5, 0.6, 0.7, 0.8)); }", "position parameter to contain", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 early() { i = p1.individuals; i.setSpatialPosition(0.5); }", "requires at least as many coordinates", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 early() { i = p1.individuals; i.setSpatialPosition(c(0.5, 0.6, 0.7)); if (identical(i.spatialPosition, rep(c(0.5, 0.6, 0.7), 10))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 early() { i = p1.individuals; i.setSpatialPosition(c(0.5, 0.6, 0.7, 0.8)); }", "position parameter to contain", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 early() { i = p1.individuals; i.setSpatialPosition(1.0:30); if (identical(i.spatialPosition, 1.0:30)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 early() { i = p1.individuals; i.setSpatialPosition(1.0:30); if (identical(i.z, (1.0:10)*3)) stop(); }", __LINE__);
	
	// Some specific testing for setting of accelerated properties
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; i.tag = (seqAlong(i) % 2 == 0); if (all(i.tag == (seqAlong(i) % 2 == 0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; i.tag = seqAlong(i); if (all(i.tag == seqAlong(i))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; i.tagF = (seqAlong(i) % 2 == 0); if (all(i.tagF == (seqAlong(i) % 2 == 0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; i.tagF = seqAlong(i); if (all(i.tagF == seqAlong(i))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; i.tagF = asFloat(seqAlong(i)); if (all(i.tagF == seqAlong(i))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; i.fitnessScaling = (seqAlong(i) % 2 == 0); if (all(i.fitnessScaling == (seqAlong(i) % 2 == 0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; i.fitnessScaling = seqAlong(i); if (all(i.fitnessScaling == seqAlong(i))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; i.fitnessScaling = asFloat(seqAlong(i)); if (all(i.fitnessScaling == seqAlong(i))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; i.x = (seqAlong(i) % 2 == 0); if (all(i.x == (seqAlong(i) % 2 == 0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; i.x = seqAlong(i); if (all(i.x == seqAlong(i))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; i.x = asFloat(seqAlong(i)); if (all(i.x == seqAlong(i))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; i.y = (seqAlong(i) % 2 == 0); if (all(i.y == (seqAlong(i) % 2 == 0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; i.y = seqAlong(i); if (all(i.y == seqAlong(i))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; i.y = asFloat(seqAlong(i)); if (all(i.y == seqAlong(i))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; i.z = (seqAlong(i) % 2 == 0); if (all(i.z == (seqAlong(i) % 2 == 0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; i.z = seqAlong(i); if (all(i.z == seqAlong(i))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; i.z = asFloat(seqAlong(i)); if (all(i.z == seqAlong(i))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals; i.color = format('#%.6X', seqAlong(i)); if (all(i.color == format('#%.6X', seqAlong(i)))) stop(); }", __LINE__);
	
	// Test Individual - (logical)containsMutations(object<Mutation> mutations)
	SLiMAssertScriptStop(gen1_setup_p1 + "10 early() { i = p1.individuals; i.containsMutations(object()); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 early() { i = p1.individuals; i.containsMutations(sim.mutations); stop(); }", __LINE__);
	
	// Test Individual - (integer$)countOfMutationsOfType(io<MutationType>$ mutType)
	SLiMAssertScriptStop(gen1_setup_p1 + "10 early() { i = p1.individuals; i.countOfMutationsOfType(m1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 early() { i = p1.individuals; i.countOfMutationsOfType(1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 early() { i = p1.individuals; i[0:1].countOfMutationsOfType(1); stop(); }", __LINE__);
	
	// Test Individual - (float$)sumOfMutationsOfType(io<MutationType>$ mutType)
	SLiMAssertScriptStop(gen1_setup_p1 + "10 early() { i = p1.individuals; i.sumOfMutationsOfType(m1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 early() { i = p1.individuals; i.sumOfMutationsOfType(1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 early() { i = p1.individuals; i[0:1].sumOfMutationsOfType(1); stop(); }", __LINE__);
	
	// Test Individual - (object<Mutation>)uniqueMutationsOfType(io<MutationType>$ mutType)
	SLiMAssertScriptStop(gen1_setup_p1 + "10 early() { i = p1.individuals; i.uniqueMutationsOfType(m1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 early() { i = p1.individuals; i.uniqueMutationsOfType(1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 early() { i = p1.individuals; i[0:1].uniqueMutationsOfType(1); stop(); }", __LINE__);
	/*
	 Positions are tested with identical() instead of the mutation vectors themselves, only because the sorted order of mutations
	 at exactly the same position may differ; identical(um1, um2) will occasionally flag these as false positives.
	 */
	SLiMAssertScriptSuccess(R"V0G0N(
		initialize() {
			initializeMutationRate(1e-5);
			initializeMutationType("m1", 0.5, "f", 0.0);
			initializeGenomicElementType("g1", m1, 1.0);
			initializeGenomicElement(g1, 0, 99999);
			initializeRecombinationRate(1e-8);
		}
		1 early() {
			sim.addSubpop("p1", 500);
		}
		1:200 late() {
			for (i in p1.individuals) {
				um1 = i.uniqueMutations;
				um2 = sortBy(unique(i.genomes.mutations), "position");
				if (!identical(um1.position, um2.position))
					stop("Mismatch!");
			}
		})V0G0N");
	
	// Test optional pedigree stuff; note that relatedness can be higher than 0.5 due to inbreeding, even if preventIncidentalSelfing=T were set
	// see the model test_relatedness.slim (not in the GitHub repo) for more precise tests that relatedness() does the right calculations
	std::string gen1_setup_norel("initialize() { initializeSLiMOptions(keepPedigrees=F); initializeMutationRate(1e-7); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); } 1 early() { sim.addSubpop('p1', 10); } ");
	std::string gen1_setup_rel("initialize() { initializeSLiMOptions(keepPedigrees=T); initializeMutationRate(1e-7); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); } 1 early() { sim.addSubpop('p1', 10); } ");
	std::string gen1_setup_rel_S("initialize() { initializeSLiMOptions(keepPedigrees=T); initializeMutationRate(1e-7); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeSex('A'); } 1 early() { sim.addSubpop('p1', 10); } ");
	
	SLiMAssertScriptRaise(gen1_setup_norel + "5 early() { if (all(p1.individuals.pedigreeID == -1)) stop(); }", "is not available", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_norel + "5 early() { if (all(p1.individuals.pedigreeParentIDs == -1)) stop(); }", "has not been enabled", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_norel + "5 early() { if (all(p1.individuals.pedigreeGrandparentIDs == -1)) stop(); }", "has not been enabled", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_norel + "5 early() { if (all(p1.individuals.reproductiveOutput == 0)) stop(); }", "has not been enabled", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_norel + "5 early() { if (p1.individuals[0].reproductiveOutput == 0) stop(); }", "has not been enabled", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_norel + "5 early() { if (mean(p1.lifetimeReproductiveOutput) > 0) stop(); }", "has not been enabled", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_norel + "5 early() { if (mean(p1.lifetimeReproductiveOutputM) > 0) stop(); }", "has not been enabled", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_norel + "5 early() { if (mean(p1.lifetimeReproductiveOutputF) > 0) stop(); }", "has not been enabled", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_rel + "5 early() { if (mean(p1.lifetimeReproductiveOutputM) > 0) stop(); }", "separate sexes are not enabled", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_rel + "5 early() { if (mean(p1.lifetimeReproductiveOutputF) > 0) stop(); }", "separate sexes are not enabled", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_norel + "5 early() { if (all(p1.individuals.genomes.genomePedigreeID == -1)) stop(); }", "is not available", __LINE__);
	SLiMAssertScriptStop(gen1_setup_norel + "5 early() { if (p1.individuals[0].relatedness(p1.individuals[0]) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_norel + "5 early() { if (p1.individuals[0].sharedParentCount(p1.individuals[0]) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_norel + "5 early() { if (p1.individuals[0].relatedness(p1.individuals[1]) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_norel + "5 early() { if (p1.individuals[0].sharedParentCount(p1.individuals[1]) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_norel + "5 early() { if (all(p1.individuals[0].relatedness(p1.individuals[1:9]) == 0.0)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_norel + "5 early() { if (all(p1.individuals[0].sharedParentCount(p1.individuals[1:9]) == 0)) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_rel + "5 early() { if (all(p1.individuals.pedigreeID != -1)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_rel + "5 early() { if (all(p1.individuals.pedigreeParentIDs != -1)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_rel + "5 early() { if (all(p1.individuals.pedigreeGrandparentIDs != -1)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_rel + "5 early() { if (all(p1.individuals.reproductiveOutput == 0)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_rel + "5 early() { if (p1.individuals[0].reproductiveOutput == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_rel + "5 early() { if (mean(p1.lifetimeReproductiveOutput) > 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_rel_S + "5 early() { if (mean(p1.lifetimeReproductiveOutput) > 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_rel_S + "5 early() { if (mean(p1.lifetimeReproductiveOutputM) > 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_rel_S + "5 early() { if (mean(p1.lifetimeReproductiveOutputF) > 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_rel + "5 early() { if (all(p1.individuals.genomes.genomePedigreeID != -1)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_rel + "5 early() { if (p1.individuals[0].relatedness(p1.individuals[0]) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_rel + "5 early() { if (p1.individuals[0].sharedParentCount(p1.individuals[0]) == 2) stop(); }", __LINE__);
	// In certain inbreeding scenarios, which can happen by chance, relatedness of individuals can be 1.0 (maybe even higher?) so these tests are no good
	//SLiMAssertScriptStop(gen1_setup_rel + "5 early() { if (p1.individuals[0].relatedness(p1.individuals[1]) < 1.0) stop(); }", __LINE__);
	//SLiMAssertScriptStop(gen1_setup_rel + "5 early() { if (all(p1.individuals[0].relatedness(p1.individuals[1:9]) < 1.0)) stop(); }", __LINE__);
	
	// Test Individual EidosDictionaryUnretained functionality: - (+)getValue(is$ key) and - (void)setValue(is$ key, + value)
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals[0]; i.setValue('foo', 7:9); i.setValue('bar', 'baz'); if (identical(i.getValue('foo'), 7:9) & identical(i.getValue('bar'), 'baz')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals[0]; i.setValue('foo', 3:5); i.setValue('foo', 'foobar'); if (identical(i.getValue('foo'), 'foobar')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { i = p1.individuals[0]; i.setValue('foo', 3:5); i.setValue('foo', NULL); if (isNULL(i.getValue('foo'))) stop(); }", __LINE__);
}

#pragma mark SLiMEidosBlock tests
void _RunSLiMEidosBlockTests(void)
{
	// ************************************************************************************
	//
	//	Gen 1+ tests: SLiMEidosBlock
	//
	
	// Test SLiMEidosBlock properties
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (s1.active == -1) stop(); } s1 2:4 early() { sim = 10; } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (s1.end == 4) stop(); } s1 2:4 early() { sim = 10; } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (s1.id == 1) stop(); } s1 2:4 early() { sim = 10; } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (s1.source == '{ sim = 10; }') stop(); } s1 2:4 early() { sim = 10; } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (s1.start == 2) stop(); } s1 2:4 early() { sim = 10; } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (s1.type == 'early') stop(); } s1 2:4 early() { sim = 10; } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (s1.type == 'early') stop(); } s1 2:4 early() { sim = 10; } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { if (s1.type == 'late') stop(); } s1 2:4 late() { sim = 10; } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { s1.active = 198; if (s1.active == 198) stop(); } s1 2:4 early() { sim = 10; } ", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { s1.end = 4; stop(); } s1 2:4 early() { sim = 10; } ", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { s1.id = 1; stop(); } s1 2:4 early() { sim = 10; } ", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { s1.source = '{ sim = 10; }'; stop(); } s1 2:4 early() { sim = 10; } ", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { s1.start = 2; stop(); } s1 2:4 early() { sim = 10; } ", "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { s1.tag; } s1 2:4 early() { sim = 10; } ", "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { c(s1,s1).tag; } s1 2:4 early() { sim = 10; } ", "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 early() { s1.tag = 219; if (s1.tag == 219) stop(); } s1 2:4 early() { sim = 10; } ", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 early() { s1.type = 'event'; stop(); } s1 2:4 early() { sim = 10; } ", "read-only property", __LINE__);
	
	// No methods on SLiMEidosBlock
	
	// Test user-defined functions in SLiM; there is a huge amount more that could be tested, but these get tested by EidosScribe too,
	// so mostly we just need to make sure here that they get declared and defined properly in SLiM, and are callable.
	SLiMAssertScriptStop(gen1_setup_p1 + "function (i)A(i x) {return x*2;} 1 early() { if (A(2) == 4) stop(); } 10 early() {  } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "function (i)A(i x) {return B(x)+1;} function (i)B(i x) {return x*2;} 1 early() { if (A(2) == 5) stop(); } 10 early() {  } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "function (i)fac([i b=10]) { if (b <= 1) return 1; else return b*fac(b-1); } 1 early() { if (fac(5) == 120) stop(); } 10 early() {  } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "function (i)spsize(o<Subpopulation>$ sp) { return sp.individualCount; } 2 early() { if (spsize(p1) == 10) stop(); } 10 early() {  } ", __LINE__);
	
	// Test callbacks; we don't attempt to test their functionality here, just their declaration and the fact that they get called
	// Their actual functionality gets tested by the R test suite and the recipes; we want to probe error cases here, more
	// Things to be careful of: declaration syntax, return value types, special optimized cases, pseudo-parameter definitions
	
	// fitnessEffect() callbacks
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "fitnessEffect() { return 1.0; } 100 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "fitnessEffect() { stop(); } 100 early() { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "fitnessEffect(p1) { return 1.0; } 100 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "fitnessEffect(p1) { stop(); } 100 early() { ; }", __LINE__);

	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "fitnessEffect(p4) { stop(); } 100 early() { ; }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "early() { s1.active = 0; } s1 fitnessEffect(p1) { stop(); } 100 early() { ; }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "fitnessEffect(m1) { stop(); } 100 early() { ; }", "identifier prefix 'p' was expected", __LINE__);
	
	// mutationEffect() callbacks
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mutationEffect(m1) { return effect; } 100 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mutationEffect(m1) { stop(); } 100 early() { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mutationEffect(m1, p1) { return effect; } 100 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mutationEffect(m1, p1) { stop(); } 100 early() { ; }", __LINE__);
	
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "mutationEffect(m2) { stop(); } 100 early() { ; }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "mutationEffect(m2, p1) { stop(); } 100 early() { ; }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "mutationEffect(m1, p4) { stop(); } 100 early() { ; }", __LINE__);
	
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "early() { s1.active = 0; } s1 mutationEffect(m1) { stop(); } 100 early() { ; }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "early() { s1.active = 0; } s1 mutationEffect(m1, p1) { stop(); } 100 early() { ; }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutationEffect() { stop(); } 100 early() { ; }", "mutation type id is required", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutationEffect(m1, p1, p2) { stop(); } 100 early() { ; }", "unexpected token", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutationEffect(m1, m1) { stop(); } 100 early() { ; }", "identifier prefix 'p' was expected", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutationEffect(p1) { stop(); } 100 early() { ; }", "identifier prefix 'm' was expected", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutationEffect(m1, NULL) { stop(); } 100 early() { ; }", "identifier prefix 'p' was expected", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutationEffect(m1) { ; } 100 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutationEffect(m1) { return NULL; } 100 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutationEffect(m1) { return F; } 100 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutationEffect(m1) { return T; } 100 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutationEffect(m1) { return 1; } 100 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutationEffect(m1) { return 'a'; } 100 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutationEffect(m1) { return mut; } 100 early() { ; }", "return value", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutationEffect(m1) { mut; ; } 100 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutationEffect(m1) { mut; return NULL; } 100 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutationEffect(m1) { mut; return F; } 100 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutationEffect(m1) { mut; return T; } 100 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutationEffect(m1) { mut; return 1; } 100 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutationEffect(m1) { mut; return 'a'; } 100 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutationEffect(m1) { mut; return mut; } 100 early() { ; }", "return value", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mutationEffect(m1) { mut; homozygous; individual; subpop; return effect; } 100 early() { stop(); }", __LINE__);
	
	// mateChoice() callbacks
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mateChoice() { return weights; } 10 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mateChoice() { stop(); } 10 early() { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mateChoice(p1) { return weights; } 10 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mateChoice(p1) { stop(); } 10 early() { ; }", __LINE__);
	
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "mateChoice(p4) { stop(); } 10 early() { ; }", __LINE__);
	
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "early() { s1.active = 0; } s1 mateChoice(p1) { stop(); } 10 early() { ; }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(m1) { stop(); } 10 early() { ; }", "identifier prefix 'p' was expected", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(p1, p1) { stop(); } 10 early() { ; }", "unexpected token", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(NULL) { stop(); } 10 early() { ; }", "identifier prefix 'p' was expected", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(p1) { ; } 10 early() { ; }", "must explicitly return a value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(p1) { return F; } 10 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(p1) { return T; } 10 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(p1) { return 1; } 10 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(p1) { return 1.0; } 10 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(p1) { return 'a'; } 10 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(p1) { return individual.genome1; } 10 early() { ; }", "return value", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(p1) { subpop; ; } 10 early() { ; }", "must explicitly return a value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(p1) { subpop; return F; } 10 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(p1) { subpop; return T; } 10 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(p1) { subpop; return 1; } 10 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(p1) { subpop; return 1.0; } 10 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(p1) { subpop; return 'a'; } 10 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(p1) { subpop; return individual.genome1; } 10 early() { ; }", "return value", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mateChoice(p1) { individual; subpop; sourceSubpop; return weights; } 10 early() { stop(); }", __LINE__);
	
	// modifyChild() callbacks
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "modifyChild() { return T; } 10 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "modifyChild() { stop(); } 10 early() { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "modifyChild(p1) { return T; } 10 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "modifyChild(p1) { stop(); } 10 early() { ; }", __LINE__);
	
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "modifyChild(p4) { stop(); } 10 early() { ; }", __LINE__);
	
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "early() { s1.active = 0; } s1 modifyChild(p1) { stop(); } 10 early() { ; }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "modifyChild(m1) { stop(); } 10 early() { ; }", "identifier prefix 'p' was expected", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "modifyChild(p1, p1) { stop(); } 10 early() { ; }", "unexpected token", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "modifyChild(NULL) { stop(); } 10 early() { ; }", "identifier prefix 'p' was expected", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "modifyChild(p1) { ; } 10 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "modifyChild(p1) { return NULL; } 10 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "modifyChild(p1) { return 1; } 10 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "modifyChild(p1) { return 1.0; } 10 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "modifyChild(p1) { return 'a'; } 10 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "modifyChild(p1) { return child; } 10 early() { ; }", "return value", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "modifyChild(p1) { subpop; ; } 10 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "modifyChild(p1) { subpop; return NULL; } 10 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "modifyChild(p1) { subpop; return 1; } 10 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "modifyChild(p1) { subpop; return 1.0; } 10 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "modifyChild(p1) { subpop; return 'a'; } 10 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "modifyChild(p1) { subpop; return child; } 10 early() { ; }", "return value", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "modifyChild(p1) { child; parent1; isCloning; isSelfing; parent2; subpop; sourceSubpop; return T; } 10 early() { stop(); }", __LINE__);
	
	// recombination() callbacks
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "recombination() { return F; } 10 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "recombination() { return T; } 10 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "recombination() { stop(); } 10 early() { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "recombination(p1) { return F; } 10 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "recombination(p1) { return T; } 10 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "recombination(p1) { stop(); } 10 early() { ; }", __LINE__);
	
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "recombination(p4) { stop(); } 10 early() { ; }", __LINE__);
	
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "early() { s1.active = 0; } s1 recombination(p1) { stop(); } 10 early() { ; }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "recombination(m1) { stop(); } 10 early() { ; }", "identifier prefix 'p' was expected", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "recombination(p1, p1) { stop(); } 10 early() { ; }", "unexpected token", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "recombination(NULL) { stop(); } 10 early() { ; }", "identifier prefix 'p' was expected", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "recombination(p1) { ; } 10 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "recombination(p1) { return NULL; } 10 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "recombination(p1) { return 1; } 10 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "recombination(p1) { return 1.0; } 10 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "recombination(p1) { return 'a'; } 10 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "recombination(p1) { return subpop; } 10 early() { ; }", "return value", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "recombination(p1) { subpop; ; } 10 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "recombination(p1) { subpop; return NULL; } 10 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "recombination(p1) { subpop; return 1; } 10 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "recombination(p1) { subpop; return 1.0; } 10 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "recombination(p1) { subpop; return 'a'; } 10 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "recombination(p1) { subpop; return subpop; } 10 early() { ; }", "return value", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "recombination(p1) { individual; genome1; genome2; subpop; breakpoints; return T; } 10 early() { stop(); }", __LINE__);
	
	// interaction() callbacks
	static std::string gen1_setup_p1p2p3_i1(gen1_setup_p1p2p3 + "initialize() { initializeInteractionType('i1', ''); } early() { i1.evaluate(sim.subpopulations); i1.strength(p1.individuals[0]); } ");
	
	SLiMAssertScriptStop(gen1_setup_p1p2p3_i1 + "interaction(i1) { return 1.0; } 10 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3_i1 + "interaction(i1) { stop(); } 10 early() { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3_i1 + "interaction(i1, p1) { return 1.0; } 10 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3_i1 + "interaction(i1, p1) { stop(); } 10 early() { ; }", __LINE__);
	
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3_i1 + "interaction(i2) { stop(); } 10 early() { ; }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3_i1 + "interaction(i2, p1) { stop(); } 10 early() { ; }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3_i1 + "interaction(i1, p4) { stop(); } 10 early() { ; }", __LINE__);
	
	SLiMAssertScriptSuccess("early() { s1.active = 0; } " + gen1_setup_p1p2p3_i1 + "s1 interaction(i1) { stop(); } 10 early() { ; }", __LINE__);
	SLiMAssertScriptSuccess("early() { s1.active = 0; } " + gen1_setup_p1p2p3_i1 + "s1 interaction(i1, p1) { stop(); } 10 early() { ; }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction() { stop(); } 10 early() { ; }", "interaction type id is required", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(i1, p1, p2) { stop(); } 10 early() { ; }", "unexpected token", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(i1, i1) { stop(); } 10 early() { ; }", "identifier prefix 'p' was expected", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(p1) { stop(); } 10 early() { ; }", "identifier prefix 'i' was expected", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(i1, NULL) { stop(); } 10 early() { ; }", "identifier prefix 'p' was expected", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(NULL, i1) { stop(); } 10 early() { ; }", "identifier prefix 'i' was expected", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(i1) { ; } 10 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(i1) { return NULL; } 10 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(i1) { return F; } 10 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(i1) { return T; } 10 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(i1) { return 1; } 10 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(i1) { return 'a'; } 10 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(i1) { return exerter; } 10 early() { ; }", "return value", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(i1) { exerter; ; } 10 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(i1) { exerter; return F; } 10 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(i1) { exerter; return T; } 10 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(i1) { exerter; return 1; } 10 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(i1) { exerter; return 'a'; } 10 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(i1) { exerter; return exerter; } 10 early() { ; }", "return value", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_p1p2p3_i1 + "interaction(i1) { distance; strength; receiver; exerter; return 1.0; } 10 early() { stop(); }", __LINE__);
	
	// reproduction() callbacks
	static std::string gen1_setup_p1p2p3_nonWF(nonWF_prefix + gen1_setup_sex_p1 + "1 early() { sim.addSubpop('p2', 10); sim.addSubpop('p3', 10); } " + "late() { sim.subpopulations.individuals.fitnessScaling = 0.0; } ");
	
	SLiMAssertScriptStop(gen1_setup_p1p2p3_nonWF + "reproduction() { subpop.addCloned(individual); } 10 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3_nonWF + "reproduction() { stop(); } 10 early() { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { subpop.addCloned(individual); } 10 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { stop(); } 10 early() { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3_nonWF + "reproduction(p1, 'F') { subpop.addCloned(individual); } 10 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3_nonWF + "reproduction(p1, 'F') { stop(); } 10 early() { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3_nonWF + "reproduction(NULL, 'F') { subpop.addCloned(individual); } 10 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3_nonWF + "reproduction(NULL, 'F') { stop(); } 10 early() { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3_nonWF + "reproduction(p1, NULL) { subpop.addCloned(individual); } 10 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3_nonWF + "reproduction(p1, NULL) { stop(); } 10 early() { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3_nonWF + "reproduction(NULL, NULL) { subpop.addCloned(individual); } 10 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3_nonWF + "reproduction(NULL, NULL) { stop(); } 10 early() { ; }", __LINE__);
	
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3_nonWF + "reproduction(p4) { stop(); } 10 early() { ; }", __LINE__);
	
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3_nonWF + "reproduction() { s1.active = 0; } s1 reproduction(p1) { stop(); } 10 early() { ; }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(m1) { stop(); } 10 early() { ; }", "identifier prefix 'p' was expected", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(p1, p1) { stop(); } 10 early() { ; }", "needs a value for sex", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(NULL, '*') { stop(); } 10 early() { ; }", "needs a value for sex", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { return NULL; } 10 early() { ; }", "must return void", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { return F; } 10 early() { ; }", "must return void", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { return T; } 10 early() { ; }", "must return void", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { return 1; } 10 early() { ; }", "must return void", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { return 1.0; } 10 early() { ; }", "must return void", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { return 'a'; } 10 early() { ; }", "must return void", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { return subpop; } 10 early() { ; }", "must return void", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { subpop; return NULL; } 10 early() { ; }", "must return void", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { subpop; return F; } 10 early() { ; }", "must return void", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { subpop; return T; } 10 early() { ; }", "must return void", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { subpop; return 1; } 10 early() { ; }", "must return void", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { subpop; return 1.0; } 10 early() { ; }", "must return void", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { subpop; return 'a'; } 10 early() { ; }", "must return void", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { subpop; return subpop; } 10 early() { ; }", "must return void", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { individual; subpop; subpop.addCloned(individual); } 10 early() { stop(); }", __LINE__);
	
	// mutation() callbacks
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mutation(m1) { return T; } 100 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mutation(m1) { return mut; } 100 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mutation(m1) { stop(); } 100 early() { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mutation() { return T; } 100 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mutation() { return mut; } 100 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mutation() { stop(); } 100 early() { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mutation(NULL) { return T; } 100 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mutation(NULL) { return mut; } 100 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mutation(NULL) { stop(); } 100 early() { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mutation(m1, p1) { return T; } 100 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mutation(m1, p1) { return mut; } 100 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mutation(m1, p1) { stop(); } 100 early() { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mutation(NULL, p1) { return T; } 100 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mutation(NULL, p1) { return mut; } 100 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mutation(NULL, p1) { stop(); } 100 early() { ; }", __LINE__);
	
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "mutation(m2) { stop(); } 100 early() { ; }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "mutation(m2, p1) { stop(); } 100 early() { ; }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "mutation(m1, p4) { stop(); } 100 early() { ; }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "mutation(NULL, p4) { stop(); } 100 early() { ; }", __LINE__);
	
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "early() { s1.active = 0; } s1 mutation(m1) { stop(); } 100 early() { ; }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "early() { s1.active = 0; } s1 mutation(m1, p1) { stop(); } 100 early() { ; }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "early() { s1.active = 0; } s1 mutation(NULL, p1) { stop(); } 100 early() { ; }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutation(m1, p1, p2) { stop(); } 100 early() { ; }", "unexpected token", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutation(m1, m1) { stop(); } 100 early() { ; }", "identifier prefix 'p' was expected", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutation(p1) { stop(); } 100 early() { ; }", "identifier prefix 'm' was expected", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutation(m1, NULL) { stop(); } 100 early() { ; }", "identifier prefix 'p' was expected", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutation(NULL, m1) { stop(); } 100 early() { ; }", "identifier prefix 'p' was expected", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutation(m1) { ; } 100 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutation(m1) { return NULL; } 100 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutation(m1) { return 1; } 100 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutation(m1) { return 1.0; } 100 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutation(m1) { return 'a'; } 100 early() { ; }", "return value", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutation(m1) { mut; ; } 100 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutation(m1) { mut; return NULL; } 100 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutation(m1) { mut; return 1; } 100 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutation(m1) { mut; return 1.0; } 100 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutation(m1) { mut; return 'a'; } 100 early() { ; }", "return value", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mutation(m1) { mut; genome; element; originalNuc; parent; subpop; return T; } 100 early() { stop(); }", __LINE__);
	
	// survival() callbacks
	static std::string gen1_setup_p1p2p3_nonWF_clonal(gen1_setup_p1p2p3_nonWF + "reproduction() { subpop.addCloned(individual); } ");
	
	SLiMAssertScriptStop(gen1_setup_p1p2p3_nonWF_clonal + "survival() { return F; } 10 early() { if (p1.individualCount == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3_nonWF_clonal + "survival() { return T; } 10 early() { if (p1.individualCount == 5120) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3_nonWF_clonal + "survival() { return NULL; } 10 early() { stop(); }", __LINE__);
	//SLiMAssertScriptStop(gen1_setup_p1p2p3_nonWF_clonal + "survival() { if (subpop == p1) return p2; return NULL; } 10 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3_nonWF_clonal + "survival() { stop(); } 10 early() { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3_nonWF_clonal + "survival(p1) { return F; } 10 early() { if (p1.individualCount == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3_nonWF_clonal + "survival(p1) { return T; } 10 early() { if (p1.individualCount == 5120) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3_nonWF_clonal + "survival(p1) { return NULL; } 10 early() { stop(); }", __LINE__);
	//SLiMAssertScriptStop(gen1_setup_p1p2p3_nonWF_clonal + "survival(p1) { return p2; } 10 early() { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3_nonWF_clonal + "survival(p1) { stop(); } 10 early() { ; }", __LINE__);
	
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3_nonWF_clonal + "survival(p4) { stop(); } 10 early() { ; }", __LINE__);
	
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3_nonWF_clonal + "early() { s1.active = 0; } s1 survival(p1) { stop(); } 10 early() { ; }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF_clonal + "survival(m1) { stop(); } 10 early() { ; }", "identifier prefix 'p' was expected", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF_clonal + "survival(p1, p1) { stop(); } 10 early() { ; }", "unexpected token", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF_clonal + "survival(NULL) { stop(); } 10 early() { ; }", "identifier prefix 'p' was expected", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF_clonal + "survival(p1) { ; } 10 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF_clonal + "survival(p1) { return 1; } 10 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF_clonal + "survival(p1) { return 1.0; } 10 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF_clonal + "survival(p1) { return 'a'; } 10 early() { ; }", "return value", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF_clonal + "survival(p1) { subpop; ; } 10 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF_clonal + "survival(p1) { subpop; return 1; } 10 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF_clonal + "survival(p1) { subpop; return 1.0; } 10 early() { ; }", "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF_clonal + "survival(p1) { subpop; return 'a'; } 10 early() { ; }", "return value", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_p1p2p3_nonWF_clonal + "survival(p1) { individual; subpop; fitness; draw; return T; } 10 early() { stop(); }", __LINE__);
}





























