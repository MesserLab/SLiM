//
//  slim_functions.cpp
//  SLiM
//
//  Created by Ben Haller on 2/15/19.
//  Copyright (c) 2014-2023 Philipp Messer.  All rights reserved.
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
#include "genome.h"
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
extern const char *gSLiMSourceCode_calcPairHeterozygosity;
extern const char *gSLiMSourceCode_calcHeterozygosity;
extern const char *gSLiMSourceCode_calcWattersonsTheta;
extern const char *gSLiMSourceCode_calcInbreedingLoad;


const std::vector<EidosFunctionSignature_CSP> *Community::SLiMFunctionSignatures(void)
{
	// Allocate our own EidosFunctionSignature objects
	static std::vector<EidosFunctionSignature_CSP> sim_func_signatures_;
	
	if (!sim_func_signatures_.size())
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("Community::SLiMFunctionSignatures(): not warmed up");
		
		// Nucleotide utilities
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("codonsToAminoAcids", SLiM_ExecuteFunction_codonsToAminoAcids, kEidosValueMaskString | kEidosValueMaskInt, "SLiM"))->AddInt("codons")->AddArgWithDefault(kEidosValueMaskLogical | kEidosValueMaskInt | kEidosValueMaskOptional | kEidosValueMaskSingleton, "long", nullptr, gStaticEidosValue_LogicalF)->AddLogical_OS("paste", gStaticEidosValue_LogicalT));
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("codonsToNucleotides", SLiM_ExecuteFunction_codonsToNucleotides, kEidosValueMaskInt | kEidosValueMaskString, "SLiM"))->AddInt("codons")->AddString_OS("format", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("string"))));
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("mm16To256", SLiM_ExecuteFunction_mm16To256, kEidosValueMaskFloat, "SLiM"))->AddFloat("mutationMatrix16"));
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("mmJukesCantor", SLiM_ExecuteFunction_mmJukesCantor, kEidosValueMaskFloat, "SLiM"))->AddFloat_S("alpha"));
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("mmKimura", SLiM_ExecuteFunction_mmKimura, kEidosValueMaskFloat, "SLiM"))->AddFloat_S("alpha")->AddFloat_S("beta"));
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("nucleotideCounts", SLiM_ExecuteFunction_nucleotideCounts, kEidosValueMaskInt, "SLiM"))->AddIntString("sequence"));
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("nucleotideFrequencies", SLiM_ExecuteFunction_nucleotideFrequencies, kEidosValueMaskFloat, "SLiM"))->AddIntString("sequence"));
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("nucleotidesToCodons", SLiM_ExecuteFunction_nucleotidesToCodons, kEidosValueMaskInt, "SLiM"))->AddIntString("sequence"));
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("randomNucleotides", SLiM_ExecuteFunction_randomNucleotides, kEidosValueMaskInt | kEidosValueMaskString, "SLiM"))->AddInt_S("length")->AddNumeric_ON("basis", gStaticEidosValueNULL)->AddString_OS("format", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("string"))));
		
		// Population genetics utilities (implemented with Eidos code)
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("calcFST", gSLiMSourceCode_calcFST, kEidosValueMaskFloat | kEidosValueMaskSingleton, "SLiM"))->AddObject("genomes1", gSLiM_Genome_Class)->AddObject("genomes2", gSLiM_Genome_Class)->AddObject_ON("muts", gSLiM_Mutation_Class, gStaticEidosValueNULL)->AddInt_OSN("start", gStaticEidosValueNULL)->AddInt_OSN("end", gStaticEidosValueNULL));
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("calcVA", gSLiMSourceCode_calcVA, kEidosValueMaskFloat | kEidosValueMaskSingleton, "SLiM"))->AddObject("individuals", gSLiM_Individual_Class)->AddIntObject_S("mutType", gSLiM_MutationType_Class));
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("calcPairHeterozygosity", gSLiMSourceCode_calcPairHeterozygosity, kEidosValueMaskFloat | kEidosValueMaskSingleton, "SLiM"))->AddObject_S("genome1", gSLiM_Genome_Class)->AddObject_S("genome2", gSLiM_Genome_Class)->AddInt_OSN("start", gStaticEidosValueNULL)->AddInt_OSN("end", gStaticEidosValueNULL)->AddLogical_OS("infiniteSites", gStaticEidosValue_LogicalT));
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("calcHeterozygosity", gSLiMSourceCode_calcHeterozygosity, kEidosValueMaskFloat | kEidosValueMaskSingleton, "SLiM"))->AddObject("genomes", gSLiM_Genome_Class)->AddObject_ON("muts", gSLiM_Mutation_Class, gStaticEidosValueNULL)->AddInt_OSN("start", gStaticEidosValueNULL)->AddInt_OSN("end", gStaticEidosValueNULL));
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("calcWattersonsTheta", gSLiMSourceCode_calcWattersonsTheta, kEidosValueMaskFloat | kEidosValueMaskSingleton, "SLiM"))->AddObject("genomes", gSLiM_Genome_Class)->AddObject_ON("muts", gSLiM_Mutation_Class, gStaticEidosValueNULL)->AddInt_OSN("start", gStaticEidosValueNULL)->AddInt_OSN("end", gStaticEidosValueNULL));
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("calcInbreedingLoad", gSLiMSourceCode_calcInbreedingLoad, kEidosValueMaskFloat | kEidosValueMaskSingleton, "SLiM"))->AddObject("genomes", gSLiM_Genome_Class)->AddObject_OSN("mutType", gSLiM_MutationType_Class, gStaticEidosValueNULL));
		
		// Other built-in SLiM functions
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("summarizeIndividuals", SLiM_ExecuteFunction_summarizeIndividuals, kEidosValueMaskFloat, "SLiM"))->AddObject("individuals", gSLiM_Individual_Class)->AddInt("dim")->AddNumeric("spatialBounds")->AddString_S("operation")->AddLogicalEquiv_OSN("empty", gStaticEidosValue_Float0)->AddLogical_OS("perUnitArea", gStaticEidosValue_LogicalF)->AddString_OSN("spatiality", gStaticEidosValueNULL));
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("treeSeqMetadata", SLiM_ExecuteFunction_treeSeqMetadata, kEidosValueMaskObject | kEidosValueMaskSingleton, gEidosDictionaryRetained_Class, "SLiM"))->AddString_S("filePath")->AddLogical_OS("userData", gStaticEidosValue_LogicalT));
		
		// Internal SLiM functions
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("_startBenchmark", SLiM_ExecuteFunction__startBenchmark, kEidosValueMaskVOID, "SLiM"))->AddString_S("type"));
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

// (float$)calcFST(object<Genome> genomes1, object<Genome> genomes2, [No<Mutation> muts = NULL], [Ni$ start = NULL], [Ni$ end = NULL])
const char *gSLiMSourceCode_calcFST = 
R"V0G0N({
	if ((genomes1.length() == 0) | (genomes2.length() == 0))
		stop("ERROR (calcFST()): genomes1 and genomes2 must both be non-empty.");
	if (community.allSpecies.length() > 1)
	{
		species = unique(genomes1.individual.subpopulation.species, preserveOrder=F);
		if (species.length() != 1)
			stop("ERROR (calcFST()): all genomes must belong to the same species.");
		if (!all(species == genomes2.individual.subpopulation.species))
			stop("ERROR (calcFST()): all genomes must belong to the same species.");
		if (!isNULL(muts))
			if (!all(species == muts.mutationType.species))
				stop("ERROR (calcFST()): all mutations must belong to the same species as the genomes.");
	}
	else
	{
		species = community.allSpecies;
	}
	
	// handle windowing
	if (!isNULL(start) & !isNULL(end))
	{
		if (start > end)
			stop("ERROR (calcFST()): start must be less than or equal to end.");
		if (isNULL(muts))
			muts = species.mutations;
		mpos = muts.position;
		muts = muts[(mpos >= start) & (mpos <= end)];
		length = end - start + 1;
	}
	else if (!isNULL(start) | !isNULL(end))
	{
		stop("ERROR (calcFST()): start and end must both be NULL or both be non-NULL.");
	}
	
	// do the calculation
	p1_p = genomes1.mutationFrequenciesInGenomes(muts);
	p2_p = genomes2.mutationFrequenciesInGenomes(muts);
	mean_p = (p1_p + p2_p) / 2.0;
	H_t = 2.0 * mean_p * (1.0 - mean_p);
	H_s = p1_p * (1.0 - p1_p) + p2_p * (1.0 - p2_p);
	fst = 1.0 - mean(H_s) / mean(H_t);
	return fst;
})V0G0N";

// (float$)calcVA(object<Individual> individuals, io<MutationType>$ mutType)
const char *gSLiMSourceCode_calcVA = 
R"V0G0N({
	species = mutType.species;
	if (community.allSpecies.length() > 1)
		if (!all(individuals.subpopulation.species == species))
			stop("ERROR (calcVA()): all individuals must belong to the same species as mutType.");
	
	// look up an integer mutation type id
	if (type(mutType) == "integer") {
		mutType = species.mutationTypes[species.mutationTypes.id == mutType];
		assert(length(mutType) == 1, "calcVA() mutation type lookup failed");
	}
	return var(individuals.sumOfMutationsOfType(mutType));
})V0G0N";

// (float$)calcPairHeterozygosity(object<Genome>$ genome1, object<Genome>$ genome2, [Ni$ start = NULL], [Ni$ end = NULL], [l$ infiniteSites = T])
const char *gSLiMSourceCode_calcPairHeterozygosity = 
R"V0G0N({
	if (community.allSpecies.length() > 1)
	{
		species = unique(c(genome1.individual.subpopulation.species, genome2.individual.subpopulation.species), preserveOrder=F);
		if (species.length() != 1)
			stop("ERROR (calcPairHeterozygosity()): genome1 and genome2 must belong to the same species.");
	}
	else
	{
		species = community.allSpecies;
	}
	
	muts1 = genome1.mutations;
	muts2 = genome2.mutations;
	length = species.chromosome.lastPosition + 1;

	// handle windowing
	if (!isNULL(start) & !isNULL(end))
	{
		if (start > end)
			stop("ERROR (calcPairHeterozygosity()): start must be less than or equal to end.");
		m1pos = muts1.position;
		m2pos = muts2.position;
		muts1 = muts1[(m1pos >= start) & (m1pos <= end)];
		muts2 = muts2[(m2pos >= start) & (m2pos <= end)];
		length = end - start + 1;
	}
	else if (!isNULL(start) | !isNULL(end))
	{
		stop("ERROR (calcPairHeterozygosity()): start and end must both be NULL or both be non-NULL.");
	}

	// do the calculation
	unshared = setSymmetricDifference(muts1, muts2);
	if (!infiniteSites)
		unshared = unique(unshared.position, preserveOrder=F);

	return size(unshared) / length;
})V0G0N";

// (float$)calcHeterozygosity(o<Genome> genomes, [No<Mutation> muts = NULL], [Ni$ start = NULL], [Ni$ end = NULL])
const char *gSLiMSourceCode_calcHeterozygosity = 
R"V0G0N({
	if (genomes.length() == 0)
		stop("ERROR (calcHeterozygosity()): genomes must be non-empty.");
	if (community.allSpecies.length() > 1)
	{
		species = unique(genomes.individual.subpopulation.species, preserveOrder=F);
		if (species.length() != 1)
			stop("ERROR (calcHeterozygosity()): genomes must all belong to the same species.");
		if (!isNULL(muts))
			if (!all(muts.mutationType.species == species))
				stop("ERROR (calcHeterozygosity()): muts must all belong to the same species as genomes.");
	}
	else
	{
		species = community.allSpecies;
	}
	
	length = species.chromosome.lastPosition + 1;

	// handle windowing
	if (!isNULL(start) & !isNULL(end))
	{
		if (start > end)
			stop("ERROR (calcHeterozygosity()): start must be less than or equal to end.");
		if (isNULL(muts))
			muts = species.mutations;
		mpos = muts.position;
		muts = muts[(mpos >= start) & (mpos <= end)];
		length = end - start + 1;
	}
	else if (!isNULL(start) | !isNULL(end))
	{
		stop("ERROR (calcHeterozygosity()): start and end must both be NULL or both be non-NULL.");
	}

	// do the calculation
	p = genomes.mutationFrequenciesInGenomes(muts);
	heterozygosity = 2 * sum(p * (1 - p)) / length;
	return heterozygosity;
})V0G0N";

// (float$)calcWattersonsTheta(o<Genome> genomes, [No<Mutation> muts = NULL], [Ni$ start = NULL], [Ni$ end = NULL])
const char *gSLiMSourceCode_calcWattersonsTheta = 
R"V0G0N({
	if (genomes.length() == 0)
		stop("ERROR (calcWattersonsTheta()): genomes must be non-empty.");
	if (community.allSpecies.length() > 1)
	{
		species = unique(genomes.individual.subpopulation.species, preserveOrder=F);
		if (species.length() != 1)
			stop("ERROR (calcWattersonsTheta()): genomes must all belong to the same species.");
		if (!isNULL(muts))
			if (!all(muts.mutationType.species == species))
				stop("ERROR (calcWattersonsTheta()): muts must all belong to the same species as genomes.");
	}
	else
	{
		species = community.allSpecies;
	}
	
	if (isNULL(muts))
		muts = species.mutations;
	
	// handle windowing
	if (!isNULL(start) & !isNULL(end))
	{
		if (start > end)
			stop("ERROR (calcWattersonsTheta()): start must be less than or equal to end.");
		mpos = muts.position;
		muts = muts[(mpos >= start) & (mpos <= end)];
		length = end - start + 1;
	}
	else if (!isNULL(start) | !isNULL(end))
	{
		stop("ERROR (calcWattersonsTheta()): start and end must both be NULL or both be non-NULL.");
	}

	// narrow down to the mutations that are actually present in the genomes and aren't fixed
	p = genomes.mutationFrequenciesInGenomes(muts);
	muts = muts[(p != 0.0) & (p != 1.0)];

	// do the calculation
	k = size(muts);
	n = genomes.size();
	a_n = sum(1 / 1:(n-1));
	theta = (k / a_n) / (species.chromosome.lastPosition + 1);
	return theta;
})V0G0N";

// (float$)calcInbreedingLoad(object<Genome> genomes, [No<MutationType>$ mutType = NULL])
const char *gSLiMSourceCode_calcInbreedingLoad = 
R"V0G0N({
	if (genomes.length() == 0)
		stop("ERROR (calcInbreedingLoad()): genomes must be non-empty.");
	if (community.allSpecies.length() > 1)
	{
		species = unique(genomes.individual.subpopulation.species, preserveOrder=F);
		if (species.length() != 1)
			stop("ERROR (calcInbreedingLoad()): genomes must all belong to the same species.");
		if (!isNULL(mutType))
			if (mutType.species != species)
				stop("ERROR (calcInbreedingLoad()): mutType must belong to the same species as genomes.");
	}
	else
	{
		species = community.allSpecies;
	}
	
	// get the focal mutations and narrow down to those that are deleterious
	if (isNULL(mutType))
		muts = species.mutations;
	else
		muts = species.mutationsOfType(mutType);
	
	muts = muts[muts.selectionCoeff < 0.0];
	
	// get frequencies and focus on those that are in the genomes
	q = genomes.mutationFrequenciesInGenomes(muts);
	inGenomes = (q > 0);
	
	muts = muts[inGenomes];
	q = q[inGenomes];
	
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

//	(string)codonsToAminoAcids(integer codons, [li$ long = F])
EidosValue_SP SLiM_ExecuteFunction_codonsToAminoAcids(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue *codons_value = p_arguments[0].get();
	EidosValue *long_value = p_arguments[1].get();
	
	int codons_length = codons_value->Count();
	
	bool integer_result = (long_value->Type() == EidosValueType::kValueInt);
	eidos_logical_t long_strings = (integer_result ? false : long_value->LogicalAtIndex(0, nullptr));
	
	if (integer_result)
	{
		int64_t long_intval = long_value->IntAtIndex(0, nullptr);
		
		if (long_intval != 0)
			EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_codonsToAminoAcids): function codonsToAminoAcids() requires 'long' to be T, F, or 0." << EidosTerminate(nullptr);
	}
	
	if (codons_length == 1)
	{
		int64_t codon = codons_value->IntAtIndex(0, nullptr);
		
		if ((codon < 0) || (codon > 63))
			EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_codonsToAminoAcids): function codonsToAminoAcids() requires codons to be in [0, 63]." << EidosTerminate(nullptr);
		
		if (integer_result)
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(codon2aa_int[codon]));
		}
		else
		{
			std::string &aa = (long_strings ? codon2aa_long[codon] : codon2aa_short[codon]);
		
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(aa));
		}
	}
	else
	{
		const int64_t *int_data = codons_value->IntVector()->data();
		
		if (integer_result)
		{
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(codons_length);
			
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
			eidos_logical_t paste = paste_value->LogicalAtIndex(0, nullptr);
			
			if (paste)
			{
				if (long_strings && (codons_length > 0))
				{
					// pasting: "Aaa-Bbb-Ccc"
					EidosValue_String_singleton *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(""));
					std::string &aa_string = string_result->StringValue_Mutable();
					
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
					EidosValue_String_singleton *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(""));
					std::string &aa_string = string_result->StringValue_Mutable();
					
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
				EidosValue_String_vector *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve(codons_length);
				
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
			const std::string &string_ref = sequence_value->IsSingleton() ? ((EidosValue_String_singleton *)sequence_value)->StringValue() : (*sequence_value->StringVector())[0];
			int64_t length = (int64_t)string_ref.length();
			
			if (length % 3 != 0)
				EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_nucleotidesToCodons): function nucleotidesToCodons() requires the nucleotide sequence to be a multiple of three in length." << EidosTerminate(nullptr);
			
			int64_t length_3 = length / 3;
			
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize((int)length_3);
			
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
		
		EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize((int)length_3);
		
		if (sequence_type == EidosValueType::kValueString)
		{
			// string vector case
			uint8_t *nuc_lookup = NucleotideArray::NucleotideCharToIntLookup();
			const std::vector<std::string> *string_vec = sequence_value->StringVector();
			
			for (int value_index = 0; value_index < length_3; ++value_index)
			{
				int64_t codon_base = (size_t)value_index * 3;
				
				const std::string &nucstring1 = (*string_vec)[codon_base];
				const std::string &nucstring2 = (*string_vec)[codon_base + 1];
				const std::string &nucstring3 = (*string_vec)[codon_base + 2];
				
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
			const int64_t *int_data = sequence_value->IntVector()->data();
			
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
			uint64_t nuc = sequence_value->IntAtIndex(0, nullptr);
			
			if (nuc > 3)
				EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_" << function_name << "): function " << function_name << "() requires integer sequence values to be in [0,3]." << EidosTerminate(nullptr);
			
			total_ACGT[nuc]++;
		}
		else // sequence_type == EidosValueType::kValueString
		{
			uint8_t *nuc_lookup = NucleotideArray::NucleotideCharToIntLookup();
			const std::string &string_ref = sequence_value->IsSingleton() ? ((EidosValue_String_singleton *)sequence_value)->StringValue() : (*sequence_value->StringVector())[0];
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
			const int64_t *int_data = sequence_value->IntVector()->data();
			
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
			const std::vector<std::string> *string_vec = sequence_value->StringVector();
			
			for (int value_index = 0; value_index < sequence_count; ++value_index)
			{
				const std::string &nuc_string = (*string_vec)[value_index];
				
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
	
	EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(256);
	
	for (int i = 0; i < 256; ++i)
	{
		int ancestral_nucleotide = ((i / 4) % 4);
		int derived_nucleotide = (i / 64);
		double value = mutationMatrix16_value->FloatAtIndex(ancestral_nucleotide + derived_nucleotide * 4, nullptr);
		
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
	double alpha = alpha_value->FloatAtIndex(0, nullptr);
	
	if (alpha < 0.0)
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_mmJukesCantor): function mmJukesCantor() requires alpha >= 0.0." << EidosTerminate(nullptr);
	if (3 * alpha > 1.0)
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_mmJukesCantor): function mmJukesCantor() requires 3 * alpha <= 1.0." << EidosTerminate(nullptr);
	
	EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(16);
	
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
	
	double alpha = alpha_value->FloatAtIndex(0, nullptr);
	double beta = beta_value->FloatAtIndex(0, nullptr);
	
	if ((alpha < 0.0) || (alpha > 1.0))
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_mmKimura): function mmKimura() requires alpha to be in [0.0, 1.0]." << EidosTerminate(nullptr);
	if ((beta < 0.0) || (beta > 0.5))
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_mmKimura): function mmKimura() requires beta to be in [0.0, 0.5]." << EidosTerminate(nullptr);
	if (alpha + 2 * beta > 1.0)
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_mmKimura): function mmKimura() requires alpha + 2 * beta to be <= 1.0." << EidosTerminate(nullptr);
	
	EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(16);
	
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
	
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(4);
	
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
	
	EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(4);
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
	int64_t length = length_value->IntAtIndex(0, nullptr);
	
	if ((length < 0) || (length > 2000000000L))
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_randomNucleotides): function randomNucleotides() requires length to be in [0, 2e9]." << EidosTerminate(nullptr);
	
	// Figure out the probability threshold for each base
	double pA = 0.25, pC = 0.25, pG = 0.25, pT; // = 0.25;	// not used below
	
	if (basis_value->Type() != EidosValueType::kValueNULL)
	{
		if (basis_value->Count() != 4)
			EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_randomNucleotides): function randomNucleotides() requires basis to be either NULL, or an integer or float vector of length 4 (with relative probabilities for A/C/G/T)." << EidosTerminate(nullptr);
		
		pA = basis_value->FloatAtIndex(0, nullptr);
		pC = basis_value->FloatAtIndex(1, nullptr);
		pG = basis_value->FloatAtIndex(2, nullptr);
		pT = basis_value->FloatAtIndex(3, nullptr);
		
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
	const std::string &format = format_value->StringRefAtIndex(0, nullptr);
	
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
		EidosValue_String_vector *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve((int)length);
		
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
		EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize((int)length);
		
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
		EidosValue_String_singleton *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(""));
		std::string &nuc_string = string_result->StringValue_Mutable();
		
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
	const std::string &format = format_value->StringRefAtIndex(0, nullptr);
	
	if (format == "char")
	{
		// return a vector of one-character strings, "T" "A" "T" "A" "C" "G"
		EidosValue_String_vector *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve((int)length);
		
		for (int codon_index = 0; codon_index < codons_length; ++codon_index)
		{
			int codon = (int)codons_value->IntAtIndex(codon_index, nullptr);
			
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
		EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize((int)length);
		
		for (int codon_index = 0; codon_index < codons_length; ++codon_index)
		{
			int codon = (int)codons_value->IntAtIndex(codon_index, nullptr);
			
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
		EidosValue_String_singleton *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(""));
		std::string &nuc_string = string_result->StringValue_Mutable();
		
		nuc_string.resize(length);	// create space for all the nucleotides we will generate
		
		char *nuc_string_ptr = &nuc_string[0];	// data() returns a const pointer, but this is safe in C++11 and later
		
		for (int codon_index = 0; codon_index < codons_length; ++codon_index)
		{
			int codon = (int)codons_value->IntAtIndex(codon_index, nullptr);
			
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
	Individual *singleton_ind = nullptr;
	Individual **individuals_buffer = nullptr;
	
	if (individuals_count == 0)
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_summarizeIndividuals): summarizeIndividuals() cannot be called with a zero-length individuals vector, because the focal species, and thus the spatial dimensionality, cannot be determined." << EidosTerminate();
	
	if (individuals_count == 1)
	{
		singleton_ind = (Individual *)individuals_value->ObjectElementAtIndex(0, nullptr);
		individuals_buffer = &singleton_ind;
	}
	else
	{
		individuals_buffer = (Individual **)((EidosValue_Object_vector *)individuals_value)->data();
	}
	
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
		std::string spatiality_string = spatiality_value->StringAtIndex(0, nullptr);
		
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
		spatialBounds[0] = spatialBounds_value->FloatAtIndex(component0, nullptr);
		spatialBounds[1] = spatialBounds_value->FloatAtIndex(component0 + spatiality, nullptr);
		if (spatialBounds[0] >= spatialBounds[1])
			invalid_bounds = true;
	}
	if (spatiality >= 2)
	{
		spatialBounds[2] = spatialBounds_value->FloatAtIndex(component1, nullptr);
		spatialBounds[3] = spatialBounds_value->FloatAtIndex(component1 + spatiality, nullptr);
		if (spatialBounds[2] >= spatialBounds[3])
			invalid_bounds = true;
	}
	if (spatiality >= 3)
	{
		spatialBounds[4] = spatialBounds_value->FloatAtIndex(component2, nullptr);
		spatialBounds[5] = spatialBounds_value->FloatAtIndex(component2 + spatiality, nullptr);
		if (spatialBounds[4] >= spatialBounds[5])
			invalid_bounds = true;
	}
	
	if (invalid_bounds)
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_summarizeIndividuals): summarizeIndividuals() spatialBounds are invalid; it is required that x0 < x1, y0 < y1, and z0 < z1." << EidosTerminate();
	
	// Get the operation lambda string and the empty-cell value (NULL to execute the lambda for empty cells too)
	std::string operation_string = operation_value->StringAtIndex(0, nullptr);
	bool uses_empty = (empty_value->Type() != EidosValueType::kValueNULL);
	double empty = uses_empty ? empty_value->FloatAtIndex(0, nullptr) : 0.0;	// handles logical, integer, and float
	
	// Get the edgeScale value, which is used to postprocess vaues at the very end
	bool perUnitArea = perUnitArea_value->LogicalAtIndex(0, nullptr);
	
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
		dims[dim_index] = dim_value->IntAtIndex(dim_index, nullptr);
		
		if ((dims[dim_index] < 2) || (dims[dim_index] > 10000))
			EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_summarizeIndividuals): summarizeIndividuals() requires dimensions in dim to be in the range [2, 10000]." << EidosTerminate();
		
		result_count *= dims[dim_index];
	}
	
	if ((result_count <= 0) || (result_count >= INT32_MAX))
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_summarizeIndividuals): (internal error) calculated size for returned vector (" << result_count << ") is out of range for int32_t." << EidosTerminate();
	
	// Allocate our return value, set its dimensions, and get set up for using it
	EidosValue_Float_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(result_count);
	double *result_data = result_vec->data();
	
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
		
		EidosValue_String_singleton *lambda_value_singleton = dynamic_cast<EidosValue_String_singleton *>(operation_value);
		EidosScript *script = (lambda_value_singleton ? lambda_value_singleton->CachedScript() : nullptr);
		
		// Errors in lambdas should be reported for the lambda script, not for the calling script,
		// if possible.  In the GUI this does not work well, however; there, errors should be
		// reported as occurring in the call to sapply().  Here we save off the current
		// error context and set up the error context for reporting errors inside the lambda,
		// in case that is possible; see how exceptions are handled below.
		EidosErrorContext error_context_save = gEidosErrorContext;
		
		// We try to do tokenization and parsing once per script, by caching the script inside the EidosValue_String_singleton instance
		if (!script)
		{
			script = new EidosScript(operation_string, -1);
			
			gEidosErrorContext = EidosErrorContext{{-1, -1, -1, -1}, script, true};
			
			try
			{
				script->Tokenize();
				script->ParseInterpreterBlockToAST(false);
			}
			catch (...)
			{
				if (gEidosTerminateThrows)
					gEidosErrorContext = error_context_save;
				
				delete script;
				
				throw;
			}
			
			if (lambda_value_singleton)
				lambda_value_singleton->SetCachedScript(script);
		}
		
		// Execute inside try/catch so we can handle errors well
		gEidosErrorContext = EidosErrorContext{{-1, -1, -1, -1}, script, true};
		
		EidosValue_Object_vector individuals_vec(gSLiM_Individual_Class);
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
						result_data[bin_index] = return_value_SP->FloatAtIndex(0, nullptr);
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
				gEidosErrorContext = error_context_save;
			
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
	std::string file_path = Eidos_ResolvedPath(Eidos_StripTrailingSlash(filePath_value->StringAtIndex(0, nullptr)));
	
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
		
		// With no metadata, return an empty dictionary
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gEidosDictionaryRetained_Class));
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
	bool userData = userData_value->LogicalAtIndex(0, nullptr);
	
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
	EidosValue_SP result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(objectElement, gEidosDictionaryRetained_Class));
	
	objectElement->AddJSONFrom(metadata);
	objectElement->ContentsChanged("treeSeqMetadata()");
	return result_SP;
}
































