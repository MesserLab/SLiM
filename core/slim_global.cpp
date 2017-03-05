//
//  slim_global.cpp
//  SLiM
//
//  Created by Ben Haller on 1/4/15.
//  Copyright (c) 2014-2016 Philipp Messer.  All rights reserved.
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


#include "slim_global.h"
#include "mutation.h"
#include "mutation_run.h"

#include <string>


void SLiM_WarmUp(void)
{
	static bool been_here = false;
	
	if (!been_here)
	{
		been_here = true;
		
		// Set up our shared pool for Mutation objects
		gSLiM_Mutation_Pool = new EidosObjectPool(sizeof(Mutation));
		
		// Register global strings and IDs for SLiM; this is in addition to the globals set up by Eidos
		SLiM_RegisterGlobalStringsAndIDs();
		
#if DO_MEMORY_CHECKS
		// Check for a memory limit and prepare for memory-limit testing
		EidosCheckRSSAgainstMax("SLiM_WarmUp()", "This internal check should never fail!");
#endif
	}
}


// a stringstream for SLiM output; see the header for details
std::ostringstream gSLiMOut;


// Functions for casting from Eidos ints (int64_t) to SLiM int types safely
void SLiMRaiseGenerationRangeError(int64_t p_long_value)
{
	EIDOS_TERMINATION << "ERROR (SLiMRaiseGenerationRangeError): value " << p_long_value << " for a generation index or duration is out of range." << eidos_terminate();
}

void SLiMRaisePositionRangeError(int64_t p_long_value)
{
	EIDOS_TERMINATION << "ERROR (SLiMRaisePositionRangeError): value " << p_long_value << " for a chromosome position or length is out of range." << eidos_terminate();
}

void SLiMRaiseObjectidRangeError(int64_t p_long_value)
{
	EIDOS_TERMINATION << "ERROR (SLiMRaiseObjectidRangeError): value " << p_long_value << " for a SLiM object identifier value is out of range." << eidos_terminate();
}

void SLiMRaisePopsizeRangeError(int64_t p_long_value)
{
	EIDOS_TERMINATION << "ERROR (SLiMRaisePopsizeRangeError): value " << p_long_value << " for a subpopulation size, individual index, or genome index is out of range." << eidos_terminate();
}

void SLiMRaiseUsertagRangeError(int64_t p_long_value)
{
	EIDOS_TERMINATION << "ERROR (SLiMRaiseUsertagRangeError): value " << p_long_value << " for a user-supplied tag is out of range." << eidos_terminate();
}

void SLiMRaisePolymorphismidRangeError(int64_t p_long_value)
{
	EIDOS_TERMINATION << "ERROR (SLiMRaisePolymorphismidRangeError): value " << p_long_value << " for a polymorphism identifier is out of range." << eidos_terminate();
}


// stream output for enumerations
std::ostream& operator<<(std::ostream& p_out, GenomeType p_genome_type)
{
	switch (p_genome_type)
	{
		case GenomeType::kAutosome:		p_out << gStr_A; break;
		case GenomeType::kXChromosome:	p_out << gStr_X; break;	// SEX ONLY
		case GenomeType::kYChromosome:	p_out << gStr_Y; break;	// SEX ONLY
	}
	
	return p_out;
}

std::ostream& operator<<(std::ostream& p_out, IndividualSex p_sex)
{
	switch (p_sex)
	{
		case IndividualSex::kUnspecified:		p_out << "*"; break;
		case IndividualSex::kHermaphrodite:		p_out << "H"; break;
		case IndividualSex::kFemale:			p_out << "F"; break;	// SEX ONLY
		case IndividualSex::kMale:				p_out << "M"; break;	// SEX ONLY
	}
	
	return p_out;
}

// initialize...() functions defined by SLiMSim
const std::string gStr_initializeGenomicElement = "initializeGenomicElement";
const std::string gStr_initializeGenomicElementType = "initializeGenomicElementType";
const std::string gStr_initializeMutationType = "initializeMutationType";
const std::string gStr_initializeGeneConversion = "initializeGeneConversion";
const std::string gStr_initializeMutationRate = "initializeMutationRate";
const std::string gStr_initializeRecombinationRate = "initializeRecombinationRate";
const std::string gStr_initializeSex = "initializeSex";
const std::string gStr_initializeSLiMOptions = "initializeSLiMOptions";
const std::string gStr_initializeInteractionType = "initializeInteractionType";

// SLiMEidosDictionary
const std::string gStr_getValue = "getValue";
const std::string gStr_setValue = "setValue";

// mostly property names
const std::string gStr_genomicElements = "genomicElements";
const std::string gStr_lastPosition = "lastPosition";
const std::string gStr_overallRecombinationRate = "overallRecombinationRate";
const std::string gStr_overallRecombinationRateM = "overallRecombinationRateM";
const std::string gStr_overallRecombinationRateF = "overallRecombinationRateF";
const std::string gStr_recombinationEndPositions = "recombinationEndPositions";
const std::string gStr_recombinationEndPositionsM = "recombinationEndPositionsM";
const std::string gStr_recombinationEndPositionsF = "recombinationEndPositionsF";
const std::string gStr_recombinationRates = "recombinationRates";
const std::string gStr_recombinationRatesM = "recombinationRatesM";
const std::string gStr_recombinationRatesF = "recombinationRatesF";
const std::string gStr_geneConversionFraction = "geneConversionFraction";
const std::string gStr_geneConversionMeanLength = "geneConversionMeanLength";
const std::string gStr_mutationRate = "mutationRate";
const std::string gStr_genomeType = "genomeType";
const std::string gStr_isNullGenome = "isNullGenome";
const std::string gStr_mutations = "mutations";
const std::string gStr_uniqueMutations = "uniqueMutations";
const std::string gStr_genomicElementType = "genomicElementType";
const std::string gStr_startPosition = "startPosition";
const std::string gStr_endPosition = "endPosition";
const std::string gStr_id = "id";
const std::string gStr_mutationTypes = "mutationTypes";
const std::string gStr_mutationFractions = "mutationFractions";
const std::string gStr_mutationType = "mutationType";
const std::string gStr_originGeneration = "originGeneration";
const std::string gStr_position = "position";
const std::string gStr_selectionCoeff = "selectionCoeff";
const std::string gStr_subpopID = "subpopID";
const std::string gStr_convertToSubstitution = "convertToSubstitution";
const std::string gStr_distributionType = "distributionType";
const std::string gStr_distributionParams = "distributionParams";
const std::string gStr_dominanceCoeff = "dominanceCoeff";
const std::string gStr_mutationStackPolicy = "mutationStackPolicy";
const std::string gStr_start = "start";
const std::string gStr_end = "end";
const std::string gStr_type = "type";
const std::string gStr_source = "source";
const std::string gStr_active = "active";
const std::string gStr_chromosome = "chromosome";
const std::string gStr_chromosomeType = "chromosomeType";
const std::string gStr_genomicElementTypes = "genomicElementTypes";
const std::string gStr_interactionTypes = "interactionTypes";
const std::string gStr_scriptBlocks = "scriptBlocks";
const std::string gStr_sexEnabled = "sexEnabled";
const std::string gStr_subpopulations = "subpopulations";
const std::string gStr_substitutions = "substitutions";
const std::string gStr_dominanceCoeffX = "dominanceCoeffX";
const std::string gStr_generation = "generation";
const std::string gStr_color = "color";
const std::string gStr_colorSubstitution = "colorSubstitution";
const std::string gStr_tag = "tag";
const std::string gStr_tagF = "tagF";
const std::string gStr_firstMaleIndex = "firstMaleIndex";
const std::string gStr_genomes = "genomes";
const std::string gStr_sex = "sex";
const std::string gStr_individuals = "individuals";
const std::string gStr_subpopulation = "subpopulation";
const std::string gStr_index = "index";
const std::string gStr_immigrantSubpopIDs = "immigrantSubpopIDs";
const std::string gStr_immigrantSubpopFractions = "immigrantSubpopFractions";
const std::string gStr_selfingRate = "selfingRate";
const std::string gStr_cloningRate = "cloningRate";
const std::string gStr_sexRatio = "sexRatio";
const std::string gStr_individualCount = "individualCount";
const std::string gStr_fixationGeneration = "fixationGeneration";
const std::string gStr_pedigreeID = "pedigreeID";
const std::string gStr_pedigreeParentIDs = "pedigreeParentIDs";
const std::string gStr_pedigreeGrandparentIDs = "pedigreeGrandparentIDs";
const std::string gStr_reciprocality = "reciprocality";
const std::string gStr_sexSegregation = "sexSegregation";
const std::string gStr_dimensionality = "dimensionality";
const std::string gStr_spatiality = "spatiality";
const std::string gStr_spatialPosition = "spatialPosition";
const std::string gStr_maxDistance = "maxDistance";

// mostly method names
const std::string gStr_setRecombinationRate = "setRecombinationRate";
const std::string gStr_addMutations = "addMutations";
const std::string gStr_addNewDrawnMutation = "addNewDrawnMutation";
const std::string gStr_addNewMutation = "addNewMutation";
const std::string gStr_containsMutations = "containsMutations";
const std::string gStr_countOfMutationsOfType = "countOfMutationsOfType";
const std::string gStr_containsMarkerMutation = "containsMarkerMutation";
const std::string gStr_relatedness = "relatedness";
const std::string gStr_mutationsOfType = "mutationsOfType";
const std::string gStr_uniqueMutationsOfType = "uniqueMutationsOfType";
const std::string gStr_removeMutations = "removeMutations";
const std::string gStr_setGenomicElementType = "setGenomicElementType";
const std::string gStr_setMutationFractions = "setMutationFractions";
const std::string gStr_setSelectionCoeff = "setSelectionCoeff";
const std::string gStr_setMutationType = "setMutationType";
const std::string gStr_setDistribution = "setDistribution";
const std::string gStr_addSubpop = "addSubpop";
const std::string gStr_addSubpopSplit = "addSubpopSplit";
const std::string gStr_deregisterScriptBlock = "deregisterScriptBlock";
const std::string gStr_mutationFrequencies = "mutationFrequencies";
const std::string gStr_mutationCounts = "mutationCounts";
//const std::string gStr_mutationsOfType = "mutationsOfType";
//const std::string gStr_countOfMutationsOfType = "countOfMutationsOfType";
const std::string gStr_outputFixedMutations = "outputFixedMutations";
const std::string gStr_outputFull = "outputFull";
const std::string gStr_outputMutations = "outputMutations";
const std::string gStr_readFromPopulationFile = "readFromPopulationFile";
const std::string gStr_recalculateFitness = "recalculateFitness";
const std::string gStr_registerEarlyEvent = "registerEarlyEvent";
const std::string gStr_registerLateEvent = "registerLateEvent";
const std::string gStr_registerFitnessCallback = "registerFitnessCallback";
const std::string gStr_registerMateChoiceCallback = "registerMateChoiceCallback";
const std::string gStr_registerModifyChildCallback = "registerModifyChildCallback";
const std::string gStr_registerRecombinationCallback = "registerRecombinationCallback";
const std::string gStr_simulationFinished = "simulationFinished";
const std::string gStr_setMigrationRates = "setMigrationRates";
const std::string gStr_setCloningRate = "setCloningRate";
const std::string gStr_setSelfingRate = "setSelfingRate";
const std::string gStr_setSexRatio = "setSexRatio";
const std::string gStr_setSubpopulationSize = "setSubpopulationSize";
const std::string gStr_cachedFitness = "cachedFitness";
const std::string gStr_outputMSSample = "outputMSSample";
const std::string gStr_outputVCFSample = "outputVCFSample";
const std::string gStr_outputSample = "outputSample";
const std::string gStr_outputMS = "outputMS";
const std::string gStr_outputVCF = "outputVCF";
const std::string gStr_output = "output";
const std::string gStr_evaluate = "evaluate";
const std::string gStr_distance = "distance";
const std::string gStr_distanceToPoint = "distanceToPoint";
const std::string gStr_nearestNeighbors = "nearestNeighbors";
const std::string gStr_nearestNeighborsOfPoint = "nearestNeighborsOfPoint";
const std::string gStr_setInteractionFunction = "setInteractionFunction";
const std::string gStr_strength = "strength";
const std::string gStr_totalOfNeighborStrengths = "totalOfNeighborStrengths";
const std::string gStr_drawByStrength = "drawByStrength";

// mostly SLiM variable names used in callbacks and such
const std::string gStr_sim = "sim";
const std::string gStr_self = "self";
const std::string gStr_individual = "individual";
const std::string gStr_genome1 = "genome1";
const std::string gStr_genome2 = "genome2";
const std::string gStr_subpop = "subpop";
const std::string gStr_sourceSubpop = "sourceSubpop";
//const std::string gStr_weights = "weights";		now gEidosStr_weights
const std::string gStr_child = "child";
const std::string gStr_childGenome1 = "childGenome1";
const std::string gStr_childGenome2 = "childGenome2";
const std::string gStr_childIsFemale = "childIsFemale";
const std::string gStr_parent1 = "parent1";
const std::string gStr_parent1Genome1 = "parent1Genome1";
const std::string gStr_parent1Genome2 = "parent1Genome2";
const std::string gStr_isCloning = "isCloning";
const std::string gStr_isSelfing = "isSelfing";
const std::string gStr_parent2 = "parent2";
const std::string gStr_parent2Genome1 = "parent2Genome1";
const std::string gStr_parent2Genome2 = "parent2Genome2";
const std::string gStr_mut = "mut";
const std::string gStr_relFitness = "relFitness";
const std::string gStr_homozygous = "homozygous";
const std::string gStr_breakpoints = "breakpoints";
const std::string gStr_gcStarts = "gcStarts";
const std::string gStr_gcEnds = "gcEnds";

// mostly SLiM element types
const std::string gStr_SLiMEidosDictionary = "SLiMEidosDictionary";
const std::string gStr_Chromosome = "Chromosome";
const std::string gStr_Genome = "Genome";
const std::string gStr_GenomicElement = "GenomicElement";
const std::string gStr_GenomicElementType = "GenomicElementType";
const std::string gStr_Mutation = "Mutation";
const std::string gStr_MutationType = "MutationType";
const std::string gStr_SLiMEidosBlock = "SLiMEidosBlock";
const std::string gStr_SLiMSim = "SLiMSim";
const std::string gStr_Subpopulation = "Subpopulation";
const std::string gStr_Individual = "Individual";
const std::string gStr_Substitution = "Substitution";
const std::string gStr_InteractionType = "InteractionType";

// mostly other fixed strings
const std::string gStr_A = "A";
const std::string gStr_X = "X";
const std::string gStr_Y = "Y";
const std::string gStr_f = "f";
const std::string gStr_g = "g";
const std::string gStr_e = "e";
//const std::string gStr_n = "n";		now gEidosStr_n
const std::string gStr_w = "w";
const std::string gStr_l = "l";
const std::string gStr_s = "s";
const std::string gStr_early = "early";
const std::string gStr_late = "late";
const std::string gStr_initialize = "initialize";
const std::string gStr_fitness = "fitness";
const std::string gStr_mateChoice = "mateChoice";
const std::string gStr_modifyChild = "modifyChild";
const std::string gStr_recombination = "recombination";


void SLiM_RegisterGlobalStringsAndIDs(void)
{
	static bool been_here = false;
	
	if (!been_here)
	{
		been_here = true;
		
		gEidosContextVersion = "SLiM version 2.2.1";	// SLIM VERSION
		gEidosContextLicense = "SLiM is free software: you can redistribute it and/or\nmodify it under the terms of the GNU General Public\nLicense as published by the Free Software Foundation,\neither version 3 of the License, or (at your option)\nany later version.\n\nSLiM is distributed in the hope that it will be\nuseful, but WITHOUT ANY WARRANTY; without even the\nimplied warranty of MERCHANTABILITY or FITNESS FOR\nA PARTICULAR PURPOSE.  See the GNU General Public\nLicense for more details.\n\nYou should have received a copy of the GNU General\nPublic License along with SLiM.  If not, see\n<http://www.gnu.org/licenses/>.\n";
		gEidosContextCitation = "To cite SLiM in publications please use:\n\nHaller, B.C., and Messer, P.W. (2017). SLiM 2: Flexible,\nInteractive Forward Genetic Simulations. Molecular\nBiology and Evolution 34(1), 230-240.\nDOI: 10.1093/molbev/msw211\n";
		
		Eidos_RegisterStringForGlobalID(gStr_initializeGenomicElement, gID_initializeGenomicElement);
		Eidos_RegisterStringForGlobalID(gStr_initializeGenomicElementType, gID_initializeGenomicElementType);
		Eidos_RegisterStringForGlobalID(gStr_initializeMutationType, gID_initializeMutationType);
		Eidos_RegisterStringForGlobalID(gStr_initializeGeneConversion, gID_initializeGeneConversion);
		Eidos_RegisterStringForGlobalID(gStr_initializeMutationRate, gID_initializeMutationRate);
		Eidos_RegisterStringForGlobalID(gStr_initializeRecombinationRate, gID_initializeRecombinationRate);
		Eidos_RegisterStringForGlobalID(gStr_initializeSex, gID_initializeSex);
		Eidos_RegisterStringForGlobalID(gStr_initializeSLiMOptions, gID_initializeSLiMOptions);
		Eidos_RegisterStringForGlobalID(gStr_initializeInteractionType, gID_initializeInteractionType);
		
		Eidos_RegisterStringForGlobalID(gStr_getValue, gID_getValue);
		Eidos_RegisterStringForGlobalID(gStr_setValue, gID_setValue);
		
		Eidos_RegisterStringForGlobalID(gStr_genomicElements, gID_genomicElements);
		Eidos_RegisterStringForGlobalID(gStr_lastPosition, gID_lastPosition);
		Eidos_RegisterStringForGlobalID(gStr_overallRecombinationRate, gID_overallRecombinationRate);
		Eidos_RegisterStringForGlobalID(gStr_overallRecombinationRateM, gID_overallRecombinationRateM);
		Eidos_RegisterStringForGlobalID(gStr_overallRecombinationRateF, gID_overallRecombinationRateF);
		Eidos_RegisterStringForGlobalID(gStr_recombinationEndPositions, gID_recombinationEndPositions);
		Eidos_RegisterStringForGlobalID(gStr_recombinationEndPositionsM, gID_recombinationEndPositionsM);
		Eidos_RegisterStringForGlobalID(gStr_recombinationEndPositionsF, gID_recombinationEndPositionsF);
		Eidos_RegisterStringForGlobalID(gStr_recombinationRates, gID_recombinationRates);
		Eidos_RegisterStringForGlobalID(gStr_recombinationRatesM, gID_recombinationRatesM);
		Eidos_RegisterStringForGlobalID(gStr_recombinationRatesF, gID_recombinationRatesF);
		Eidos_RegisterStringForGlobalID(gStr_geneConversionFraction, gID_geneConversionFraction);
		Eidos_RegisterStringForGlobalID(gStr_geneConversionMeanLength, gID_geneConversionMeanLength);
		Eidos_RegisterStringForGlobalID(gStr_mutationRate, gID_mutationRate);
		Eidos_RegisterStringForGlobalID(gStr_genomeType, gID_genomeType);
		Eidos_RegisterStringForGlobalID(gStr_isNullGenome, gID_isNullGenome);
		Eidos_RegisterStringForGlobalID(gStr_mutations, gID_mutations);
		Eidos_RegisterStringForGlobalID(gStr_uniqueMutations, gID_uniqueMutations);
		Eidos_RegisterStringForGlobalID(gStr_genomicElementType, gID_genomicElementType);
		Eidos_RegisterStringForGlobalID(gStr_startPosition, gID_startPosition);
		Eidos_RegisterStringForGlobalID(gStr_endPosition, gID_endPosition);
		Eidos_RegisterStringForGlobalID(gStr_id, gID_id);
		Eidos_RegisterStringForGlobalID(gStr_mutationTypes, gID_mutationTypes);
		Eidos_RegisterStringForGlobalID(gStr_mutationFractions, gID_mutationFractions);
		Eidos_RegisterStringForGlobalID(gStr_mutationType, gID_mutationType);
		Eidos_RegisterStringForGlobalID(gStr_originGeneration, gID_originGeneration);
		Eidos_RegisterStringForGlobalID(gStr_position, gID_position);
		Eidos_RegisterStringForGlobalID(gStr_selectionCoeff, gID_selectionCoeff);
		Eidos_RegisterStringForGlobalID(gStr_subpopID, gID_subpopID);
		Eidos_RegisterStringForGlobalID(gStr_convertToSubstitution, gID_convertToSubstitution);
		Eidos_RegisterStringForGlobalID(gStr_distributionType, gID_distributionType);
		Eidos_RegisterStringForGlobalID(gStr_distributionParams, gID_distributionParams);
		Eidos_RegisterStringForGlobalID(gStr_dominanceCoeff, gID_dominanceCoeff);
		Eidos_RegisterStringForGlobalID(gStr_mutationStackPolicy, gID_mutationStackPolicy);
		Eidos_RegisterStringForGlobalID(gStr_start, gID_start);
		Eidos_RegisterStringForGlobalID(gStr_end, gID_end);
		Eidos_RegisterStringForGlobalID(gStr_type, gID_type);
		Eidos_RegisterStringForGlobalID(gStr_source, gID_source);
		Eidos_RegisterStringForGlobalID(gStr_active, gID_active);
		Eidos_RegisterStringForGlobalID(gStr_chromosome, gID_chromosome);
		Eidos_RegisterStringForGlobalID(gStr_chromosomeType, gID_chromosomeType);
		Eidos_RegisterStringForGlobalID(gStr_genomicElementTypes, gID_genomicElementTypes);
		Eidos_RegisterStringForGlobalID(gStr_interactionTypes, gID_interactionTypes);
		Eidos_RegisterStringForGlobalID(gStr_scriptBlocks, gID_scriptBlocks);
		Eidos_RegisterStringForGlobalID(gStr_sexEnabled, gID_sexEnabled);
		Eidos_RegisterStringForGlobalID(gStr_subpopulations, gID_subpopulations);
		Eidos_RegisterStringForGlobalID(gStr_substitutions, gID_substitutions);
		Eidos_RegisterStringForGlobalID(gStr_dominanceCoeffX, gID_dominanceCoeffX);
		Eidos_RegisterStringForGlobalID(gStr_generation, gID_generation);
		Eidos_RegisterStringForGlobalID(gStr_color, gID_color);
		Eidos_RegisterStringForGlobalID(gStr_colorSubstitution, gID_colorSubstitution);
		Eidos_RegisterStringForGlobalID(gStr_tag, gID_tag);
		Eidos_RegisterStringForGlobalID(gStr_tagF, gID_tagF);
		Eidos_RegisterStringForGlobalID(gStr_firstMaleIndex, gID_firstMaleIndex);
		Eidos_RegisterStringForGlobalID(gStr_genomes, gID_genomes);
		Eidos_RegisterStringForGlobalID(gStr_sex, gID_sex);
		Eidos_RegisterStringForGlobalID(gStr_individuals, gID_individuals);
		Eidos_RegisterStringForGlobalID(gStr_subpopulation, gID_subpopulation);
		Eidos_RegisterStringForGlobalID(gStr_index, gID_index);
		Eidos_RegisterStringForGlobalID(gStr_immigrantSubpopIDs, gID_immigrantSubpopIDs);
		Eidos_RegisterStringForGlobalID(gStr_immigrantSubpopFractions, gID_immigrantSubpopFractions);
		Eidos_RegisterStringForGlobalID(gStr_selfingRate, gID_selfingRate);
		Eidos_RegisterStringForGlobalID(gStr_cloningRate, gID_cloningRate);
		Eidos_RegisterStringForGlobalID(gStr_sexRatio, gID_sexRatio);
		Eidos_RegisterStringForGlobalID(gStr_individualCount, gID_individualCount);
		Eidos_RegisterStringForGlobalID(gStr_fixationGeneration, gID_fixationGeneration);
		Eidos_RegisterStringForGlobalID(gStr_pedigreeID, gID_pedigreeID);
		Eidos_RegisterStringForGlobalID(gStr_pedigreeParentIDs, gID_pedigreeParentIDs);
		Eidos_RegisterStringForGlobalID(gStr_pedigreeGrandparentIDs, gID_pedigreeGrandparentIDs);
		Eidos_RegisterStringForGlobalID(gStr_reciprocality, gID_reciprocality);
		Eidos_RegisterStringForGlobalID(gStr_sexSegregation, gID_sexSegregation);
		Eidos_RegisterStringForGlobalID(gStr_dimensionality, gID_dimensionality);
		Eidos_RegisterStringForGlobalID(gStr_spatiality, gID_spatiality);
		Eidos_RegisterStringForGlobalID(gStr_spatialPosition, gID_spatialPosition);
		Eidos_RegisterStringForGlobalID(gStr_maxDistance, gID_maxDistance);
		
		Eidos_RegisterStringForGlobalID(gStr_setRecombinationRate, gID_setRecombinationRate);
		Eidos_RegisterStringForGlobalID(gStr_addMutations, gID_addMutations);
		Eidos_RegisterStringForGlobalID(gStr_addNewDrawnMutation, gID_addNewDrawnMutation);
		Eidos_RegisterStringForGlobalID(gStr_addNewMutation, gID_addNewMutation);
		Eidos_RegisterStringForGlobalID(gStr_countOfMutationsOfType, gID_countOfMutationsOfType);
		Eidos_RegisterStringForGlobalID(gStr_containsMarkerMutation, gID_containsMarkerMutation);
		Eidos_RegisterStringForGlobalID(gStr_relatedness, gID_relatedness);
		Eidos_RegisterStringForGlobalID(gStr_containsMutations, gID_containsMutations);
		Eidos_RegisterStringForGlobalID(gStr_mutationsOfType, gID_mutationsOfType);
		Eidos_RegisterStringForGlobalID(gStr_uniqueMutationsOfType, gID_uniqueMutationsOfType);
		Eidos_RegisterStringForGlobalID(gStr_removeMutations, gID_removeMutations);
		Eidos_RegisterStringForGlobalID(gStr_setGenomicElementType, gID_setGenomicElementType);
		Eidos_RegisterStringForGlobalID(gStr_setMutationFractions, gID_setMutationFractions);
		Eidos_RegisterStringForGlobalID(gStr_setSelectionCoeff, gID_setSelectionCoeff);
		Eidos_RegisterStringForGlobalID(gStr_setMutationType, gID_setMutationType);
		Eidos_RegisterStringForGlobalID(gStr_setDistribution, gID_setDistribution);
		Eidos_RegisterStringForGlobalID(gStr_addSubpop, gID_addSubpop);
		Eidos_RegisterStringForGlobalID(gStr_addSubpopSplit, gID_addSubpopSplit);
		Eidos_RegisterStringForGlobalID(gStr_deregisterScriptBlock, gID_deregisterScriptBlock);
		Eidos_RegisterStringForGlobalID(gStr_mutationFrequencies, gID_mutationFrequencies);
		Eidos_RegisterStringForGlobalID(gStr_mutationCounts, gID_mutationCounts);
		Eidos_RegisterStringForGlobalID(gStr_outputFixedMutations, gID_outputFixedMutations);
		Eidos_RegisterStringForGlobalID(gStr_outputFull, gID_outputFull);
		Eidos_RegisterStringForGlobalID(gStr_outputMutations, gID_outputMutations);
		Eidos_RegisterStringForGlobalID(gStr_readFromPopulationFile, gID_readFromPopulationFile);
		Eidos_RegisterStringForGlobalID(gStr_recalculateFitness, gID_recalculateFitness);
		Eidos_RegisterStringForGlobalID(gStr_registerEarlyEvent, gID_registerEarlyEvent);
		Eidos_RegisterStringForGlobalID(gStr_registerLateEvent, gID_registerLateEvent);
		Eidos_RegisterStringForGlobalID(gStr_registerFitnessCallback, gID_registerFitnessCallback);
		Eidos_RegisterStringForGlobalID(gStr_registerMateChoiceCallback, gID_registerMateChoiceCallback);
		Eidos_RegisterStringForGlobalID(gStr_registerModifyChildCallback, gID_registerModifyChildCallback);
		Eidos_RegisterStringForGlobalID(gStr_registerRecombinationCallback, gID_registerRecombinationCallback);
		Eidos_RegisterStringForGlobalID(gStr_simulationFinished, gID_simulationFinished);
		Eidos_RegisterStringForGlobalID(gStr_setMigrationRates, gID_setMigrationRates);
		Eidos_RegisterStringForGlobalID(gStr_setCloningRate, gID_setCloningRate);
		Eidos_RegisterStringForGlobalID(gStr_setSelfingRate, gID_setSelfingRate);
		Eidos_RegisterStringForGlobalID(gStr_setSexRatio, gID_setSexRatio);
		Eidos_RegisterStringForGlobalID(gStr_setSubpopulationSize, gID_setSubpopulationSize);
		Eidos_RegisterStringForGlobalID(gStr_cachedFitness, gID_cachedFitness);
		Eidos_RegisterStringForGlobalID(gStr_outputMSSample, gID_outputMSSample);
		Eidos_RegisterStringForGlobalID(gStr_outputVCFSample, gID_outputVCFSample);
		Eidos_RegisterStringForGlobalID(gStr_outputSample, gID_outputSample);
		Eidos_RegisterStringForGlobalID(gStr_outputMS, gID_outputMS);
		Eidos_RegisterStringForGlobalID(gStr_outputVCF, gID_outputVCF);
		Eidos_RegisterStringForGlobalID(gStr_output, gID_output);
		Eidos_RegisterStringForGlobalID(gStr_evaluate, gID_evaluate);
		Eidos_RegisterStringForGlobalID(gStr_distance, gID_distance);
		Eidos_RegisterStringForGlobalID(gStr_distanceToPoint, gID_distanceToPoint);
		Eidos_RegisterStringForGlobalID(gStr_nearestNeighbors, gID_nearestNeighbors);
		Eidos_RegisterStringForGlobalID(gStr_nearestNeighborsOfPoint, gID_nearestNeighborsOfPoint);
		Eidos_RegisterStringForGlobalID(gStr_setInteractionFunction, gID_setInteractionFunction);
		Eidos_RegisterStringForGlobalID(gStr_strength, gID_strength);
		Eidos_RegisterStringForGlobalID(gStr_totalOfNeighborStrengths, gID_totalOfNeighborStrengths);
		Eidos_RegisterStringForGlobalID(gStr_drawByStrength, gID_drawByStrength);
		
		Eidos_RegisterStringForGlobalID(gStr_sim, gID_sim);
		Eidos_RegisterStringForGlobalID(gStr_self, gID_self);
		Eidos_RegisterStringForGlobalID(gStr_individual, gID_individual);
		Eidos_RegisterStringForGlobalID(gStr_genome1, gID_genome1);
		Eidos_RegisterStringForGlobalID(gStr_genome2, gID_genome2);
		Eidos_RegisterStringForGlobalID(gStr_subpop, gID_subpop);
		Eidos_RegisterStringForGlobalID(gStr_sourceSubpop, gID_sourceSubpop);
		Eidos_RegisterStringForGlobalID(gStr_child, gID_child);
		Eidos_RegisterStringForGlobalID(gStr_childGenome1, gID_childGenome1);
		Eidos_RegisterStringForGlobalID(gStr_childGenome2, gID_childGenome2);
		Eidos_RegisterStringForGlobalID(gStr_childIsFemale, gID_childIsFemale);
		Eidos_RegisterStringForGlobalID(gStr_parent1, gID_parent1);
		Eidos_RegisterStringForGlobalID(gStr_parent1Genome1, gID_parent1Genome1);
		Eidos_RegisterStringForGlobalID(gStr_parent1Genome2, gID_parent1Genome2);
		Eidos_RegisterStringForGlobalID(gStr_isCloning, gID_isCloning);
		Eidos_RegisterStringForGlobalID(gStr_isSelfing, gID_isSelfing);
		Eidos_RegisterStringForGlobalID(gStr_parent2, gID_parent2);
		Eidos_RegisterStringForGlobalID(gStr_parent2Genome1, gID_parent2Genome1);
		Eidos_RegisterStringForGlobalID(gStr_parent2Genome2, gID_parent2Genome2);
		Eidos_RegisterStringForGlobalID(gStr_mut, gID_mut);
		Eidos_RegisterStringForGlobalID(gStr_relFitness, gID_relFitness);
		Eidos_RegisterStringForGlobalID(gStr_homozygous, gID_homozygous);
		Eidos_RegisterStringForGlobalID(gStr_breakpoints, gID_breakpoints);
		Eidos_RegisterStringForGlobalID(gStr_gcStarts, gID_gcStarts);
		Eidos_RegisterStringForGlobalID(gStr_gcEnds, gID_gcEnds);
		
		Eidos_RegisterStringForGlobalID(gStr_Chromosome, gID_Chromosome);
		Eidos_RegisterStringForGlobalID(gStr_Genome, gID_Genome);
		Eidos_RegisterStringForGlobalID(gStr_GenomicElement, gID_GenomicElement);
		Eidos_RegisterStringForGlobalID(gStr_GenomicElementType, gID_GenomicElementType);
		Eidos_RegisterStringForGlobalID(gStr_Mutation, gID_Mutation);
		Eidos_RegisterStringForGlobalID(gStr_MutationType, gID_MutationType);
		Eidos_RegisterStringForGlobalID(gStr_SLiMEidosBlock, gID_SLiMEidosBlock);
		Eidos_RegisterStringForGlobalID(gStr_SLiMSim, gID_SLiMSim);
		Eidos_RegisterStringForGlobalID(gStr_Subpopulation, gID_Subpopulation);
		Eidos_RegisterStringForGlobalID(gStr_Individual, gID_Individual);
		Eidos_RegisterStringForGlobalID(gStr_Substitution, gID_Substitution);
		Eidos_RegisterStringForGlobalID(gStr_InteractionType, gID_InteractionType);
		
		Eidos_RegisterStringForGlobalID(gStr_A, gID_A);
		Eidos_RegisterStringForGlobalID(gStr_X, gID_X);
		Eidos_RegisterStringForGlobalID(gStr_Y, gID_Y);
		Eidos_RegisterStringForGlobalID(gStr_f, gID_f);
		Eidos_RegisterStringForGlobalID(gStr_g, gID_g);
		Eidos_RegisterStringForGlobalID(gStr_e, gID_e);
		Eidos_RegisterStringForGlobalID(gStr_w, gID_w);
		Eidos_RegisterStringForGlobalID(gStr_l, gID_l);
		Eidos_RegisterStringForGlobalID(gStr_s, gID_s);
		Eidos_RegisterStringForGlobalID(gStr_early, gID_early);
		Eidos_RegisterStringForGlobalID(gStr_late, gID_late);
		Eidos_RegisterStringForGlobalID(gStr_initialize, gID_initialize);
		Eidos_RegisterStringForGlobalID(gStr_fitness, gID_fitness);
		Eidos_RegisterStringForGlobalID(gStr_mateChoice, gID_mateChoice);
		Eidos_RegisterStringForGlobalID(gStr_modifyChild, gID_modifyChild);
		Eidos_RegisterStringForGlobalID(gStr_recombination, gID_recombination);
	}
}


// *******************************************************************************************************************
//
//	Support for named / specified colors in SLiM
//

// Named colors.  This list is taken directly from R, used under their GPL-3.

SLiMNamedColor gSLiMNamedColors[] = {
	{"white", 255, 255, 255},
	{"aliceblue", 240, 248, 255},
	{"antiquewhite", 250, 235, 215},
	{"antiquewhite1", 255, 239, 219},
	{"antiquewhite2", 238, 223, 204},
	{"antiquewhite3", 205, 192, 176},
	{"antiquewhite4", 139, 131, 120},
	{"aquamarine", 127, 255, 212},
	{"aquamarine1", 127, 255, 212},
	{"aquamarine2", 118, 238, 198},
	{"aquamarine3", 102, 205, 170},
	{"aquamarine4", 69, 139, 116},
	{"azure", 240, 255, 255},
	{"azure1", 240, 255, 255},
	{"azure2", 224, 238, 238},
	{"azure3", 193, 205, 205},
	{"azure4", 131, 139, 139},
	{"beige", 245, 245, 220},
	{"bisque", 255, 228, 196},
	{"bisque1", 255, 228, 196},
	{"bisque2", 238, 213, 183},
	{"bisque3", 205, 183, 158},
	{"bisque4", 139, 125, 107},
	{"black", 0, 0, 0},
	{"blanchedalmond", 255, 235, 205},
	{"blue", 0, 0, 255},
	{"blue1", 0, 0, 255},
	{"blue2", 0, 0, 238},
	{"blue3", 0, 0, 205},
	{"blue4", 0, 0, 139},
	{"blueviolet", 138, 43, 226},
	{"brown", 165, 42, 42},
	{"brown1", 255, 64, 64},
	{"brown2", 238, 59, 59},
	{"brown3", 205, 51, 51},
	{"brown4", 139, 35, 35},
	{"burlywood", 222, 184, 135},
	{"burlywood1", 255, 211, 155},
	{"burlywood2", 238, 197, 145},
	{"burlywood3", 205, 170, 125},
	{"burlywood4", 139, 115, 85},
	{"cadetblue", 95, 158, 160},
	{"cadetblue1", 152, 245, 255},
	{"cadetblue2", 142, 229, 238},
	{"cadetblue3", 122, 197, 205},
	{"cadetblue4", 83, 134, 139},
	{"chartreuse", 127, 255, 0},
	{"chartreuse1", 127, 255, 0},
	{"chartreuse2", 118, 238, 0},
	{"chartreuse3", 102, 205, 0},
	{"chartreuse4", 69, 139, 0},
	{"chocolate", 210, 105, 30},
	{"chocolate1", 255, 127, 36},
	{"chocolate2", 238, 118, 33},
	{"chocolate3", 205, 102, 29},
	{"chocolate4", 139, 69, 19},
	{"coral", 255, 127, 80},
	{"coral1", 255, 114, 86},
	{"coral2", 238, 106, 80},
	{"coral3", 205, 91, 69},
	{"coral4", 139, 62, 47},
	{"cornflowerblue", 100, 149, 237},
	{"cornsilk", 255, 248, 220},
	{"cornsilk1", 255, 248, 220},
	{"cornsilk2", 238, 232, 205},
	{"cornsilk3", 205, 200, 177},
	{"cornsilk4", 139, 136, 120},
	{"cyan", 0, 255, 255},
	{"cyan1", 0, 255, 255},
	{"cyan2", 0, 238, 238},
	{"cyan3", 0, 205, 205},
	{"cyan4", 0, 139, 139},
	{"darkblue", 0, 0, 139},
	{"darkcyan", 0, 139, 139},
	{"darkgoldenrod", 184, 134, 11},
	{"darkgoldenrod1", 255, 185, 15},
	{"darkgoldenrod2", 238, 173, 14},
	{"darkgoldenrod3", 205, 149, 12},
	{"darkgoldenrod4", 139, 101, 8},
	{"darkgray", 169, 169, 169},
	{"darkgreen", 0, 100, 0},
	{"darkgrey", 169, 169, 169},
	{"darkkhaki", 189, 183, 107},
	{"darkmagenta", 139, 0, 139},
	{"darkolivegreen", 85, 107, 47},
	{"darkolivegreen1", 202, 255, 112},
	{"darkolivegreen2", 188, 238, 104},
	{"darkolivegreen3", 162, 205, 90},
	{"darkolivegreen4", 110, 139, 61},
	{"darkorange", 255, 140, 0},
	{"darkorange1", 255, 127, 0},
	{"darkorange2", 238, 118, 0},
	{"darkorange3", 205, 102, 0},
	{"darkorange4", 139, 69, 0},
	{"darkorchid", 153, 50, 204},
	{"darkorchid1", 191, 62, 255},
	{"darkorchid2", 178, 58, 238},
	{"darkorchid3", 154, 50, 205},
	{"darkorchid4", 104, 34, 139},
	{"darkred", 139, 0, 0},
	{"darksalmon", 233, 150, 122},
	{"darkseagreen", 143, 188, 143},
	{"darkseagreen1", 193, 255, 193},
	{"darkseagreen2", 180, 238, 180},
	{"darkseagreen3", 155, 205, 155},
	{"darkseagreen4", 105, 139, 105},
	{"darkslateblue", 72, 61, 139},
	{"darkslategray", 47, 79, 79},
	{"darkslategray1", 151, 255, 255},
	{"darkslategray2", 141, 238, 238},
	{"darkslategray3", 121, 205, 205},
	{"darkslategray4", 82, 139, 139},
	{"darkslategrey", 47, 79, 79},
	{"darkturquoise", 0, 206, 209},
	{"darkviolet", 148, 0, 211},
	{"deeppink", 255, 20, 147},
	{"deeppink1", 255, 20, 147},
	{"deeppink2", 238, 18, 137},
	{"deeppink3", 205, 16, 118},
	{"deeppink4", 139, 10, 80},
	{"deepskyblue", 0, 191, 255},
	{"deepskyblue1", 0, 191, 255},
	{"deepskyblue2", 0, 178, 238},
	{"deepskyblue3", 0, 154, 205},
	{"deepskyblue4", 0, 104, 139},
	{"dimgray", 105, 105, 105},
	{"dimgrey", 105, 105, 105},
	{"dodgerblue", 30, 144, 255},
	{"dodgerblue1", 30, 144, 255},
	{"dodgerblue2", 28, 134, 238},
	{"dodgerblue3", 24, 116, 205},
	{"dodgerblue4", 16, 78, 139},
	{"firebrick", 178, 34, 34},
	{"firebrick1", 255, 48, 48},
	{"firebrick2", 238, 44, 44},
	{"firebrick3", 205, 38, 38},
	{"firebrick4", 139, 26, 26},
	{"floralwhite", 255, 250, 240},
	{"forestgreen", 34, 139, 34},
	{"gainsboro", 220, 220, 220},
	{"ghostwhite", 248, 248, 255},
	{"gold", 255, 215, 0},
	{"gold1", 255, 215, 0},
	{"gold2", 238, 201, 0},
	{"gold3", 205, 173, 0},
	{"gold4", 139, 117, 0},
	{"goldenrod", 218, 165, 32},
	{"goldenrod1", 255, 193, 37},
	{"goldenrod2", 238, 180, 34},
	{"goldenrod3", 205, 155, 29},
	{"goldenrod4", 139, 105, 20},
	{"gray", 190, 190, 190},
	{"gray0", 0, 0, 0},
	{"gray1", 3, 3, 3},
	{"gray2", 5, 5, 5},
	{"gray3", 8, 8, 8},
	{"gray4", 10, 10, 10},
	{"gray5", 13, 13, 13},
	{"gray6", 15, 15, 15},
	{"gray7", 18, 18, 18},
	{"gray8", 20, 20, 20},
	{"gray9", 23, 23, 23},
	{"gray10", 26, 26, 26},
	{"gray11", 28, 28, 28},
	{"gray12", 31, 31, 31},
	{"gray13", 33, 33, 33},
	{"gray14", 36, 36, 36},
	{"gray15", 38, 38, 38},
	{"gray16", 41, 41, 41},
	{"gray17", 43, 43, 43},
	{"gray18", 46, 46, 46},
	{"gray19", 48, 48, 48},
	{"gray20", 51, 51, 51},
	{"gray21", 54, 54, 54},
	{"gray22", 56, 56, 56},
	{"gray23", 59, 59, 59},
	{"gray24", 61, 61, 61},
	{"gray25", 64, 64, 64},
	{"gray26", 66, 66, 66},
	{"gray27", 69, 69, 69},
	{"gray28", 71, 71, 71},
	{"gray29", 74, 74, 74},
	{"gray30", 77, 77, 77},
	{"gray31", 79, 79, 79},
	{"gray32", 82, 82, 82},
	{"gray33", 84, 84, 84},
	{"gray34", 87, 87, 87},
	{"gray35", 89, 89, 89},
	{"gray36", 92, 92, 92},
	{"gray37", 94, 94, 94},
	{"gray38", 97, 97, 97},
	{"gray39", 99, 99, 99},
	{"gray40", 102, 102, 102},
	{"gray41", 105, 105, 105},
	{"gray42", 107, 107, 107},
	{"gray43", 110, 110, 110},
	{"gray44", 112, 112, 112},
	{"gray45", 115, 115, 115},
	{"gray46", 117, 117, 117},
	{"gray47", 120, 120, 120},
	{"gray48", 122, 122, 122},
	{"gray49", 125, 125, 125},
	{"gray50", 127, 127, 127},
	{"gray51", 130, 130, 130},
	{"gray52", 133, 133, 133},
	{"gray53", 135, 135, 135},
	{"gray54", 138, 138, 138},
	{"gray55", 140, 140, 140},
	{"gray56", 143, 143, 143},
	{"gray57", 145, 145, 145},
	{"gray58", 148, 148, 148},
	{"gray59", 150, 150, 150},
	{"gray60", 153, 153, 153},
	{"gray61", 156, 156, 156},
	{"gray62", 158, 158, 158},
	{"gray63", 161, 161, 161},
	{"gray64", 163, 163, 163},
	{"gray65", 166, 166, 166},
	{"gray66", 168, 168, 168},
	{"gray67", 171, 171, 171},
	{"gray68", 173, 173, 173},
	{"gray69", 176, 176, 176},
	{"gray70", 179, 179, 179},
	{"gray71", 181, 181, 181},
	{"gray72", 184, 184, 184},
	{"gray73", 186, 186, 186},
	{"gray74", 189, 189, 189},
	{"gray75", 191, 191, 191},
	{"gray76", 194, 194, 194},
	{"gray77", 196, 196, 196},
	{"gray78", 199, 199, 199},
	{"gray79", 201, 201, 201},
	{"gray80", 204, 204, 204},
	{"gray81", 207, 207, 207},
	{"gray82", 209, 209, 209},
	{"gray83", 212, 212, 212},
	{"gray84", 214, 214, 214},
	{"gray85", 217, 217, 217},
	{"gray86", 219, 219, 219},
	{"gray87", 222, 222, 222},
	{"gray88", 224, 224, 224},
	{"gray89", 227, 227, 227},
	{"gray90", 229, 229, 229},
	{"gray91", 232, 232, 232},
	{"gray92", 235, 235, 235},
	{"gray93", 237, 237, 237},
	{"gray94", 240, 240, 240},
	{"gray95", 242, 242, 242},
	{"gray96", 245, 245, 245},
	{"gray97", 247, 247, 247},
	{"gray98", 250, 250, 250},
	{"gray99", 252, 252, 252},
	{"gray100", 255, 255, 255},
	{"green", 0, 255, 0},
	{"green1", 0, 255, 0},
	{"green2", 0, 238, 0},
	{"green3", 0, 205, 0},
	{"green4", 0, 139, 0},
	{"greenyellow", 173, 255, 47},
	{"grey", 190, 190, 190},
	{"grey0", 0, 0, 0},
	{"grey1", 3, 3, 3},
	{"grey2", 5, 5, 5},
	{"grey3", 8, 8, 8},
	{"grey4", 10, 10, 10},
	{"grey5", 13, 13, 13},
	{"grey6", 15, 15, 15},
	{"grey7", 18, 18, 18},
	{"grey8", 20, 20, 20},
	{"grey9", 23, 23, 23},
	{"grey10", 26, 26, 26},
	{"grey11", 28, 28, 28},
	{"grey12", 31, 31, 31},
	{"grey13", 33, 33, 33},
	{"grey14", 36, 36, 36},
	{"grey15", 38, 38, 38},
	{"grey16", 41, 41, 41},
	{"grey17", 43, 43, 43},
	{"grey18", 46, 46, 46},
	{"grey19", 48, 48, 48},
	{"grey20", 51, 51, 51},
	{"grey21", 54, 54, 54},
	{"grey22", 56, 56, 56},
	{"grey23", 59, 59, 59},
	{"grey24", 61, 61, 61},
	{"grey25", 64, 64, 64},
	{"grey26", 66, 66, 66},
	{"grey27", 69, 69, 69},
	{"grey28", 71, 71, 71},
	{"grey29", 74, 74, 74},
	{"grey30", 77, 77, 77},
	{"grey31", 79, 79, 79},
	{"grey32", 82, 82, 82},
	{"grey33", 84, 84, 84},
	{"grey34", 87, 87, 87},
	{"grey35", 89, 89, 89},
	{"grey36", 92, 92, 92},
	{"grey37", 94, 94, 94},
	{"grey38", 97, 97, 97},
	{"grey39", 99, 99, 99},
	{"grey40", 102, 102, 102},
	{"grey41", 105, 105, 105},
	{"grey42", 107, 107, 107},
	{"grey43", 110, 110, 110},
	{"grey44", 112, 112, 112},
	{"grey45", 115, 115, 115},
	{"grey46", 117, 117, 117},
	{"grey47", 120, 120, 120},
	{"grey48", 122, 122, 122},
	{"grey49", 125, 125, 125},
	{"grey50", 127, 127, 127},
	{"grey51", 130, 130, 130},
	{"grey52", 133, 133, 133},
	{"grey53", 135, 135, 135},
	{"grey54", 138, 138, 138},
	{"grey55", 140, 140, 140},
	{"grey56", 143, 143, 143},
	{"grey57", 145, 145, 145},
	{"grey58", 148, 148, 148},
	{"grey59", 150, 150, 150},
	{"grey60", 153, 153, 153},
	{"grey61", 156, 156, 156},
	{"grey62", 158, 158, 158},
	{"grey63", 161, 161, 161},
	{"grey64", 163, 163, 163},
	{"grey65", 166, 166, 166},
	{"grey66", 168, 168, 168},
	{"grey67", 171, 171, 171},
	{"grey68", 173, 173, 173},
	{"grey69", 176, 176, 176},
	{"grey70", 179, 179, 179},
	{"grey71", 181, 181, 181},
	{"grey72", 184, 184, 184},
	{"grey73", 186, 186, 186},
	{"grey74", 189, 189, 189},
	{"grey75", 191, 191, 191},
	{"grey76", 194, 194, 194},
	{"grey77", 196, 196, 196},
	{"grey78", 199, 199, 199},
	{"grey79", 201, 201, 201},
	{"grey80", 204, 204, 204},
	{"grey81", 207, 207, 207},
	{"grey82", 209, 209, 209},
	{"grey83", 212, 212, 212},
	{"grey84", 214, 214, 214},
	{"grey85", 217, 217, 217},
	{"grey86", 219, 219, 219},
	{"grey87", 222, 222, 222},
	{"grey88", 224, 224, 224},
	{"grey89", 227, 227, 227},
	{"grey90", 229, 229, 229},
	{"grey91", 232, 232, 232},
	{"grey92", 235, 235, 235},
	{"grey93", 237, 237, 237},
	{"grey94", 240, 240, 240},
	{"grey95", 242, 242, 242},
	{"grey96", 245, 245, 245},
	{"grey97", 247, 247, 247},
	{"grey98", 250, 250, 250},
	{"grey99", 252, 252, 252},
	{"grey100", 255, 255, 255},
	{"honeydew", 240, 255, 240},
	{"honeydew1", 240, 255, 240},
	{"honeydew2", 224, 238, 224},
	{"honeydew3", 193, 205, 193},
	{"honeydew4", 131, 139, 131},
	{"hotpink", 255, 105, 180},
	{"hotpink1", 255, 110, 180},
	{"hotpink2", 238, 106, 167},
	{"hotpink3", 205, 96, 144},
	{"hotpink4", 139, 58, 98},
	{"indianred", 205, 92, 92},
	{"indianred1", 255, 106, 106},
	{"indianred2", 238, 99, 99},
	{"indianred3", 205, 85, 85},
	{"indianred4", 139, 58, 58},
	{"ivory", 255, 255, 240},
	{"ivory1", 255, 255, 240},
	{"ivory2", 238, 238, 224},
	{"ivory3", 205, 205, 193},
	{"ivory4", 139, 139, 131},
	{"khaki", 240, 230, 140},
	{"khaki1", 255, 246, 143},
	{"khaki2", 238, 230, 133},
	{"khaki3", 205, 198, 115},
	{"khaki4", 139, 134, 78},
	{"lavender", 230, 230, 250},
	{"lavenderblush", 255, 240, 245},
	{"lavenderblush1", 255, 240, 245},
	{"lavenderblush2", 238, 224, 229},
	{"lavenderblush3", 205, 193, 197},
	{"lavenderblush4", 139, 131, 134},
	{"lawngreen", 124, 252, 0},
	{"lemonchiffon", 255, 250, 205},
	{"lemonchiffon1", 255, 250, 205},
	{"lemonchiffon2", 238, 233, 191},
	{"lemonchiffon3", 205, 201, 165},
	{"lemonchiffon4", 139, 137, 112},
	{"lightblue", 173, 216, 230},
	{"lightblue1", 191, 239, 255},
	{"lightblue2", 178, 223, 238},
	{"lightblue3", 154, 192, 205},
	{"lightblue4", 104, 131, 139},
	{"lightcoral", 240, 128, 128},
	{"lightcyan", 224, 255, 255},
	{"lightcyan1", 224, 255, 255},
	{"lightcyan2", 209, 238, 238},
	{"lightcyan3", 180, 205, 205},
	{"lightcyan4", 122, 139, 139},
	{"lightgoldenrod", 238, 221, 130},
	{"lightgoldenrod1", 255, 236, 139},
	{"lightgoldenrod2", 238, 220, 130},
	{"lightgoldenrod3", 205, 190, 112},
	{"lightgoldenrod4", 139, 129, 76},
	{"lightgoldenrodyellow", 250, 250, 210},
	{"lightgray", 211, 211, 211},
	{"lightgreen", 144, 238, 144},
	{"lightgrey", 211, 211, 211},
	{"lightpink", 255, 182, 193},
	{"lightpink1", 255, 174, 185},
	{"lightpink2", 238, 162, 173},
	{"lightpink3", 205, 140, 149},
	{"lightpink4", 139, 95, 101},
	{"lightsalmon", 255, 160, 122},
	{"lightsalmon1", 255, 160, 122},
	{"lightsalmon2", 238, 149, 114},
	{"lightsalmon3", 205, 129, 98},
	{"lightsalmon4", 139, 87, 66},
	{"lightseagreen", 32, 178, 170},
	{"lightskyblue", 135, 206, 250},
	{"lightskyblue1", 176, 226, 255},
	{"lightskyblue2", 164, 211, 238},
	{"lightskyblue3", 141, 182, 205},
	{"lightskyblue4", 96, 123, 139},
	{"lightslateblue", 132, 112, 255},
	{"lightslategray", 119, 136, 153},
	{"lightslategrey", 119, 136, 153},
	{"lightsteelblue", 176, 196, 222},
	{"lightsteelblue1", 202, 225, 255},
	{"lightsteelblue2", 188, 210, 238},
	{"lightsteelblue3", 162, 181, 205},
	{"lightsteelblue4", 110, 123, 139},
	{"lightyellow", 255, 255, 224},
	{"lightyellow1", 255, 255, 224},
	{"lightyellow2", 238, 238, 209},
	{"lightyellow3", 205, 205, 180},
	{"lightyellow4", 139, 139, 122},
	{"limegreen", 50, 205, 50},
	{"linen", 250, 240, 230},
	{"magenta", 255, 0, 255},
	{"magenta1", 255, 0, 255},
	{"magenta2", 238, 0, 238},
	{"magenta3", 205, 0, 205},
	{"magenta4", 139, 0, 139},
	{"maroon", 176, 48, 96},
	{"maroon1", 255, 52, 179},
	{"maroon2", 238, 48, 167},
	{"maroon3", 205, 41, 144},
	{"maroon4", 139, 28, 98},
	{"mediumaquamarine", 102, 205, 170},
	{"mediumblue", 0, 0, 205},
	{"mediumorchid", 186, 85, 211},
	{"mediumorchid1", 224, 102, 255},
	{"mediumorchid2", 209, 95, 238},
	{"mediumorchid3", 180, 82, 205},
	{"mediumorchid4", 122, 55, 139},
	{"mediumpurple", 147, 112, 219},
	{"mediumpurple1", 171, 130, 255},
	{"mediumpurple2", 159, 121, 238},
	{"mediumpurple3", 137, 104, 205},
	{"mediumpurple4", 93, 71, 139},
	{"mediumseagreen", 60, 179, 113},
	{"mediumslateblue", 123, 104, 238},
	{"mediumspringgreen", 0, 250, 154},
	{"mediumturquoise", 72, 209, 204},
	{"mediumvioletred", 199, 21, 133},
	{"midnightblue", 25, 25, 112},
	{"mintcream", 245, 255, 250},
	{"mistyrose", 255, 228, 225},
	{"mistyrose1", 255, 228, 225},
	{"mistyrose2", 238, 213, 210},
	{"mistyrose3", 205, 183, 181},
	{"mistyrose4", 139, 125, 123},
	{"moccasin", 255, 228, 181},
	{"navajowhite", 255, 222, 173},
	{"navajowhite1", 255, 222, 173},
	{"navajowhite2", 238, 207, 161},
	{"navajowhite3", 205, 179, 139},
	{"navajowhite4", 139, 121, 94},
	{"navy", 0, 0, 128},
	{"navyblue", 0, 0, 128},
	{"oldlace", 253, 245, 230},
	{"olivedrab", 107, 142, 35},
	{"olivedrab1", 192, 255, 62},
	{"olivedrab2", 179, 238, 58},
	{"olivedrab3", 154, 205, 50},
	{"olivedrab4", 105, 139, 34},
	{"orange", 255, 165, 0},
	{"orange1", 255, 165, 0},
	{"orange2", 238, 154, 0},
	{"orange3", 205, 133, 0},
	{"orange4", 139, 90, 0},
	{"orangered", 255, 69, 0},
	{"orangered1", 255, 69, 0},
	{"orangered2", 238, 64, 0},
	{"orangered3", 205, 55, 0},
	{"orangered4", 139, 37, 0},
	{"orchid", 218, 112, 214},
	{"orchid1", 255, 131, 250},
	{"orchid2", 238, 122, 233},
	{"orchid3", 205, 105, 201},
	{"orchid4", 139, 71, 137},
	{"palegoldenrod", 238, 232, 170},
	{"palegreen", 152, 251, 152},
	{"palegreen1", 154, 255, 154},
	{"palegreen2", 144, 238, 144},
	{"palegreen3", 124, 205, 124},
	{"palegreen4", 84, 139, 84},
	{"paleturquoise", 175, 238, 238},
	{"paleturquoise1", 187, 255, 255},
	{"paleturquoise2", 174, 238, 238},
	{"paleturquoise3", 150, 205, 205},
	{"paleturquoise4", 102, 139, 139},
	{"palevioletred", 219, 112, 147},
	{"palevioletred1", 255, 130, 171},
	{"palevioletred2", 238, 121, 159},
	{"palevioletred3", 205, 104, 137},
	{"palevioletred4", 139, 71, 93},
	{"papayawhip", 255, 239, 213},
	{"peachpuff", 255, 218, 185},
	{"peachpuff1", 255, 218, 185},
	{"peachpuff2", 238, 203, 173},
	{"peachpuff3", 205, 175, 149},
	{"peachpuff4", 139, 119, 101},
	{"peru", 205, 133, 63},
	{"pink", 255, 192, 203},
	{"pink1", 255, 181, 197},
	{"pink2", 238, 169, 184},
	{"pink3", 205, 145, 158},
	{"pink4", 139, 99, 108},
	{"plum", 221, 160, 221},
	{"plum1", 255, 187, 255},
	{"plum2", 238, 174, 238},
	{"plum3", 205, 150, 205},
	{"plum4", 139, 102, 139},
	{"powderblue", 176, 224, 230},
	{"purple", 160, 32, 240},
	{"purple1", 155, 48, 255},
	{"purple2", 145, 44, 238},
	{"purple3", 125, 38, 205},
	{"purple4", 85, 26, 139},
	{"red", 255, 0, 0},
	{"red1", 255, 0, 0},
	{"red2", 238, 0, 0},
	{"red3", 205, 0, 0},
	{"red4", 139, 0, 0},
	{"rosybrown", 188, 143, 143},
	{"rosybrown1", 255, 193, 193},
	{"rosybrown2", 238, 180, 180},
	{"rosybrown3", 205, 155, 155},
	{"rosybrown4", 139, 105, 105},
	{"royalblue", 65, 105, 225},
	{"royalblue1", 72, 118, 255},
	{"royalblue2", 67, 110, 238},
	{"royalblue3", 58, 95, 205},
	{"royalblue4", 39, 64, 139},
	{"saddlebrown", 139, 69, 19},
	{"salmon", 250, 128, 114},
	{"salmon1", 255, 140, 105},
	{"salmon2", 238, 130, 98},
	{"salmon3", 205, 112, 84},
	{"salmon4", 139, 76, 57},
	{"sandybrown", 244, 164, 96},
	{"seagreen", 46, 139, 87},
	{"seagreen1", 84, 255, 159},
	{"seagreen2", 78, 238, 148},
	{"seagreen3", 67, 205, 128},
	{"seagreen4", 46, 139, 87},
	{"seashell", 255, 245, 238},
	{"seashell1", 255, 245, 238},
	{"seashell2", 238, 229, 222},
	{"seashell3", 205, 197, 191},
	{"seashell4", 139, 134, 130},
	{"sienna", 160, 82, 45},
	{"sienna1", 255, 130, 71},
	{"sienna2", 238, 121, 66},
	{"sienna3", 205, 104, 57},
	{"sienna4", 139, 71, 38},
	{"skyblue", 135, 206, 235},
	{"skyblue1", 135, 206, 255},
	{"skyblue2", 126, 192, 238},
	{"skyblue3", 108, 166, 205},
	{"skyblue4", 74, 112, 139},
	{"slateblue", 106, 90, 205},
	{"slateblue1", 131, 111, 255},
	{"slateblue2", 122, 103, 238},
	{"slateblue3", 105, 89, 205},
	{"slateblue4", 71, 60, 139},
	{"slategray", 112, 128, 144},
	{"slategray1", 198, 226, 255},
	{"slategray2", 185, 211, 238},
	{"slategray3", 159, 182, 205},
	{"slategray4", 108, 123, 139},
	{"slategrey", 112, 128, 144},
	{"snow", 255, 250, 250},
	{"snow1", 255, 250, 250},
	{"snow2", 238, 233, 233},
	{"snow3", 205, 201, 201},
	{"snow4", 139, 137, 137},
	{"springgreen", 0, 255, 127},
	{"springgreen1", 0, 255, 127},
	{"springgreen2", 0, 238, 118},
	{"springgreen3", 0, 205, 102},
	{"springgreen4", 0, 139, 69},
	{"steelblue", 70, 130, 180},
	{"steelblue1", 99, 184, 255},
	{"steelblue2", 92, 172, 238},
	{"steelblue3", 79, 148, 205},
	{"steelblue4", 54, 100, 139},
	{"tan", 210, 180, 140},
	{"tan1", 255, 165, 79},
	{"tan2", 238, 154, 73},
	{"tan3", 205, 133, 63},
	{"tan4", 139, 90, 43},
	{"thistle", 216, 191, 216},
	{"thistle1", 255, 225, 255},
	{"thistle2", 238, 210, 238},
	{"thistle3", 205, 181, 205},
	{"thistle4", 139, 123, 139},
	{"tomato", 255, 99, 71},
	{"tomato1", 255, 99, 71},
	{"tomato2", 238, 92, 66},
	{"tomato3", 205, 79, 57},
	{"tomato4", 139, 54, 38},
	{"turquoise", 64, 224, 208},
	{"turquoise1", 0, 245, 255},
	{"turquoise2", 0, 229, 238},
	{"turquoise3", 0, 197, 205},
	{"turquoise4", 0, 134, 139},
	{"violet", 238, 130, 238},
	{"violetred", 208, 32, 144},
	{"violetred1", 255, 62, 150},
	{"violetred2", 238, 58, 140},
	{"violetred3", 205, 50, 120},
	{"violetred4", 139, 34, 82},
	{"wheat", 245, 222, 179},
	{"wheat1", 255, 231, 186},
	{"wheat2", 238, 216, 174},
	{"wheat3", 205, 186, 150},
	{"wheat4", 139, 126, 102},
	{"whitesmoke", 245, 245, 245},
	{"yellow", 255, 255, 0},
	{"yellow1", 255, 255, 0},
	{"yellow2", 238, 238, 0},
	{"yellow3", 205, 205, 0},
	{"yellow4", 139, 139, 0},
	{"yellowgreen", 154, 205, 50},
	{nullptr, 0, 0, 0}
};

void SLiMGetColorComponents(std::string &p_color_name, float *p_red_component, float *p_green_component, float *p_blue_component)
{
	// Colors can be specified either in hex as "#RRGGBB" or as a named color from the list above
	if ((p_color_name.length() == 7) && (p_color_name[0] == '#'))
	{
		try
		{
			unsigned long r = stoul(p_color_name.substr(1, 2), nullptr, 16);
			unsigned long g = stoul(p_color_name.substr(3, 2), nullptr, 16);
			unsigned long b = stoul(p_color_name.substr(5, 2), nullptr, 16);
			
			*p_red_component = r / 255.0f;
			*p_green_component = g / 255.0f;
			*p_blue_component = b / 255.0f;
			return;
		}
		catch (std::runtime_error err)
		{
			EIDOS_TERMINATION << "ERROR (SLiMGetColorComponents): color specification \"" << p_color_name << "\" is malformed." << eidos_terminate();
		}
	}
	else
	{
		for (SLiMNamedColor *color_table = gSLiMNamedColors; color_table->name; ++color_table)
		{
			if (p_color_name == color_table->name)
			{
				*p_red_component = color_table->red / 255.0f;
				*p_green_component = color_table->green / 255.0f;
				*p_blue_component = color_table->blue / 255.0f;
				return;
			}
		}
	}
	
	EIDOS_TERMINATION << "ERROR (SLiMGetColorComponents): color named \"" << p_color_name << "\" could not be found." << eidos_terminate();
}























































