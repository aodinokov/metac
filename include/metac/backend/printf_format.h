#ifndef INCLUDE_METAC_PRINTF_FORMAT_H_
#define INCLUDE_METAC_PRINTF_FORMAT_H_

#include "metac/reflect.h"

// naive implementation
// we can lookup in https://github.com/bminor/glibc/blob/master/stdio-common/vfprintf-internal.c#L288

/** @brief this is a set of possible flags for the entry started by % in printf format string */
typedef struct metac_printf_specifier_flag {
    union {
        struct {
            unsigned int plus:1;    /**< %+... */
            unsigned int minus:1;   /**< %-... */
            unsigned int pound:1;   /**< %#... */
            unsigned int oct:1;     /**< %0... */
            unsigned int space:1;   /**< % ... */
        };
        unsigned int all;           /**< representation as 1 number */
    };
} metac_printf_specifier_flag_t;

/** @brief modifiers which affects the parameter length for the entry started by % in printf format string */
typedef enum metac_printf_specifier_arg_len {
    psal_no = 0, /**< empty (default len) */
    psal_h,  /**< short */
    psal_hh, /**< char */
    psal_l,  /**< long */
    psal_ll, /**< long long */
    psal_L,  /**< long double */
}metac_printf_specifier_arg_len_t;

/** @brief all parameters set by one printf format entry started by % */
typedef struct metac_printf_specifier {
    metac_printf_specifier_flag_t flags; /**< flags */
    metac_num_t format_len;
    metac_num_t precision;
    metac_printf_specifier_arg_len_t arg_len;    /**< defined by metac_printf_specifier_arg_len_t length */
    char t;
} metac_printf_specifier_t;

/** @brief parse printf format string starting *pos position and put all info to p_specifier */
size_t metac_parse_format_specifier(const char *format, size_t *pos, metac_printf_specifier_t *p_specifier);
/** @brief returns number os specifiers in printf format string */
size_t metac_count_format_specifiers(const char *format);

/** @brief parses variadic parameters ... based on printf format string and stores all info into p_pload */
int metac_store_printf_params(metac_parameter_storage_t * p_pload, const char * format, ...);
/** @brief parses va_list parameters based on printf format string and stores all info into p_pload */
int metac_store_vprintf_params(metac_parameter_storage_t * p_pload, const char * format, va_list parameters);
/** @brief helper function that returns metac_entry for ... parameter) */
metac_entry_t * metac_store_printf_params_entry();
/** @brief helper function that returns metac_entry for va_list parameters parameter) */
metac_entry_t * metac_store_vprintf_params_entry();

#endif
