/**
 * @file yaml_deser_context.c
 * @brief Context lifecycle management for YAML deserialization
 *
 * Provides shared core functionality for managing the deserialization context:
 * - Context allocation and deallocation
 * - Parser lifecycle management
 * - Error message management
 *
 * This is SHARED CODE used by both Context API and Convenience API.
 */

#include "metac/serialization/yaml.h"
#include "metac/reflect.h"
#include <stdlib.h>
#include <string.h>
#include <yaml.h>

/**
 * @brief Create new YAML deserialization context
 *
 * Allocates and initializes a new context for deserializing a YAML string into a metac value.
 * The context holds:
 * - The target value to be populated
 * - The parser for reading YAML events
 * - Error tracking for diagnostic information
 *
 * @param p_value   Pointer to metac_value_t to populate (must not be NULL)
 * @param walk_mode How to traverse the value (WALK_MODE_DEEP or WALK_MODE_SHALLOW)
 *
 * @return New context on success, NULL on failure
 *
 * Caller is responsible for freeing via metac_yaml_deserialization_delete().
 */
metac_yaml_deserialization_t* metac_yaml_deserialization_new(
    metac_value_t *p_value,
    metac_value_walk_mode_t walk_mode)
{
    if (!p_value) {
        return NULL;
    }

    // Allocate context
    metac_yaml_deserialization_t *p_deser =
        (metac_yaml_deserialization_t *)malloc(sizeof(*p_deser));

    if (!p_deser) {
        return NULL;
    }

    // Initialize context
    memset(p_deser, 0, sizeof(*p_deser));
    p_deser->p_value = p_value;
    p_deser->walk_mode = walk_mode;
    p_deser->parsed = 0;  // Not parsed yet
    p_deser->error_code = 0;
    p_deser->p_error_message = NULL;

    // Initialize YAML parser
    if (!yaml_parser_initialize(&p_deser->parser)) {
        free(p_deser);
        return NULL;
    }

    return p_deser;
}

/**
 * @brief Delete YAML deserialization context
 *
 * Frees all resources associated with the context:
 * - The YAML parser
 * - The error message (if present)
 * - The context itself
 *
 * Safe to call with NULL pointer.
 *
 * @param p_deser  Context to free (can be NULL)
 */
void metac_yaml_deserialization_delete(
    metac_yaml_deserialization_t *p_deser)
{
    if (!p_deser) {
        return;
    }

    // Delete parser
    yaml_parser_delete(&p_deser->parser);

    // Free error message if present
    if (p_deser->p_error_message) {
        free(p_deser->p_error_message);
        p_deser->p_error_message = NULL;
    }

    // Free context
    free(p_deser);
}

/**
 * @brief Get error details from deserialization failure
 *
 * Returns the human-readable error message from the last deserialization attempt.
 * Valid only after a failed metac_yaml_deserialization_from_string() call.
 *
 * @param p_deser  Context to query
 *
 * @return Error message string, or empty string if no error
 */
const char* metac_yaml_deserialization_get_error(
    metac_yaml_deserialization_t *p_deser)
{
    if (!p_deser) {
        return "";
    }

    if (p_deser->p_error_message) {
        return p_deser->p_error_message;
    }

    return "";
}

/**
 * @brief Set error message in context
 *
 * Internal helper to record an error message. Frees previous message if present.
 *
 * @param p_deser   Context to update
 * @param p_message Error message to set (will be copied)
 * @param error_code Numeric error code
 */
void metac_yaml_deserialization_set_error(
    metac_yaml_deserialization_t *p_deser,
    const char *p_message,
    int error_code)
{
    if (!p_deser) {
        return;
    }

    // Free previous error message
    if (p_deser->p_error_message) {
        free(p_deser->p_error_message);
        p_deser->p_error_message = NULL;
    }

    // Copy new error message
    if (p_message) {
        size_t len = strlen(p_message);
        p_deser->p_error_message = (char *)malloc(len + 1);
        if (p_deser->p_error_message) {
            strcpy(p_deser->p_error_message, p_message);
        }
    }

    p_deser->error_code = error_code;
}

/**
 * @brief Check if deserialization has been completed
 *
 * Returns whether from_string() has been called successfully.
 *
 * @param p_deser  Context to check
 *
 * @return 1 if parsed, 0 otherwise
 */
int metac_yaml_deserialization_is_parsed(
    metac_yaml_deserialization_t *p_deser)
{
    if (!p_deser) {
        return 0;
    }

    return p_deser->parsed;
}
