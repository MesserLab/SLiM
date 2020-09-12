//
//  slim_functions.h
//  SLiM
//
//  Created by Ben Haller on 12/26/14.
//  Copyright (c) 2014-2020 Philipp Messer.  All rights reserved.
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


#ifndef slim_functions_h
#define slim_functions_h


#include <stdio.h>

#include "eidos_value.h"
#include "eidos_interpreter.h"


// SLiM built-in functions; the signatures for these are declared in SLiMSim::SLiMFunctionSignatures()

EidosValue_SP SLiM_ExecuteFunction_codonsToAminoAcids(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP SLiM_ExecuteFunction_mm16To256(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP SLiM_ExecuteFunction_mmJukesCantor(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP SLiM_ExecuteFunction_mmKimura(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP SLiM_ExecuteFunction_nucleotideCounts(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP SLiM_ExecuteFunction_nucleotideFrequencies(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP SLiM_ExecuteFunction_nucleotidesToCodons(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP SLiM_ExecuteFunction_randomNucleotides(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP SLiM_ExecuteFunction_codonsToNucleotides(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);


#endif /* slim_functions_h */































