//
//  ScriptMod_AddMutationType.h
//  SLiM
//
//  Created by Ben Haller on 3/21/15.
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


@interface ScriptMod_AddMutationType : ScriptMod
{
	IBOutlet NSTextField *mutationTypeTextField;
	IBOutlet NSTextField *dominanceCoeffTextField;
	IBOutlet NSMatrix *dfeMatrix;
	IBOutlet NSTextField *fixedDFEParamsLabel;
	IBOutlet NSTextField *fixedSelCoeffTextField;
	IBOutlet NSTextField *expDFEParamsLabel;
	IBOutlet NSTextField *expMeanSelCoeffTextField;
	IBOutlet NSTextField *gammaDFEParamsLabel1;
	IBOutlet NSTextField *gammaDFEParamsLabel2;
	IBOutlet NSTextField *gammaMeanSelCoeffTextField;
	IBOutlet NSTextField *gammaAlphaTextField;
	IBOutlet NSTextField *normalDFEParamsLabel1;
	IBOutlet NSTextField *normalDFEParamsLabel2;
	IBOutlet NSTextField *normalMeanSelCoeffTextField;
	IBOutlet NSTextField *normalSigmaTextField;
	IBOutlet NSTextField *weibullDFEParamsLabel1;
	IBOutlet NSTextField *weibullDFEParamsLabel2;
	IBOutlet NSTextField *weibullLambdaTextField;
	IBOutlet NSTextField *weibullKTextField;
}
@end















































































