#include "metac/reflect.h"

metac_value_t * metac_new_value_with_call_params(metac_entry_t *p_entry);

#define METAC_NEW_VALUE_WITH_CALL_PARAMS_AND_WRAP(_tag_map_, _fn_entry_, _fn_, _args_...) ({ \
        metac_value_t * p_val = metac_value_parameter_wrap( \
            metac_new_value_with_call_params(_fn_entry_), _tag_map_, _args_); \
        /* this is to control number of parameters during compilation */ \
        if (0) { \
            _fn_(_args_); \
        } \
        p_val; \
    })

metac_value_t * metac_new_value_with_call_result(metac_value_t * p_param_storage_val);
int metac_value_call(metac_value_t * p_param_storage_val, void (*fn)(void), metac_value_t * p_res_value);
void metac_value_with_call_result_delete(metac_value_t * p_res_value);
void metac_value_with_call_params_delete(metac_value_t * p_param_value);
