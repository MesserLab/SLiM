//
//  eidos_beep.cpp
//  Eidos
//
//  Created by Ben Haller on 5/4/16.
//  Copyright (c) 2016-2020 Philipp Messer.  All rights reserved.
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


#include "eidos_beep.h"

#include <stdio.h>
#include <sys/stat.h> 
#include <fcntl.h>
#include <unistd.h>


// The base implementation, defined below
static std::string Eidos_Beep_BASE(std::string p_sound_name);

// The function pointer that delivers whatever platform-specific implementation we want to use
std::string (*Eidos_Beep)(std::string p_sound_name) = &Eidos_Beep_BASE;


// This code is derived from the code of the beep utility, by Johnathan Nightingale, found at https://github.com/johnath/beep
// Much of the guts of that code has been removed, since we don't need the command-line parsing, and we don't want to
// generate tones of specified frequency etc., so this is pretty trivial, but still, credit where credit is due...

/* This code is copyright (C) Johnathan Nightingale, 2000.
 *
 * This code may distributed only under the terms of the GNU Public License 
 * which can be found at http://www.gnu.org/copyleft or in the file COPYING 
 * supplied with this code.
 *
 * This code is not distributed with warranties of any kind, including implied
 * warranties of merchantability or fitness for a particular use or ability to 
 * breed pandas in captivity, it just can't be done.
 *
 */

std::string Eidos_Beep_BASE(std::string p_sound_name)
{
#pragma unused (p_sound_name)
	// Find a device to output to.  The beep utility uses /dev/tty0 or /dev/vc/0, but those do not seem to work on OS X;
	// I think perhaps this usage of them is a Linux-only thing?  No idea.  The mysteries of Unix.  So on OS X, we fall
	// through to the printf("\a"), which outputs a \a to the output stream, unfortunately; undesirable but apparently
	// unavoidable.  We will have to document that using beep() may result in \a characters in the output stream.
	int console_fd = -1;
	
	if ((console_fd = open("/dev/tty0", O_WRONLY)) == -1)
		console_fd = open("/dev/vc/0", O_WRONLY);
	
	if (console_fd == -1)
	{
		static bool been_here = false;
		
		printf("\a");	// Output the only beep we can; note this might corrupt the output stream...
		
		// Output an error message only once...
		if (been_here)
			return "";
		
		been_here = true;
		return "#WARNING (Eidos_Beep_BASE): function beep() could not open /dev/tty0 or /dev/vc/0 for writing; output stream may contain control characters to produce beeps.";
	}
	
	// Output a '\a', which rings the bell.  All the other stuff in the beep utility seems to be Linux-specific...
	putchar('\a');
	
	// Close the console to clean up
	close(console_fd);
	
	return "";
}



































































