#include "metac/test.h"

#include "entry.c"
#include "entry_cdecl.c"
#include "entry_db.c"
#include "entry_tag.c"
#include "hashmap.c"
#include "iterator.c"
#include "printf_format.c"
#include "value.c"
#include "value_base_type.c"
#include "value_with_args.c"

/* test things on char and test everything with some basic sanity checks */

/* char common part */
static void test_char(metac_value_t * p_val, char * p_actual_data, metac_flag_t expected_write_err) {
    fail_unless(p_val != NULL, "couldn't create p_val");

    metac_entry_t *p_entry = metac_entry_final_entry(metac_value_entry(p_val), NULL);
    fail_unless(metac_entry_is_base_type(p_entry)!=0);
    fail_unless(metac_entry_name(p_entry) != 0, "name is null");
    fail_unless(strcmp("char", metac_entry_name(p_entry)) == 0, "expected char, got %s", metac_entry_name(p_entry));
    metac_size_t sz;
    fail_unless(metac_entry_byte_size(p_entry, &sz) == 0);
    fail_unless(sz == sizeof(char), "sz got %d, expected %d", (int)sz, (int)sizeof(char));
    metac_encoding_t enc;
    fail_unless(metac_entry_base_type_encoding(p_entry, &enc)==0);
    fail_unless(enc == METAC_ENC_signed_char || enc == METAC_ENC_unsigned_char, "sz got %d, expected %d or %d", 
        (int)enc, (int)METAC_ENC_signed_char, (int)METAC_ENC_unsigned_char);


    char target;
    fail_unless(metac_value_is_char(p_val) != 0, "0 metac_value_is_char returned error");
    fail_unless(metac_value_char(p_val, &target) == 0, "0 metac_value_char returned error");
    fail_unless(*p_actual_data == target, "0 expected %c, got %c", *p_actual_data, target);

    ++target;
    fail_unless((metac_value_set_char(p_val, target) == 0) == (expected_write_err == 0), "1 metac_value_set_char returned error");
    fail_unless(metac_value_char(p_val, &target) == 0, "1 metac_value_char returned error");
    fail_unless(*p_actual_data == target, "1 expected %c, got %c", *p_actual_data, target);

    --target;
    fail_unless((metac_value_set_char(p_val, target) == 0) == (expected_write_err == 0), "2 metac_value_set_char returned error");
    fail_unless(metac_value_char(p_val, &target) == 0, "2 metac_value_char returned error");
    fail_unless(*p_actual_data == target, "2 expected %c, got %c", *p_actual_data, target);

    metac_value_delete(p_val);
}

char char_0 = 'a';
METAC_GSYM_LINK(char_0);
METAC_START_TEST(test_char_0) {
    test_char(METAC_VALUE_FROM_LINK(char_0), &char_0, 0);
}END_TEST

typedef char char_t;
char_t char_1 = 'b';
METAC_GSYM_LINK(char_1);

METAC_START_TEST(test_char_1) {
    test_char(METAC_VALUE_FROM_LINK(char_1), &char_1, 0);
}END_TEST

const char_t char_2 = 'c';
METAC_GSYM_LINK(char_2);

METAC_START_TEST(test_char_2) {
    test_char(METAC_VALUE_FROM_LINK(char_2), (char*)&char_2, 1/* because of const!*/);
}END_TEST

struct {
    char a;
    char b;
    char c;
}char_3_struct = {.a = 'a', .b = 'b', .c = 'c'};
METAC_GSYM_LINK(char_3_struct);
METAC_START_TEST(test_char_3) {
    metac_value_t * p_val = METAC_VALUE_FROM_LINK(char_3_struct);
    metac_num_t count = metac_value_member_count(p_val);
    fail_unless(count == 3, "got invalid number of fields %d, expected 3", count);
    for (metac_num_t id = 0; id < count; ++id) {
        test_char(metac_new_value_by_member_id(p_val, id), &((char*)&char_3_struct)[id], 0);
    }
    metac_value_delete(p_val);
}

// sanity checks
#define ___test_sanity_(_unique_id_,  _type_, _type_pseudoname_, _is_fn_, _fn_, _set_fn_, _val_0_, _val_1_, _val_1_str) \
    _type_ _type_pseudoname_ ##_sanity ##_unique_id_ = _val_0_; \
    METAC_GSYM_LINK(_type_pseudoname_ ##_sanity ##_unique_id_); \
    METAC_START_TEST \
    (test_## _type_pseudoname_ ##_sanity ##_unique_id_) { \
        metac_value_t * p_val = METAC_VALUE_FROM_LINK(_type_pseudoname_ ##_sanity ##_unique_id_); \
        fail_unless(_is_fn_(p_val) == 1, "%s failed unexpectedly", # _is_fn_); \
        _type_ val; \
        fail_unless(_fn_(p_val, &val) == 0, "%s failed unexpectedly", # _fn_); \
        fail_unless(val == _type_pseudoname_ ##_sanity ##_unique_id_ , "incorrect value 0 for " #_type_); \
        val = _val_1_; \
        fail_unless(_set_fn_(p_val, val) == 0, "%s failed unexpectedly", # _set_fn_); \
        fail_unless(val == _type_pseudoname_ ##_sanity ##_unique_id_, "incorrect value 1 for " #_type_); \
        char * str = metac_value_base_type_string(p_val); \
        fail_unless(str != NULL, "couldn't conver to string"); \
        fail_unless(strcmp(str, _val_1_str) == 0, "converted to %s, expected %s", str, _val_1_str); \
        val = _val_0_; \
        fail_unless(_set_fn_(p_val, val) == 0, "%s 2 failed unexpectedly", # _set_fn_); /* resetting data to initial value */ \
        fail_unless(val != _val_1_, "_val_1_ must not be equal to _val_0 - please select different values"); \
        char * str2 = metac_value_base_type_string(p_val); \
        fail_unless(str2 != NULL, "second convertion didn't work"); \
        fail_unless(metac_value_base_type_from_string(p_val, str) == p_val, "couln't convert back from string %s", str); \
        char * str3 = metac_value_base_type_string(p_val); \
        fail_unless(str3 != NULL, "third convertion didn't work"); \
        fail_unless(_fn_(p_val, &val) == 0, "%s 2 failed unexpectedly", # _fn_); \
        /* because of precision it doesn't work for some float types fail_unless(val == _val_1_, "got unexpected result for %s via %s to %s", str, str2, str3); */ \
        fail_unless(strcmp(str, str3) == 0 && strcmp(str, str2) != 0, "got unexpected result for %s via %s to %s", str, str2, str3); \
        free(str3); \
        free(str2); \
        free(str); \
        metac_value_delete(p_val); \
    }END_TEST
#define __test_sanity_(_unique_id_, _type_, _type_pseudoname_, _is_fn_, _fn_, _set_fn_, _val_0_, _val_1_, _val_1_str) \
      ___test_sanity_(_unique_id_,  _type_, _type_pseudoname_, _is_fn_, _fn_, _set_fn_, _val_0_, _val_1_, _val_1_str)
#define _test_sanity_(_type_, _type_pseudoname_, _val_0_, _val_1_, _val_1_str) \
      __test_sanity_(__COUNTER__, _type_, _type_pseudoname_, metac_value_is_## _type_pseudoname_, metac_value_## _type_pseudoname_, metac_value_set_## _type_pseudoname_, _val_0_, _val_1_, _val_1_str)


// printable chars
_test_sanity_(char, char, 'a', 'b', "'b'")
// non printable chars
_test_sanity_(char, char, 0, 13, "13")
_test_sanity_(unsigned char, uchar, 'a', 'b', "98")
_test_sanity_(short, short, 0, 0x7fff, "32767")
_test_sanity_(unsigned short, ushort, 0, 0xffff, "65535")
_test_sanity_(int, int, 0, 0x7fffff, "8388607")
_test_sanity_(unsigned int, uint, 0, 0xffffff, "16777215")
_test_sanity_(long, long, 0, 0x7fffffff, "2147483647")
_test_sanity_(unsigned long, ulong, 0, 4294967295, "4294967295")
_test_sanity_(long long, llong, 0, 0x7fffffff, "2147483647")
_test_sanity_(unsigned long long, ullong, 0, 0xffffffff, "4294967295")
_test_sanity_(bool, bool, 1, 0, "false")
_test_sanity_(bool, bool, 0, 1, "true")
_test_sanity_(float, float, 0.0, 3.14, "3.140000")
_test_sanity_(double, double, 0.0, 3.1415, "3.141500")
_test_sanity_(long double, ldouble, 0, 3.141590, "3.141590")
_test_sanity_(float complex, float_complex, 1.0*I, 1.0 + 1.0 * I, "1.000000 + I * 1.000000")
_test_sanity_(float complex, float_complex, 1.0*I, 1.0 - 1.0 * I, "1.000000 - I * 1.000000")
_test_sanity_(float complex, float_complex, 1.0*I, -1.0 + 1.0 * I, "-1.000000 + I * 1.000000")
_test_sanity_(float complex, float_complex, 1.0*I, -1.0 - 1.0 * I, "-1.000000 - I * 1.000000")
_test_sanity_(double complex, double_complex, 1.1*I, 1.1, "1.100000 + I * 0.000000")
_test_sanity_(double complex, double_complex, 1.0*I, 1.0 + 1.0 * I, "1.000000 + I * 1.000000")
_test_sanity_(double complex, double_complex, 1.0*I, 1.0 - 1.0 * I, "1.000000 - I * 1.000000")
_test_sanity_(double complex, double_complex, 1.0*I, -1.0 + 1.0 * I, "-1.000000 + I * 1.000000")
_test_sanity_(double complex, double_complex, 1.0*I, -1.0 - 1.0 * I, "-1.000000 - I * 1.000000")
_test_sanity_(long double complex, ldouble_complex, 1.2*I, 2.1, "2.100000 + I * 0.000000")
_test_sanity_(long double complex, ldouble_complex, 1.0*I, 1.0 + 1.0 * I, "1.000000 + I * 1.000000")
_test_sanity_(long double complex, ldouble_complex, 1.0*I, 1.0 - 1.0 * I, "1.000000 - I * 1.000000")
_test_sanity_(long double complex, ldouble_complex, 1.0*I, -1.0 + 1.0 * I, "-1.000000 + I * 1.000000")
_test_sanity_(long double complex, ldouble_complex, 1.0*I, -1.0 - 1.0 * I, "-1.000000 - I * 1.000000")

#undef _test_sanity_

// // TODO: metac_value_base_type_string negative tests, e.g convert from incorrect strings
// METAC_START_TEST(test_metac_value_base_type_string) {

// }END_TEST
