#ifndef PRINT_ARGS_H_
#define PRINT_ARGS_H_

#include "metac/reflect.h"

#include <stdlib.h> /*free*/

#define METAC_WRAP_FN_NORES(_fn_, _args_...) { \
        metac_value_t * p_val = NULL; \
        metac_parameter_storage_t * p_param_storage = metac_new_parameter_storage(); \
        if (p_param_storage != NULL) { \
            p_val = metac_new_value_with_parameters(p_param_storage, NULL, METAC_GSYM_LINK_ENTRY(_fn_), _args_); \
        } \
        if (p_val != NULL) { \
            char * s = metac_value_string_ex(p_val, METAC_WMODE_deep, NULL); \
            if (s != NULL) { \
                printf("Calling %s\n", s); \
                free(s); \
            } \
        } \
        _fn_(_args_); \
        if (p_val != NULL) { \
            char * s = metac_value_string_ex(p_val, METAC_WMODE_deep, NULL); \
            if (s != NULL) { \
                printf("Returned void from %s\n", s); \
                free(s); \
            } \
            metac_value_delete(p_val); \
        } \
        if (p_param_storage != NULL) { \
            metac_parameter_storage_delete(p_param_storage); \
        } \
    }
#define METAC_WRAP_FN_RES(_type_, _fn_, _args_...) ({ \
        metac_value_t * p_val = NULL; \
        metac_parameter_storage_t * p_param_storage = metac_new_parameter_storage(); \
        if (p_param_storage != NULL) { \
            p_val = metac_new_value_with_parameters(p_param_storage, NULL, METAC_GSYM_LINK_ENTRY(_fn_), _args_); \
        } \
        if (p_val != NULL) { \
            char * s = metac_value_string_ex(p_val, METAC_WMODE_deep, NULL); \
            if (s != NULL) { \
                printf("Calling %s\n", s); \
                free(s); \
            } \
        } \
        WITH_METAC_DECLLOC(loc, _type_ res = _fn_(_args_)); \
        if (p_val != NULL) { \
            metac_value_t *p_res_val = METAC_VALUE_FROM_DECLLOC(loc, res); \
            char * s = metac_value_string_ex(p_val, METAC_WMODE_deep, NULL); \
            char * s_res = NULL; \
            if (p_res_val != NULL) { \
               s_res = metac_value_string_ex(p_res_val, METAC_WMODE_deep, NULL); \
               metac_value_delete(p_res_val); \
            } \
            if (s != NULL && s_res != NULL) { \
                printf("Returned %s from %s\n", s_res, s); \
            } \
            if (s_res != NULL) { \
                free(s_res); \
            } \
            if (s != NULL) { \
                free(s); \
            } \
            metac_value_delete(p_val); \
        } \
        if (p_param_storage != NULL) { \
            metac_parameter_storage_delete(p_param_storage); \
        } \
        res; \
    })

#endif