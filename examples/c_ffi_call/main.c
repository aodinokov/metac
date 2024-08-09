#include "metac/reflect.h"
#include <stdio.h>
#include <stdlib.h> /*free*/

#include <ffi.h>

#define METAC_WRAP_FN_PARAMS(_tag_map_, _fn_, _args_...) ({ \
        metac_value_t * p_val = NULL; \
        metac_parameter_storage_t * p_param_storage = metac_new_parameter_storage(); \
        if (p_param_storage != NULL) { \
            p_val = metac_new_value_with_parameters(p_param_storage, _tag_map_, METAC_GSYM_LINK_ENTRY(_fn_), _args_); \
        } \
        /* this is to control number of parameters during compilation */ \
        if (0) { \
            _fn_(_args_); \
        } \
        p_val; \
    })

#define METAC_CLEAN_PARAMS(_p_val_) do { \
        metac_value_t * _p_val = _p_val_; \
        metac_parameter_storage_t * p_param_storage = (metac_parameter_storage_t *)metac_value_addr(_p_val); \
        metac_value_delete(_p_val); \
        metac_parameter_storage_delete(p_param_storage); \
    } while(0)


int _call_val(void (*fn)(void),/* metac_value_t * p_val*/ int a, short b) {
    ffi_cif cif;
    ffi_type *args[] = {
        &ffi_type_sint,
        &ffi_type_sshort
    };
    
    /* Initialize the cif */
    if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 2, 
		       &ffi_type_sint, args) == FFI_OK) {

        ffi_arg rc;

        void *values[] = {
            &a, &b,
        };

        ffi_call(&cif, fn, &rc, values);
        return (int)rc;
    }
    return -1;
}

int test_function1_with_args(int a, short b) {
    printf("test_function1_with_args %d %d\n", a, b);
    return a + b + 6;
}
METAC_GSYM_LINK(test_function1_with_args);


int main() {
    metac_value_t * p_val = METAC_WRAP_FN_PARAMS(NULL, test_function1_with_args, 1, 2);

    if (p_val != NULL) {
        char * s = metac_value_string_ex(p_val, METAC_WMODE_deep, NULL);
        if (s != NULL) {
            printf("captured %s\n", s);
            free(s);
        }

        METAC_CLEAN_PARAMS(p_val);
    }

    printf("called fn res = %d\n", _call_val((void (*)(void))test_function1_with_args, 1, 2));

    return 0;
}