/**
 * @file yaml_handler.c
 * @brief Custom libyaml write handler for zero-malloc serialization
 * 
 * Provides the write callback for libyaml's emitter that copies events
 * directly to user-controlled buffer without internal buffering.
 * 
 * This is SHARED CODE used by both Context API and Convenience API.
 */

#include "metac/serialization/yaml.h"
#include <string.h>
#include <stdio.h>

/**
 * @brief Custom write handler for libyaml emitter
 * 
 * This callback is called by libyaml when it needs to output YAML data.
 * Instead of buffering internally, it directly copies to user buffer.
 * 
 * Handler behavior:
 * - Copies data to buffer at current position
 * - Tracks position in context
 * - Detects overflow and sets error flag
 * - Returns 1 on success, 0 on overflow
 * 
 * Called directly by yaml_emitter_flush() during serialization.
 * 
 * @param data      User-defined data (metac_yaml_handler_context_t*)
 * @param buffer    Pointer to data to write
 * @param size      Number of bytes to write
 * 
 * @return 1 on success, 0 on error (overflow)
 */
int metac_yaml_write_handler(
    void *data,
    unsigned char *buffer,
    size_t size)
{
    if (!data || !buffer) {
        return 0;
    }

    metac_yaml_handler_context_t *p_ctx = 
        (metac_yaml_handler_context_t *)data;

    // Check for overflow
    if (p_ctx->pos + size > p_ctx->capacity) {
        // Buffer overflow - mark error and stop
        p_ctx->overflow = 1;
        return 0;
    }

    // Copy data to buffer
    memcpy(p_ctx->p_buffer + p_ctx->pos, buffer, size);
    p_ctx->pos += size;

    return 1;  // Success
}

/**
 * @brief Initialize write handler context
 * 
 * Sets up the context that will be passed to yaml_write_handler.
 * This context tracks:
 * - Destination buffer and capacity
 * - Current write position
 * - Overflow status
 * 
 * Called before yaml_emitter_set_output().
 * 
 * @param p_handler     Handler context to initialize
 * @param p_buffer      Destination buffer
 * @param capacity      Buffer capacity in bytes
 */
void metac_yaml_handler_init(
    metac_yaml_handler_context_t *p_handler,
    char *p_buffer,
    size_t capacity)
{
    if (!p_handler) {
        return;
    }

    p_handler->p_buffer = p_buffer;
    p_handler->capacity = capacity;
    p_handler->pos = 0;
    p_handler->overflow = 0;
}

/**
 * @brief Check if overflow occurred during writing
 * 
 * Returns whether the output buffer was too small.
 * If overflow occurred, the buffer contains partial data.
 * 
 * @param p_handler  Handler context to check
 * 
 * @return 1 if overflow, 0 otherwise
 */
int metac_yaml_handler_has_overflow(
    metac_yaml_handler_context_t *p_handler)
{
    if (!p_handler) {
        return 0;
    }

    return p_handler->overflow;
}

/**
 * @brief Get bytes written so far
 * 
 * Returns the number of bytes successfully written to buffer.
 * If overflow occurred, this is the partial output.
 * 
 * @param p_handler  Handler context to query
 * 
 * @return Number of bytes written
 */
size_t metac_yaml_handler_get_bytes_written(
    metac_yaml_handler_context_t *p_handler)
{
    if (!p_handler) {
        return 0;
    }

    return p_handler->pos;
}

/**
 * @brief Configure emitter to use our custom write handler
 * 
 * Sets up the libyaml emitter with our zero-malloc write callback.
 * This is the key connection between our buffer management and libyaml.
 * 
 * The emitter will call metac_yaml_write_handler for all output.
 * 
 * @param p_emitter     libyaml emitter to configure
 * @param p_handler_ctx Handler context (user data)
 * 
 * @return 1 on success, 0 on failure
 */
int metac_yaml_emitter_set_custom_output(
    yaml_emitter_t *p_emitter,
    metac_yaml_handler_context_t *p_handler_ctx)
{
    if (!p_emitter || !p_handler_ctx) {
        return 0;
    }

    //return 
    yaml_emitter_set_output(
        p_emitter,
        metac_yaml_write_handler,
        p_handler_ctx);
    return 1;
}
