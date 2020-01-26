//
//  ScriptMod_AddGenomicElementType.h
//  SLiM
//
//  Created by Ben Haller on 3/22/15.
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


#import "ScriptMod.h"


@interface ScriptMod_AddGenomicElementType : ScriptMod
{
	IBOutlet NSTextField *genomicElementTypeTextField;
	
	IBOutlet NSPopUpButton *mutationType1PopUp;
	IBOutlet NSTextField *mutationType1ProportionLabel;
	IBOutlet NSTextField *mutationType1ProportionTextField;
	
	IBOutlet NSPopUpButton *mutationType2PopUp;
	IBOutlet NSTextField *mutationType2ProportionLabel;
	IBOutlet NSTextField *mutationType2ProportionTextField;
	
	IBOutlet NSPopUpButton *mutationType3PopUp;
	IBOutlet NSTextField *mutationType3ProportionLabel;
	IBOutlet NSTextField *mutationType3ProportionTextField;
}
@end















































































