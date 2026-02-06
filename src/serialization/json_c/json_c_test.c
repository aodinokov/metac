/**
 * @file json_test.c
 * @brief Comprehensive test suite for JSON serialization/deserialization
 *
 * Tests json-c based serialization and deserialization, mirroring yaml_test.c
 * tests for consistency and correctness.
 */

#include "metac/test.h"
#include <stdio.h>
#include <string.h>

#if WITH_JSON_C

/* Include core implementation units so the test is self-contained */
#include "../../entry.c"
#include "../../entry_db.c"
#include "../../entry_cdecl.c"
#include "../../entry_tag.c"
#include "../../iterator.c"
#include "../../hashmap.c"
#include "../../printf_format.c"
#include "../../value.c"
#include "../../value_base_type.c"
#include "../../value_string.c"
#include "../../value_with_args.c"

/* Include JSON serialization/deserialization implementation */
#include "json_c_ser_context.c"
#include "json_c_ser_builder.c"
#include "json_c_ser_api.c"
#include "json_c_deser_context.c"
#include "json_c_deser_parser.c"
#include "json_c_deser_api.c"

#include "metac/serialization/json_c.h"

/* Define test data globals matching yaml_test.c pattern */
char json_test_char = 'z';
METAC_GSYM_LINK(json_test_char);

int json_test_int = 42;
METAC_GSYM_LINK(json_test_int);

enum json_test_enum { JTE_A = 0, JTE_B = 5 } json_test_enum_v = JTE_B;
METAC_GSYM_LINK(json_test_enum_v);

struct json_test_struct {
    int a;
    char b;
} json_test_struct_v = {.a = -7, .b = 'k'};
METAC_GSYM_LINK(json_test_struct_v);

int json_test_array[3] = {1, 2, 3};
METAC_GSYM_LINK(json_test_array);

/* Test consistency between convenience API and context API */
METAC_START_TEST(json_basic_types_consistency) {
    /* Test char serialization consistency */
    metac_value_t *p_char = METAC_VALUE_FROM_LINK(json_test_char);
    fail_unless(p_char != NULL, "failed to obtain char value");

    char *conv = metac_value_to_json_c_string(p_char, METAC_WMODE_shallow);
    fail_unless(conv != NULL, "convenience API returned NULL for char");


    metac_json_c_serialization_t *ctx =
        metac_json_c_serialization_new(p_char, METAC_WMODE_shallow);
    fail_unless(ctx != NULL, "failed to create context for char");

    fail_unless(metac_json_c_serialization_serialize(ctx) == 0, "serialize failed for char");
    const char *ctx_str = metac_json_c_serialization_get_string(ctx);
    fail_unless(ctx_str != NULL, "get_string failed");

    fail_unless(strcmp(conv, ctx_str) == 0,
        "char: outputs differ: %s vs %s", conv, ctx_str);

    metac_json_c_serialization_delete(ctx);
    metac_free_json_c_string(conv);
    metac_value_delete(p_char);

} END_TEST

/* Test basic struct serialization */
METAC_START_TEST(json_struct_serialization) {
    metac_value_t *p_struct = METAC_VALUE_FROM_LINK(json_test_struct_v);
    fail_unless(p_struct != NULL, "failed to obtain struct value");

    char *json_str = metac_value_to_json_c_string(p_struct, METAC_WMODE_shallow);
    fail_unless(json_str != NULL, "failed to serialize struct");
    fail_unless(strlen(json_str) > 0, "serialized JSON is empty");

    /* Verify JSON contains expected fields */
    fail_unless(strstr(json_str, "\"a\"") != NULL, "JSON missing 'a' field");
    fail_unless(strstr(json_str, "\"b\"") != NULL, "JSON missing 'b' field");

    metac_free_json_c_string(json_str);
    metac_value_delete(p_struct);

} END_TEST

/* Test basic array serialization */
METAC_START_TEST(json_array_serialization) {
    metac_value_t *p_array = METAC_VALUE_FROM_LINK(json_test_array);
    fail_unless(p_array != NULL, "failed to obtain array value");

    char *json_str = metac_value_to_json_c_string(p_array, METAC_WMODE_shallow);
    fail_unless(json_str != NULL, "failed to serialize array");
    fail_unless(strlen(json_str) > 0, "serialized JSON is empty");

    /* Verify JSON contains array marker and values */
    fail_unless(strstr(json_str, "[") != NULL, "JSON missing array marker");
    fail_unless(strstr(json_str, "1") != NULL, "JSON missing first element");

    metac_free_json_c_string(json_str);
    metac_value_delete(p_array);

} END_TEST

/* Test struct deserialization */
METAC_START_TEST(json_basic_deserialization) {
    /* Serialize original struct */
    metac_value_t *p_struct_src = METAC_VALUE_FROM_LINK(json_test_struct_v);
    fail_unless(p_struct_src != NULL, "failed to obtain source struct");

    char *json_str = metac_value_to_json_c_string(p_struct_src, METAC_WMODE_shallow);
    fail_unless(json_str != NULL, "failed to serialize struct");

    /* Create fresh target struct */
    struct json_test_struct target_struct;
    memset(&target_struct, 0, sizeof(target_struct));

    metac_value_t *p_struct_tgt = metac_new_value(
        METAC_GSYM_LINK_ENTRY(json_test_struct_v),
        &target_struct);
    fail_unless(p_struct_tgt != NULL, "failed to create target struct value");

    /* Deserialize */
    int ret = metac_value_from_json_c_string(p_struct_tgt, json_str, METAC_WMODE_shallow);
    fail_unless(ret == 0, "struct deserialization failed");

    /* Verify member values */
    fail_unless(target_struct.a == -7, "struct member 'a' mismatch: got %d, expected -7", target_struct.a);
    fail_unless(target_struct.b == 'k', "struct member 'b' mismatch: got %c, expected k", target_struct.b);

    /* Cleanup */
    metac_value_delete(p_struct_tgt);
    metac_free_json_c_string(json_str);
    metac_value_delete(p_struct_src);

} END_TEST

/* Test array deserialization */
METAC_START_TEST(json_array_deserialization) {
    /* Serialize original array */
    metac_value_t *p_array_src = METAC_VALUE_FROM_LINK(json_test_array);
    fail_unless(p_array_src != NULL, "failed to obtain source array");

    char *json_str = metac_value_to_json_c_string(p_array_src, METAC_WMODE_shallow);
    fail_unless(json_str != NULL, "failed to serialize array");

    /* Create fresh target array */
    int target_array[3] = {0, 0, 0};

    metac_value_t *p_array_tgt = metac_new_value(
        METAC_GSYM_LINK_ENTRY(json_test_array),
        &target_array);
    fail_unless(p_array_tgt != NULL, "failed to create target array value");

    /* Deserialize */
    int ret = metac_value_from_json_c_string(p_array_tgt, json_str, METAC_WMODE_shallow);
    fail_unless(ret == 0, "array deserialization failed");

    /* Verify elements */
    fail_unless(target_array[0] == 1, "array element 0 mismatch");
    fail_unless(target_array[1] == 2, "array element 1 mismatch");
    fail_unless(target_array[2] == 3, "array element 2 mismatch");

    /* Cleanup */
    metac_value_delete(p_array_tgt);
    metac_free_json_c_string(json_str);
    metac_value_delete(p_array_src);

} END_TEST

/* Test roundtrip consistency - serialize → deserialize → serialize */
METAC_START_TEST(json_roundtrip_consistency) {
    /* Serialize original struct */
    metac_value_t *p_orig = METAC_VALUE_FROM_LINK(json_test_struct_v);
    fail_unless(p_orig != NULL, "failed to obtain original struct");

    char *json_str1 = metac_value_to_json_c_string(p_orig, METAC_WMODE_shallow);
    fail_unless(json_str1 != NULL, "failed to serialize original");

    /* Deserialize into fresh struct */
    struct json_test_struct roundtrip;
    memset(&roundtrip, 0, sizeof(roundtrip));

    metac_value_t *p_rtval = metac_new_value(
        METAC_GSYM_LINK_ENTRY(json_test_struct_v),
        &roundtrip);
    fail_unless(p_rtval != NULL, "failed to create roundtrip value");

    int ret = metac_value_from_json_c_string(p_rtval, json_str1, METAC_WMODE_shallow);
    fail_unless(ret == 0, "roundtrip deserialization failed");

    /* Re-serialize the roundtrip value */
    char *json_str2 = metac_value_to_json_c_string(p_rtval, METAC_WMODE_shallow);
    fail_unless(json_str2 != NULL, "failed to re-serialize roundtrip");

    /* Compare JSON strings - should be identical */
    fail_unless(strcmp(json_str1, json_str2) == 0,
        "JSON round-trip mismatch:\nOriginal:\n%s\nRound-trip:\n%s", json_str1, json_str2);

    /* Cleanup */
    metac_value_delete(p_rtval);
    metac_free_json_c_string(json_str2);
    metac_free_json_c_string(json_str1);
    metac_value_delete(p_orig);

} END_TEST

/* Test context API for serialization */
METAC_START_TEST(json_context_api_serialization) {
    metac_value_t *p_struct = METAC_VALUE_FROM_LINK(json_test_struct_v);
    fail_unless(p_struct != NULL, "failed to obtain struct");

    /* Phase 1: Create context */
    metac_json_c_serialization_t *ctx =
        metac_json_c_serialization_new(p_struct, METAC_WMODE_shallow);
    fail_unless(ctx != NULL, "failed to create serialization context");

    /* Phase 2: Serialize */
    fail_unless(metac_json_c_serialization_serialize(ctx) == 0, "serialize failed");

    /* Phase 3: Get string */
    const char *json_str = metac_json_c_serialization_get_string(ctx);
    fail_unless(json_str != NULL, "get_string returned NULL");
    fail_unless(strlen(json_str) > 0, "serialized JSON is empty");

    /* Phase 4: Cleanup */
    metac_json_c_serialization_delete(ctx);
    metac_value_delete(p_struct);

} END_TEST

/* Test context API for deserialization */
METAC_START_TEST(json_context_api_deserialization) {
    /* First serialize to get JSON */
    metac_value_t *p_src = METAC_VALUE_FROM_LINK(json_test_struct_v);
    char *json_str = metac_value_to_json_c_string(p_src, METAC_WMODE_shallow);
    fail_unless(json_str != NULL, "failed to serialize for test");
    metac_value_delete(p_src);

    /* Create target and context */
    struct json_test_struct target;
    memset(&target, 0, sizeof(target));

    metac_value_t *p_tgt = metac_new_value(
        METAC_GSYM_LINK_ENTRY(json_test_struct_v),
        &target);
    fail_unless(p_tgt != NULL, "failed to create target value");

    /* Phase 1: Create context */
    metac_json_c_deserialization_t *ctx =
        metac_json_c_deserialization_new(p_tgt, METAC_WMODE_shallow);
    fail_unless(ctx != NULL, "failed to create deserialization context");

    /* Phase 2: Parse and populate */
    fail_unless(metac_json_c_deserialization_from_string(ctx, json_str, strlen(json_str)) == 0,
        "deserialization failed");

    /* Phase 3: Verify */
    fail_unless(target.a == -7, "deserialized a mismatch");
    fail_unless(target.b == 'k', "deserialized b mismatch");

    /* Phase 4: Cleanup */
    metac_json_c_deserialization_delete(ctx);
    metac_value_delete(p_tgt);
    metac_free_json_c_string(json_str);

} END_TEST

/* Test error handling with invalid JSON */
METAC_START_TEST(json_error_handling_invalid_json) {
    struct json_test_struct target;
    memset(&target, 0, sizeof(target));

    metac_value_t *p_tgt = metac_new_value(
        METAC_GSYM_LINK_ENTRY(json_test_struct_v),
        &target);
    fail_unless(p_tgt != NULL, "failed to create target");

    /* Try to deserialize invalid JSON */
    int ret = metac_value_from_json_c_string(p_tgt, "not valid json", METAC_WMODE_shallow);
    fail_unless(ret != 0, "deserialization should fail for invalid JSON");

    metac_value_delete(p_tgt);

} END_TEST

#endif /* WITH_JSON_C */
