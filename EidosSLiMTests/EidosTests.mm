//
//  EidosTests.m
//  SLiM
//
//  Created by Ben Haller on 1/27/17.
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


#import <XCTest/XCTest.h>

#import "eidos_test.h"


@interface EidosTests : XCTestCase

@end

@implementation EidosTests

- (void)setUp {
    [super setUp];
    // Put setup code here. This method is called before the invocation of each test method in the class.
}

- (void)tearDown {
    // Put teardown code here. This method is called after the invocation of each test method in the class.
    [super tearDown];
}

- (void)testEidos {
	// Since we roll our own testing code, for testability by the user, we don't fit very well into
	// the Xcode testing framework.  We make no attempt here to split the individual tests into their
	// own methods, or to call Xcode's assert functions to assert failures, or anything like that.
	// The point of having these tests at all is to be able to assess test code coverage in Xcode.
	RunEidosTests();
}

@end







































