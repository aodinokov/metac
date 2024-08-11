#include "metac/reflect.h"
#include <stdio.h>
#include <stdlib.h> /*free*/

#include "value_ffi.h"

enum x{
    xOne = 1,
    xTwo = 2,
};

int test_function1_with_args(int a, short b, double c, enum x x) {
    return a + b + 6;
}
METAC_GSYM_LINK(test_function1_with_args);

int main() {
    metac_value_t * p_params_val = METAC_WRAP_FN_PARAMS(NULL, test_function1_with_args, -100, 200, 0.1, xTwo);

    if (p_params_val != NULL) {
        char * s = metac_value_string_ex(p_params_val, METAC_WMODE_deep, NULL);
        if (s != NULL) {
            printf("preparing params for %s\n", s);
            free(s);
        }

        metac_value_t *p_res_val = metac_new_value_with_call_result(p_params_val);
        if (metac_value_call(p_params_val, (void (*)(void))test_function1_with_args, p_res_val) == 0) {
            if (p_res_val != NULL) {
                s = metac_value_string_ex(p_res_val, METAC_WMODE_deep, NULL);
                if (s != NULL) {
                    printf("returned %s\n", s);
                    free(s);
                }
            }
        }
        metac_value_with_call_result_delete(p_res_val);

        metac_value_with_call_parameters_delete(p_params_val);
    }
    return 0;
}