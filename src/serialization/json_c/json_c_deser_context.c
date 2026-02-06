/**
 * @file json_deser_context.c
 * @brief JSON deserialization context lifecycle management
 *
 * Handles context creation, cleanup, and error tracking for JSON deserialization.
 * Mirrors the YAML deserialization context pattern for consistency.
 */

#include "metac/serialization/json_c.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* Maximum error message length to prevent unbounded allocations */
#define METAC_JSON_C_ERROR_MSG_MAX 256

/**
 * Create new JSON deserialization context
 * Initializes all fields and prepares for deserialization
 */
metac_json_c_deserialization_t* metac_json_c_deserialization_new(
	metac_value_t *p_value,
	metac_value_walk_mode_t walk_mode)
{
	if (!p_value) {
		return NULL;
	}

	metac_json_c_deserialization_t *p_deser =
		(metac_json_c_deserialization_t *)malloc(sizeof(*p_deser));

	if (!p_deser) {
		return NULL;
	}

	/* Initialize all fields */
	p_deser->p_value = p_value;
	p_deser->walk_mode = walk_mode;
	p_deser->p_root_object = NULL;
	p_deser->p_tokener = json_tokener_new();
	p_deser->parsed = 0;
	p_deser->error_code = 0;
	p_deser->p_error_message = NULL;

	if (!p_deser->p_tokener) {
		free(p_deser);
		return NULL;
	}

	return p_deser;
}

/**
 * Delete deserialization context and free all resources
 * Called in cleanup phase of both APIs
 */
void metac_json_c_deserialization_delete(
	metac_json_c_deserialization_t *p_deser)
{
	if (!p_deser) {
		return;
	}

	/* Free parsed JSON object tree */
	if (p_deser->p_root_object) {
		json_object_put(p_deser->p_root_object);
		p_deser->p_root_object = NULL;
	}

	/* Free tokenizer */
	if (p_deser->p_tokener) {
		json_tokener_free(p_deser->p_tokener);
		p_deser->p_tokener = NULL;
	}

	/* Free error message */
	if (p_deser->p_error_message) {
		free(p_deser->p_error_message);
		p_deser->p_error_message = NULL;
	}

	/* Free context itself */
	free(p_deser);
}

/**
 * Set error in deserialization context
 * Internal function used by deserialization algorithm
 */
void metac_json_c_deserialization_set_error(
	metac_json_c_deserialization_t *p_deser,
	const char *p_message,
	int error_code)
{
	if (!p_deser) {
		return;
	}

	/* Free previous error message if any */
	if (p_deser->p_error_message) {
		free(p_deser->p_error_message);
		p_deser->p_error_message = NULL;
	}

	/* Set error code */
	p_deser->error_code = error_code;

	/* Duplicate error message if provided */
	if (p_message) {
		size_t len = strlen(p_message);
		/* Limit message length to prevent unbounded allocations */
		if (len > METAC_JSON_C_ERROR_MSG_MAX) {
			len = METAC_JSON_C_ERROR_MSG_MAX;
		}
		p_deser->p_error_message = (char *)malloc(len + 1);
		if (p_deser->p_error_message) {
			memcpy(p_deser->p_error_message, p_message, len);
			p_deser->p_error_message[len] = '\0';
		}
	}
}

/**
 * Get error message from failed deserialization
 */
const char* metac_json_c_deserialization_get_error(
	metac_json_c_deserialization_t *p_deser)
{
	if (!p_deser) {
		return NULL;
	}
	return p_deser->p_error_message;
}
