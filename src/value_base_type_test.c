#include "metac/test.h"

#include "entry.c"
#include "entry_cdecl.c"
#include "entry_tag.c"
#include "hashmap.c"
#include "iterator.c"
#include "value.c"
#include "value_base_type.c"
#include "value_with_args.c"

/* test things on char and test everything with some basic sanity checks */

/* char common part */
static void test_char(metac_value_t * p_val, char * p_actual_data, metac_flag_t expected_write_err) {
    fail_unless(p_val != NULL, "couldn't create p_val");

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
#define _test_sanity_(_type_, _type_pseudoname_, _val_0_, _val_1_) \
    _type_ _type_pseudoname_ ##_sanity = _val_0_; \
    METAC_GSYM_LINK(_type_pseudoname_ ##_sanity); \
    METAC_START_TEST \
    (test_## _type_pseudoname_ ##_sanity) { \
        metac_value_t * p_val = METAC_VALUE_FROM_LINK(_type_pseudoname_ ##_sanity); \
        fail_unless(metac_value_is_## _type_pseudoname_(p_val) == 1, "metac_value_is_%s failed unexpectedly", # _type_pseudoname_); \
        _type_ val; \
        fail_unless(metac_value_## _type_pseudoname_(p_val, &val) == 0, "metac_value_%s failed unexpectedly", # _type_pseudoname_); \
        fail_unless(val == _type_pseudoname_ ##_sanity , "incorrect value 0 for " #_type_); \
        val = _val_1_; \
        fail_unless(metac_value_set_## _type_pseudoname_(p_val, val) == 0, "metac_value_set_%s failed unexpectedly", # _type_pseudoname_); \
        fail_unless(val == _type_pseudoname_ ##_sanity , "incorrect value 1 for " #_type_); \
        metac_value_delete(p_val); \
    }END_TEST

_test_sanity_(char, char, 'a', 'b')
_test_sanity_(unsigned char, uchar, 'a', 'b')
_test_sanity_(short, short, 0, 0x7fff)
_test_sanity_(unsigned short, ushort, 0, 0xffff)
_test_sanity_(int, int, 0, 0x7fffff)
_test_sanity_(unsigned int, uint, 0, 0xffffff)
_test_sanity_(long, long, 0, 0x7fffffff)
_test_sanity_(unsigned long, ulong, 0, 0xffffffff)
_test_sanity_(long long, llong, 0, 0x7fffffff)
_test_sanity_(unsigned long long, ullong, 0, 0xffffffff)
_test_sanity_(bool, bool, 0, 1)
_test_sanity_(float, float, 0.0, 3.14)
_test_sanity_(double, double, 0.0, 3.1415)
_test_sanity_(long double, ldouble, 0, 3.1415926535)
_test_sanity_(float complex, float_complex, 1.0*I, 1.0)
_test_sanity_(double complex, double_complex, 1.1*I, 1.1)
_test_sanity_(long double complex, ldouble_complex, 1.2*I, 2.1)
#undef _test_sanity_
