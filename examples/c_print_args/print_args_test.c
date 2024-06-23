#include "metac/test.h"

#include "print_args.c"

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
    long double  arg_13,
    float complex  arg_14,
    double complex arg_15,
    long double complex arg_16
    ){
    printf("arg_11 %f\n", arg_11);
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
    long double complex *arg_16){
    return;
}
METAC_GSYM_LINK(test_function_with_base_args_ptr);


METAC_START_TEST(sanity){
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
    float complex  arg_14 = 14.0;
    double complex arg_15 = 15.0;
    long double complex arg_16 = 16.0;

    METAC_WRAP_FN_NORES(test_function_with_base_args,
        arg_00, arg_01, arg_02, arg_03, arg_04,
        arg_05, arg_06, arg_07, arg_08, arg_09, 
        arg_10, arg_11, arg_12, arg_13, arg_14,
        arg_15, arg_16);

    METAC_WRAP_FN_NORES(test_function_with_base_args_ptr,
        &arg_00, &arg_01, &arg_02, &arg_03, &arg_04,
        &arg_05, &arg_06, &arg_07, &arg_08, &arg_09, 
        &arg_10, &arg_11, &arg_12, &arg_13, &arg_14,
        &arg_15, &arg_16);


}END_TEST