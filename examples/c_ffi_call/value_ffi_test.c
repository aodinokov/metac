#include "metac/test.h"

#include "metac/reflect.h"

#include "value_ffi.c"

#include "metac/backend/va_list_ex.h"

// const arg count tests data
static char called[1024];

#define _CALL_PROCESS_FN(_tag_map_, _fn_, _args_...) { \
    called[0] = 0; \
    metac_entry_t * p_entry = METAC_GSYM_LINK_ENTRY(_fn_); \
    metac_value_t * p_params_val = METAC_NEW_VALUE_WITH_CALL_PARAMS_AND_WRAP(_tag_map_, p_entry, _fn_, _args_); \
    metac_value_t *p_res_val = metac_new_value_with_call_result(p_entry); \
    int res = metac_value_call(p_params_val, (void (*)(void)) _fn_, p_res_val);

#define _CALL_PROCESS_FN_PTR(_tag_map_, _type_, _fn_, _args_...) { \
    called[0] = 0; \
    WITH_METAC_DECLLOC(dec, _type_ * p = _fn_); \
    metac_entry_t * p_entry = METAC_ENTRY_FROM_DECLLOC(dec, p); \
    metac_value_t * p_params_val = METAC_NEW_VALUE_WITH_CALL_PARAMS_AND_WRAP(_tag_map_, metac_entry_pointer_entry(p_entry), _fn_, _args_); \
    metac_value_t *p_res_val = metac_new_value_with_call_result(metac_entry_pointer_entry(p_entry)); \
    int res = metac_value_call(p_params_val, (void (*)(void)) p, p_res_val);


#define _CALL_PROCESS_END \
        metac_value_with_call_result_delete(p_res_val); \
        metac_value_with_call_params_delete(p_params_val); \
    }

// test base types
int test_function_with_base_args(
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
    snprintf(called, sizeof(called),
        "test_function_with_base_args %hhi %hhu %hi %hu %i %u %li %lu %lli %llu %s %f %f %LF %f %f %f %f %f %f",
        arg_00, arg_01,
        arg_02, arg_03,
        arg_04, arg_05,
        arg_06, arg_07,
        arg_08, arg_09,
        arg_10?"true":"false", // bool
        arg_11, arg_12, arg_13,
        creal(arg_14), cimag(arg_14),
        creal(arg_15), cimag(arg_15),
        creal(arg_16), cimag(arg_16));
    return 1;
}
METAC_GSYM_LINK(test_function_with_base_args);
typedef int test_function_with_base_args_t(
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

int * test_function_with_base_args_ptr(char * arg_00,
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

    snprintf(called, sizeof(called),
        "test_function_with_base_args_ptr %hhi %hhu %hi %hu %i %u %li %lu %lli %llu %s %f %f %LF %f %f %f %f %f %f",
        *arg_00, *arg_01,
        *arg_02, *arg_03,
        *arg_04, *arg_05,
        *arg_06, *arg_07,
        *arg_08, *arg_09,
        (*arg_10)?"true":"false", // bool
        *arg_11, *arg_12, *arg_13,
        creal(*arg_14), cimag(*arg_14),
        creal(*arg_15), cimag(*arg_15),
        creal(*arg_16), cimag(*arg_16));

    return arg_04;
}
METAC_GSYM_LINK(test_function_with_base_args_ptr);
typedef int * test_function_with_base_args_ptr_t(char * arg_00,
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

METAC_START_TEST(test_ffi_base_type) {
    char * s = NULL;
    char * expected = NULL;
    char * expected_called = NULL;

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

    _CALL_PROCESS_FN(NULL, test_function_with_base_args,
        arg_00, arg_01, arg_02, arg_03, arg_04,
        arg_05, arg_06, arg_07, arg_08, arg_09, 
        arg_10, arg_11, arg_12, arg_13, arg_14,
        arg_15, arg_16)
        fail_unless(res == 0, "Call wasn't successful, expected successful");

        expected_called = "test_function_with_base_args 0 1 2 3 4 5 6 7 8 9 true 11.000000 12.000000 13.000000 14.000000 -14.000000 15.000000 -15.000000 16.000000 116.000000";
        fail_unless(strcmp(called, expected_called) == 0, "called: got %s, expected %s", called, expected_called);

        expected = "1";
        s = metac_value_string_ex(p_res_val, METAC_WMODE_deep, NULL);
        fail_unless(s != NULL);
        fail_unless(strcmp(s, expected) == 0, "got %s, expected %s", s, expected);
        free(s);

    _CALL_PROCESS_END

    _CALL_PROCESS_FN_PTR(NULL, test_function_with_base_args_t, test_function_with_base_args,
        arg_00, arg_01, arg_02, arg_03, arg_04,
        arg_05, arg_06, arg_07, arg_08, arg_09, 
        arg_10, arg_11, arg_12, arg_13, arg_14,
        arg_15, arg_16)
        fail_unless(res == 0, "Call wasn't successful, expected successful");

        expected_called = "test_function_with_base_args 0 1 2 3 4 5 6 7 8 9 true 11.000000 12.000000 13.000000 14.000000 -14.000000 15.000000 -15.000000 16.000000 116.000000";
        fail_unless(strcmp(called, expected_called) == 0, "called: got %s, expected %s", called, expected_called);

        expected = "1";
        s = metac_value_string_ex(p_res_val, METAC_WMODE_deep, NULL);
        fail_unless(s != NULL);
        fail_unless(strcmp(s, expected) == 0, "got %s, expected %s", s, expected);
        free(s);

    _CALL_PROCESS_END


    _CALL_PROCESS_FN(NULL, test_function_with_base_args_ptr,
        &arg_00, &arg_01, &arg_02, &arg_03, &arg_04,
        &arg_05, &arg_06, &arg_07, &arg_08, &arg_09, 
        &arg_10, &arg_11, &arg_12, &arg_13, &arg_14,
        &arg_15, &arg_16)
        fail_unless(res == 0, "Call wasn't successful, expected successful");

        expected_called = "test_function_with_base_args_ptr 0 1 2 3 4 5 6 7 8 9 true 11.000000 12.000000 13.000000 14.000000 -14.000000 15.000000 -15.000000 16.000000 116.000000";
        fail_unless(strcmp(called, expected_called) == 0, "called: got %s, expected %s", called, expected_called);

        expected = "(int []){4,}"; // int * is 4th arguments - it's its value
        s = metac_value_string_ex(p_res_val, METAC_WMODE_deep, NULL);
        fail_unless(s != NULL);
        fail_unless(strcmp(s, expected) == 0, "got %s, expected %s", s, expected);
        free(s);

    _CALL_PROCESS_END

    _CALL_PROCESS_FN_PTR(NULL, test_function_with_base_args_ptr_t, test_function_with_base_args_ptr,
        &arg_00, &arg_01, &arg_02, &arg_03, &arg_04,
        &arg_05, &arg_06, &arg_07, &arg_08, &arg_09, 
        &arg_10, &arg_11, &arg_12, &arg_13, &arg_14,
        &arg_15, &arg_16)
        fail_unless(res == 0, "Call wasn't successful, expected successful");

        expected_called = "test_function_with_base_args_ptr 0 1 2 3 4 5 6 7 8 9 true 11.000000 12.000000 13.000000 14.000000 -14.000000 15.000000 -15.000000 16.000000 116.000000";
        fail_unless(strcmp(called, expected_called) == 0, "called: got %s, expected %s", called, expected_called);

        expected = "(int []){4,}"; // int * is 4th arguments - it's its value
        s = metac_value_string_ex(p_res_val, METAC_WMODE_deep, NULL);
        fail_unless(s != NULL);
        fail_unless(strcmp(s, expected) == 0, "got %s, expected %s", s, expected);
        free(s);

    _CALL_PROCESS_END
}END_TEST

// test enums
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

enum test_enum_04 test_function_with_enum_args(
    enum test_enum_01 arg_00,
    enum test_enum_02 arg_01,
    enum test_enum_04 arg_02,
    test_enum_08_t arg_03) {
    snprintf(called, sizeof(called),
        "test_function_with_enum_args %hhx %hx %x %lx", 
            arg_00, arg_01, arg_02, arg_03);
    return arg_02;
}
METAC_GSYM_LINK(test_function_with_enum_args);

enum test_enum_04 * test_function_with_enum_args_ptr(
    enum test_enum_01 * arg_00,
    enum test_enum_02 * arg_01,
    enum test_enum_04 * arg_02,
    test_enum_08_t * arg_03) {
    snprintf(called, sizeof(called),
        "test_function_with_enum_args_ptr %hhx %hx %x %lx", 
            *arg_00, *arg_01, *arg_02, *arg_03);
    return arg_02;
}
METAC_GSYM_LINK(test_function_with_enum_args_ptr);

METAC_START_TEST(test_ffi_enum_type) {
    char * s = NULL;
    char * expected = NULL;
    char * expected_called = NULL;
    char calling[256];

    enum test_enum_01 arg_00 = e01_end;
    enum test_enum_02 arg_01 = e02_end;
    enum test_enum_04 arg_02 = e04_end;
    enum test_enum_08 arg_03 = e08_end;

    _CALL_PROCESS_FN(NULL, test_function_with_enum_args,
        arg_00, arg_01, arg_02, arg_03)
        fail_unless(res == 0, "Call wasn't successful, expected successful");

        snprintf(calling, sizeof(calling),
            "test_function_with_enum_args %hhx %hx %x %lx", 
            arg_00, arg_01, arg_02, arg_03);
        expected_called = calling;
        fail_unless(strcmp(called, expected_called) == 0, "called: got %s, expected %s", called, expected_called);

        expected = "e04_end";
        s = metac_value_string_ex(p_res_val, METAC_WMODE_deep, NULL);
        fail_unless(s != NULL);
        fail_unless(strcmp(s, expected) == 0, "got %s, expected %s", s, expected);
        free(s);

    _CALL_PROCESS_END

    _CALL_PROCESS_FN(NULL, test_function_with_enum_args_ptr,
        &arg_00, &arg_01, &arg_02, &arg_03)
        fail_unless(res == 0, "Call wasn't successful, expected successful");

        snprintf(calling, sizeof(calling),
            "test_function_with_enum_args_ptr %hhx %hx %x %lx", 
            arg_00, arg_01, arg_02, arg_03);
        expected_called = calling;
        fail_unless(strcmp(called, expected_called) == 0, "called: got %s, expected %s", called, expected_called);

        expected = "(enum test_enum_04 []){e04_end,}";
        s = metac_value_string_ex(p_res_val, METAC_WMODE_deep, NULL);
        fail_unless(s != NULL);
        fail_unless(strcmp(s, expected) == 0, "got %s, expected %s", s, expected);
        free(s);

    _CALL_PROCESS_END
}END_TEST

// test struct
typedef struct {
    short a;
    int arr[30];
} test_struct_t;

test_struct_t test_function_with_struct_args(
    test_struct_t arg_00,
    test_struct_t * arg_01,
    test_struct_t ** arg_02) {
    snprintf(called, sizeof(called),
        "test_function_with_struct_args %hi %i %i, %hi %i %i, %hi %i %i",
            arg_00.a, arg_00.arr[0], arg_00.arr[29],
            arg_01->a, arg_01->arr[0], arg_01->arr[29],
            (*arg_02)->a, (*arg_02)->arr[0], (*arg_02)->arr[29]);
    return arg_00;
}
METAC_GSYM_LINK(test_function_with_struct_args);

METAC_START_TEST(test_ffi_struct_type) {
    char * s = NULL;
    char * expected = NULL;
    char * expected_called = NULL;

    test_struct_t arg_00 = {.a = 1, .arr = {0,}};
    test_struct_t * arg_01 = &arg_00;
    test_struct_t ** arg_02 = &arg_01;
    for (int i=0; i < sizeof(arg_00.arr)/sizeof(arg_00.arr[0]); ++i){
        arg_00.arr[i] = i;
    }
    arg_00.a = sizeof(arg_00.arr)/sizeof(arg_00.arr[0]);

    _CALL_PROCESS_FN(NULL, test_function_with_struct_args,
        arg_00, arg_01, arg_02)
        fail_unless(res == 0, "Call wasn't successful, expected successful");

        expected_called = "test_function_with_struct_args 30 0 29, 30 0 29, 30 0 29";
        fail_unless(strcmp(called, expected_called) == 0, "called: got %s, expected %s", called, expected_called);

        expected = "{.a = 30, .arr = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29,},}";
        s = metac_value_string_ex(p_res_val, METAC_WMODE_deep, NULL);
        fail_unless(s != NULL);
        fail_unless(strcmp(s, expected) == 0, "got %s, expected %s", s, expected);
        free(s);

    _CALL_PROCESS_END
}END_TEST

// test the same but with aligned member
typedef struct {
    short a;
    __attribute__((aligned(32))) int arr[10];
} test_aligned_memb_struct_t;

test_aligned_memb_struct_t test_function_with_aligned_memb_struct_args(
    test_aligned_memb_struct_t arg_00,
    test_aligned_memb_struct_t * arg_01,
    test_aligned_memb_struct_t ** arg_02) {
    snprintf(called, sizeof(called),
        "test_function_with_aligned_memb_struct_args %hi %i %i, %hi %i %i, %hi %i %i",
            arg_00.a, arg_00.arr[0], arg_00.arr[1],
            arg_01->a, arg_01->arr[0], arg_01->arr[1],
            (*arg_02)->a, (*arg_02)->arr[0], (*arg_02)->arr[1]);
    return arg_00;
}
METAC_GSYM_LINK(test_function_with_aligned_memb_struct_args);

METAC_START_TEST(test_ffi_aligned_memb_struct_type) {
    char * s = NULL;
    char * expected = NULL;
    char * expected_called = NULL;

    fail_unless(sizeof(test_aligned_memb_struct_t) != sizeof(test_struct_t), "size must be different");

    test_aligned_memb_struct_t arg_00 = {.a = 1, .arr = {0,}};
    test_aligned_memb_struct_t * arg_01 = &arg_00;
    test_aligned_memb_struct_t ** arg_02 = &arg_01;
    for (int i=0; i < sizeof(arg_00.arr)/sizeof(arg_00.arr[0]); ++i){
        arg_00.arr[i] = i;
    }
    arg_00.a = sizeof(arg_00.arr)/sizeof(arg_00.arr[0]);

    _CALL_PROCESS_FN(NULL, test_function_with_aligned_memb_struct_args,
        arg_00, arg_01, arg_02)
        fail_unless(res == 0, "Call wasn't successful, expected successful");

        expected_called = "test_function_with_aligned_memb_struct_args 10 0 1, 10 0 1, 10 0 1";
        fail_unless(strcmp(called, expected_called) == 0, "called: got %s, expected %s", called, expected_called);

        expected = "{.a = 10, .arr = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9,},}";
        s = metac_value_string_ex(p_res_val, METAC_WMODE_deep, NULL);
        fail_unless(s != NULL);
        fail_unless(strcmp(s, expected) == 0, "got %s, expected %s", s, expected);
        free(s);

    _CALL_PROCESS_END
}END_TEST

// test the same but with aligned struct and put inside array
typedef struct {
    short a;
    int arr[10];
} __attribute__((aligned(32))) test_aligned_struct_t;
typedef test_aligned_struct_t test_1d_array_t[15];
typedef int test_2d_array_t[3][5];

test_1d_array_t * test_function_with_array_args(
    test_1d_array_t arg_00,
    test_1d_array_t * arg_01,
    test_1d_array_t ** arg_02,
    test_2d_array_t arg_03,
    test_2d_array_t * arg_04,
    test_2d_array_t ** arg_05) {
    snprintf(called, sizeof(called),
        "test_function_with_array_args %d %d %d %d %d %d %d %d, %d %d %d %d %d %d",
            (int)arg_00[0].a, (int)arg_00[0].arr[0], (int)arg_00[0].arr[1], (int)arg_00[0].arr[9],
            (int)arg_00[14].a, (int)arg_00[14].arr[0], (int)arg_00[14].arr[1], (int)arg_00[14].arr[9],
            arg_03[0][0], arg_03[0][1], arg_03[0][2],
            arg_03[2][2], arg_03[2][3], arg_03[2][4]);
    return arg_01;
}
METAC_GSYM_LINK(test_function_with_array_args);

METAC_START_TEST(test_function_with_array) {
    char * s = NULL;
    char * expected = NULL;
    char * expected_called = NULL;

    test_1d_array_t arg_00 = {{.a = 0, .arr = {0,}}, {.a = 0, .arr = {0,}}, };
    test_1d_array_t * arg_01 = &arg_00;
    test_1d_array_t ** arg_02 = &arg_01;
    test_2d_array_t arg_03 = {{1,2,3,4,5}, {6,7,8,9,10}, {11,12,13,14,15}};
    test_2d_array_t * arg_04 = &arg_03;
    test_2d_array_t ** arg_05 = &arg_04;

    for (int i=0; i < sizeof(arg_00)/sizeof(arg_00[0]); ++i){
        arg_00[i].a = i;
        for (int j=0; j< sizeof(arg_00[i].arr)/sizeof(arg_00[i].arr[0]); ++j) {
            arg_00[i].arr[j] = i+j;
        }
    }

    _CALL_PROCESS_FN(NULL, test_function_with_array_args,
        arg_00, arg_01, arg_02, arg_03, arg_04, arg_05)
        fail_unless(res == 0, "Call wasn't successful, expected successful");

        expected_called = "test_function_with_array_args 0 0 1 9 14 14 15 23, 1 2 3 13 14 15";
        fail_unless(strcmp(called, expected_called) == 0, "called: got %s, expected %s", called, expected_called);

        expected = "(test_1d_array_t []){{"
            "{.a = 0, .arr = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9,},}, "
            "{.a = 1, .arr = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10,},}, "
            "{.a = 2, .arr = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11,},}, "
            "{.a = 3, .arr = {3, 4, 5, 6, 7, 8, 9, 10, 11, 12,},}, "
            "{.a = 4, .arr = {4, 5, 6, 7, 8, 9, 10, 11, 12, 13,},}, "
            "{.a = 5, .arr = {5, 6, 7, 8, 9, 10, 11, 12, 13, 14,},}, "
            "{.a = 6, .arr = {6, 7, 8, 9, 10, 11, 12, 13, 14, 15,},}, "
            "{.a = 7, .arr = {7, 8, 9, 10, 11, 12, 13, 14, 15, 16,},}, "
            "{.a = 8, .arr = {8, 9, 10, 11, 12, 13, 14, 15, 16, 17,},}, "
            "{.a = 9, .arr = {9, 10, 11, 12, 13, 14, 15, 16, 17, 18,},}, "
            "{.a = 10, .arr = {10, 11, 12, 13, 14, 15, 16, 17, 18, 19,},}, "
            "{.a = 11, .arr = {11, 12, 13, 14, 15, 16, 17, 18, 19, 20,},}, "
            "{.a = 12, .arr = {12, 13, 14, 15, 16, 17, 18, 19, 20, 21,},}, "
            "{.a = 13, .arr = {13, 14, 15, 16, 17, 18, 19, 20, 21, 22,},}, "
            "{.a = 14, .arr = {14, 15, 16, 17, 18, 19, 20, 21, 22, 23,},},"
        "},}";
        s = metac_value_string_ex(p_res_val, METAC_WMODE_deep, NULL);
        fail_unless(s != NULL);
        fail_unless(strcmp(s, expected) == 0, "got %s, expected %s", s, expected);
        free(s);

    _CALL_PROCESS_END
}END_TEST

//unions hierarchy
typedef union {
    union {
        int a_int;
        char a_char;
    } a;
    union {
        long b_long;
        char b_char;
    } b;
}test_union_hierarchy_t;

//stucts with unions
typedef struct {
    union {
        int a_int;
        char a_char;
    } a;
    union {
        long b_long;
        char b_char;
    } b;
}test_struct_with_union_t;

// struct with bitfields
typedef struct {
    long lng01:5;
    long lng02:18;
    long lng03:5;
    long :0;// next long 
    long lng11:7;
    long lng12:16;
    long lng13:30;
}test_struct_with_bitfields_t;

// flexible arrays
typedef struct {
    int len;
    int arr[]; 
}test_struct_with_flexarr_t;

test_struct_with_bitfields_t test_function_with_extra_cases(
    test_union_hierarchy_t arg_00,
    test_struct_with_union_t arg_01,
    test_struct_with_bitfields_t arg_02,
    test_struct_with_flexarr_t arg_03) {
    WITH_METAC_DECLLOC(decl,
        test_struct_with_bitfields_t * p_arg_02 = &arg_02;
        test_struct_with_flexarr_t * p_arg_03 = &arg_03
    );
    metac_value_t
        * p_v_02 = METAC_VALUE_FROM_DECLLOC(decl, p_arg_02),
        * p_v_03 = METAC_VALUE_FROM_DECLLOC(decl, p_arg_03);
    fail_unless(
        p_v_02 != NULL &&
        p_v_03 != NULL
    );
    char
        * s_02 = metac_value_string_ex(p_v_02, METAC_WMODE_deep, NULL),
        * s_03 = metac_value_string_ex(p_v_03, METAC_WMODE_deep, NULL);
    fail_unless(
        s_02 != NULL && 
        s_03 != NULL
    );

    snprintf(called, sizeof(called),
        "test_function_with_extra_cases %d %d %ld %s %s", 
        arg_00.a.a_int,
        arg_01.a.a_int, arg_01.b.b_long,
        s_02,
        s_03);

    free(s_03);
    free(s_02);


    metac_value_delete(p_v_03);
    metac_value_delete(p_v_02);

    
    return arg_02;
}
METAC_GSYM_LINK(test_function_with_extra_cases);

METAC_START_TEST(test_function_with_extra) {
    char * s = NULL;
    char * expected = NULL;
    char * expected_called = NULL;

    test_union_hierarchy_t arg_00 = { .a = {.a_int = 55,},};
    test_struct_with_union_t arg_01 = {.a = {.a_int = 55}, .b = {.b_long = 5555}};
    test_struct_with_bitfields_t arg_02 = {.lng01 = 1, .lng02 = 22222, .lng03 = 3, .lng11 = 11, .lng12 = 2222, .lng13 = 13l};
    test_struct_with_flexarr_t arg_03 = {.len = 1};

    _CALL_PROCESS_FN(NULL, test_function_with_extra_cases,
        arg_00, 
        arg_01, 
        arg_02, 
        arg_03
    )
        fail_unless(res == 0, "Call wasn't successful, expected successful");

        expected_called = "test_function_with_extra_cases 55 55 5555 "
            "(test_struct_with_bitfields_t []){{.lng01 = 1, .lng02 = 22222, .lng03 = 3, .lng11 = 11, .lng12 = 2222, .lng13 = 13,"
            "},} "
            "(test_struct_with_flexarr_t []){{.len = 1,"
            " .arr = {},},}";
        fail_unless(strcmp(called, expected_called) == 0, "called: got %s, expected %s", called, expected_called);

        expected = "{.lng01 = 1, .lng02 = 22222, .lng03 = 3, .lng11 = 11, .lng12 = 2222, .lng13 = 13,"
            "}";
        s = metac_value_string_ex(p_res_val, METAC_WMODE_deep, NULL);
        fail_unless(s != NULL);
        fail_unless(strcmp(s, expected) == 0, "got %s, expected %s", s, expected);
        free(s);

    _CALL_PROCESS_END
}END_TEST

// variadic param tests
int test_function_with_va_list(const char * format, va_list vl) {
    return vsnprintf(called, sizeof(called), format, vl);
}
METAC_GSYM_LINK(test_function_with_va_list);

int test_function_with_va_args(const char * format, ...) {
    va_list l;
    va_start(l, format);
    int res = test_function_with_va_list(format, l);
    va_end(l);
    return res;
}
METAC_GSYM_LINK(test_function_with_va_args);

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

METAC_TAG_MAP_END

METAC_START_TEST(test_variadic_arg) {
    metac_tag_map_t * p_tagmap = va_args_tag_map();

    char * s = NULL;
    char * expected = NULL;
    char * expected_called = NULL;

    _CALL_PROCESS_FN(p_tagmap, test_function_with_va_args,
        "test_function_with_va_args"
    )
        fail_unless(res == 0, "Call wasn't successful, expected successful");

        expected_called = "test_function_with_va_args";
        fail_unless(strcmp(called, expected_called) == 0, "called: got %s, expected %s", called, expected_called);

        expected = "26";
        s = metac_value_string_ex(p_res_val, METAC_WMODE_deep, NULL);
        fail_unless(s != NULL);
        fail_unless(strcmp(s, expected) == 0, "got %s, expected %s", s, expected);
        free(s);
    _CALL_PROCESS_END

    _CALL_PROCESS_FN(p_tagmap, test_function_with_va_args,
        "test_function_with_va_args %s", "ppp"
    )
        fail_unless(res == 0, "Call wasn't successful, expected successful");

        expected_called = "test_function_with_va_args ppp";
        fail_unless(strcmp(called, expected_called) == 0, "called: got %s, expected %s", called, expected_called);

        expected = "30";
        s = metac_value_string_ex(p_res_val, METAC_WMODE_deep, NULL);
        fail_unless(s != NULL);
        fail_unless(strcmp(s, expected) == 0, "got %s, expected %s", s, expected);
        free(s);

    _CALL_PROCESS_END

    _CALL_PROCESS_FN(p_tagmap, test_function_with_va_args,
        "test_function_with_va_args %d", 777
    )
        fail_unless(res == 0, "Call wasn't successful, expected successful");

        expected_called = "test_function_with_va_args 777";
        fail_unless(strcmp(called, expected_called) == 0, "called: got %s, expected %s", called, expected_called);

        expected = "30";
        s = metac_value_string_ex(p_res_val, METAC_WMODE_deep, NULL);
        fail_unless(s != NULL);
        fail_unless(strcmp(s, expected) == 0, "got %s, expected %s", s, expected);
        free(s);

    _CALL_PROCESS_END

    _CALL_PROCESS_FN(p_tagmap, test_function_with_va_args,
        "test_function_with_va_args %o, %u, %x, %X", 1180000, 1200000, 1210000, 1220000
    )
        fail_unless(res == 0, "Call wasn't successful, expected successful");

        expected_called = "test_function_with_va_args 4400540, 1200000, 127690, 129DA0";
        fail_unless(strcmp(called, expected_called) == 0, "called: got %s, expected %s", called, expected_called);

        expected = "59";
        s = metac_value_string_ex(p_res_val, METAC_WMODE_deep, NULL);
        fail_unless(s != NULL);
        fail_unless(strcmp(s, expected) == 0, "got %s, expected %s", s, expected);
        free(s);

    _CALL_PROCESS_END

    _CALL_PROCESS_FN(p_tagmap, test_function_with_va_args,
        "test_function_with_va_args %lo, %lu, %lx, %lX", 11800000L, 12000000L, 12100000L, 12200000L
    )
        fail_unless(res == 0, "Call wasn't successful, expected successful");

        expected_called = "test_function_with_va_args 55006700, 12000000, b8a1a0, BA2840";
        fail_unless(strcmp(called, expected_called) == 0, "called: got %s, expected %s", called, expected_called);

        expected = "61";
        s = metac_value_string_ex(p_res_val, METAC_WMODE_deep, NULL);
        fail_unless(s != NULL);
        fail_unless(strcmp(s, expected) == 0, "got %s, expected %s", s, expected);
        free(s);

    _CALL_PROCESS_END

    _CALL_PROCESS_FN(p_tagmap, test_function_with_va_args,
        "test_function_with_va_args %f, %g, %e", 11.1, 11.2, -11.3
    )
        fail_unless(res == 0, "Call wasn't successful, expected successful");

        expected_called = "test_function_with_va_args 11.100000, 11.2, -1.130000e+01";
        fail_unless(strcmp(called, expected_called) == 0, "called: got %s, expected %s", called, expected_called);

        expected = "57";
        s = metac_value_string_ex(p_res_val, METAC_WMODE_deep, NULL);
        fail_unless(s != NULL);
        fail_unless(strcmp(s, expected) == 0, "got %s, expected %s", s, expected);
        free(s);

    _CALL_PROCESS_END

    metac_tag_map_delete(p_tagmap);
}END_TEST

METAC_START_TEST(test_variadic_list) {
    metac_tag_map_t * p_tagmap = va_args_tag_map();

#define VA_LIST(_args_...) VA_LIST_FROM_CONTAINER(c, _args_)

    char * s = NULL;
    char * expected = NULL;
    char * expected_called = NULL;

    WITH_VA_LIST_CONTAINER(c,
        _CALL_PROCESS_FN(p_tagmap, test_function_with_va_list,
            "test_function_with_va_list %x %x %x %x %x %x", VA_LIST(1,2,3,4,5,6))

            expected = "test_function_with_va_list(\"test_function_with_va_list %x %x %x %x %x %x\", "
                "VA_LIST((unsigned int)1, (unsigned int)2, (unsigned int)3, (unsigned int)4, (unsigned int)5, (unsigned int)6))";
            s = metac_value_string_ex(p_params_val, METAC_WMODE_deep, p_tagmap);
            fail_unless(s != NULL);
            fail_unless(strcmp(s, expected) == 0, "got %s, expected %s", s, expected);
            free(s);

            fail_unless(res == 0, "Call wasn't successful, expected successful");

            expected_called = "test_function_with_va_list 1 2 3 4 5 6";
            fail_unless(strcmp(called, expected_called) == 0, "called: got %s, expected %s", called, expected_called);

            expected = "38";
            s = metac_value_string_ex(p_res_val, METAC_WMODE_deep, NULL);
            fail_unless(s != NULL);
            fail_unless(strcmp(s, expected) == 0, "got %s, expected %s", s, expected);
            free(s);

        _CALL_PROCESS_END
    );

    metac_tag_map_delete(p_tagmap);
}END_TEST