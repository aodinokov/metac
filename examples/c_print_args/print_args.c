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

    char buf[32];

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
#define _base_type_arg_(_type_, _va_type_, _pseudoname_) \
            do { \
                if (handled == 0 && strcmp(param_base_type_name, #_pseudoname_) == 0 && param_byte_sz == sizeof(_type_)) { \
                    _type_ val = va_arg(args, _va_type_); \
                    memcpy(buf, &val, sizeof(val)); \
                    handled = 1; \
                } \
            } while(0)
            // handle all known base types
            _base_type_arg_(char, int, char);
            _base_type_arg_(unsigned char, int, unsigned char);
            _base_type_arg_(short, int, short int);
            _base_type_arg_(unsigned short, int, unsigned short int);
            _base_type_arg_(int, int, int);
            _base_type_arg_(unsigned int, unsigned int, unsigned int);
            _base_type_arg_(long, long, long int);
            _base_type_arg_(unsigned long, unsigned long, unsigned long int);
            _base_type_arg_(long long, long long, long long int);
            _base_type_arg_(unsigned long long, unsigned long long, unsigned long long int);
            _base_type_arg_(bool, int, _Bool);
            _base_type_arg_(float, double, float);
            _base_type_arg_(double, double, double);
            _base_type_arg_(long double, long double, long double);
            _base_type_arg_(float complex, float complex, complex float);
            _base_type_arg_(double complex, double complex, complex double);
            _base_type_arg_(long double complex, long double complex, long complex double);
#undef _base_type_arg_
        } else if (metac_entry_is_pointer(p_param_type_entry) != 0){
            do {
                if (handled == 0 ) {
                    void * val = va_arg(args, void *);
                    memcpy(buf, &val, sizeof(val));
                    handled = 1;
                }
            } while(0);
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
