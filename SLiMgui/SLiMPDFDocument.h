//
//  SLiMPDFDocument.h
//  SLiM
//
//  Created by Ben Haller on 4/20/17.
//  Copyright (c) 2017-2020 Philipp Messer.  All rights reserved.
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


#import <Cocoa/Cocoa.h>


@interface SLiMPDFDocument : NSDocument
{
	NSData *pdfData;			// we hold onto the NSData we are given as our "model"
	NSDate *modificationDate;	// we have to track the mod date of our file to tell when it changes
}
@end
























































