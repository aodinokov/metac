#include "metac/test.h"

#include "entry.c"
#include "iterator.c"
#include "value.c"
#include "value_base_type.c"

struct {
    char a:3;
    unsigned char b:3;
    short c:5;
    unsigned short d:9;
    int e: 9;
    unsigned int f: 8;
    long g: 17;
    unsigned long h: 17;
    long long i: 9;
    unsigned long long j: 9;
    bool k:1;
}test0_struct;
METAC_GSYM_LINK(test0_struct);
METAC_START_TEST(test0_read) {
    metac_value_t * p_val = METAC_VALUE_FROM_LINK(test0_struct);
    metac_num_t count = metac_value_member_count(p_val);
    fail_unless(count != 0, "got empty struct");
    for (metac_num_t fld_id = 0; fld_id < count; ++fld_id) {
        metac_value_t * p_mem_val = metac_new_value_by_member_id(p_val, fld_id);
        unsigned long val_counter = 0;
#define _check_field_read_(_type_, _expr_ , _fn_check_, _fn_read_) \
                fail_unless(_fn_check_(p_mem_val) != 0, "%s returned false", #_fn_check_); \
                _expr_ = 0; \
                do { \
                    _type_ data; \
                    _type_ exp_data = _expr_; \
                    \
                    fail_unless(_fn_read_(p_mem_val, &data) == 0, "%s returned error on %lx", #_fn_read_ , (long)exp_data); \
                    fail_unless(exp_data == data, "expected %lx, got %lx", (long)exp_data, (long)data); \
                    \
                    if (strcmp("bool", #_type_) == 0) { \
                        _expr_ = !_expr_; \
                    } else { \
                        ++_expr_; \
                    } \
                    ++val_counter; \
                }while(_expr_ != 0)

        switch (fld_id) {
        case 0: 
            _check_field_read_(char, test0_struct.a, metac_value_is_char, metac_value_char);
            break;
        case 1:
            _check_field_read_(unsigned char, test0_struct.b, metac_value_is_uchar, metac_value_uchar);
            break;
        case 2:
            _check_field_read_(short, test0_struct.c, metac_value_is_short, metac_value_short);
            break;
        case 3:
            _check_field_read_(unsigned short, test0_struct.d, metac_value_is_ushort, metac_value_ushort);
            break;
        case 4: 
            _check_field_read_(int, test0_struct.e, metac_value_is_int, metac_value_int);
            break;
        case 5: 
            _check_field_read_(unsigned int, test0_struct.f, metac_value_is_uint, metac_value_uint);
            break;
        case 6: 
            _check_field_read_(long, test0_struct.g, metac_value_is_long, metac_value_long);
            break;
        case 7: 
            _check_field_read_(unsigned long, test0_struct.h, metac_value_is_ulong, metac_value_ulong);
            break;
        case 8: 
            _check_field_read_(long long, test0_struct.i, metac_value_is_llong, metac_value_llong);
            break;
        case 9: 
            _check_field_read_(unsigned long long, test0_struct.j, metac_value_is_ullong, metac_value_ullong);
            break;
        case 10: 
            _check_field_read_(bool, test0_struct.k, metac_value_is_bool, metac_value_bool);
            break;

#undef _check_field_read_
        }
        fail_unless(val_counter != 0, "looks like we didn't do loops");
        metac_value_delete(p_mem_val);
    }
    metac_value_delete(p_val);
}END_TEST

METAC_START_TEST(test0_write) {
    metac_value_t * p_val = METAC_VALUE_FROM_LINK(test0_struct);
    metac_num_t count = metac_value_member_count(p_val);
    fail_unless(count != 0, "got empty struct");
    for (metac_num_t fld_id = 0; fld_id < count; ++fld_id) {
        metac_value_t * p_mem_val = metac_new_value_by_member_id(p_val, fld_id);
        unsigned long val_counter = 0;
#define _check_field_write_(_type_, _expr_ , _read_fn_, _write_fn_) \
                _expr_ = 0; \
                do { \
                    _type_ data; \
                    \
                    fail_unless(_read_fn_(p_mem_val, &data) == 0, "%s returned error on %lx", #_read_fn_ , (long)_expr_); \
                    if (strcmp("bool", #_type_) == 0) { \
                        data = !data; \
                    } else { \
                        ++data; \
                    } \
                    fail_unless(_write_fn_(p_mem_val, data) == 0, "%s returned error on %lx", #_write_fn_ , (long)_expr_); \
                    fail_unless(_expr_ == data, "%s: wrote %lx, got %lx", #_type_, (long)data, (long)_expr_); \
                    \
                    if (strcmp("bool", #_type_) == 0) { \
                        _expr_ = !_expr_; \
                    } else { \
                        ++_expr_; \
                    } \
                    ++val_counter; \
                }while(_expr_ != 0)

        switch (fld_id) {
        case 0:
            test0_struct.b = 0x5;
            _check_field_write_(char, test0_struct.a, metac_value_char, metac_value_set_char);
            fail_unless(test0_struct.b == 0x5, "filed a affected field b");
            break;
        case 1:
            test0_struct.a = 0x2;
            test0_struct.c = 0x5;
            _check_field_write_(unsigned char, test0_struct.b, metac_value_uchar, metac_value_set_uchar);
            fail_unless(test0_struct.a == 0x2, "field b affected field a");
            fail_unless(test0_struct.c == 0x5, "field b affected field c");
            break;
        case 2:
            test0_struct.b = 0x5;
            test0_struct.d = 0xa;
            _check_field_write_(short, test0_struct.c, metac_value_short, metac_value_set_short);
            fail_unless(test0_struct.b == 0x5, "field c affected field b");
            fail_unless(test0_struct.d == 0xa, "field c affected field d");
            break;
        case 3:
            test0_struct.c = 0x5;
            test0_struct.e = 0xa;
            _check_field_write_(unsigned short, test0_struct.d, metac_value_ushort, metac_value_set_ushort);
            fail_unless(test0_struct.c == 0x5, "field d affected field c");
            fail_unless(test0_struct.e == 0xa, "field d affected field e");
            break;
        case 4: 
            test0_struct.d = 0x5;
            test0_struct.f = 0xa;
            _check_field_write_(int, test0_struct.e, metac_value_int, metac_value_set_int);
            fail_unless(test0_struct.d == 0x5, "field e affected field d");
            fail_unless(test0_struct.f == 0xa, "field e affected field f");
            break;
        case 5: 
            test0_struct.e = 0x55;
            test0_struct.g = 0xaa;
            _check_field_write_(unsigned int, test0_struct.f, metac_value_uint, metac_value_set_uint);
            fail_unless(test0_struct.e == 0x55, "field f affected field e");
            fail_unless(test0_struct.g == 0xaa, "field f affected field g");
            break;
        case 6: 
            test0_struct.f = 0x55;
            test0_struct.h = 0xaa;
            _check_field_write_(long, test0_struct.g, metac_value_long, metac_value_set_long);
            fail_unless(test0_struct.f == 0x55, "filed g affected field f");
            fail_unless(test0_struct.h == 0xaa, "filed g affected field h");
            break;
        case 7: 
            test0_struct.g = 0x55;
            test0_struct.i = 0xaa;
            _check_field_write_(unsigned long, test0_struct.h, metac_value_ulong, metac_value_set_ulong);
            fail_unless(test0_struct.g == 0x55, "field h affected field g");
            fail_unless(test0_struct.i == 0xaa, "field h affected field i");
            break;
        case 8: 
            test0_struct.h = 0x55;
            test0_struct.j = 0xaa;
            _check_field_write_(long long, test0_struct.i, metac_value_llong, metac_value_set_llong);
            fail_unless(test0_struct.h == 0x55, "filed i affected field j");
            fail_unless(test0_struct.j == 0xaa, "filed i affected field j");
            break;
        case 9: 
            test0_struct.i = 0x55;
            _check_field_write_(unsigned long long, test0_struct.j, metac_value_ullong, metac_value_set_ullong);
            fail_unless(test0_struct.i == 0x55, "field j affected field i");
            break;
        case 10: 
            _check_field_write_(bool, test0_struct.k, metac_value_bool, metac_value_set_bool);
            break;

#undef _check_field_write_
        }
        fail_unless(val_counter != 0, "looks like we didn't do loops");
        metac_value_delete(p_mem_val);
    }
    metac_value_delete(p_val);
}END_TEST
