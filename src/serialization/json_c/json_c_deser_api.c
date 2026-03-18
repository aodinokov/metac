/**
 * @file json_deser_api.c
 * @brief JSON deserialization convenience API - fire-and-forget interface
 *
 * Provides single-function API for deserializing JSON strings into metac values.
 * Orchestrates context creation, parsing, and cleanup internally.
 */

#include "metac/serialization/json_c.h"
#include <stdlib.h>
#include <string.h>

/**
 * Parse JSON string and populate metac_value (single-call convenience API)
 * Handles full workflow: create context, parse, populate, cleanup
 * Returns 0 on success, -1 on failure
 */
int metac_value_from_json_c_string(
	metac_value_t *p_value,
	const char *p_json_string,
	metac_value_walk_mode_t walk_mode)
{
	if (!p_value || !p_json_string) {
		return -1;
	}

	size_t json_len = strlen(p_json_string);

	/* Phase 1: Create context */
	metac_json_c_deserialization_t *p_deser =
		metac_json_c_deserialization_new(p_value, walk_mode);

	if (!p_deser) {
		return -1;
	}

	/* Phase 2: Parse JSON and populate value */
	if (metac_json_c_deserialization_from_string(p_deser, p_json_string, json_len) != 0) {
		metac_json_c_deserialization_delete(p_deser);
		return -1;
	}

	/* Phase 3: Cleanup context */
	metac_json_c_deserialization_delete(p_deser);

	return 0;
}
