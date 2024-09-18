
#include "print_args.h"
#include "metac/backend/va_list_ex.h"

#include <stdlib.h> /*free*/

int test_function1_with_args(int a, short b){
    printf("called\n");
    return a + b + 6;
}
METAC_GSYM_LINK(test_function1_with_args);

typedef struct list_s{
    double x;
    struct list_s * p_next;
}list_t;

double test_function3_with_args(list_t *p_list) {
    double sum = 0.0;
    while(p_list != NULL) {
        sum += p_list->x;
        p_list->x += 1;
        
        p_list = p_list->p_next;
    }
    printf("!!!result is %f %Lf!!!\n", sum, (long double)sum);
    return sum;
}
METAC_GSYM_LINK(test_function3_with_args);

int my_printf(const char * format, ...) {
    va_list l;
    va_start(l, format);
    int res = vprintf(format, l);
    va_end(l);
    return res;
}
METAC_GSYM_LINK(my_printf);

int my_vprintf(const char * format, va_list l) {
    return vprintf(format, l);
}
METAC_GSYM_LINK(my_vprintf);

enum x {
    xOne = 1,
    xTwo = 2,
    xMinusOne = -1,
};
int test_function4_with_enum_args(enum x arg0, enum x arg1, enum x arg2) {
    return 0;
}
METAC_GSYM_LINK(test_function4_with_enum_args);

int test_function4_with_struct_args(list_t x) {
    return 0;
}
METAC_GSYM_LINK(test_function4_with_struct_args);

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

    METAC_TAG_MAP_ENTRY(METAC_GSYM_LINK_ENTRY(my_vprintf))
        METAC_TAG_MAP_SET_TAG(0, METAC_TEO_entry, 0, METAC_TAG_MAP_ENTRY_PARAMETER({.n = "format"}),
            METAC_ZERO_ENDED_STRING()
        )
        METAC_TAG_MAP_SET_TAG(0, METAC_TEO_entry, 0, METAC_TAG_MAP_ENTRY_PARAMETER({.n = "l"}), 
            METAC_FORMAT_BASED_VA_ARG()
        )
    METAC_TAG_MAP_ENTRY_END

METAC_TAG_MAP_END
//

int main() {
    p_tagmap = va_args_tag_map();

    printf("fn returned: %i\n", METAC_WRAP_FN_RES(NULL, test_function1_with_args, 10, 22));

    list_t * p_list = (list_t[]){{.x = 42.42, .p_next = (list_t[]){{ .x = 45.4, .p_next = NULL}}}};
    printf("fn returned: %f\n", METAC_WRAP_FN_RES(NULL, test_function3_with_args, p_list));

    printf("fn returned: %i\n", METAC_WRAP_FN_RES(p_tagmap, my_printf, "%d %d\n", 10, 22));

    printf("fn returned: %i\n", METAC_WRAP_FN_RES(p_tagmap, test_function4_with_enum_args, xOne, xTwo, xMinusOne));
    printf("fn returned: %i\n", METAC_WRAP_FN_RES(p_tagmap, test_function4_with_enum_args, 1, 2, -1));

    printf("fn returned: %i\n", METAC_WRAP_FN_RES(p_tagmap, test_function4_with_struct_args, *p_list));

    WITH_VA_LIST_CONTAINER(c,
        VA_LIST_CONTAINER(c, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
        printf("fn returned: %i\n", METAC_WRAP_FN_RES(p_tagmap, my_vprintf, "%d %d %d %d %d %d %d %d %d %d\n", c.parameters));
    );

    metac_tag_map_delete(p_tagmap);
    return 0;
}