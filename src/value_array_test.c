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

int arr_test1[] = {1,2,3,4,5};
METAC_GSYM_LINK(arr_test1);
METAC_START_TEST(test1_sanity) {
    metac_value_t * p_val = METAC_VALUE_FROM_LINK(arr_test1);
    fail_unless(p_val != NULL, "expected value pointer non-NULL");

    fail_unless(metac_value_has_elements(p_val) != 0, "expected has_elemented not false");

    metac_num_t sz = metac_value_element_count(p_val);
    fail_unless(sz == 5, "expected size 5, got %d", (int)sz);

    for (metac_num_t i = 0; i< sz; ++i) {
        metac_value_t * p_el_val = metac_new_value_by_element_id(p_val, i);
        fail_unless(p_el_val != NULL, "expected value pointer non-NULL");
        
        fail_unless(metac_value_is_int(p_el_val) != 0, "expected element to be int");
        fail_unless(metac_value_is_char(p_el_val) == 0, "expected element not char");

        int val;
        fail_unless(metac_value_int(p_el_val, &val) == 0, "expected read int operation successful");
        fail_unless(val == i + 1, "expected to read %d, got %d", (int)i+1, val);
        metac_value_delete(p_el_val);
    }
    fail_unless(metac_new_value_by_element_id(p_val, sz) == NULL, "expected value pointer be NULL for out of range > ");
    fail_unless(metac_new_value_by_element_id(p_val, -1) == NULL, "expected value pointer be NULL for out of range < ");

    metac_value_delete(p_val);
}END_TEST

int arr_test2[][5] = {
    {1,2,3,4,5},
    {1,2,3,4,5},
};
METAC_GSYM_LINK(arr_test2);
METAC_START_TEST(test2_sanity) {
    metac_value_t * p_val = METAC_VALUE_FROM_LINK(arr_test2);
    fail_unless(p_val != NULL, "expected value pointer non-NULL");

    fail_unless(metac_value_has_elements(p_val) != 0, "expected has_elemented not false");

    metac_num_t sz = metac_value_element_count(p_val);
    fail_unless(sz == 2, "expected size 2, got %d", (int)sz);

    for (metac_num_t i = 0; i< sz; ++i) {
        metac_value_t * p_ael_val = metac_new_value_by_element_id(p_val, i);
        fail_unless(metac_value_final_kind(p_ael_val, NULL) == METAC_KND_array_type, "expected array on the 1st level");

        metac_num_t sz = metac_value_element_count(p_ael_val);
        fail_unless(sz == 5, "expected size 5, got %d", (int)sz);
        for (metac_num_t i = 0; i< sz; ++i) {
            metac_value_t * p_el_val = metac_new_value_by_element_id(p_ael_val, i);
            fail_unless(p_el_val != NULL, "expected value pointer non-NULL");
            
            fail_unless(metac_value_is_int(p_el_val) != 0, "expected element to be int");
            fail_unless(metac_value_is_char(p_el_val) == 0, "expected element not char");

            int val;
            fail_unless(metac_value_int(p_el_val, &val) == 0, "expected read int operation successful");
            fail_unless(val == i + 1, "expected to read %d, got %d", (int)i+1, val);
            metac_value_delete(p_el_val);
        }
        metac_value_delete(p_ael_val);
    }
    fail_unless(metac_new_value_by_element_id(p_val, sz) == NULL, "expected value pointer be NULL for out of range > ");
    fail_unless(metac_new_value_by_element_id(p_val, -1) == NULL, "expected value pointer be NULL for out of range < ");

    metac_value_delete(p_val);
}END_TEST

struct {
    int sz;
    char data[];
}*p_string_test3 = NULL;
METAC_GSYM_LINK(p_string_test3);
METAC_START_TEST(test3_sanity) {
    metac_value_t * p_val = METAC_VALUE_FROM_LINK(p_string_test3);
    fail_unless(metac_value_is_pointer(p_val) != 0, "expected pointer");
    void * addr;
    fail_unless(metac_value_pointer(p_val, &addr) == 0, "failed to read pointer");
    fail_unless(addr == p_string_test3, "pointer read 1: expected %p, got %p", p_string_test3, addr);

    // init data
    char * data = "012345678";
    p_string_test3 = calloc(1, sizeof(*p_string_test3) + strlen(data) + 1);
    fail_unless(p_string_test3 != NULL, "couldn't allocate mem");
    p_string_test3->sz = strlen(data) + 1;
    strncpy(p_string_test3->data, data, p_string_test3->sz);

    fail_unless(metac_value_pointer(p_val, &addr) == 0, "failed to read pointer");
    fail_unless(addr == p_string_test3, "pointer read 2: expected %p, got %p", p_string_test3, addr);

    metac_value_t * p_arr_val = metac_new_element_count_value(p_val, 1);
    fail_unless(p_arr_val != NULL, "couldn't create p_arr_val");
    
    fail_unless(metac_value_final_kind(p_arr_val, NULL) == METAC_KND_array_type, "expected array type");
    metac_num_t cnt = metac_value_element_count(p_arr_val);
    fail_unless(cnt == 1, "expected elements count 1, got %d", (int)cnt);

    for (metac_num_t i = 0; i < cnt; ++i) {
        metac_value_t * p_elem_val = metac_new_value_by_element_id(p_arr_val, i);
        fail_unless(p_elem_val != NULL, "coun't get value for element level 1");

        fail_unless(metac_value_has_members(p_elem_val) == 1, "expected structs with members");
        metac_num_t mcnt = metac_value_member_count(p_elem_val);
        fail_unless(mcnt == 2, "expected member count 2, got %d", (int)mcnt);

        metac_value_t * p_sz_val = metac_new_value_by_member_id(p_elem_val, 0);
        fail_unless(p_sz_val != NULL, "wasn't able to get value for sz member");
        fail_unless(metac_value_is_int(p_sz_val) != 0, "expected int type for sz member");
        
        int sz;
        fail_unless(metac_value_int(p_sz_val, &sz) == 0, "counln't read sz member");
        fail_unless(sz == p_string_test3->sz, "sz member: expected %d, got %d", (int)p_string_test3->sz, (int)sz);
        metac_value_delete(p_sz_val);
        
        metac_value_t * p_data_val = metac_new_value_by_member_id(p_elem_val, 1);
        fail_unless(p_data_val != NULL, "wasn't able to get value for data member");
        fail_unless(metac_value_has_elements(p_data_val) != 0, "expected array for data member");
        fail_unless(metac_value_element_count_flexible(p_data_val) != 0, "expected flexible array for data member");

        metac_value_t * p_data_val_resized = metac_new_element_count_value(p_data_val, sz);
        metac_value_delete(p_data_val);
        fail_unless(p_data_val_resized != NULL, "wasn't able to get value for resized data member");
        fail_unless(metac_value_has_elements(p_data_val_resized) != 0, "expected array for resized data member");
        metac_num_t data_cnt = metac_value_element_count(p_data_val_resized);
        fail_unless(data_cnt == sz, "p_data_val_resized: expected count %d, got %d", (int)sz, (int)data_cnt);

        for (metac_num_t j = 0; j < data_cnt; ++j) {
            metac_value_t * p_elem_val = metac_new_value_by_element_id(p_data_val_resized, j);
            fail_unless(p_elem_val != NULL, "coun't get value for element level 2");
            fail_unless(metac_value_is_char(p_elem_val) != 0, "expected char on level 2");
            char v;
            fail_unless(metac_value_char(p_elem_val, &v) == 0, "couldn't read char");
            if (j != data_cnt - 1) {
                fail_unless(v == '0'+j, "iteration %i: expected %x, got %x", j, (int)('0'+j), (int)v);
            } else {
                // '\0' as the last symbol
                fail_unless(v == 0, "iteration %i: expected %x, got %x", j, (int)0, (int)v);
            }
            metac_value_delete(p_elem_val);
        }

        metac_value_delete(p_data_val_resized);
        metac_value_delete(p_elem_val);
    }
    metac_value_delete(p_arr_val);
    free(p_string_test3);

    addr = NULL;
    fail_unless(metac_value_set_pointer(p_val, addr) == 0, "failed to set pointer");
    fail_unless(addr == p_string_test3, "pointer write 1: expected %p, got %p", addr, p_string_test3);

    metac_value_delete(p_val);
}END_TEST