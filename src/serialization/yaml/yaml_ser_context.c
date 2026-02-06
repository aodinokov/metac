/**
 * @file yaml_context.c
 * @brief Context lifecycle management for YAML serialization
 * 
 * Provides shared core functionality for managing the serialization context:
 * - Context allocation and deallocation
 * - Iterator lifecycle management
 * - Cache initialization
 * 
 * This is SHARED CODE used by both Context API and Convenience API.
 */

#include "metac/serialization/yaml.h"
#include "metac/reflect.h"
#include "metac/backend/iterator.h"
#include <stdlib.h>
#include <string.h>
#include <yaml.h>

/**
 * @brief Create new YAML serialization context
 * 
 * Allocates and initializes a new context for serializing a metac value to YAML.
 * The context holds:
 * - The value to be serialized
 * - The recursive iterator for traversal
 * - The cache for measuring and serialization phases
 * - The YAML emitter for event generation
 * 
 * @param p_value   Pointer to metac_value_t to serialize (must not be NULL)
 * @param walk_mode How to traverse the value (WALK_MODE_DEEP or WALK_MODE_SHALLOW)
 * 
 * @return New context on success, NULL on failure
 * 
 * Caller is responsible for freeing via metac_yaml_serialization_delete().
 */
metac_yaml_serialization_t* metac_yaml_serialization_new(
    metac_value_t *p_value,
    metac_value_walk_mode_t walk_mode)
{
    if (!p_value) {
        return NULL;
    }

    // Allocate context
    metac_yaml_serialization_t *p_serial = 
        (metac_yaml_serialization_t *)malloc(sizeof(*p_serial));
    
    if (!p_serial) {
        return NULL;
    }

    // Initialize context
    memset(p_serial, 0, sizeof(*p_serial));
    p_serial->p_value = p_value;
    p_serial->walk_mode = walk_mode;
    p_serial->measured = 0;  // Not measured yet
    p_serial->required_size = 0;
    p_serial->p_iterator_cached = NULL;

    // Initialize YAML emitter (will be configured in measure/to_buffer phases)
    if (!yaml_emitter_initialize(&p_serial->emitter)) {
        free(p_serial);
        return NULL;
    }

    return p_serial;
}

/**
 * @brief Delete YAML serialization context
 * 
 * Frees all resources associated with the context:
 * - The recursive iterator (if cached)
 * - The YAML emitter
 * - The context itself
 * 
 * Safe to call with NULL pointer.
 * 
 * @param p_serial  Context to free (can be NULL)
 */
void metac_yaml_serialization_delete(
    metac_yaml_serialization_t *p_serial)
{
    if (!p_serial) {
        return;
    }

    // Delete cached iterator if present
    if (p_serial->p_iterator_cached) {
        metac_recursive_iterator_free(p_serial->p_iterator_cached);
        p_serial->p_iterator_cached = NULL;
    }

    // Delete YAML emitter
    yaml_emitter_delete(&p_serial->emitter);

    // Free context
    free(p_serial);
}

/**
 * @brief Initialize or reuse cached iterator for traversal
 * 
 * This is a key optimization function:
 * - First call creates a new iterator
 * - Subsequent calls reuse the same iterator (without re-traversal)
 * 
 * Called by both measure() and to_buffer() phases.
 * The same iterator is reused to avoid double-traversal penalty.
 * 
 * @param p_serial  Context to initialize iterator for
 * 
 * @return Iterator for traversal, or NULL on failure
 */
metac_recursive_iterator_t* metac_yaml_serialization_get_iterator(
    metac_yaml_serialization_t *p_serial)
{
    if (!p_serial) {
        return NULL;
    }

    // If iterator already cached, return it
    if (p_serial->p_iterator_cached) {
        return p_serial->p_iterator_cached;
    }

    // Create new iterator for the value
    p_serial->p_iterator_cached = metac_new_recursive_iterator(p_serial->p_value);
    
    return p_serial->p_iterator_cached;
}

/**
 * @brief Check if context has been measured
 * 
 * Returns whether measure() has been called successfully.
 * Used to determine if to_buffer() can proceed.
 * 
 * @param p_serial  Context to check
 * 
 * @return 1 if measured, 0 otherwise
 */
int metac_yaml_serialization_is_measured(
    metac_yaml_serialization_t *p_serial)
{
    if (!p_serial) {
        return 0;
    }

    return p_serial->measured;
}

/**
 * @brief Mark context as measured and set required size
 * 
 * Called after successful measure() phase to record:
 * - That measurement was completed
 * - The required buffer size
 * 
 * @param p_serial  Context to update
 * @param size      Required buffer size (including null terminator)
 */
void metac_yaml_serialization_set_measured(
    metac_yaml_serialization_t *p_serial,
    size_t size)
{
    if (!p_serial) {
        return;
    }

    p_serial->measured = 1;
    p_serial->required_size = size;
}

/**
 * @brief Get required buffer size from measurement
 * 
 * Returns the buffer size needed for to_buffer() to succeed.
 * Only valid after measure() has been called.
 * 
 * @param p_serial  Context to query
 * 
 * @return Required size in bytes (0 if not measured)
 */
size_t metac_yaml_serialization_get_required_size(
    metac_yaml_serialization_t *p_serial)
{
    if (!p_serial) {
        return 0;
    }

    return p_serial->required_size;
}
