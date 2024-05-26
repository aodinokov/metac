#ifndef INCLUDE_METAC_BACKEND_STRING_H_
#define INCLUDE_METAC_BACKEND_STRING_H_

#include <stdio.h> /* snprintf */
#include <stdlib.h> /* calloc */
#include <string.h> /* strlen */
#include <stdarg.h> /* va_start */

#define dsprintf_render_with_buf(_n_) \
    static char * dsprintf##_n_(const char *format, ...){ \
        char * res; \
        char buf[_n_]; \
        va_list args; \
        va_start(args, format); \
        int len = vsnprintf(buf, sizeof(buf), format, args); \
        va_end(args); \
        if ((len) < sizeof(buf)) { \
            res = strdup(buf); \
        } else { \
            res = calloc(len + 1, sizeof(char)); \
            if (res != NULL) { \
                va_start(args, format); \
                vsnprintf(res, len + 1, format, args); \
                va_end(args); \
            } \
        } \
        return res; \
    }

static char * trim_trailing_spaces(char *param) {
    if (param == NULL) {
        return NULL;
    }
    size_t param_len = strlen(param);
    size_t i = 0;
    while (i < param_len) {
        if (param[param_len - i -1] == ' '){
            param[param_len - i -1] = '\0';
        } else {
            break;
        }
        ++i;
    }
    return param;
}


#endif