#include "metac/reflect.h"
#include <stdlib.h> /*free*/

#define METAC_WRAP_FN_RES(_tag_map_, _fn_, _args_...) ({ \
        metac_parameter_storage_t * p_param_storage = metac_new_parameter_storage(); \
        if (p_param_storage != NULL) { \
            p_val = metac_value_parameter_wrap(metac_new_value(METAC_GSYM_LINK_ENTRY(_fn_), p_param_storage), _tag_map_, _args_); \
        } \
        _fn_(_args_);\
})

int my_printf(const char * format, ...) {
    va_list l;
    va_start(l, format);
    int res = vprintf(format, l);
    va_end(l);
    return res;
}
METAC_GSYM_LINK(my_printf);

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


int main() {
    metac_tag_map_t * p_tagmap = va_args_tag_map();

    metac_value_t * p_val = NULL;
    printf("fn returned: %i\n", METAC_WRAP_FN_RES(p_tagmap, my_printf, "%d %d\n", 10, 22));
    if (p_val != NULL) {
        char * s = metac_value_string_ex(p_val, METAC_WMODE_deep, p_tagmap);
        if (s != NULL) {
            printf("captured %s\n", s);
            free(s);
        }
        metac_parameter_storage_t * p_param_storage = (metac_parameter_storage_t *)metac_value_addr(p_val);
        metac_parameter_storage_delete(p_param_storage);
        metac_value_delete(p_val);
    }

    metac_tag_map_delete(p_tagmap);
    return 0;
}