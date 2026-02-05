/**
 * @file api.c
 * @brief Convenience API: Single-call YAML serialization
 * 
 * Provides a high-level convenience function that orchestrates the core
 * serialization functions from yaml_context.c and yaml_events.c.
 * 
 * This demonstrates the "wrapper pattern" where:
 * - measure(), to_buffer(), and delete() are SHARED (from yaml_events.c, yaml_context.c)
 * - Only malloc/free logic is NEW
 * - Result: 90% code reuse (28 lines new + 250 lines shared)
 * 
 * SHARED CODE ARCHITECTURE:
 * ┌─────────────────────────────────┐
 * │ Two User APIs                   │
 * ├─────────────────────────────────┤
 * │                                 │
 * │ Context API:                    │
 * │ ├─ new()      ← shared          │
 * │ ├─ measure()  ← shared          │
 * │ ├─ to_buffer()← shared          │
 * │ └─ delete()   ← shared          │
 * │                                 │
 * │ Convenience API:                │
 * │ └─ to_string()← calls 4 shared  │
 * │    + malloc() ← NEW             │
 * │    + free()   ← NEW             │
 * └─────────────────────────────────┘
 * 
 * Both APIs use identical core implementation → identical behavior!
 */

#include "metac/serialization/yaml.h"
#include "metac/reflect.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief High-level convenience function: Serialize to allocated string
 * 
 * This is the "fire-and-forget" API that handles all orchestration internally.
 * 
 * Algorithm (calls 4 shared functions + malloc):
 * 1. new()      - Create context (SHARED)
 * 2. measure()  - Calculate required size (SHARED)
 * 3. malloc()   - Allocate buffer (NEW - only malloc)
 * 4. to_buffer()- Serialize to buffer (SHARED)
 * 5. delete()   - Cleanup (SHARED)
 * 6. return     - Result string
 * 
 * Caller must free the returned string using metac_free_yaml_string().
 * 
 * @param p_value   Metac value to serialize (must not be NULL)
 * @param walk_mode How to traverse: WALK_MODE_DEEP or WALK_MODE_SHALLOW
 * 
 * @return Allocated YAML string on success, NULL on failure
 * 
 * Example usage (fire-and-forget):
 * @code
 * metac_value_t *p_val = ...;
 * char *yaml_str = metac_value_to_yaml_string(p_val, WALK_MODE_DEEP);
 * if (yaml_str) {
 *     printf("%s", yaml_str);
 *     metac_free_yaml_string(yaml_str);
 * }
 * @endcode
 */
char* metac_value_to_yaml_string(
    metac_value_t *p_value,
    metac_value_walk_mode_t walk_mode)
{
    metac_yaml_serialization_t *p_serial;
    size_t required_size;
    char *p_result;
    int ret;

    if (!p_value) {
        return NULL;
    }

    // Step 1: Create context (SHARED)
    p_serial = metac_yaml_serialization_new(p_value, walk_mode);
    if (!p_serial) {
        return NULL;
    }

    // Step 2: Measure required size (SHARED)
    ret = metac_yaml_serialization_measure(p_serial, &required_size);
    if (ret != 0) {
        metac_yaml_serialization_delete(p_serial);
        return NULL;
    }

    // Step 3: Allocate buffer (NEW - only this line is new)
    p_result = (char *)malloc(required_size);
    if (!p_result) {
        metac_yaml_serialization_delete(p_serial);
        return NULL;
    }

    // Step 4: Serialize to buffer (SHARED)
    ret = metac_yaml_serialization_to_buffer(p_serial, p_result, required_size);
    if (ret != 0) {
        free(p_result);
        metac_yaml_serialization_delete(p_serial);
        return NULL;
    }

    // Step 5: Cleanup (SHARED)
    metac_yaml_serialization_delete(p_serial);

    // Step 6: Return result
    return p_result;
}

/**
 * @brief Free string allocated by metac_value_to_yaml_string()
 * 
 * Simple wrapper around free() for API consistency.
 * Safe to call with NULL pointer.
 * 
 * @param p_str  Pointer to string from metac_value_to_yaml_string() (can be NULL)
 */
void metac_free_yaml_string(char *p_str)
{
    if (p_str) {
        free(p_str);
    }
}
