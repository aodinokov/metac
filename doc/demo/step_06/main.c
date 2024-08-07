#include "metac/reflect.h"
#include <stdlib.h> /*free*/

#define METAC_WRAP_FN_RES(_tag_map_, _fn_, _args_...) ({ \
        metac_parameter_storage_t * p_param_storage = metac_new_parameter_storage(); \
        if (p_param_storage != NULL) { \
            p_val = metac_new_value_with_parameters(p_param_storage, _tag_map_, METAC_GSYM_LINK_ENTRY(_fn_), _args_); \
        } \
        _fn_(_args_);\
})

int test_function1_with_args(int a, short b){
    return a + b + 6;
}
METAC_GSYM_LINK(test_function1_with_args);

int main() {
    metac_value_t * p_val = NULL;
    printf("fn returned: %i\n", METAC_WRAP_FN_RES(NULL, test_function1_with_args, 10, 22));
    if (p_val != NULL) {
        char * s = metac_value_string_ex(p_val, METAC_WMODE_deep, NULL);
        if (s != NULL) {
            printf("captured %s\n", s);
            free(s);
        }
        metac_parameter_storage_t * p_param_storage = (metac_parameter_storage_t *)metac_value_addr(p_val);
        metac_parameter_storage_delete(p_param_storage);
        metac_value_delete(p_val);
    }
    return 0;
}