//
//  slim_test.h
//  SLiM
//
//  Created by Ben Haller on 8/14/15.
//  Copyright (c) 2015-2020 Philipp Messer.  All rights reserved.
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

#ifndef __SLiM__slim_test__
#define __SLiM__slim_test__

#include <stdio.h>
#include <string>


int RunSLiMTests(void);


// Helper functions for testing
extern void SLiMAssertScriptSuccess(const std::string &p_script_string, int p_lineNumber = -1);
extern void SLiMAssertScriptRaise(const std::string &p_script_string, const int p_bad_line, const int p_bad_position, const std::string &p_reason_snip, int p_lineNumber = -1);
extern void SLiMAssertScriptStop(const std::string &p_script_string, int p_lineNumber = -1);


// Conceptually, all the slim_test_X.cpp stuff is a single source file, and all the details below are private.
// It is split into multiple files to improve compile performance; the single source file took almost a minute to compile


// Defined in various slim_test_X.cpp files
extern void _RunInitTests(void);
extern void _RunSLiMSimTests(std::string temp_path);
extern void _RunMutationTypeTests(void);
extern void _RunGenomicElementTypeTests(void);
extern void _RunGenomicElementTests(void);
extern void _RunChromosomeTests(void);
extern void _RunMutationTests(void);
extern void _RunGenomeTests(std::string temp_path);
extern void _RunSubpopulationTests(void);
extern void _RunIndividualTests(void);
extern void _RunInteractionTypeTests(void);
extern void _RunSubstitutionTests(void);
extern void _RunSLiMEidosBlockTests(void);
extern void _RunContinuousSpaceTests(void);
extern void _RunNonWFTests(void);
extern void _RunTreeSeqTests(std::string temp_path);
extern void _RunNucleotideFunctionTests(void);
extern void _RunNucleotideMethodTests(void);

// Test function shared strings
extern std::string gen1_setup;
extern std::string gen1_setup_sex;
extern std::string gen2_stop;
extern std::string gen1_setup_highmut_p1;
extern std::string gen1_setup_fixmut_p1;
extern std::string gen1_setup_i1;
extern std::string gen1_setup_i1x;
extern std::string gen1_setup_i1xPx;
extern std::string gen1_setup_i1xyz;
extern std::string gen1_setup_i1xyzPxz;
extern std::string gen1_setup_p1;
extern std::string gen1_setup_sex_p1;
extern std::string gen1_setup_p1p2p3;
extern std::string WF_prefix;
extern std::string nonWF_prefix;


#endif /* defined(__SLiM__slim_test__) */































































