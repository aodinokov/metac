#include "metac/reflect.h"
#include <stdio.h>
#include <stdlib.h> /*free*/

#include "value_ffi.h"

enum x{
    xOne = 1,
    xTwo = 2,
};

int test_function1_with_args(int a, short b, double c, enum x x) {
    printf("test_function1_with_args %d %d %f %d\n", a, b, c, x);
    return a + b + 6;
}
METAC_GSYM_LINK(test_function1_with_args);

//int _call_val(void (*fn)(void),/* metac_value_t * p_val*/ int a, short b);

int main() {
    metac_value_t * p_val = METAC_WRAP_FN_PARAMS(NULL, test_function1_with_args, -100, 200, 0.1, xTwo);

    if (p_val != NULL) {
        char * s = metac_value_string_ex(p_val, METAC_WMODE_deep, NULL);
        if (s != NULL) {
            printf("captured %s\n", s);
            free(s);
        }

        metac_value_call(p_val, (void (*)(void))test_function1_with_args);

        METAC_CLEAN_PARAMS(p_val);
    }

    //printf("called fn res = %d\n", _call_val((void (*)(void))test_function1_with_args, 1, 2));

    return 0;
}