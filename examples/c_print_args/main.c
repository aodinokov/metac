#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "metac/reflect.h"

static metac_flag_t handle_sz(va_list *p_args, metac_size_t sz, char * p_buf /*16*/) {
    if (p_buf == NULL) {
        return 0;
    }
#define _handle_sz_(_sz_) \
        do { \
            if (sz == _sz_) { \
                char *x = va_arg(*p_args, char[_sz_]); \
                memcpy(p_buf, x, sz); \
                return 1;\
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
    return 0;
}

void vprint_args(metac_tag_map_t * p_tag_map, metac_flag_t want_res, metac_entry_t *p_entry, va_list args) {
    if (p_entry == NULL || metac_entry_has_paremeter(p_entry) == 0) {
        return;
    }

    if (want_res == 0) {
        printf("calling ");
    }

    printf("%s(", metac_entry_name(p_entry));

    char buf[16];

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

        int handled = handle_sz(&args, param_byte_sz, &buf[0]);
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

    do {
        if (want_res != 0) {
            printf(" returned");
            if (metac_entry_has_result(p_entry)!=0) {
                metac_entry_t * p_result_type_entry = metac_entry_result_type(p_entry);
                if (p_result_type_entry == NULL) {
                    break;
                }

                metac_size_t result_byte_sz = 0;
                if (metac_entry_byte_size(p_result_type_entry, &result_byte_sz) != 0) {
                    break;
                }

                int handled = handle_sz(&args, result_byte_sz, &buf[0]);
                if (handled == 0) {
                    break;
                }

                metac_value_t * p_val = metac_new_value(p_result_type_entry, &buf[0]);
                if (p_val == NULL) {
                    break;
                }
                char * v = metac_value_string_ex(p_val, METAC_WMODE_deep, p_tag_map);
                if (v == NULL) {
                    metac_value_delete(p_val);
                    break;
                }
                printf(" %s", v);

                free(v);

                metac_value_delete(p_val);
            }
        }
    }while(0);

    printf("\n");
}

void print_args(metac_tag_map_t * p_tag_map, metac_flag_t has_res, metac_entry_t *p_entry, ...) {
    va_list args;
    va_start(args, p_entry);
    vprint_args(p_tag_map, has_res, p_entry, args);
    va_end(args);
    return;
}

#define METAC_WRAP_FN(_fn_, _args_...) ({ \
        print_args(NULL, 0, METAC_GSYM_LINK_ENTRY(_fn_), _args_); \
        _fn_(_args_); \
    })

#define METAC_WRAP_FN_RES(_type_, _fn_, _args_...) ({ \
        print_args(NULL, 0, METAC_GSYM_LINK_ENTRY(_fn_), _args_); \
        _type_ res = _fn_(_args_); \
        print_args(NULL, 1, METAC_GSYM_LINK_ENTRY(_fn_), _args_, res); \
        res; \
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
        p_list->x += 1;
        
        p_list = p_list->p_next;
    }
    return sum;
 }
 METAC_GSYM_LINK(test_function3_with_args);

double test_function4_with_args(list_t *p_list) {
    return METAC_WRAP_FN_RES(double, test_function3_with_args, p_list) - 1000;
}
METAC_GSYM_LINK(test_function4_with_args);


int main() {
    printf("fn returned: %i\n", METAC_WRAP_FN(test_function1_with_args, 10, 22));

    int x = 689; /* could use (int[]){{689, }}, but used this to simplify reading */
    printf("fn returned: %i\n", METAC_WRAP_FN(test_function2_with_args, &x, 22));

    list_t * p_list = (list_t[]){{.x = 42.42, .p_next = (list_t[]){{ .x = 45.4, .p_next = NULL}}}};
    printf("fn returned: %f\n", METAC_WRAP_FN(test_function3_with_args, p_list));

    METAC_WRAP_FN_RES(int, test_function2_with_args, &x, 22);

    printf("fn returned: %f\n", METAC_WRAP_FN_RES(double, test_function4_with_args, p_list));
    
    return 0;
}