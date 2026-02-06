/**
 * @file json_ser_context.c
 * @brief JSON serialization context lifecycle management
 *
 * Handles context creation, cleanup, and error tracking for JSON serialization.
 * Mirrors the YAML serialization context pattern for consistency.
 */

#include "metac/serialization/json_c.h"
#include "metac/reflect.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* Maximum error message length to prevent unbounded allocations */
#define METAC_JSON_C_ERROR_MSG_MAX 256

/**
 * Create new JSON serialization context
 * Initializes all fields and prepares for serialization
 */
metac_json_c_serialization_t* metac_json_c_serialization_new(
	metac_value_t *p_value,
	metac_value_walk_mode_t walk_mode)
{
	if (!p_value) {
		return NULL;
	}

	metac_json_c_serialization_t *p_serial =
		(metac_json_c_serialization_t *)malloc(sizeof(*p_serial));

	if (!p_serial) {
		return NULL;
	}

	/* Initialize all fields */
	p_serial->p_value = p_value;
	p_serial->walk_mode = walk_mode;
	p_serial->p_root_object = NULL;
	p_serial->p_json_string = NULL;
	p_serial->serialized = 0;
	p_serial->error_code = 0;
	p_serial->p_error_message = NULL;

	return p_serial;
}

/**
 * Delete serialization context and free all resources
 * Called in cleanup phase of both APIs
 */
void metac_json_c_serialization_delete(
	metac_json_c_serialization_t *p_serial)
{
	if (!p_serial) {
		return;
	}

	/* Free JSON object tree */
	if (p_serial->p_root_object) {
		json_object_put(p_serial->p_root_object);
		p_serial->p_root_object = NULL;
	}

	/* Free error message */
	if (p_serial->p_error_message) {
		free(p_serial->p_error_message);
		p_serial->p_error_message = NULL;
	}

	/* Free context itself */
	free(p_serial);
}

/**
 * Set error in serialization context
 * Internal function used by serialization algorithm
 */
void metac_json_c_serialization_set_error(
	metac_json_c_serialization_t *p_serial,
	const char *p_message,
	int error_code)
{
	if (!p_serial) {
		return;
	}

	/* Free previous error message if any */
	if (p_serial->p_error_message) {
		free(p_serial->p_error_message);
		p_serial->p_error_message = NULL;
	}

	/* Set error code */
	p_serial->error_code = error_code;

	/* Duplicate error message if provided */
	if (p_message) {
		size_t len = strlen(p_message);
		/* Limit message length to prevent unbounded allocations */
		if (len > METAC_JSON_C_ERROR_MSG_MAX) {
			len = METAC_JSON_C_ERROR_MSG_MAX;
		}
		p_serial->p_error_message = (char *)malloc(len + 1);
		if (p_serial->p_error_message) {
			memcpy(p_serial->p_error_message, p_message, len);
			p_serial->p_error_message[len] = '\0';
		}
	}
}

/**
 * Get error message from failed serialization
 */
const char* metac_json_c_serialization_get_error(
	metac_json_c_serialization_t *p_serial)
{
	if (!p_serial) {
		return NULL;
	}
	return p_serial->p_error_message;
}
