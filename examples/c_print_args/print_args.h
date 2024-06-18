#ifndef PRINT_ARGS_H_
#define PRINT_ARGS_H_

#include "metac/reflect.h"

void vprint_args(metac_tag_map_t * p_tag_map, metac_flag_t calling, metac_entry_t *p_entry,  metac_value_t * p_res, va_list args);
void print_args(metac_tag_map_t * p_tag_map, metac_flag_t calling, metac_entry_t *p_entry, metac_value_t * p_res, ...);

#define METAC_WRAP_FN_NORES(_fn_, _args_...) { \
        print_args(NULL, 1, METAC_GSYM_LINK_ENTRY(_fn_), NULL, _args_); \
        _fn_(_args_); \
        print_args(NULL, 0, METAC_GSYM_LINK_ENTRY(_fn_), NULL, _args_); \
    }
#define METAC_WRAP_FN_RES(_type_, _fn_, _args_...) ({ \
        print_args(NULL, 1, METAC_GSYM_LINK_ENTRY(_fn_), NULL, _args_); \
        WITH_METAC_DECLLOC(loc, _type_ res = _fn_(_args_)); \
        print_args(NULL, 0, METAC_GSYM_LINK_ENTRY(_fn_), METAC_VALUE_FROM_DECLLOC(loc, res), _args_); \
        res; \
    })


#endif