//
//  QtSLiMEidosPrettyprinter.h
//  SLiM
//
//  Created by Ben Haller on 8/1/2019.
//  Copyright (c) 2019-2020 Philipp Messer.  All rights reserved.
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

#ifndef QTSLIMEIDOSPRETTYPRINTER_H
#define QTSLIMEIDOSPRETTYPRINTER_H


#include <string>
#include <eidos_token.h>

class EidosScript;


// Generate a prettyprinted script string into parameter pretty, from the tokens and script supplied
bool Eidos_prettyprintTokensFromScript(const std::vector<EidosToken> &tokens, EidosScript &tokenScript, std::string &pretty);

bool Eidos_reformatTokensFromScript(const std::vector<EidosToken> &tokens, EidosScript &tokenScript, std::string &pretty);


#endif // QTSLIMEIDOSPRETTYPRINTER_H































