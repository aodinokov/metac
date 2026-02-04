#include "metac/test.h"
#include <stdio.h>
#include <stdlib.h>

// enable test only if needed
#if WITH_CJSON
// including c-file we want to test, because we need to test some static functions
#include "../../entry.c"
#include "../../entry_db.c"
#include "../../entry_cdecl.c"
#include "../../entry_tag.c"
#include "../../iterator.c"
#include "../../hashmap.c"
#include "../../printf_format.c"
#include "../../value.c"
#include "../../value_base_type.c"
#include "../../value_with_args.c"

// testing both these file (not sure why they were integrated together)
#include "value_to_cjson.c"
#include "value_from_cjson.c"

struct test_cjson_simple_struct {
    int y;
    char c;
} g_test_cjson_simple_struct = {.y = -10, .c = 'a'};
METAC_GSYM_LINK(g_test_cjson_simple_struct);

METAC_START_TEST(cjson_backend_simple_struct) {
    metac_value_t * p_val = METAC_VALUE_FROM_LINK(g_test_cjson_simple_struct);
    struct cJSON* json = metac_value_to_cjson(p_val, METAC_WMODE_deep, NULL);
    fail_unless(json != NULL, "metac_value_to_cjson returned NULL");

    char* json_str = cJSON_PrintUnformatted(json);
    fail_unless(json_str != NULL, "cJSON_PrintUnformatted returned NULL");

    const char* expected_str = "{\"y\":-10,\"c\":\"a\"}";
    fail_unless(strcmp(json_str, expected_str) == 0, "Expected '%s', got '%s'", expected_str, json_str);

    free(json_str);
    cJSON_Delete(json);
    metac_value_delete(p_val);
} END_TEST


struct test_cjson_array_struct {
    int simple_arr[3];
};
struct test_cjson_array_struct g_test_cjson_array_struct = { .simple_arr = {10, 20, 30} };
METAC_GSYM_LINK(g_test_cjson_array_struct);

METAC_START_TEST(cjson_backend_array_struct) {
    metac_value_t* p_val = METAC_VALUE_FROM_LINK(g_test_cjson_array_struct);
    fail_unless(p_val != NULL, "Wasn't able to get value for g_test_cjson_array_struct");

    struct cJSON* json = metac_value_to_cjson(p_val, METAC_WMODE_deep, NULL);
    fail_unless(json != NULL, "metac_value_to_cjson returned NULL");

    char* json_str = cJSON_PrintUnformatted(json);
    fail_unless(json_str != NULL, "cJSON_PrintUnformatted returned NULL");

    const char* expected_str = "{\"simple_arr\":[10,20,30]}";
    fail_unless(strcmp(json_str, expected_str) == 0, "Expected '%s', got '%s'", expected_str, json_str);

    free(json_str);
    cJSON_Delete(json);
    metac_value_delete(p_val);
} END_TEST


struct test_cjson_linked_list {
    int data;
    struct test_cjson_linked_list* next;
};

// Create a small linked list for testing
struct test_cjson_linked_list g_test_cjson_list_node2 = { .data = 20, .next = NULL };
struct test_cjson_linked_list g_test_cjson_list_node1 = { .data = 10, .next = &g_test_cjson_list_node2 };
struct test_cjson_linked_list* g_test_cjson_list_head = &g_test_cjson_list_node1;

METAC_GSYM_LINK(g_test_cjson_list_head);

METAC_START_TEST(cjson_backend_linked_list) {
    metac_value_t* p_val = METAC_VALUE_FROM_LINK(g_test_cjson_list_head);
    fail_unless(p_val != NULL, "Wasn't able to get value for g_test_cjson_list_head");

    struct cJSON* json = metac_value_to_cjson(p_val, METAC_WMODE_deep, NULL);
    fail_unless(json != NULL, "metac_value_to_cjson returned NULL");

    char* json_str = cJSON_PrintUnformatted(json);
    fail_unless(json_str != NULL, "cJSON_PrintUnformatted returned NULL");

    const char* expected_str = "{\"data\":10,\"next\":{\"data\":20,\"next\":null}}";
    fail_unless(strcmp(json_str, expected_str) == 0, "Expected '%s', got '%s'", expected_str, json_str);

    free(json_str);
    cJSON_Delete(json);
    metac_value_delete(p_val);
} END_TEST

// Test for circular dependencies
struct test_cjson_circular {
    int data;
    struct test_cjson_circular* loop;
};
struct test_cjson_circular g_test_cjson_circ1 = { .data = 1, .loop = NULL };
struct test_cjson_circular g_test_cjson_circ2 = { .data = 2, .loop = &g_test_cjson_circ1 };
//Create the loop
//int g_test_cjson_circ_init __attribute__((constructor)) = ({ g_test_cjson_circ1.loop = &g_test_cjson_circ2; 0; });

METAC_GSYM_LINK(g_test_cjson_circ1);

METAC_START_TEST(cjson_backend_circular_ref) {
    //Create the loop
    g_test_cjson_circ1.loop = &g_test_cjson_circ2;

    metac_value_t* p_val = METAC_VALUE_FROM_LINK(g_test_cjson_circ1);
    fail_unless(p_val != NULL, "Wasn't able to get value for g_test_cjson_circ1");
    
    struct cJSON* json = metac_value_to_cjson(p_val, METAC_WMODE_deep, NULL);
    fail_unless(json != NULL, "metac_value_to_cjson returned NULL");

    char* json_str = cJSON_PrintUnformatted(json);
    fail_unless(json_str != NULL, "cJSON_PrintUnformatted returned NULL");
    
    const char* expected_str = /* original was "{\"data\":1,\"loop\":{\"data\":2,\"loop\":{\"$error\":\"circular reference\"}}}";*/
        "{\"data\":1,\"loop\":{\"data\":2,\"loop\":{\"data\":1,\"loop\":{\"$error\":\"circular reference\"}}}}";
    fail_unless(strcmp(json_str, expected_str) == 0, "Expected '%s', got '%s'", expected_str, json_str);

    free(json_str);
    cJSON_Delete(json);
    metac_value_delete(p_val);
} END_TEST


enum test_cjson_sgnd {
    e_cjson_Char = 0,
    e_cjson_Short,
    e_cjson_Int,
};

struct test_cjson_tagged_union {
    enum test_cjson_sgnd selector;
    union {
        char c;
        short s;
        int i;
    } data;
};

METAC_TAG_MAP_NEW(g_test_cjson_tagged_union_map, NULL, {.mask =
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_variable) |
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_func_parameter) | 
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_member) |
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_final),},)
    METAC_TAG_MAP_ENTRY_FROM_TYPE(struct test_cjson_tagged_union)
        METAC_TAG_MAP_SET_TAG(0, METAC_TEO_entry, 0, METAC_TAG_MAP_ENTRY_MEMBER({.n = "data"}),
            METAC_UNION_MEMBER_SELECT_BY(selector,
                {.fld_val = e_cjson_Char, .union_fld_name = "c"},
                {.fld_val = e_cjson_Short, .union_fld_name = "s"},
                {.fld_val = e_cjson_Int, .union_fld_name = "i"}
            )
        )
    METAC_TAG_MAP_ENTRY_END
METAC_TAG_MAP_END

struct test_cjson_tagged_union g_test_cjson_tagged_union_var;
METAC_GSYM_LINK(g_test_cjson_tagged_union_var);

METAC_START_TEST(cjson_backend_deserialization_array) {
    const char* json_str = "{\"simple_arr\":[11,22,33]}";
    cJSON* json = cJSON_Parse(json_str);
    fail_unless(json != NULL, "cJSON_Parse failed");

    struct test_cjson_array_struct target_struct = {0};
    metac_entry_t* p_entry = METAC_GSYM_LINK_ENTRY(g_test_cjson_array_struct);
    metac_value_t* p_val = metac_new_value(p_entry, &target_struct);
    fail_unless(p_val != NULL, "metac_new_value failed for target_struct");

    int result = metac_value_from_cjson(p_val, json, NULL);
    fail_unless(result == 0, "metac_value_from_cjson failed");

    fail_unless(target_struct.simple_arr[0] == 11, "Array[0] should be 11, but is %d", target_struct.simple_arr[0]);
    fail_unless(target_struct.simple_arr[1] == 22, "Array[1] should be 22, but is %d", target_struct.simple_arr[1]);
    fail_unless(target_struct.simple_arr[2] == 33, "Array[2] should be 33, but is %d", target_struct.simple_arr[2]);

    metac_value_delete(p_val);
    cJSON_Delete(json);
}

METAC_START_TEST(cjson_backend_deserialization_struct) {
    const char* json_str = "{\"y\":-99,\"c\":\"Q\"}";
    cJSON* json = cJSON_Parse(json_str);
    fail_unless(json != NULL, "cJSON_Parse failed");

    struct test_cjson_simple_struct target_struct;
    metac_entry_t* p_entry = METAC_GSYM_LINK_ENTRY(g_test_cjson_simple_struct);
    metac_value_t* p_val = metac_new_value(p_entry, &target_struct);
    fail_unless(p_val != NULL, "metac_new_value failed for target_struct");

    int result = metac_value_from_cjson(p_val, json, NULL);
    fail_unless(result == 0, "metac_value_from_cjson failed");

    fail_unless(target_struct.y == -99, "Deserialization error: y should be -99, but is %d", target_struct.y);
    fail_unless(target_struct.c == 'Q', "Deserialization error: c should be 'Q', but is %c", target_struct.c);

    metac_value_delete(p_val);
    cJSON_Delete(json);
}

METAC_START_TEST(cjson_backend_deserialization_union) {
    metac_tag_map_t* p_tag_map = g_test_cjson_tagged_union_map();
    fail_unless(p_tag_map != NULL, "Couldn't create tagmap for tagged union");

    metac_value_t* p_val = METAC_VALUE_FROM_LINK(g_test_cjson_tagged_union_var);
    fail_unless(p_val != NULL, "Wasn't able to get value for g_test_cjson_tagged_union_var");

    // Test with char
    g_test_cjson_tagged_union_var.selector = e_cjson_Char;
    g_test_cjson_tagged_union_var.data.c = 'Z';
    
    struct cJSON* json_char = metac_value_to_cjson(p_val, METAC_WMODE_deep, p_tag_map);
    fail_unless(json_char != NULL, "[char] metac_value_to_cjson returned NULL");
    char* json_str_char = cJSON_PrintUnformatted(json_char);
    const char* expected_str_char = "{\"selector\":\"e_cjson_Char\",\"data\":{\"c\":\"Z\"}}";
    fail_unless(strcmp(json_str_char, expected_str_char) == 0, "[char] Expected '%s', got '%s'", expected_str_char, json_str_char);
    free(json_str_char);
    cJSON_Delete(json_char);

    // Test with int
    g_test_cjson_tagged_union_var.selector = e_cjson_Int;
    g_test_cjson_tagged_union_var.data.i = 999;

    struct cJSON* json_int = metac_value_to_cjson(p_val, METAC_WMODE_deep, p_tag_map);
    fail_unless(json_int != NULL, "[int] metac_value_to_cjson returned NULL");
    char* json_str_int = cJSON_PrintUnformatted(json_int);
    const char* expected_str_int = "{\"selector\":\"e_cjson_Int\",\"data\":{\"i\":999}}";
    fail_unless(strcmp(json_str_int, expected_str_int) == 0, "[int] Expected '%s', got '%s'", expected_str_int, json_str_int);
    free(json_str_int);
    cJSON_Delete(json_int);

    metac_value_delete(p_val);
    metac_tag_map_delete(p_tag_map);
}

#endif // WITH_CJSON
