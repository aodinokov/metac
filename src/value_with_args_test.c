#include "metac/test.h"
#include <stdio.h>

#include "entry.c"
#include "entry_cdecl.c"
#include "entry_db.c"
#include "entry_tag.c"
#include "iterator.c"
#include "hashmap.c"
#include "value.c"
#include "value_base_type.c"
#include "value_string.c"

#include "value_with_args.c"

#define METAC_NEW_VALUE_WITH_ARGS(_p_tag_map_, _fn_, _args_...) \
        metac_new_value_with_args(_p_tag_map_, METAC_GSYM_LINK_ENTRY(_fn_), _args_)

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
    metac_value_with_args_delete(p_val);

    p_val = METAC_NEW_VALUE_WITH_ARGS(NULL, test_function_with_base_args_ptr,
        &arg_00, &arg_01, &arg_02, &arg_03, &arg_04,
        &arg_05, &arg_06, &arg_07, &arg_08, &arg_09, 
        &arg_10, &arg_11, &arg_12, &arg_13, &arg_14,
        &arg_15, &arg_16);
    fail_unless(p_val != NULL, "failed to collect args of test_function_with_base_args");
    metac_value_with_args_delete(p_val);
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
    metac_value_with_args_delete(p_val);
    p_val = METAC_NEW_VALUE_WITH_ARGS(NULL, test_function_with_enum_args_ptr, &arg_00, &arg_01, &arg_02, &arg_03);
        fail_unless(p_val != NULL, "failed to collect args of test_function_with_enum_args_ptr");
    metac_value_with_args_delete(p_val);
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
    metac_value_with_args_delete(p_val);
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
    metac_value_with_args_delete(p_val);
}END_TEST

void test_function_with_va_args(const char * format, ...) {
    va_list l;
    va_start(l, format);
    vprintf(format, l);
    va_end(l);
    return;
}
METAC_GSYM_LINK(test_function_with_va_args);

#include <stdio.h>
#include <ctype.h>

// naive implementation
// we can lookup in https://github.com/bminor/glibc/blob/master/stdio-common/vfprintf-internal.c#L288

// Function to parse a single format specifier
size_t parse_format_specifier(const char *format, size_t *pos, char *specifier) {
  size_t specifier_len = 0;
  // Skip leading percent sign
  if (format[*pos] == '%') {
    (*pos)++;
  } else {
    // Not a format specifier, return 0
    return 0;
  }

  // Check for optional flags (+, -, #, 0, space)
  while (*pos < strlen(format) && (format[*pos] == '+' || format[*pos] == '-' || format[*pos] == '#' || format[*pos] == '0' || format[*pos] == ' ')) {
    // specifier[specifier_len++] = format[*pos];
    (*pos)++;
  }

  // Check for optional minimum field width (digits)
  while (*pos < strlen(format) && isdigit(format[*pos])) {
    // specifier[specifier_len++] = format[*pos];
    (*pos)++;
  }

  // Check for optional precision (. and digits)
  if (*pos < strlen(format) && format[*pos] == '.') {
    // specifier[specifier_len++] = format[*pos];
    (*pos)++;
    while (*pos < strlen(format) && isdigit(format[*pos])) {
      // specifier[specifier_len++] = format[*pos];
      (*pos)++;
    }
  }

  // Check for optional length modifier (h, l, ll, L)
  if (*pos < strlen(format) && (format[*pos] == 'h' || format[*pos] == 'l' || format[*pos] == 'L')) {
    // specifier[specifier_len++] = format[*pos];
    (*pos)++;
    if (*pos < strlen(format) && (format[*pos] == 'l')) {
        // specifier[specifier_len++] = format[*pos];
        (*pos)++;
    }
  }

  // Check for conversion specifier
  if (isalpha(format[*pos])) {
    specifier[specifier_len++] = format[*pos];
    (*pos)++;
  } else {
    // Invalid format specifier
    return 0;
  }

  specifier[specifier_len] = '\0'; // Null terminate the specifier string
  return specifier_len;
}

// Function to count the number of format specifiers
size_t count_format_specifiers(const char *format) {
  size_t num_specifiers = 0;
  size_t pos = 0;
  char dummy_specifier[10]; // Temporary buffer to avoid passing NULL

  while (pos < strlen(format)) {
    if (format[pos] == '%') {
      // Check if a valid format specifier follows
      if (parse_format_specifier(format, &pos, dummy_specifier) > 0) {
        printf("%s at %d\n", dummy_specifier, pos);
        num_specifiers++;
      }
    }
    pos++;
  }
  return num_specifiers;
}


static int _va_arg_hdlr(metac_value_walker_hierarchy_t *p_hierarchy, metac_value_event_t * p_ev, void *p_context) {
    if (p_ev == NULL) {
        return -(EINVAL);
    }
    if (p_ev->type != METAC_RQVST_va_list && 
        metac_value_walker_hierarchy_level(p_hierarchy) < 0) {
        return -(EINVAL);
    }
    metac_value_t *p_val = metac_value_walker_hierarchy_value(p_hierarchy, 0);
    printf("yes %d!!!\n", p_ev->va_list_param_id);
    return -(EINVAL);
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

    printf("count: %d\n", count_format_specifiers("%% %05p |%12.4f|%12.4e|%12.4g|%12.4Lf|%12.4Lg|\n"));

    metac_value_t *p_val;
    test_function_with_va_args("%% %05p\n", NULL);
    p_val = METAC_NEW_VALUE_WITH_ARGS(p_tag_map, test_function_with_va_args, "%05p\n", NULL);
    // fail_unless(p_val != NULL, "failed to collect args of test_function_with_enum_args");
    // metac_value_with_args_delete(p_val);

    metac_tag_map_delete(p_tag_map);
}