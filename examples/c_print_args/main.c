#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "metac/reflect.h"

void print_args(metac_entry_t *p_entry, ...) {
    if (p_entry == NULL || metac_entry_has_paremeter(p_entry) == 0) {
        return;
    }
    
    va_list args;
    va_start(args, p_entry);

    printf("%s(", metac_entry_name(p_entry));

    for (int i = 0; i < metac_entry_paremeter_count(p_entry); ++i) {
        if (i > 0) {
            printf(", ");
        }

        metac_entry_t * p_param_entry = metac_entry_by_paremeter_id(p_entry, i);
        if (metac_entry_is_parameter(p_param_entry) == 0) {
            // something is wrong
            break;
        }
        if (metac_entry_is_unspecified_parameter(p_param_entry) != 0) {
            // we don't support printing va_args... there is no generic way
            printf("...");
            break;
        }

        metac_name_t param_name = metac_entry_name(p_param_entry);
        metac_entry_t * p_param_type_entry = metac_entry_parameter_entry(p_param_entry);
        if (param_name == NULL || param_name == NULL) {
            // something is wrong
            break;
        }

        if (metac_entry_is_base_type(p_param_type_entry) != 0) {
            metac_name_t param_base_type_name = metac_entry_base_type_name(p_param_type_entry);
#define _base_type_arg_(_type_, _pseudoname_) \
    do { \
        if (strcmp(param_base_type_name, #_pseudoname_) == 0) { \
            _type_ val = va_arg(args, _type_); \
            metac_value_t * p_val = metac_new_value(p_param_type_entry, &val); \
            if (p_val == NULL) { \
                break; \
            } \
            char * s = metac_value_string(p_val); \
            if (s == NULL) { \
                metac_value_delete(p_val); \
                break; \
            } \
            printf("%s: %s", param_name, s); \
            free(s); \
            metac_value_delete(p_val); \
        } \
    } while(0)
    _base_type_arg_(char, char);
    _base_type_arg_(unsigned char, unsigned char);
    _base_type_arg_(short, short int);
    _base_type_arg_(unsigned short, unsigned short int);
    _base_type_arg_(int, int);
    _base_type_arg_(unsigned int, unsigned int);
    _base_type_arg_(long, long int);
    _base_type_arg_(unsigned long, unsigned long int);
    _base_type_arg_(long long, long long int);
    _base_type_arg_(unsigned long long, unsigned long long int);
    _base_type_arg_(bool, _Bool);
    _base_type_arg_(float, float);
    _base_type_arg_(double, double);
    _base_type_arg_(long double, long double);
    _base_type_arg_(float complex, complex);
    _base_type_arg_(double complex, complex);
    _base_type_arg_(long double complex, complex);
#undef _base_type_arg_
        }
    }
    printf(")\n");
    va_end(args);
    return;
}

#define METAC_WRAP_FN(_fn_, _args_...) ({ \
        print_args(METAC_GSYM_LINK_ENTRY(_fn_), _args_); \
        _fn_(_args_); \
    })

int test_function1_with_args(int a, short b){
    return a + b + 6;
}
METAC_GSYM_LINK(test_function1_with_args);

int main() {
    // test_function1_with_args(1, 2);
    printf("fn returned: %i\n", METAC_WRAP_FN(test_function1_with_args, 1, 2));

    return 0;
}