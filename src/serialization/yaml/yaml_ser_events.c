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
#include "metac/const.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
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

    /* Check if this is a base type using final_kind to skip wrappers */
    metac_kind_t kind = metac_value_final_kind(p_value, NULL);

    if (kind == METAC_KND_base_type) {
        out = metac_value_base_type_string(p_value);
    } else if (kind == METAC_KND_enumeration_type) {
        out = metac_value_enumeration_string(p_value);
    } else {
        out = metac_value_string_ex(p_value, p_serial->walk_mode, NULL);
    }

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

/* Iterative emitter using metac_recursive_iterator */
static int yaml_run_iterator(metac_yaml_serialization_t *p_serial, yaml_emitter_t *p_emitter)
{
    metac_recursive_iterator_t *p_iter = metac_yaml_serialization_get_iterator(p_serial);
    if (!p_iter) return 0;

    for (metac_value_t *p = (metac_value_t *)metac_recursive_iterator_next(p_iter);
         p != NULL;
         p = (metac_value_t *)metac_recursive_iterator_next(p_iter)) {
        
        int state = metac_recursive_iterator_get_state(p_iter);
        metac_kind_t kind = metac_value_final_kind(p, NULL);

        if (state == METAC_R_ITER_start) {
            /* Emit Key if this is a member of a struct (and not the root object)
             * BUT: Do NOT emit for array elements - only for struct members */
            if (p != p_serial->p_value) {
                /* Check if parent is an array - if so, don't emit element name */
                metac_value_t *p_parent = (metac_value_t *)metac_recursive_iterator_get_in(p_iter, 1);
                int is_array_element = (p_parent != NULL &&
                    metac_value_final_kind(p_parent, NULL) == METAC_KND_array_type);

                if (!is_array_element) {
                    /* Emit name for struct members only */
                    metac_name_t name = metac_value_name(p);
                    if (name) {
                        if (!yaml_emit_scalar_string(p_emitter, name)) {
                            metac_recursive_iterator_fail(p_iter);
                            continue;
                        }
                    }
                }
            }

            /* Emit Value Start */
            /* Check if we should recurse based on walk_mode */
            int should_recurse = 1;
            if (p_serial->walk_mode == METAC_WMODE_shallow && p != p_serial->p_value) {
                /* In shallow mode, skip recursion for complex types (except root) */
                kind = metac_value_final_kind(p, NULL);
                if (kind != METAC_KND_base_type && kind != METAC_KND_enumeration_type &&
                    kind != METAC_KND_pointer_type) {
                    should_recurse = 0;
                }
            }

            if (should_recurse && metac_value_has_members(p)) { /* Struct/Union */
                if (!yaml_emit_mapping_start(p_emitter)) {
                    metac_recursive_iterator_fail(p_iter);
                    continue;
                }

                /* Push children */
                metac_num_t mcount = metac_value_member_count(p);
                for (metac_num_t i = 0; i < mcount; ++i) {
                    metac_value_t *p_memb = metac_new_value_by_member_id(p, i);
                    if (p_memb) {
                        metac_recursive_iterator_create_and_append_dep(p_iter, p_memb);
                    }
                }
                metac_recursive_iterator_set_state(p_iter, 1);

            } else if (should_recurse && metac_value_final_kind(p, NULL) == METAC_KND_array_type) { /* Array */
                if (!yaml_emit_sequence_start(p_emitter)) {
                    metac_recursive_iterator_fail(p_iter);
                    continue;
                }

                /* Push elements */
                metac_num_t count = metac_value_element_count(p);
                if (count < 0) count = 0; /* flexible array unknown size */
                for (metac_num_t i = 0; i < count; ++i) {
                    metac_value_t *p_elem = metac_new_value_by_element_id(p, i);
                    if (p_elem) {
                        metac_recursive_iterator_create_and_append_dep(p_iter, p_elem);
                    }
                }
                metac_recursive_iterator_set_state(p_iter, 1);

            } else { /* Scalar */
                if (!yaml_emit_scalar(p_serial, p_emitter, p)) {
                    metac_recursive_iterator_fail(p_iter);
                    continue;
                }
                metac_recursive_iterator_done(p_iter, NULL);
            }
        } else if (state == 1) {
            /* End of container */
            
            /* Dequeue dependencies (we don't use their return values) */
            while (!metac_recursive_iterator_dep_queue_is_empty(p_iter)) {
                metac_value_t *p_dep_val = NULL;
                metac_recursive_iterator_dequeue_and_delete_dep(p_iter, (void**)&p_dep_val, NULL);
                if (p_dep_val) metac_value_delete(p_dep_val);
            }

            if (metac_value_has_members(p)) {
                if (!yaml_emit_mapping_end(p_emitter)) {
                    metac_recursive_iterator_fail(p_iter);
                    continue;
                }
            } else if (metac_value_final_kind(p, NULL) == METAC_KND_array_type) {
                if (!yaml_emit_sequence_end(p_emitter)) {
                    metac_recursive_iterator_fail(p_iter);
                    continue;
                }
            }
            metac_recursive_iterator_done(p_iter, NULL);
        }
    }

    int fail = 0;
    metac_recursive_iterator_get_out(p_iter, NULL, &fail);
    /* Note: we don't free p_iter here, it's owned by context */
    return (fail == 0) ? 1 : 0;
}

/**
 * @brief Measure phase: Calculate required buffer size
 *
 * Returns a conservative size estimate without consuming the iterator.
 * The actual iterator is reserved for the to_buffer() phase to emit YAML.
 *
 * @param p_serial  YAML serialization context
 * @param p_size    Output: Required buffer size (including null terminator)
 *
 * @return 0 on success, -1 on failure
 */
int metac_yaml_serialization_measure(
    metac_yaml_serialization_t *p_serial,
    size_t *p_size)
{
    size_t estimated_size;

    if (!p_serial || !p_size) {
        return -1;
    }

    // Conservative size estimate for YAML output
    // We estimate based on the metac value's string representation
    // rather than fully iterating, to preserve the iterator for to_buffer()
    char *str = metac_value_string_ex(p_serial->p_value, p_serial->walk_mode, NULL);
    if (str) {
        // YAML typically expands due to structure (mappings, sequences, indentation)
        // Conservative multiplier: 4x the input string length
        estimated_size = strlen(str) * 4 + 256;  // +256 for YAML overhead
        free(str);
    } else {
        // Fallback safe estimate
        estimated_size = 4096;
    }

    // Mark context as measured and cache size
    metac_yaml_serialization_set_measured(p_serial, estimated_size);

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
    metac_yaml_handler_context_t handler;

    if (!p_serial || !p_buffer) {
        return -1;
    }

    // Must call measure() first
    if (!metac_yaml_serialization_is_measured(p_serial)) {
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

    // Walk the value tree and emit YAML events using iterative emitter
    if (!yaml_run_iterator(p_serial, &p_serial->emitter)) {
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
