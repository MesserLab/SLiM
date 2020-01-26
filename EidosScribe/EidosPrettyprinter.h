//
//  EidosPrettyprinter.h
//  SLiM
//
//  Created by Ben Haller on 7/30/17.
//  Copyright (c) 2017-2020 Philipp Messer.  All rights reserved.
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


#import <Foundation/Foundation.h>

#include <vector>

#include "eidos_script.h"


@interface EidosPrettyprinter : NSObject
{
}

// Prettyprint the given token stream into an NSMutableString
+ (BOOL)prettyprintTokens:(const std::vector<EidosToken> &)tokens fromScript:(EidosScript &)tokenScript intoString:(NSMutableString *)pretty;

@end
































