// this is a very naive version. it's needed to demonstrate the idea of how to print va_args
// see value_with_args.c and its test for more details
#include "metac/reflect.h"

#include "metac/backend/printf_format.h"
#include "metac/backend/value.h"

#include <stdio.h>
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
            p_specifier->arg_len = psal_l;
        }
        (*pos)++;
        if (*pos < strlen(format) && (format[*pos] == 'l')) {
            p_specifier->arg_len = psal_ll;
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

static metac_value_t * _new_value_from_format_specifier(const char * format, void * fn_ptr, struct va_list_container * p_cntr);

metac_value_t * metac_new_value_vprintf(const char * format, va_list parameters) {
    struct va_list_container cntr = {};

    va_copy(cntr.parameters, parameters);
    metac_value_t * p_value = _new_value_from_format_specifier(format, (void*)metac_new_value_vprintf, &cntr);
    va_end(cntr.parameters);

    return p_value;
}
METAC_GSYM_LINK(metac_new_value_vprintf);

metac_value_t * metac_new_value_printf(const char * format, ...) {
    struct va_list_container cntr = {};

    va_start(cntr.parameters, format);
    metac_value_t * p_value = _new_value_from_format_specifier(format, (void*)metac_new_value_printf, &cntr);
    va_end(cntr.parameters);

    return p_value;
}
METAC_GSYM_LINK(metac_new_value_printf);

static metac_value_t * _new_value_from_format_specifier(const char * format, void * fn_ptr, struct va_list_container * p_cntr) {
    metac_entry_t *p_fn_entry = NULL;
    if (fn_ptr == metac_new_value_vprintf) {
        p_fn_entry = METAC_GSYM_LINK_ENTRY(metac_new_value_vprintf);
    } else if (fn_ptr == metac_new_value_printf) {
        p_fn_entry = METAC_GSYM_LINK_ENTRY(metac_new_value_printf);
    }

    if (p_fn_entry == NULL ||
        metac_entry_has_parameters(p_fn_entry) == 0 ||
        metac_entry_parameters_count(p_fn_entry) != 2) {
        return NULL;
    }

    metac_entry_t * p_va_list_entry = metac_entry_by_paremeter_id(p_fn_entry, 1);
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
                //case 's'/* hmm.. here we could identify length TODO: to think. maybe instead of p_load we need to return array of metac_values */: 
                case 'p'/* pointers including strings */: {
                        WITH_METAC_DECLLOC(decl, void * p_param_value_ptr = NULL);
                        metac_entry_t * p_param_entry = METAC_ENTRY_FROM_DECLLOC(decl, p_param_value_ptr);
                        metac_value_t * p_param_value = metac_load_of_parameter_new_value(p_pload, param_id, p_param_entry, sizeof(void*));
                        if (p_param_value == NULL) {
                            metac_load_of_parameter_delete(p_pload);
                            return NULL;                           
                        }
                        p_param_value_ptr = metac_value_addr(p_param_value);

                        void * p = va_arg(p_cntr->parameters, void*);
                        memcpy(p_param_value_ptr, &p, sizeof(p));
                        
                    }
                    break;
                case 'd':/* basic types */
                case 'i':
                case 'c': {

                    }
                    break;
                default: {
                        metac_load_of_parameter_delete(p_pload);
                        return NULL;
                    }
                }
            }
        }
        ++param_id;
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