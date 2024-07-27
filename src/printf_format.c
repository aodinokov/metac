// this is a very naive version. it's needed to demonstrate the idea of how to print va_args
// see value_with_args.c and its test for more details
#include "metac/reflect.h"

#include "metac/backend/printf_format.h"
#include "metac/backend/value.h"

#include <assert.h>
#include <stdio.h>
#include <string.h> /*strlen*/
#include <ctype.h>

// parse a single format specifier
size_t metac_parse_format_specifier(const char *format, size_t *pos, metac_printf_specifier_t *p_specifier) {
    // Skip leading percent sign
    if (format[*pos] == '%') {
        (*pos)++;
    } else {
        // Not a format specifier, return 0
        return 0;
    }

    // Check for optional flags (+, -, #, 0, space)
    p_specifier->flags.all = 0;
    while (*pos < strlen(format)) {
        if (format[*pos] == '+') {
            p_specifier->flags.plus = 1;
        } else if (format[*pos] == '-') {
            p_specifier->flags.minus = 1;
        } else if (format[*pos] == '#') {
            p_specifier->flags.pound = 1;
        } else if (format[*pos] == '0') {
            p_specifier->flags.oct = 1;
        } else if (format[*pos] == ' ') {
            p_specifier->flags.oct = 1;
        } else {
            break;
        }
        (*pos)++;
    }

    // Check for optional minimum field width (digits)
    p_specifier->format_len = 0;
    while (*pos < strlen(format) && isdigit(format[*pos])) {
        metac_num_t d = format[*pos] - '0';
        if (p_specifier->flags.oct != 0) {
            p_specifier->format_len = p_specifier->format_len * 8 + d;
        } else {
            p_specifier->format_len = p_specifier->format_len * 10 + d;
        }
        (*pos)++;
    }

    // Check for optional precision (. and digits)
    p_specifier->precision = -1;
    if (*pos < strlen(format) && format[*pos] == '.') {
        p_specifier->precision = 0;
        (*pos)++;
        while (*pos < strlen(format) && isdigit(format[*pos])) {
            metac_num_t d = format[*pos] - '0';
            if (p_specifier->flags.oct != 0) {
                p_specifier->precision = p_specifier->precision * 8 + d;
            } else {
                p_specifier->precision = p_specifier->precision * 10 + d;
            }
            (*pos)++;
        }
    }

    // Check for optional length modifier (h, l, ll, L)
    p_specifier->arg_len = psal_no;
    if (*pos < strlen(format) && (format[*pos] == 'h' || format[*pos] == 'l' || format[*pos] == 'L')) {
        p_specifier->arg_len = psal_no;
        if (format[*pos] == 'h') {
            p_specifier->arg_len = psal_h;
        } else if (format[*pos] == 'l') {
            p_specifier->arg_len = psal_l;
        } else if (format[*pos] == 'L') {
            p_specifier->arg_len = psal_L;
        }
        (*pos)++;
        if (*pos < strlen(format) && p_specifier->arg_len == psal_l && (format[*pos] == 'l')) {
            p_specifier->arg_len = psal_ll;
            (*pos)++;
        } else if (*pos < strlen(format) && p_specifier->arg_len == psal_h && (format[*pos] == 'h')) {
            p_specifier->arg_len = psal_hh;
            (*pos)++;
        }
    }

    // Check for conversion specifier
    if (isalpha(format[*pos])) {
        p_specifier->t = format[*pos];
        (*pos)++;
    } else {
        // Invalid format specifier
        return 0;
    }
    
    return 1;
}

// Function to count the number of format specifiers
size_t metac_count_format_specifiers(const char *format) {
    size_t num_specifiers = 0;
    size_t pos = 0;
    metac_printf_specifier_t dummy_specifier;

    while (pos < strlen(format)) {
        if (format[pos] == '%') {
            // Check if a valid format specifier follows
            if (metac_parse_format_specifier(format, &pos, &dummy_specifier) > 0) {
            num_specifiers++;
            }
        }
        pos++;
    }
    return num_specifiers;
}

static metac_value_t * _new_value_from_format_specifier(const char * format, metac_entry_t * p_va_list_entry, struct va_list_container * p_cntr);

metac_value_t * metac_new_value_vprintf_ex(const char * format, metac_entry_t * p_va_list_entry, va_list parameters) {
    struct va_list_container cntr = {};

    va_copy(cntr.parameters, parameters);
    metac_value_t * p_value = _new_value_from_format_specifier(format, p_va_list_entry, &cntr);
    va_end(cntr.parameters);

    return p_value;
}

METAC_GSYM_LINK_DECLARE(metac_new_value_vprintf);
metac_value_t * metac_new_value_vprintf(const char * format, va_list parameters) {
    metac_entry_t *p_fn_entry = METAC_GSYM_LINK_ENTRY(metac_new_value_vprintf);
    assert(
        p_fn_entry != NULL &&
        metac_entry_has_parameters(p_fn_entry) != 0 &&
        metac_entry_parameters_count(p_fn_entry) == 2);
    metac_entry_t * p_va_list_entry = metac_entry_by_paremeter_id(p_fn_entry, 1);

    return metac_new_value_vprintf_ex(format, p_va_list_entry, parameters);
}
METAC_GSYM_LINK(metac_new_value_vprintf);

METAC_GSYM_LINK_DECLARE(metac_new_value_printf);
metac_value_t * metac_new_value_printf(const char * format, ...) {
    metac_entry_t *p_fn_entry = METAC_GSYM_LINK_ENTRY(metac_new_value_printf);
    assert(
        p_fn_entry != NULL &&
        metac_entry_has_parameters(p_fn_entry) != 0 &&
        metac_entry_parameters_count(p_fn_entry) == 2);
    metac_entry_t * p_va_list_entry = metac_entry_by_paremeter_id(p_fn_entry, 1);

    struct va_list_container cntr = {};

    va_start(cntr.parameters, format);
    metac_value_t * p_value = _new_value_from_format_specifier(format, p_va_list_entry, &cntr);
    va_end(cntr.parameters);

    return p_value;
}
METAC_GSYM_LINK(metac_new_value_printf);

static metac_value_t * _new_value_from_format_specifier(const char * format, metac_entry_t * p_va_list_entry, struct va_list_container * p_cntr) {
    if (p_va_list_entry == NULL ||
        metac_entry_has_load_of_parameter(p_va_list_entry) == 0) {
        return NULL;
    }

    metac_num_t parameters_count = metac_count_format_specifiers(format);

    metac_value_load_of_parameter_t * p_pload = metac_new_load_of_parameter(parameters_count);
    if (p_pload == NULL) {
        return NULL;
    }

    metac_num_t param_id = 0;
    size_t pos = 0;
    metac_printf_specifier_t dummy_specifier;

    while (pos < strlen(format)) {
        if (format[pos] == '%') {
            // Check if a valid format specifier follows
            if (metac_parse_format_specifier(format, &pos, &dummy_specifier) > 0) {
                switch (dummy_specifier.t) {
                case 's': {
                        WITH_METAC_DECLLOC(decl, char * p = NULL);
                        p = (char*)va_arg(p_cntr->parameters, char*);

                        metac_entry_t * p_param_entry = metac_entry_final_entry(METAC_ENTRY_FROM_DECLLOC(decl, p), NULL);

                        if (p == NULL) { // use std ptr approach
                            metac_value_t * p_param_value = metac_load_of_parameter_new_value(p_pload, param_id, p_param_entry, sizeof(p));
                            if (p_param_value == NULL) {
                                metac_load_of_parameter_delete(p_pload);
                                return NULL;
                            }
                            void * p_param_value_ptr = metac_value_addr(p_param_value);
                            memcpy(p_param_value_ptr, &p, sizeof(p));
                        }else{
                            size_t len = strlen(p);
                            metac_entry_t * p_param_with_len_entry = metac_new_element_count_entry(p_param_entry, len+1);
                            if (p_param_with_len_entry == NULL) {
                                return NULL;
                            }

                            metac_value_t * p_param_value = metac_load_of_parameter_new_value(p_pload, param_id, p_param_with_len_entry, len+1);
                            metac_entry_delete(p_param_with_len_entry);
                            if (p_param_value == NULL) {
                                metac_load_of_parameter_delete(p_pload);
                                return NULL;
                            }
                            void * p_param_value_ptr = metac_value_addr(p_param_value);
                            memcpy(p_param_value_ptr, p, len+1);
                        }
                    }
                    break;
                /* pointers */
#define _process(_type_, _va_arg_type) { \
                        WITH_METAC_DECLLOC(decl, _type_ dummy = NULL); \
                        metac_entry_t * p_param_entry = metac_entry_final_entry(METAC_ENTRY_FROM_DECLLOC(decl, dummy), NULL); \
                        metac_value_t * p_param_value = metac_load_of_parameter_new_value(p_pload, param_id, p_param_entry, sizeof(dummy)); \
                        if (p_param_value == NULL) { \
                            metac_load_of_parameter_delete(p_pload); \
                            return NULL; \
                        } \
                        void * p_param_value_ptr = metac_value_addr(p_param_value); \
                        void * p = (_type_)va_arg(p_cntr->parameters, void*); \
                        memcpy(p_param_value_ptr, &p, sizeof(p)); \
                    }
                case 'p'/* simple pointers */:
                    _process(void*, void*);
                    break;
                case 'n'/* n pointers */:
                    if (dummy_specifier.arg_len == psal_hh) {
                        _process(char*, void*);
                    } else if (dummy_specifier.arg_len == psal_h) {
                        _process(short*, void*);
                    } else if (dummy_specifier.arg_len == psal_no) {
                        _process(int*, void*);
                    } else if (dummy_specifier.arg_len == psal_l) {
                        _process(long*, void*);
                    } else if (dummy_specifier.arg_len == psal_ll) {
                        _process(long long*, void*);
                    } else {
                        return NULL;
                    }
                    break;
#undef _process
                /* basic types */
#define _process(_type_, _va_arg_type) { \
                        WITH_METAC_DECLLOC(decl, _type_ dummy = 0); \
                        metac_entry_t * p_param_entry = metac_entry_final_entry(METAC_ENTRY_FROM_DECLLOC(decl, dummy), NULL); \
                        metac_value_t * p_param_value = metac_load_of_parameter_new_value(p_pload, param_id, p_param_entry, sizeof(dummy)); \
                        if (p_param_value == NULL) { \
                            metac_load_of_parameter_delete(p_pload); \
                            return NULL; \
                        } \
                        _type_ * p_param_value_ptr = metac_value_addr(p_param_value); \
                        *p_param_value_ptr = (_type_)va_arg(p_cntr->parameters, _va_arg_type); \
                    }
                    break;
                // signed
                case 'c':
                case 'i':
                case 'd': {
                        if (dummy_specifier.t == 'c' ||
                            dummy_specifier.arg_len == psal_hh) {
                            _process(char, int);
                        } else if (dummy_specifier.t != 'c') {
                            if (dummy_specifier.arg_len == psal_h) {
                                _process(short, int);
                            } else if (dummy_specifier.arg_len == psal_no) {
                                _process(int, int);
                            } else if (dummy_specifier.arg_len == psal_l) {
                                _process(long, long);
                            } else if (dummy_specifier.arg_len == psal_ll) {
                                _process(long long, long long);
                            } else {
                                return NULL;
                            }
                        } else {
                            return NULL;
                        }
                    }
                    break;
                // unsigned
                case 'o': 
                case 'u':
                case 'x':
                case 'X': {
                        if (dummy_specifier.arg_len == psal_hh) {
                            _process(unsigned char, unsigned int);
                        } else if (dummy_specifier.arg_len == psal_h) {
                            _process(unsigned short, unsigned int);
                        } else if (dummy_specifier.arg_len == psal_no) {
                            _process(unsigned int, unsigned int);
                        } else if (dummy_specifier.arg_len == psal_l) {
                            _process(unsigned long, unsigned long);
                        } else if (dummy_specifier.arg_len == psal_ll) {
                            _process(unsigned long long, unsigned long long);
                        } else {
                            return NULL;
                        }
                    }
                    break;
                // unsigned
                case 'f': 
                case 'g':
                case 'e': {
                        if (dummy_specifier.arg_len == psal_no) {
                            _process(double, double);
                        } else if (dummy_specifier.arg_len == psal_L) {
                            _process(long double, long double);
                        } else {
                            return NULL;
                        }
                    }
                    break;
#undef _process
                default: {
                        metac_load_of_parameter_delete(p_pload);
                        return NULL;
                    }
                }
            }
            ++param_id;
        }
        ++pos;
    }

    metac_value_t * p_return_value = NULL;
    if (p_pload != NULL) {
        p_return_value = metac_new_value(p_va_list_entry, p_pload);
        if (p_return_value == NULL) {
            metac_load_of_parameter_delete(p_pload);
        }
    }
    return p_return_value;
}