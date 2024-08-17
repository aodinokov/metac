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

// uncomment the next line if the test doesn't work
// #define PRECHECK_DEBUG
struct {
    char a:6;
    char b:3;
    char c:5; // goes beyond the first char (shoud be moved to the next byte by compiler)
    int x: 10;
    long z: 22;
}precheck_struct;
METAC_GSYM_LINK(precheck_struct);

#define DUMP_MEM(_start_, _size_) \
	do { \
		int i; \
		unsigned char * p = (unsigned char *)_start_; \
		for (i=0; i<(_size_); i++) { \
			printf("%02x ", (int)*p++); \
		} \
		printf("\n"); \
	}while(0)

METAC_START_TEST(pre_check) {
    metac_value_t * p_val = METAC_VALUE_FROM_LINK(precheck_struct);
    char target;
    fail_unless(metac_value_char(p_val, &target) != 0, "metac_value_char must return error because p_val isn't char");
    metac_num_t count = metac_value_member_count(p_val);
    fail_unless(count != 0, "got empty struct");
    for (metac_num_t fld_id = 0; fld_id < count; ++fld_id) {
        metac_value_t * p_mem_val = metac_new_value_by_member_id(p_val, fld_id);
        metac_entry_t * p_mem_entry = p_mem_val->p_entry;

        metac_entry_t * p_final_entry = metac_entry_final_entry(p_mem_entry, NULL);
        metac_encoding_t enc;
        fail_unless(metac_entry_base_type_encoding(p_final_entry, &enc)==0);

        fail_unless(p_mem_entry->kind == METAC_KND_member, "must be member info kind");
        metac_offset_t byte_offset;
        metac_offset_t bit_offset;
        metac_offset_t bit_size;
        if (p_mem_entry->member_info.p_bit_offset != NULL) {
#ifdef PRECHECK_DEBUG
            printf("DWARF 3, LE %d\n", (int)IS_LITTLE_ENDIAN);
#endif
            metac_offset_t  _bit_size = (p_mem_entry->member_info.p_bit_size?(*p_mem_entry->member_info.p_bit_size):0),
                            _bit_offset = (p_mem_entry->member_info.p_bit_offset?(*p_mem_entry->member_info.p_bit_offset):0),
                            _byte_size = (p_mem_entry->member_info.p_byte_size?(*p_mem_entry->member_info.p_byte_size):0);
            metac_offset_t _data_bit_offset = (p_mem_entry->member_info.byte_offset + _byte_size) * 8 - (_bit_offset + _bit_size);
            byte_offset = _data_bit_offset >> 3;
            bit_offset = _data_bit_offset & 0x7;
            bit_size = _bit_size;
#ifdef PRECHECK_DEBUG
            printf("fld %4s, bit_offset %d, bit_size %d, location %d byte_size %d=> byte_offset %d, bit_offset %d bit_size %d\n",
                p_mem_entry->name, (int)(_bit_offset), (int)_bit_size, (int)p_mem_entry->member_info.byte_offset, (int)_byte_size,
                (int)byte_offset, (int)bit_offset, (int)bit_size);
#endif
        } else  if (p_mem_entry->member_info.p_data_bit_offset != NULL) { // test DWARF 4 approach
#ifdef PRECHECK_DEBUG
            printf("DWARF 4, LE %d\n", (int)IS_LITTLE_ENDIAN);
#endif
            metac_offset_t _data_bit_offset = *p_mem_entry->member_info.p_data_bit_offset;
            metac_offset_t _bit_size = p_mem_entry->member_info.p_bit_size != NULL?(*p_mem_entry->member_info.p_bit_size): 0;
            byte_offset = _data_bit_offset >> 3;
            bit_offset = _data_bit_offset & 0x7;
            bit_size = _bit_size;
#ifdef PRECHECK_DEBUG
            printf("fld %4s, data_bit_offset %d, bit_size %d => byte_offset %d, bit_offset %d bit_size %d\n",
                p_mem_entry->name, (int)(_data_bit_offset), (int)bit_size,
                (int)byte_offset, (int)bit_offset, (int)bit_size);
#endif
        }
        void * base_addr = NULL;
        switch (fld_id) {
        case 0: // field 'a'
            fail_unless(strcmp(p_mem_entry->name, "a") == 0);
            precheck_struct.a = 0;
            
            if (IS_LITTLE_ENDIAN) {
                base_addr = p_mem_val->addr + byte_offset;
            } else {
                fail_unless(p_mem_entry->parents_count == 1 && p_mem_entry->parents[0]->structure_type_info.byte_size != 0);
                base_addr = p_mem_val->addr + byte_offset;// + p_mem_entry->parents[0]->structure_type_info.byte_size - byte_offset;
                bit_offset = 8 - (bit_offset + bit_size);
            }
            do {
#ifdef PRECHECK_DEBUG
                printf("value %d: ", (int)precheck_struct.a);
                DUMP_MEM(&precheck_struct, sizeof(precheck_struct));
#endif
                fail_unless(base_addr != NULL, "based addr mustn't be NULL");
                char data = *((char*)base_addr);
#ifdef PRECHECK_DEBUG
                printf("read %x, bit_offset %x, mask %x => %x\n", (int)data, (int)bit_offset,
                    (int)((1 << bit_size) - 1),
                    (int)(data >> bit_offset) & ((1 << bit_size) - 1));
#endif
                data = (data >> bit_offset) & ((1 << bit_size) - 1);
                if ((enc == METAC_ENC_signed_char) && (data & (1 << (bit_size-1)))) { 
                    data = ((0xff) << ((bit_size))) ^ data; 
                }
                char exp_data = precheck_struct.a;
                fail_unless(exp_data == data, "expected %x, got %x", exp_data, data);
                ++precheck_struct.a;
            }while(precheck_struct.a != 0);
            break;
        case 2: // field 'c'
            fail_unless(strcmp(p_mem_entry->name, "c") == 0);
            precheck_struct.c = 0;
            if (IS_LITTLE_ENDIAN) {
                base_addr = p_mem_val->addr + byte_offset;
            } else {
                fail_unless(p_mem_entry->parents_count == 1 && p_mem_entry->parents[0]->structure_type_info.byte_size != 0);
                base_addr = p_mem_val->addr + byte_offset;//+ p_mem_entry->parents[0]->structure_type_info.byte_size - byte_offset;
                bit_offset = 8 - (bit_offset + bit_size);
            }
            do {
#ifdef PRECHECK_DEBUG
                printf("value %d: ", (int)precheck_struct.c);
                DUMP_MEM(&precheck_struct, sizeof(precheck_struct));
#endif
                fail_unless(base_addr != NULL, "based addr mustn't be NULL");
                char data = *((char*)base_addr);
#ifdef PRECHECK_DEBUG
                printf("read %x, bit_offset %x, mask %x => %x\n", (int)data, (int)bit_offset,
                    (int)((1 << bit_size) - 1),
                    (int)(data >> bit_offset) & ((1 << bit_size) - 1));
#endif
                data = (data >> bit_offset) & ((1 << bit_size) - 1);
                if ((enc == METAC_ENC_signed_char) && (data & (1 << (bit_size-1)))) { 
                    data = ((0xff) << ((bit_size))) ^ data; 
                }
                char exp_data = precheck_struct.c;
                fail_unless(exp_data == data, "expected %x, got %x", exp_data, data);
                ++precheck_struct.c;
            }while(precheck_struct.c != 0);
            break;
        }
        metac_value_delete(p_mem_val);
    }
    metac_value_delete(p_val);
}END_TEST

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
