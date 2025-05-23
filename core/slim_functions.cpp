//
//  slim_functions.cpp
//  SLiM
//
//  Created by Ben Haller on 2/15/19.
//  Copyright (c) 2014-2025 Benjamin C. Haller.  All rights reserved.
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


#include "slim_functions.h"
#include "slim_globals.h"
#include "community.h"
#include "species.h"
#include "subpopulation.h"
#include "haplosome.h"
#include "mutation.h"
#include "mutation_type.h"
#include "individual.h"
#include "eidos_rng.h"
#include "json.hpp"

#include <string>
#include <vector>
#include <limits>
#include <algorithm>


extern const char *gSLiMSourceCode_calcFST;
extern const char *gSLiMSourceCode_calcVA;
extern const char *gSLiMSourceCode_calcMeanFroh;
extern const char *gSLiMSourceCode_calcPairHeterozygosity;
extern const char *gSLiMSourceCode_calcHeterozygosity;
extern const char *gSLiMSourceCode_calcWattersonsTheta;
extern const char *gSLiMSourceCode_calcInbreedingLoad;
extern const char *gSLiMSourceCode_calcPi;
extern const char *gSLiMSourceCode_calcSFS;
extern const char *gSLiMSourceCode_calcTajimasD;

extern const char *gSLiMSourceCode_initializeMutationRateFromFile;
extern const char *gSLiMSourceCode_initializeRecombinationRateFromFile;


const std::vector<EidosFunctionSignature_CSP> *Community::SLiMFunctionSignatures(void)
{
	// Allocate our own EidosFunctionSignature objects
	static std::vector<EidosFunctionSignature_CSP> sim_func_signatures_;
	
	if (!sim_func_signatures_.size())
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("Community::SLiMFunctionSignatures(): not warmed up");
		
		// Nucleotide utilities
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("codonsToAminoAcids", SLiM_ExecuteFunction_codonsToAminoAcids, kEidosValueMaskString | kEidosValueMaskInt, "SLiM"))->AddInt("codons")->AddArgWithDefault(kEidosValueMaskLogical | kEidosValueMaskInt | kEidosValueMaskOptional | kEidosValueMaskSingleton, "long", nullptr, gStaticEidosValue_LogicalF)->AddLogical_OS("paste", gStaticEidosValue_LogicalT));
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("codonsToNucleotides", SLiM_ExecuteFunction_codonsToNucleotides, kEidosValueMaskInt | kEidosValueMaskString, "SLiM"))->AddInt("codons")->AddString_OS("format", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("string"))));
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("mm16To256", SLiM_ExecuteFunction_mm16To256, kEidosValueMaskFloat, "SLiM"))->AddFloat("mutationMatrix16"));
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("mmJukesCantor", SLiM_ExecuteFunction_mmJukesCantor, kEidosValueMaskFloat, "SLiM"))->AddFloat_S("alpha"));
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("mmKimura", SLiM_ExecuteFunction_mmKimura, kEidosValueMaskFloat, "SLiM"))->AddFloat_S("alpha")->AddFloat_S("beta"));
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("nucleotideCounts", SLiM_ExecuteFunction_nucleotideCounts, kEidosValueMaskInt, "SLiM"))->AddIntString("sequence"));
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("nucleotideFrequencies", SLiM_ExecuteFunction_nucleotideFrequencies, kEidosValueMaskFloat, "SLiM"))->AddIntString("sequence"));
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("nucleotidesToCodons", SLiM_ExecuteFunction_nucleotidesToCodons, kEidosValueMaskInt, "SLiM"))->AddIntString("sequence"));
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("randomNucleotides", SLiM_ExecuteFunction_randomNucleotides, kEidosValueMaskInt | kEidosValueMaskString, "SLiM"))->AddInt_S("length")->AddNumeric_ON("basis", gStaticEidosValueNULL)->AddString_OS("format", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("string"))));
		
		// Population genetics utilities (implemented with Eidos code)
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("calcFST", gSLiMSourceCode_calcFST, kEidosValueMaskFloat | kEidosValueMaskSingleton, "SLiM"))->AddObject("haplosomes1", gSLiM_Haplosome_Class)->AddObject("haplosomes2", gSLiM_Haplosome_Class)->AddObject_ON("muts", gSLiM_Mutation_Class, gStaticEidosValueNULL)->AddInt_OSN("start", gStaticEidosValueNULL)->AddInt_OSN("end", gStaticEidosValueNULL));
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("calcVA", gSLiMSourceCode_calcVA, kEidosValueMaskFloat | kEidosValueMaskSingleton, "SLiM"))->AddObject("individuals", gSLiM_Individual_Class)->AddIntObject_S("mutType", gSLiM_MutationType_Class));
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("calcMeanFroh", gSLiMSourceCode_calcMeanFroh, kEidosValueMaskFloat | kEidosValueMaskSingleton, "SLiM"))->AddObject("individuals", gSLiM_Individual_Class)->AddInt_OS("minimumLength", EidosValue_Int_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(1000000)))->AddArgWithDefault(kEidosValueMaskNULL | kEidosValueMaskInt | kEidosValueMaskString | kEidosValueMaskObject | kEidosValueMaskOptional | kEidosValueMaskSingleton, "chromosome", gSLiM_Chromosome_Class, gStaticEidosValueNULL));
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("calcPairHeterozygosity", gSLiMSourceCode_calcPairHeterozygosity, kEidosValueMaskFloat | kEidosValueMaskSingleton, "SLiM"))->AddObject_S("haplosome1", gSLiM_Haplosome_Class)->AddObject_S("haplosome2", gSLiM_Haplosome_Class)->AddInt_OSN("start", gStaticEidosValueNULL)->AddInt_OSN("end", gStaticEidosValueNULL)->AddLogical_OS("infiniteSites", gStaticEidosValue_LogicalT));
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("calcHeterozygosity", gSLiMSourceCode_calcHeterozygosity, kEidosValueMaskFloat | kEidosValueMaskSingleton, "SLiM"))->AddObject("haplosomes", gSLiM_Haplosome_Class)->AddObject_ON("muts", gSLiM_Mutation_Class, gStaticEidosValueNULL)->AddInt_OSN("start", gStaticEidosValueNULL)->AddInt_OSN("end", gStaticEidosValueNULL));
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("calcWattersonsTheta", gSLiMSourceCode_calcWattersonsTheta, kEidosValueMaskFloat | kEidosValueMaskSingleton, "SLiM"))->AddObject("haplosomes", gSLiM_Haplosome_Class)->AddObject_ON("muts", gSLiM_Mutation_Class, gStaticEidosValueNULL)->AddInt_OSN("start", gStaticEidosValueNULL)->AddInt_OSN("end", gStaticEidosValueNULL));
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("calcInbreedingLoad", gSLiMSourceCode_calcInbreedingLoad, kEidosValueMaskFloat | kEidosValueMaskSingleton, "SLiM"))->AddObject("haplosomes", gSLiM_Haplosome_Class)->AddIntObject_OSN("mutType", gSLiM_MutationType_Class, gStaticEidosValueNULL));
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("calcPi", gSLiMSourceCode_calcPi, kEidosValueMaskFloat | kEidosValueMaskSingleton, "SLiM"))->AddObject("haplosomes", gSLiM_Haplosome_Class)->AddObject_ON("muts", gSLiM_Mutation_Class, gStaticEidosValueNULL)->AddInt_OSN("start", gStaticEidosValueNULL)->AddInt_OSN("end", gStaticEidosValueNULL));
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("calcSFS", gSLiMSourceCode_calcSFS, kEidosValueMaskNumeric, "SLiM"))->AddInt_OSN("binCount", gStaticEidosValueNULL)->AddObject_ON("haplosomes", gSLiM_Haplosome_Class, gStaticEidosValueNULL)->AddObject_ON("muts", gSLiM_Mutation_Class, gStaticEidosValueNULL)->AddString_OS("metric", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("density")))->AddLogical_OS("fold", gStaticEidosValue_LogicalF));
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("calcTajimasD", gSLiMSourceCode_calcTajimasD, kEidosValueMaskFloat | kEidosValueMaskSingleton, "SLiM"))->AddObject("haplosomes", gSLiM_Haplosome_Class)->AddObject_ON("muts", gSLiM_Mutation_Class, gStaticEidosValueNULL)->AddInt_OSN("start", gStaticEidosValueNULL)->AddInt_OSN("end", gStaticEidosValueNULL));
		
		// Other built-in SLiM functions
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("summarizeIndividuals", SLiM_ExecuteFunction_summarizeIndividuals, kEidosValueMaskFloat, "SLiM"))->AddObject("individuals", gSLiM_Individual_Class)->AddInt("dim")->AddNumeric("spatialBounds")->AddString_S("operation")->AddLogicalEquiv_OSN("empty", gStaticEidosValue_Float0)->AddLogical_OS("perUnitArea", gStaticEidosValue_LogicalF)->AddString_OSN("spatiality", gStaticEidosValueNULL));
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("treeSeqMetadata", SLiM_ExecuteFunction_treeSeqMetadata, kEidosValueMaskObject | kEidosValueMaskSingleton, gEidosDictionaryRetained_Class, "SLiM"))->AddString_S("filePath")->AddLogical_OS("userData", gStaticEidosValue_LogicalT));

		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("initializeMutationRateFromFile", gSLiMSourceCode_initializeMutationRateFromFile, kEidosValueMaskVOID, "SLiM"))->AddString_S("path")->AddInt_S("lastPosition")->AddFloat_OS("scale", EidosValue_Float_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(1e-8)))->AddString_OS("sep", gStaticEidosValue_StringTab)->AddString_OS("dec", gStaticEidosValue_StringPeriod));
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("initializeRecombinationRateFromFile", gSLiMSourceCode_initializeRecombinationRateFromFile, kEidosValueMaskVOID, "SLiM"))->AddString_S("path")->AddInt_S("lastPosition")->AddFloat_OS("scale", EidosValue_Float_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(1e-8)))->AddString_OS("sep", gStaticEidosValue_StringTab)->AddString_OS("dec", gStaticEidosValue_StringPeriod)->AddString_OS("sex", gStaticEidosValue_StringAsterisk));
		
		// Internal SLiM functions
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("_startBenchmark", SLiM_ExecuteFunction__startBenchmark, kEidosValueMaskVOID, "SLiM"))->AddString_S(gEidosStr_type));
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("_stopBenchmark", SLiM_ExecuteFunction__stopBenchmark, kEidosValueMaskFloat | kEidosValueMaskSingleton, "SLiM")));
		
		// ************************************************************************************
		//
		//	object instantiation – add in constructors for SLiM classes that have them
		//	see also EidosInterpreter::BuiltInFunctions(), which this extends
		//
		const std::vector<EidosFunctionSignature_CSP> *class_functions = gSLiM_SpatialMap_Class->Functions();
		
		sim_func_signatures_.insert(sim_func_signatures_.end(), class_functions->begin(), class_functions->end());
	}
	
	return &sim_func_signatures_;
}


// ************************************************************************************
//
//	population genetics utilities
//
#pragma mark -
#pragma mark Population genetics utilities
#pragma mark -

// These are implemented in Eidos, for transparency/modifiability.  These strings are globals mostly so the
// formatting of the code looks nice in Xcode; they are used only by Community::SLiMFunctionSignatures().

#pragma mark (float$)calcFST(object<Haplosome> haplosomes1, object<Haplosome> haplosomes2, [No<Mutation> muts = NULL], [Ni$ start = NULL], [Ni$ end = NULL])
const char *gSLiMSourceCode_calcFST = 
R"V0G0N({
	if ((haplosomes1.length() == 0) | (haplosomes2.length() == 0))
		stop("ERROR (calcFST): haplosomes1 and haplosomes2 must both be non-empty.");
	
	species = haplosomes1[0].individual.subpopulation.species;
	if (community.allSpecies.length() > 1)
	{
		if (any(c(haplosomes1, haplosomes2).individual.subpopulation.species != species))
			stop("ERROR (calcFST): all haplosomes must belong to the same species.");
		if (!isNULL(muts))
			if (any(muts.mutationType.species != species))
				stop("ERROR (calcFST): all mutations must belong to the same species as the haplosomes.");
	}
	
	chromosome = haplosomes1[0].chromosome;
	if (species.chromosomes.length() > 1)
	{
		if (any(c(haplosomes1, haplosomes2).chromosome != chromosome))
			stop("ERROR (calcFST): all haplosomes must be associated with the same chromosome.");
		if (!isNULL(muts))
			if (any(muts.chromosome != chromosome))
				stop("ERROR (calcFST): all mutations must be associated with the same chromosome as the haplosomes.");
	}
	
	if (isNULL(muts))
		muts = species.subsetMutations(chromosome=chromosome);
	
	// handle windowing
	if (!isNULL(start) & !isNULL(end))
	{
		if (start > end)
			stop("ERROR (calcFST): start must be less than or equal to end.");
		if ((start < 0) | (end >= length))
			stop("ERROR (calcFST): start and end must be within the bounds of the focal chromosome");
		mpos = muts.position;
		muts = muts[(mpos >= start) & (mpos <= end)];
	}
	else if (!isNULL(start) | !isNULL(end))
	{
		stop("ERROR (calcFST): start and end must both be NULL or both be non-NULL.");
	}
	
	// do the calculation; if the FST is undefined, return NULL
	p1_p = haplosomes1.mutationFrequenciesInHaplosomes(muts);
	p2_p = haplosomes2.mutationFrequenciesInHaplosomes(muts);
	mean_p = (p1_p + p2_p) / 2.0;
	H_t = 2.0 * mean_p * (1.0 - mean_p);
	H_s = p1_p * (1.0 - p1_p) + p2_p * (1.0 - p2_p);
	mean_H_t = mean(H_t);
	
	if (isNULL(mean_H_t))	// occurs if muts is zero-length
		return NAN;
	if (mean_H_t == 0)		// occurs if muts is not zero-length but all frequencies are zero
		return NAN;
	
	fst = 1.0 - mean(H_s) / mean_H_t;
	return fst;
})V0G0N";

#pragma mark (float$)calcVA(object<Individual> individuals, io<MutationType>$ mutType)
const char *gSLiMSourceCode_calcVA = 
R"V0G0N({
	if (individuals.length() < 2)
		stop("ERROR (calcVA): individuals must contain at least two elements.");
	
	// look up an integer mutation type id from the community
	if (type(mutType) == "integer") {
		mutTypes = community.allMutationTypes;
		mutTypeForID = mutTypes[mutTypes.id == mutType];
		assert(length(mutTypeForID) == 1, "calcVA() did not find a mutation type with id " + mutType);
		mutType = mutTypeForID;
	}
	
	// the mutation type dictates the focal species
	species = mutType.species;
	
	// all individuals must belong to the focal species
	if (community.allSpecies.length() > 1)
		if (!all(individuals.subpopulation.species == species))
			stop("ERROR (calcVA): all individuals must belong to the same species as mutType.");
	
	return var(individuals.sumOfMutationsOfType(mutType));
})V0G0N";

#pragma mark (float$)calcMeanFroh(object<Individual> individuals, [integer$ minimumLength = 1e6], [Niso<Chromosome>$ chromosome = NULL])
const char *gSLiMSourceCode_calcMeanFroh = 
R"V0G0N({
	// With zero individuals, we return NAN; it's good to be flexible on
	// this, so models don't error out on local extinction and such.
	if (length(individuals) == 0)
		return NAN;
	
	species = individuals[0].subpopulation.species;
	
	if (community.allSpecies.length() > 1)
	{
		if (any(individuals.subpopulation.species != species))
		stop("ERROR (calcMeanFroh): calcMeanFroh() requires that all individuals belong to a single species.");
	}
	
	if (minimumLength < 0)
		stop("ERROR (calcMeanFroh): calcMeanFroh() requires minimumLength >= 0 (" + minimumLength + " supplied).");
	
	// get the chromosomes we will operate over
	if (isNULL(chromosome))
	{
		chromosomes = species.chromosomes;
		chromosomes = chromosomes[chromosomes.intrinsicPloidy == 2];
	}
	else
	{
		if (type(chromosome) == "integer")
			chromosome = species.chromosomesWithIDs(chromosome);
		else if (type(chromosome) == "string")
			chromosome = species.chromosomesWithSymbols(chromosome);
		
		if (chromosome.species != species)
			stop("ERROR (calcMeanFroh): calcMeanFroh() requires that chromosome belong to the same species as individual.");
		if (chromosome.intrinsicPloidy != 2)
			stop("ERROR (calcMeanFroh): calcMeanFroh() requires that chromosome have intrinsic ploidy 2.");
		
		chromosomes = chromosome;
	}
	
	if (chromosomes.length() == 0)
		stop("ERROR (calcMeanFroh): no chromosomes with intrinsic ploidy 2 in calcMeanFroh().");
	
	// average over the individuals supplied; some might be skipped over,
	// if they have no diploid haplosomes (due to null haplosomes)
	total_individuals = 0;
	total_froh = 0;
	
	for (individual in individuals)
	{
		// do the calculation
		total_chr_length = 0;
		total_roh_length = 0;
		
		for (chr in chromosomes)
		{
			// get the haplosomes we will operate over
			haplosomes = individual.haplosomesForChromosomes(chr, includeNulls=F);
			
			if (haplosomes.length() != 2)
				next;
			
			het_pos = individual.mutationsFromHaplosomes("heterozygous").position;
			het_pos_1 = c(-1, het_pos);
			het_pos_2 = c(het_pos, chr.lastPosition + 1);
			roh = (het_pos_2 - het_pos_1) - 1;
			
			// filter the ROH we found by the threshold length passed in
			if (minimumLength > 0)
				roh = roh[roh >= minimumLength];
			
			total_roh_length = total_roh_length + sum(roh);
			total_chr_length = total_chr_length + chr.length;
		}
		
		// if total_chr_length is zero, the individual has no chromosome for which
		// it is actually diploid, so we can't calculate F_ROH; we skip it
		if (total_chr_length == 0)
			next;
		
		// we calculate F_ROH as the total ROH lengths divided by the total length
		// we add that in to our running total, to average over the individuals
		total_froh = total_froh + (total_roh_length / total_chr_length);
		total_individuals = total_individuals + 1;
	}
	
	// if we got zero individuals that we could actually calculate F_ROH for, we
	// return NAN, because it seems like we want to be tolerant of this case;
	// the user can handle it as they wish
	if (total_individuals == 0)
		return NAN;
	
	// return the average F_ROH across individuals it could be calculated for
	return total_froh / total_individuals;
})V0G0N";

#pragma mark (float$)calcPairHeterozygosity(object<Haplosome>$ haplosome1, object<Haplosome>$ haplosome2, [Ni$ start = NULL], [Ni$ end = NULL], [l$ infiniteSites = T])
const char *gSLiMSourceCode_calcPairHeterozygosity = 
R"V0G0N({
	species = haplosome1.individual.subpopulation.species;
	if (haplosome2.individual.subpopulation.species != species)
		stop("ERROR (calcPairHeterozygosity): haplosome1 and haplosome2 must belong to the same species.");
	
	chromosome = haplosome1.chromosome;
	if (haplosome2.chromosome != chromosome)
		stop("ERROR (calcPairHeterozygosity): haplosome1 and haplosome2 must belong to the same chromosome.");
	
	muts1 = haplosome1.mutations;
	muts2 = haplosome2.mutations;
	length = chromosome.length;
	
	// handle windowing
	if (!isNULL(start) & !isNULL(end))
	{
		if (start > end)
			stop("ERROR (calcPairHeterozygosity): start must be less than or equal to end.");
		if ((start < 0) | (end >= length))
			stop("ERROR (calcPairHeterozygosity): start and end must be within the bounds of the focal chromosome");
		m1pos = muts1.position;
		m2pos = muts2.position;
		muts1 = muts1[(m1pos >= start) & (m1pos <= end)];
		muts2 = muts2[(m2pos >= start) & (m2pos <= end)];
		length = end - start + 1;
	}
	else if (!isNULL(start) | !isNULL(end))
	{
		stop("ERROR (calcPairHeterozygosity): start and end must both be NULL or both be non-NULL.");
	}
	
	// do the calculation
	unshared = setSymmetricDifference(muts1, muts2);
	if (!infiniteSites)
		unshared = unique(unshared.position, preserveOrder=F);
	
	return size(unshared) / length;
})V0G0N";

#pragma mark (float$)calcHeterozygosity(o<Haplosome> haplosomes, [No<Mutation> muts = NULL], [Ni$ start = NULL], [Ni$ end = NULL])
const char *gSLiMSourceCode_calcHeterozygosity = 
R"V0G0N({
	if (haplosomes.length() == 0)
		stop("ERROR (calcHeterozygosity): haplosomes must be non-empty.");
	
	species = haplosomes[0].individual.subpopulation.species;
	if (community.allSpecies.length() > 1)
	{
		if (any(haplosomes.individual.subpopulation.species != species))
			stop("ERROR (calcHeterozygosity): all haplosomes must belong to the same species.");
		if (!isNULL(muts))
			if (any(muts.mutationType.species != species))
				stop("ERROR (calcHeterozygosity): all mutations must belong to the same species as the haplosomes.");
	}
	
	chromosome = haplosomes[0].chromosome;
	if (species.chromosomes.length() > 1)
	{
		if (any(haplosomes.chromosome != chromosome))
			stop("ERROR (calcHeterozygosity): all haplosomes must be associated with the same chromosome.");
		if (!isNULL(muts))
			if (any(muts.chromosome != chromosome))
				stop("ERROR (calcHeterozygosity): all mutations must be associated with the same chromosome as the haplosomes.");
	}
	
	length = chromosome.length;
	
	if (isNULL(muts))
		muts = species.subsetMutations(chromosome=chromosome);
	
	// handle windowing
	if (!isNULL(start) & !isNULL(end))
	{
		if (start > end)
			stop("ERROR (calcHeterozygosity): start must be less than or equal to end.");
		if ((start < 0) | (end >= length))
			stop("ERROR (calcHeterozygosity): start and end must be within the bounds of the focal chromosome");
		mpos = muts.position;
		muts = muts[(mpos >= start) & (mpos <= end)];
		length = end - start + 1;
	}
	else if (!isNULL(start) | !isNULL(end))
	{
		stop("ERROR (calcHeterozygosity): start and end must both be NULL or both be non-NULL.");
	}

	// do the calculation
	p = haplosomes.mutationFrequenciesInHaplosomes(muts);
	heterozygosity = 2 * sum(p * (1 - p)) / length;
	return heterozygosity;
})V0G0N";

#pragma mark (float$)calcWattersonsTheta(o<Haplosome> haplosomes, [No<Mutation> muts = NULL], [Ni$ start = NULL], [Ni$ end = NULL])
const char *gSLiMSourceCode_calcWattersonsTheta = 
R"V0G0N({
	if (haplosomes.length() == 0)
		stop("ERROR (calcWattersonsTheta): haplosomes must be non-empty.");
	
	species = haplosomes[0].individual.subpopulation.species;
	if (community.allSpecies.length() > 1)
	{
		if (any(haplosomes.individual.subpopulation.species != species))
			stop("ERROR (calcWattersonsTheta): all haplosomes must belong to the same species.");
		if (!isNULL(muts))
			if (any(muts.mutationType.species != species))
				stop("ERROR (calcWattersonsTheta): all mutations must belong to the same species as the haplosomes.");
	}
	
	chromosome = haplosomes[0].chromosome;
	if (species.chromosomes.length() > 1)
	{
		if (any(haplosomes.chromosome != chromosome))
			stop("ERROR (calcWattersonsTheta): all haplosomes must be associated with the same chromosome.");
		if (!isNULL(muts))
			if (any(muts.chromosome != chromosome))
				stop("ERROR (calcWattersonsTheta): all mutations must be associated with the same chromosome as the haplosomes.");
	}
	
	length = chromosome.length;
	
	if (isNULL(muts))
		muts = species.subsetMutations(chromosome=chromosome);
	
	// handle windowing
	if (!isNULL(start) & !isNULL(end))
	{
		if (start > end)
			stop("ERROR (calcWattersonsTheta): start must be less than or equal to end.");
		if ((start < 0) | (end >= length))
			stop("ERROR (calcWattersonsTheta): start and end must be within the bounds of the focal chromosome");
		mpos = muts.position;
		muts = muts[(mpos >= start) & (mpos <= end)];
		length = end - start + 1;
	}
	else if (!isNULL(start) | !isNULL(end))
	{
		stop("ERROR (calcWattersonsTheta): start and end must both be NULL or both be non-NULL.");
	}

	// narrow down to the mutations that are actually present in the haplosomes and aren't fixed
	p = haplosomes.mutationFrequenciesInHaplosomes(muts);
	muts = muts[(p != 0.0) & (p != 1.0)];

	// do the calculation
	k = size(muts);
	n = haplosomes.size();
	a_n = sum(1 / 1:(n-1));
	theta = (k / a_n) / length;
	return theta;
})V0G0N";

#pragma mark (float$)calcInbreedingLoad(object<Haplosome> haplosomes, [Nio<MutationType>$ mutType = NULL])
const char *gSLiMSourceCode_calcInbreedingLoad = 
R"V0G0N({
	// look up an integer mutation type id from the community
	if (type(mutType) == "integer") {
		mutTypes = community.allMutationTypes;
		mutTypeForID = mutTypes[mutTypes.id == mutType];
		assert(length(mutTypeForID) == 1, "calcInbreedingLoad() did not find a mutation type with id " + mutType);
		mutType = mutTypeForID;
	}
	
	if (haplosomes.length() == 0)
		stop("ERROR (calcInbreedingLoad): haplosomes must be non-empty.");
	
	species = haplosomes[0].individual.subpopulation.species;
	if (community.allSpecies.length() > 1)
	{
		if (any(haplosomes.individual.subpopulation.species != species))
			stop("ERROR (calcInbreedingLoad): all haplosomes must belong to the same species.");
		if (!isNULL(mutType))
			if (mutType.species != species)
				stop("ERROR (calcInbreedingLoad): mutType must belong to the same species as the haplosomes.");
	}
	
	chromosome = haplosomes[0].chromosome;
	if (species.chromosomes.length() > 1)
	{
		if (any(haplosomes.chromosome != chromosome))
			stop("ERROR (calcInbreedingLoad): all haplosomes must be associated with the same chromosome.");
	}
	
	// get the focal mutations and narrow down to those that are deleterious
	if (isNULL(mutType))
		muts = species.subsetMutations(chromosome=chromosome);
	else
		muts = species.subsetMutations(mutType=mutType, chromosome=chromosome);
	
	muts = muts[muts.selectionCoeff < 0.0];
	
	// get frequencies and focus on those that are in the haplosomes
	q = haplosomes.mutationFrequenciesInHaplosomes(muts);
	inHaplosomes = (q > 0);
	
	muts = muts[inHaplosomes];
	q = q[inHaplosomes];
	
	// fetch selection coefficients; note that we use the negation of
	// SLiM's selection coefficient, following Morton et al. 1956's usage
	s = -muts.selectionCoeff;
	
	// replace s > 1.0 with s == 1.0; a mutation can't be more lethal
	// than lethal (this can happen when drawing from a gamma distribution)
	s[s > 1.0] = 1.0;
	
	// get h for each mutation; note that this will not work if changing
	// h using mutationEffect() callbacks or other scripted approaches
	h = muts.mutationType.dominanceCoeff;
	
	// calculate number of haploid lethal equivalents (B or inbreeding load)
	// this equation is from Morton et al. 1956
	return (sum(q*s) - sum(q^2*s) - 2*sum(q*(1-q)*s*h));
})V0G0N";

#pragma mark (float$)calcPi(object<Haplosome> haplosomes, [No<Mutation> muts = NULL], [Ni$ start = NULL], [Ni$ end = NULL])
const char *gSLiMSourceCode_calcPi = 
R"V0G0N({
	if (haplosomes.length() < 2)
		stop("ERROR (calcPi): haplosomes must contain at least two elements.");
	
	species = haplosomes[0].individual.subpopulation.species;
	if (community.allSpecies.length() > 1)
	{
		if (any(haplosomes.individual.subpopulation.species != species))
			stop("ERROR (calcPi): all haplosomes must belong to the same species.");
		if (!isNULL(muts))
			if (any(muts.mutationType.species != species))
				stop("ERROR (calcPi): all mutations must belong to the same species as the haplosomes.");
	}
	
	chromosome = haplosomes[0].chromosome;
	if (species.chromosomes.length() > 1)
	{
		if (any(haplosomes.chromosome != chromosome))
			stop("ERROR (calcPi): all haplosomes must be associated with the same chromosome.");
		if (!isNULL(muts))
			if (any(muts.chromosome != chromosome))
				stop("ERROR (calcPi): all mutations must be associated with the same chromosome as the haplosomes.");
	}
	
	length = chromosome.length;
	
	if (isNULL(muts))
		muts = species.subsetMutations(chromosome=chromosome);
	
	// handle windowing
	if (!isNULL(start) & !isNULL(end))
	{
		if (start > end)
			stop("ERROR (calcPi): start must be less than or equal to end.");
		if ((start < 0) | (end >= length))
			stop("ERROR (calcPi): start and end must be within the bounds of the focal chromosome");
		mpos = muts.position;
		muts = muts[(mpos >= start) & (mpos <= end)];
		length = end - start + 1;
	}
	else if (!isNULL(start) | !isNULL(end))
	{
		stop("ERROR (calcPi): start and end must both be NULL or both be non-NULL.");
	}
	
	// narrow down to the mutations that are actually present in the haplosomes and aren't fixed
	p = haplosomes.mutationFrequenciesInHaplosomes(muts);
	muts = muts[(p != 0.0) & (p != 1.0)];
	
	// do the calculation
	// obtain counts of variant sequences for all segregating sites
	varCount = haplosomes.mutationCountsInHaplosomes(muts);
	// total count of sequences minus count of variant sequences equals count of invariant sequences
	invarCount = haplosomes.size() - varCount;
	// count of pairwise differences per site is the product of counts of both alleles
	// (equation 1 in Korunes and Samuk 2021); this is then summed for all sites 
	diffs = sum(varCount * invarCount);
	// pi is the ratio of pairwise differences to number of possible combinations of the given sequences
	pi = diffs / ((haplosomes.size() * (haplosomes.size() - 1)) / 2);
	// pi is conventionally averaged per site (consistent with SLiM's calculation of Watterson's theta)
	avg_pi = pi / length;
	return avg_pi;
})V0G0N";

#pragma mark (numeric)calcSFS([Ni$ binCount = NULL], [No<Haplosome> haplosomes = NULL], [No<Mutation> muts = NULL], [string$ metric = "density"], [logical$ fold = F])
const char *gSLiMSourceCode_calcSFS = 
R"V0G0N({
	// first determine the haplosomes and the species
	if (isNULL(haplosomes)) {
		if (community.allSpecies.length() != 1)
			stop("ERROR (): calcSFS() can only infer the value of haplosomes in a single-species model; otherwise, you need to supply the specific haplosomes to be used.");
		
		species = community.allSpecies;
		haplosomes = sim.subpopulations.haplosomesNonNull;
	}
	else
	{
		if (haplosomes.length() == 0)
			stop("ERROR (calcSFS): haplosomes must be non-empty.");
		
		species = haplosomes[0].individual.subpopulation.species;
		
		if (community.allSpecies.length() > 1)
		{
			if (any(haplosomes.individual.subpopulation.species != species))
				stop("ERROR (calcSFS): all haplosomes must belong to the same species.");
		}
		
		// exclude null haplosomes silently
		haplosomes = haplosomes[!haplosomes.isNullHaplosome];
	}
	
	// validate binCount
	if (!isNULL(binCount))
		if ((binCount <= 0) | (binCount > 100000))
				stop("ERROR (calcSFS): binCount must be in the range [1, 100000], or NULL.");
	
	// if no haplosomes are supplied, we don't want to error (we want to work
	// even when called on an empty simulation, for ease of use), so we just
	// return zeros; note after this point haplosomes is guaranteed non-empty
	if (haplosomes.length() == 0)
	{
		if (isNULL(binCount))
				return integer(0);
		return float(binCount);
	}
	
	// a NULL binCount means: each haplosome is a sample, and bins should be
	// counts, not frequency bins; with N samples, we have bins for the counts
	// from 1 to N-1 (here we do 0 to N, we will remove the end bins downstream)
	// note for this mode we do require that all haplosomes belong to a single
	// chromosome; counts combined across different haplosomes are not valid
	// in the general case.
	binsAreSampleCounts = F;
	if (isNULL(binCount))
	{
		chromosome = haplosomes[0].chromosome;
		if (any(haplosomes.chromosome != chromosome))
				stop("ERROR (calcSFS): when binCount is NULL, all haplosomes must be associated with the same chromosome; counts should be within a single chromosome, since a different number of haplosomes could be present for different chromosomes.");
		
		binCount = haplosomes.length() + 1;		// 0 to number of haplosomes
		binsAreSampleCounts = T;
	}
	
	// apart from the case above, we do not need to require a single chromosome;
	// we work with all mutations in the model, even if the haplosomes supplied
	// belong to a single chromosome.  this works because mutations that are not
	// present in any of the supplied haplosomes will have a frequency of zero,
	// and those will be filtered out below.
	if (isNULL(muts))
	{
		muts = species.mutations;
	}
	else
	{
		if (any(muts.chromosome.species != species))
			stop("ERROR (calcSFS): all mutations in muts must belong to the same species as the haplosomes; an SFS can be calculated only within a single species.");
	}
	
	if (!binsAreSampleCounts)
	{
		// get the frequencies; note mutationFrequenciesInHaplosomes() is smart enough
		// to assess the frequency of each mutation only within the haplosomes that
		// are associated with the same chromosome as the mutation, so this should do
		// the right thing even in multichrom models.
		freqs = haplosomes.mutationFrequenciesInHaplosomes(muts);
		
		// filter out frequencies of zero; they should not influence the SFS.
		freqs = freqs[freqs != 0.0];
		
		// discretize the frequencies into the specified number of bins
		bins = pmin(asInteger(floor(freqs * binCount)), binCount - 1);
		
		// tabulate the number of mutations in each frequency bin to make a histogram
		tallies = tabulate(bins, maxbin=binCount-1);
	}
	else
	{
		// this is the "binCount=NULL" case, where each bin is an integer count
		// we handle this separately to avoid possible float numerical error
		counts = haplosomes.mutationCountsInHaplosomes(muts);
		
		// filter out counts of zero; they should not influence the SFS.
		counts = counts[counts != 0.0];
		
		// tabulate the number of mutations in each count bin to make a histogram
		tallies = tabulate(counts, maxbin=binCount-1);
		
		// unlike above (frequency bins), with count bins we discard the bottom
		// and top bins; count bins span [1, N-1]; the top bin might contain fixed
		// mutations in SLiM, but they are not empirically observable
		if (binsAreSampleCounts)
			tallies = tallies[1:(length(tallies)-2)];
	}
	
	// "fold" the SFS if requested, combining the first and last value, and on
	// to the center; this is often done empirically because you don't know
	// which allele is ancestral and which is derived.  the tricky thing is
	// what to do with the central bin, if there is an odd number of bins; we
	// add it to itself, but the central bin could be handled other ways too,
	// such as excluding that data point (which the user can do after).
	if (fold & (size(tallies) >= 2))
	{
		midpoint = (length(tallies) - 1) / 2;
		leftSeq = asInteger(seq(from=0, to=midpoint));
		rightSeq = asInteger(seq(from=length(tallies)-1, to=midpoint));
		tallies = tallies[leftSeq] + tallies[rightSeq];
	}
	
	// return either counts or densities, as requested; note that you can have
	// binsAreSampleCounts be T, and yet return density values, that is legal,
	// and just means that you want densities for singletons, doubletons, etc.
	if (metric == "count")
		return tallies;
	else if (metric == "density")
		return tallies / sum(tallies);
	else
		stop("ERROR (calcSFS): unrecognized value '" + metric + "' for parameter metric.");
})V0G0N";

#pragma mark (float$)calcTajimasD(object<Haplosome> haplosomes, [No<Mutation> muts = NULL], [Ni$ start = NULL], [Ni$ end = NULL])
const char *gSLiMSourceCode_calcTajimasD = 
R"V0G0N({
	if (haplosomes.length() < 4)
		stop("ERROR (calcTajimasD): haplosomes must contain at least four elements.");
	
	species = haplosomes[0].individual.subpopulation.species;
	if (community.allSpecies.length() > 1)
	{
		if (any(haplosomes.individual.subpopulation.species != species))
			stop("ERROR (calcTajimasD): all haplosomes must belong to the same species.");
		if (!isNULL(muts))
			if (any(muts.mutationType.species != species))
				stop("ERROR (calcTajimasD): all mutations must belong to the same species as the haplosomes.");
	}
	
	chromosome = haplosomes[0].chromosome;
	if (species.chromosomes.length() > 1)
	{
		if (any(haplosomes.chromosome != chromosome))
			stop("ERROR (calcTajimasD): all haplosomes must be associated with the same chromosome.");
		if (!isNULL(muts))
			if (any(muts.chromosome != chromosome))
				stop("ERROR (calcTajimasD): all mutations must be associated with the same chromosome as the haplosomes.");
	}
	
	length = chromosome.length;
	
	if (isNULL(muts))
		muts = species.subsetMutations(chromosome=chromosome);
	
	// handle windowing
	if (!isNULL(start) & !isNULL(end))
	{
		if (start > end)
			stop("ERROR (calcTajimasD): start must be less than or equal to end.");
		if ((start < 0) | (end >= length))
			stop("ERROR (calcTajimasD): start and end must be within the bounds of the focal chromosome");
		mpos = muts.position;
		muts = muts[(mpos >= start) & (mpos <= end)];
		length = end - start + 1;
	}
	else if (!isNULL(start) | !isNULL(end))
	{
		stop("ERROR (calcTajimasD): start and end must both be NULL or both be non-NULL.");
	}
	
	// narrow down to the mutations that are actually present in the haplosomes and aren't fixed
	p = haplosomes.mutationFrequenciesInHaplosomes(muts);
	muts = muts[(p != 0.0) & (p != 1.0)];
	
	// do the calculation
	// Pi and Watterson's theta functions divide by sequence length so this must be undone in Tajima's D
	// Sequence length is constant (i.e. no missing data or indels) so this can be applied equally over both metrics
	diff = (calcPi(haplosomes, muts, start, end) - calcWattersonsTheta(haplosomes, muts, start, end)) * length;
	// calculate standard deviation of covariance of pi and Watterson's theta
	// note that first 3 variables defined below are sufficient for Watterson's theta calculation as well,
	// although the calcWattersonsTheta() function is used above for proper interval handling and clarity 
	k = size(muts);
	n = haplosomes.size();
	a_1 = sum(1 / 1:(n - 1));
	a_2 = sum(1 / (1:(n - 1)) ^ 2);
	b_1 = (n + 1) / (3 * (n - 1));
	b_2 = 2 * (n ^ 2 + n + 3) / (9 * n * (n - 1));
	c_1 = b_1 - 1 / a_1;
	c_2 = b_2 - (n + 2) / (a_1 * n) + a_2 / a_1 ^ 2;
	e_1 = c_1 / a_1;
	e_2 = c_2 / (a_1 ^ 2 + a_2);
	covar = e_1 * k + e_2 * k * (k - 1);
	stdev = sqrt(covar);
	tajima_d = diff / stdev;
	return tajima_d;
})V0G0N";


// ************************************************************************************
//
//	other built-in functions
//
#pragma mark -
#pragma mark Other built-in functions
#pragma mark -

#pragma mark (void)initializeMutationRateFromFile(s$ path, i$ lastPosition, [f$ scale=1e-8], [s$ sep="\t"], [s$ dec="."])
const char *gSLiMSourceCode_initializeMutationRateFromFile = 
R"V0G0N({
	errbase = "ERROR (initializeMutationRateFromFile): ";
	udf = errbase + "unexpected data format; the file cannot be read.";
	
	if ((scale <= 0.0) | (scale > 1.0))
		stop(errbase + "scale must be in (0.0, 1.0].");
	if (!fileExists(path))
		stop(errbase + "file not found at path '" + path + "'.");
	
	map = readCSV(path, colNames=c("ends", "rates"), sep=sep, dec=dec);
	if (length(map) == 0)
		stop(udf);
	if (length(map.allKeys) != 2)
		stop(udf);
	
	ends = map.getValue("ends");
	rates = map.getValue("rates");
	if (!isInteger(ends) | !isFloat(rates) | (length(ends) == 0))
		stop(udf);
	
	// We expect the first column to be start positions, not end positions.
	// The first value in that column therefore tells us whether the data
	// is zero-based or one-based; we require one or the other.  There is
	// another -1 applied to the positions because we convert them from
	// start positions to end positions; each segment ends at the base
	// previous to the start of the next segment.
	base = ends[0];
	if ((base != 0) & (base != 1))
		stop(errbase + "the first position in the file must be 0 (for 0-based positions) or 1 (for 1-based positions).");
	
	if (length(ends) == 1)
		ends = lastPosition;		// only the first start position is present
	else
		ends = c(ends[1:(size(ends)-1)] - base - 1, lastPosition);
	
	initializeMutationRate(rates * scale, ends);
})V0G0N";

#pragma mark (void)initializeRecombinationRateFromFile(s$ path, i$ lastPosition, [f$ scale=1e-8], [s$ sep="\t"], [s$ dec="."], [string$ sex = "*"])
const char *gSLiMSourceCode_initializeRecombinationRateFromFile = 
R"V0G0N({
	errbase = "ERROR (initializeRecombinationRateFromFile): ";
	udf = errbase + "unexpected data format; the file cannot be read.";
	
	if ((scale <= 0.0) | (scale > 1.0))
		stop(errbase + "scale must be in (0.0, 1.0].");
	if (!fileExists(path))
		stop(errbase + "file not found at path '" + path + "'.");
	
	map = readCSV(path, colNames=c("ends", "rates"), sep=sep, dec=dec);
	if (length(map) == 0)
		stop(udf);
	if (length(map.allKeys) != 2)
		stop(udf);
	
	ends = map.getValue("ends");
	rates = map.getValue("rates");
	if (!isInteger(ends) | !isFloat(rates) | (length(ends) == 0))
		stop(udf);
	
	// We expect the first column to be start positions, not end positions.
	// The first value in that column therefore tells us whether the data
	// is zero-based or one-based; we require one or the other.  There is
	// another -1 applied to the positions because we convert them from
	// start positions to end positions; each segment ends at the base
	// previous to the start of the next segment.
	base = ends[0];
	if ((base != 0) & (base != 1))
		stop(errbase + "the first position in the file must be 0 (for 0-based positions) or 1 (for 1-based positions).");
	
	if (length(ends) == 1)
		ends = lastPosition;		// only the first start position is present
	else
		ends = c(ends[1:(size(ends)-1)] - base - 1, lastPosition);
	
	initializeRecombinationRate(rates * scale, ends, sex);
})V0G0N";


// ************************************************************************************
//
//	codon tables
//
#pragma mark -
#pragma mark Codon tables
#pragma mark -

static std::string codon2aa_short[64] = {
	/* AAA */	"K",
	/* AAC */	"N",
	/* AAG */	"K",
	/* AAT */	"N",
	/* ACA */	"T",
	/* ACC */	"T",
	/* ACG */	"T",
	/* ACT */	"T",
	/* AGA */	"R",
	/* AGC */	"S",
	/* AGG */	"R",
	/* AGT */	"S",
	/* ATA */	"I",
	/* ATC */	"I",
	/* ATG */	"M",
	/* ATT */	"I",
	/* CAA */	"Q",
	/* CAC */	"H",
	/* CAG */	"Q",
	/* CAT */	"H",
	/* CCA */	"P",
	/* CCC */	"P",
	/* CCG */	"P",
	/* CCT */	"P",
	/* CGA */	"R",
	/* CGC */	"R",
	/* CGG */	"R",
	/* CGT */	"R",
	/* CTA */	"L",
	/* CTC */	"L",
	/* CTG */	"L",
	/* CTT */	"L",
	/* GAA */	"E",
	/* GAC */	"D",
	/* GAG */	"E",
	/* GAT */	"D",
	/* GCA */	"A",
	/* GCC */	"A",
	/* GCG */	"A",
	/* GCT */	"A",
	/* GGA */	"G",
	/* GGC */	"G",
	/* GGG */	"G",
	/* GGT */	"G",
	/* GTA */	"V",
	/* GTC */	"V",
	/* GTG */	"V",
	/* GTT */	"V",
	/* TAA */	"X",
	/* TAC */	"Y",
	/* TAG */	"X",
	/* TAT */	"Y",
	/* TCA */	"S",
	/* TCC */	"S",
	/* TCG */	"S",
	/* TCT */	"S",
	/* TGA */	"X",
	/* TGC */	"C",
	/* TGG */	"W",
	/* TGT */	"C",
	/* TTA */	"L",
	/* TTC */	"F",
	/* TTG */	"L",
	/* TTT */	"F"
};

static std::string codon2aa_long[64] = {
	/* AAA */	"Lys",
	/* AAC */	"Asn",
	/* AAG */	"Lys",
	/* AAT */	"Asn",
	/* ACA */	"Thr",
	/* ACC */	"Thr",
	/* ACG */	"Thr",
	/* ACT */	"Thr",
	/* AGA */	"Arg",
	/* AGC */	"Ser",
	/* AGG */	"Arg",
	/* AGT */	"Ser",
	/* ATA */	"Ile",
	/* ATC */	"Ile",
	/* ATG */	"Met",
	/* ATT */	"Ile",
	/* CAA */	"Gln",
	/* CAC */	"His",
	/* CAG */	"Gln",
	/* CAT */	"His",
	/* CCA */	"Pro",
	/* CCC */	"Pro",
	/* CCG */	"Pro",
	/* CCT */	"Pro",
	/* CGA */	"Arg",
	/* CGC */	"Arg",
	/* CGG */	"Arg",
	/* CGT */	"Arg",
	/* CTA */	"Leu",
	/* CTC */	"Leu",
	/* CTG */	"Leu",
	/* CTT */	"Leu",
	/* GAA */	"Glu",
	/* GAC */	"Asp",
	/* GAG */	"Glu",
	/* GAT */	"Asp",
	/* GCA */	"Ala",
	/* GCC */	"Ala",
	/* GCG */	"Ala",
	/* GCT */	"Ala",
	/* GGA */	"Gly",
	/* GGC */	"Gly",
	/* GGG */	"Gly",
	/* GGT */	"Gly",
	/* GTA */	"Val",
	/* GTC */	"Val",
	/* GTG */	"Val",
	/* GTT */	"Val",
	/* TAA */	"Ter",
	/* TAC */	"Tyr",
	/* TAG */	"Ter",
	/* TAT */	"Tyr",
	/* TCA */	"Ser",
	/* TCC */	"Ser",
	/* TCG */	"Ser",
	/* TCT */	"Ser",
	/* TGA */	"Ter",
	/* TGC */	"Cys",
	/* TGG */	"Trp",
	/* TGT */	"Cys",
	/* TTA */	"Leu",
	/* TTC */	"Phe",
	/* TTG */	"Leu",
	/* TTT */	"Phe"
};

static int codon2aa_int[64] = {
	/* AAA */	12,
	/* AAC */	3,
	/* AAG */	12,
	/* AAT */	3,
	/* ACA */	17,
	/* ACC */	17,
	/* ACG */	17,
	/* ACT */	17,
	/* AGA */	2,
	/* AGC */	16,
	/* AGG */	2,
	/* AGT */	16,
	/* ATA */	10,
	/* ATC */	10,
	/* ATG */	13,
	/* ATT */	10,
	/* CAA */	6,
	/* CAC */	9,
	/* CAG */	6,
	/* CAT */	9,
	/* CCA */	15,
	/* CCC */	15,
	/* CCG */	15,
	/* CCT */	15,
	/* CGA */	2,
	/* CGC */	2,
	/* CGG */	2,
	/* CGT */	2,
	/* CTA */	11,
	/* CTC */	11,
	/* CTG */	11,
	/* CTT */	11,
	/* GAA */	7,
	/* GAC */	4,
	/* GAG */	7,
	/* GAT */	4,
	/* GCA */	1,
	/* GCC */	1,
	/* GCG */	1,
	/* GCT */	1,
	/* GGA */	8,
	/* GGC */	8,
	/* GGG */	8,
	/* GGT */	8,
	/* GTA */	20,
	/* GTC */	20,
	/* GTG */	20,
	/* GTT */	20,
	/* TAA */	0,
	/* TAC */	19,
	/* TAG */	0,
	/* TAT */	19,
	/* TCA */	16,
	/* TCC */	16,
	/* TCG */	16,
	/* TCT */	16,
	/* TGA */	0,
	/* TGC */	5,
	/* TGG */	18,
	/* TGT */	5,
	/* TTA */	11,
	/* TTC */	14,
	/* TTG */	11,
	/* TTT */	14
};


// ************************************************************************************
//
//	nucleotide utilities
//
#pragma mark -
#pragma mark Nucleotide utilities
#pragma mark -

//	(string)codonsToAminoAcids(integer codons, [li$ long = F], [l$ paste = T])
EidosValue_SP SLiM_ExecuteFunction_codonsToAminoAcids(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue *codons_value = p_arguments[0].get();
	EidosValue *long_value = p_arguments[1].get();
	
	int codons_length = codons_value->Count();
	
	bool integer_result = (long_value->Type() == EidosValueType::kValueInt);
	eidos_logical_t long_strings = (integer_result ? false : long_value->LogicalAtIndex_NOCAST(0, nullptr));
	
	if (integer_result)
	{
		int64_t long_intval = long_value->IntAtIndex_NOCAST(0, nullptr);
		
		if (long_intval != 0)
			EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_codonsToAminoAcids): function codonsToAminoAcids() requires 'long' to be T, F, or 0." << EidosTerminate(nullptr);
	}
	
	if (codons_length == 1)
	{
		int64_t codon = codons_value->IntAtIndex_NOCAST(0, nullptr);
		
		if ((codon < 0) || (codon > 63))
			EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_codonsToAminoAcids): function codonsToAminoAcids() requires codons to be in [0, 63]." << EidosTerminate(nullptr);
		
		if (integer_result)
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(codon2aa_int[codon]));
		}
		else
		{
			std::string &aa = (long_strings ? codon2aa_long[codon] : codon2aa_short[codon]);
		
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String(aa));
		}
	}
	else
	{
		const int64_t *int_data = codons_value->IntData();
		
		if (integer_result)
		{
			EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(codons_length);
			
			for (int value_index = 0; value_index < codons_length; ++value_index)
			{
				int64_t codon = int_data[value_index];
				
				if ((codon < 0) || (codon > 63))
					EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_codonsToAminoAcids): function codonsToAminoAcids() requires codons to be in [0, 63]." << EidosTerminate(nullptr);
				
				int_result->set_int_no_check(codon2aa_int[codon], value_index);
			}
			
			return EidosValue_SP(int_result);
		}
		else
		{
			EidosValue *paste_value = p_arguments[2].get();
			eidos_logical_t paste = paste_value->LogicalAtIndex_NOCAST(0, nullptr);
			
			if (paste)
			{
				if (long_strings && (codons_length > 0))
				{
					// pasting: "Aaa-Bbb-Ccc"
					EidosValue_String *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String(""));
					std::string &aa_string = string_result->StringData_Mutable()[0];
					
					aa_string.resize(codons_length * 4 - 1);	// create space for all the amino acids we will generate, including hyphens
					
					char *aa_string_ptr = &aa_string[0];	// data() returns a const pointer, but this is safe in C++11 and later
					
					for (int value_index = 0; value_index < codons_length; ++value_index)
					{
						int64_t codon = int_data[value_index];
						
						if ((codon < 0) || (codon > 63))
							EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_codonsToAminoAcids): function codonsToAminoAcids() requires codons to be in [0, 63]." << EidosTerminate(nullptr);
						
						std::string &aa = codon2aa_long[codon];
						char *codon_aa_ptr = &aa[0];	// data() returns a const pointer, but this is safe in C++11 and later
						
						if (value_index > 0)
							*(aa_string_ptr++) = '-';
						
						*(aa_string_ptr++) = codon_aa_ptr[0];
						*(aa_string_ptr++) = codon_aa_ptr[1];
						*(aa_string_ptr++) = codon_aa_ptr[2];
					}
					
					return EidosValue_SP(string_result);
				}
				else
				{
					// pasting: "ABC"
					EidosValue_String *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String(""));
					std::string &aa_string = string_result->StringData_Mutable()[0];
					
					aa_string.resize(codons_length);	// create space for all the amino acids we will generate
					
					char *aa_string_ptr = &aa_string[0];	// data() returns a const pointer, but this is safe in C++11 and later
					
					for (int value_index = 0; value_index < codons_length; ++value_index)
					{
						int64_t codon = int_data[value_index];
						
						if ((codon < 0) || (codon > 63))
							EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_codonsToAminoAcids): function codonsToAminoAcids() requires codons to be in [0, 63]." << EidosTerminate(nullptr);
						
						std::string &aa = codon2aa_short[codon];
						
						aa_string_ptr[value_index] = aa[0];
					}
					
					return EidosValue_SP(string_result);
				}
			}
			else
			{
				// no pasting: "A" "C" "C" or "Aaa" "Bbb" "Ccc"
				EidosValue_String *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String())->Reserve(codons_length);
				
				for (int value_index = 0; value_index < codons_length; ++value_index)
				{
					int64_t codon = int_data[value_index];
					
					if ((codon < 0) || (codon > 63))
						EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_codonsToAminoAcids): function codonsToAminoAcids() requires codons to be in [0, 63]." << EidosTerminate(nullptr);
					
					std::string &aa = (long_strings ? codon2aa_long[codon] : codon2aa_short[codon]);
					
					string_result->PushString(aa);
				}
				
				return EidosValue_SP(string_result);
			}
		}
	}
}

//	(integer)nucleotidesToCodons(is sequence)
EidosValue_SP SLiM_ExecuteFunction_nucleotidesToCodons(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue *sequence_value = p_arguments[0].get();
	EidosValueType sequence_type = sequence_value->Type();
	int64_t sequence_count = sequence_value->Count();
	
	if (sequence_count == 1)
	{
		if (sequence_type == EidosValueType::kValueString)
		{
			// singleton string case
			uint8_t *nuc_lookup = NucleotideArray::NucleotideCharToIntLookup();
			const std::string &string_ref = sequence_value->StringData()[0];
			int64_t length = (int64_t)string_ref.length();
			
			if (length % 3 != 0)
				EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_nucleotidesToCodons): function nucleotidesToCodons() requires the nucleotide sequence to be a multiple of three in length." << EidosTerminate(nullptr);
			
			int64_t length_3 = length / 3;
			
			EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize((int)length_3);
			
			for (int64_t value_index = 0; value_index < length_3; ++value_index)
			{
				int64_t codon_base = value_index * 3;
				
				int nuc1 = nuc_lookup[(int)(unsigned char)(string_ref[codon_base])];
				int nuc2 = nuc_lookup[(int)(unsigned char)(string_ref[codon_base + 1])];
				int nuc3 = nuc_lookup[(int)(unsigned char)(string_ref[codon_base + 2])];
				
				if ((nuc1 > 3) || (nuc2 > 3) || (nuc3 > 3))
					EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_nucleotidesToCodons): function nucleotidesToCodons() requires string sequence values to be 'A', 'C', 'G', or 'T'." << EidosTerminate(nullptr);
				
				int codon = nuc1 * 16 + nuc2 * 4 + nuc3;	// 0..63
				
				int_result->set_int_no_check(codon, value_index);
			}
			
			return EidosValue_SP(int_result);
		}
		else
			EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_nucleotidesToCodons): function nucleotidesToCodons() requires the nucleotide sequence to be a multiple of three in length." << EidosTerminate(nullptr);
	}
	else
	{
		if (sequence_count % 3 != 0)
			EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_nucleotidesToCodons): function nucleotidesToCodons() requires the nucleotide sequence to be a multiple of three in length." << EidosTerminate(nullptr);
		
		int64_t length_3 = sequence_count / 3;
		
		EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize((int)length_3);
		
		if (sequence_type == EidosValueType::kValueString)
		{
			// string vector case
			uint8_t *nuc_lookup = NucleotideArray::NucleotideCharToIntLookup();
			const std::string *string_vec = sequence_value->StringData();
			
			for (int value_index = 0; value_index < length_3; ++value_index)
			{
				int64_t codon_base = (size_t)value_index * 3;
				
				const std::string &nucstring1 = string_vec[codon_base];
				const std::string &nucstring2 = string_vec[codon_base + 1];
				const std::string &nucstring3 = string_vec[codon_base + 2];
				
				if ((nucstring1.length() != 1) || (nucstring2.length() != 1) || (nucstring3.length() != 1))
					EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_nucleotidesToCodons): function nucleotidesToCodons() requires string sequence values to be 'A', 'C', 'G', or 'T'." << EidosTerminate(nullptr);
				
				int nuc1 = nuc_lookup[(int)(unsigned char)(nucstring1[0])];
				int nuc2 = nuc_lookup[(int)(unsigned char)(nucstring2[0])];
				int nuc3 = nuc_lookup[(int)(unsigned char)(nucstring3[0])];
				
				if ((nuc1 > 3) || (nuc2 > 3) || (nuc3 > 3))
					EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_nucleotidesToCodons): function nucleotidesToCodons() requires string sequence values to be 'A', 'C', 'G', or 'T'." << EidosTerminate(nullptr);
				
				int codon = nuc1 * 16 + nuc2 * 4 + nuc3;	// 0..63
				
				int_result->set_int_no_check(codon, value_index);
			}
		}
		else // sequence_type == EidosValueType::kValueInt
		{
			// int vector case
			const int64_t *int_data = sequence_value->IntData();
			
			for (int value_index = 0; value_index < length_3; ++value_index)
			{
				int64_t codon_base = (size_t)value_index * 3;
				
				int64_t nuc1 = int_data[codon_base];
				int64_t nuc2 = int_data[codon_base + 1];
				int64_t nuc3 = int_data[codon_base + 2];
				
				if ((nuc1 < 0) || (nuc1 > 3) || (nuc2 < 0) || (nuc2 > 3) || (nuc3 < 0) || (nuc3 > 3))
					EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_nucleotidesToCodons): function nucleotidesToCodons() requires integer sequence values to be in [0,3]." << EidosTerminate(nullptr);
				
				int64_t codon = nuc1 * 16 + nuc2 * 4 + nuc3;	// 0..63
				
				int_result->set_int_no_check(codon, value_index);
			}
		}
		
		return EidosValue_SP(int_result);
	}
}

static void CountNucleotides(EidosValue *sequence_value, int64_t *total_ACGT, const char *function_name)
{
	EidosValueType sequence_type = sequence_value->Type();
	int sequence_count = sequence_value->Count();
	
	if (sequence_count == 1)
	{
		// Singleton case
		if (sequence_type == EidosValueType::kValueInt)
		{
			uint64_t nuc = sequence_value->IntAtIndex_NOCAST(0, nullptr);
			
			if (nuc > 3)
				EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_" << function_name << "): function " << function_name << "() requires integer sequence values to be in [0,3]." << EidosTerminate(nullptr);
			
			total_ACGT[nuc]++;
		}
		else // sequence_type == EidosValueType::kValueString
		{
			// Note that this is different from the case below - a single string versus a vector of single characters
			uint8_t *nuc_lookup = NucleotideArray::NucleotideCharToIntLookup();
			const std::string &string_ref = sequence_value->StringData()[0];
			std::size_t length = string_ref.length();
			
			for (std::size_t i = 0; i < length; ++i)
			{
				char nuc_char = string_ref[i];
				uint8_t nuc_index = nuc_lookup[(int)(unsigned char)(nuc_char)];
				
				if (nuc_index > 3)
					EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_" << function_name << "): function " << function_name << "() requires string sequence values to be 'A', 'C', 'G', or 'T'." << EidosTerminate(nullptr);
				
				total_ACGT[nuc_index]++;
			}
		}
	}
	else
	{
		// Vector case, optimized for speed
		if (sequence_type == EidosValueType::kValueInt)
		{
			const int64_t *int_data = sequence_value->IntData();
			
			for (int value_index = 0; value_index < sequence_count; ++value_index)
			{
				uint64_t nuc = int_data[value_index];
				
				if (nuc > 3)
					EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_" << function_name << "): function " << function_name << "() requires integer sequence values to be in [0,3]." << EidosTerminate(nullptr);
				
				total_ACGT[nuc]++;
			}
		}
		else // sequence_type == EidosValueType::kValueString
		{
			uint8_t *nuc_lookup = NucleotideArray::NucleotideCharToIntLookup();
			const std::string *string_vec = sequence_value->StringData();
			
			for (int value_index = 0; value_index < sequence_count; ++value_index)
			{
				const std::string &nuc_string = string_vec[value_index];
				
				if (nuc_string.length() != 1)
					EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_" << function_name << "): function " << function_name << "() requires string sequence values to be 'A', 'C', 'G', or 'T'." << EidosTerminate(nullptr);
				
				char nuc_char = nuc_string[0];
				uint8_t nuc_index = nuc_lookup[(int)(unsigned char)(nuc_char)];
				
				if (nuc_index > 3)
					EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_" << function_name << "): function " << function_name << "() requires string sequence values to be 'A', 'C', 'G', or 'T'." << EidosTerminate(nullptr);
				
				total_ACGT[nuc_index]++;
			}
		}
	}
}

//	(float)mm16To256(float mutationMatrix16)
EidosValue_SP SLiM_ExecuteFunction_mm16To256(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue *mutationMatrix16_value = p_arguments[0].get();
	
	if (mutationMatrix16_value->Count() != 16)
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_mm16To256): function mm16To256() requires mutationMatrix16 to be of length 16." << EidosTerminate(nullptr);
	if ((mutationMatrix16_value->DimensionCount() != 2) || (mutationMatrix16_value->Dimensions()[0] != 4) || (mutationMatrix16_value->Dimensions()[1] != 4))
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_mm16To256): function mm16To256() requires mutationMatrix16 to be a 4x4 matrix." << EidosTerminate(nullptr);
	
	EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->resize_no_initialize(256);
	
	for (int i = 0; i < 256; ++i)
	{
		int ancestral_nucleotide = ((i / 4) % 4);
		int derived_nucleotide = (i / 64);
		double value = mutationMatrix16_value->FloatAtIndex_NOCAST(ancestral_nucleotide + derived_nucleotide * 4, nullptr);
		
		float_result->set_float_no_check(value, i);
	}
	
	const int64_t dims[2] = {64, 4};
	float_result->SetDimensions(2, dims);
	
	return EidosValue_SP(float_result);
}

//	(float)mmJukesCantor(float$ alpha)
EidosValue_SP SLiM_ExecuteFunction_mmJukesCantor(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue *alpha_value = p_arguments[0].get();
	double alpha = alpha_value->FloatAtIndex_NOCAST(0, nullptr);
	
	if (alpha < 0.0)
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_mmJukesCantor): function mmJukesCantor() requires alpha >= 0.0." << EidosTerminate(nullptr);
	if (3 * alpha > 1.0)
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_mmJukesCantor): function mmJukesCantor() requires 3 * alpha <= 1.0." << EidosTerminate(nullptr);
	
	EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->resize_no_initialize(16);
	
	float_result->set_float_no_check(0.0, 0);
	float_result->set_float_no_check(alpha, 1);
	float_result->set_float_no_check(alpha, 2);
	float_result->set_float_no_check(alpha, 3);
	
	float_result->set_float_no_check(alpha, 4);
	float_result->set_float_no_check(0.0, 5);
	float_result->set_float_no_check(alpha, 6);
	float_result->set_float_no_check(alpha, 7);
	
	float_result->set_float_no_check(alpha, 8);
	float_result->set_float_no_check(alpha, 9);
	float_result->set_float_no_check(0.0, 10);
	float_result->set_float_no_check(alpha, 11);
	
	float_result->set_float_no_check(alpha, 12);
	float_result->set_float_no_check(alpha, 13);
	float_result->set_float_no_check(alpha, 14);
	float_result->set_float_no_check(0.0, 15);
	
	const int64_t dims[2] = {4, 4};
	float_result->SetDimensions(2, dims);
	
	return EidosValue_SP(float_result);
}

//	(float)mmKimura(float$ alpha, float$ beta)
EidosValue_SP SLiM_ExecuteFunction_mmKimura(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue *alpha_value = p_arguments[0].get();
	EidosValue *beta_value = p_arguments[1].get();
	
	double alpha = alpha_value->FloatAtIndex_NOCAST(0, nullptr);
	double beta = beta_value->FloatAtIndex_NOCAST(0, nullptr);
	
	if ((alpha < 0.0) || (alpha > 1.0))
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_mmKimura): function mmKimura() requires alpha to be in [0.0, 1.0]." << EidosTerminate(nullptr);
	if ((beta < 0.0) || (beta > 0.5))
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_mmKimura): function mmKimura() requires beta to be in [0.0, 0.5]." << EidosTerminate(nullptr);
	if (alpha + 2 * beta > 1.0)
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_mmKimura): function mmKimura() requires alpha + 2 * beta to be <= 1.0." << EidosTerminate(nullptr);
	
	EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->resize_no_initialize(16);
	
	float_result->set_float_no_check(0.0, 0);
	float_result->set_float_no_check(beta, 1);
	float_result->set_float_no_check(alpha, 2);
	float_result->set_float_no_check(beta, 3);
	
	float_result->set_float_no_check(beta, 4);
	float_result->set_float_no_check(0.0, 5);
	float_result->set_float_no_check(beta, 6);
	float_result->set_float_no_check(alpha, 7);
	
	float_result->set_float_no_check(alpha, 8);
	float_result->set_float_no_check(beta, 9);
	float_result->set_float_no_check(0.0, 10);
	float_result->set_float_no_check(beta, 11);
	
	float_result->set_float_no_check(beta, 12);
	float_result->set_float_no_check(alpha, 13);
	float_result->set_float_no_check(beta, 14);
	float_result->set_float_no_check(0.0, 15);
	
	const int64_t dims[2] = {4, 4};
	float_result->SetDimensions(2, dims);
	
	return EidosValue_SP(float_result);
}

//	(integer)nucleotideCounts(is sequence)
EidosValue_SP SLiM_ExecuteFunction_nucleotideCounts(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue *sequence_value = p_arguments[0].get();
	int64_t total_ACGT[4] = {0, 0, 0, 0};
	
	CountNucleotides(sequence_value, total_ACGT, "nucleotideCounts");
	
	EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(4);
	
	int_result->set_int_no_check(total_ACGT[0], 0);
	int_result->set_int_no_check(total_ACGT[1], 1);
	int_result->set_int_no_check(total_ACGT[2], 2);
	int_result->set_int_no_check(total_ACGT[3], 3);
	
	return EidosValue_SP(int_result);
}

//	(float)nucleotideFrequencies(is sequence)
EidosValue_SP SLiM_ExecuteFunction_nucleotideFrequencies(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue *sequence_value = p_arguments[0].get();
	int64_t total_ACGT[4] = {0, 0, 0, 0};
	
	CountNucleotides(sequence_value, total_ACGT, "nucleotideFrequencies");
	
	EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->resize_no_initialize(4);
	double total = total_ACGT[0] + total_ACGT[1] + total_ACGT[2] + total_ACGT[3];
	
	float_result->set_float_no_check(total_ACGT[0] / total, 0);
	float_result->set_float_no_check(total_ACGT[1] / total, 1);
	float_result->set_float_no_check(total_ACGT[2] / total, 2);
	float_result->set_float_no_check(total_ACGT[3] / total, 3);
	
	return EidosValue_SP(float_result);
}

//	(is)randomNucleotides(i$ length, [Nif basis = NULL], [s$ format = "string"])
EidosValue_SP SLiM_ExecuteFunction_randomNucleotides(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *length_value = p_arguments[0].get();
	EidosValue *basis_value = p_arguments[1].get();
	EidosValue_String *format_value = (EidosValue_String *)p_arguments[2].get();
	
	// Get the sequence length to generate
	int64_t length = length_value->IntAtIndex_NOCAST(0, nullptr);
	
	if ((length < 0) || (length > 2000000000L))
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_randomNucleotides): function randomNucleotides() requires length to be in [0, 2e9]." << EidosTerminate(nullptr);
	
	// Figure out the probability threshold for each base
	double pA = 0.25, pC = 0.25, pG = 0.25, pT; // = 0.25;	// not used below
	
	if (basis_value->Type() != EidosValueType::kValueNULL)
	{
		if (basis_value->Count() != 4)
			EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_randomNucleotides): function randomNucleotides() requires basis to be either NULL, or an integer or float vector of length 4 (with relative probabilities for A/C/G/T)." << EidosTerminate(nullptr);
		
		pA = basis_value->NumericAtIndex_NOCAST(0, nullptr);
		pC = basis_value->NumericAtIndex_NOCAST(1, nullptr);
		pG = basis_value->NumericAtIndex_NOCAST(2, nullptr);
		pT = basis_value->NumericAtIndex_NOCAST(3, nullptr);
		
		if (!std::isfinite(pA) || !std::isfinite(pC) || !std::isfinite(pG) || !std::isfinite(pT) || (pA < 0.0) || (pC < 0.0) || (pG < 0.0) || (pT < 0.0))
			EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_randomNucleotides): function randomNucleotides() requires basis values to be finite and >= 0.0." << EidosTerminate(nullptr);
		
		double sum = pA + pC + pG + pT;
		
		if (sum <= 0.0)
			EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_randomNucleotides): function randomNucleotides() requires at least one basis value to be > 0.0." << EidosTerminate(nullptr);
		
		// Normalize to probabilities
		pA = pA / sum;
		pC = pC / sum;
		pG = pG / sum;
		//pT = pT / sum;		// not used below since it will end up as 1.0 below
	}
	
	// Convert probabilities to thresholds
	//pT += pA + pC + pG;		// should be 1.0; not used
	pG += pA + pC;
	pC += pA;
	
	// Generate a result in the requested format
	const std::string &format = format_value->StringRefAtIndex_NOCAST(0, nullptr);
	
	if ((format != "string") && (format != "char") && (format != "integer"))
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_randomNucleotides): function randomNucleotides() requires a format of 'string', 'char', or 'integer'." << EidosTerminate();
	
	if (length == 0)
	{
		if (format == "integer")	return gStaticEidosValue_Integer_ZeroVec;
		else						return gStaticEidosValue_String_ZeroVec;
	}
	
	gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
	
	if (length == 1)
	{
		// Handle the singleton case separately for speed
		double runif = Eidos_rng_uniform(rng);
		
		if (format == "integer")
		{
			if (runif < pA)			return gStaticEidosValue_Integer0;
			else if (runif < pC)	return gStaticEidosValue_Integer1;
			else if (runif < pG)	return gStaticEidosValue_Integer2;
			else /* (runif < pT) */	return gStaticEidosValue_Integer3;
		}
		else	// "string", "char"
		{
			if (runif < pA)			return gStaticEidosValue_StringA;
			else if (runif < pC)	return gStaticEidosValue_StringC;
			else if (runif < pG)	return gStaticEidosValue_StringG;
			else /* (runif < pT) */	return gStaticEidosValue_StringT;
		}
	}
	
	if (format == "char")
	{
		// return a vector of one-character strings, "T" "A" "T" "A"
		EidosValue_String *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String())->Reserve((int)length);
		
		for (int value_index = 0; value_index < length; ++value_index)
		{
			double runif = Eidos_rng_uniform(rng);
			
			if (runif < pA)			string_result->PushString(gStr_A);
			else if (runif < pC)	string_result->PushString(gStr_C);
			else if (runif < pG)	string_result->PushString(gStr_G);
			else /* (runif < pT) */	string_result->PushString(gStr_T);
		}
		
		return EidosValue_SP(string_result);
	}
	else if (format == "integer")
	{
		// return a vector of integers, 3 0 3 0
		EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize((int)length);
		
		for (int value_index = 0; value_index < length; ++value_index)
		{
			double runif = Eidos_rng_uniform(rng);
			
			if (runif < pA)			int_result->set_int_no_check(0, value_index);
			else if (runif < pC)	int_result->set_int_no_check(1, value_index);
			else if (runif < pG)	int_result->set_int_no_check(2, value_index);
			else /* (runif < pT) */	int_result->set_int_no_check(3, value_index);
		}
		
		return EidosValue_SP(int_result);
	}
	else if (format == "string")
	{
		// return a singleton string for the whole sequence, "TATA"; we munge the std::string inside the EidosValue to avoid memory copying, very naughty
		EidosValue_String *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String(""));
		std::string &nuc_string = string_result->StringData_Mutable()[0];
		
		nuc_string.resize(length);	// create space for all the nucleotides we will generate
		
		char *nuc_string_ptr = &nuc_string[0];	// data() returns a const pointer, but this is safe in C++11 and later
		
		for (int value_index = 0; value_index < length; ++value_index)
		{
			double runif = Eidos_rng_uniform(rng);
			
			if (runif < pA)			nuc_string_ptr[value_index] = 'A';
			else if (runif < pC)	nuc_string_ptr[value_index] = 'C';
			else if (runif < pG)	nuc_string_ptr[value_index] = 'G';
			else /* (runif < pT) */	nuc_string_ptr[value_index] = 'T';
		}
		
		return EidosValue_SP(string_result);
	}
	
	// CODE COVERAGE: This is dead code
	return result_SP;
}

// (is)codonsToNucleotides(integer codons, [string$ format = "string"])
EidosValue_SP SLiM_ExecuteFunction_codonsToNucleotides(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue *codons_value = p_arguments[0].get();
	EidosValue_String *format_value = (EidosValue_String *)p_arguments[1].get();
	
	int codons_length = codons_value->Count();
	int length = codons_length * 3;
	const std::string &format = format_value->StringRefAtIndex_NOCAST(0, nullptr);
	const int64_t *codons_data = codons_value->IntData();
	
	if (format == "char")
	{
		// return a vector of one-character strings, "T" "A" "T" "A" "C" "G"
		EidosValue_String *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String())->Reserve((int)length);
		
		for (int codon_index = 0; codon_index < codons_length; ++codon_index)
		{
			int codon = (int)codons_data[codon_index];
			
			if ((codon < 0) || (codon > 63))
				EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_codonsToNucleotides): function codonsToNucleotides() requires codon values to be in [0,63]." << EidosTerminate();
			
			int nuc1 = codon >> 4;
			int nuc2 = (codon >> 2) & 0x03;
			int nuc3 = codon & 0x03;
			
			switch (nuc1) {
				case 0: string_result->PushString(gStr_A); break;
				case 1: string_result->PushString(gStr_C); break;
				case 2: string_result->PushString(gStr_G); break;
				case 3: string_result->PushString(gStr_T); break;
				default:
					EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_codonsToNucleotides): nucleotide value out of range." << EidosTerminate();
			}
			switch (nuc2) {
				case 0: string_result->PushString(gStr_A); break;
				case 1: string_result->PushString(gStr_C); break;
				case 2: string_result->PushString(gStr_G); break;
				case 3: string_result->PushString(gStr_T); break;
				default:
					EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_codonsToNucleotides): nucleotide value out of range." << EidosTerminate();
			}
			switch (nuc3) {
				case 0: string_result->PushString(gStr_A); break;
				case 1: string_result->PushString(gStr_C); break;
				case 2: string_result->PushString(gStr_G); break;
				case 3: string_result->PushString(gStr_T); break;
				default:
					EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_codonsToNucleotides): nucleotide value out of range." << EidosTerminate();
			}
		}
		
		return EidosValue_SP(string_result);
	}
	else if (format == "integer")
	{
		// return a vector of integers, 3 0 3 0 1 2
		EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize((int)length);
		
		for (int codon_index = 0; codon_index < codons_length; ++codon_index)
		{
			int codon = (int)codons_data[codon_index];
			
			if ((codon < 0) || (codon > 63))
				EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_codonsToNucleotides): function codonsToNucleotides() requires codon values to be in [0,63]." << EidosTerminate();
			
			int nuc1 = codon >> 4;
			int nuc2 = (codon >> 2) & 0x03;
			int nuc3 = codon & 0x03;
			
			int_result->set_int_no_check(nuc1, (size_t)codon_index * 3);
			int_result->set_int_no_check(nuc2, (size_t)codon_index * 3 + 1);
			int_result->set_int_no_check(nuc3, (size_t)codon_index * 3 + 2);
		}
		
		return EidosValue_SP(int_result);
	}
	else if (format == "string")
	{
		// return a singleton string for the whole sequence, "TATACG"; we munge the std::string inside the EidosValue to avoid memory copying, very naughty
		EidosValue_String *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String(""));
		std::string &nuc_string = string_result->StringData_Mutable()[0];
		
		nuc_string.resize(length);	// create space for all the nucleotides we will generate
		
		char *nuc_string_ptr = &nuc_string[0];	// data() returns a const pointer, but this is safe in C++11 and later
		
		for (int codon_index = 0; codon_index < codons_length; ++codon_index)
		{
			int codon = (int)codons_data[codon_index];
			
			if ((codon < 0) || (codon > 63))
				EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_codonsToNucleotides): function codonsToNucleotides() requires codon values to be in [0,63]." << EidosTerminate();
			
			int nuc1 = codon >> 4;
			int nuc2 = (codon >> 2) & 0x03;
			int nuc3 = codon & 0x03;
			
			nuc_string_ptr[(size_t)codon_index * 3] = gSLiM_Nucleotides[nuc1];
			nuc_string_ptr[(size_t)codon_index * 3 + 1] = gSLiM_Nucleotides[nuc2];
			nuc_string_ptr[(size_t)codon_index * 3 + 2] = gSLiM_Nucleotides[nuc3];
		}
		
		return EidosValue_SP(string_result);
	}
	else
	{
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_codonsToNucleotides): function codonsToNucleotides() requires a format of 'string', 'char', or 'integer'." << EidosTerminate();
	}
}


// ************************************************************************************
//
//	other functions
//
#pragma mark -
#pragma mark Other functions
#pragma mark -

static inline int SummarizeGridIndex_1D(Individual *individual, int component0, double *spatialBounds, int64_t *dims, __attribute__((unused)) int64_t result_count)
{
	double coord0 = (&(individual->spatial_x_))[component0];
	
	if ((coord0 < spatialBounds[0]) || (coord0 > spatialBounds[1]))
		return -1;	// a flag value that indicates "individual out of bounds"
	
	// figure out which grid cell this individual is in
	int grid0 = (int)round(((coord0 - spatialBounds[0]) / (spatialBounds[1] - spatialBounds[0])) * (dims[0] - 1));
	
#if DEBUG
	if ((grid0 < 0) || (grid0 >= dims[0]))
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_summarizeIndividuals): (internal error) grid coordinates out of bounds." << EidosTerminate();
#endif
	
	int grid_index = (int)grid0;	// this is the index in tallies, result_data
	
#if DEBUG
	if ((grid_index < 0) || (grid_index >= result_count))
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_summarizeIndividuals): (internal error) grid index out of bounds." << EidosTerminate();
#endif
	
	return grid_index;
}

static inline int SummarizeGridIndex_2D(Individual *individual, int component0, int component1, double *spatialBounds, int64_t *dims, __attribute__((unused)) int64_t result_count)
{
	double coord0 = (&(individual->spatial_x_))[component0];	// x, for "xy"
	double coord1 = (&(individual->spatial_x_))[component1];	// y, for "xy"
	
	if ((coord0 < spatialBounds[0]) || (coord0 > spatialBounds[1]) || (coord1 < spatialBounds[2]) || (coord1 > spatialBounds[3]))
		return -1;	// a flag value that indicates "individual out of bounds"
	
	// figure out which grid cell this individual is in
	int grid0 = (int)round(((coord0 - spatialBounds[0]) / (spatialBounds[1] - spatialBounds[0])) * (dims[1] - 1));	// x index, for "xy"
	int grid1 = (int)round(((coord1 - spatialBounds[2]) / (spatialBounds[3] - spatialBounds[2])) * (dims[0] - 1));	// y index, for "xy"
	
#if DEBUG
	if (((grid0 < 0) || (grid0 >= dims[1])) || ((grid1 < 0) || (grid1 >= dims[0])))		// dims[0] is row count, dims[1] is col count
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_summarizeIndividuals): (internal error) grid coordinates out of bounds." << EidosTerminate();
#endif
	
	int grid_index = (int)(grid0 * dims[0] + dims[0] - 1 - grid1);	// this is the index in tallies, result_data; x * row_count + y, by row, for "xy"
	
#if DEBUG
	if ((grid_index < 0) || (grid_index >= result_count))
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_summarizeIndividuals): (internal error) grid index out of bounds." << EidosTerminate();
#endif
	
	return grid_index;
}

static inline int SummarizeGridIndex_3D(Individual *individual, int component0, int component1, int component2, double *spatialBounds, int64_t *dims, __attribute__((unused)) int64_t result_count)
{
	double coord0 = (&(individual->spatial_x_))[component0];	// x, for "xyz"
	double coord1 = (&(individual->spatial_x_))[component1];	// y, for "xyz"
	double coord2 = (&(individual->spatial_x_))[component2];	// z, for "xyz"
	
	if ((coord0 < spatialBounds[0]) || (coord0 > spatialBounds[1]) || (coord1 < spatialBounds[2]) || (coord1 > spatialBounds[3]) || (coord2 < spatialBounds[4]) || (coord2 > spatialBounds[5]))
		return -1;	// a flag value that indicates "individual out of bounds"
	
	// figure out which grid cell this individual is in
	int grid0 = (int)round(((coord0 - spatialBounds[0]) / (spatialBounds[1] - spatialBounds[0])) * (dims[1] - 1));	// x index, for "xyz"
	int grid1 = (int)round(((coord1 - spatialBounds[2]) / (spatialBounds[3] - spatialBounds[2])) * (dims[0] - 1));	// y index, for "xyz"
	int grid2 = (int)round(((coord2 - spatialBounds[4]) / (spatialBounds[5] - spatialBounds[4])) * (dims[2] - 1));	// z index, for "xyz"
	
#if DEBUG
	if (((grid0 < 0) || (grid0 >= dims[1])) || ((grid1 < 0) || (grid1 >= dims[0])) || ((grid2 < 0) || (grid2 >= dims[2])))		// dims[0] is row count, dims[1] is col count
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_summarizeIndividuals): (internal error) grid coordinates out of bounds." << EidosTerminate();
#endif
	
	int grid_index = (int)(grid0 * dims[0] + dims[0] - 1 - grid1 + grid2 * dims[0] * dims[1]);	// this is the index in tallies, result_data; x * row_count + y + z * cow_count * col_count, by row, for "xy"
	
#if DEBUG
	if ((grid_index < 0) || (grid_index >= result_count))
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_summarizeIndividuals): (internal error) grid index out of bounds." << EidosTerminate();
#endif
	
	return grid_index;
}

// (float)summarizeIndividuals(o<Individual> individuals, integer dim, numeric spatialBounds, s$ operation, [Nlif$ empty = 0.0], [l$ perUnitArea = F], [Ns$ spatiality = NULL])
EidosValue_SP SLiM_ExecuteFunction_summarizeIndividuals(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue *individuals_value = p_arguments[0].get();
	EidosValue *dim_value = p_arguments[1].get();
	EidosValue *spatialBounds_value = p_arguments[2].get();
	EidosValue *operation_value = p_arguments[3].get();
	EidosValue *empty_value = p_arguments[4].get();
	EidosValue *perUnitArea_value = p_arguments[5].get();
	EidosValue *spatiality_value = p_arguments[6].get();
	
	// Get individuals vector; complicated as usual by singleton vs. vector
	int individuals_count = individuals_value->Count();
	
	if (individuals_count == 0)
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_summarizeIndividuals): summarizeIndividuals() cannot be called with a zero-length individuals vector, because the focal species, and thus the spatial dimensionality, cannot be determined." << EidosTerminate();
	
	Individual **individuals_buffer = (Individual **)individuals_value->ObjectData();
	
	// This very weird code tests that the layout of ivars inside Individual is what we expect it to be below
	// We use the first individual in the buffer as a test subject, rather than nullptr, to make UBSan happy
	static bool beenHere = false;
	
	if (!beenHere)
	{
		THREAD_SAFETY_IN_ACTIVE_PARALLEL("SLiM_ExecuteFunction_summarizeIndividuals(): usage of statics");
		
		Individual *test_ind_layout = individuals_buffer[0];
	
		if (((&(test_ind_layout->spatial_x_)) + 1 != (&(test_ind_layout->spatial_y_))) ||
			((&(test_ind_layout->spatial_x_)) + 2 != (&(test_ind_layout->spatial_z_))))
			EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_summarizeIndividuals): (internal error) Individual ivar layout unexpected." << EidosTerminate();
		
		beenHere = true;
	}
	
	// SPECIES CONSISTENCY CHECK
	Species *species = Community::SpeciesForIndividuals(individuals_value);
	
	if (!species)
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_summarizeIndividuals): summarizeIndividuals() requires that all individuals belong to the same species." << EidosTerminate();
	
	// Get the model's dimensionality, which will be context for everything we do below
	Community &community = species->community_;
	int spatial_dimensionality = species->SpatialDimensionality();
	
	if (spatial_dimensionality <= 0)
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_summarizeIndividuals): summarizeIndividuals() can only be called in spatial models, since it summarizes spatially-partitioned information." << EidosTerminate();
	
	// Get our spatiality and interpret it
	int spatiality, required_dimensionality;
	int component0 = -1, component1 = -1, component2 = -1;
	
	if (spatiality_value->Type() == EidosValueType::kValueNULL)
	{
		spatiality = spatial_dimensionality;
		required_dimensionality = spatial_dimensionality;
		
		if (spatiality >= 1)
			component0 = 0;
		if (spatiality >= 2)
			component1 = 1;
		if (spatiality >= 3)
			component2 = 2;
	}
	else
	{
		std::string spatiality_string = spatiality_value->StringAtIndex_NOCAST(0, nullptr);
		
		if (spatiality_string.compare(gEidosStr_x) == 0)		{ spatiality = 1; required_dimensionality = 1; component0 = 0; }
		else if (spatiality_string.compare(gEidosStr_y) == 0)	{ spatiality = 1; required_dimensionality = 2; component0 = 1; }
		else if (spatiality_string.compare(gEidosStr_z) == 0)	{ spatiality = 1; required_dimensionality = 3; component0 = 2; }
		else if (spatiality_string.compare("xy") == 0)			{ spatiality = 2; required_dimensionality = 2; component0 = 0; component1 = 1; }
		else if (spatiality_string.compare("xz") == 0)			{ spatiality = 2; required_dimensionality = 3; component0 = 0; component1 = 2; }
		else if (spatiality_string.compare("yz") == 0)			{ spatiality = 2; required_dimensionality = 3; component0 = 1; component1 = 2; }
		else if (spatiality_string.compare("xyz") == 0)			{ spatiality = 3; required_dimensionality = 3; component0 = 0; component1 = 1; component2 = 2; }
		else
			EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_summarizeIndividuals): summarizeIndividuals() spatiality '" << spatiality_string << "' must be 'x', 'y', 'z', 'xy', 'xz', 'yz', or 'xyz'." << EidosTerminate();
	}
	
	if (required_dimensionality > spatial_dimensionality)
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_summarizeIndividuals): summarizeIndividuals() spatiality cannot utilize spatial dimensions beyond those set in initializeSLiMOptions()." << EidosTerminate();
	
	if ((spatiality != 1) && (spatiality != 2) && (spatiality != 3))
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_summarizeIndividuals): (internal error) unexpected spatiality " << spatiality << "." << EidosTerminate();
	
	// Get the spatial bounds and check that it matches the model dimensionality; note that we rearrange the order of the bounds vector here!
	int spatialBounds_count = spatialBounds_value->Count();
	double spatialBounds[6] = {-1, -1, -1, -1, -1, -1};
	bool invalid_bounds = false;
	
	if (spatialBounds_count != spatial_dimensionality * 2)
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_summarizeIndividuals): summarizeIndividuals() spatialBounds is an unexpected length.  It must be supplied in the model's dimensionality (as from the spatialBounds property of a Subpopulation)." << EidosTerminate();
	
	if (spatiality >= 1)
	{
		spatialBounds[0] = spatialBounds_value->NumericAtIndex_NOCAST(component0, nullptr);
		spatialBounds[1] = spatialBounds_value->NumericAtIndex_NOCAST(component0 + spatiality, nullptr);
		if (spatialBounds[0] >= spatialBounds[1])
			invalid_bounds = true;
	}
	if (spatiality >= 2)
	{
		spatialBounds[2] = spatialBounds_value->NumericAtIndex_NOCAST(component1, nullptr);
		spatialBounds[3] = spatialBounds_value->NumericAtIndex_NOCAST(component1 + spatiality, nullptr);
		if (spatialBounds[2] >= spatialBounds[3])
			invalid_bounds = true;
	}
	if (spatiality >= 3)
	{
		spatialBounds[4] = spatialBounds_value->NumericAtIndex_NOCAST(component2, nullptr);
		spatialBounds[5] = spatialBounds_value->NumericAtIndex_NOCAST(component2 + spatiality, nullptr);
		if (spatialBounds[4] >= spatialBounds[5])
			invalid_bounds = true;
	}
	
	if (invalid_bounds)
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_summarizeIndividuals): summarizeIndividuals() spatialBounds are invalid; it is required that x0 < x1, y0 < y1, and z0 < z1." << EidosTerminate();
	
	// Get the operation lambda string and the empty-cell value (NULL to execute the lambda for empty cells too)
	std::string operation_string = operation_value->StringAtIndex_NOCAST(0, nullptr);
	bool uses_empty = (empty_value->Type() != EidosValueType::kValueNULL);
	double empty = uses_empty ? empty_value->FloatAtIndex_CAST(0, nullptr) : 0.0;	// handles logical, integer, and float
	
	// Get the edgeScale value, which is used to postprocess vaues at the very end
	bool perUnitArea = perUnitArea_value->LogicalAtIndex_NOCAST(0, nullptr);
	
	if (perUnitArea && std::isfinite(empty) && (empty != 0.0))
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_summarizeIndividuals): summarizeIndividuals() requires that when perUnitArea is T, empty is F, 0, 0.0, INF, -INF, or NAN (so that the empty value does not get scaled, which presumably does not make sense)." << EidosTerminate();
	
	// Get our dimensions, for our returned vector/matrix/array
	int dim_count = dim_value->Count();
	
	if (dim_count != spatiality)
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_summarizeIndividuals): summarizeIndividuals() spatiality does not match the number of dimensions in dim; " << spatiality << " dimension(s) expected based on spatiality." << EidosTerminate();
	
	int64_t dims[3] = {0, 0, 0};
	int64_t result_count = 1;
	
	for (int dim_index = 0; dim_index < dim_count; ++dim_index)
	{
		dims[dim_index] = dim_value->IntAtIndex_NOCAST(dim_index, nullptr);
		
		if ((dims[dim_index] < 2) || (dims[dim_index] > 10000))
			EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_summarizeIndividuals): summarizeIndividuals() requires dimensions in dim to be in the range [2, 10000]." << EidosTerminate();
		
		result_count *= dims[dim_index];
	}
	
	if ((result_count <= 0) || (result_count >= INT32_MAX))
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_summarizeIndividuals): (internal error) calculated size for returned vector (" << result_count << ") is out of range for int32_t." << EidosTerminate();
	
	// Allocate our return value, set its dimensions, and get set up for using it
	EidosValue_Float *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->resize_no_initialize(result_count);
	double *result_data = result_vec->data_mutable();
	
	if (dim_count > 1)
		result_vec->SetDimensions(dim_count, dims);
	
	// Collect individuals into bins, then execute the operation on each bin
	std::vector<std::vector<Individual *>> binned_individuals;
	binned_individuals.resize(result_count);
	
	if (spatiality == 1)
	{
		for (int individual_index = 0; individual_index < individuals_count; ++individual_index)
		{
			Individual *individual = individuals_buffer[individual_index];
			int grid_index = SummarizeGridIndex_1D(individual, component0, spatialBounds, dims, result_count);
			if (grid_index >= 0)
				binned_individuals[grid_index].emplace_back(individual);
		}
	}
	else if (spatiality == 2)
	{
		for (int individual_index = 0; individual_index < individuals_count; ++individual_index)
		{
			Individual *individual = individuals_buffer[individual_index];
			int grid_index = SummarizeGridIndex_2D(individual, component0, component1, spatialBounds, dims, result_count);
			if (grid_index >= 0)
				binned_individuals[grid_index].emplace_back(individual);
		}
	}
	else // (spatiality == 3)
	{
		for (int individual_index = 0; individual_index < individuals_count; ++individual_index)
		{
			Individual *individual = individuals_buffer[individual_index];
			int grid_index = SummarizeGridIndex_3D(individual, component0, component1, component2, spatialBounds, dims, result_count);
			if (grid_index >= 0)
				binned_individuals[grid_index].emplace_back(individual);
		}
	}
	
	// Now we handle some special-case situations that we anticipate being common.  We have to have an *exact* match to use one of these.
	// I haven't implemented many of these for now, because actually this function is quite fast anyway; since the lambda gets called
	// only once per grid square, and then typically uses vectorized calls to do its work, it is actually quite an efficient design.
	// Even the optimizations here probably only pay off when the number of grid cells is very large and the number of individuals is small.
	// A better optimization would avoid building the binned_individuals vector at all, for simple cases like these where the result
	// could be accumulated directly into the result_data vector; but I'll wait until I see a model where this is a real hotspot.
	if (((operation_string == "individuals.size();") || (operation_string == "individuals.length();") || (operation_string == "size(individuals);") || (operation_string == "length(individuals);") || (operation_string == "return individuals.size();") || (operation_string == "return individuals.length();") || (operation_string == "return size(individuals);") || (operation_string == "return length(individuals);")) && (!uses_empty || (empty == 0.0)))
	{
		// simple abundance: a count of the individuals in each cell
		for (int64_t bin_index = 0; bin_index < result_count; ++bin_index)
		{
			std::vector<Individual *> &individuals = binned_individuals[bin_index];
			
			result_data[bin_index] = individuals.size();
		}
	}
	else if ((((operation_string == "1.0;") || (operation_string == "1;") || (operation_string == "T;")) && uses_empty && (empty == 0.0)))
	{
		// simple presence/absence: 1.0 if individuals are present, 0.0 otherwise
		for (int64_t bin_index = 0; bin_index < result_count; ++bin_index)
		{
			std::vector<Individual *> &individuals = binned_individuals[bin_index];
			
			result_data[bin_index] = ((individuals.size() == 0) ? 0.0 : 1.0);
		}
	}
	else
	{
		// run the lambda on each bin, which does not depend upon the spatiality
		THREAD_SAFETY_IN_ACTIVE_PARALLEL("SLiM_ExecuteFunction_summarizeIndividuals(): running Eidos lambda");
		
		EidosValue_String *lambda_value_singleton = (EidosValue_String *)operation_value;
		EidosScript *script = lambda_value_singleton->CachedScript();
		
		// Errors in lambdas should be reported for the lambda script, not for the calling script,
		// if possible.  In the GUI this does not work well, however; there, errors should be
		// reported as occurring in the call to sapply().  Here we save off the current
		// error context and set up the error context for reporting errors inside the lambda,
		// in case that is possible; see how exceptions are handled below.
		EidosErrorContext error_context_save = gEidosErrorContext;
		
		// We try to do tokenization and parsing once per script, by caching the script inside the EidosValue_String_singleton instance
		if (!script)
		{
			script = new EidosScript(operation_string);
			
			gEidosErrorContext = EidosErrorContext{{-1, -1, -1, -1}, script};
			
			try
			{
				script->Tokenize();
				script->ParseInterpreterBlockToAST(false);
			}
			catch (...)
			{
				if (gEidosTerminateThrows)
				{
					gEidosErrorContext = error_context_save;
					TranslateErrorContextToUserScript("SLiM_ExecuteFunction_summarizeIndividuals()");
				}
				
				delete script;
				
				throw;
			}
			
			if (lambda_value_singleton)
				lambda_value_singleton->SetCachedScript(script);
		}
		
		// Execute inside try/catch so we can handle errors well
		gEidosErrorContext = EidosErrorContext{{-1, -1, -1, -1}, script};
		
		EidosValue_Object individuals_vec(gSLiM_Individual_Class);
		individuals_vec.StackAllocated();
		
		try
		{
			EidosSymbolTable &interpreter_symbols = p_interpreter.SymbolTable();						// use our own symbol table
			EidosSymbolTable constants(EidosSymbolTableType::kContextConstantsTable, &interpreter_symbols);
			EidosSymbolTable symbols(EidosSymbolTableType::kLocalVariablesTable, &constants);	// add a variables symbol table on top, shared across all invocations
			EidosFunctionMap &function_map = p_interpreter.FunctionMap();								// use our own function map
			EidosInterpreter interpreter(*script, symbols, function_map, &community, p_interpreter.ExecutionOutputStream(), p_interpreter.ErrorOutputStream());
			
			// We set up a "constant" value for `individuals` that refers to the stack-allocated object vector made above
			// For each grid cell we will munge the contents of that vector, without having to touch the symbol table again
			constants.InitializeConstantSymbolEntry(gID_individuals, EidosValue_SP(&individuals_vec));
			
			// go through the individuals and tally them
			for (int64_t bin_index = 0; bin_index < result_count; ++bin_index)
			{
				std::vector<Individual *> &bin_individuals = binned_individuals[bin_index];
				size_t bin_individuals_count = bin_individuals.size();
				
				if (uses_empty && (bin_individuals_count == 0))
				{
					result_data[bin_index] = empty;
				}
				else
				{
					// Set the variable "individuals" to the focal individuals.  We want to do this as efficiently as possible.
					// Best would be to swap() bin_individuals into individuals_vec, which maybe we will do eventually.  In the
					// meantime, we use clear() to release the old values in the vector, resize_no_initialize() to expand to the
					// needed capacity without initializing, and set_object_element_no_check_NORR() to put values into their
					// slots without any checks.  Note that Individual is not under retain/release, which simplifies things.
					Individual **bin_individuals_data = bin_individuals.data();
					
					individuals_vec.clear();
					individuals_vec.resize_no_initialize(bin_individuals_count);
					
					for (size_t index = 0; index < bin_individuals_count; ++index)
						individuals_vec.set_object_element_no_check_NORR(bin_individuals_data[index], index);
					
					// Get the result.  BEWARE!  This calls causes re-entry into the Eidos interpreter, which is not usually
					// possible since Eidos does not support multithreaded usage.  This is therefore a key failure point for
					// bugs that would otherwise not manifest.
					EidosValue_SP &&return_value_SP = interpreter.EvaluateInterpreterBlock(false, true);		// do not print output, return the last statement value
					
					if ((return_value_SP->Count() == 1) && ((return_value_SP->Type() == EidosValueType::kValueFloat) || (return_value_SP->Type() == EidosValueType::kValueInt) || (return_value_SP->Type() == EidosValueType::kValueLogical)))
					{
						result_data[bin_index] = return_value_SP->FloatAtIndex_CAST(0, nullptr);
					}
					else
					{
						EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_summarizeIndividuals): the lambda operation must return a singleton float, integer, or logical." << EidosTerminate();
					}
				}
			}
		}
		catch (...)
		{
			// If exceptions throw, then we want to set up the error information to highlight the
			// sapply() that failed, since we can't highlight the actual error.  (If exceptions
			// don't throw, this catch block will never be hit; exit() will already have been called
			// and the error will have been reported from the context of the lambda script string.)
			if (gEidosTerminateThrows)
			{
				// In some cases, such as if the error occurred in a derived user-defined function, we can
				// actually get a user script error context at this point, and don't need to intervene.
				if (!gEidosErrorContext.currentScript || (gEidosErrorContext.currentScript->UserScriptUTF16Offset() == -1))
				{
					gEidosErrorContext = error_context_save;
					TranslateErrorContextToUserScript("SLiM_ExecuteFunction_summarizeIndividuals()");
				}
			}
			
			if (!lambda_value_singleton)
				delete script;
			
			throw;
		}
		
		// Restore the normal error context in the event that no exception occurring within the lambda
		gEidosErrorContext = error_context_save;
		
		if (!lambda_value_singleton)
			delete script;
	}
	
	// rescale values if requested with perUnitArea; this post-processing code is shared with the lambda case
	if (perUnitArea)
	{
		if (spatiality == 1)
		{
			// first scale end values by the amount of area they contain relative to interior grid cells
			*(result_data) *= 2.0;
			*(result_data + dims[0] - 1) *= 2.0;
			
			// now divide each value by the area encompassed by an interior grid cell, which is a fraction of the total spatialBounds area
			double total_area = spatialBounds[1] - spatialBounds[0];
			double interior_cell_count = dims[0] - 1;								// -1 because end cells combine to make one fewer cell
			double interior_cell_area = total_area / interior_cell_count;
			
			for (int value_index = 0; value_index < result_count; ++value_index)
				result_data[value_index] /= interior_cell_area;
		}
		else if (spatiality == 2)
		{
			// first scale edge and corner values by the amount of area they contain relative to interior grid cells
			// note that these loops hit the corners twice, intentionally; they contain 1/4 the area of interior cells
			for (int row = 0; row < dims[0]; row++)
			{
				*(result_data + row) *= 2.0;
				*(result_data + row + (dims[1] - 1) * dims[0]) *= 2.0;
			}
			for (int col = 0; col < dims[1]; col++)
			{
				*(result_data + col * dims[0]) *= 2.0;
				*(result_data + (dims[0] - 1) + col * dims[0]) *= 2.0;
			}
			
			// now divide each value by the area encompassed by an interior grid cell, which is a fraction of the total spatialBounds area
			double total_area = (spatialBounds[1] - spatialBounds[0]) * (spatialBounds[3] - spatialBounds[2]);		// width * height
			double interior_cell_count = (dims[0] - 1) * (dims[1] - 1);												// -1 because edge/corner cells combine to make one fewer rows/columns
			double interior_cell_area = total_area / interior_cell_count;
			
			for (int value_index = 0; value_index < result_count; ++value_index)
				result_data[value_index] /= interior_cell_area;
		}
		else if (spatiality == 3)
		{
			// first scale edge and corner values by the amount of area they contain relative to interior grid cells
			// note that this hits the corners three times, intentionally; they contain 1/8 the area of interior cells
			// we do this by scanning through the whole array; it's less efficient but the logic is much simpler
			for (int row = 0; row < dims[0]; row++)
			{
				for (int col = 0; col < dims[1]; col++)
				{
					for (int plane = 0; plane < dims[2]; plane++)
					{
						int extreme_row = ((row == 0) || (row == dims[0] - 1)) ? 1 : 0;
						int extreme_col = ((col == 0) || (col == dims[1] - 1)) ? 1 : 0;
						int extreme_plane = ((plane == 0) || (plane == dims[2] - 1)) ? 1 : 0;
						int extremity_sum = extreme_row + extreme_col + extreme_plane;
						
						if (extremity_sum == 0)
							continue;
						
						double factor = 1.0 * (extreme_row ? 2.0 : 1.0) * (extreme_col ? 2.0 : 1.0) * (extreme_plane ? 2.0 : 1.0);
						
						*(result_data + row + col * dims[0] + plane * dims[0] * dims[1]) *= factor;	// (dims[0] - 1 - row) not row, actually, but we can do it flipped, doesn't matter
					}
				}
			}
			
			// now divide each value by the area encompassed by an interior grid cell, which is a fraction of the total spatialBounds area
			double total_area = (spatialBounds[1] - spatialBounds[0]) * (spatialBounds[3] - spatialBounds[2]) * (spatialBounds[5] - spatialBounds[4]);		// width * height * depth
			double interior_cell_count = (dims[0] - 1) * (dims[1] - 1) * (dims[2] - 1);							// -1 because edge/corner cells combine to make one fewer rows/columns
			double interior_cell_area = total_area / interior_cell_count;
			
			for (int value_index = 0; value_index < result_count; ++value_index)
				result_data[value_index] /= interior_cell_area;
		}
	}

	return EidosValue_SP(result_vec);
}

// (object<Dictionary>$)treeSeqMetadata(string$ filePath, [logical$ userData=T])
EidosValue_SP SLiM_ExecuteFunction_treeSeqMetadata(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue *filePath_value = p_arguments[0].get();
	std::string file_path = Eidos_ResolvedPath(Eidos_StripTrailingSlash(filePath_value->StringAtIndex_NOCAST(0, nullptr)));
	
	tsk_table_collection_t temp_tables;
	
	int ret = tsk_table_collection_load(&temp_tables, file_path.c_str(), TSK_LOAD_SKIP_TABLES | TSK_LOAD_SKIP_REFERENCE_SEQUENCE);
	if (ret != 0)
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_treeSeqMetadata): tree-sequence file at " << file_path << " could not be read; error " << ret << " from tsk_table_collection_load()." << EidosTerminate();
	
	if (temp_tables.metadata_schema_length == 0)
	{
		tsk_table_collection_free(&temp_tables);
		
		// With no schema, error out
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_treeSeqMetadata): no metadata schema present in file " << file_path << "; a JSON schema is required." << EidosTerminate();
	}
	
	if (temp_tables.metadata_length == 0)
	{
		tsk_table_collection_free(&temp_tables);
		
		// With no metadata, return an empty dictionary.  BCH 1/17/2025: prior to SLiM 5, this erroneously returned object<Dictionary>(0)
		EidosDictionaryRetained *objectElement = new EidosDictionaryRetained();
		EidosValue_SP result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(objectElement, gEidosDictionaryRetained_Class));
		
		objectElement->Release();	// retained by result_SP
		return result_SP;
	}
	
	std::string metadata_schema_string(temp_tables.metadata_schema, temp_tables.metadata_schema_length);
	nlohmann::json metadata_schema;
	
	try {
		metadata_schema = nlohmann::json::parse(metadata_schema_string);
	} catch (...) {
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_treeSeqMetadata): the metadata schema must be a JSON string." << EidosTerminate();
	}
	
	std::string codec = metadata_schema["codec"];
	
	if (codec != "json")
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_treeSeqMetadata): the metadata codec must be 'json'." << EidosTerminate();
	
	std::string metadata_string(temp_tables.metadata, temp_tables.metadata_length);
	nlohmann::json metadata;
	
	tsk_table_collection_free(&temp_tables);
	
	try {
		metadata = nlohmann::json::parse(metadata_string);
	} catch (...) {
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_treeSeqMetadata): the metadata must be a JSON string." << EidosTerminate();
	}
	
	EidosValue *userData_value = p_arguments[1].get();
	bool userData = userData_value->LogicalAtIndex_NOCAST(0, nullptr);
	
	if (userData)
	{
		if (!metadata.contains("SLiM"))
			EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_treeSeqMetadata): the user metadata was requested, but the top-level metadata does not contain a 'SLiM' key." << EidosTerminate();
		
		metadata = metadata["SLiM"];
		if (metadata.type() != nlohmann::json::value_t::object)
			EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_treeSeqMetadata): the user metadata was requested, but the 'SLiM' key is not of type object." << EidosTerminate();
		
		if (!metadata.contains("user_metadata"))
			EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_treeSeqMetadata): the user metadata was requested, but the 'SLiM' dictionary does not contain a 'user_metadata' key." << EidosTerminate();
		
		metadata = metadata["user_metadata"];
		if (metadata.type() != nlohmann::json::value_t::object)
			EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_treeSeqMetadata): the user metadata was requested, but the 'user_metadata' key is not of type object." << EidosTerminate();
	}
	
	EidosDictionaryRetained *objectElement = new EidosDictionaryRetained();
	EidosValue_SP result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(objectElement, gEidosDictionaryRetained_Class));
	
	objectElement->Release();	// retained by result_SP
	objectElement->AddJSONFrom(metadata);
	objectElement->ContentsChanged("treeSeqMetadata()");
	return result_SP;
}
































