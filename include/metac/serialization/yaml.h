/**
 * @file yaml.h
 * @brief YAML serialization API for metac values - Two APIs with 90% shared code
 */

#ifndef METAC_BACKEND_YAML_H
#define METAC_BACKEND_YAML_H

#include <stddef.h>
#include <yaml.h>
#include "metac/reflect.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct metac_recursive_iterator metac_recursive_iterator_t;

/**
 * @brief Handler context for custom libyaml write callback
 */
typedef struct {
	char *p_buffer;
	size_t capacity;
	size_t pos;
	int overflow;
} metac_yaml_handler_context_t;

/**
 * @brief YAML serialization context with iterator caching
 */
typedef struct {
	metac_value_t *p_value;
	metac_value_walk_mode_t walk_mode;
	int measured;
	size_t required_size;
	metac_recursive_iterator_t *p_iterator_cached;
	yaml_emitter_t emitter;
} metac_yaml_serialization_t;

/* Context API - Explicit Control */
metac_yaml_serialization_t* metac_yaml_serialization_new(
	metac_value_t *p_value,
	metac_value_walk_mode_t walk_mode);
int metac_yaml_serialization_measure(
	metac_yaml_serialization_t *p_serial,
	size_t *p_size);
int metac_yaml_serialization_to_buffer(
	metac_yaml_serialization_t *p_serial,
	char *p_buffer,
	size_t size);
void metac_yaml_serialization_delete(
	metac_yaml_serialization_t *p_serial);

/* Convenience API - Fire-and-Forget */
char* metac_value_to_yaml_string(
	metac_value_t *p_value,
	metac_value_walk_mode_t walk_mode);
void metac_free_yaml_string(char *p_str);

/* Legacy API */
char* metac_value_to_yaml(metac_value_t* p_val, metac_value_walk_mode_t wmode, metac_tag_map_t* p_tag_map);
int metac_value_from_yaml(metac_value_t* p_val, const char* yaml_string, metac_tag_map_t* p_tag_map);

/* Internal functions */
metac_recursive_iterator_t* metac_yaml_serialization_get_iterator(metac_yaml_serialization_t *p_serial);
int metac_yaml_serialization_is_measured(metac_yaml_serialization_t *p_serial);
void metac_yaml_serialization_set_measured(metac_yaml_serialization_t *p_serial, size_t size);
size_t metac_yaml_serialization_get_required_size(metac_yaml_serialization_t *p_serial);
void metac_yaml_handler_init(metac_yaml_handler_context_t *p_handler, char *p_buffer, size_t capacity);
int metac_yaml_handler_has_overflow(metac_yaml_handler_context_t *p_handler);
size_t metac_yaml_handler_get_bytes_written(metac_yaml_handler_context_t *p_handler);
int metac_yaml_emitter_set_custom_output(yaml_emitter_t *p_emitter, metac_yaml_handler_context_t *p_handler_ctx);
int metac_yaml_write_handler(void *data, unsigned char *buffer, size_t size);

#ifdef __cplusplus
}
#endif

#endif // METAC_BACKEND_YAML_H
