// this is a very naive version. it's needed to demonstrate the idea of how to print va_args
// see value_with_args.c and its test for more details
#include "metac/backend/printf_format.h"


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
