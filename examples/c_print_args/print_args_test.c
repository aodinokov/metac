#include "metac/test.h"

#include "print_args.c"

int test_function2_with_args(int *a, short b){
    return *a + b + 999;
}
METAC_GSYM_LINK(test_function2_with_args);

METAC_START_TEST(sanity){
    int x = 689;
    METAC_WRAP_FN_RES(int, test_function2_with_args, &x, 2);
}END_TEST