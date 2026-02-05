#include "metac/test.h"
#include <stdio.h>
#include <string.h>

#if WITH_YAML

/* Include core implementation units so the test is self-contained like other tests */
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



/* Include our YAML implementation to test it in-process */
#include "yaml_context.c"
#include "yaml_handler.c"
#include "yaml_events.c"
#include "api.c"

#include "metac/serialization/yaml.h"

/* Define sample globals to create metac values from links */
char yaml_test_char = 'z';
METAC_GSYM_LINK(yaml_test_char);

int yaml_test_int = 42;
METAC_GSYM_LINK(yaml_test_int);

enum yaml_test_enum { YTE_A = 0, YTE_B = 5 } yaml_test_enum_v = YTE_B;
METAC_GSYM_LINK(yaml_test_enum_v);

struct yaml_test_struct {
    int a;
    char b;
} yaml_test_struct_v = {.a = -7, .b = 'k'};
METAC_GSYM_LINK(yaml_test_struct_v);

/* Test: convenience API vs context API consistency for several types */
METAC_START_TEST(yaml_basic_types_consistency) {
    /* Char */
    metac_value_t *p_char = METAC_VALUE_FROM_LINK(yaml_test_char);
    fail_unless(p_char != NULL, "failed to obtain char value");

    char *conv = metac_value_to_yaml_string(p_char, METAC_WMODE_shallow);
    fail_unless(conv != NULL, "convenience API returned NULL for char");

    metac_yaml_serialization_t *ctx = metac_yaml_serialization_new(p_char, METAC_WMODE_shallow);
    fail_unless(ctx != NULL, "failed to create context for char");
    size_t sz = 0;
    fail_unless(metac_yaml_serialization_measure(ctx, &sz) == 0, "measure failed for char");
    char *buf = malloc(sz);
    fail_unless(buf != NULL, "malloc failed");
    fail_unless(metac_yaml_serialization_to_buffer(ctx, buf, sz) == 0, "to_buffer failed for char");
    fail_unless(strcmp(conv, buf) == 0, "char: outputs differ: %s vs %s", conv, buf);
    free(buf);
    metac_yaml_serialization_delete(ctx);
    metac_free_yaml_string(conv);
    metac_value_delete(p_char);

    /* Int */
    metac_value_t *p_int = METAC_VALUE_FROM_LINK(yaml_test_int);
    fail_unless(p_int != NULL, "failed to obtain int value");

    conv = metac_value_to_yaml_string(p_int, METAC_WMODE_shallow);
    fail_unless(conv != NULL, "convenience API returned NULL for int");

    ctx = metac_yaml_serialization_new(p_int, METAC_WMODE_shallow);
    fail_unless(ctx != NULL, "failed to create context for int");
    sz = 0;
    fail_unless(metac_yaml_serialization_measure(ctx, &sz) == 0, "measure failed for int");
    buf = malloc(sz);
    fail_unless(buf != NULL, "malloc failed");
    fail_unless(metac_yaml_serialization_to_buffer(ctx, buf, sz) == 0, "to_buffer failed for int");
    fail_unless(strcmp(conv, buf) == 0, "int: outputs differ: %s vs %s", conv, buf);
    free(buf);
    metac_yaml_serialization_delete(ctx);
    metac_free_yaml_string(conv);
    metac_value_delete(p_int);

    /* Enum */
    metac_value_t *p_enum = METAC_VALUE_FROM_LINK(yaml_test_enum_v);
    fail_unless(p_enum != NULL, "failed to obtain enum value");

    conv = metac_value_to_yaml_string(p_enum, METAC_WMODE_shallow);
    fail_unless(conv != NULL, "convenience API returned NULL for enum");

    ctx = metac_yaml_serialization_new(p_enum, METAC_WMODE_shallow);
    fail_unless(ctx != NULL, "failed to create context for enum");
    sz = 0;
    fail_unless(metac_yaml_serialization_measure(ctx, &sz) == 0, "measure failed for enum");
    buf = malloc(sz);
    fail_unless(buf != NULL, "malloc failed");
    fail_unless(metac_yaml_serialization_to_buffer(ctx, buf, sz) == 0, "to_buffer failed for enum");
    fail_unless(strcmp(conv, buf) == 0, "enum: outputs differ: %s vs %s", conv, buf);
    free(buf);
    metac_yaml_serialization_delete(ctx);
    metac_free_yaml_string(conv);
    metac_value_delete(p_enum);

    /* Struct */
    metac_value_t *p_struct = METAC_VALUE_FROM_LINK(yaml_test_struct_v);
    fail_unless(p_struct != NULL, "failed to obtain struct value");

    conv = metac_value_to_yaml_string(p_struct, METAC_WMODE_shallow);
    fail_unless(conv != NULL, "convenience API returned NULL for struct");

    ctx = metac_yaml_serialization_new(p_struct, METAC_WMODE_shallow);
    fail_unless(ctx != NULL, "failed to create context for struct");
    sz = 0;
    fail_unless(metac_yaml_serialization_measure(ctx, &sz) == 0, "measure failed for struct");
    buf = malloc(sz);
    fail_unless(buf != NULL, "malloc failed");
    fail_unless(metac_yaml_serialization_to_buffer(ctx, buf, sz) == 0, "to_buffer failed for struct");
    fail_unless(strcmp(conv, buf) == 0, "struct: outputs differ: %s vs %s", conv, buf);
    free(buf);
    metac_yaml_serialization_delete(ctx);
    metac_free_yaml_string(conv);
    metac_value_delete(p_struct);

} END_TEST

// /* Register tests */
// SIMPLE_TEST_CASE(yaml_basic_types_consistency)
// /**
//  * @file yaml_test.c
//  * @brief YAML serialization API tests
//  * 
//  * Tests both Context API and Convenience API with shared code architecture.
//  * Verifies that both APIs produce identical output.
//  */

// #include "metac/test.h"
// #include "metac/backend/yaml.h"
// #include <string.h>
// #include <stdlib.h>
// #include <check.h>

/**
 * @brief Test: Create and delete context
 */
METAC_START_TEST(yaml_context_lifecycle) {
    metac_value_t val;
    memset(&val, 0, sizeof(val));
    
    // Create context
    metac_yaml_serialization_t *p_serial = 
        metac_yaml_serialization_new(&val, METAC_WMODE_shallow);
    fail_unless(p_serial != NULL, "Failed to create context");
    
    // Context should not be measured yet
    fail_unless(!metac_yaml_serialization_is_measured(p_serial), 
        "Context should not be measured initially");
    
    // Delete context
    metac_yaml_serialization_delete(p_serial);
    
} END_TEST

/**
 * @brief Test: Convenience API fire-and-forget
 */
METAC_START_TEST(yaml_convenience_api) {
    metac_value_t *p_val = METAC_VALUE_FROM_LINK(yaml_test_int);
    fail_unless(p_val != NULL, "failed to obtain int value");

    // Use convenience API
    char *yaml_str = metac_value_to_yaml_string(p_val, METAC_WMODE_shallow);

    // Should succeed and return allocated string
    fail_unless(yaml_str != NULL, "Convenience API returned NULL");

    // String should be non-empty
    fail_unless(strlen(yaml_str) > 0, "YAML output is empty");

    // Free using convenience API
    metac_free_yaml_string(yaml_str);
    metac_value_delete(p_val);

} END_TEST

/**
 * @brief Test: Context API measure + to_buffer
 */
METAC_START_TEST(yaml_context_api_workflow) {
    metac_value_t *p_val = METAC_VALUE_FROM_LINK(yaml_test_struct_v);
    fail_unless(p_val != NULL, "failed to obtain struct value");

    // Create context
    metac_yaml_serialization_t *p_serial =
        metac_yaml_serialization_new(p_val, METAC_WMODE_shallow);
    fail_unless(p_serial != NULL, "Failed to create context");

    // Step 1: Measure
    size_t required_size = 0;
    int ret = metac_yaml_serialization_measure(p_serial, &required_size);
    fail_unless(ret == 0, "measure() failed");
    fail_unless(required_size > 0, "Measured size should be > 0");

    // Step 2: Allocate buffer
    char *buf = (char *)malloc(required_size);
    fail_unless(buf != NULL, "Failed to allocate buffer");

    // Step 3: Serialize to buffer
    ret = metac_yaml_serialization_to_buffer(p_serial, buf, required_size);
    fail_unless(ret == 0, "to_buffer() failed");

    // Step 4: Verify output
    fail_unless(strlen(buf) > 0, "Output is empty");
    fail_unless(buf[required_size - 1] == '\0' || buf[strlen(buf)] == '\0',
        "Output not properly null-terminated");

    // Cleanup
    free(buf);
    metac_yaml_serialization_delete(p_serial);
    metac_value_delete(p_val);

} END_TEST 
/**
 * @brief Test: NULL pointer safety
 */
METAC_START_TEST(yaml_null_safety) {
    // Create context with NULL value
    metac_yaml_serialization_t *p_serial = 
        metac_yaml_serialization_new(NULL, METAC_WMODE_shallow);
    fail_unless(p_serial == NULL, "Should fail with NULL value");
    
    // Convenience API with NULL
    char *yaml_str = metac_value_to_yaml_string(NULL, METAC_WMODE_shallow);
    fail_unless(yaml_str == NULL, "Convenience API should return NULL for NULL value");
    
    // Free NULL (should be safe)
    metac_free_yaml_string(NULL);  // Should not crash
    
    // Delete NULL (should be safe)
    metac_yaml_serialization_delete(NULL);  // Should not crash
    
} END_TEST

/**
 * @brief Test: Iterator caching efficiency
 *
 * Verifies that to_buffer() reuses iterator from measure() phase.
 * This tests the KEY OPTIMIZATION that eliminates double-traversal.
 */
METAC_START_TEST(yaml_iterator_caching) {
    metac_value_t *p_val = METAC_VALUE_FROM_LINK(yaml_test_struct_v);
    fail_unless(p_val != NULL, "failed to obtain struct value");

    // Create context
    metac_yaml_serialization_t *p_serial =
        metac_yaml_serialization_new(p_val, METAC_WMODE_shallow);
    fail_unless(p_serial != NULL, "Failed to create context");

    // Initially no cached iterator
    fail_unless(metac_yaml_serialization_get_iterator(p_serial) != NULL,
        "Should create iterator on first access");

    // Measure
    size_t size1 = 0;
    int ret = metac_yaml_serialization_measure(p_serial, &size1);
    fail_unless(ret == 0, "measure() failed");

    // Get iterator after measure
    void *iter_after_measure = metac_yaml_serialization_get_iterator(p_serial);

    // to_buffer should reuse same iterator
    char buf[1024];
    ret = metac_yaml_serialization_to_buffer(p_serial, buf, sizeof(buf));
    fail_unless(ret == 0, "to_buffer() failed");

    // Verify iterator still same (cached)
    void *iter_after_buffer = metac_yaml_serialization_get_iterator(p_serial);
    fail_unless(iter_after_measure == iter_after_buffer,
        "Iterator should be cached and reused");

    metac_yaml_serialization_delete(p_serial);
    metac_value_delete(p_val);

} END_TEST

/**
 * @brief Test: Consistency - Both APIs produce identical output
 *
 * This is critical: verify that Context API and Convenience API
 * produce the same YAML output.
 */
METAC_START_TEST(yaml_api_consistency) {
    metac_value_t *p_val = METAC_VALUE_FROM_LINK(yaml_test_struct_v);
    fail_unless(p_val != NULL, "failed to obtain struct value");

    /* Get output via Convenience API */
    char *conv_output = metac_value_to_yaml_string(p_val, METAC_WMODE_shallow);
    fail_unless(conv_output != NULL, "Convenience API failed");

    /* Get output via Context API */
    metac_yaml_serialization_t *p_serial =
        metac_yaml_serialization_new(p_val, METAC_WMODE_shallow);
    fail_unless(p_serial != NULL, "Failed to create context");

    size_t size = 0;
    int ret = metac_yaml_serialization_measure(p_serial, &size);
    fail_unless(ret == 0, "measure() failed");

    char *ctx_output = (char *)malloc(size);
    fail_unless(ctx_output != NULL, "Failed to allocate buffer");

    ret = metac_yaml_serialization_to_buffer(p_serial, ctx_output, size);
    fail_unless(ret == 0, "to_buffer() failed");

    /* Verify outputs are identical */
    fail_unless(strcmp(conv_output, ctx_output) == 0,
        "Context API and Convenience API outputs differ!");

    /* Cleanup */
    free(ctx_output);
    metac_free_yaml_string(conv_output);
    metac_yaml_serialization_delete(p_serial);
    metac_value_delete(p_val);

} END_TEST

/* Additional sample globals for extended coverage */
char yaml_test_cstr[] = "hello\n";
METAC_GSYM_LINK(yaml_test_cstr);

char *yaml_test_ptrstr = (char *)"world";
METAC_GSYM_LINK(yaml_test_ptrstr);

int yaml_test_arr[3] = {1,2,3};
METAC_GSYM_LINK(yaml_test_arr);

float yaml_test_float = 3.14f;
METAC_GSYM_LINK(yaml_test_float);

double yaml_test_double = 2.71828;
METAC_GSYM_LINK(yaml_test_double);

#ifndef __cplusplus
double _Complex yaml_test_complex = 1.0 + 2.0*I;
METAC_GSYM_LINK(yaml_test_complex);
#endif

/**
 * @brief Test: Extended types coverage
 */
METAC_START_TEST(yaml_extended_types_coverage) {
    struct { metac_value_t *p; const char *name; } cases[] = {
        { METAC_VALUE_FROM_LINK(yaml_test_cstr), "cstr" },
        { METAC_VALUE_FROM_LINK(yaml_test_ptrstr), "ptrstr" },
        { METAC_VALUE_FROM_LINK(yaml_test_arr), "arr" },
        { METAC_VALUE_FROM_LINK(yaml_test_float), "float" },
        { METAC_VALUE_FROM_LINK(yaml_test_double), "double" },
#ifndef __cplusplus
        { METAC_VALUE_FROM_LINK(yaml_test_complex), "complex" },
#endif
    };

    for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); ++i) {
        metac_value_t *p = cases[i].p;
        fail_unless(p != NULL, "failed to create value for %s", cases[i].name);

        char *conv = metac_value_to_yaml_string(p, METAC_WMODE_shallow);
        fail_unless(conv != NULL, "convenience API returned NULL for %s", cases[i].name);

        metac_yaml_serialization_t *ctx = metac_yaml_serialization_new(p, METAC_WMODE_shallow);
        fail_unless(ctx != NULL, "failed to create context for %s", cases[i].name);
        size_t sz = 0;
        fail_unless(metac_yaml_serialization_measure(ctx, &sz) == 0, "measure failed for %s", cases[i].name);
        char *buf = malloc(sz);
        fail_unless(buf != NULL, "malloc failed");
        fail_unless(metac_yaml_serialization_to_buffer(ctx, buf, sz) == 0, "to_buffer failed for %s", cases[i].name);
        fail_unless(strcmp(conv, buf) == 0, "%s: outputs differ: %s vs %s", cases[i].name, conv, buf);
        free(buf);
        metac_yaml_serialization_delete(ctx);
        metac_free_yaml_string(conv);
        metac_value_delete(p);
    }
}

#endif // WITH_YAML
