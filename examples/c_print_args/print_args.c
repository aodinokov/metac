#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "print_args.h"

void vprint_args(metac_tag_map_t * p_tag_map, metac_flag_t calling, metac_entry_t *p_entry,  metac_value_t * p_res, va_list args) {
    if (p_entry == NULL || metac_entry_has_paremeters(p_entry) == 0) {
        return;
    }

    if (calling == 1) {
        printf("calling ");
    }

    printf("%s(", metac_entry_name(p_entry));

    char buf[16];

    for (int i = 0; i < metac_entry_paremeters_count(p_entry); ++i) {
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


        int handled = 0;

        if (metac_entry_is_base_type(p_param_type_entry) != 0) {
            // take what type of base type it is. It can be char, unsigned char.. etc
            metac_name_t param_base_type_name = metac_entry_base_type_name(p_param_type_entry);
#define _base_type_arg_(_type_, _pseudoname_) \
            do { \
                if (handled == 0 && strcmp(param_base_type_name, #_pseudoname_) == 0 && param_byte_sz == sizeof(_type_)) { \
                    _type_ val = va_arg(args, _type_); \
                    memcpy(buf, &val, sizeof(val)); \
                    handled = 1; \
                } \
            } while(0)
            // handle all known base types
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
            //_base_type_arg_(float, float);
            do {
                // va_arg always put double if it's float, e.g.
                // https://github.com/lattera/glibc/blob/master/stdio-common/vfprintf.c#L771
                if (handled == 0 && strcmp(param_base_type_name, "float") == 0) {
                    double val = va_arg(args, double);
                    float fval = val;
                    memcpy(buf, &fval, sizeof(fval));
                    handled = 1;
                }
            } while(0);
            _base_type_arg_(double, double);
            _base_type_arg_(long double, long double);
            _base_type_arg_(float complex, complex);
            _base_type_arg_(double complex, complex);
            _base_type_arg_(long double complex, complex);
#undef _base_type_arg_
        } else {
#define _handle_sz_(_sz_) \
            do { \
                if (handled == 0 && param_byte_sz == _sz_) { \
                    char *x = va_arg(args, char[_sz_]); \
                    if (x == NULL) { break; } \
                    memcpy(buf, x, param_byte_sz); \
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
    #undef _handle_sz_
        }
        if (handled == 0) {
            break;
        }

        metac_value_t * p_val = metac_new_value(p_param_type_entry, &buf[0]);
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

    }
    printf(")");

    if (calling == 0) {
        printf(" returned");
        if (p_res != NULL) {
            char * v = metac_value_string_ex(p_res, METAC_WMODE_deep, p_tag_map);
            if (v == NULL) {
                return;
            }
            printf(" %s", v);

            free(v);

            metac_value_delete(p_res);
        }
    }

    printf("\n");
}

void print_args(metac_tag_map_t * p_tag_map, metac_flag_t calling, metac_entry_t *p_entry, metac_value_t * p_res, ...) {
    va_list args;
    va_start(args, p_res);
    vprint_args(p_tag_map, calling, p_entry, p_res, args);
    va_end(args);
    return;
}
