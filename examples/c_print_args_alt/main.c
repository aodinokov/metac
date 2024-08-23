#include "metac/reflect.h"
#include "mr.h"

#include <stdlib.h> /*free*/

int test_function1_with_args(int a, short b){
    printf("called\n");
    return a + b + 6;
}
METAC_GSYM_LINK(test_function1_with_args);

int my_printf(const char * format, ...) {
    va_list l;
    va_start(l, format);
    int res = vprintf(format, l);
    va_end(l);
    return res;
}
METAC_GSYM_LINK(my_printf);


metac_tag_map_t * p_tagmap = NULL;
METAC_TAG_MAP_NEW(va_args_tag_map, NULL, {.mask = 
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_variable) |
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_func_parameter) | 
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_member) |
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_final),},)
    /* start tags for all types */

    METAC_TAG_MAP_ENTRY(METAC_GSYM_LINK_ENTRY(my_printf))
        METAC_TAG_MAP_SET_TAG(0, METAC_TEO_entry, 0, METAC_TAG_MAP_ENTRY_PARAMETER({.n = "format"}),
            METAC_ZERO_ENDED_STRING()
        )
        METAC_TAG_MAP_SET_TAG(0, METAC_TEO_entry, 0, METAC_TAG_MAP_ENTRY_PARAMETER({.i = 1}), 
            METAC_FORMAT_BASED_VA_ARG()
        )
    METAC_TAG_MAP_ENTRY_END

METAC_TAG_MAP_END

#define _APPEND(x) do { \
        WITH_METAC_DECLLOC(decl, typeof(x) _x_val = x); \
        metac_entry_t *p_entry = METAC_ENTRY_FROM_DECLLOC(decl, _x_val); \
        metac_entry_t *p_param_entry = metac_entry_by_paremeter_id(p_val_entry, param_id); \
        /* TODO: compare types */ \
        if (metac_parameter_storage_append_by_buffer(p_param_storage, p_param_entry, sizeof(_x_val)) == 0) { \
            metac_value_t * p_param_value = metac_parameter_storage_new_param_value(p_param_storage, param_id); \
            if (p_param_value != NULL) { \
                memcpy(metac_value_addr(p_param_value), &_x_val, sizeof(_x_val)); \
                metac_value_delete(p_param_value); \
            } \
        } \
        ++param_id; \
    }while(0);

// alternative implementation
#define METAC_WRAP_FN_RES(_tag_map_, _fn_, _args_...) ({ \
        metac_value_t * p_val = NULL; \
        metac_parameter_storage_t * p_param_storage = metac_new_parameter_storage(); \
        if (p_param_storage != NULL) { \
            /*p_val = metac_value_parameter_wrap(metac_new_value(METAC_GSYM_LINK_ENTRY(_fn_), p_param_storage), _tag_map_, _args_); */ \
            p_val = metac_new_value(METAC_GSYM_LINK_ENTRY(_fn_), p_param_storage); \
            metac_num_t param_id = 0; \
            metac_entry_t *p_val_entry = metac_value_entry(p_val); \
            /* append params*/ \
            MR_FOREACH(_APPEND, _args_) \
        } \
        if (p_val != NULL) { \
            char * s = metac_value_string_ex(p_val, METAC_WMODE_deep, _tag_map_); \
            if (s != NULL) { \
                printf("Calling %s\n", s); \
                free(s); \
            } \
        } \
        WITH_METAC_DECLLOC(loc, typeof(_fn_(_args_)) res = _fn_(_args_)); \
        if (p_val != NULL) { \
            metac_value_t *p_res_val = METAC_VALUE_FROM_DECLLOC(loc, res); \
            char * s = metac_value_string_ex(p_val, METAC_WMODE_deep, _tag_map_); \
            char * s_res = NULL; \
            if (p_res_val != NULL) { \
               s_res = metac_value_string_ex(p_res_val, METAC_WMODE_deep, _tag_map_); \
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

//

int main() {
    p_tagmap = va_args_tag_map();

    printf("fn returned: %i\n", METAC_WRAP_FN_RES(NULL, test_function1_with_args,10, 22));

    metac_tag_map_delete(p_tagmap);
    return 0;
}