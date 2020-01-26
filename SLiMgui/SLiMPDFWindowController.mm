//
//  SLiMPDFWindowController.mm
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


#import "SLiMPDFWindowController.h"


@implementation SLiMPDFWindowController

- (instancetype)init
{
	if (self = [super initWithWindowNibName:@"SLiMPDFWindow"])
	{
	}
	
	return self;
}

- (void)dealloc
{
	pdfView = nil;
	[self setDisplayPDFDocument:nil];
	
	[super dealloc];
}

- (void)windowDidLoad
{
    [super windowDidLoad];
    
	[pdfView setDocument:_displayPDFDocument];
}

- (void)setDisplayPDFDocument:(PDFDocument *)doc
{
	if (doc != _displayPDFDocument)
	{
		[doc retain];
		[_displayPDFDocument release];
		_displayPDFDocument = doc;
		
		[pdfView setDocument:_displayPDFDocument];
	}
}

@end














































