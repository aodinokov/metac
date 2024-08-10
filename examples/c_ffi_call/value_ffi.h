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

#define METAC_CLEAN_PARAMS(_p_val_) do { \
        metac_value_t * _p_val = _p_val_; \
        metac_parameter_storage_t * p_param_storage = (metac_parameter_storage_t *)metac_value_addr(_p_val); \
        metac_value_delete(_p_val); \
        metac_parameter_storage_delete(p_param_storage); \
    } while(0)

metac_value_t * metac_value_call(metac_value_t * p_param_val, void (*fn)(void));
