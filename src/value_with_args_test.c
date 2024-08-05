#include "metac/test.h"
#include <stdio.h>

#include "metac/backend/va_list_ex.h"

#include "entry.c"
#include "entry_cdecl.c"
#include "entry_db.c"
#include "entry_tag.c"
#include "iterator.c"
#include "hashmap.c"
#include "printf_format.c"
#include "value.c"
#include "value_base_type.c"
#include "value_deep.c"
#include "value_string.c"

#include "value_with_args.c"

#if VA_ARG_IN_VA_ARG != 0
void va_arg_in_va_arg_lvl_3(int expected, ...) {
    struct va_list_container in_cntr;
    va_start(in_cntr.parameters, expected);
    
    struct va_list_container local_cntr = {};
#if __linux__
    // linux (gcc) can't extract va_arg from va_arg
    void * p = va_arg(in_cntr.parameters, void*);
    memcpy(&local_cntr, p, sizeof(local_cntr));
#else
    // other platforms
    _va_list_cp_to_container(&local_cntr, va_arg(in_cntr.parameters, va_list));
#endif

    int res = va_arg(local_cntr.parameters, int);
    fail_unless(expected == res, "expected %d, got %d", expected, res);
    va_end(in_cntr.parameters);
}

void va_arg_in_va_arg_lvl_2(int expected, va_list ap) {
    va_arg_in_va_arg_lvl_3(expected, ap);
}

void va_arg_in_va_arg_lvl_1(int expected, ...) {
    va_list ap;
    va_start(ap, expected);
    va_arg_in_va_arg_lvl_2(expected, ap);
    va_end(ap);
}

METAC_START_TEST(va_arg_in_va_arg_precheck) {
    va_arg_in_va_arg_lvl_1(1, 1);
    va_arg_in_va_arg_lvl_1(-1000, -1000);
}END_TEST
#endif

#define METAC_NEW_VALUE_WITH_ARGS_FOR_FN(_p_tag_map_, _fn_, _args_...) ({ \
    metac_value_t * p_val = NULL; \
    metac_parameter_storage_t * p_param_storage = metac_new_parameter_storage(); \
    if (p_param_storage != NULL) { \
        p_val = metac_new_value_with_parameters(p_param_storage, _p_tag_map_, METAC_GSYM_LINK_ENTRY(_fn_), _args_); \
    } \
    p_val; \
})

#define METAC_VALUE_WITH_ARGS_DELETE(_p_val_) do { \
        metac_parameter_storage_t * p_param_storage = (metac_parameter_storage_t *)metac_value_addr(_p_val_); \
        if (p_param_storage != NULL) { \
            metac_parameter_storage_delete(p_param_storage); \
        } \
        metac_value_delete(_p_val_); \
    }while(0)

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
typedef void test_function_with_base_args_t(
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
    long double complex arg_16);

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
typedef void test_function_with_base_args_ptr_t(char * arg_00,
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
    long double complex *arg_16);

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

    p_val = METAC_NEW_VALUE_WITH_ARGS_FOR_FN(NULL, test_function_with_base_args,
        arg_00, arg_01, arg_02, arg_03, arg_04,
        arg_05, arg_06, arg_07, arg_08, arg_09, 
        arg_10, arg_11, arg_12, arg_13, arg_14,
        arg_15, arg_16);
    fail_unless(p_val != NULL, "failed to collect args of test_function_with_base_args");
    METAC_VALUE_WITH_ARGS_DELETE(p_val);

    p_val = METAC_NEW_VALUE_WITH_ARGS_FOR_FN(NULL, test_function_with_base_args_ptr,
        &arg_00, &arg_01, &arg_02, &arg_03, &arg_04,
        &arg_05, &arg_06, &arg_07, &arg_08, &arg_09, 
        &arg_10, &arg_11, &arg_12, &arg_13, &arg_14,
        &arg_15, &arg_16);
    fail_unless(p_val != NULL, "failed to collect args of test_function_with_base_args");
    METAC_VALUE_WITH_ARGS_DELETE(p_val);
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

    p_val = METAC_NEW_VALUE_WITH_ARGS_FOR_FN(NULL, test_function_with_enum_args, arg_00, arg_01, arg_02, arg_03);
    fail_unless(p_val != NULL, "failed to collect args of test_function_with_enum_args");
    METAC_VALUE_WITH_ARGS_DELETE(p_val);
    p_val = METAC_NEW_VALUE_WITH_ARGS_FOR_FN(NULL, test_function_with_enum_args_ptr, &arg_00, &arg_01, &arg_02, &arg_03);
        fail_unless(p_val != NULL, "failed to collect args of test_function_with_enum_args_ptr");
    METAC_VALUE_WITH_ARGS_DELETE(p_val);
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

    p_val = METAC_NEW_VALUE_WITH_ARGS_FOR_FN(NULL, test_function_with_struct_args, arg_00, arg_01, arg_02);
    fail_unless(p_val != NULL, "failed to collect args of test_function_with_enum_args");
    METAC_VALUE_WITH_ARGS_DELETE(p_val);
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

    p_val = METAC_NEW_VALUE_WITH_ARGS_FOR_FN(NULL, test_function_with_array_args, arg_00, arg_01, arg_02, arg_03, arg_04, arg_05);
    fail_unless(p_val != NULL, "failed to collect args of test_function_with_enum_args");
    METAC_VALUE_WITH_ARGS_DELETE(p_val);
}END_TEST


void test_function_with_va_list(const char * format, va_list vl) {
    vprintf(format, vl);
}
METAC_GSYM_LINK(test_function_with_va_list);

void test_function_with_va_args(const char * format, ...) {
    va_list l;
    va_start(l, format);
    test_function_with_va_list(format, l);
    va_end(l);
}
METAC_GSYM_LINK(test_function_with_va_args);

void test_array_len(int * arr, int len) {}
METAC_GSYM_LINK(test_array_len);

METAC_TAG_MAP_NEW(va_args_tag_map, NULL, {.mask = 
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_variable) |
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_func_parameter) | 
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_member) |
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_final),},)
    /* start tags for all types */

    METAC_TAG_MAP_ENTRY(METAC_GSYM_LINK_ENTRY(test_function_with_va_args))
        METAC_TAG_MAP_SET_TAG(0, METAC_TEO_entry, 0, METAC_TAG_MAP_ENTRY_PARAMETER({.n = "format"}),
            METAC_ZERO_ENDED_STRING()
        )
        METAC_TAG_MAP_SET_TAG(0, METAC_TEO_entry, 0, METAC_TAG_MAP_ENTRY_PARAMETER({.i = 1}), 
            METAC_FORMAT_BASED_VA_ARG()
        )
    METAC_TAG_MAP_ENTRY_END

    METAC_TAG_MAP_ENTRY(METAC_GSYM_LINK_ENTRY(test_function_with_va_list))
        METAC_TAG_MAP_SET_TAG(0, METAC_TEO_entry, 0, METAC_TAG_MAP_ENTRY_PARAMETER({.n = "format"}),
            METAC_ZERO_ENDED_STRING()
        )
        METAC_TAG_MAP_SET_TAG(0, METAC_TEO_entry, 0, METAC_TAG_MAP_ENTRY_PARAMETER({.n = "vl"}), 
            METAC_FORMAT_BASED_VA_ARG()
        )
    METAC_TAG_MAP_ENTRY_END

    METAC_TAG_MAP_ENTRY(METAC_GSYM_LINK_ENTRY(test_array_len))
        METAC_TAG_MAP_SET_TAG(0, METAC_TEO_entry, 0, METAC_TAG_MAP_ENTRY_PARAMETER({.n = "arr"}),
            METAC_COUNT_BY(len)
        )
    METAC_TAG_MAP_ENTRY_END

METAC_TAG_MAP_END

METAC_START_TEST(va_arg_to_value) {
    metac_tag_map_t *p_tag_map = va_args_tag_map();

    char _c_ = 100;
    short _s_ = 10000;
    int _i_ = 1000000;
    long _l_ = 20000000;
    long long _ll_ = 20000000;

    char b1[32], b2[32];
    snprintf(b1, sizeof(b1), "%p", (void*)0x100);
    snprintf(b2, sizeof(b2), "%p", (void*)0xff00);

#if VA_ARG_IN_VA_ARG != 0
    // special case - test_function_with_va_list
    metac_value_t *p_list_val;  
    WITH_VA_LIST_CONTAINER(c,
        p_list_val = METAC_NEW_VALUE_WITH_ARGS_FOR_FN(p_tag_map, test_function_with_va_list, "%s %s", VA_LIST_FROM_CONTAINER(c, "some", "test"));
    );
#endif

    struct {
        metac_value_t * p_parsed_value;
        metac_num_t expected_sz;
        char ** expected_s; //strings
    }tcs[] = {
        {
            .p_parsed_value = METAC_NEW_VALUE_WITH_ARGS_FOR_FN(p_tag_map, test_function_with_va_args, "%p %p", NULL, NULL),
            .expected_sz = 3,
            .expected_s = (char *[]){
                "(const char []){'%', 'p', ' ', '%', 'p', 0,}",
                "NULL", "NULL"
            },
        },
        {
            .p_parsed_value = METAC_NEW_VALUE_WITH_ARGS_FOR_FN(p_tag_map, test_function_with_va_args, "%p %p", (void*)0x100,(void*)0xff00),
            .expected_sz = 3,
            .expected_s = (char *[]){
                "(const char []){'%', 'p', ' ', '%', 'p', 0,}",
                b1, b2
            },
        },
        {
            .p_parsed_value = METAC_NEW_VALUE_WITH_ARGS_FOR_FN(p_tag_map, test_function_with_va_args, "%c %hhi, %hhd", 'x', 'y', 'z'),
            .expected_sz = 4,
            .expected_s = (char *[]){
                "(const char []){'%', 'c', ' ', '%', 'h', 'h', 'i', ',', ' ', '%', 'h', 'h', 'd', 0,}",
                "'x'", "'y'", "'z'"
            },
        },
        {
            .p_parsed_value = METAC_NEW_VALUE_WITH_ARGS_FOR_FN(p_tag_map, test_function_with_va_args, "%hd %hi", -1500, 1499),
            .expected_sz = 3,
            .expected_s = (char *[]){
                "(const char []){'%', 'h', 'd', ' ', '%', 'h', 'i', 0,}",
                "-1500", "1499"
            },
        },
        {
            .p_parsed_value = METAC_NEW_VALUE_WITH_ARGS_FOR_FN(p_tag_map, test_function_with_va_args, "%d %i", -100000, 1000001),
            .expected_sz = 3,
            .expected_s = (char *[]){
                "(const char []){'%', 'd', ' ', '%', 'i', 0,}",
                "-100000", "1000001"
            },
        },
        {
            .p_parsed_value = METAC_NEW_VALUE_WITH_ARGS_FOR_FN(p_tag_map, test_function_with_va_args, "%ld %li", -2000000L, 2000000L),
            .expected_sz = 3,
            .expected_s = (char *[]){
                "(const char []){'%', 'l', 'd', ' ', '%', 'l', 'i', 0,}",
                "-2000000", "2000000"
            },
        },
        {
            .p_parsed_value = METAC_NEW_VALUE_WITH_ARGS_FOR_FN(p_tag_map, test_function_with_va_args, "%lld %lli", -2000000LL, 2000000LL),
            .expected_sz = 3,
            .expected_s = (char *[]){
                "(const char []){'%', 'l', 'l', 'd', ' ', '%', 'l', 'l', 'i', 0,}",
                "-2000000", "2000000"
            },
        },
        {
            .p_parsed_value = METAC_NEW_VALUE_WITH_ARGS_FOR_FN(p_tag_map, test_function_with_va_args, "%hho, %hhu, %hhx, %hhX", 118, 120, 121, 122),
            .expected_sz = 5,
            .expected_s = (char *[]){
                "(const char []){'%', 'h', 'h', 'o', ',', ' ', '%', 'h', 'h', 'u', ',', ' ', '%', 'h', 'h', 'x', ',', ' ', '%', 'h', 'h', 'X', 0,}",
                "118", "120", "121", "122"
            },
        },
        {
            .p_parsed_value = METAC_NEW_VALUE_WITH_ARGS_FOR_FN(p_tag_map, test_function_with_va_args, "%ho, %hu, %hx, %hX", 11800, 12000, 12100, 12200),
            .expected_sz = 5,
            .expected_s = (char *[]){
                "(const char []){'%', 'h', 'o', ',', ' ', '%', 'h', 'u', ',', ' ', '%', 'h', 'x', ',', ' ', '%', 'h', 'X', 0,}",
                "11800", "12000", "12100", "12200"
            },
        },
        {
            .p_parsed_value = METAC_NEW_VALUE_WITH_ARGS_FOR_FN(p_tag_map, test_function_with_va_args, "%o, %u, %x, %X", 1180000, 1200000, 1210000, 1220000),
            .expected_sz = 5,
            .expected_s = (char *[]){
                "(const char []){'%', 'o', ',', ' ', '%', 'u', ',', ' ', '%', 'x', ',', ' ', '%', 'X', 0,}",
                "1180000", "1200000", "1210000", "1220000"
            },
        },
        {
            .p_parsed_value = METAC_NEW_VALUE_WITH_ARGS_FOR_FN(p_tag_map, test_function_with_va_args, "%lo, %lu, %lx, %lX", 11800000L, 12000000L, 12100000L, 12200000L),
            .expected_sz = 5,
            .expected_s = (char *[]){
                "(const char []){'%', 'l', 'o', ',', ' ', '%', 'l', 'u', ',', ' ', '%', 'l', 'x', ',', ' ', '%', 'l', 'X', 0,}",
                "11800000", "12000000", "12100000", "12200000"
            },
        },
        {
            .p_parsed_value = METAC_NEW_VALUE_WITH_ARGS_FOR_FN(p_tag_map, test_function_with_va_args, "%llo, %llu, %llx, %llX", 11800000, 12000000, 12100000, 12200000),
            .expected_sz = 5,
            .expected_s = (char *[]){
                "(const char []){'%', 'l', 'l', 'o', ',', ' ', '%', 'l', 'l', 'u', ',', ' ', '%', 'l', 'l', 'x', ',', ' ', '%', 'l', 'l', 'X', 0,}",
                "11800000", "12000000", "12100000", "12200000"
            },
        },
        {
            .p_parsed_value = METAC_NEW_VALUE_WITH_ARGS_FOR_FN(p_tag_map, test_function_with_va_args, "%f, %g, %e", 11.1, 11.2, -11.3),
            .expected_sz = 4,
            .expected_s = (char *[]){
                "(const char []){'%', 'f', ',', ' ', '%', 'g', ',', ' ', '%', 'e', 0,}",
                "11.100000", "11.200000", "-11.300000"
            },
        },
        {
            .p_parsed_value = METAC_NEW_VALUE_WITH_ARGS_FOR_FN(p_tag_map, test_function_with_va_args, "%Lf, %Lg, %Le", 11.1L, 11.2L, -11.3L),
            .expected_sz = 4,
            .expected_s = (char *[]){
                "(const char []){'%', 'L', 'f', ',', ' ', '%', 'L', 'g', ',', ' ', '%', 'L', 'e', 0,}",
                "11.100000", "11.200000", "-11.300000"
            },
        },
        {
            .p_parsed_value = METAC_NEW_VALUE_WITH_ARGS_FOR_FN(p_tag_map, test_function_with_va_args, "%hhn, %hn, %n, %ln, %lln", &_c_, &_s_, &_i_, &_l_, &_ll_),
            .expected_sz = 6,
            .expected_s = (char *[]){
                "(const char []){'%', 'h', 'h', 'n', ',', ' ', '%', 'h', 'n', ',', ' ', '%', 'n', ',', ' ', '%', 'l', 'n', ',', ' ', '%', 'l', 'l', 'n', 0,}",
                "(char []){'d',}", "(short int []){10000,}", "(int []){1000000,}", "(long int []){20000000,}", "(long long int []){20000000,}"
            },
        },
        {
            .p_parsed_value = METAC_NEW_VALUE_WITH_ARGS_FOR_FN(p_tag_map, test_function_with_va_args, "%s %s", "some", "test"),
            .expected_sz = 3,
            .expected_s = (char *[]){
                "(const char []){'%', 's', ' ', '%', 's', 0,}",
                "{'s', 'o', 'm', 'e', 0,}", "{'t', 'e', 's', 't', 0,}"
            },
        },
        {
            .p_parsed_value = METAC_NEW_VALUE_WITH_ARGS_FOR_FN(p_tag_map, test_function_with_va_args, "%s %s", NULL, NULL),
            .expected_sz = 3,
            .expected_s = (char *[]){
                "(const char []){'%', 's', ' ', '%', 's', 0,}",
                "NULL", "NULL"
            },
        },
#if VA_ARG_IN_VA_ARG != 0
        {
            .p_parsed_value = p_list_val,
            .expected_sz = 3,
            .expected_s = (char *[]){
                "(const char []){'%', 's', ' ', '%', 's', 0,}",
                "{'s', 'o', 'm', 'e', 0,}", "{'t', 'e', 's', 't', 0,}",
            },
        },
#endif
    };

    for (int tc_inx = 0; tc_inx < sizeof(tcs)/sizeof(tcs[0]); tc_inx++) {
        metac_num_t i = 0;
        for (int arg_id = 0; arg_id < metac_value_parameter_count(tcs[tc_inx].p_parsed_value); ++arg_id) {
            metac_value_t * va_arg_parsed = metac_value_parameter_new_item(tcs[tc_inx].p_parsed_value, arg_id);
            fail_unless(va_arg_parsed != NULL, "tc %d.%d, va_arg_parsed is null", tc_inx, i);
            if (metac_value_has_parameter_load(va_arg_parsed) == 0) { //simple parameter
                fail_unless(i < tcs[tc_inx].expected_sz, "tc %d: counter %d is bigger than expected %d", tc_inx, i, tcs[tc_inx].expected_sz);
                char *s = metac_value_string_ex(va_arg_parsed, METAC_WMODE_deep, p_tag_map);
                fail_unless(s != NULL);
                fail_unless(strcmp(tcs[tc_inx].expected_s[i], s) == 0, "tc %d.%d, expected %s, got %s",
                    tc_inx, i, tcs[tc_inx].expected_s[i], s);
                free(s);
                ++i;
            } else {
                for (int sub_arg_id = 0;  sub_arg_id < metac_value_parameter_count(va_arg_parsed); ++sub_arg_id) {
                    metac_value_t * va_subarg_parsed = metac_value_parameter_new_item(va_arg_parsed, sub_arg_id);
                    fail_unless(va_subarg_parsed != NULL, "tc %d.%d, va_subarg_parsed is null", tc_inx, i);

                    fail_unless(metac_value_has_parameter_load(va_subarg_parsed) == 0);

                    fail_unless(i < tcs[tc_inx].expected_sz, "tc %d: counter %d is bigger than expected %d", tc_inx, i, tcs[tc_inx].expected_sz);
                    char *s = metac_value_string_ex(va_subarg_parsed, METAC_WMODE_deep, p_tag_map);
                    metac_value_delete(va_subarg_parsed);
                    fail_unless(s != NULL);
                    fail_unless(strcmp(tcs[tc_inx].expected_s[i], s) == 0, "tc %d.%d, expected %s, got %s",
                        tc_inx, i, tcs[tc_inx].expected_s[i], s);
                    free(s);
                    ++i;
                }
            }
            metac_value_delete(va_arg_parsed);
        }

        METAC_VALUE_WITH_ARGS_DELETE(tcs[tc_inx].p_parsed_value);
    }

    metac_tag_map_delete(p_tag_map);
}END_TEST

METAC_START_TEST(va_arg_sanity) {

    metac_value_t * p_val;
    char *s, *expected_s;
    metac_tag_map_t * p_tagmap = va_args_tag_map();

    p_val = METAC_NEW_VALUE_WITH_ARGS_FOR_FN(p_tagmap, test_function_with_va_args, "%s %s", "some", "test");
    fail_unless(p_val != NULL);

    expected_s = "test_function_with_va_args("
        "(const char []){'%', 's', ' ', '%', 's', 0,}, "
        "(char [5]){'s', 'o', 'm', 'e', 0,}, "
        "(char [5]){'t', 'e', 's', 't', 0,})";
    s  = metac_value_string_ex(p_val, METAC_WMODE_deep, p_tagmap);
    fail_unless(s != NULL, "got NULL");
    fail_unless(strcmp(s, expected_s) == 0, "expected %s, got %s", expected_s, s);
    free(s);

    METAC_VALUE_WITH_ARGS_DELETE(p_val);

    metac_tag_map_delete(p_tagmap);
}END_TEST

#if VA_ARG_IN_VA_ARG != 0

METAC_START_TEST(va_list_sanity) {
    metac_value_t * p_val;
    char *s, *expected_s;
    metac_tag_map_t * p_tagmap = va_args_tag_map();
#define VA_LIST(_args_...) VA_LIST_FROM_CONTAINER(c, _args_)

    WITH_VA_LIST_CONTAINER(c, 
        p_val = METAC_NEW_VALUE_WITH_ARGS_FOR_FN(p_tagmap, test_function_with_va_list, "%s %s", VA_LIST("some", "test"));
    );
    fail_unless(p_val != NULL);

    expected_s = "test_function_with_va_list("
        "(const char []){'%', 's', ' ', '%', 's', 0,}, "
        "VA_LIST((char [5]){'s', 'o', 'm', 'e', 0,}, (char [5]){'t', 'e', 's', 't', 0,}))";
    s  = metac_value_string_ex(p_val, METAC_WMODE_deep, p_tagmap);
    fail_unless(s != NULL, "got NULL");
    fail_unless(strcmp(s, expected_s) == 0, "expected %s, got %s", expected_s, s);
    free(s);

    METAC_VALUE_WITH_ARGS_DELETE(p_val);

    WITH_VA_LIST_CONTAINER(c, 
        p_val = METAC_NEW_VALUE_WITH_ARGS_FOR_FN(p_tagmap, test_function_with_va_list, "%d %ld", VA_LIST((int)5, (long int)-100));
    );
    fail_unless(p_val != NULL);

    expected_s = "test_function_with_va_list("
        "(const char []){'%', 'd', ' ', '%', 'l', 'd', 0,}, "
        "VA_LIST((int)5, (long int)-100))";
    s  = metac_value_string_ex(p_val, METAC_WMODE_deep, p_tagmap);
    fail_unless(s != NULL, "got NULL");
    fail_unless(strcmp(s, expected_s) == 0, "expected %s, got %s", expected_s, s);
    free(s);

    METAC_VALUE_WITH_ARGS_DELETE(p_val);

    WITH_VA_LIST_CONTAINER(c, 
        p_val = METAC_NEW_VALUE_WITH_ARGS_FOR_FN(p_tagmap, test_function_with_va_list, "%p", VA_LIST((void *)NULL));
    );
    fail_unless(p_val != NULL);

    expected_s = "test_function_with_va_list("
        "(const char []){'%', 'p', 0,}, "
        "VA_LIST((void *)NULL))";
    s  = metac_value_string_ex(p_val, METAC_WMODE_deep, p_tagmap);
    fail_unless(s != NULL, "got NULL");
    fail_unless(strcmp(s, expected_s) == 0, "expected %s, got %s", expected_s, s);
    free(s);

    METAC_VALUE_WITH_ARGS_DELETE(p_val);

#undef VA_LIST
    metac_tag_map_delete(p_tagmap);
}END_TEST

#endif

METAC_START_TEST(array_len_sanity) {
    metac_value_t * p_val;
    char *s, *expected_s;
    metac_tag_map_t * p_tagmap = va_args_tag_map();

    int * test_arr1 = (int[]){0, 1, 2, 3};
    p_val = METAC_NEW_VALUE_WITH_ARGS_FOR_FN(p_tagmap, test_array_len, test_arr1, 4);
    fail_unless(p_val != NULL);

    expected_s = "test_array_len((int []){0, 1, 2, 3,}, 4)";
    s  = metac_value_string_ex(p_val, METAC_WMODE_deep, p_tagmap);
    fail_unless(s != NULL, "got NULL");
    fail_unless(strcmp(s, expected_s) == 0, "expected %s, got %s", expected_s, s);
    free(s);

    METAC_VALUE_WITH_ARGS_DELETE(p_val);


    int test_arr2[] = {0, 1, 2, 3, 4};
    p_val = METAC_NEW_VALUE_WITH_ARGS_FOR_FN(p_tagmap, test_array_len, test_arr2, sizeof(test_arr2)/sizeof(test_arr2[0]));
    fail_unless(p_val != NULL);

    expected_s = "test_array_len((int []){0, 1, 2, 3, 4,}, 5)";
    s  = metac_value_string_ex(p_val, METAC_WMODE_deep, p_tagmap);
    fail_unless(s != NULL, "got NULL");
    fail_unless(strcmp(s, expected_s) == 0, "expected %s, got %s", expected_s, s);
    free(s);

    METAC_VALUE_WITH_ARGS_DELETE(p_val);


    metac_tag_map_delete(p_tagmap);

}END_TEST

// function pointer tests
#undef METAC_NEW_VALUE_WITH_ARGS_PTR
#define METAC_NEW_VALUE_WITH_ARGS_PTR(_p_tag_map_, _type_, _val_, _args_...) ({ \
        metac_parameter_storage_t * p_param_storage = metac_new_parameter_storage(); \
        WITH_METAC_DECLLOC(dec, _type_ * p = _val_); \
        metac_entry_t * p_entry = METAC_ENTRY_FROM_DECLLOC(dec, p); \
        metac_value_t * p_res = NULL; \
        if (p_entry != NULL && p_param_storage != NULL) { \
            p_res = metac_new_value_with_parameters(p_param_storage, _p_tag_map_, metac_entry_pointer_entry(p_entry), _args_); \
        } \
        p_res; \
    })

METAC_START_TEST(base_type_args_to_value_ptr) {
    char *s, *expected_s;
    metac_tag_map_t * p_tagmap = NULL;

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

    p_val = METAC_NEW_VALUE_WITH_ARGS_PTR(
        NULL,
        test_function_with_base_args_t,
        &test_function_with_base_args,
        arg_00, arg_01, arg_02, arg_03, arg_04,
        arg_05, arg_06, arg_07, arg_08, arg_09, 
        arg_10, arg_11, arg_12, arg_13, arg_14,
        arg_15, arg_16);
    fail_unless(p_val != NULL, "failed to collect args of test_function_with_base_args");

    expected_s = "(test_function_with_base_args_t)(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, true, 11.000000, 12.000000, 13.000000, 14.000000 - I * 14.000000, 15.000000 - I * 15.000000, 16.000000 + I * 116.000000)";
    s  = metac_value_string_ex(p_val, METAC_WMODE_deep, p_tagmap);
    fail_unless(s != NULL, "got NULL");
    fail_unless(strcmp(s, expected_s) == 0, "expected %s, got %s", expected_s, s);
    free(s);

    METAC_VALUE_WITH_ARGS_DELETE(p_val);

    p_val = METAC_NEW_VALUE_WITH_ARGS_PTR(
        NULL, 
        test_function_with_base_args_ptr_t,
        &test_function_with_base_args_ptr,
        &arg_00, &arg_01, &arg_02, &arg_03, &arg_04,
        &arg_05, &arg_06, &arg_07, &arg_08, &arg_09, 
        &arg_10, &arg_11, &arg_12, &arg_13, &arg_14,
        &arg_15, &arg_16);
    fail_unless(p_val != NULL, "failed to collect args of test_function_with_base_args");
    // types are diffenent on differnt platforms (some don't have actual long double complex) - we can't
    // test sting
    METAC_VALUE_WITH_ARGS_DELETE(p_val);
}END_TEST

METAC_START_TEST(args_deep_compare_sanity) {
    metac_tag_map_t * p_tagmap = va_args_tag_map();
    int eq;
    metac_value_t * p_val1, *p_val2;

    int * test_arr1 = (int[]){0, 1, 2, 3};
    int * test_arr2 = (int[]){0, 1, 2, 3};
    p_val1 = METAC_NEW_VALUE_WITH_ARGS_FOR_FN(p_tagmap, test_array_len, test_arr1, 4);
    fail_unless(p_val1 != NULL);
    p_val2 = METAC_NEW_VALUE_WITH_ARGS_FOR_FN(p_tagmap, test_array_len, test_arr2, 4);
    fail_unless(p_val2 != NULL);

    eq = metac_value_equal_ex(p_val1, p_val2, NULL, p_tagmap);
    fail_unless(eq == 1, "expected values equal, got %d", eq);

    METAC_VALUE_WITH_ARGS_DELETE(p_val2);
    
    // changing arg
    test_arr2[0] = 1;
    p_val2 = METAC_NEW_VALUE_WITH_ARGS_FOR_FN(p_tagmap, test_array_len, test_arr2, 4);
    fail_unless(p_val2 != NULL);

    eq = metac_value_equal_ex(p_val1, p_val2, NULL, p_tagmap);
    fail_unless(eq == 0, "expected values NON equal, got %d", eq);

    METAC_VALUE_WITH_ARGS_DELETE(p_val2);

    METAC_VALUE_WITH_ARGS_DELETE(p_val1);

    metac_tag_map_delete(p_tagmap);
}END_TEST


METAC_START_TEST(args_deep_copy_and_delete_sanity) {
    metac_tag_map_t * p_tagmap = va_args_tag_map();
    metac_value_t * p_val1, *p_val2;
    metac_parameter_storage_t * p_param_storage;
    char *s, *expected_s;

    int * test_arr1 = (int[]){0, 1, 2, 3};
    p_val1 = METAC_NEW_VALUE_WITH_ARGS_FOR_FN(p_tagmap, test_array_len, test_arr1, 4);
    fail_unless(p_val1 != NULL);

    expected_s = "test_array_len((int []){0, 1, 2, 3,}, 4)";
    s = metac_value_string_ex(p_val1, METAC_WMODE_deep, p_tagmap);
    fail_unless(s != NULL);
    fail_unless(strcmp(s, expected_s) == 0, "got %s, expected %s", s, expected_s);
    free(s);

    p_param_storage = metac_new_parameter_storage();
    p_val2 = metac_new_value(METAC_GSYM_LINK_ENTRY(test_array_len), p_param_storage);

    fail_unless(metac_value_copy_ex(p_val1, p_val2, NULL, NULL, NULL, p_tagmap) == p_val2);

    s = metac_value_string_ex(p_val2, METAC_WMODE_deep, p_tagmap);
    fail_unless(s != NULL);
    fail_unless(strcmp(s, expected_s) == 0, "got %s, expected %s", s, expected_s);
    free(s);

    test_arr1[0] = 1;

    // the ideat is that the change in the p_val args won't affect the string, because all artguments are copied with deep-copy
    s = metac_value_string_ex(p_val2, METAC_WMODE_deep, p_tagmap);
    fail_unless(s != NULL);
    fail_unless(strcmp(s, expected_s) == 0, "got %s, expected %s", s, expected_s);
    free(s);

    // we need to cleanup allocated by deep copy memory
    fail_unless(metac_value_free_ex(p_val2, NULL, NULL, p_tagmap) == 1);

    METAC_VALUE_WITH_ARGS_DELETE(p_val2);

    METAC_VALUE_WITH_ARGS_DELETE(p_val1);

    metac_tag_map_delete(p_tagmap);
}END_TEST
