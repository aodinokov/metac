#include "metac/reflect.h"

#define METAC_WRAP_FN_PARAMS(_tag_map_, _fn_, _args_...) ({ \
        metac_value_t * p_val = NULL; \
        metac_parameter_storage_t * p_param_storage = metac_new_parameter_storage(); \
        if (p_param_storage != NULL) { \
            p_val = metac_new_value_with_parameters(p_param_storage, _tag_map_, METAC_GSYM_LINK_ENTRY(_fn_), _args_); \
        } \
        /* this is to control number of parameters during compilation */ \
        if (0) { \
            _fn_(_args_); \
        } \
        p_val; \
    })

metac_value_t * metac_new_value_with_call_result(metac_value_t * p_param_storage_val);
int metac_value_call(metac_value_t * p_param_storage_val, void (*fn)(void), metac_value_t * p_res_value);
void metac_value_with_call_result_delete(metac_value_t * p_res_value);
void metac_value_with_call_parameters_delete(metac_value_t * p_param_value);
