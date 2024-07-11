#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "print_args.h"

/*
so, we'll have a 
metac_call(entry, args, calling, void* p_res);
metac_args(entry, ...) > metac_args

so we can
metac_call_value(entry, metac_args(entry, ...), calling, void* p_res);
and it will return metac_value_t, which will be possible to manipulate with:
at least - print... probably also serialize/deserialize (to binary - easy)
not sure if we need deep copy/compare/delete? probably yes.
call is always on the top if exists

drawbacks - va_list isn't supported - need to pass  metac_tag_map_t * p_tag_map ?
or not at this stage??? or it's not possible at all? it will be the rest of va_list
this will be always the last param. there should be always 1 level only???

lets say we created 
ourprintf(char *, ...);
we pack it -> 
we meet unspec_param and we just put out struct va_list_container *p_va_list_container there?
metac_args will store it

??? or we must have 
metac_args(metac_tag_map_t, entry, ...) > metac_args
we'll make metac_args using ... and tag_map...,

in that case it maybe will be even better to put everything including *p_va_list_container
into metac_value and use tag_map when we print or build memory_map, so tagmap would be in 1 place.

metac_value with fn will have additional api to walk args, get result

that leaves us with metac_call_value(entry, args, calling, void* p_res);
which will use metac_value underthehood + some static function to pack everything
and api to access args,result.

we'll need to add support of subprogram args/results parsing/ printing (will start from printing)

*/

#if 0
/** @brief metac internal buffer to store argument */
struct metac_arg {
    metac_size_t sz; //??? do we need this?
    void * p_buf;
};

/** @brief metac internal buffer to store arguments
 * we can't used va_list, because it's getting destroyed as soon as va_arg is called and some fn which uses va_list
 */
struct metac_args {
    metac_num_t args_count;
    struct metac_arg args[];
};

struct metac_call {
    struct metac_args args;
    metac_flag_t calling;   // if not 0 - we have only args
    void * p_res_buf;       // buffer of result
};

static void _args(metac_entry_t *p_entry, struct va_list_container *p_va_list_container) {
    if (p_entry == NULL || metac_entry_has_parameters(p_entry) == 0) {
        return;
    }
    // we need to calculate buffer size first.
    metac_size_t all_param_byte_sz = 0;

    for (int i = 0; i < metac_entry_parameters_count(p_entry); ++i) {
        metac_entry_t * p_param_entry = metac_entry_by_paremeter_id(p_entry, i);
        if (metac_entry_is_parameter(p_param_entry) == 0) {
            // something is wrong
            break;
        }
        if (metac_entry_is_unspecified_parameter(p_param_entry) != 0) {
            // we don't support printing va_args... there is no generic way (callback?)
            break;
        }
        metac_entry_t * p_param_type_entry = metac_entry_parameter_entry(p_param_entry);
        if (p_param_type_entry == NULL) {
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
                    all_param_byte_sz += param_byte_sz; \
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
        } else if (metac_entry_is_pointer(p_param_type_entry) != 0) {
            do {
                if (handled == 0 ) {
                    all_param_byte_sz += sizeof(void*);
                    handled = 1;
                }
            } while(0);
        } else if (metac_entry_is_enumeration(p_param_type_entry) != 0) {
#define _enum_arg_(_type_, _va_type_) \
            do { \
                if (handled == 0 && param_byte_sz == sizeof(_type_)) { \
                    all_param_byte_sz += param_byte_sz; \
                    handled = 1; \
                } \
            } while(0)
            _enum_arg_(char, int);
            _enum_arg_(short, int);
            _enum_arg_(int, int);
            _enum_arg_(long, long);
            _enum_arg_(long long, long long);
#undef _enum_arg_          
        }else if (metac_entry_has_members(p_param_type_entry) != 0) {
            all_param_byte_sz += param_byte_sz;
            handled = 1;
        }
        if (handled == 0) {
            // wont' be able to do anything
            break;
        }
    }

}
#endif

static void _vprint_args(metac_tag_map_t * p_tag_map, metac_flag_t calling, metac_entry_t *p_entry,  metac_value_t * p_res, struct va_list_container *p_va_list_container) {
    if (p_entry == NULL || metac_entry_has_parameters(p_entry) == 0) {
        return;
    }

    if (calling == 1) {
        printf("calling ");
    }

    printf("%s(", metac_entry_name(p_entry));

    char buf[128];

    for (int i = 0; i < metac_entry_parameters_count(p_entry); ++i) {

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

        void * addr = NULL;

        if (metac_entry_is_base_type(p_param_type_entry) != 0) {
            // take what type of base type it is. It can be char, unsigned char.. etc
            metac_name_t param_base_type_name = metac_entry_base_type_name(p_param_type_entry);
#define _base_type_arg_(_type_, _va_type_, _pseudoname_) \
            do { \
                if (addr == NULL && strcmp(param_base_type_name, #_pseudoname_) == 0 && param_byte_sz == sizeof(_type_)) { \
                    _type_ val = va_arg(p_va_list_container->parameters, _va_type_); \
                    memcpy(buf, &val, sizeof(val)); \
                    addr = &buf[0]; \
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
        } else if (metac_entry_is_pointer(p_param_type_entry) != 0) {
            do {
                if (addr == NULL ) {
                    void * val = va_arg(p_va_list_container->parameters, void *);
                    memcpy(buf, &val, sizeof(val));
                    addr = &buf[0];
                }
            } while(0);
        } else if (metac_entry_is_enumeration(p_param_type_entry) != 0) {
#define _enum_arg_(_type_, _va_type_) \
            do { \
                if (addr == NULL && param_byte_sz == sizeof(_type_)) { \
                    _type_ val = va_arg(p_va_list_container->parameters, _va_type_); \
                    memcpy(buf, &val, sizeof(val)); \
                    addr = &buf[0]; \
                } \
            } while(0)
            _enum_arg_(char, int);
            _enum_arg_(short, int);
            _enum_arg_(int, int);
            _enum_arg_(long, long);
            _enum_arg_(long long, long long);
#undef _enum_arg_          
        }else if (metac_entry_has_members(p_param_type_entry) != 0) {
            do {
                if (addr == NULL) {
                    /*  NOTE: we can't call calloc AFTER metac_entry_struct_va_arg, we can only copy data.
                        calloc, printf and other functions damage the data 
                    */
                    if (param_byte_sz > sizeof(buf)) {
                        addr = calloc(1, param_byte_sz);
                        if (addr == NULL) {
                            break;
                        }
                        void * val = metac_entry_struct_va_arg(p_param_type_entry, p_va_list_container);
                        if (val == NULL) {
                            free(addr);
                            break;
                        }
                        memcpy(addr, val, param_byte_sz);
                    } else {
                        void * val = metac_entry_struct_va_arg(p_param_type_entry, p_va_list_container);
                        if (val == NULL) {
                            break;
                        }
                        memcpy(buf, val, param_byte_sz);
                        addr = &buf[0];
                    }
                }
            } while(0);
        }
        if (addr == NULL) {
            break;
        }

        metac_value_t * p_val = metac_new_value(p_param_type_entry, addr);
        if (p_val == NULL) {
            break;
        }
        char * v = metac_value_string_ex(p_val, METAC_WMODE_deep, p_tag_map);
        // delete val immideatly
        metac_value_delete(p_val);
        if (addr != &buf[0]) {
            // if buf was too small we allocated memory
            free(addr);
        }
        if (v == NULL) {
            break;
        }

        char * arg_decl = metac_entry_cdecl(p_param_type_entry);
        if (arg_decl == NULL) {
            free(v);
            break;
        }

        printf(arg_decl, param_name);
        printf(" = %s", v);

        free(arg_decl);
        free(v);

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

void vprint_args(metac_tag_map_t * p_tag_map, metac_flag_t calling, metac_entry_t *p_entry,  metac_value_t * p_res, va_list args) {
    struct va_list_container cntr = {};
    va_copy(cntr.parameters, args);
    _vprint_args(p_tag_map, calling, p_entry, p_res, &cntr);
    va_end(cntr.parameters);
}

void print_args(metac_tag_map_t * p_tag_map, metac_flag_t calling, metac_entry_t *p_entry, metac_value_t * p_res, ...) {
    struct va_list_container cntr = {};
    va_start(cntr.parameters, p_res);
    _vprint_args(p_tag_map, calling, p_entry, p_res, &cntr);
    va_end(cntr.parameters);
    return;
}
