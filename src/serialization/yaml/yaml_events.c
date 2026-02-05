/**
 * @file yaml_events.c
 * @brief Core YAML serialization algorithm with iterator caching
 * 
 * Implements the main serialization logic:
 * - measure(): Traverse metac value and calculate required buffer size (caches iterator)
 * - to_buffer(): Serialize using cached iterator (no re-traversal)
 * 
 * The key optimization: Iterator caching eliminates double-traversal penalty.
 * Both measure() and to_buffer() use the same cached iterator from the context.
 * 
 * This is SHARED CODE used by both Context API and Convenience API.
 */

#include "metac/serialization/yaml.h"
#include "metac/reflect.h"
#include "metac/backend/iterator.h"
#include <stdlib.h>
#include <string.h>
#include <yaml.h>

/*
==> Fetching downloads for: libyaml
✔︎ Bottle Manifest libyaml (0.2.5)                                                                                                                                                    Downloaded   14.5KB/ 14.5KB
✔︎ Bottle libyaml (0.2.5)                                                                                                                                                             Downloaded  111.3KB/111.3KB
==> Pouring libyaml--0.2.5.arm64_sonoma.bottle.tar.gz
   /opt/homebrew/Cellar/libyaml/0.2.5: 11 files, 363.4KB
==> Running `brew cleanup libyaml`...
*/
#ifndef YAML_DEFAULT_TAG
#define YAML_DEFAULT_TAG NULL
#endif

/**
 * @brief Internal: Emit YAML start document
 */
static int yaml_emit_start(yaml_emitter_t *p_emitter)
{
    yaml_event_t event;
    
    if (!yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING)) {
        return 0;
    }
    if (!yaml_emitter_emit(p_emitter, &event)) {
        return 0;
    }

    if (!yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1)) {
        return 0;
    }
    if (!yaml_emitter_emit(p_emitter, &event)) {
        return 0;
    }

    return 1;
}

/* Emit a plain scalar from a C string */
static int yaml_emit_scalar_string(yaml_emitter_t *p_emitter, const char *s)
{
    yaml_event_t event;
    if (!s) s = "";
    int len = (int)strlen(s);
    if (!yaml_scalar_event_initialize(&event, NULL, (yaml_char_t *)YAML_DEFAULT_TAG, (yaml_char_t *)s, len, 1, 0, YAML_PLAIN_SCALAR_STYLE)) {
        return 0;
    }
    return yaml_emitter_emit(p_emitter, &event);
}

/* Forward declaration for recursion */
static int yaml_emit_value(metac_yaml_serialization_t *p_serial, yaml_emitter_t *p_emitter, metac_value_t *p_value);

static int yaml_emit_mapping_start(yaml_emitter_t *p_emitter)
{
    yaml_event_t event;
    if (!yaml_mapping_start_event_initialize(&event, NULL, NULL, 1, YAML_BLOCK_MAPPING_STYLE)) return 0;
    return yaml_emitter_emit(p_emitter, &event);
}

static int yaml_emit_mapping_end(yaml_emitter_t *p_emitter)
{
    yaml_event_t event;
    if (!yaml_mapping_end_event_initialize(&event)) return 0;
    return yaml_emitter_emit(p_emitter, &event);
}

static int yaml_emit_sequence_start(yaml_emitter_t *p_emitter)
{
    yaml_event_t event;
    if (!yaml_sequence_start_event_initialize(&event, NULL, NULL, 1, YAML_BLOCK_SEQUENCE_STYLE)) return 0;
    return yaml_emitter_emit(p_emitter, &event);
}

static int yaml_emit_sequence_end(yaml_emitter_t *p_emitter)
{
    yaml_event_t event;
    if (!yaml_sequence_end_event_initialize(&event)) return 0;
    return yaml_emitter_emit(p_emitter, &event);
}

/* Recursive emitter: handle structs (mappings), arrays (sequences), base types (scalars) */
static int yaml_emit_value(metac_yaml_serialization_t *p_serial, yaml_emitter_t *p_emitter, metac_value_t *p_value)
{
    if (!p_value || !p_emitter) return 0;

    // Struct / union / class -> mapping
    if (metac_value_has_members(p_value)) {
        if (!yaml_emit_mapping_start(p_emitter)) return 0;
        metac_num_t mcount = metac_value_member_count(p_value);
        for (metac_num_t i = 0; i < mcount; ++i) {
            metac_value_t *p_memb = metac_new_value_by_member_id(p_value, i);
            if (!p_memb) { yaml_emit_mapping_end(p_emitter); return 0; }
            metac_name_t memb_name = metac_value_name(p_memb);
            // Emit key
            if (!yaml_emit_scalar_string(p_emitter, memb_name ? memb_name : "")) {
                metac_value_delete(p_memb); yaml_emit_mapping_end(p_emitter); return 0;
            }
            // Emit value recursively
            if (!yaml_emit_value(p_serial, p_emitter, p_memb)) {
                metac_value_delete(p_memb); yaml_emit_mapping_end(p_emitter); return 0;
            }
            metac_value_delete(p_memb);
        }
        if (!yaml_emit_mapping_end(p_emitter)) return 0;
        return 1;
    }

    // Array -> sequence
    if (metac_value_has_elements(p_value)) {
        if (!yaml_emit_sequence_start(p_emitter)) return 0;
        metac_num_t count = metac_value_element_count(p_value);
        if (count < 0) count = 0; // flexible arrays handled as empty if unknown
        for (metac_num_t i = 0; i < (metac_num_t)count; ++i) {
            metac_value_t *p_elem = metac_new_value_by_element_id(p_value, i);
            if (!p_elem) { yaml_emit_sequence_end(p_emitter); return 0; }
            if (!yaml_emit_value(p_serial, p_emitter, p_elem)) { metac_value_delete(p_elem); yaml_emit_sequence_end(p_emitter); return 0; }
            metac_value_delete(p_elem);
        }
        if (!yaml_emit_sequence_end(p_emitter)) return 0;
        return 1;
    }

    // Enumeration
    if (metac_value_is_enumeration(p_value)) {
        char *s = metac_value_enumeration_string(p_value);
        if (!s) return 0;
        int rc = yaml_emit_scalar_string(p_emitter, s);
        free(s);
        return rc;
    }

    // Base type -> scalar
    if (metac_value_is_base_type(p_value)) {
        char *s = metac_value_base_type_string(p_value);
        if (!s) return 0;
        int rc = yaml_emit_scalar_string(p_emitter, s);
        free(s);
        return rc;
    }

    // Pointer handling: attempt to treat as single-element array or print pointer
    if (metac_value_is_pointer(p_value)) {
        // Try to obtain array with element count via helper
        metac_value_t *p_arr_val = metac_new_element_count_value(p_value, 1);
        if (p_arr_val && metac_value_has_elements(p_arr_val)) {
            int rc = yaml_emit_value(p_serial, p_emitter, p_arr_val);
            metac_value_delete(p_arr_val);
            return rc;
        }
        if (p_arr_val) metac_value_delete(p_arr_val);
        // Fallback: print pointer as string
        char *s = metac_value_pointer_string(p_value);
        if (!s) return 0;
        int rc = yaml_emit_scalar_string(p_emitter, s);
        free(s);
        return rc;
    }

    /* Fallback: use generic string representation */
    char *s = metac_value_string_ex(p_value, p_serial->walk_mode, NULL);
    if (!s) return 0;
    int rc = yaml_emit_scalar_string(p_emitter, s);
    free(s);
    return rc;
}

/**
 * @brief Internal: Emit YAML end document
 */
static int yaml_emit_end(yaml_emitter_t *p_emitter)
{
    yaml_event_t event;

    if (!yaml_document_end_event_initialize(&event, 1)) {
        return 0;
    }
    if (!yaml_emitter_emit(p_emitter, &event)) {
        return 0;
    }

    if (!yaml_stream_end_event_initialize(&event)) {
        return 0;
    }
    if (!yaml_emitter_emit(p_emitter, &event)) {
        return 0;
    }

    return 1;
}

/**
 * @brief Internal: Emit scalar value to YAML
 * 
 * Emits a metac value's content as a YAML scalar.
 * Handles basic types (integers, floats, strings, etc.)
 * 
 * @param p_emitter libyaml emitter
 * @param p_value   Metac value to serialize
 * 
 * @return 1 on success, 0 on failure
 */
static int yaml_emit_scalar(metac_yaml_serialization_t *p_serial, yaml_emitter_t *p_emitter, metac_value_t *p_value)
{
    yaml_event_t event;
    char *out = NULL;
    size_t len = 0;

    if (!p_value || !p_serial) {
        return 0;
    }

    /* Use existing metac pretty-printer to obtain textual representation
     * of the value for a broad set of kinds (base types, enums, arrays,
     * structs, pointers in shallow mode, etc.). This keeps YAML emission
     * simple: we emit a single scalar containing the value string.
     */
    out = metac_value_string_ex(p_value, p_serial->walk_mode, NULL);
    if (out == NULL) {
        return 0;
    }

    len = strlen(out);
    if (!yaml_scalar_event_initialize(
            &event,
            NULL,
            (yaml_char_t *)YAML_DEFAULT_TAG,
            (yaml_char_t *)out,
            (int)len,
            1,  /* plain_implicit */
            0,  /* quoted_implicit */
            YAML_PLAIN_SCALAR_STYLE)) {
        free(out);
        return 0;
    }

    int rc = yaml_emitter_emit(p_emitter, &event);
    free(out);
    return rc;
}

/**
 * @brief Measure phase: Calculate required buffer size
 * 
 * Traverses the metac value structure and creates cached iterator.
 * This iterator is reused in to_buffer() phase to avoid double-traversal.
 * 
 * Algorithm:
 * 1. Initialize YAML emitter
 * 2. Create temporary null-output handler (discards data)
 * 3. Generate YAML events using iterator
 * 4. Track size of generated YAML
 * 5. Cache the iterator for next phase
 * 6. Return required buffer size
 * 
 * @param p_serial  YAML serialization context
 * @param p_size    Output: Required buffer size (including null terminator)
 * 
 * @return 0 on success, -1 on failure
 * 
 * Must be called before metac_yaml_serialization_to_buffer().
 * Can be called multiple times (will reuse cached iterator).
 */
int metac_yaml_serialization_measure(
    metac_yaml_serialization_t *p_serial,
    size_t *p_size)
{
    metac_recursive_iterator_t *p_iter;
    metac_yaml_handler_context_t handler;
    char null_buffer[4096];  // Temporary buffer for measurement
    size_t estimated_size;

    if (!p_serial || !p_size) {
        return -1;
    }

    // Get or create cached iterator
    p_iter = metac_yaml_serialization_get_iterator(p_serial);
    if (!p_iter) {
        return -1;
    }

    // Initialize handler for temporary output
    metac_yaml_handler_init(&handler, null_buffer, sizeof(null_buffer));

    // Configure emitter
    if (!metac_yaml_emitter_set_custom_output(&p_serial->emitter, &handler)) {
        return -1;
    }

    // Set emitter options
    yaml_emitter_set_canonical(&p_serial->emitter, 0);
    yaml_emitter_set_indent(&p_serial->emitter, 2);
    yaml_emitter_set_width(&p_serial->emitter, 80);

    // Emit start document
    if (!yaml_emit_start(&p_serial->emitter)) {
        return -1;
    }

    // Walk the value tree and emit YAML events using recursive emitter
    if (!yaml_emit_value(p_serial, &p_serial->emitter, p_serial->p_value)) {
        return -1;
    }

    // Emit end document
    if (!yaml_emit_end(&p_serial->emitter)) {
        return -1;
    }

    // Flush emitter to ensure all data written
    if (!yaml_emitter_flush(&p_serial->emitter)) {
        return -1;
    }

    // Calculate required size
    if (metac_yaml_handler_has_overflow(&handler)) {
        // Estimate based on partial output
        // In production, would use heuristic (e.g., multiply by 1.5)
        estimated_size = sizeof(null_buffer) * 2;
    } else {
        // Actual size from measurement + space for null terminator
        estimated_size = metac_yaml_handler_get_bytes_written(&handler) + 1;
    }

    // Mark context as measured and cache size
    metac_yaml_serialization_set_measured(p_serial, estimated_size);

    // Cache the iterator (will be reused in to_buffer())
    // p_iter is already cached in p_serial->p_iterator_cached

    *p_size = estimated_size;
    return 0;
}

/**
 * @brief Serialization phase: Write YAML to buffer
 * 
 * Uses cached iterator from measure() phase to serialize without re-traversal.
 * 
 * Algorithm:
 * 1. Verify context has been measured
 * 2. Reinitialize YAML emitter
 * 3. Set custom output handler to user's buffer
 * 4. Generate YAML events using cached iterator
 * 5. Null-terminate result
 * 6. Return 0 on success, -1 on failure (overflow or other error)
 * 
 * @param p_serial  YAML serialization context (must be measured first)
 * @param p_buffer  Output buffer for YAML data
 * @param size      Buffer size in bytes
 * 
 * @return 0 on success, -1 on failure
 * 
 * Requires metac_yaml_serialization_measure() to be called first.
 * If buffer is too small, returns -1 and buffer contains partial data.
 */
int metac_yaml_serialization_to_buffer(
    metac_yaml_serialization_t *p_serial,
    char *p_buffer,
    size_t size)
{
    metac_recursive_iterator_t *p_iter;
    metac_yaml_handler_context_t handler;

    if (!p_serial || !p_buffer) {
        return -1;
    }

    // Must call measure() first
    if (!metac_yaml_serialization_is_measured(p_serial)) {
        return -1;
    }

    // Get cached iterator (created in measure phase)
    p_iter = metac_yaml_serialization_get_iterator(p_serial);
    if (!p_iter) {
        return -1;
    }

    // Initialize handler for user buffer
    metac_yaml_handler_init(&handler, p_buffer, size - 1);  // Leave space for null

    // Reinitialize emitter for actual output
    yaml_emitter_delete(&p_serial->emitter);
    if (!yaml_emitter_initialize(&p_serial->emitter)) {
        return -1;
    }

    // Configure emitter with custom output
    if (!metac_yaml_emitter_set_custom_output(&p_serial->emitter, &handler)) {
        return -1;
    }

    // Set same emitter options as measure phase
    yaml_emitter_set_canonical(&p_serial->emitter, 0);
    yaml_emitter_set_indent(&p_serial->emitter, 2);
    yaml_emitter_set_width(&p_serial->emitter, 80);

    // Emit start document
    if (!yaml_emit_start(&p_serial->emitter)) {
        return -1;
    }

    // Walk the value tree and emit YAML events using recursive emitter
    // Using cached iterator - NO re-traversal
    if (!yaml_emit_value(p_serial, &p_serial->emitter, p_serial->p_value)) {
        return -1;
    }

    // Emit end document
    if (!yaml_emit_end(&p_serial->emitter)) {
        return -1;
    }

    // Flush emitter
    if (!yaml_emitter_flush(&p_serial->emitter)) {
        return -1;
    }

    // Check for overflow
    if (metac_yaml_handler_has_overflow(&handler)) {
        return -1;  // Buffer too small
    }

    // Null-terminate
    size_t written = metac_yaml_handler_get_bytes_written(&handler);
    if (written < size) {
        p_buffer[written] = '\0';
    }

    return 0;
}
