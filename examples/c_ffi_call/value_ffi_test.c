#include "metac/test.h"

#include "metac/reflect.h"

#include "value_ffi.c"


// const arg count tests data //
static char called[256];
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
    snprintf(called, sizeof(called),
        "test_function_with_enum_args");
    return;
}
METAC_GSYM_LINK(test_function_with_enum_args);

void test_function_with_enum_args_ptr(
    enum test_enum_01 * arg_00,
    enum test_enum_02 * arg_01,
    enum test_enum_04 * arg_02,
    test_enum_08_t * arg_03) {
    snprintf(called, sizeof(called),
        "test_function_with_enum_args_ptr");
    return;
}
METAC_GSYM_LINK(test_function_with_enum_args_ptr);

typedef struct {
    short a;
    int arr[10];
} test_struct_t;

void test_function_with_struct_args(
    test_struct_t arg_00,
    test_struct_t * arg_01,
    test_struct_t ** arg_02) {
    snprintf(called, sizeof(called),
        "test_function_with_struct_args");
    return;
}
METAC_GSYM_LINK(test_function_with_struct_args);

typedef int test_1d_array_t[15];
typedef int test_2d_array_t[3][5];

void test_function_with_array_args(
    test_1d_array_t arg_00,
    test_1d_array_t * arg_01,
    test_1d_array_t ** arg_02,
    test_2d_array_t arg_03,
    test_2d_array_t * arg_04,
    test_2d_array_t ** arg_05) {
    snprintf(called, sizeof(called),
        "test_function_with_array_args");
    return;
}
METAC_GSYM_LINK(test_function_with_array_args);
////

#define _CALL_PROCESS_FN(_tag_map_, _fn_, _args_...) { \
    called[0] = 0; \
    metac_entry_t * p_entry = METAC_GSYM_LINK_ENTRY(_fn_); \
    metac_value_t * p_params_val = METAC_NEW_VALUE_WITH_CALL_PARAMS_AND_WRAP(_tag_map_, p_entry, _fn_, _args_); \
    metac_value_t *p_res_val = metac_new_value_with_call_result(p_params_val); \
    int res = metac_value_call(p_params_val, (void (*)(void)) _fn_, p_res_val);

#define _CALL_PROCESS_FN_PTR(_tag_map_, _type_, _fn_, _args_...) { \
    called[0] = 0; \
    WITH_METAC_DECLLOC(dec, _type_ * p = _fn_); \
    metac_entry_t * p_entry = METAC_ENTRY_FROM_DECLLOC(dec, p); \
    metac_value_t * p_params_val = METAC_NEW_VALUE_WITH_CALL_PARAMS_AND_WRAP(_tag_map_, metac_entry_pointer_entry(p_entry), _fn_, _args_); \
    metac_value_t *p_res_val = metac_new_value_with_call_result(p_params_val); \
    int res = metac_value_call(p_params_val, (void (*)(void)) p, p_res_val);


#define _CALL_PROCESS_END \
        metac_value_with_call_result_delete(p_res_val); \
        metac_value_with_call_params_delete(p_params_val); \
    }

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

    _CALL_PROCESS_END


}END_TEST