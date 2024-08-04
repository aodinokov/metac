#ifndef INCLUDE_METAC_PRINTF_FORMAT_H_
#define INCLUDE_METAC_PRINTF_FORMAT_H_

#include "metac/reflect.h"

// naive implementation
// we can lookup in https://github.com/bminor/glibc/blob/master/stdio-common/vfprintf-internal.c#L288

typedef struct metac_printf_specifier_flag {
    union {
        struct {
            unsigned int plus:1;
            unsigned int minus:1;
            unsigned int pound:1;
            unsigned int oct:1;
            unsigned int space:1;
        };
        unsigned int all;
    };
} metac_printf_specifier_flag_t;

/* modifiers */
typedef enum metac_printf_specifier_arg_len {
    psal_no = 0,
    psal_h,  // short
    psal_hh, // char
    psal_l,  // long
    psal_ll, // long long
    psal_L,  // long double
}metac_printf_specifier_arg_len_t;

typedef struct metac_printf_specifier {
    metac_printf_specifier_flag_t flags;
    metac_num_t format_len;
    metac_num_t precision;
    metac_num_t arg_len;
    char t;
} metac_printf_specifier_t;

size_t metac_parse_format_specifier(const char *format, size_t *pos, metac_printf_specifier_t *p_specifier);
size_t metac_count_format_specifiers(const char *format);
int metac_store_printf_params(metac_parameter_storage_t * p_pload, const char * format, ...);
int metac_store_vprintf_params(metac_parameter_storage_t * p_pload, const char * format, va_list parameters);
metac_entry_t * metac_store_printf_params_entry(); // returns p_entry for ...
metac_entry_t * metac_store_vprintf_params_entry(); // returns p_entry for va_list parameters

#endif
