#include "gpu.h"
#import <XCTest/XCTest.h>

@interface Tests : XCTestCase

@end

@implementation Tests

- (void)setUp {
}

- (void)tearDown {
}

- (void)testAll {
    assert(run_tests());
}


@end
