#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "metac/reflect.h"

void print_args(metac_tag_map_t * p_tag_map, metac_entry_t *p_entry, ...) {
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
        if (param_name == NULL || p_param_type_entry == NULL) {
            // something is wrong
            break;
        }

        metac_size_t param_byte_sz = 0;
        if (metac_entry_byte_size(p_param_type_entry, &param_byte_sz) != 0) {
            // something is wrong
            break;
        }

        char buf[32];

        int handled = 0;
#define _handle_sz_(_sz_) \
        do { \
            if (param_byte_sz == _sz_) { \
                char *x = &buf[0]; \
                x = va_arg(args, char[_sz_]); \
                memcpy(buf, x, _sz_); \
                handled = 1; \
            } \
        } while(0)
        _handle_sz_(1);
        _handle_sz_(2);
        _handle_sz_(3);
        _handle_sz_(4);
        _handle_sz_(5);
        _handle_sz_(6);
        _handle_sz_(7);
        _handle_sz_(8);
        _handle_sz_(9);
        _handle_sz_(10);
        _handle_sz_(11);
        _handle_sz_(12);
        _handle_sz_(13);
        _handle_sz_(14);
        _handle_sz_(15);
        _handle_sz_(16);
        _handle_sz_(17);
        _handle_sz_(18);
        _handle_sz_(19);
        _handle_sz_(20);
        _handle_sz_(21);
        _handle_sz_(22);
        _handle_sz_(23);
        _handle_sz_(24);
        _handle_sz_(25);
        _handle_sz_(26);
        _handle_sz_(27);
        _handle_sz_(28);
        _handle_sz_(29);
        _handle_sz_(30);
        _handle_sz_(31);
        _handle_sz_(32);
#undef _handle_sz_

        if (handled == 0) {
            break;
        }

        metac_value_t * p_val = metac_new_value(p_param_type_entry, &buf);
        if (p_val == NULL) {
            break;
        }
        char * v = metac_value_string_ex(p_val, METAC_WMODE_deep, p_tag_map);
        if (v == NULL) {
            metac_value_delete(p_val);
            break;
        }
        char * arg_decl = metac_entry_cdecl(p_param_type_entry);
        if (arg_decl == NULL) {
            free(v);
            metac_value_delete(p_val);
            break;
        }

        printf(arg_decl, param_name);
        printf(" = %s", v);

        free(arg_decl);
        free(v);
        metac_value_delete(p_val);

#if 0
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
#endif
    }
    printf(")\n");
    va_end(args);
    return;
}

#define METAC_WRAP_FN(_fn_, _args_...) ({ \
        print_args(NULL, METAC_GSYM_LINK_ENTRY(_fn_), _args_); \
        _fn_(_args_); \
    })



int test_function1_with_args(int a, short b){
    return a + b + 6;
}
METAC_GSYM_LINK(test_function1_with_args);

int test_function2_with_args(int *a, short b){
    return *a + b + 999;
}
METAC_GSYM_LINK(test_function2_with_args);

typedef struct list_s{
    double x;
    struct list_s * p_next;
}list_t;

double test_function3_with_args(list_t *p_list) {
    double sum = 0.0;
    while(p_list != NULL) {
        sum += p_list->x;
        p_list = p_list->p_next;
    }
    return sum;
 }
 METAC_GSYM_LINK(test_function3_with_args);

int main() {
    printf("fn returned: %i\n", METAC_WRAP_FN(test_function1_with_args, 10, 22));

    int x = 689; /* could use (int[]){{689, }}, but used this to simplify reading */
    printf("fn returned: %i\n", METAC_WRAP_FN(test_function2_with_args, &x, 22));

    list_t * p_list = (list_t[]){{.x = 42.42, .p_next = (list_t[]){{ .x = 45.4, .p_next = NULL}}}};
    printf("fn returned: %f\n", METAC_WRAP_FN(test_function3_with_args, p_list));

    return 0;
}