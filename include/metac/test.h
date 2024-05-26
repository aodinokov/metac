#ifndef INCLUDE_METAC_TEST_H_
#define INCLUDE_METAC_TEST_H_

#include <check.h>  /* complete implementation of test framework */

#include "metac/reqresp.h"

/** move address of the test function so it won't optimized even if the function isn't used (it's static, so it could be possible) */
#define METAC_START_TEST(__testname) \
    static const TTest __testname ## _ttest; \
    const TTest * METAC_REQUEST(test, __testname) = &__testname ## _ttest; \
    START_TEST(__testname)

#endif