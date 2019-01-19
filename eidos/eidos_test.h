//
//  eidos_test.h
//  Eidos
//
//  Created by Ben Haller on 4/7/15.
//  Copyright (c) 2015-2019 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/slim/
//

//	This file is part of Eidos.
//
//	Eidos is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
//
//	Eidos is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License along with Eidos.  If not, see <http://www.gnu.org/licenses/>.

/*
 
 This file contains code to test Eidos.
 
 */

#ifndef __Eidos__eidos_test__
#define __Eidos__eidos_test__

#include <string>


int RunEidosTests(void);


// Can turn on escape sequences to color test output; at present we turn these on for the command-line
// tools and off for the GUI tools, since Terminal supports these codes but Xcode does not.
#ifdef EIDOS_GUI

#define EIDOS_OUTPUT_FAILURE_TAG	"FAILURE"
#define EIDOS_OUTPUT_SUCCESS_TAG	"SUCCESS"

#else

#define EIDOS_OUTPUT_FAILURE_TAG	"\e[31mFAILURE\e[0m"
#define EIDOS_OUTPUT_SUCCESS_TAG	"\e[32mSUCCESS\e[0m"

#endif


#endif /* defined(__Eidos__eidos_test__) */































































