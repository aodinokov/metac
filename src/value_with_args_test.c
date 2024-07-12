#include "metac/test.h"
#include <stdio.h>

#include "entry.c"
#include "entry_cdecl.c"
#include "entry_db.c"
#include "entry_tag.c"
#include "iterator.c"
#include "hashmap.c"
#include "printf_format.c"
#include "value.c"
#include "value_base_type.c"
#include "value_string.c"

#include "value_with_args.c"

#define METAC_NEW_VALUE_WITH_ARGS(_p_tag_map_, _fn_, _args_...) \
        metac_new_value_with_parameters(_p_tag_map_, METAC_GSYM_LINK_ENTRY(_fn_), _args_)

void test_function_with_base_args(
    char arg_00,
    unsigned char arg_01,
    short arg_02,
    unsigned short arg_03,
    int arg_04,
    unsigned int arg_05,
    long arg_06,
    unsigned long arg_07,
    long long arg_08,
    unsigned long long arg_09,
    bool arg_10,
    float arg_11,
    double arg_12,
    long double arg_13,
    float complex arg_14,
    double complex arg_15,
    long double complex arg_16) {
    return;
}
METAC_GSYM_LINK(test_function_with_base_args);

void test_function_with_base_args_ptr(char * arg_00,
    unsigned char *arg_01,
    short *arg_02,
    unsigned short *arg_03,
    int *arg_04,
    unsigned int *arg_05,
    long *arg_06,
    unsigned long *arg_07,
    long long *arg_08,
    unsigned long long *arg_09,
    bool *arg_10,
    float *arg_11,
    double *arg_12,
    long double *arg_13,
    float complex *arg_14,
    double complex *arg_15,
    long double complex *arg_16) {
    return;
}
METAC_GSYM_LINK(test_function_with_base_args_ptr);

METAC_START_TEST(base_type_args_to_value) {
    char arg_00 = 0;
    unsigned char arg_01 = 1;
    short arg_02 = 2;
    unsigned short arg_03 = 3;
    int arg_04 = 4;
    unsigned int arg_05 = 5;
    long arg_06 = 6;
    unsigned long arg_07 = 7;
    long long arg_08 = 8;
    unsigned long long arg_09 = 9;
    bool arg_10 = true;
    float arg_11 = 11.0;
    double arg_12 = 12.0;
    long double  arg_13 = 13.0;
    float complex  arg_14 = 14.0 - 14.0*I;
    double complex arg_15 = 15.0 - 15.0*I;
    long double complex arg_16 = 16.0 + 116.0*I;

    metac_value_t *p_val;

    p_val = METAC_NEW_VALUE_WITH_ARGS(NULL, test_function_with_base_args,
        arg_00, arg_01, arg_02, arg_03, arg_04,
        arg_05, arg_06, arg_07, arg_08, arg_09, 
        arg_10, arg_11, arg_12, arg_13, arg_14,
        arg_15, arg_16);
    fail_unless(p_val != NULL, "failed to collect args of test_function_with_base_args");
    metac_value_with_parameters_delete(p_val);

    p_val = METAC_NEW_VALUE_WITH_ARGS(NULL, test_function_with_base_args_ptr,
        &arg_00, &arg_01, &arg_02, &arg_03, &arg_04,
        &arg_05, &arg_06, &arg_07, &arg_08, &arg_09, 
        &arg_10, &arg_11, &arg_12, &arg_13, &arg_14,
        &arg_15, &arg_16);
    fail_unless(p_val != NULL, "failed to collect args of test_function_with_base_args");
    metac_value_with_parameters_delete(p_val);
}END_TEST

enum test_enum_01 {
    e01_0 = 0,
    e01_end = 1,
};

enum test_enum_02 {
    e02_0 = 0,
    e02_1 = 1,
    e02_end = 0x100,
};

enum test_enum_04 {
    e04_0 = 0,
    e04_1 = 1,
    e04_end = 0x100000,
};

typedef enum test_enum_08 {
    e08_0 = 0,
    e08_1 = 1,
    e08_end = 0x1000000000,
}test_enum_08_t;

void test_function_with_enum_args(
    enum test_enum_01 arg_00,
    enum test_enum_02 arg_01,
    enum test_enum_04 arg_02,
    test_enum_08_t arg_03) {
    return;
}
METAC_GSYM_LINK(test_function_with_enum_args);

void test_function_with_enum_args_ptr(
    enum test_enum_01 * arg_00,
    enum test_enum_02 * arg_01,
    enum test_enum_04 * arg_02,
    test_enum_08_t * arg_03) {
    return;
}
METAC_GSYM_LINK(test_function_with_enum_args_ptr);

METAC_START_TEST(enum_to_value) {
    enum test_enum_01 arg_00 = e01_end;
    enum test_enum_02 arg_01 = e02_end;
    enum test_enum_04 arg_02 = e04_end;
    enum test_enum_08 arg_03 = e08_end;
    
    metac_value_t *p_val;

    p_val = METAC_NEW_VALUE_WITH_ARGS(NULL, test_function_with_enum_args, arg_00, arg_01, arg_02, arg_03);
    fail_unless(p_val != NULL, "failed to collect args of test_function_with_enum_args");
    metac_value_with_parameters_delete(p_val);
    p_val = METAC_NEW_VALUE_WITH_ARGS(NULL, test_function_with_enum_args_ptr, &arg_00, &arg_01, &arg_02, &arg_03);
        fail_unless(p_val != NULL, "failed to collect args of test_function_with_enum_args_ptr");
    metac_value_with_parameters_delete(p_val);
}END_TEST


typedef struct {
    short a;
    int arr[10];
} test_struct_t;

void test_function_with_struct_args(
    test_struct_t arg_00,
    test_struct_t * arg_01,
    test_struct_t ** arg_02) {
    return;
}
METAC_GSYM_LINK(test_function_with_struct_args);

METAC_START_TEST(struct_to_value) {
    test_struct_t arg_00 = {.a = 1, .arr = {0,}};
    test_struct_t * arg_01 = &arg_00;
    test_struct_t ** arg_02 = &arg_01;
    for (int i=0; i < sizeof(arg_00.arr)/sizeof(arg_00.arr[0]); ++i){
        arg_00.arr[i] = i;
    }
    arg_00.a = sizeof(arg_00.arr)/sizeof(arg_00.arr[0]);

    metac_value_t *p_val;

    p_val = METAC_NEW_VALUE_WITH_ARGS(NULL, test_function_with_struct_args, arg_00, arg_01, arg_02);
    fail_unless(p_val != NULL, "failed to collect args of test_function_with_enum_args");
    metac_value_with_parameters_delete(p_val);
}END_TEST


typedef int test_1d_array_t[15];
typedef int test_2d_array_t[3][5];

void test_function_with_array_args(
    test_1d_array_t arg_00,
    test_1d_array_t * arg_01,
    test_1d_array_t ** arg_02,
    test_2d_array_t arg_03,
    test_2d_array_t * arg_04,
    test_2d_array_t ** arg_05) {
    return;
}
METAC_GSYM_LINK(test_function_with_array_args);

METAC_START_TEST(array_to_value) {
    test_1d_array_t arg_00 = {1,2,3};
    test_1d_array_t * arg_01 = &arg_00;
    test_1d_array_t ** arg_02 = &arg_01;

    test_2d_array_t arg_03 = {{1,2,3},{4,5,6},};
    test_2d_array_t * arg_04 = &arg_03;
    test_2d_array_t ** arg_05 = &arg_04;

    metac_value_t *p_val;

    p_val = METAC_NEW_VALUE_WITH_ARGS(NULL, test_function_with_array_args, arg_00, arg_01, arg_02, arg_03, arg_04, arg_05);
    fail_unless(p_val != NULL, "failed to collect args of test_function_with_enum_args");
    metac_value_with_parameters_delete(p_val);
}END_TEST

void test_function_with_va_args(const char * format, ...) {
    va_list l;
    va_start(l, format);
    vprintf(format, l);
    va_end(l);
    return;
}
METAC_GSYM_LINK(test_function_with_va_args);

#include "metac/backend/printf_format.h"

// TODO: we need to write a test for this
static int _va_arg_hdlr(metac_value_walker_hierarchy_t *p_hierarchy, metac_value_event_t * p_ev, void *p_context) {
    if (p_ev == NULL) {
        return -(EINVAL);
    }
    if (p_ev->type != METAC_RQVST_va_list ||
        p_ev->p_va_list_container == NULL ||
        metac_value_walker_hierarchy_level(p_hierarchy) < 0) {
        return -(EINVAL);
    }
    metac_value_t *p_val = metac_value_walker_hierarchy_value(p_hierarchy, 0);
    metac_value_t *p_param_val = metac_new_value_by_paremeter_id(p_val, p_ev->va_list_param_id -1 /* use previous param */);

    if (p_param_val == NULL) {
        return -(EINVAL);
    }

    if (metac_value_is_pointer(p_param_val) == 0) {
        metac_value_delete(p_param_val);
        return -(EINVAL);
    }
    // TODO: check that it's char * (use cdecl?)

    // extract pointer
    char * format = NULL;
    if (metac_value_pointer(p_param_val, (void **)&format) != 0) {
        metac_value_delete(p_param_val);
        return -(EINVAL);
    }
    metac_value_delete(p_param_val);

    if (format == NULL) {
        return -(EINVAL);
    }

    metac_num_t parameters_count = metac_count_format_specifiers(format);
    metac_entry_t * p_entry = NULL;
    metac_parameter_load_t * p_pload = metac_new_parameter_load(parameters_count);
    if (p_pload == NULL) {
        return -(EINVAL);
    }

    metac_num_t param_id = 0;
    size_t pos = 0;
    metac_printf_specifier_t dummy_specifier;

    struct va_list_container cntr = {};
    va_copy(cntr.parameters, p_ev->p_va_list_container->parameters);

    while (pos < strlen(format)) {
        if (format[pos] == '%') {
            // Check if a valid format specifier follows
            if (metac_parse_format_specifier(format, &pos, &dummy_specifier) > 0) {
                switch (dummy_specifier.t) {
                case 's'/* hmm.. here we could identify length TODO: to think. maybe instead of p_load we need to return array of metac_values */: 
                case 'p'/* pointers including strings */:{
                        WITH_METAC_DECLLOC(decl, void * p_buf = calloc(1, sizeof(void*)));
                        if (p_buf == NULL) {
                            va_end(cntr.parameters);
                            metac_parameter_load_delete(p_pload);
                            return -(EINVAL);                           
                        }
                        void * p = va_arg(cntr.parameters, void*);
                        memcpy(p_buf, &p, sizeof(p));
                        p_entry = METAC_ENTRY_FROM_DECLLOC(decl, p_buf);

                        // TODO: p_pload->p_val[param_id]
                    }
                    break;
                case 'd':/* basic types */
                case 'i':
                case 'c': {

                    }
                    break;
                default: {
                        va_end(cntr.parameters);
                        metac_parameter_load_delete(p_pload);
                        return -(EINVAL);
                    }
                }
            }
        }
        ++param_id;
        ++pos;
    }
    va_end(cntr.parameters);

    if (p_pload != NULL) {    // TODO: check leaks
        p_ev->p_return_value = metac_new_value(p_ev->p_va_list_param_entry, p_pload);
        if (p_ev->p_return_value == NULL) {
            metac_parameter_load_delete(p_pload);
        }
    }
    return 0;
}

METAC_TAG_MAP_NEW(va_args_tag_map, NULL, {.mask = 
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_variable) |
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_subprogram_parameter) | 
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_member) |
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_final),},)
    /* start tags for all types */

    METAC_TAG_MAP_ENTRY(METAC_GSYM_LINK_ENTRY(test_function_with_va_args))
        METAC_TAG_MAP_SET_TAG(0, METAC_TEO_entry, 0, METAC_TAG_MAP_ENTRY_PARAMETER({.i = 1}), 
            .handler = _va_arg_hdlr,
        )
    METAC_TAG_MAP_ENTRY_END
METAC_TAG_MAP_END

METAC_START_TEST(va_arg_to_value) {
    metac_tag_map_t *p_tag_map = va_args_tag_map();

    metac_value_t *p_val;
    p_val = METAC_NEW_VALUE_WITH_ARGS(p_tag_map, test_function_with_va_args, "%05p\n", NULL);
    fail_unless(p_val != NULL, "failed to collect args of test_function_with_enum_args");
    metac_value_with_parameters_delete(p_val);

    metac_tag_map_delete(p_tag_map);
}