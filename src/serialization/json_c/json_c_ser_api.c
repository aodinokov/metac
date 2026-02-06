/**
 * @file json_ser_api.c
 * @brief JSON serialization convenience API - fire-and-forget interface
 *
 * Provides single-function API for serializing metac values to JSON strings.
 * Orchestrates context creation, serialization, and cleanup internally.
 */

#include "metac/serialization/json_c.h"
#include <stdlib.h>
#include <string.h>

/**
 * Serialize metac_value to JSON string (single-call convenience API)
 * Handles full workflow: create context, serialize, return string copy
 * Caller must free returned string with metac_free_json_c_string()
 */
char* metac_value_to_json_c_string(
	metac_value_t *p_value,
	metac_value_walk_mode_t walk_mode)
{
	if (!p_value) {
		return NULL;
	}

	/* Phase 1: Create context */
	metac_json_c_serialization_t *p_serial =
		metac_json_c_serialization_new(p_value, walk_mode);

	if (!p_serial) {
		return NULL;
	}

	/* Phase 2: Serialize to JSON tree and string */
	if (metac_json_c_serialization_serialize(p_serial) != 0) {
		metac_json_c_serialization_delete(p_serial);
		return NULL;
	}

	/* Phase 3: Get string from context */
	const char *json_str = metac_json_c_serialization_get_string(p_serial);

	if (!json_str) {
		metac_json_c_serialization_delete(p_serial);
		return NULL;
	}

	/* Phase 4: Duplicate string (json_str is internal to json-c object) */
	size_t str_len = strlen(json_str);
	char *p_result = (char *)malloc(str_len + 1);

	if (!p_result) {
		metac_json_c_serialization_delete(p_serial);
		return NULL;
	}

	memcpy(p_result, json_str, str_len);
	p_result[str_len] = '\0';

	/* Phase 5: Cleanup context */
	metac_json_c_serialization_delete(p_serial);

	return p_result;
}

/**
 * Free string allocated by metac_value_to_json_c_string()
 */
void metac_free_json_c_string(char *p_str)
{
	if (p_str) {
		free(p_str);
	}
}
