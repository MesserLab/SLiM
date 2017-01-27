//
//  SLiMTests.m
//  SLiM
//
//  Created by Ben Haller on 1/27/17.
//  Copyright © 2017 Messer Lab, http://messerlab.org/software/. All rights reserved.
//

#import <XCTest/XCTest.h>

#import "slim_test.h"


@interface SLiMTests : XCTestCase

@end

@implementation SLiMTests

- (void)setUp {
    [super setUp];
    // Put setup code here. This method is called before the invocation of each test method in the class.
}

- (void)tearDown {
    // Put teardown code here. This method is called after the invocation of each test method in the class.
    [super tearDown];
}

- (void)testSLiM {
	// Since we roll our own testing code, for testability by the user, we don't fit very well into
	// the Xcode testing framework.  We make no attempt here to split the individual tests into their
	// own methods, or to call Xcode's assert functions to assert failures, or anything like that.
	// The point of having these tests at all is to be able to assess test code coverage in Xcode.
	RunSLiMTests();
}

@end
