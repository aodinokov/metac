/**
 * @file json_deser_parser.c
 * @brief JSON deserialization parser - recursively converts JSON to metac values
 *
 * Core deserialization algorithm that walks JSON object tree and populates
 * corresponding metac_value_t structures. Mirrors the YAML parser pattern.
 */

#include "metac/serialization/json_c.h"
#include "metac/reflect.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <stdint.h>

/* Forward declarations */
static int json_value_from_object(
	metac_json_c_deserialization_t *p_deser,
	json_object *json_obj,
	metac_value_t *p_target);

/**
 * Parse enumeration from JSON string
 * Finds enum by name and sets the value
 */
static int json_parse_enum(
	metac_json_c_deserialization_t *p_deser,
	metac_value_t *p_target,
	const char *enum_name)
{
	if (!enum_name) {
		metac_json_c_deserialization_set_error(p_deser,
			"Enumeration name is NULL", EINVAL);
		return -1;
	}

	metac_entry_t *p_entry = metac_value_entry(p_target);
	metac_num_t enum_count = metac_entry_enumeration_info_size(p_entry);

	/* Iterate through all enum values to find matching name */
	for (metac_num_t i = 0; i < enum_count; ++i) {
		struct metac_type_enumerator_info const *p_enum_info =
			metac_entry_enumeration_info(p_entry, i);

		if (p_enum_info && p_enum_info->name &&
		    strcmp(p_enum_info->name, enum_name) == 0) {
			/* Found matching enum - set the value */
			return metac_value_set_enumeration(p_target, p_enum_info->const_value);
		}
	}

	/* Enum name not found */
	metac_json_c_deserialization_set_error(p_deser,
		"Enumeration value not found", EINVAL);
	return -1;
}

/**
 * Parse JSON scalar value and assign to metac base type
 * Handles int, float, char, bool conversion
 */
static int json_parse_scalar_to_value(
	metac_json_c_deserialization_t *p_deser,
	metac_value_t *p_target,
	json_object *json_obj)
{
	if (!json_obj) {
		metac_json_c_deserialization_set_error(p_deser,
			"Scalar JSON object is NULL", EINVAL);
		return -1;
	}

	int ret = -1;

	/* Handle different base types */
	if (metac_value_is_int(p_target)) {
		int val = json_object_get_int(json_obj);
		ret = metac_value_set_int(p_target, val);
	}
	else if (metac_value_is_uint(p_target)) {
		unsigned int val = (unsigned int)json_object_get_int(json_obj);
		ret = metac_value_set_uint(p_target, val);
	}
	else if (metac_value_is_float(p_target)) {
		float val = (float)json_object_get_double(json_obj);
		ret = metac_value_set_float(p_target, val);
	}
	else if (metac_value_is_double(p_target)) {
		double val = json_object_get_double(json_obj);
		ret = metac_value_set_double(p_target, val);
	}
	else if (metac_value_is_bool(p_target)) {
		bool val = (bool)json_object_get_boolean(json_obj);
		ret = metac_value_set_bool(p_target, val);
	}
	else if (metac_value_is_char(p_target)) {
		/* For char: extract from string or take from JSON int/string */
		const char *str_val = json_object_get_string(json_obj);
		if (str_val && str_val[0] != '\0') {
			ret = metac_value_set_char(p_target, str_val[0]);
		} else {
			int int_val = json_object_get_int(json_obj);
			ret = metac_value_set_char(p_target, (char)int_val);
		}
	}
	else if (metac_value_is_long(p_target)) {
		int64_t val = json_object_get_int64(json_obj);
		ret = metac_value_set_long(p_target, (long)val);
	}
	else if (metac_value_is_ulong(p_target)) {
		int64_t val = json_object_get_int64(json_obj);
		ret = metac_value_set_ulong(p_target, (unsigned long)val);
	}
	else if (metac_value_is_llong(p_target)) {
		int64_t val = json_object_get_int64(json_obj);
		ret = metac_value_set_llong(p_target, (long long)val);
	}
	else if (metac_value_is_ullong(p_target)) {
		int64_t val = json_object_get_int64(json_obj);
		ret = metac_value_set_ullong(p_target, (unsigned long long)val);
	}
	else {
		/* Try generic string conversion for other base types */
		const char *str_val = json_object_get_string(json_obj);
		if (str_val) {
			size_t str_len = strlen(str_val);
			ret = metac_value_set_base_type(p_target, NULL, 0,
				(void *)str_val, str_len);
		}
	}

	if (ret != 0) {
		metac_json_c_deserialization_set_error(p_deser,
			"Failed to assign base type value", EINVAL);
	}

	return ret;
}

/**
 * Populate struct/union/class from JSON object
 */
static int json_parse_struct(
	metac_json_c_deserialization_t *p_deser,
	json_object *json_obj,
	metac_value_t *p_target)
{
	if (!json_object_is_type(json_obj, json_type_object)) {
		metac_json_c_deserialization_set_error(p_deser,
			"Expected JSON object for struct type", EINVAL);
		return -1;
	}

	metac_num_t member_count = metac_value_member_count(p_target);

	/* Iterate through all struct members */
	for (metac_num_t i = 0; i < member_count; ++i) {
		metac_value_t *p_member = metac_new_value_by_member_id(p_target, i);
		if (!p_member) {
			metac_json_c_deserialization_set_error(p_deser,
				"Failed to get struct member by ID", EINVAL);
			return -1;
		}

		const char *member_name = metac_value_name(p_member);

		/* Find corresponding JSON object member */
		json_object *member_json = json_object_object_get(json_obj, member_name);

		if (member_json) {
			/* Recursively deserialize member */
			if (json_value_from_object(p_deser, member_json, p_member) != 0) {
				metac_value_delete(p_member);
				return -1;
			}
		}
		/* If member not found in JSON, it remains unset (initialized by caller) */

		metac_value_delete(p_member);
	}

	return 0;
}

/**
 * Populate array/fixed buffer from JSON array
 */
static int json_parse_array(
	metac_json_c_deserialization_t *p_deser,
	json_object *json_obj,
	metac_value_t *p_target)
{
	if (!json_object_is_type(json_obj, json_type_array)) {
		metac_json_c_deserialization_set_error(p_deser,
			"Expected JSON array for array type", EINVAL);
		return -1;
	}

	metac_num_t target_count = metac_value_element_count(p_target);
	int json_count = json_object_array_length(json_obj);

	/* Verify array size matches */
	if (target_count != (metac_num_t)json_count) {
		metac_json_c_deserialization_set_error(p_deser,
			"Array size mismatch between JSON and target", EINVAL);
		return -1;
	}

	/* Iterate through all array elements */
	for (metac_num_t i = 0; i < target_count; ++i) {
		json_object *element_json = json_object_array_get_idx(json_obj, (size_t)i);

		if (!element_json) {
			metac_json_c_deserialization_set_error(p_deser,
				"Failed to get JSON array element", EINVAL);
			return -1;
		}

		metac_value_t *p_element = metac_new_value_by_element_id(p_target, i);
		if (!p_element) {
			metac_json_c_deserialization_set_error(p_deser,
				"Failed to get target array element", EINVAL);
			return -1;
		}

		/* Recursively deserialize element */
		if (json_value_from_object(p_deser, element_json, p_element) != 0) {
			metac_value_delete(p_element);
			return -1;
		}

		metac_value_delete(p_element);
	}

	return 0;
}

/**
 * Recursively deserialize JSON object into metac value
 * Main dispatcher that handles all JSON type → metac type conversions
 */
static int json_value_from_object(
	metac_json_c_deserialization_t *p_deser,
	json_object *json_obj,
	metac_value_t *p_target)
{
	if (!json_obj || !p_target) {
		return -1;
	}

	/* Handle enumeration type */
	if (metac_value_is_enumeration(p_target)) {
		if (!json_object_is_type(json_obj, json_type_string)) {
			metac_json_c_deserialization_set_error(p_deser,
				"Expected JSON string for enumeration type", EINVAL);
			return -1;
		}

		const char *enum_str = json_object_get_string(json_obj);
		return json_parse_enum(p_deser, p_target, enum_str);
	}

	/* Handle base types (scalars) */
	if (metac_value_is_base_type(p_target)) {
		/* Make sure it's a scalar JSON type */
		enum json_type jtype = json_object_get_type(json_obj);
		if (jtype == json_type_object || jtype == json_type_array) {
			metac_json_c_deserialization_set_error(p_deser,
				"Expected JSON scalar for base type", EINVAL);
			return -1;
		}

		return json_parse_scalar_to_value(p_deser, p_target, json_obj);
	}

	/* Handle array types */
	metac_kind_t kind = metac_value_final_kind(p_target, NULL);

	if (kind == METAC_KND_array_type) {
		return json_parse_array(p_deser, json_obj, p_target);
	}

	/* Handle struct/union/class types */
	if (kind == METAC_KND_struct_type ||
	    kind == METAC_KND_union_type ||
	    kind == METAC_KND_class_type) {
		return json_parse_struct(p_deser, json_obj, p_target);
	}

	/* Handle pointer types - not supported for deserialization */
	if (kind == METAC_KND_pointer_type) {
		metac_json_c_deserialization_set_error(p_deser,
			"Pointer deserialization not supported", EINVAL);
		return -1;
	}

	/* Unsupported type */
	metac_json_c_deserialization_set_error(p_deser,
		"Unsupported type for JSON deserialization", EINVAL);
	return -1;
}

/**
 * Parse JSON string and populate target metac_value
 * Called by convenience and context APIs
 */
int metac_json_c_deserialization_from_string(
	metac_json_c_deserialization_t *p_deser,
	const char *p_json_string,
	size_t json_len)
{
	if (!p_deser || !p_deser->p_value || !p_json_string || json_len == 0) {
		if (p_deser) {
			metac_json_c_deserialization_set_error(p_deser,
				"Invalid argument to from_string", EINVAL);
		}
		return -1;
	}

	/* Parse JSON string */
	if (json_len == 0) {
		json_len = strlen(p_json_string);
	}

	p_deser->p_root_object = json_tokener_parse_ex(p_deser->p_tokener,
		p_json_string, (int)json_len);

	if (!p_deser->p_root_object) {
		enum json_tokener_error err = json_tokener_get_error(p_deser->p_tokener);
		const char *err_msg = json_tokener_error_desc(err);
		metac_json_c_deserialization_set_error(p_deser,
			err_msg ? err_msg : "JSON parse error", EINVAL);
		return -1;
	}

	/* Recursively populate target value from parsed JSON */
	if (json_value_from_object(p_deser, p_deser->p_root_object,
	                           p_deser->p_value) != 0) {
		return -1;
	}

	/* Mark as successfully parsed */
	p_deser->parsed = 1;

	return 0;
}
