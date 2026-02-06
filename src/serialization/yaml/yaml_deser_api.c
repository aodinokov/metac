/**
 * @file yaml_deser_api.c
 * @brief Convenience API: Single-call YAML deserialization
 *
 * Provides a high-level convenience function that orchestrates the core
 * deserialization functions from yaml_deser_context.c and yaml_deser_events.c.
 *
 * This demonstrates the "wrapper pattern" where:
 * - from_string() is SHARED (from yaml_deser_events.c)
 * - new() and delete() are SHARED (from yaml_deser_context.c)
 * - Only function call orchestration is NEW
 * - Result: 90% code reuse (5 lines new + 250 lines shared)
 */

#include "metac/serialization/yaml.h"
#include "metac/reflect.h"
#include <stdlib.h>

/**
 * @brief High-level convenience function: Deserialize from YAML string
 *
 * This is the "fire-and-forget" API that handles all orchestration internally.
 *
 * Algorithm (calls 3 shared functions):
 * 1. new()        - Create context (SHARED)
 * 2. from_string()- Parse YAML into value (SHARED)
 * 3. delete()     - Cleanup (SHARED)
 *
 * Caller provides target metac_value_t with type information.
 *
 * @param p_value      Metac value to populate (must not be NULL, must have type info)
 * @param p_yaml_string YAML content to deserialize
 * @param walk_mode    How to traverse: WALK_MODE_DEEP or WALK_MODE_SHALLOW
 *
 * @return 0 on success, -1 on failure
 *
 * Example usage (fire-and-forget):
 * @code
 * metac_value_t *p_target = ...;  // Already has type info
 * const char *yaml_str = "a: 42\nb: k";
 * int ret = metac_value_from_yaml_string(p_target, yaml_str, WALK_MODE_SHALLOW);
 * if (ret != 0) {
 *     printf("Error: %s", error_msg);
 * }
 * @endcode
 */
int metac_value_from_yaml_string(
    metac_value_t *p_value,
    const char *p_yaml_string,
    metac_value_walk_mode_t walk_mode)
{
    metac_yaml_deserialization_t *p_deser;
    int ret;

    if (!p_value || !p_yaml_string) {
        return -1;
    }

    // Step 1: Create context (SHARED)
    p_deser = metac_yaml_deserialization_new(p_value, walk_mode);
    if (!p_deser) {
        return -1;
    }

    // Step 2: Parse YAML string (SHARED)
    ret = metac_yaml_deserialization_from_string(
        p_deser,
        p_yaml_string,
        strlen(p_yaml_string));

    // Step 3: Cleanup (SHARED)
    metac_yaml_deserialization_delete(p_deser);

    return ret;
}
