//
//  slim_global.cpp
//  SLiM
//
//  Created by Ben Haller on 1/4/15.
//  Copyright (c) 2014 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/software/
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


// zero-generation functions defined by SLiMSim
const std::string gStr_addGenomicElement0 = "addGenomicElement0";
const std::string gStr_addGenomicElementType0 = "addGenomicElementType0";
const std::string gStr_addMutationType0 = "addMutationType0";
const std::string gStr_addRecombinationIntervals0 = "addRecombinationIntervals0";
const std::string gStr_setGeneConversion0 = "setGeneConversion0";
const std::string gStr_setGenerationRange0 = "setGenerationRange0";
const std::string gStr_setMutationRate0 = "setMutationRate0";
const std::string gStr_setRandomSeed0 = "setRandomSeed0";
const std::string gStr_setSexEnabled0 = "setSexEnabled0";

// mostly property names
const std::string gStr_genomicElements = "genomicElements";
const std::string gStr_lastPosition = "lastPosition";
const std::string gStr_overallRecombinationRate = "overallRecombinationRate";
const std::string gStr_recombinationEndPositions = "recombinationEndPositions";
const std::string gStr_recombinationRates = "recombinationRates";
const std::string gStr_geneConversionFraction = "geneConversionFraction";
const std::string gStr_geneConversionMeanLength = "geneConversionMeanLength";
const std::string gStr_overallMutationRate = "overallMutationRate";
const std::string gStr_genomeType = "genomeType";
const std::string gStr_isNullGenome = "isNullGenome";
const std::string gStr_mutations = "mutations";
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
const std::string gStr_distributionType = "distributionType";
const std::string gStr_distributionParams = "distributionParams";
const std::string gStr_dominanceCoeff = "dominanceCoeff";
const std::string gStr_start = "start";
const std::string gStr_end = "end";
const std::string gStr_source = "source";
const std::string gStr_active = "active";
const std::string gStr_chromosome = "chromosome";
const std::string gStr_chromosomeType = "chromosomeType";
const std::string gStr_genomicElementTypes = "genomicElementTypes";
const std::string gStr_scriptBlocks = "scriptBlocks";
const std::string gStr_sexEnabled = "sexEnabled";
const std::string gStr_subpopulations = "subpopulations";
const std::string gStr_substitutions = "substitutions";
const std::string gStr_dominanceCoeffX = "dominanceCoeffX";
const std::string gStr_duration = "duration";
const std::string gStr_generation = "generation";
const std::string gStr_randomSeed = "randomSeed";
const std::string gStr_tag = "tag";
const std::string gStr_firstMaleIndex = "firstMaleIndex";
const std::string gStr_genomes = "genomes";
const std::string gStr_immigrantSubpopIDs = "immigrantSubpopIDs";
const std::string gStr_immigrantSubpopFractions = "immigrantSubpopFractions";
const std::string gStr_selfingFraction = "selfingFraction";
const std::string gStr_sexRatio = "sexRatio";
const std::string gStr_fixationTime = "fixationTime";

// mostly method names
const std::string gStr_setRecombinationIntervals = "setRecombinationIntervals";
const std::string gStr_addMutations = "addMutations";
const std::string gStr_addNewDrawnMutation = "addNewDrawnMutation";
const std::string gStr_addNewMutation = "addNewMutation";
const std::string gStr_removeMutations = "removeMutations";
const std::string gStr_setGenomicElementType = "setGenomicElementType";
const std::string gStr_setMutationFractions = "setMutationFractions";
const std::string gStr_setSelectionCoeff = "setSelectionCoeff";
const std::string gStr_setDistribution = "setDistribution";
const std::string gStr_addSubpop = "addSubpop";
const std::string gStr_addSubpopSplit = "addSubpopSplit";
const std::string gStr_deregisterScriptBlock = "deregisterScriptBlock";
const std::string gStr_mutationFrequencies = "mutationFrequencies";
const std::string gStr_outputFixedMutations = "outputFixedMutations";
const std::string gStr_outputFull = "outputFull";
const std::string gStr_outputMutations = "outputMutations";
const std::string gStr_readFromPopulationFile = "readFromPopulationFile";
const std::string gStr_registerScriptEvent = "registerScriptEvent";
const std::string gStr_registerScriptFitnessCallback = "registerScriptFitnessCallback";
const std::string gStr_registerScriptMateChoiceCallback = "registerScriptMateChoiceCallback";
const std::string gStr_registerScriptModifyChildCallback = "registerScriptModifyChildCallback";
const std::string gStr_setMigrationRates = "setMigrationRates";
const std::string gStr_setCloningRate = "setCloningRate";
const std::string gStr_setSelfingRate = "setSelfingRate";
const std::string gStr_setSexRatio = "setSexRatio";
const std::string gStr_setSubpopulationSize = "setSubpopulationSize";
const std::string gStr_fitness = "fitness";
const std::string gStr_outputMSSample = "outputMSSample";
const std::string gStr_outputSample = "outputSample";

// mostly SLiM variable names used in callbacks and such
const std::string gStr_sim = "sim";
const std::string gStr_self = "self";
const std::string gStr_genome1 = "genome1";
const std::string gStr_genome2 = "genome2";
const std::string gStr_subpop = "subpop";
const std::string gStr_sourceSubpop = "sourceSubpop";
const std::string gStr_weights = "weights";
const std::string gStr_childGenome1 = "childGenome1";
const std::string gStr_childGenome2 = "childGenome2";
const std::string gStr_childIsFemale = "childIsFemale";
const std::string gStr_parent1Genome1 = "parent1Genome1";
const std::string gStr_parent1Genome2 = "parent1Genome2";
const std::string gStr_isCloning = "isCloning";
const std::string gStr_isSelfing = "isSelfing";
const std::string gStr_parent2Genome1 = "parent2Genome1";
const std::string gStr_parent2Genome2 = "parent2Genome2";
const std::string gStr_mut = "mut";
const std::string gStr_relFitness = "relFitness";
const std::string gStr_homozygous = "homozygous";

// mostly SLiM element types
const std::string gStr_Chromosome = "Chromosome";
const std::string gStr_Genome = "Genome";
const std::string gStr_GenomicElement = "GenomicElement";
const std::string gStr_GenomicElementType = "GenomicElementType";
const std::string gStr_Mutation = "Mutation";
const std::string gStr_MutationType = "MutationType";
const std::string gStr_SLiMScriptBlock = "SLiMScriptBlock";
const std::string gStr_SLiMSim = "SLiMSim";
const std::string gStr_Subpopulation = "Subpopulation";
const std::string gStr_Substitution = "Substitution";

// mostly other fixed strings
const std::string gStr_Autosome = "Autosome";
const std::string gStr_X_chromosome = "X chromosome";
const std::string gStr_Y_chromosome = "Y chromosome";
const std::string gStr_event = "event";
const std::string gStr_mateChoice = "mateChoice";
const std::string gStr_modifyChild = "modifyChild";


void SLiM_RegisterGlobalStringsAndIDs(void)
{
	static bool been_here = false;
	
	if (!been_here)
	{
		been_here = true;
		
		SLiMScript_RegisterStringForGlobalID(gStr_addGenomicElement0, gID_addGenomicElement0);
		SLiMScript_RegisterStringForGlobalID(gStr_addGenomicElementType0, gID_addGenomicElementType0);
		SLiMScript_RegisterStringForGlobalID(gStr_addMutationType0, gID_addMutationType0);
		SLiMScript_RegisterStringForGlobalID(gStr_addRecombinationIntervals0, gID_addRecombinationIntervals0);
		SLiMScript_RegisterStringForGlobalID(gStr_setGeneConversion0, gID_setGeneConversion0);
		SLiMScript_RegisterStringForGlobalID(gStr_setGenerationRange0, gID_setGenerationRange0);
		SLiMScript_RegisterStringForGlobalID(gStr_setMutationRate0, gID_setMutationRate0);
		SLiMScript_RegisterStringForGlobalID(gStr_setRandomSeed0, gID_setRandomSeed0);
		SLiMScript_RegisterStringForGlobalID(gStr_setSexEnabled0, gID_setSexEnabled0);
		SLiMScript_RegisterStringForGlobalID(gStr_genomicElements, gID_genomicElements);
		SLiMScript_RegisterStringForGlobalID(gStr_lastPosition, gID_lastPosition);
		SLiMScript_RegisterStringForGlobalID(gStr_overallRecombinationRate, gID_overallRecombinationRate);
		SLiMScript_RegisterStringForGlobalID(gStr_recombinationEndPositions, gID_recombinationEndPositions);
		SLiMScript_RegisterStringForGlobalID(gStr_recombinationRates, gID_recombinationRates);
		SLiMScript_RegisterStringForGlobalID(gStr_geneConversionFraction, gID_geneConversionFraction);
		SLiMScript_RegisterStringForGlobalID(gStr_geneConversionMeanLength, gID_geneConversionMeanLength);
		SLiMScript_RegisterStringForGlobalID(gStr_overallMutationRate, gID_overallMutationRate);
		SLiMScript_RegisterStringForGlobalID(gStr_genomeType, gID_genomeType);
		SLiMScript_RegisterStringForGlobalID(gStr_isNullGenome, gID_isNullGenome);
		SLiMScript_RegisterStringForGlobalID(gStr_mutations, gID_mutations);
		SLiMScript_RegisterStringForGlobalID(gStr_genomicElementType, gID_genomicElementType);
		SLiMScript_RegisterStringForGlobalID(gStr_startPosition, gID_startPosition);
		SLiMScript_RegisterStringForGlobalID(gStr_endPosition, gID_endPosition);
		SLiMScript_RegisterStringForGlobalID(gStr_id, gID_id);
		SLiMScript_RegisterStringForGlobalID(gStr_mutationTypes, gID_mutationTypes);
		SLiMScript_RegisterStringForGlobalID(gStr_mutationFractions, gID_mutationFractions);
		SLiMScript_RegisterStringForGlobalID(gStr_mutationType, gID_mutationType);
		SLiMScript_RegisterStringForGlobalID(gStr_originGeneration, gID_originGeneration);
		SLiMScript_RegisterStringForGlobalID(gStr_position, gID_position);
		SLiMScript_RegisterStringForGlobalID(gStr_selectionCoeff, gID_selectionCoeff);
		SLiMScript_RegisterStringForGlobalID(gStr_subpopID, gID_subpopID);
		SLiMScript_RegisterStringForGlobalID(gStr_distributionType, gID_distributionType);
		SLiMScript_RegisterStringForGlobalID(gStr_distributionParams, gID_distributionParams);
		SLiMScript_RegisterStringForGlobalID(gStr_dominanceCoeff, gID_dominanceCoeff);
		SLiMScript_RegisterStringForGlobalID(gStr_start, gID_start);
		SLiMScript_RegisterStringForGlobalID(gStr_end, gID_end);
		SLiMScript_RegisterStringForGlobalID(gStr_source, gID_source);
		SLiMScript_RegisterStringForGlobalID(gStr_active, gID_active);
		SLiMScript_RegisterStringForGlobalID(gStr_chromosome, gID_chromosome);
		SLiMScript_RegisterStringForGlobalID(gStr_chromosomeType, gID_chromosomeType);
		SLiMScript_RegisterStringForGlobalID(gStr_genomicElementTypes, gID_genomicElementTypes);
		SLiMScript_RegisterStringForGlobalID(gStr_scriptBlocks, gID_scriptBlocks);
		SLiMScript_RegisterStringForGlobalID(gStr_sexEnabled, gID_sexEnabled);
		SLiMScript_RegisterStringForGlobalID(gStr_subpopulations, gID_subpopulations);
		SLiMScript_RegisterStringForGlobalID(gStr_substitutions, gID_substitutions);
		SLiMScript_RegisterStringForGlobalID(gStr_dominanceCoeffX, gID_dominanceCoeffX);
		SLiMScript_RegisterStringForGlobalID(gStr_duration, gID_duration);
		SLiMScript_RegisterStringForGlobalID(gStr_generation, gID_generation);
		SLiMScript_RegisterStringForGlobalID(gStr_randomSeed, gID_randomSeed);
		SLiMScript_RegisterStringForGlobalID(gStr_tag, gID_tag);
		SLiMScript_RegisterStringForGlobalID(gStr_firstMaleIndex, gID_firstMaleIndex);
		SLiMScript_RegisterStringForGlobalID(gStr_genomes, gID_genomes);
		SLiMScript_RegisterStringForGlobalID(gStr_immigrantSubpopIDs, gID_immigrantSubpopIDs);
		SLiMScript_RegisterStringForGlobalID(gStr_immigrantSubpopFractions, gID_immigrantSubpopFractions);
		SLiMScript_RegisterStringForGlobalID(gStr_selfingFraction, gID_selfingFraction);
		SLiMScript_RegisterStringForGlobalID(gStr_sexRatio, gID_sexRatio);
		SLiMScript_RegisterStringForGlobalID(gStr_fixationTime, gID_fixationTime);
		SLiMScript_RegisterStringForGlobalID(gStr_setRecombinationIntervals, gID_setRecombinationIntervals);
		SLiMScript_RegisterStringForGlobalID(gStr_addMutations, gID_addMutations);
		SLiMScript_RegisterStringForGlobalID(gStr_addNewDrawnMutation, gID_addNewDrawnMutation);
		SLiMScript_RegisterStringForGlobalID(gStr_addNewMutation, gID_addNewMutation);
		SLiMScript_RegisterStringForGlobalID(gStr_removeMutations, gID_removeMutations);
		SLiMScript_RegisterStringForGlobalID(gStr_setGenomicElementType, gID_setGenomicElementType);
		SLiMScript_RegisterStringForGlobalID(gStr_setMutationFractions, gID_setMutationFractions);
		SLiMScript_RegisterStringForGlobalID(gStr_setSelectionCoeff, gID_setSelectionCoeff);
		SLiMScript_RegisterStringForGlobalID(gStr_setDistribution, gID_setDistribution);
		SLiMScript_RegisterStringForGlobalID(gStr_addSubpop, gID_addSubpop);
		SLiMScript_RegisterStringForGlobalID(gStr_addSubpopSplit, gID_addSubpopSplit);
		SLiMScript_RegisterStringForGlobalID(gStr_deregisterScriptBlock, gID_deregisterScriptBlock);
		SLiMScript_RegisterStringForGlobalID(gStr_mutationFrequencies, gID_mutationFrequencies);
		SLiMScript_RegisterStringForGlobalID(gStr_outputFixedMutations, gID_outputFixedMutations);
		SLiMScript_RegisterStringForGlobalID(gStr_outputFull, gID_outputFull);
		SLiMScript_RegisterStringForGlobalID(gStr_outputMutations, gID_outputMutations);
		SLiMScript_RegisterStringForGlobalID(gStr_readFromPopulationFile, gID_readFromPopulationFile);
		SLiMScript_RegisterStringForGlobalID(gStr_registerScriptEvent, gID_registerScriptEvent);
		SLiMScript_RegisterStringForGlobalID(gStr_registerScriptFitnessCallback, gID_registerScriptFitnessCallback);
		SLiMScript_RegisterStringForGlobalID(gStr_registerScriptMateChoiceCallback, gID_registerScriptMateChoiceCallback);
		SLiMScript_RegisterStringForGlobalID(gStr_registerScriptModifyChildCallback, gID_registerScriptModifyChildCallback);
		SLiMScript_RegisterStringForGlobalID(gStr_setMigrationRates, gID_setMigrationRates);
		SLiMScript_RegisterStringForGlobalID(gStr_setCloningRate, gID_setCloningRate);
		SLiMScript_RegisterStringForGlobalID(gStr_setSelfingRate, gID_setSelfingRate);
		SLiMScript_RegisterStringForGlobalID(gStr_setSexRatio, gID_setSexRatio);
		SLiMScript_RegisterStringForGlobalID(gStr_setSubpopulationSize, gID_setSubpopulationSize);
		SLiMScript_RegisterStringForGlobalID(gStr_fitness, gID_fitness);
		SLiMScript_RegisterStringForGlobalID(gStr_outputMSSample, gID_outputMSSample);
		SLiMScript_RegisterStringForGlobalID(gStr_outputSample, gID_outputSample);
	}
}
























































