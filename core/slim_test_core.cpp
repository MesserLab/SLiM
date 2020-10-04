//
//  slim_test_core.cpp
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


#pragma mark initialize() tests
void _RunInitTests(void)
{	
	// ************************************************************************************
	//
	//	Initialization function tests
	//
	
	// Test (void)initializeGeneConversion(numeric$ conversionFraction, numeric$ meanLength)
	SLiMAssertScriptStop("initialize() { initializeGeneConversion(0.5, 10000000000000, 0.0); stop(); }", __LINE__);										// legal; no max for meanLength
	SLiMAssertScriptRaise("initialize() { initializeGeneConversion(-0.001, 10000000000000, 0.0); stop(); }", 1, 15, "nonCrossoverFraction must be between 0.0 and 1.0", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeGeneConversion(1.001, 10000000000000, 0.0); stop(); }", 1, 15, "nonCrossoverFraction must be between 0.0 and 1.0", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeGeneConversion(0.5, -0.01, 0.0); stop(); }", 1, 15, "meanLength must be >= 0.0", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeGeneConversion(0.5, 1000, -0.001); stop(); }", 1, 15, "simpleConversionFraction must be between 0.0 and 1.0", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeGeneConversion(0.5, 1000, 1.001); stop(); }", 1, 15, "simpleConversionFraction must be between 0.0 and 1.0", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeGeneConversion(0.5, 1000, 0.0, -1.001); stop(); }", 1, 15, "bias must be between -1.0 and 1.0", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeGeneConversion(0.5, 1000, 0.0, 1.001); stop(); }", 1, 15, "bias must be between -1.0 and 1.0", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeGeneConversion(0.5, 1000, 0.0, 0.1); stop(); }", 1, 15, "must be 0.0 in non-nucleotide-based models", __LINE__);
	
	// Test (object<MutationType>$)initializeMutationType(is$ id, numeric$ dominanceCoeff, string$ distributionType, ...)
	SLiMAssertScriptStop("initialize() { initializeMutationType('m1', 0.5, 'f', 0.0); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeMutationType(1, 0.5, 'f', 0.0); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType(-1, 0.5, 'f', 0.0); stop(); }", 1, 15, "identifier value is out of range", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('p2', 0.5, 'f', 0.0); stop(); }", 1, 15, "identifier prefix \"m\" was expected", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('mm1', 0.5, 'f', 0.0); stop(); }", 1, 15, "must be a simple integer", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'f'); stop(); }", 1, 15, "requires exactly 1 DFE parameter", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'f', 0.0, 0.0); stop(); }", 1, 15, "requires exactly 1 DFE parameter", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'g', 0.0); stop(); }", 1, 15, "requires exactly 2 DFE parameters", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'e', 0.0, 0.0); stop(); }", 1, 15, "requires exactly 1 DFE parameter", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'n', 0.0); stop(); }", 1, 15, "requires exactly 2 DFE parameters", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'w', 0.0); stop(); }", 1, 15, "requires exactly 2 DFE parameters", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'f', 'foo'); stop(); }", 1, 15, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'g', 'foo', 0.0); stop(); }", 1, 15, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'g', 0.0, 'foo'); stop(); }", 1, 15, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'e', 'foo'); stop(); }", 1, 15, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'n', 'foo', 0.0); stop(); }", 1, 15, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'n', 0.0, 'foo'); stop(); }", 1, 15, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'w', 'foo', 0.0); stop(); }", 1, 15, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'w', 0.0, 'foo'); stop(); }", 1, 15, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'f', '1'); stop(); }", 1, 15, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'g', '1', 0.0); stop(); }", 1, 15, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'g', 0.0, '1'); stop(); }", 1, 15, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'e', '1'); stop(); }", 1, 15, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'n', '1', 0.0); stop(); }", 1, 15, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'n', 0.0, '1'); stop(); }", 1, 15, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'w', '1', 0.0); stop(); }", 1, 15, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'w', 0.0, '1'); stop(); }", 1, 15, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'x', 0.0); stop(); }", 1, 15, "must be \"f\", \"g\", \"e\", \"n\", \"w\", or \"s\"", __LINE__);
	SLiMAssertScriptStop("initialize() { x = initializeMutationType('m7', 0.5, 'f', 0.0); if (x == m7) stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { x = initializeMutationType(7, 0.5, 'f', 0.0); if (x == m7) stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { m7 = 15; initializeMutationType(7, 0.5, 'f', 0.0); stop(); }", 1, 24, "already defined", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'f', 0.0); initializeMutationType('m1', 0.5, 'f', 0.0); stop(); }", 1, 60, "already defined", __LINE__);
	
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'g', 3.1, 0.0); stop(); }", 1, 15, "must have a shape parameter > 0", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'g', 3.1, -1.0); stop(); }", 1, 15, "must have a shape parameter > 0", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'n', 3.1, -1.0); stop(); }", 1, 15, "must have a standard deviation parameter >= 0", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'w', 0.0, 7.5); stop(); }", 1, 15, "must have a scale parameter > 0", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'w', -1.0, 7.5); stop(); }", 1, 15, "must have a scale parameter > 0", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'w', 3.1, 0.0); stop(); }", 1, 15, "must have a shape parameter > 0", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'w', 3.1, -7.5); stop(); }", 1, 15, "must have a shape parameter > 0", __LINE__);
	
	// Test (object<GenomicElementType>$)initializeGenomicElementType(is$ id, io<MutationType> mutationTypes, numeric proportions)
	std::string define_m12(" initializeMutationType('m1', 0.5, 'f', 0.0); initializeMutationType('m2', 0.5, 'f', 0.5); ");
	
	SLiMAssertScriptStop("initialize() {" + define_m12 + "initializeGenomicElementType('g1', object(), integer(0)); stop(); }", __LINE__);			// legal: genomic element with no mutations
	SLiMAssertScriptStop("initialize() {" + define_m12 + "initializeGenomicElementType('g1', integer(0), float(0)); stop(); }", __LINE__);			// legal: genomic element with no mutations
	SLiMAssertScriptStop("initialize() {" + define_m12 + "initializeGenomicElementType('g1', c(m1,m2), c(0,0)); stop(); }", __LINE__);				// legal: genomic element with all zero proportions (must be fixed later...)
	SLiMAssertScriptStop("initialize() {" + define_m12 + "initializeGenomicElementType('g1', c(m1,m2), 1:2); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() {" + define_m12 + "initializeGenomicElementType(1, c(m1,m2), 1:2); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() {" + define_m12 + "initializeGenomicElementType('g1', 1:2, 1:2); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_m12 + "initializeGenomicElementType('g1', c(m1,m2)); stop(); }", 1, 105, "missing required argument", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_m12 + "initializeGenomicElementType('g1', c(m1,m2), 1); stop(); }", 1, 105, "requires the sizes", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_m12 + "initializeGenomicElementType('g1', c(m1,m2), c(-1,2)); stop(); }", 1, 105, "must be greater than or equal to zero", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_m12 + "initializeGenomicElementType('g1', 2:3, 1:2); stop(); }", 1, 105, "not defined", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_m12 + "initializeGenomicElementType('g1', c(2,2), 1:2); stop(); }", 1, 105, "used more than once", __LINE__);
	SLiMAssertScriptStop("initialize() {" + define_m12 + "x = initializeGenomicElementType('g7', c(m1,m2), 1:2); if (x == g7) stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() {" + define_m12 + "x = initializeGenomicElementType(7, c(m1,m2), 1:2); if (x == g7) stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_m12 + "g7 = 17; initializeGenomicElementType(7, c(m1,m2), 1:2); stop(); }", 1, 114, "already defined", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_m12 + "initializeGenomicElementType('g1', c(m1,m2), 1:2); initializeGenomicElementType('g1', c(m1,m2), c(0,0)); stop(); }", 1, 156, "already defined", __LINE__);
	
	// Test (void)initializeGenomicElement(io<GenomicElementType>$ genomicElementType, integer$ start, integer$ end)
	std::string define_g1(define_m12 + " initializeGenomicElementType('g1', c(m1,m2), 1:2); ");
	
	SLiMAssertScriptStop("initialize() {" + define_g1 + "initializeGenomicElement(g1, 0, 1000000000); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() {" + define_g1 + "initializeGenomicElement(1, 0, 1000000000); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_g1 + "initializeGenomicElement(g1, 0); stop(); }", 1, 157, "missing required argument", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_g1 + "initializeGenomicElement(2, 0, 1000000000); stop(); }", 1, 157, "not defined", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_g1 + "initializeGenomicElement(g1, -1, 1000000000); stop(); }", 1, 157, "out of range", __LINE__);
	//SLiMAssertScriptRaise("initialize() {" + define_g1 + "initializeGenomicElement(g1, 0, 1000000001); stop(); }", 1, 157, "out of range", __LINE__);		// now legal!
	SLiMAssertScriptStop("initialize() {" + define_g1 + "initializeGenomicElement(g1, 0, 1000000000000000); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_g1 + "initializeGenomicElement(g1, 0, 1000000000000001); stop(); }", 1, 157, "out of range", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_g1 + "initializeGenomicElement(g1, 100, 99); stop(); }", 1, 157, "is less than start position", __LINE__);
	
	// Test (void)initializeMutationRate(numeric$ rate)
	SLiMAssertScriptStop("initialize() { initializeMutationRate(0.0); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationRate(); stop(); }", 1, 15, "missing required argument", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationRate(-0.0000001); stop(); }", 1, 15, "requires rates to be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationRate(10000000); stop(); }", 1, 15, "requires rates to be", __LINE__);	// no longer legal, in SLiM 3.5
	
	// Test (void)initializeRecombinationRate(numeric rates, [integer ends])
	SLiMAssertScriptStop("initialize() { initializeRecombinationRate(0.0); stop(); }", __LINE__);														// legal: singleton rate, no end
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(); stop(); }", 1, 15, "missing required argument", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(-0.00001); stop(); }", 1, 15, "requires rates to be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeRecombinationRate(0.5); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(0.6); stop(); }", 1, 15, "requires rates to be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(10000); stop(); }", 1, 15, "requires rates to be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeRecombinationRate(c(0.0, 0.1), c(1000, 2000)); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(c(0.0, 0.1)); stop(); }", 1, 15, "requires rates to be a singleton if", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(integer(0), integer(0)); stop(); }", 1, 15, "ends and rates to be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(c(0.0, 0.1), 1000); stop(); }", 1, 15, "ends and rates to be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(c(0.0, 0.1), 1:3); stop(); }", 1, 15, "ends and rates to be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(c(0.0, 0.1), c(2000, 1000)); stop(); }", 1, 15, "ascending order", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(c(0.0, 0.1), c(1000, 1000)); stop(); }", 1, 15, "ascending order", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(c(0.0, -0.001), c(1000, 2000)); stop(); }", 1, 15, "requires rates to be in [0.0, 0.5]", __LINE__);
	
	SLiMAssertScriptStop("initialize() { initializeSex('A'); initializeRecombinationRate(0.0); stop(); }", __LINE__);														// legal: singleton rate, no end
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(); stop(); }", 1, 35, "missing required argument", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(-0.00001); stop(); }", 1, 35, "requires rates to be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSex('A'); initializeRecombinationRate(0.5); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(0.6); stop(); }", 1, 35, "requires rates to be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(10000); stop(); }", 1, 35, "requires rates to be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(1000, 2000)); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1)); stop(); }", 1, 35, "requires rates to be a singleton if", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(integer(0), integer(0)); stop(); }", 1, 35, "ends and rates to be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), 1000); stop(); }", 1, 35, "ends and rates to be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), 1:3); stop(); }", 1, 35, "ends and rates to be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(2000, 1000)); stop(); }", 1, 35, "ascending order", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(1000, 1000)); stop(); }", 1, 35, "ascending order", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, -0.001), c(1000, 2000)); stop(); }", 1, 35, "requires rates to be in [0.0, 0.5]", __LINE__);
	
	SLiMAssertScriptStop("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(1000, 2000), '*'); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(integer(0), integer(0), '*'); stop(); }", 1, 35, "ends and rates to be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), 1000, '*'); stop(); }", 1, 35, "ends and rates to be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), 1:3, '*'); stop(); }", 1, 35, "ends and rates to be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(2000, 1000), '*'); stop(); }", 1, 35, "ascending order", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(1000, 1000), '*'); stop(); }", 1, 35, "ascending order", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, -0.001), c(1000, 2000), '*'); stop(); }", 1, 35, "requires rates to be in [0.0, 0.5]", __LINE__);
	
	SLiMAssertScriptStop("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(1000, 2000), 'M'); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(integer(0), integer(0), 'M'); stop(); }", 1, 35, "ends and rates to be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), 1000, 'M'); stop(); }", 1, 35, "ends and rates to be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), 1:3, 'M'); stop(); }", 1, 35, "ends and rates to be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(2000, 1000), 'M'); stop(); }", 1, 35, "ascending order", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(1000, 1000), 'M'); stop(); }", 1, 35, "ascending order", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, -0.001), c(1000, 2000), 'M'); stop(); }", 1, 35, "requires rates to be in [0.0, 0.5]", __LINE__);
	
	SLiMAssertScriptStop("initialize() {" + define_g1 + "initializeMutationRate(0.0); initializeGenomicElement(g1, 0, 2000); initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(1000, 2000), 'M'); initializeRecombinationRate(0.0, 2000, 'F'); stop(); } 1 {}", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_g1 + "initializeMutationRate(0.0); initializeGenomicElement(g1, 0, 3000); initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(1000, 2000), 'M'); initializeRecombinationRate(0.0, 2000, 'F'); } 1 {}", -1, -1, "do not cover the full chromosome", __LINE__);
	SLiMAssertScriptStop("initialize() {" + define_g1 + "initializeMutationRate(0.0); initializeGenomicElement(g1, 0, 1000); initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(1000, 2000), 'M'); initializeRecombinationRate(0.0, 2000, 'F'); } 1 { stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_g1 + "initializeMutationRate(0.0); initializeGenomicElement(g1, 0, 2000); initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(1000, 2000), 'M'); initializeRecombinationRate(0.0, 1999, 'F'); } 1 {}", -1, -1, "do not cover the full chromosome", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_g1 + "initializeMutationRate(0.0); initializeGenomicElement(g1, 0, 2000); initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(1000, 2000), 'M'); initializeRecombinationRate(0.0, 2001, 'F'); } 1 { stop(); }", -1, -1, "do not cover the full chromosome", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_g1 + "initializeMutationRate(0.0); initializeGenomicElement(g1, 0, 2000); initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(1000, 2000), 'M'); initializeRecombinationRate(0.0, 2000, '*'); } 1 {}", 1, 307, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_g1 + "initializeMutationRate(0.0); initializeGenomicElement(g1, 0, 2000); initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(1000, 2000), '*'); initializeRecombinationRate(0.0, 2000, 'F'); } 1 {}", 1, 307, "single map versus separate maps", __LINE__);
	
	// Test (void)initializeSex(string$ chromosomeType, [numeric$ xDominanceCoeff])
	SLiMAssertScriptStop("initialize() { initializeSex('A'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSex('X'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSex('Y'); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('Z'); stop(); }", 1, 15, "requires a chromosomeType of", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex(); stop(); }", 1, 15, "missing required argument", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A', 0.0); stop(); }", 1, 15, "may be supplied only for", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSex('X', 0.0); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('Y', 0.0); stop(); }", 1, 15, "may be supplied only for", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('Z', 0.0); stop(); }", 1, 15, "requires a chromosomeType of", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSex('X', -10000); stop(); }", __LINE__);															// legal: no minimum value for dominance coeff
	SLiMAssertScriptStop("initialize() { initializeSex('X', 10000); stop(); }", __LINE__);															// legal: no maximum value for dominance coeff
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeSex('A'); stop(); }", 1, 35, "may be called only once", __LINE__);
	
	// Test (void)initializeSLiMModelType(string$ modelType)
	SLiMAssertScriptRaise("initialize() { initializeSLiMModelType(); stop(); }", 1, 15, "missing required argument modelType", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMModelType('WF'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMModelType('nonWF'); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMModelType('foo'); stop(); }", 1, 15, "legal values", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(); initializeSLiMModelType('WF'); stop(); }", 1, 40, "must be called before", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationRate(0.0); initializeSLiMModelType('WF'); stop(); }", 1, 44, "must be called before", __LINE__);
	
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
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(keepPedigrees=NULL); stop(); }", 1, 15, "cannot be type NULL", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality=NULL); stop(); }", 1, 15, "cannot be type NULL", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(mutationRuns=NULL); stop(); }", 1, 15, "cannot be type NULL", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(preventIncidentalSelfing=NULL); stop(); }", 1, 15, "cannot be type NULL", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='foo'); stop(); }", 1, 15, "legal non-empty values", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='y'); stop(); }", 1, 15, "legal non-empty values", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='z'); stop(); }", 1, 15, "legal non-empty values", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='xz'); stop(); }", 1, 15, "legal non-empty values", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='yz'); stop(); }", 1, 15, "legal non-empty values", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='zyx'); stop(); }", 1, 15, "legal non-empty values", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='', periodicity='x'); stop(); }", 1, 15, "may not be set in non-spatial simulations", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='x', periodicity='y'); stop(); }", 1, 15, "cannot utilize spatial dimensions beyond", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='x', periodicity='z'); stop(); }", 1, 15, "cannot utilize spatial dimensions beyond", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='xy', periodicity='z'); stop(); }", 1, 15, "cannot utilize spatial dimensions beyond", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='xyz', periodicity='foo'); stop(); }", 1, 15, "legal non-empty values", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='xyz', periodicity='xzy'); stop(); }", 1, 15, "legal non-empty values", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(); initializeSLiMOptions(); stop(); }", 1, 40, "may be called only once", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationRate(0.0); initializeSLiMOptions(); stop(); }", 1, 44, "must be called before", __LINE__);
	
	// Test (object<InteractionType>$)initializeInteractionType(is$ id, string$ spatiality, [logical$ reciprocal = F], [numeric$ maxDistance = INF], [string$ sexSegregation = "**"])
	SLiMAssertScriptRaise("initialize() { initializeInteractionType(-1, ''); stop(); }", 1, 15, "identifier value is out of range", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeInteractionType(0, ''); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeInteractionType('i0', ''); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeInteractionType(0, 'x'); stop(); }", 1, 15, "spatial dimensions beyond those set", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeInteractionType('i0', 'x'); stop(); }", 1, 15, "spatial dimensions beyond those set", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeInteractionType(0, 'w'); stop(); }", 1, 15, "spatiality \"w\" must be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeInteractionType('i0', 'w'); stop(); }", 1, 15, "spatiality \"w\" must be", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeInteractionType(0, '', T); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeInteractionType(0, '', T, 0.1); stop(); }", 1, 15, "must be INF for non-spatial interactions", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeInteractionType(0, '', T, INF, '**'); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeInteractionType(0, '', T, INF, '*M'); stop(); }", 1, 15, "unsupported in non-sexual simulation", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSex('A'); initializeInteractionType(0, '', T, INF, '**'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSex('A'); initializeInteractionType(0, '', T, INF, '*M'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSex('A'); initializeInteractionType(0, '', T, INF, '*F'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSex('A'); initializeInteractionType(0, '', T, INF, 'M*'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSex('A'); initializeInteractionType(0, '', T, INF, 'MM'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSex('A'); initializeInteractionType(0, '', T, INF, 'MF'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSex('A'); initializeInteractionType(0, '', T, INF, 'F*'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSex('A'); initializeInteractionType(0, '', T, INF, 'FM'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSex('A'); initializeInteractionType(0, '', T, INF, 'FF'); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeInteractionType(0, '', T, INF, 'W*'); stop(); }", 1, 15, "unsupported sexSegregation value", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeInteractionType(0, '', T, INF, '*W'); stop(); }", 1, 15, "unsupported sexSegregation value", __LINE__);

	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='x'); initializeInteractionType(0, 'w'); stop(); }", 1, 58, "spatiality \"w\" must be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='x'); initializeInteractionType('i0', 'w'); stop(); }", 1, 58, "spatiality \"w\" must be", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='x'); initializeInteractionType(0, '', T); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='x'); initializeInteractionType(0, '', T, 0.1); stop(); }", 1, 58, "must be INF for non-spatial interactions", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='x'); initializeInteractionType(0, '', T, INF, '**'); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='x'); initializeInteractionType(0, '', T, INF, '*M'); stop(); }", 1, 58, "unsupported in non-sexual simulation", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='x'); initializeSex('A'); initializeInteractionType(0, '', T, INF, '*M'); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='x'); initializeInteractionType(0, '', T, INF, 'W*'); stop(); }", 1, 58, "unsupported sexSegregation value", __LINE__);
	
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='x'); initializeInteractionType(0, 'x'); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='x'); initializeInteractionType(0, 'y'); stop(); }", 1, 58, "spatial dimensions beyond those set", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='x'); initializeInteractionType(0, 'x', F); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='x'); initializeInteractionType(0, 'x', T); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='x'); initializeInteractionType(0, 'x', F, 0.1); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='x'); initializeInteractionType(0, 'x', T, 0.1); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='x'); initializeInteractionType(0, 'x', T, 0.0); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='x'); initializeInteractionType(0, 'x', T, -0.1); stop(); }", 1, 58, "maxDistance must be >= 0.0", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='x'); initializeInteractionType(0, 'x', T, 0.1, '*M'); stop(); }", 1, 58, "unsupported in non-sexual simulation", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='x'); initializeSex('A'); initializeInteractionType(0, 'x', T, 0.1, '*M'); stop(); }", __LINE__);
	
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='xyz'); initializeInteractionType(0, 'x'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='xyz'); initializeInteractionType(0, 'y'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='xyz'); initializeInteractionType(0, 'z'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='xyz'); initializeInteractionType(0, 'xy'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='xyz'); initializeInteractionType(0, 'yz'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='xyz'); initializeInteractionType(0, 'xz'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='xyz'); initializeInteractionType(0, 'xyz'); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='xyz'); initializeInteractionType(0, 'w'); stop(); }", 1, 60, "spatiality \"w\" must be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='xyz'); initializeInteractionType(0, 'yx'); stop(); }", 1, 60, "spatiality \"yx\" must be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='xyz'); initializeInteractionType(0, 'zyx'); stop(); }", 1, 60, "spatiality \"zyx\" must be", __LINE__);
}

#pragma mark SLiMSim tests
void _RunSLiMSimTests(std::string temp_path)
{
	// ************************************************************************************
	//
	//	Gen 1+ tests: SLiMSim
	//
	
	// Test sim properties
	SLiMAssertScriptStop(gen1_setup + "1 { } " + gen2_stop, __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { sim.chromosome; } " + gen2_stop, __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.chromosome = sim.chromosome; } " + gen2_stop, 1, 231, "read-only property", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { if (sim.chromosomeType == 'A') stop(); } ", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.chromosomeType = 'A'; } " + gen2_stop, 1, 235, "read-only property", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { if (sim.chromosomeType == 'X') stop(); } ", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { sim.chromosomeType = 'X'; } " + gen2_stop, 1, 255, "read-only property", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { sim.dominanceCoeffX; } " + gen2_stop);															// legal: the property is meaningless but may be accessed
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.dominanceCoeffX = 0.2; } ", 1, 236, "when not simulating an X chromosome", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { sim.dominanceCoeffX; } " + gen2_stop, __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { sim.dominanceCoeffX = 0.2; } " + gen2_stop, __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 { sim.generation; } ", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 { sim.generation = 7; } " + gen2_stop, __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { if (sim.genomicElementTypes == g1) stop(); } ", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.genomicElementTypes = g1; } ", 1, 240, "read-only property", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { if (sim.modelType == 'WF') stop(); } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { if (sim.modelType == 'WF') stop(); } ", __LINE__);
	SLiMAssertScriptStop(WF_prefix + gen1_setup + "1 { if (sim.modelType == 'WF') stop(); } ", __LINE__);
	SLiMAssertScriptStop(WF_prefix + gen1_setup_sex + "1 { if (sim.modelType == 'WF') stop(); } ", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.modelType = 'foo'; } ", 1, 230, "read-only property", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { if (sim.mutationTypes == m1) stop(); } ", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.mutationTypes = m1; } ", 1, 234, "read-only property", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 { sim.mutations; } ", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.mutations = _Test(7); } ", 1, 230, "cannot be object element type", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 { sim.scriptBlocks; } ", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.scriptBlocks = sim.scriptBlocks[0]; } ", 1, 233, "read-only property", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { if (sim.sexEnabled == F) stop(); } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { if (sim.sexEnabled == T) stop(); } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { if (size(sim.subpopulations) == 0) stop(); } ", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.subpopulations = _Test(7); } ", 1, 235, "cannot be object element type", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { if (size(sim.substitutions) == 0) stop(); } ", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.substitutions = _Test(7); } ", 1, 234, "cannot be object element type", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.tag; } ", 1, 220, "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { c(sim,sim).tag; } ", 1, 227, "before being set", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 { sim.tag = -17; } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { sim.tag = -17; } 2 { if (sim.tag == -17) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { if (sim.dimensionality == '') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1 + "1 { if (sim.dimensionality == '') stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1 + "1 { sim.dimensionality = 'x'; }", 1, 366, "read-only property", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { if (sim.dimensionality == 'x') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { if (size(sim.interactionTypes) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1 + "1 { if (sim.interactionTypes == i1) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1 + "1 { sim.interactionTypes = i1; }", 1, 368, "read-only property", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { if (sim.interactionTypes == i1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { if (sim.periodicity == '') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1 + "1 { if (sim.periodicity == '') stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1 + "1 { sim.periodicity = 'x'; }", 1, 363, "read-only property", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { if (sim.periodicity == '') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyzPxz + "1 { if (sim.periodicity == 'xz') stop(); }", __LINE__);
	
#ifdef SLIMGUI
	SLiMAssertScriptStop(gen1_setup + "1 { if (sim.inSLiMgui == T) stop(); } ", __LINE__);
#else
	SLiMAssertScriptStop(gen1_setup + "1 { if (sim.inSLiMgui == F) stop(); } ", __LINE__);
#endif
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.inSLiMgui = T; }", 1, 230, "read-only property", __LINE__);
	
	// Test sim - (object<Subpopulation>)addSubpop(is$ subpopID, integer$ size, [float$ sexRatio])
	SLiMAssertScriptStop(gen1_setup + "1 { sim.addSubpop('p1', 10); } " + gen2_stop, __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { sim.addSubpop(1, 10); } " + gen2_stop, __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { sim.addSubpop('p1', 10, 0.5); } " + gen2_stop, __LINE__);	// default value
	SLiMAssertScriptStop(gen1_setup + "1 { sim.addSubpop(1, 10, 0.5); } " + gen2_stop, __LINE__);	// default value
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.addSubpop('p1', 10, 0.4); } " + gen2_stop, 1, 220, "non-sexual simulation", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.addSubpop(1, 10, 0.4); } " + gen2_stop, 1, 220, "non-sexual simulation", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { sim.addSubpop('p1', 10, 0.5); } " + gen2_stop, __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { sim.addSubpop(1, 10, 0.5); } " + gen2_stop, __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { x = sim.addSubpop('p7', 10); if (x == p7) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { x = sim.addSubpop(7, 10); if (x == p7) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { p7 = 17; sim.addSubpop('p7', 10); stop(); }", 1, 229, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.addSubpop('p7', 10); sim.addSubpop(7, 10); stop(); }", 1, 245, "already exists", __LINE__);
	
	// Test sim - (object<Subpopulation>)addSubpopSplit(is$ subpopID, integer$ size, io<Subpopulation>$ sourceSubpop, [float$ sexRatio])
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.addSubpopSplit('p2', 10, p1); } " + gen2_stop, __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.addSubpopSplit('p2', 10, 1); } " + gen2_stop, __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.addSubpopSplit(2, 10, p1); } " + gen2_stop, __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.addSubpopSplit(2, 10, 1); } " + gen2_stop, __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.addSubpopSplit(2, 10, 7); } " + gen2_stop, 1, 251, "not defined", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.addSubpopSplit('p2', 10, p1, 0.5); } " + gen2_stop, __LINE__);	// default value
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.addSubpopSplit(2, 10, p1, 0.5); } " + gen2_stop, __LINE__);	// default value
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.addSubpopSplit('p2', 10, p1, 0.4); } " + gen2_stop, 1, 251, "non-sexual simulation", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.addSubpopSplit(2, 10, p1, 0.4); } " + gen2_stop, 1, 251, "non-sexual simulation", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { sim.addSubpopSplit('p2', 10, p1, 0.5); } " + gen2_stop, __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { sim.addSubpopSplit(2, 10, p1, 0.5); } " + gen2_stop, __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { x = sim.addSubpopSplit('p7', 10, p1); if (x == p7) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { x = sim.addSubpopSplit(7, 10, p1); if (x == p7) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p7 = 17; sim.addSubpopSplit('p7', 10, p1); stop(); }", 1, 260, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.addSubpopSplit('p7', 10, p1); sim.addSubpopSplit(7, 10, p1); stop(); }", 1, 285, "already exists", __LINE__);
	
	// Test sim - (void)deregisterScriptBlock(io<SLiMEidosBlock> scriptBlocks)
	SLiMAssertScriptSuccess(gen1_setup_p1 + "1 { sim.deregisterScriptBlock(s1); } s1 2 { stop(); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1 + "1 { sim.deregisterScriptBlock(1); } s1 2 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.deregisterScriptBlock(object()); } s1 2 { stop(); }", __LINE__);									// legal: deregister nothing
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.deregisterScriptBlock(c(s1, s1)); } s1 2 { stop(); }", 1, 251, "same script block", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.deregisterScriptBlock(c(1, 1)); } s1 2 { stop(); }", 1, 251, "same script block", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.deregisterScriptBlock(s1); sim.deregisterScriptBlock(s1); } s1 2 { stop(); }", 1, 282, "same script block", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.deregisterScriptBlock(1); sim.deregisterScriptBlock(1); } s1 2 { stop(); }", 1, 281, "same script block", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1 + "1 { sim.deregisterScriptBlock(c(s1, s2)); } s1 2 { stop(); } s2 3 { stop(); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1 + "1 { sim.deregisterScriptBlock(c(1, 2)); } s1 2 { stop(); } s2 3 { stop(); }", __LINE__);
	
	// Test sim - (float)mutationFrequencies(No<Subpopulation> subpops, [object<Mutation> mutations])
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 { sim.mutationFrequencies(p1); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 { sim.mutationFrequencies(c(p1, p2)); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 { sim.mutationFrequencies(NULL); }", __LINE__);													// legal, requests population-wide frequencies
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 { sim.mutationFrequencies(sim.subpopulations); }", __LINE__);										// legal, requests population-wide frequencies
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 { sim.mutationFrequencies(object()); }", __LINE__);												// legal to specify an empty object vector
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 { sim.mutationFrequencies(1); }", 1, 301, "cannot be type integer", __LINE__);						// this is one API where integer identifiers can't be used
	
	// Test sim - (integer)mutationCounts(No<Subpopulation> subpops, [object<Mutation> mutations])
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 { sim.mutationCounts(p1); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 { sim.mutationCounts(c(p1, p2)); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 { sim.mutationCounts(NULL); }", __LINE__);													// legal, requests population-wide frequencies
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 { sim.mutationCounts(sim.subpopulations); }", __LINE__);										// legal, requests population-wide frequencies
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 { sim.mutationCounts(object()); }", __LINE__);												// legal to specify an empty object vector
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 { sim.mutationCounts(1); }", 1, 301, "cannot be type integer", __LINE__);						// this is one API where integer identifiers can't be used
	
	// Test sim - (object<Mutation>)mutationsOfType(io<MutationType>$ mutType)
	SLiMAssertScriptSuccess(gen1_setup_p1 + "10 { sim.mutationsOfType(m1); } ", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1 + "10 { sim.mutationsOfType(1); } ", __LINE__);
	
	// Test sim - (object<Mutation>)countOfMutationsOfType(io<MutationType>$ mutType)
	SLiMAssertScriptSuccess(gen1_setup_p1 + "10 { sim.countOfMutationsOfType(m1); } ", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1 + "10 { sim.countOfMutationsOfType(1); } ", __LINE__);
	
	// Test sim - (void)outputFixedMutations(void)
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 late() { sim.outputFixedMutations(); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 late() { sim.outputFixedMutations(NULL); }", __LINE__);
	if (Eidos_SlashTmpExists())
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
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 late() { sim.outputFull(NULL, T); }", 1, 308, "cannot output in binary format", __LINE__);
	if (Eidos_SlashTmpExists())
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
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "5 late() { sim.outputMutations(NULL); }", 1, 258, "cannot be type NULL", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_highmut_p1 + "5 late() { sim.outputMutations(sim.mutations, NULL); }", __LINE__);
	if (Eidos_SlashTmpExists())
		SLiMAssertScriptSuccess(gen1_setup_highmut_p1 + "5 late() { sim.outputMutations(sim.mutations, '" + temp_path + "/slimOutputMutationsTest.txt'); }", __LINE__);
	
	// Test - (void)readFromPopulationFile(string$ filePath)
	if (Eidos_SlashTmpExists())
	{
		SLiMAssertScriptSuccess(gen1_setup + "1 { sim.readFromPopulationFile('" + temp_path + "/slimOutputFullTest.txt'); }", __LINE__);												// legal, read from file path; depends on the outputFull() test above
		SLiMAssertScriptSuccess(gen1_setup + "1 { sim.readFromPopulationFile('" + temp_path + "/slimOutputFullTest.slimbinary'); }", __LINE__);										// legal, read from file path; depends on the outputFull() test above
		SLiMAssertScriptRaise(gen1_setup + "1 { sim.readFromPopulationFile('" + temp_path + "/slimOutputFullTest_POSITIONS.txt'); }", 1, 220, "spatial dimension or age information", __LINE__);
		SLiMAssertScriptRaise(gen1_setup + "1 { sim.readFromPopulationFile('" + temp_path + "/slimOutputFullTest_POSITIONS.slimbinary'); }", 1, 220, "output spatial dimensionality does not match", __LINE__);
		SLiMAssertScriptSuccess(gen1_setup_i1x + "1 { sim.readFromPopulationFile('" + temp_path + "/slimOutputFullTest_POSITIONS.txt'); }", __LINE__);
		SLiMAssertScriptSuccess(gen1_setup_i1x + "1 { sim.readFromPopulationFile('" + temp_path + "/slimOutputFullTest_POSITIONS.slimbinary'); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup + "1 { sim.readFromPopulationFile('" + temp_path + "/notAFile.foo'); }", 1, 220, "does not exist or is empty", __LINE__);
		SLiMAssertScriptSuccess(gen1_setup_p1 + "1 { sim.readFromPopulationFile('" + temp_path + "/slimOutputFullTest.txt'); if (size(sim.subpopulations) != 3) stop(); }", __LINE__);			// legal; should wipe previous state
		SLiMAssertScriptSuccess(gen1_setup_p1 + "1 { sim.readFromPopulationFile('" + temp_path + "/slimOutputFullTest.slimbinary'); if (size(sim.subpopulations) != 3) stop(); }", __LINE__);	// legal; should wipe previous state
	}
	
	// Test sim - (object<SLiMEidosBlock>)registerEarlyEvent(Nis$ id, string$ source, [integer$ start], [integer$ end])
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerEarlyEvent(NULL, '{ stop(); }', 2, 2); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerEarlyEvent('s1', '{ stop(); }', 2, 2); } s1 { }", 1, 251, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1 = 7; sim.registerEarlyEvent('s1', '{ stop(); }', 2, 2); }", 1, 259, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1 = 7; sim.registerEarlyEvent(1, '{ stop(); }', 2, 2); }", 1, 259, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerEarlyEvent(1, '{ stop(); }', 2, 2); sim.registerEarlyEvent(1, '{ stop(); }', 2, 2); }", 1, 299, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerEarlyEvent(1, '{ stop(); }', 3, 2); }", 1, 251, "requires start <= end", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerEarlyEvent(1, '{ stop(); }', -1, -1); }", 1, 251, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerEarlyEvent(1, '{ stop(); }', 0, 0); }", 1, 251, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerEarlyEvent(1, '{ $; }', 2, 2); }", 1, 2, "unexpected token '$'", __LINE__);
	
	// Test sim - (object<SLiMEidosBlock>)registerLateEvent(Nis$ id, string$ source, [integer$ start], [integer$ end])
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerLateEvent(NULL, '{ stop(); }', 2, 2); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerLateEvent('s1', '{ stop(); }', 2, 2); } s1 { }", 1, 251, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1 = 7; sim.registerLateEvent('s1', '{ stop(); }', 2, 2); }", 1, 259, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1 = 7; sim.registerLateEvent(1, '{ stop(); }', 2, 2); }", 1, 259, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerLateEvent(1, '{ stop(); }', 2, 2); sim.registerLateEvent(1, '{ stop(); }', 2, 2); }", 1, 298, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerLateEvent(1, '{ stop(); }', 3, 2); }", 1, 251, "requires start <= end", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerLateEvent(1, '{ stop(); }', -1, -1); }", 1, 251, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerLateEvent(1, '{ stop(); }', 0, 0); }", 1, 251, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerLateEvent(1, '{ $; }', 2, 2); }", 1, 2, "unexpected token '$'", __LINE__);
	
	// Test sim - (object<SLiMEidosBlock>)registerFitnessCallback(Nis$ id, string$ source, Nio<MutationType>$ mutType, [Nio<Subpopulation>$ subpop], [integer$ start], [integer$ end])
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(NULL, '{ stop(); }', 1, NULL, 5, 10); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(NULL, '{ stop(); }', m1, NULL, 5, 10); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(NULL, '{ stop(); }', NULL, NULL, 5, 10); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(NULL, '{ stop(); }', 1, 1, 5, 10); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(NULL, '{ stop(); }', m1, p1, 5, 10); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(NULL, '{ stop(); }', NULL, p1, 5, 10); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(NULL, '{ stop(); }', 1); } 10 { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(NULL, '{ stop(); }', m1); } 10 { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(NULL, '{ stop(); }', NULL); } 10 { ; }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(NULL, '{ stop(); }'); }", 1, 251, "missing required argument", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback('s1', '{ stop(); }', m1, NULL, 2, 2); } s1 { }", 1, 251, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { s1 = 7; sim.registerFitnessCallback('s1', '{ stop(); }', m1, NULL, 2, 2); }", 1, 259, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { s1 = 7; sim.registerFitnessCallback(1, '{ stop(); }', m1, NULL, 2, 2); }", 1, 259, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(1, '{ stop(); }', m1, NULL, 2, 2); sim.registerFitnessCallback(1, '{ stop(); }', m1, NULL, 2, 2); }", 1, 314, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(1, '{ stop(); }', m1, NULL, 3, 2); }", 1, 251, "requires start <= end", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(1, '{ stop(); }', m1, NULL, -1, -1); }", 1, 251, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(1, '{ stop(); }', m1, NULL, 0, 0); }", 1, 251, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(1, '{ $; }', m1, NULL, 2, 2); }", 1, 2, "unexpected token '$'", __LINE__);
	
	// Test sim - (object<SLiMEidosBlock>)registerInteractionCallback(Nis$ id, string$ source, io<InteractionType>$ intType, [Nio<Subpopulation>$ subpop], [integer$ start], [integer$ end])
	SLiMAssertScriptStop(gen1_setup_i1 + "1 { sim.registerInteractionCallback(NULL, '{ stop(); }', 1, NULL, 5, 10); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1 + "1 { sim.registerInteractionCallback(NULL, '{ stop(); }', i1, NULL, 5, 10); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1 + "1 { sim.registerInteractionCallback(NULL, '{ stop(); }', 1, 1, 5, 10); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1 + "1 { sim.registerInteractionCallback(NULL, '{ stop(); }', i1, p1, 5, 10); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1 + "1 { sim.registerInteractionCallback(NULL, '{ stop(); }', 1); } 10 { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1 + "1 { sim.registerInteractionCallback(NULL, '{ stop(); }', i1); } 10 { ; }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1 + "1 { sim.registerInteractionCallback(NULL, '{ stop(); }'); }", 1, 351, "missing required argument", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1 + "1 { sim.registerInteractionCallback('s1', '{ stop(); }', i1, NULL, 2, 2); } s1 { }", 1, 351, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1 + "1 { s1 = 7; sim.registerInteractionCallback('s1', '{ stop(); }', i1, NULL, 2, 2); }", 1, 359, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1 + "1 { s1 = 7; sim.registerInteractionCallback(1, '{ stop(); }', i1, NULL, 2, 2); }", 1, 359, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1 + "1 { sim.registerInteractionCallback(1, '{ stop(); }', i1, NULL, 2, 2); sim.registerInteractionCallback(1, '{ stop(); }', i1, NULL, 2, 2); }", 1, 418, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1 + "1 { sim.registerInteractionCallback(1, '{ stop(); }', i1, NULL, 3, 2); }", 1, 351, "requires start <= end", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1 + "1 { sim.registerInteractionCallback(1, '{ stop(); }', i1, NULL, -1, -1); }", 1, 351, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1 + "1 { sim.registerInteractionCallback(1, '{ stop(); }', i1, NULL, 0, 0); }", 1, 351, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1 + "1 { sim.registerInteractionCallback(1, '{ $; }', i1, NULL, 2, 2); }", 1, 2, "unexpected token '$'", __LINE__);
	
	// Test sim - (object<SLiMEidosBlock>)registerMateChoiceCallback(Nis$ id, string$ source, [Nio<Subpopulation>$ subpop], [integer$ start], [integer$ end])
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerMateChoiceCallback(NULL, '{ stop(); }', NULL, 2, 2); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerMateChoiceCallback(NULL, '{ stop(); }', NULL, 2, 2); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerMateChoiceCallback(NULL, '{ stop(); }', 1, 2, 2); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerMateChoiceCallback(NULL, '{ stop(); }', p1, 2, 2); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerMateChoiceCallback(NULL, '{ stop(); }'); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerMateChoiceCallback(NULL, '{ stop(); }'); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerMateChoiceCallback(NULL); }", 1, 251, "missing required argument", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerMateChoiceCallback('s1', '{ stop(); }', NULL, 2, 2); } s1 { }", 1, 251, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1 = 7; sim.registerMateChoiceCallback('s1', '{ stop(); }', NULL, 2, 2); }", 1, 259, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1 = 7; sim.registerMateChoiceCallback(1, '{ stop(); }', NULL, 2, 2); }", 1, 259, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerMateChoiceCallback(1, '{ stop(); }', NULL, 2, 2); sim.registerMateChoiceCallback(1, '{ stop(); }', NULL, 2, 2); }", 1, 313, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerMateChoiceCallback(1, '{ stop(); }', NULL, 3, 2); }", 1, 251, "requires start <= end", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerMateChoiceCallback(1, '{ stop(); }', NULL, -1, -1); }", 1, 251, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerMateChoiceCallback(1, '{ stop(); }', NULL, 0, 0); }", 1, 251, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerMateChoiceCallback(1, '{ $; }', NULL, 2, 2); }", 1, 2, "unexpected token '$'", __LINE__);
	
	// Test sim - (object<SLiMEidosBlock>)registerModifyChildCallback(Nis$ id, string$ source, [Nio<Subpopulation>$ subpop], [integer$ start], [integer$ end])
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerModifyChildCallback(NULL, '{ stop(); }', NULL, 2, 2); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerModifyChildCallback(NULL, '{ stop(); }', NULL, 2, 2); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerModifyChildCallback(NULL, '{ stop(); }', 1, 2, 2); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerModifyChildCallback(NULL, '{ stop(); }', p1, 2, 2); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerModifyChildCallback(NULL, '{ stop(); }'); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerModifyChildCallback(NULL, '{ stop(); }'); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerModifyChildCallback(NULL); }", 1, 251, "missing required argument", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerModifyChildCallback('s1', '{ stop(); }', NULL, 2, 2); } s1 { }", 1, 251, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1 = 7; sim.registerModifyChildCallback('s1', '{ stop(); }', NULL, 2, 2); }", 1, 259, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1 = 7; sim.registerModifyChildCallback(1, '{ stop(); }', NULL, 2, 2); }", 1, 259, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerModifyChildCallback(1, '{ stop(); }', NULL, 2, 2); sim.registerModifyChildCallback(1, '{ stop(); }', NULL, 2, 2); }", 1, 314, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerModifyChildCallback(1, '{ stop(); }', NULL, 3, 2); }", 1, 251, "requires start <= end", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerModifyChildCallback(1, '{ stop(); }', NULL, -1, -1); }", 1, 251, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerModifyChildCallback(1, '{ stop(); }', NULL, 0, 0); }", 1, 251, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerModifyChildCallback(1, '{ $; }', NULL, 2, 2); }", 1, 2, "unexpected token '$'", __LINE__);
	
	// Test sim – (object<SLiMEidosBlock>)rescheduleScriptBlock(object<SLiMEidosBlock>$ block, [Ni$ start = NULL], [Ni$ end = NULL], [Ni generations = NULL])
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { b = sim.rescheduleScriptBlock(s1, start=10, end=9); stop(); } s1 10 { }", 1, 255, "requires start <= end", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { b = sim.rescheduleScriptBlock(s1, generations=integer(0)); stop(); } s1 10 { }", 1, 255, "requires at least one generation", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { b = sim.rescheduleScriptBlock(s1, generations=c(25, 25)); stop(); } s1 10 { }", 1, 255, "same generation cannot be used twice", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { b = sim.rescheduleScriptBlock(s1, start=25, end=25, generations=25); stop(); } s1 10 { }", 1, 255, "either start/end or generations", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { b = sim.rescheduleScriptBlock(s1, start=25, end=NULL, generations=25); stop(); } s1 10 { }", 1, 255, "either start/end or generations", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { b = sim.rescheduleScriptBlock(s1, start=NULL, end=25, generations=25); stop(); } s1 10 { }", 1, 255, "either start/end or generations", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { b = sim.rescheduleScriptBlock(s1); stop(); } s1 10 { }", 1, 255, "either start/end or generations", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { b = sim.rescheduleScriptBlock(s1, start=25, end=25); r = sapply(b, 'applyValue.start:applyValue.end;'); if (identical(r, 25)) stop(); } s1 10 { }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { b = sim.rescheduleScriptBlock(s1, start=25, end=29); r = sapply(b, 'applyValue.start:applyValue.end;'); if (identical(r, 25:29)) stop(); } s1 10 { }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { b = sim.rescheduleScriptBlock(s1, start=NULL, end=29); r = sapply(b, 'applyValue.start:applyValue.end;'); if (identical(r, 1:29)) stop(); } s1 10 { }", 1, 255, "for the currently executing", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { b = sim.rescheduleScriptBlock(s1, end=29); r = sapply(b, 'applyValue.start:applyValue.end;'); if (identical(r, 1:29)) stop(); } s1 10 { }", 1, 255, "for the currently executing", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "2 { b = sim.rescheduleScriptBlock(s1, start=NULL, end=29); r = sapply(b, 'applyValue.start:applyValue.end;'); if (identical(r, 1:29)) stop(); } s1 10 { }", 1, 255, "scheduled for a past generation", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "2 { b = sim.rescheduleScriptBlock(s1, end=29); r = sapply(b, 'applyValue.start:applyValue.end;'); if (identical(r, 1:29)) stop(); } s1 10 { }", 1, 255, "scheduled for a past generation", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { b = sim.rescheduleScriptBlock(s1, start=25, end=NULL); if (b.start == 25 & b.end == 1000000001) stop(); } s1 10 { }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { b = sim.rescheduleScriptBlock(s1, start=25); if (b.start == 25 & b.end == 1000000001) stop(); } s1 10 { }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { b = sim.rescheduleScriptBlock(s1, generations=25); r = sapply(b, 'applyValue.start:applyValue.end;'); if (identical(r, 25)) stop(); } s1 10 { }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { b = sim.rescheduleScriptBlock(s1, generations=25:28); r = sapply(b, 'applyValue.start:applyValue.end;'); if (identical(r, 25:28)) stop(); } s1 10 { }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { b = sim.rescheduleScriptBlock(s1, generations=c(25:28, 35)); r = sapply(b, 'applyValue.start:applyValue.end;'); if (identical(r, c(25:28, 35))) stop(); } s1 10 { }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { b = sim.rescheduleScriptBlock(s1, generations=c(13, 25:28)); r = sapply(b, 'applyValue.start:applyValue.end;'); if (identical(r, c(13, 25:28))) stop(); } s1 10 { }", __LINE__);
	
	// Test sim - (void)simulationFinished(void)
	SLiMAssertScriptStop(gen1_setup_p1 + "11 { stop(); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1 + "10 { sim.simulationFinished(); } 11 { stop(); }", __LINE__);
	
	// Test sim - (object<Mutation>)subsetMutations([No<Mutation>$ exclude = NULL], [Nio<MutationType>$ mutationType = NULL], [Ni$ position = NULL], [Nis$ nucleotide = NULL], [Ni$ tag = NULL], [Ni$ id = NULL])
	// unusually, we do this with custom SLiM scripts that check the API stochastically, since it would be difficult
	// to test all the possible parameter combinations otherwise; we do a non-nucleotide test and a nucleotide test
	SLiMAssertScriptSuccess(R"(
	initialize() {
		initializeMutationRate(1e-2);
		initializeMutationType('m1', 0.5, 'f', 0.0);
		initializeMutationType('m2', 0.5, 'f', 0.0);
		initializeGenomicElementType('g1', c(m1,m2), c(1,1));
		initializeGenomicElement(g1, 0, 99);
		initializeRecombinationRate(1e-8);
		m2.color="red";
	}
	1 { sim.addSubpop('p1', 10); }
	50 {
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
	)", __LINE__);
	SLiMAssertScriptSuccess(R"(
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
	1 { sim.addSubpop('p1', 10); }
	50 {
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
	)", __LINE__);
	
	// Test sim EidosDictionary functionality: - (+)getValue(string$ key) and - (void)setValue(string$ key, + value)
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.setValue('foo', 7:9); sim.setValue('bar', 'baz'); } 10 { if (identical(sim.getValue('foo'), 7:9) & identical(sim.getValue('bar'), 'baz')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.setValue('foo', 3:5); sim.setValue('foo', 'foobar'); } 10 { if (identical(sim.getValue('foo'), 'foobar')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.setValue('foo', 3:5); sim.setValue('foo', NULL); } 10 { if (isNULL(sim.getValue('foo'))) stop(); }", __LINE__);
}

#pragma mark Subpopulation tests
void _RunSubpopulationTests(void)
{
	// ************************************************************************************
	//
	//	Gen 1+ tests: Subpopulation
	//
	
	// Test Subpopulation properties
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (p1.cloningRate == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (p1.firstMaleIndex == p1.firstMaleIndex) stop(); }", __LINE__);					// legal but undefined value in non-sexual sims
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (size(p1.genomes) == 20) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (size(p1.individuals) == 10) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (p1.id == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(p1.immigrantSubpopFractions, float(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(p1.immigrantSubpopIDs, integer(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (p1.selfingRate == 0.0) stop(); }", __LINE__);									// legal but always 0.0 in non-sexual sims
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (p1.sexRatio == 0.0) stop(); }", __LINE__);										// legal but always 0.0 in non-sexual sims
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (p1.individualCount == 10) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.tag; }", 1, 250, "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { c(p1,p1).tag; }", 1, 256, "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.tag = 135; if (p1.tag == 135) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.fitnessScaling = 135.0; if (p1.fitnessScaling == 135.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.fitnessScaling = 0.0; if (p1.fitnessScaling == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.fitnessScaling = -0.01; }", 1, 265, "must be >= 0.0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.fitnessScaling = NAN; }", 1, 265, "must be >= 0.0", __LINE__);

	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.cloningRate = 0.0; stop(); }", 1, 262, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.firstMaleIndex = p1.firstMaleIndex; stop(); }", 1, 265, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.genomes = p1.genomes[0]; stop(); }", 1, 258, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.individuals = p1.individuals[0]; stop(); }", 1, 262, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.id = 1; stop(); }", 1, 253, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.immigrantSubpopFractions = 1.0; stop(); }", 1, 275, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.immigrantSubpopIDs = 1; stop(); }", 1, 269, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.selfingRate = 0.0; stop(); }", 1, 262, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.sexRatio = 0.5; stop(); }", 1, 259, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.individualCount = 10; stop(); }", 1, 266, "read-only property", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (identical(p1.cloningRate, c(0.0,0.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (p1.firstMaleIndex == 5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.genomes) == 20) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.individuals) == 10) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (p1.id == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (identical(p1.immigrantSubpopFractions, float(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (identical(p1.immigrantSubpopIDs, integer(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (p1.selfingRate == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (p1.sexRatio == 0.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (p1.individualCount == 10) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.tag; }", 1, 270, "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { c(p1,p1).tag; }", 1, 276, "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.tag = 135; if (p1.tag == 135) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.fitnessScaling = 135.0; if (p1.fitnessScaling == 135.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.fitnessScaling = 0.0; if (p1.fitnessScaling == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.fitnessScaling = -0.01; }", 1, 285, "must be >= 0.0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.fitnessScaling = NAN; }", 1, 285, "must be >= 0.0", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.cloningRate = 0.0; stop(); }", 1, 282, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.firstMaleIndex = p1.firstMaleIndex; stop(); }", 1, 285, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.genomes = p1.genomes[0]; stop(); }", 1, 278, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.individuals = p1.individuals[0]; stop(); }", 1, 282, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.id = 1; stop(); }", 1, 273, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.immigrantSubpopFractions = 1.0; stop(); }", 1, 295, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.immigrantSubpopIDs = 1; stop(); }", 1, 289, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.selfingRate = 0.0; stop(); }", 1, 282, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.sexRatio = 0.5; stop(); }", 1, 279, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.individualCount = 10; stop(); }", 1, 286, "read-only property", __LINE__);
	
	// Test Subpopulation - (float)cachedFitness(Ni indices)
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(p1.cachedFitness(NULL), rep(1.0, 10))) stop(); }", __LINE__);				// legal (after subpop construction)
	SLiMAssertScriptStop(gen1_setup_p1 + "2 { if (identical(p1.cachedFitness(NULL), rep(1.0, 10))) stop(); }", __LINE__);				// legal (after child generation)
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(p1.cachedFitness(0), 1.0)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(p1.cachedFitness(0:3), rep(1.0, 4))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { identical(p1.cachedFitness(-1), rep(1.0, 10)); stop(); }", 1, 260, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { identical(p1.cachedFitness(10), rep(1.0, 10)); stop(); }", 1, 260, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { identical(p1.cachedFitness(c(-1,5)), rep(1.0, 10)); stop(); }", 1, 260, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { identical(p1.cachedFitness(c(5,10)), rep(1.0, 10)); stop(); }", 1, 260, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "2 { identical(p1.cachedFitness(-1), rep(1.0, 10)); stop(); }", 1, 260, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "2 { identical(p1.cachedFitness(10), rep(1.0, 10)); stop(); }", 1, 260, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "2 { identical(p1.cachedFitness(c(-1,5)), rep(1.0, 10)); stop(); }", 1, 260, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "2 { identical(p1.cachedFitness(c(5,10)), rep(1.0, 10)); stop(); }", 1, 260, "out of range", __LINE__);
	
	// Test Subpopulation – (object<Individual>)sampleIndividuals(integer$ size, [logical$ replace = F], [No<Individual>$ exclude = NULL], [Ns$ sex = NULL], [Ni$ tag = NULL], [Ni$ minAge = NULL], [Ni$ maxAge = NULL], [Nl$ migrant = NULL])
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (size(p1.sampleIndividuals(0)) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (size(p1.sampleIndividuals(1)) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (size(p1.sampleIndividuals(2)) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (size(p1.sampleIndividuals(4)) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (size(p1.sampleIndividuals(0, exclude=p1.individuals[2])) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (size(p1.sampleIndividuals(1, exclude=p1.individuals[2])) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (size(p1.sampleIndividuals(2, exclude=p1.individuals[2])) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (size(p1.sampleIndividuals(4, exclude=p1.individuals[2])) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (size(p1.sampleIndividuals(0, replace=T)) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (size(p1.sampleIndividuals(1, replace=T)) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (size(p1.sampleIndividuals(2, replace=T)) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (size(p1.sampleIndividuals(4, replace=T)) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (size(p1.sampleIndividuals(0, replace=T, exclude=p1.individuals[2])) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (size(p1.sampleIndividuals(1, replace=T, exclude=p1.individuals[2])) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (size(p1.sampleIndividuals(2, replace=T, exclude=p1.individuals[2])) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (size(p1.sampleIndividuals(4, replace=T, exclude=p1.individuals[2])) == 4) stop(); }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.sampleIndividuals(-1); }", 1, 250, "requires a sample size", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.sampleIndividuals(15, replace=F); if (size(i) == 10) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.sampleIndividuals(1, sex='M'); }", 1, 250, "in non-sexual models", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.sampleIndividuals(1, sex='W'); }", 1, 270, "unrecognized value for sex", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (p1.sampleIndividuals(3, replace=T, exclude=p1.individuals[5], sex='M', tag=1).size() == 3) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (p1.sampleIndividuals(3, replace=F, exclude=p1.individuals[5], sex='M', tag=1).size() == 2) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.sampleIndividuals(0, tag=1).tag, integer(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.sampleIndividuals(1, tag=1).tag, c(1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.sampleIndividuals(2, tag=1).tag, c(1,1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.sampleIndividuals(4, tag=1).tag, c(1,1,1,1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.sampleIndividuals(0, exclude=p1.individuals[2], tag=1).tag, integer(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.sampleIndividuals(1, exclude=p1.individuals[2], tag=1).tag, c(1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.sampleIndividuals(2, exclude=p1.individuals[2], tag=1).tag, c(1,1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.sampleIndividuals(4, exclude=p1.individuals[2], tag=1).tag, c(1,1,1,1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.sampleIndividuals(0, replace=T, tag=1).tag, integer(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.sampleIndividuals(1, replace=T, tag=1).tag, c(1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.sampleIndividuals(2, replace=T, tag=1).tag, c(1,1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.sampleIndividuals(4, replace=T, tag=1).tag, c(1,1,1,1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.sampleIndividuals(0, replace=T, exclude=p1.individuals[2], tag=1).tag, integer(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.sampleIndividuals(1, replace=T, exclude=p1.individuals[2], tag=1).tag, c(1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.sampleIndividuals(2, replace=T, exclude=p1.individuals[2], tag=1).tag, c(1,1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.sampleIndividuals(4, replace=T, exclude=p1.individuals[2], tag=1).tag, c(1,1,1,1))) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(0)) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(1)) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(2)) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(4)) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(0, exclude=p1.individuals[2])) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(1, exclude=p1.individuals[2])) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(2, exclude=p1.individuals[2])) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(4, exclude=p1.individuals[2])) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(0, replace=T)) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(1, replace=T)) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(2, replace=T)) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(4, replace=T)) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(0, replace=T, exclude=p1.individuals[2])) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(1, replace=T, exclude=p1.individuals[2])) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(2, replace=T, exclude=p1.individuals[2])) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(4, replace=T, exclude=p1.individuals[2])) == 4) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(0, sex='M')) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(1, sex='M')) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(2, sex='M')) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(4, sex='M')) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(0, exclude=p1.individuals[2], sex='M')) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(1, exclude=p1.individuals[2], sex='M')) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(2, exclude=p1.individuals[2], sex='M')) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(4, exclude=p1.individuals[2], sex='M')) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(0, replace=T, sex='M')) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(1, replace=T, sex='M')) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(2, replace=T, sex='M')) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(4, replace=T, sex='M')) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(0, replace=T, exclude=p1.individuals[2], sex='M')) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(1, replace=T, exclude=p1.individuals[2], sex='M')) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(2, replace=T, exclude=p1.individuals[2], sex='M')) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(4, replace=T, exclude=p1.individuals[2], sex='M')) == 4) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(0, sex='F')) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(1, sex='F')) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(2, sex='F')) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(4, sex='F')) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(0, exclude=p1.individuals[2], sex='F')) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(1, exclude=p1.individuals[2], sex='F')) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(2, exclude=p1.individuals[2], sex='F')) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(4, exclude=p1.individuals[2], sex='F')) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(0, replace=T, sex='F')) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(1, replace=T, sex='F')) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(2, replace=T, sex='F')) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(4, replace=T, sex='F')) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(0, replace=T, exclude=p1.individuals[2], sex='F')) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(1, replace=T, exclude=p1.individuals[2], sex='F')) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(2, replace=T, exclude=p1.individuals[2], sex='F')) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(4, replace=T, exclude=p1.individuals[2], sex='F')) == 4) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(0, migrant=F)) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(1, migrant=F)) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(2, migrant=F)) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(4, migrant=F)) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(0, exclude=p1.individuals[2], migrant=F)) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(1, exclude=p1.individuals[2], migrant=F)) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(2, exclude=p1.individuals[2], migrant=F)) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(4, exclude=p1.individuals[2], migrant=F)) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(0, replace=T, migrant=F)) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(1, replace=T, migrant=F)) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(2, replace=T, migrant=F)) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(4, replace=T, migrant=F)) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(0, replace=T, exclude=p1.individuals[2], migrant=F)) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(1, replace=T, exclude=p1.individuals[2], migrant=F)) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(2, replace=T, exclude=p1.individuals[2], migrant=F)) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(4, replace=T, exclude=p1.individuals[2], migrant=F)) == 4) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(0, migrant=T)) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(1, migrant=T)) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(1, exclude=p1.individuals[2], migrant=T)) == 0) stop(); }", __LINE__);
	
	// Test Subpopulation – (object<Individual>)subsetIndividuals([No<Individual>$ exclude = NULL], [Ns$ sex = NULL], [Ni$ tag = NULL], [Ni$ minAge = NULL], [Ni$ maxAge = NULL], [Nl$ migrant = NULL])
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (size(p1.subsetIndividuals()) == 10) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (size(p1.subsetIndividuals(exclude=p1.individuals[2])) == 9) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.subsetIndividuals(sex='M'); }", 1, 250, "in non-sexual models", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.subsetIndividuals(sex='W'); }", 1, 270, "unrecognized value for sex", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.subsetIndividuals(tag=1).tag, c(1,1,1,1,1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.subsetIndividuals(exclude=p1.individuals[3], tag=1).tag, c(1,1,1,1))) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.subsetIndividuals()) == 10) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.subsetIndividuals(exclude=p1.individuals[2])) == 9) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.subsetIndividuals(sex='M')) == 5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.subsetIndividuals(exclude=p1.individuals[2], sex='M')) == 5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.subsetIndividuals(sex='F')) == 5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.subsetIndividuals(exclude=p1.individuals[2], sex='F')) == 4) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.subsetIndividuals(migrant=F)) == 10) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.subsetIndividuals(exclude=p1.individuals[2], migrant=F)) == 9) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.subsetIndividuals(migrant=T)) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.subsetIndividuals(exclude=p1.individuals[2], migrant=T)) == 0) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (size(p1.subsetIndividuals(sex='F', tag=1)) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (size(p1.subsetIndividuals(exclude=p1.individuals[3], sex='F', tag=1)) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (size(p1.subsetIndividuals(sex='F', tag=0)) == 3) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (size(p1.subsetIndividuals(exclude=p1.individuals[3], sex='F', tag=0)) == 3) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (size(p1.subsetIndividuals(tag=1, migrant=F)) == 5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (size(p1.subsetIndividuals(exclude=p1.individuals[3], tag=1, migrant=F)) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (size(p1.subsetIndividuals(tag=0, migrant=F)) == 5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (size(p1.subsetIndividuals(exclude=p1.individuals[3], tag=0, migrant=F)) == 5) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (size(p1.subsetIndividuals(tag=1, migrant=T)) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (size(p1.subsetIndividuals(exclude=p1.individuals[3], tag=1, migrant=T)) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (size(p1.subsetIndividuals(tag=0, migrant=T)) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (size(p1.subsetIndividuals(exclude=p1.individuals[3], tag=0, migrant=T)) == 0) stop(); }", __LINE__);
	
	// Test Subpopulation - (void)outputMSSample(integer$ sampleSize, [logical$ replace], [string$ requestedSex], [Ns$ filePath = NULL], [logical$ append = F], [logical$ filterMonomorphic = F])
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputMSSample(1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputMSSample(1, F); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputMSSample(1, T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputMSSample(5); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputMSSample(5, F); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputMSSample(5, T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputMSSample(10); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputMSSample(20); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputMSSample(30); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 late() { p1.outputMSSample(30, F); stop(); }", 1, 257, "not enough eligible genomes", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputMSSample(30, T); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 late() { p1.outputMSSample(1, F, 'M'); stop(); }", 1, 257, "non-sexual simulation", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 late() { p1.outputMSSample(1, F, 'F'); stop(); }", 1, 257, "non-sexual simulation", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputMSSample(1, F, '*'); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 late() { p1.outputMSSample(1, F, 'Z'); stop(); }", 1, 257, "requested sex", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputMSSample(1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputMSSample(1, F); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputMSSample(1, T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputMSSample(5); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputMSSample(5, F); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputMSSample(5, T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputMSSample(10); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputMSSample(20); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputMSSample(30); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 late() { p1.outputMSSample(30, F); stop(); }", 1, 277, "not enough eligible genomes", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputMSSample(30, T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputMSSample(1, F, 'M'); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputMSSample(1, F, 'F'); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputMSSample(1, F, '*'); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 late() { p1.outputMSSample(1, F, 'Z'); stop(); }", 1, 277, "requested sex", __LINE__);
	
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
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 late() { p1.outputSample(30, F); stop(); }", 1, 257, "not enough eligible genomes", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputSample(30, T); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 late() { p1.outputSample(1, F, 'M'); stop(); }", 1, 257, "non-sexual simulation", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 late() { p1.outputSample(1, F, 'F'); stop(); }", 1, 257, "non-sexual simulation", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputSample(1, F, '*'); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 late() { p1.outputSample(1, F, 'Z'); stop(); }", 1, 257, "requested sex", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputSample(1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputSample(1, F); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputSample(1, T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputSample(5); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputSample(5, F); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputSample(5, T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputSample(10); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputSample(20); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputSample(30); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 late() { p1.outputSample(30, F); stop(); }", 1, 277, "not enough eligible genomes", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputSample(30, T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputSample(1, F, 'M'); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputSample(1, F, 'F'); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputSample(1, F, '*'); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 late() { p1.outputSample(1, F, 'Z'); stop(); }", 1, 277, "requested sex", __LINE__);
	
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
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 late() { p1.outputVCFSample(30, F); stop(); }", 1, 257, "not enough eligible individuals", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputVCFSample(30, T); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 late() { p1.outputVCFSample(1, F, 'M'); stop(); }", 1, 257, "non-sexual simulation", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 late() { p1.outputVCFSample(1, F, 'F'); stop(); }", 1, 257, "non-sexual simulation", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputVCFSample(1, F, '*'); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 late() { p1.outputVCFSample(1, F, 'Z'); stop(); }", 1, 257, "requested sex", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 late() { p1.outputVCFSample(5, F, 'M', F); stop(); }", 1, 257, "non-sexual simulation", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 late() { p1.outputVCFSample(5, F, 'F', F); stop(); }", 1, 257, "non-sexual simulation", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputVCFSample(5, F, '*', F); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 late() { p1.outputVCFSample(5, F, 'M', T); stop(); }", 1, 257, "non-sexual simulation", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 late() { p1.outputVCFSample(5, F, 'F', T); stop(); }", 1, 257, "non-sexual simulation", __LINE__);
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
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(30, F); stop(); }", 1, 277, "not enough eligible individuals", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(30, T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(1, F, 'M'); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(1, F, 'F'); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(1, F, '*'); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(1, F, 'Z'); stop(); }", 1, 277, "requested sex", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(5, F, 'M', F); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(5, F, 'F', F); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(5, F, '*', F); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(5, F, 'M', T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(5, F, 'F', T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(5, F, '*', T); stop(); }", __LINE__);
	
	// Test Subpopulation - (void)setCloningRate(numeric rate)
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.setCloningRate(0.0); } 10 { if (p1.cloningRate == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.setCloningRate(0.5); } 10 { if (p1.cloningRate == 0.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.setCloningRate(1.0); } 10 { if (p1.cloningRate == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.setCloningRate(-0.001); stop(); }", 1, 250, "within [0,1]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.setCloningRate(1.001); stop(); }", 1, 250, "within [0,1]", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.setCloningRate(0.0); } 10 { if (identical(p1.cloningRate, c(0.0, 0.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.setCloningRate(0.5); } 10 { if (identical(p1.cloningRate, c(0.5, 0.5))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.setCloningRate(1.0); } 10 { if (identical(p1.cloningRate, c(1.0, 1.0))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.setCloningRate(-0.001); stop(); }", 1, 270, "within [0,1]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.setCloningRate(1.001); stop(); }", 1, 270, "within [0,1]", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.setCloningRate(c(0.0, 0.1)); } 10 { if (identical(p1.cloningRate, c(0.0, 0.1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.setCloningRate(c(0.5, 0.1)); } 10 { if (identical(p1.cloningRate, c(0.5, 0.1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.setCloningRate(c(1.0, 0.1)); } 10 { if (identical(p1.cloningRate, c(1.0, 0.1))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.setCloningRate(c(0.0, -0.001)); stop(); }", 1, 270, "within [0,1]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.setCloningRate(c(0.0, 1.001)); stop(); }", 1, 270, "within [0,1]", __LINE__);
	
	// Test Subpopulation - (void)setMigrationRates(io<Subpopulation> sourceSubpops, numeric rates)
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "1 { p1.setMigrationRates(2, 0.1); } 10 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "1 { p1.setMigrationRates(3, 0.1); } 10 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "1 { p1.setMigrationRates(c(2, 3), c(0.1, 0.1)); } 10 { stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 { p1.setMigrationRates(1, 0.1); } 10 { stop(); }", 1, 300, "self-referential", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 { p1.setMigrationRates(4, 0.1); } 10 { stop(); }", 1, 300, "not defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 { p1.setMigrationRates(c(2, 1), c(0.1, 0.1)); } 10 { stop(); }", 1, 300, "self-referential", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 { p1.setMigrationRates(c(2, 4), c(0.1, 0.1)); } 10 { stop(); }", 1, 300, "not defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 { p1.setMigrationRates(c(2, 2), c(0.1, 0.1)); } 10 { stop(); }", 1, 300, "two rates set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 { p1.setMigrationRates(c(p2, p2), c(0.1, 0.1)); } 10 { stop(); }", 1, 300, "two rates set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 { p1.setMigrationRates(c(2, 3), 0.1); } 10 { stop(); }", 1, 300, "to be equal in size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 { p1.setMigrationRates(2, c(0.1, 0.1)); } 10 { stop(); }", 1, 300, "to be equal in size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 { p1.setMigrationRates(2, -0.0001); } 10 { stop(); }", 1, 300, "within [0,1]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 { p1.setMigrationRates(2, 1.0001); } 10 { stop(); }", 1, 300, "within [0,1]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 { p1.setMigrationRates(c(2, 3), c(0.6, 0.6)); } 10 { stop(); }", -1, -1, "must sum to <= 1.0", __LINE__);	// raise is from EvolveSubpopulation(); we don't force constraints prematurely
	
	// Test Subpopulation - (void)setSelfingRate(numeric$ rate)
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.setSelfingRate(0.0); } 10 { if (p1.selfingRate == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.setSelfingRate(0.5); } 10 { if (p1.selfingRate == 0.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.setSelfingRate(1.0); } 10 { if (p1.selfingRate == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.setSelfingRate(-0.001); }", 1, 250, "within [0,1]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.setSelfingRate(1.001); }", 1, 250, "within [0,1]", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.setSelfingRate(0.0); stop(); }", __LINE__);													// we permit this, since a rate of 0.0 makes sense even in sexual sims
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.setSelfingRate(0.1); stop(); }", 1, 270, "cannot be called in sexual simulations", __LINE__);
	
	// Test Subpopulation - (void)setSexRatio(float$ sexRatio)
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.setSexRatio(0.0); stop(); }", 1, 250, "cannot be called in asexual simulations", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.setSexRatio(0.1); stop(); }", 1, 250, "cannot be called in asexual simulations", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.setSexRatio(0.0); } 10 { if (p1.sexRatio == 0.0) stop(); }", 1, 270, "produced no males", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.setSexRatio(0.1); } 10 { if (p1.sexRatio == 0.1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.setSexRatio(0.5); } 10 { if (p1.sexRatio == 0.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.setSexRatio(0.9); } 10 { if (p1.sexRatio == 0.9) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.setSexRatio(1.0); } 10 { if (p1.sexRatio == 1.0) stop(); }", 1, 270, "produced no females", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.setSexRatio(-0.001); }", 1, 270, "within [0,1]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.setSexRatio(1.001); }", 1, 270, "within [0,1]", __LINE__);
	
	// Test Subpopulation - (void)setSubpopulationSize(integer$ size)
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.setSubpopulationSize(0); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.setSubpopulationSize(0); if (p1.individualCount == 10) stop(); }", 1, 279, "undefined identifier", __LINE__);		// the symbol is undefined immediately
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { px=p1; p1.setSubpopulationSize(0); if (px.individualCount == 10) stop(); }", __LINE__);									// does not take visible effect until child generation
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.setSubpopulationSize(0); } 2 { if (p1.individualCount == 0) stop(); }", 1, 285, "undefined identifier", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.setSubpopulationSize(20); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.setSubpopulationSize(20); if (p1.individualCount == 10) stop(); }", __LINE__);					// does not take visible effect until child generation
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.setSubpopulationSize(20); } 2 { if (p1.individualCount == 20) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.setSubpopulationSize(-1); stop(); }", 1, 250, "out of range", __LINE__);
	
	// Test Subpopulation EidosDictionary functionality: - (+)getValue(string$ key) and - (void)setValue(string$ key, + value)
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.setValue('foo', 7:9); p1.setValue('bar', 'baz'); } 10 { if (identical(p1.getValue('foo'), 7:9) & identical(p1.getValue('bar'), 'baz')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.setValue('foo', 3:5); p1.setValue('foo', 'foobar'); } 10 { if (identical(p1.getValue('foo'), 'foobar')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.setValue('foo', 3:5); p1.setValue('foo', NULL); } 10 { if (isNULL(p1.getValue('foo'))) stop(); }", __LINE__);
	
	// Test spatial stuff including spatialBounds, setSpatialBounds(), pointInBounds(), pointPeriodic(), pointReflected(), pointStopped(), pointUniform()
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(p1.spatialBounds, float(0))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.spatialBounds = 0.0; stop(); }", 1, 264, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.setSpatialBounds(-2.0); stop(); }", 1, 250, "setSpatialBounds() cannot be called in non-spatial simulations", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.pointInBounds(-2.0); stop(); }", 1, 250, "pointInBounds() cannot be called in non-spatial simulations", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.pointPeriodic(-2.0); stop(); }", 1, 250, "pointPeriodic() cannot be called in non-spatial simulations", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.pointReflected(-2.0); stop(); }", 1, 250, "pointReflected() cannot be called in non-spatial simulations", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.pointStopped(-2.0); stop(); }", 1, 250, "pointStopped() cannot be called in non-spatial simulations", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.pointUniform(); stop(); }", 1, 250, "pointUniform() cannot be called in non-spatial simulations", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-2.0, 7.5)); if (identical(p1.pointInBounds(float(0)), logical(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-2.0, 7.5)); if (identical(p1.pointReflected(float(0)), float(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-2.0, 7.5)); if (identical(p1.pointStopped(float(0)), float(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xPx + "1 { p1.setSpatialBounds(c(0.0, 7.5)); if (identical(p1.pointPeriodic(float(0)), float(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-2.0, 7.5)); if (identical(p1.pointUniform(0), float(0))) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { if (identical(p1.spatialBounds, c(0.0, 1.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-2.0, 7.5)); if (identical(p1.spatialBounds, c(-2.0, 7.5))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 { p1.setSpatialBounds(-2.0); stop(); }", 1, 424, "requires twice as many coordinates", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(0.0, 0.0, 1.0, 1.0)); stop(); }", 1, 424, "requires twice as many coordinates", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-2.0, 7.5)); if (p1.pointInBounds(-2.1) == F) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-2.0, 7.5)); if (p1.pointInBounds(-2.0) == T) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-2.0, 7.5)); if (p1.pointInBounds(0.0) == T) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-2.0, 7.5)); if (p1.pointInBounds(7.5) == T) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-2.0, 7.5)); if (p1.pointInBounds(7.6) == F) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-2.0, 7.5)); if (p1.pointInBounds(11.0, 0.0) == F) stop(); }", 1, 463, "too many arguments supplied", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-2.0, 7.5)); if (identical(p1.pointInBounds(c(11.0, 0.0)), c(F,T))) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointReflected(-15.5) == -0.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointReflected(-5.5) == -4.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointReflected(-5.0) == -5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointReflected(2.0) == 2.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointReflected(2.5) == 2.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointReflected(3.5) == 1.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointReflected(11.0) == -4.0) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointReflected(11.0, 0.0) == -4.0) stop(); }", 1, 463, "too many arguments supplied", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-5.0, 2.5)); if (identical(p1.pointReflected(c(-15.5, -5.5, 2.0, 3.5)), c(-0.5, -4.5, 2.0, 1.5))) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointStopped(-15.5) == -5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointStopped(-5.5) == -5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointStopped(-5.0) == -5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointStopped(2.0) == 2.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointStopped(2.5) == 2.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointStopped(3.5) == 2.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointStopped(11.0) == 2.5) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointStopped(11.0, 0.0) == -4.0) stop(); }", 1, 463, "too many arguments supplied", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-5.0, 2.5)); if (identical(p1.pointStopped(c(-15.5, -5.5, 2.0, 3.5)), c(-5.0, -5.0, 2.0, 2.5))) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-5.0, 2.5)); if (size(p1.pointUniform()) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-5.0, 2.5)); if (size(p1.pointUniform(1)) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-5.0, 2.5)); if (size(p1.pointUniform(5)) == 5) stop(); }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointPeriodic(-15.5) == -0.5) stop(); }", 1, 463, "no periodic spatial dimension", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xPx + "1 { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointPeriodic(-15.5) == -0.5) stop(); }", 1, 441, "requires min coordinates to be 0.0", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xPx + "1 { p1.setSpatialBounds(c(0.0, 2.5)); if (p1.pointPeriodic(-0.5) == 2.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xPx + "1 { p1.setSpatialBounds(c(0.0, 2.5)); if (p1.pointPeriodic(-5.5) == 2.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xPx + "1 { p1.setSpatialBounds(c(0.0, 2.5)); if (p1.pointPeriodic(0.0) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xPx + "1 { p1.setSpatialBounds(c(0.0, 2.5)); if (p1.pointPeriodic(2.0) == 2.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xPx + "1 { p1.setSpatialBounds(c(0.0, 2.5)); if (p1.pointPeriodic(2.5) == 2.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xPx + "1 { p1.setSpatialBounds(c(0.0, 2.5)); if (p1.pointPeriodic(3.5) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xPx + "1 { p1.setSpatialBounds(c(0.0, 2.5)); if (p1.pointPeriodic(11.0) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xPx + "1 { p1.setSpatialBounds(c(0.0, 2.5)); if (p1.pointPeriodic(11.0, 0.0) == -4.0) stop(); }", 1, 479, "too many arguments supplied", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xPx + "1 { p1.setSpatialBounds(c(0.0, 2.5)); if (identical(p1.pointPeriodic(c(-0.5, -5.5, 0.0, 2.5, 3.5)), c(2.0, 2.0, 0.0, 2.5, 1.0))) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 { if (identical(p1.spatialBounds, c(0.0, 0.0, 0.0, 1.0, 1.0, 1.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 { p1.setSpatialBounds(c(-2.0, -100, 10.0, 7.5, -99.5, 12.0)); if (identical(p1.spatialBounds, c(-2.0, -100, 10.0, 7.5, -99.5, 12.0))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 { p1.setSpatialBounds(-2.0); stop(); }", 1, 488, "requires twice as many coordinates", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 { p1.setSpatialBounds(c(0.0, 0.0, 1.0, 1.0)); stop(); }", 1, 488, "requires twice as many coordinates", __LINE__);
	
	std::string gen1_setup_i1xyz_bounds(gen1_setup_i1xyz + "1 { p1.setSpatialBounds(c(-10.0, 0.0, 10.0,    -9.0, 2.0, 13.0)); ");
	std::string gen1_setup_i1xyzPxz_bounds(gen1_setup_i1xyzPxz + "1 { p1.setSpatialBounds(c(0.0, 0.0, 0.0,    9.0, 2.0, 13.0)); ");
	
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (p1.pointInBounds(c(-10.1, 1.0, 11.0)) == F) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (p1.pointInBounds(c(-9.5, 1.0, 11.0)) == T) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (p1.pointInBounds(c(-8.0, 1.0, 11.0)) == F) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (p1.pointInBounds(c(-9.5, -1.0, 11.0)) == F) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (p1.pointInBounds(c(-9.5, 1.0, 11.0)) == T) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (p1.pointInBounds(c(-9.5, 3.0, 11.0)) == F) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (p1.pointInBounds(c(-9.5, 1.0, 9.0)) == F) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (p1.pointInBounds(c(-9.5, 1.0, 11.0)) == T) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (p1.pointInBounds(c(-9.5, 1.0, 14.0)) == F) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_bounds + "if (p1.pointInBounds(11.0, 0.0) == F) stop(); }", 1, 554, "too many arguments supplied", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_bounds + "if (p1.pointInBounds(c(11.0, 0.0)) == F) stop(); }", 1, 554, "requires the length of point", __LINE__);
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
	SLiMAssertScriptRaise(gen1_setup_i1xyz_bounds + "if (p1.pointReflected(11.0, 0.0) == -4.0) stop(); }", 1, 554, "too many arguments supplied", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_bounds + "if (p1.pointReflected(c(11.0, 0.0)) == -4.0) stop(); }", 1, 554, "requires the length of point", __LINE__);
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
	SLiMAssertScriptRaise(gen1_setup_i1xyz_bounds + "if (p1.pointStopped(11.0, 0.0) == -4.0) stop(); }", 1, 554, "too many arguments supplied", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_bounds + "if (p1.pointStopped(c(11.0, 0.0)) == -4.0) stop(); }", 1, 554, "requires the length of point", __LINE__);
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
	SLiMAssertScriptRaise(gen1_setup_i1xyzPxz_bounds + "if (p1.pointPeriodic(11.0, 0.0) == -4.0) stop(); }", 1, 568, "too many arguments supplied", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyzPxz_bounds + "if (p1.pointPeriodic(c(11.0, 0.0)) == -4.0) stop(); }", 1, 568, "requires the length of point", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyzPxz_bounds + "if (identical(p1.pointPeriodic(c(-10.5, -1.0, 4.5, -8.0, 2.5, 14.5)), c(7.5, -1.0, 4.5, 1.0, 2.5, 1.5))) stop(); }", __LINE__);
	
	// Test spatial stuff including defineSpatialMap(), spatialMapColor(), and spatialMapValue()
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.defineSpatialMap('map', '', integer(0), float(0)); stop(); }", 1, 250, "spatiality \"\" must be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.defineSpatialMap('map', 'x', 2, c(0.0, 1.0)); stop(); }", 1, 250, "spatial dimensions beyond those set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.spatialMapColor('m', 0.5); stop(); }", 1, 250, "could not find map", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.spatialMapValue('m', float(0)); stop(); }", 1, 250, "could not find map", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.spatialMapValue('m', 0.0); stop(); }", 1, 250, "could not find map", __LINE__);
	
	// a few tests supplying a matrix/array spatial map instead of a vector; no need to test spatialMapValue() etc. with these,
	// since it all funnels into the same map definition code anyway, so we just need to be sure the pre-funnel code is good...
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xy', c(2,2), matrix(1.0:4, nrow=2)); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xy', NULL, matrix(1.0:4, nrow=2)); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xy', c(3,3), matrix(1.0:9, nrow=3)); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xy', NULL, matrix(1.0:9, nrow=3)); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xy', c(2,3), matrix(1.0:6, nrow=2)); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xy', NULL, matrix(1.0:6, nrow=2)); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xy', c(6), matrix(1.0:6, nrow=2)); stop(); }", 1, 488, "gridSize must match the spatiality", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xy', c(3,2), matrix(1.0:6, nrow=2)); stop(); }", 1, 488, "gridSize does not match dimensions", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xy', c(2,2), matrix(1.0:6, nrow=2)); stop(); }", 1, 488, "gridSize does not match dimensions", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xy', c(3,3), matrix(1.0:6, nrow=2)); stop(); }", 1, 488, "gridSize does not match dimensions", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xz', c(2,2), matrix(1.0:4, nrow=2)); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xz', NULL, matrix(1.0:4, nrow=2)); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xz', c(3,3), matrix(1.0:9, nrow=3)); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xz', NULL, matrix(1.0:9, nrow=3)); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xz', c(2,3), matrix(1.0:6, nrow=2)); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xz', NULL, matrix(1.0:6, nrow=2)); stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xyz', c(2,2,2), array(1.0:8, c(2,2,2))); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xyz', NULL, array(1.0:8, c(2,2,2))); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xyz', c(3,3,3), array(1.0:27, c(3,3,3))); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xyz', NULL, array(1.0:27, c(3,3,3))); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xyz', c(2,3,2), array(1.0:12, c(2,3,2))); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xyz', NULL, array(1.0:12, c(2,3,2))); stop(); }", __LINE__);
	
	// 1D sim with 1D x map
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 { p1.defineSpatialMap('map', '', integer(0), float(0)); stop(); }", 1, 424, "spatiality \"\" must be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 { p1.defineSpatialMap('map', 'xy', 2, c(0.0, 1.0)); stop(); }", 1, 424, "spatial dimensions beyond those set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 { p1.defineSpatialMap('map', 'x', 1, 0.0); stop(); }", 1, 424, "elements of gridSize must be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 { p1.defineSpatialMap('map', 'x', 2, 0.0); stop(); }", 1, 424, "does not match the product of the sizes", __LINE__);
	
	std::string gen1_setup_i1x_mapNI(gen1_setup_i1x + "1 { p1.defineSpatialMap('map', 'x', 3, c(0.0, 1.0, 3.0), interpolate=F, valueRange=c(-5.0, 5.0), colors=c('black', 'white')); ");
	
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
	
	std::string gen1_setup_i1x_mapI(gen1_setup_i1x + "1 { p1.defineSpatialMap('map', 'x', 3, c(0.0, 1.0, 3.0), interpolate=T, valueRange=c(-5.0, 5.0), colors=c('#FF003F', '#007F00', '#00FFFF')); ");
	
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
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', '', integer(0), float(0)); stop(); }", 1, 488, "spatiality \"\" must be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'x', 1, 0.0); stop(); }", 1, 488, "elements of gridSize must be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'x', 2, 0.0); stop(); }", 1, 488, "does not match the product of the sizes", __LINE__);
	
	std::string gen1_setup_i1xyz_mapNIx(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'x', 3, c(0.0, 1.0, 3.0), interpolate=F, valueRange=c(-5.0, 5.0), colors=c('black', 'white')); ");
	
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
	
	std::string gen1_setup_i1xyz_mapIx(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'x', 3, c(0.0, 1.0, 3.0), interpolate=T, valueRange=c(-5.0, 5.0), colors=c('#FF003F', '#007F00', '#00FFFF')); ");
	
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
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', '', integer(0), float(0)); stop(); }", 1, 488, "spatiality \"\" must be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'z', 1, 0.0); stop(); }", 1, 488, "elements of gridSize must be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'z', 2, 0.0); stop(); }", 1, 488, "does not match the product of the sizes", __LINE__);
	
	std::string gen1_setup_i1xyz_mapNIz(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'z', 3, c(0.0, 1.0, 3.0), interpolate=F, valueRange=c(-5.0, 5.0), colors=c('black', 'white')); ");
	
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
	
	std::string gen1_setup_i1xyz_mapIz(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'z', 3, c(0.0, 1.0, 3.0), interpolate=T, valueRange=c(-5.0, 5.0), colors=c('#FF003F', '#007F00', '#00FFFF')); ");
	
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
	
	// 3D sim with 2D xz map
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', '', integer(0), float(0)); stop(); }", 1, 488, "spatiality \"\" must be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xz', 1, 0.0); stop(); }", 1, 488, "gridSize must match the spatiality", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xz', c(2,2), 0.0); stop(); }", 1, 488, "does not match the product of the sizes", __LINE__);
	
	std::string gen1_setup_i1xyz_mapNIxz(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xz', c(3,2), c(0.0, 1, 3, 5, 5, 5), interpolate=F, valueRange=c(-5.0, 5.0), colors=c('black', 'white')); ");
	
	SLiMAssertScriptRaise(gen1_setup_i1xyz_mapNIxz + "p1.spatialMapValue('map', 0.0); stop(); }", 1, 621, "must match spatiality of map", __LINE__);
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
	
	std::string gen1_setup_i1xyz_mapIxz(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xz', c(3,2), c(0.0, 1, 3, 5, 5, 5), interpolate=T, valueRange=c(-5.0, 5.0), colors=c('#FF003F', '#007F00', '#00FFFF')); ");
	
	SLiMAssertScriptRaise(gen1_setup_i1xyz_mapIxz + "p1.spatialMapValue('map', 0.0); stop(); }", 1, 636, "must match spatiality of map", __LINE__);
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
	
	// 3D sim with 3D xyz map
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', '', integer(0), float(0)); stop(); }", 1, 488, "spatiality \"\" must be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xyz', 1, 0.0); stop(); }", 1, 488, "gridSize must match the spatiality", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xyz', c(2,2,2), 0.0); stop(); }", 1, 488, "does not match the product of the sizes", __LINE__);
	
	std::string gen1_setup_i1xyz_mapNIxyz(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xyz', c(3,2,2), 0.0:11.0, interpolate=F, valueRange=c(-5.0, 5.0), colors=c('black', 'white')); ");
	
	SLiMAssertScriptRaise(gen1_setup_i1xyz_mapNIxyz + "p1.spatialMapValue('map', 0.0); stop(); }", 1, 611, "must match spatiality of map", __LINE__);
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
	
	std::string gen1_setup_i1xyz_mapIxyz(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xyz', c(3,2,2), 0.0:11.0, interpolate=T, valueRange=c(-5.0, 5.0), colors=c('#FF003F', '#007F00', '#00FFFF')); ");
	
	SLiMAssertScriptRaise(gen1_setup_i1xyz_mapIxyz + "p1.spatialMapValue('map', 0.0); stop(); }", 1, 626, "must match spatiality of map", __LINE__);
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
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; if (all(i.color == '')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; if (size(i.genome1) == 10) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; if (size(i.genome2) == 10) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; if (size(i.genomes) == 20) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; if (identical(i.genome1, i.genomes[0:9 * 2])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; if (identical(i.genome2, i.genomes[0:9 * 2 + 1])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; if (all(i.index == (0:9))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; if (all(i.subpopulation == rep(p1, 10))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; if (all(i.sex == rep('H', 10))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.color = 'red'; if (all(i.color == 'red')) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { i = p1.individuals; i[0].tag; }", 1, 272, "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { i = p1.individuals; i.tag; }", 1, 269, "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.tag = 135; if (all(i.tag == 135)) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { i = p1.individuals; i[0].tagF; }", 1, 272, "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { i = p1.individuals; i.tagF; }", 1, 269, "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.tagF = 135.0; if (all(i.tagF == 135.0)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; if (size(i.migrant) == 10) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; if (all(i.migrant == F)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.fitnessScaling = 135.0; if (all(i.fitnessScaling == 135.0)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.fitnessScaling = 0.0; if (all(i.fitnessScaling == 0.0)) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { i = p1.individuals; i.fitnessScaling = -0.01; }", 1, 284, "must be >= 0.0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { i = p1.individuals; i.fitnessScaling = NAN; }", 1, 284, "must be >= 0.0", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.x = 135.0; if (all(i.x == 135.0)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.y = 135.0; if (all(i.y == 135.0)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.z = 135.0; if (all(i.z == 135.0)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { i = p1.individuals; i.uniqueMutations; stop(); }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { i = p1.individuals; i.genome1 = i[0].genomes[0]; stop(); }", 1, 277, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { i = p1.individuals; i.genome2 = i[0].genomes[0]; stop(); }", 1, 277, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { i = p1.individuals; i.genomes = i[0].genomes[0]; stop(); }", 1, 277, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { i = p1.individuals; i.index = i[0].index; stop(); }", 1, 275, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { i = p1.individuals; i.subpopulation = i[0].subpopulation; stop(); }", 1, 283, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { i = p1.individuals; i.sex = i[0].sex; stop(); }", 1, 273, "read-only property", __LINE__);
	//SLiMAssertScriptRaise(gen1_setup_p1 + "10 { i = p1.individuals; i.uniqueMutations = sim.mutations[0]; stop(); }", 1, 287, "read-only property", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { i = p1.individuals; if (all(i.color == '')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { i = p1.individuals; if (size(i.genome1) == 10) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { i = p1.individuals; if (size(i.genome2) == 10) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { i = p1.individuals; if (size(i.genomes) == 20) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { i = p1.individuals; if (identical(i.genome1, i.genomes[0:9 * 2])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { i = p1.individuals; if (identical(i.genome2, i.genomes[0:9 * 2 + 1])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { i = p1.individuals; if (all(i.index == (0:9))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { i = p1.individuals; if (all(i.subpopulation == rep(p1, 10))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { i = p1.individuals; if (all(i.sex == repEach(c('F','M'), 5))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { i = p1.individuals; i.color = 'red'; if (all(i.color == 'red')) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { i = p1.individuals; i[0].tag; }", 1, 292, "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { i = p1.individuals; i.tag; }", 1, 289, "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { i = p1.individuals; i.tag = 135; if (all(i.tag == 135)) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { i = p1.individuals; i[0].tagF; }", 1, 292, "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { i = p1.individuals; i.tagF; }", 1, 289, "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { i = p1.individuals; i.tagF = 135.0; if (all(i.tagF == 135.0)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { i = p1.individuals; if (size(i.migrant) == 10) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { i = p1.individuals; if (all(i.migrant == F)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { i = p1.individuals; i.fitnessScaling = 135.0; if (all(i.fitnessScaling == 135.0)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { i = p1.individuals; i.fitnessScaling = 0.0; if (all(i.fitnessScaling == 0.0)) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { i = p1.individuals; i.fitnessScaling = -0.01; }", 1, 304, "must be >= 0.0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { i = p1.individuals; i.fitnessScaling = NAN; }", 1, 304, "must be >= 0.0", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { i = p1.individuals; i.x = 135.0; if (all(i.x == 135.0)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { i = p1.individuals; i.y = 135.0; if (all(i.y == 135.0)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { i = p1.individuals; i.z = 135.0; if (all(i.z == 135.0)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "10 { i = p1.individuals; i.uniqueMutations; stop(); }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { i = p1.individuals; i.genome1 = i[0].genomes[0]; stop(); }", 1, 297, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { i = p1.individuals; i.genome2 = i[0].genomes[0]; stop(); }", 1, 297, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { i = p1.individuals; i.genomes = i[0].genomes[0]; stop(); }", 1, 297, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { i = p1.individuals; i.index = i[0].index; stop(); }", 1, 295, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { i = p1.individuals; i.subpopulation = i[0].subpopulation; stop(); }", 1, 303, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { i = p1.individuals; i.sex = i[0].sex; stop(); }", 1, 293, "read-only property", __LINE__);
	//SLiMAssertScriptRaise(gen1_setup_sex_p1 + "10 { i = p1.individuals; i.uniqueMutations = sim.mutations[0]; stop(); }", 1, 307, "read-only property", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { i = p1.individuals; i.x = 0.5; if (identical(i.spatialPosition, rep(0.5, 10))) stop(); }", 1, 294, "position cannot be accessed", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { i = p1.individuals; i.x = 0.5; if (identical(i.spatialPosition, rep(0.5, 10))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 { i = p1.individuals; i.x = 0.5; i.y = 0.6; i.z = 0.7; if (identical(i.spatialPosition, rep(c(0.5, 0.6, 0.7), 10))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { i = p1.individuals; i.spatialPosition = 0.5; stop(); }", 1, 285, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 { i = p1.individuals; i.spatialPosition = 0.5; stop(); }", 1, 459, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 { i = p1.individuals; i.spatialPosition = 0.5; stop(); }", 1, 523, "read-only property", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { i = p1.individuals; i.setSpatialPosition(0.5); stop(); }", 1, 269, "cannot be called in non-spatial simulations", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 { i = p1.individuals; i[0].setSpatialPosition(float(0)); }", 1, 446, "requires at least as many coordinates", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { i = p1.individuals; i[0].setSpatialPosition(0.5); if (identical(i[0].spatialPosition, 0.5)) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 { i = p1.individuals; i[0].setSpatialPosition(c(0.5, 0.6)); }", 1, 446, "position parameter to contain", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 { i = p1.individuals; i.setSpatialPosition(float(0)); }", 1, 443, "requires at least as many coordinates", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { i = p1.individuals; i.setSpatialPosition(0.5); if (identical(i.spatialPosition, rep(0.5, 10))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 { i = p1.individuals; i.setSpatialPosition(c(0.5, 0.6)); }", 1, 443, "position parameter to contain", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { i = p1.individuals; i.setSpatialPosition((1:10) / 10.0); if (identical(i.spatialPosition, (1:10) / 10.0)) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 { i = p1.individuals; i[0].setSpatialPosition(0.5); }", 1, 510, "requires at least as many coordinates", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 { i = p1.individuals; i[0].setSpatialPosition(c(0.5, 0.6, 0.7)); if (identical(i[0].spatialPosition, c(0.5, 0.6, 0.7))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 { i = p1.individuals; i[0].setSpatialPosition(c(0.5, 0.6, 0.7, 0.8)); }", 1, 510, "position parameter to contain", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 { i = p1.individuals; i.setSpatialPosition(0.5); }", 1, 507, "requires at least as many coordinates", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 { i = p1.individuals; i.setSpatialPosition(c(0.5, 0.6, 0.7)); if (identical(i.spatialPosition, rep(c(0.5, 0.6, 0.7), 10))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 { i = p1.individuals; i.setSpatialPosition(c(0.5, 0.6, 0.7, 0.8)); }", 1, 507, "position parameter to contain", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 { i = p1.individuals; i.setSpatialPosition(1.0:30); if (identical(i.spatialPosition, 1.0:30)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 { i = p1.individuals; i.setSpatialPosition(1.0:30); if (identical(i.z, (1.0:10)*3)) stop(); }", __LINE__);
	
	// Some specific testing for setting of accelerated properties
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.tag = (seqAlong(i) % 2 == 0); if (all(i.tag == (seqAlong(i) % 2 == 0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.tag = seqAlong(i); if (all(i.tag == seqAlong(i))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.tagF = (seqAlong(i) % 2 == 0); if (all(i.tagF == (seqAlong(i) % 2 == 0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.tagF = seqAlong(i); if (all(i.tagF == seqAlong(i))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.tagF = asFloat(seqAlong(i)); if (all(i.tagF == seqAlong(i))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.fitnessScaling = (seqAlong(i) % 2 == 0); if (all(i.fitnessScaling == (seqAlong(i) % 2 == 0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.fitnessScaling = seqAlong(i); if (all(i.fitnessScaling == seqAlong(i))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.fitnessScaling = asFloat(seqAlong(i)); if (all(i.fitnessScaling == seqAlong(i))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.x = (seqAlong(i) % 2 == 0); if (all(i.x == (seqAlong(i) % 2 == 0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.x = seqAlong(i); if (all(i.x == seqAlong(i))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.x = asFloat(seqAlong(i)); if (all(i.x == seqAlong(i))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.y = (seqAlong(i) % 2 == 0); if (all(i.y == (seqAlong(i) % 2 == 0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.y = seqAlong(i); if (all(i.y == seqAlong(i))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.y = asFloat(seqAlong(i)); if (all(i.y == seqAlong(i))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.z = (seqAlong(i) % 2 == 0); if (all(i.z == (seqAlong(i) % 2 == 0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.z = seqAlong(i); if (all(i.z == seqAlong(i))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.z = asFloat(seqAlong(i)); if (all(i.z == seqAlong(i))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.color = format('#%.6X', seqAlong(i)); if (all(i.color == format('#%.6X', seqAlong(i)))) stop(); }", __LINE__);
	
	// Test Individual - (logical)containsMutations(object<Mutation> mutations)
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { i = p1.individuals; i.containsMutations(object()); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { i = p1.individuals; i.containsMutations(sim.mutations); stop(); }", __LINE__);
	
	// Test Individual - (integer$)countOfMutationsOfType(io<MutationType>$ mutType)
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { i = p1.individuals; i.countOfMutationsOfType(m1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { i = p1.individuals; i.countOfMutationsOfType(1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { i = p1.individuals; i[0:1].countOfMutationsOfType(1); stop(); }", __LINE__);
	
	// Test Individual - (float$)sumOfMutationsOfType(io<MutationType>$ mutType)
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { i = p1.individuals; i.sumOfMutationsOfType(m1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { i = p1.individuals; i.sumOfMutationsOfType(1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { i = p1.individuals; i[0:1].sumOfMutationsOfType(1); stop(); }", __LINE__);
	
	// Test Individual - (object<Mutation>)uniqueMutationsOfType(io<MutationType>$ mutType)
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { i = p1.individuals; i.uniqueMutationsOfType(m1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { i = p1.individuals; i.uniqueMutationsOfType(1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { i = p1.individuals; i[0:1].uniqueMutationsOfType(1); stop(); }", __LINE__);
	
	// Test optional pedigree stuff
	std::string gen1_setup_rel("initialize() { initializeMutationRate(1e-7); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); } 1 { sim.addSubpop('p1', 10); } ");
	
	SLiMAssertScriptStop(gen1_setup_rel + "5 { if (all(p1.individuals.pedigreeID != -1)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_rel + "5 { if (all(p1.individuals.pedigreeParentIDs != -1)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_rel + "5 { if (all(p1.individuals.pedigreeGrandparentIDs != -1)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_rel + "5 { if (all(p1.individuals.genomes.genomePedigreeID != -1)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_rel + "5 { if (p1.individuals[0].relatedness(p1.individuals[0]) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_rel + "5 { if (p1.individuals[0].relatedness(p1.individuals[1]) <= 0.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_rel + "5 { if (all(p1.individuals[0].relatedness(p1.individuals[1:9]) <= 0.5)) stop(); }", __LINE__);
	
	// Test Individual EidosDictionary functionality: - (+)getValue(string$ key) and - (void)setValue(string$ key, + value)
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals[0]; i.setValue('foo', 7:9); i.setValue('bar', 'baz'); if (identical(i.getValue('foo'), 7:9) & identical(i.getValue('bar'), 'baz')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals[0]; i.setValue('foo', 3:5); i.setValue('foo', 'foobar'); if (identical(i.getValue('foo'), 'foobar')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals[0]; i.setValue('foo', 3:5); i.setValue('foo', NULL); if (isNULL(i.getValue('foo'))) stop(); }", __LINE__);
}

#pragma mark SLiMEidosBlock tests
void _RunSLiMEidosBlockTests(void)
{
	// ************************************************************************************
	//
	//	Gen 1+ tests: SLiMEidosBlock
	//
	
	// Test SLiMEidosBlock properties
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (s1.active == -1) stop(); } s1 2:4 { sim = 10; } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (s1.end == 4) stop(); } s1 2:4 { sim = 10; } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (s1.id == 1) stop(); } s1 2:4 { sim = 10; } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (s1.source == '{ sim = 10; }') stop(); } s1 2:4 { sim = 10; } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (s1.start == 2) stop(); } s1 2:4 { sim = 10; } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (s1.type == 'early') stop(); } s1 2:4 { sim = 10; } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (s1.type == 'early') stop(); } s1 2:4 early() { sim = 10; } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (s1.type == 'late') stop(); } s1 2:4 late() { sim = 10; } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { s1.active = 198; if (s1.active == 198) stop(); } s1 2:4 { sim = 10; } ", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1.end = 4; stop(); } s1 2:4 { sim = 10; } ", 1, 254, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1.id = 1; stop(); } s1 2:4 { sim = 10; } ", 1, 253, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1.source = '{ sim = 10; }'; stop(); } s1 2:4 { sim = 10; } ", 1, 257, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1.start = 2; stop(); } s1 2:4 { sim = 10; } ", 1, 256, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1.tag; } s1 2:4 { sim = 10; } ", 1, 250, "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { c(s1,s1).tag; } s1 2:4 { sim = 10; } ", 1, 256, "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { s1.tag = 219; if (s1.tag == 219) stop(); } s1 2:4 { sim = 10; } ", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1.type = 'event'; stop(); } s1 2:4 { sim = 10; } ", 1, 255, "read-only property", __LINE__);
	
	// No methods on SLiMEidosBlock
	
	// Test user-defined functions in SLiM; there is a huge amount more that could be tested, but these get tested by EidosScribe too,
	// so mostly we just need to make sure here that they get declared and defined properly in SLiM, and are callable.
	SLiMAssertScriptStop(gen1_setup_p1 + "function (i)A(i x) {return x*2;} 1 { if (A(2) == 4) stop(); } 10 {  } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "function (i)A(i x) {return B(x)+1;} function (i)B(i x) {return x*2;} 1 { if (A(2) == 5) stop(); } 10 {  } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "function (i)fac([i b=10]) { if (b <= 1) return 1; else return b*fac(b-1); } 1 { if (fac(5) == 120) stop(); } 10 {  } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "function (i)spsize(o<Subpopulation>$ sp) { return sp.individualCount; } 2 { if (spsize(p1) == 10) stop(); } 10 {  } ", __LINE__);
	
	// Test callbacks; we don't attempt to test their functionality here, just their declaration and the fact that they get called
	// Their actual functionality gets tested by the R test suite and the recipes; we want to probe error cases here, more
	// Things to be careful of: declaration syntax, return value types, special optimized cases, pseudo-parameter definitions
	
	// fitness() callbacks
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "fitness(m1) { return relFitness; } 100 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "fitness(m1) { stop(); } 100 { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "fitness(NULL) { return relFitness; } 100 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "fitness(NULL) { stop(); } 100 { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "fitness(m1, p1) { return relFitness; } 100 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "fitness(m1, p1) { stop(); } 100 { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "fitness(NULL, p1) { return relFitness; } 100 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "fitness(NULL, p1) { stop(); } 100 { ; }", __LINE__);
	
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "fitness(m2) { stop(); } 100 { ; }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "fitness(m2, p1) { stop(); } 100 { ; }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "fitness(m1, p4) { stop(); } 100 { ; }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "fitness(NULL, p4) { stop(); } 100 { ; }", __LINE__);
	
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "early() { s1.active = 0; } s1 fitness(m1) { stop(); } 100 { ; }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "early() { s1.active = 0; } s1 fitness(m1, p1) { stop(); } 100 { ; }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "early() { s1.active = 0; } s1 fitness(NULL, p1) { stop(); } 100 { ; }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "fitness() { stop(); } 100 { ; }", 1, 301, "mutation type id is required", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "fitness(m1, p1, p2) { stop(); } 100 { ; }", 1, 307, "unexpected token", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "fitness(m1, m1) { stop(); } 100 { ; }", 1, 305, "identifier prefix \"p\" was expected", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "fitness(p1) { stop(); } 100 { ; }", 1, 301, "identifier prefix \"m\" was expected", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "fitness(m1, NULL) { stop(); } 100 { ; }", 1, 305, "identifier prefix \"p\" was expected", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "fitness(NULL, m1) { stop(); } 100 { ; }", 1, 307, "identifier prefix \"p\" was expected", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "fitness(m1) { ; } 100 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "fitness(m1) { return NULL; } 100 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "fitness(m1) { return F; } 100 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "fitness(m1) { return T; } 100 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "fitness(m1) { return 1; } 100 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "fitness(m1) { return 'a'; } 100 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "fitness(m1) { return mut; } 100 { ; }", 1, 293, "return value", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "fitness(m1) { mut; ; } 100 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "fitness(m1) { mut; return NULL; } 100 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "fitness(m1) { mut; return F; } 100 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "fitness(m1) { mut; return T; } 100 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "fitness(m1) { mut; return 1; } 100 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "fitness(m1) { mut; return 'a'; } 100 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "fitness(m1) { mut; return mut; } 100 { ; }", 1, 293, "return value", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "fitness(m1) { mut; homozygous; individual; genome1; genome2; subpop; return relFitness; } 100 { stop(); }", __LINE__);
	
	// mateChoice() callbacks
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mateChoice() { return weights; } 10 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mateChoice() { stop(); } 10 { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mateChoice(p1) { return weights; } 10 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mateChoice(p1) { stop(); } 10 { ; }", __LINE__);
	
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "mateChoice(p4) { stop(); } 10 { ; }", __LINE__);
	
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "early() { s1.active = 0; } s1 mateChoice(p1) { stop(); } 10 { ; }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(m1) { stop(); } 10 { ; }", 1, 304, "identifier prefix \"p\" was expected", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(p1, p1) { stop(); } 10 { ; }", 1, 306, "unexpected token", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(NULL) { stop(); } 10 { ; }", 1, 304, "identifier prefix \"p\" was expected", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(p1) { ; } 10 { ; }", 1, 293, "must explicitly return a value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(p1) { return F; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(p1) { return T; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(p1) { return 1; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(p1) { return 1.0; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(p1) { return 'a'; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(p1) { return genome1; } 10 { ; }", 1, 293, "return value", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(p1) { subpop; ; } 10 { ; }", 1, 293, "must explicitly return a value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(p1) { subpop; return F; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(p1) { subpop; return T; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(p1) { subpop; return 1; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(p1) { subpop; return 1.0; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(p1) { subpop; return 'a'; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(p1) { subpop; return genome1; } 10 { ; }", 1, 293, "return value", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mateChoice(p1) { individual; genome1; genome2; subpop; sourceSubpop; return weights; } 10 { stop(); }", __LINE__);
	
	// modifyChild() callbacks
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "modifyChild() { return T; } 10 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "modifyChild() { stop(); } 10 { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "modifyChild(p1) { return T; } 10 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "modifyChild(p1) { stop(); } 10 { ; }", __LINE__);
	
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "modifyChild(p4) { stop(); } 10 { ; }", __LINE__);
	
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "early() { s1.active = 0; } s1 modifyChild(p1) { stop(); } 10 { ; }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "modifyChild(m1) { stop(); } 10 { ; }", 1, 305, "identifier prefix \"p\" was expected", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "modifyChild(p1, p1) { stop(); } 10 { ; }", 1, 307, "unexpected token", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "modifyChild(NULL) { stop(); } 10 { ; }", 1, 305, "identifier prefix \"p\" was expected", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "modifyChild(p1) { ; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "modifyChild(p1) { return NULL; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "modifyChild(p1) { return 1; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "modifyChild(p1) { return 1.0; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "modifyChild(p1) { return 'a'; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "modifyChild(p1) { return child; } 10 { ; }", 1, 293, "return value", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "modifyChild(p1) { subpop; ; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "modifyChild(p1) { subpop; return NULL; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "modifyChild(p1) { subpop; return 1; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "modifyChild(p1) { subpop; return 1.0; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "modifyChild(p1) { subpop; return 'a'; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "modifyChild(p1) { subpop; return child; } 10 { ; }", 1, 293, "return value", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "modifyChild(p1) { child; childGenome1; childGenome2; parent1; parent1Genome1; parent1Genome2; isCloning; isSelfing; parent2; parent2Genome1; parent2Genome2; subpop; sourceSubpop; return T; } 10 { stop(); }", __LINE__);
	
	// recombination() callbacks
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "recombination() { return F; } 10 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "recombination() { return T; } 10 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "recombination() { stop(); } 10 { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "recombination(p1) { return F; } 10 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "recombination(p1) { return T; } 10 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "recombination(p1) { stop(); } 10 { ; }", __LINE__);
	
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "recombination(p4) { stop(); } 10 { ; }", __LINE__);
	
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "early() { s1.active = 0; } s1 recombination(p1) { stop(); } 10 { ; }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "recombination(m1) { stop(); } 10 { ; }", 1, 307, "identifier prefix \"p\" was expected", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "recombination(p1, p1) { stop(); } 10 { ; }", 1, 309, "unexpected token", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "recombination(NULL) { stop(); } 10 { ; }", 1, 307, "identifier prefix \"p\" was expected", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "recombination(p1) { ; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "recombination(p1) { return NULL; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "recombination(p1) { return 1; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "recombination(p1) { return 1.0; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "recombination(p1) { return 'a'; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "recombination(p1) { return subpop; } 10 { ; }", 1, 293, "return value", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "recombination(p1) { subpop; ; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "recombination(p1) { subpop; return NULL; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "recombination(p1) { subpop; return 1; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "recombination(p1) { subpop; return 1.0; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "recombination(p1) { subpop; return 'a'; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "recombination(p1) { subpop; return subpop; } 10 { ; }", 1, 293, "return value", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "recombination(p1) { individual; genome1; genome2; subpop; breakpoints; return T; } 10 { stop(); }", __LINE__);
	
	// interaction() callbacks
	static std::string gen1_setup_p1p2p3_i1(gen1_setup_p1p2p3 + "initialize() { initializeInteractionType('i1', ''); } { i1.evaluate(immediate=T); i1.strength(p1.individuals[0]); } ");
	
	SLiMAssertScriptStop(gen1_setup_p1p2p3_i1 + "interaction(i1) { return 1.0; } 10 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3_i1 + "interaction(i1) { stop(); } 10 { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3_i1 + "interaction(i1, p1) { return 1.0; } 10 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3_i1 + "interaction(i1, p1) { stop(); } 10 { ; }", __LINE__);
	
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3_i1 + "interaction(i2) { stop(); } 10 { ; }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3_i1 + "interaction(i2, p1) { stop(); } 10 { ; }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3_i1 + "interaction(i1, p4) { stop(); } 10 { ; }", __LINE__);
	
	SLiMAssertScriptSuccess("early() { s1.active = 0; } " + gen1_setup_p1p2p3_i1 + "s1 interaction(i1) { stop(); } 10 { ; }", __LINE__);
	SLiMAssertScriptSuccess("early() { s1.active = 0; } " + gen1_setup_p1p2p3_i1 + "s1 interaction(i1, p1) { stop(); } 10 { ; }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction() { stop(); } 10 { ; }", 1, 421, "interaction type id is required", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(i1, p1, p2) { stop(); } 10 { ; }", 1, 427, "unexpected token", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(i1, i1) { stop(); } 10 { ; }", 1, 425, "identifier prefix \"p\" was expected", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(p1) { stop(); } 10 { ; }", 1, 421, "identifier prefix \"i\" was expected", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(i1, NULL) { stop(); } 10 { ; }", 1, 425, "identifier prefix \"p\" was expected", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(NULL, i1) { stop(); } 10 { ; }", 1, 421, "identifier prefix \"i\" was expected", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(i1) { ; } 10 { ; }", 1, 409, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(i1) { return NULL; } 10 { ; }", 1, 409, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(i1) { return F; } 10 { ; }", 1, 409, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(i1) { return T; } 10 { ; }", 1, 409, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(i1) { return 1; } 10 { ; }", 1, 409, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(i1) { return 'a'; } 10 { ; }", 1, 409, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(i1) { return subpop; } 10 { ; }", 1, 409, "return value", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(i1) { subpop; ; } 10 { ; }", 1, 409, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(i1) { subpop; return F; } 10 { ; }", 1, 409, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(i1) { subpop; return T; } 10 { ; }", 1, 409, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(i1) { subpop; return 1; } 10 { ; }", 1, 409, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(i1) { subpop; return 'a'; } 10 { ; }", 1, 409, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(i1) { subpop; return subpop; } 10 { ; }", 1, 409, "return value", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_p1p2p3_i1 + "interaction(i1) { distance; strength; receiver; exerter; subpop; return 1.0; } 10 { stop(); }", __LINE__);
	
	// reproduction() callbacks
	static std::string gen1_setup_p1p2p3_nonWF(nonWF_prefix + gen1_setup_sex_p1 + "1 { sim.addSubpop('p2', 10); sim.addSubpop('p3', 10); } " + "late() { sim.subpopulations.individuals.fitnessScaling = 0.0; } ");
	
	SLiMAssertScriptStop(gen1_setup_p1p2p3_nonWF + "reproduction() { subpop.addCloned(individual); } 10 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3_nonWF + "reproduction() { stop(); } 10 { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { subpop.addCloned(individual); } 10 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { stop(); } 10 { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3_nonWF + "reproduction(p1, 'F') { subpop.addCloned(individual); } 10 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3_nonWF + "reproduction(p1, 'F') { stop(); } 10 { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3_nonWF + "reproduction(NULL, 'F') { subpop.addCloned(individual); } 10 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3_nonWF + "reproduction(NULL, 'F') { stop(); } 10 { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3_nonWF + "reproduction(p1, NULL) { subpop.addCloned(individual); } 10 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3_nonWF + "reproduction(p1, NULL) { stop(); } 10 { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3_nonWF + "reproduction(NULL, NULL) { subpop.addCloned(individual); } 10 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3_nonWF + "reproduction(NULL, NULL) { stop(); } 10 { ; }", __LINE__);
	
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3_nonWF + "reproduction(p4) { stop(); } 10 { ; }", __LINE__);
	
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3_nonWF + "reproduction() { s1.active = 0; } s1 reproduction(p1) { stop(); } 10 { ; }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(m1) { stop(); } 10 { ; }", 1, 447, "identifier prefix \"p\" was expected", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(p1, p1) { stop(); } 10 { ; }", 1, 434, "needs a value for sex", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(NULL, '*') { stop(); } 10 { ; }", 1, 434, "needs a value for sex", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { return NULL; } 10 { ; }", 1, 434, "must return void", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { return F; } 10 { ; }", 1, 434, "must return void", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { return T; } 10 { ; }", 1, 434, "must return void", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { return 1; } 10 { ; }", 1, 434, "must return void", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { return 1.0; } 10 { ; }", 1, 434, "must return void", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { return 'a'; } 10 { ; }", 1, 434, "must return void", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { return subpop; } 10 { ; }", 1, 434, "must return void", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { subpop; return NULL; } 10 { ; }", 1, 434, "must return void", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { subpop; return F; } 10 { ; }", 1, 434, "must return void", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { subpop; return T; } 10 { ; }", 1, 434, "must return void", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { subpop; return 1; } 10 { ; }", 1, 434, "must return void", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { subpop; return 1.0; } 10 { ; }", 1, 434, "must return void", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { subpop; return 'a'; } 10 { ; }", 1, 434, "must return void", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { subpop; return subpop; } 10 { ; }", 1, 434, "must return void", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { individual; genome1; genome2; subpop; subpop.addCloned(individual); } 10 { stop(); }", __LINE__);
	
	// mutation() callbacks
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mutation(m1) { return T; } 100 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mutation(m1) { return mut; } 100 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mutation(m1) { stop(); } 100 { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mutation() { return T; } 100 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mutation() { return mut; } 100 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mutation() { stop(); } 100 { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mutation(NULL) { return T; } 100 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mutation(NULL) { return mut; } 100 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mutation(NULL) { stop(); } 100 { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mutation(m1, p1) { return T; } 100 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mutation(m1, p1) { return mut; } 100 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mutation(m1, p1) { stop(); } 100 { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mutation(NULL, p1) { return T; } 100 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mutation(NULL, p1) { return mut; } 100 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mutation(NULL, p1) { stop(); } 100 { ; }", __LINE__);
	
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "mutation(m2) { stop(); } 100 { ; }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "mutation(m2, p1) { stop(); } 100 { ; }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "mutation(m1, p4) { stop(); } 100 { ; }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "mutation(NULL, p4) { stop(); } 100 { ; }", __LINE__);
	
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "early() { s1.active = 0; } s1 mutation(m1) { stop(); } 100 { ; }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "early() { s1.active = 0; } s1 mutation(m1, p1) { stop(); } 100 { ; }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "early() { s1.active = 0; } s1 mutation(NULL, p1) { stop(); } 100 { ; }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutation(m1, p1, p2) { stop(); } 100 { ; }", 1, 308, "unexpected token", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutation(m1, m1) { stop(); } 100 { ; }", 1, 306, "identifier prefix \"p\" was expected", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutation(p1) { stop(); } 100 { ; }", 1, 302, "identifier prefix \"m\" was expected", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutation(m1, NULL) { stop(); } 100 { ; }", 1, 306, "identifier prefix \"p\" was expected", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutation(NULL, m1) { stop(); } 100 { ; }", 1, 308, "identifier prefix \"p\" was expected", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutation(m1) { ; } 100 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutation(m1) { return NULL; } 100 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutation(m1) { return 1; } 100 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutation(m1) { return 1.0; } 100 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutation(m1) { return 'a'; } 100 { ; }", 1, 293, "return value", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutation(m1) { mut; ; } 100 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutation(m1) { mut; return NULL; } 100 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutation(m1) { mut; return 1; } 100 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutation(m1) { mut; return 1.0; } 100 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutation(m1) { mut; return 'a'; } 100 { ; }", 1, 293, "return value", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mutation(m1) { mut; genome; element; originalNuc; parent; subpop; return T; } 100 { stop(); }", __LINE__);
}





























