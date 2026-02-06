/**
 * @file json_c.h
 * @brief JSON-C serialization/deserialization API for metac values using json-c library
 * Dual APIs with Context and Convenience patterns mirroring YAML architecture
 */

#ifndef METAC_BACKEND_JSON_C_H
#define METAC_BACKEND_JSON_C_H

#include <stddef.h>
#include <json.h>
#include "metac/reflect.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct metac_recursive_iterator metac_recursive_iterator_t;

/**
 * @brief JSON serialization context
 * Holds state for building JSON representation of metac values
 */
typedef struct {
	metac_value_t *p_value;                      /* Target value to serialize */
	metac_value_walk_mode_t walk_mode;           /* WALK_MODE_DEEP or WALK_MODE_SHALLOW */
	json_object *p_root_object;                  /* Built JSON root object/array */
	char *p_json_string;                         /* Cached stringified result */
	int serialized;                              /* Flag: has serialization occurred */
	int error_code;                              /* Error tracking */
	char *p_error_message;                       /* Human-readable error message */
} metac_json_c_serialization_t;

/**
 * @brief JSON deserialization context
 * Holds state for parsing JSON and populating metac values
 */
typedef struct {
	metac_value_t *p_value;                      /* Target value to populate */
	metac_value_walk_mode_t walk_mode;           /* WALK_MODE_DEEP or WALK_MODE_SHALLOW */
	json_object *p_root_object;                  /* Parsed JSON root */
	json_tokener *p_tokener;                     /* Reusable parser instance */
	int parsed;                                  /* Flag: has parsing succeeded */
	int error_code;                              /* Error tracking */
	char *p_error_message;                       /* Human-readable error message */
} metac_json_c_deserialization_t;

/* ========== Serialization: Context API ========== */

/**
 * Create new JSON serialization context
 * @param p_value    metac_value to serialize
 * @param walk_mode  WALK_MODE_DEEP or WALK_MODE_SHALLOW
 * @return           Allocated context, or NULL on allocation failure
 */
metac_json_c_serialization_t* metac_json_c_serialization_new(
	metac_value_t *p_value,
	metac_value_walk_mode_t walk_mode);

/**
 * Serialize metac_value to JSON (builds tree and converts to string)
 * @param p_serial   Serialization context
 * @return           0 on success, -1 on error (check get_error for details)
 */
int metac_json_c_serialization_serialize(
	metac_json_c_serialization_t *p_serial);

/**
 * Get serialized JSON string
 * Valid only as long as context is alive and not modified
 * @param p_serial   Serialization context
 * @return           Pointer to JSON string, or NULL if not serialized
 */
const char* metac_json_c_serialization_get_string(
	metac_json_c_serialization_t *p_serial);

/**
 * Get error message from failed serialization
 * @param p_serial   Serialization context
 * @return           Error message string, or NULL if no error
 */
const char* metac_json_c_serialization_get_error(
	metac_json_c_serialization_t *p_serial);

/**
 * Delete serialization context and free all resources
 * @param p_serial   Serialization context
 */
void metac_json_c_serialization_delete(
	metac_json_c_serialization_t *p_serial);

/* ========== Serialization: Convenience API ========== */

/**
 * Serialize metac_value to JSON string (single-call API)
 * Allocates result string - caller must free with metac_free_json_c_string
 * @param p_value    metac_value to serialize
 * @param walk_mode  WALK_MODE_DEEP or WALK_MODE_SHALLOW
 * @return           Allocated JSON string, or NULL on error
 */
char* metac_value_to_json_c_string(
	metac_value_t *p_value,
	metac_value_walk_mode_t walk_mode);

/**
 * Free string allocated by metac_value_to_json_c_string
 * @param p_str      String to free
 */
void metac_free_json_c_string(char *p_str);

/* ========== Deserialization: Context API ========== */

/**
 * Create new JSON deserialization context
 * @param p_value    metac_value to populate (must be pre-allocated)
 * @param walk_mode  WALK_MODE_DEEP or WALK_MODE_SHALLOW
 * @return           Allocated context, or NULL on allocation failure
 */
metac_json_c_deserialization_t* metac_json_c_deserialization_new(
	metac_value_t *p_value,
	metac_value_walk_mode_t walk_mode);

/**
 * Parse JSON string and populate target metac_value
 * @param p_deser       Deserialization context
 * @param p_json_string JSON string to parse
 * @param json_len      Length of JSON string (use strlen if null-terminated)
 * @return              0 on success, -1 on error (check get_error for details)
 */
int metac_json_c_deserialization_from_string(
	metac_json_c_deserialization_t *p_deser,
	const char *p_json_string,
	size_t json_len);

/**
 * Get error message from failed deserialization
 * @param p_deser    Deserialization context
 * @return           Error message string, or NULL if no error
 */
const char* metac_json_c_deserialization_get_error(
	metac_json_c_deserialization_t *p_deser);

/**
 * Delete deserialization context and free all resources
 * @param p_deser    Deserialization context
 */
void metac_json_c_deserialization_delete(
	metac_json_c_deserialization_t *p_deser);

/* ========== Deserialization: Convenience API ========== */

/**
 * Parse JSON string and populate metac_value (single-call API)
 * @param p_value       metac_value to populate (must be pre-allocated)
 * @param p_json_string JSON string to parse
 * @param walk_mode     WALK_MODE_DEEP or WALK_MODE_SHALLOW
 * @return              0 on success, -1 on error
 */
int metac_value_from_json_c_string(
	metac_value_t *p_value,
	const char *p_json_string,
	metac_value_walk_mode_t walk_mode);

/* ========== Internal Functions ========== */

/**
 * Set error message in serialization context (internal use)
 */
void metac_json_c_serialization_set_error(
	metac_json_c_serialization_t *p_serial,
	const char *p_message,
	int error_code);

/**
 * Set error message in deserialization context (internal use)
 */
void metac_json_c_deserialization_set_error(
	metac_json_c_deserialization_t *p_deser,
	const char *p_message,
	int error_code);

#ifdef __cplusplus
}
#endif

#endif /* METAC_BACKEND_JSON_C_H */
