//
//  eidos_beep.h
//  Eidos
//
//  Created by Ben Haller on 5/4/16.
//  Copyright (c) 2016-2019 Philipp Messer.  All rights reserved.
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


#ifndef __Eidos__eidos_beep__
#define __Eidos__eidos_beep__


#include <string>


// This function is defined in eidos_beep.cpp in the command-line case, and in EidosCocoaExtra.mm in the GUI case, since
// we want a completely different implementation in the two cases.
std::string Eidos_Beep(std::string p_sound_name);


#endif /* __Eidos__eidos_beep__ */
























































