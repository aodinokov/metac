/**
 * @file json_ser_builder.c
 * @brief JSON tree builder - recursively converts metac values to JSON objects
 *
 * Core serialization algorithm that walks metac value tree and builds equivalent
 * JSON object tree. Supports structs, arrays, enumerations, and base types.
 */

#include "metac/serialization/json_c.h"
#include "metac/reflect.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>

/* Forward declarations */
static json_object* json_emit_value(
	metac_json_c_serialization_t *p_serial,
	metac_value_t *p_value);

/**
 * Check if we should continue recursing based on walk mode
 * SHALLOW mode: Only serialize scalar types (base_type, enum, pointer)
 * DEEP mode: Recursively serialize all types including structs/arrays
 */
static int should_recurse(
	metac_value_t *p_current,
	metac_value_walk_mode_t walk_mode)
{
	/* DEEP mode always recurses */
	if (walk_mode == METAC_WMODE_deep) {
		return 1;
	}

	/* SHALLOW mode: only recurse for scalar types */
	if (walk_mode == METAC_WMODE_shallow) {
		metac_kind_t kind = metac_value_final_kind(p_current, NULL);
		switch (kind) {
		case METAC_KND_base_type:
		case METAC_KND_enumeration_type:
		case METAC_KND_pointer_type:
			return 1;
		default:
			/* Skip complex types (structs, arrays) in shallow mode */
			return 0;
		}
	}

	return 1;
}

/**
 * Serialize struct/union/class members as JSON object
 */
static json_object* json_emit_struct(
	metac_json_c_serialization_t *p_serial,
	metac_value_t *p_value)
{
	json_object *p_obj = json_object_new_object();
	if (!p_obj) {
		metac_json_c_serialization_set_error(p_serial,
			"Failed to allocate JSON object", ENOMEM);
		return NULL;
	}

	metac_num_t member_count = metac_value_member_count(p_value);

	/* Iterate through all members */
	for (metac_num_t i = 0; i < member_count; ++i) {
		metac_value_t *p_member = metac_new_value_by_member_id(p_value, i);
		if (!p_member) {
			metac_json_c_serialization_set_error(p_serial,
				"Failed to get struct member", ENOMEM);
			json_object_put(p_obj);
			return NULL;
		}

		const char *member_name = metac_value_name(p_member);
		json_object *member_json = NULL;

		/* Check if we should recurse into this member based on walk mode */
		if (should_recurse(p_member, p_serial->walk_mode)) {
			member_json = json_emit_value(p_serial, p_member);
		} else {
			/* SHALLOW mode: represent complex types as type strings */
			char *type_str = metac_value_base_type_string(p_member);
			if (type_str) {
				member_json = json_object_new_string(type_str);
			}
		}

		metac_value_delete(p_member);

		if (!member_json) {
			json_object_put(p_obj);
			return NULL;
		}

		/* Add member to object */
		if (json_object_object_add(p_obj, member_name, member_json) != 0) {
			metac_json_c_serialization_set_error(p_serial,
				"Failed to add struct member to JSON object", ENOMEM);
			json_object_put(member_json);
			json_object_put(p_obj);
			return NULL;
		}
	}

	return p_obj;
}

/**
 * Serialize array/fixed buffer elements as JSON array
 */
static json_object* json_emit_array(
	metac_json_c_serialization_t *p_serial,
	metac_value_t *p_value)
{
	json_object *p_array = json_object_new_array();
	if (!p_array) {
		metac_json_c_serialization_set_error(p_serial,
			"Failed to allocate JSON array", ENOMEM);
		return NULL;
	}

	metac_num_t element_count = metac_value_element_count(p_value);

	/* Iterate through all elements */
	for (metac_num_t i = 0; i < element_count; ++i) {
		metac_value_t *p_element = metac_new_value_by_element_id(p_value, i);
		if (!p_element) {
			metac_json_c_serialization_set_error(p_serial,
				"Failed to get array element", ENOMEM);
			json_object_put(p_array);
			return NULL;
		}

		json_object *element_json = NULL;

		/* Check if we should recurse into array elements based on walk mode */
		if (should_recurse(p_element, p_serial->walk_mode)) {
			element_json = json_emit_value(p_serial, p_element);
		} else {
			/* SHALLOW mode: represent complex elements as type strings */
			char *type_str = metac_value_base_type_string(p_element);
			if (type_str) {
				element_json = json_object_new_string(type_str);
			}
		}

		metac_value_delete(p_element);

		if (!element_json) {
			json_object_put(p_array);
			return NULL;
		}

		/* Add element to array */
		if (json_object_array_add(p_array, element_json) != 0) {
			metac_json_c_serialization_set_error(p_serial,
				"Failed to add array element to JSON array", ENOMEM);
			json_object_put(element_json);
			json_object_put(p_array);
			return NULL;
		}
	}

	return p_array;
}

/**
 * Serialize enumeration as JSON string (enum name)
 */
static json_object* json_emit_enum(
	metac_json_c_serialization_t *p_serial,
	metac_value_t *p_value)
{
	char *enum_str = metac_value_enumeration_string(p_value);
	if (!enum_str) {
		metac_json_c_serialization_set_error(p_serial,
			"Failed to get enumeration string", ENOMEM);
		return NULL;
	}

	json_object *p_obj = json_object_new_string(enum_str);
	if (!p_obj) {
		metac_json_c_serialization_set_error(p_serial,
			"Failed to allocate JSON string object for enum", ENOMEM);
	}

	return p_obj;
}

/**
 * Serialize base type (int, float, char, etc.) as appropriate JSON type
 */
static json_object* json_emit_base_type(
	metac_json_c_serialization_t *p_serial,
	metac_value_t *p_value)
{
	json_object *p_obj = NULL;

	/* Try to extract as specific base types */
	if (metac_value_is_int(p_value)) {
		int val;
		if (metac_value_int(p_value, &val) == 0) {
			p_obj = json_object_new_int(val);
		}
	}
	else if (metac_value_is_uint(p_value)) {
		unsigned int val;
		if (metac_value_uint(p_value, &val) == 0) {
			p_obj = json_object_new_int((int)val);
		}
	}
	else if (metac_value_is_double(p_value)) {
		double val;
		if (metac_value_double(p_value, &val) == 0) {
			p_obj = json_object_new_double(val);
		}
	}
	else if (metac_value_is_float(p_value)) {
		float val;
		if (metac_value_float(p_value, &val) == 0) {
			p_obj = json_object_new_double((double)val);
		}
	}
	else if (metac_value_is_bool(p_value)) {
		bool val;
		if (metac_value_bool(p_value, &val) == 0) {
			p_obj = json_object_new_boolean(val ? 1 : 0);
		}
	}
	else if (metac_value_is_char(p_value)) {
		char val;
		if (metac_value_char(p_value, &val) == 0) {
			char str[2] = {val, '\0'};
			p_obj = json_object_new_string(str);
		}
	}
	else if (metac_value_is_long(p_value)) {
		long val;
		if (metac_value_long(p_value, &val) == 0) {
			p_obj = json_object_new_int64((int64_t)val);
		}
	}
	else if (metac_value_is_ulong(p_value)) {
		unsigned long val;
		if (metac_value_ulong(p_value, &val) == 0) {
			p_obj = json_object_new_int64((int64_t)val);
		}
	}
	else if (metac_value_is_llong(p_value)) {
		long long val;
		if (metac_value_llong(p_value, &val) == 0) {
			p_obj = json_object_new_int64((int64_t)val);
		}
	}
	else if (metac_value_is_ullong(p_value)) {
		unsigned long long val;
		if (metac_value_ullong(p_value, &val) == 0) {
			p_obj = json_object_new_int64((int64_t)val);
		}
	}
	else {
		/* Fall back to string representation for other base types */
		char *type_str = metac_value_base_type_string(p_value);
		if (type_str) {
			p_obj = json_object_new_string(type_str);
		}
	}

	if (!p_obj) {
		metac_json_c_serialization_set_error(p_serial,
			"Failed to serialize base type to JSON", ENOMEM);
	}

	return p_obj;
}

/**
 * Recursively emit metac value as JSON object/array/scalar
 * Dispatches to handler based on value type
 */
static json_object* json_emit_value(
	metac_json_c_serialization_t *p_serial,
	metac_value_t *p_value)
{
	if (!p_value) {
		return NULL;
	}

	/* Get the final kind of this value, skipping wrappers like variables and typedefs */
	metac_kind_t kind = metac_value_final_kind(p_value, NULL);

	/* Check value type and dispatch to appropriate handler */

	if (kind == METAC_KND_enumeration_type) {
		return json_emit_enum(p_serial, p_value);
	}

	if (kind == METAC_KND_base_type) {
		return json_emit_base_type(p_serial, p_value);
	}

	if (kind == METAC_KND_array_type) {
		return json_emit_array(p_serial, p_value);
	}

	if (kind == METAC_KND_struct_type ||
	    kind == METAC_KND_union_type ||
	    kind == METAC_KND_class_type) {
		return json_emit_struct(p_serial, p_value);
	}

	/* Handle pointer type - serialize as hex string */
	if (kind == METAC_KND_pointer_type) {
		char *ptr_str = metac_value_pointer_string(p_value);
		if (!ptr_str) {
			metac_json_c_serialization_set_error(p_serial,
				"Failed to get pointer string", ENOMEM);
			return NULL;
		}
		return json_object_new_string(ptr_str);
	}

	/* Type not handled */
	metac_json_c_serialization_set_error(p_serial,
		"Unsupported type for JSON serialization", ENOMEM);
	return NULL;
}

/**
 * Main serialization function - builds JSON tree from metac value
 * Called by convenience and context APIs after context is created
 */
int metac_json_c_serialization_serialize(
	metac_json_c_serialization_t *p_serial)
{
	if (!p_serial || !p_serial->p_value) {
		return -1;
	}

	/* Build JSON tree from value */
	p_serial->p_root_object = json_emit_value(p_serial, p_serial->p_value);

	if (!p_serial->p_root_object) {
		if (!p_serial->p_error_message) {
			metac_json_c_serialization_set_error(p_serial,
				"Failed to build JSON tree", ENOMEM);
		}
		return -1;
	}

	/* Convert JSON object to string */
	const char *json_str = json_object_to_json_string_ext(
		p_serial->p_root_object,
		JSON_C_TO_STRING_SPACED);

	if (!json_str) {
		metac_json_c_serialization_set_error(p_serial,
			"Failed to serialize JSON object to string", ENOMEM);
		return -1;
	}

	/* Store string (json_str is internal buffer - we don't own it) */
	p_serial->p_json_string = (char *)json_str;

	/* Mark as serialized */
	p_serial->serialized = 1;

	return 0;
}

/**
 * Get serialized JSON string from context
 * Valid only while context is alive and not modified
 */
const char* metac_json_c_serialization_get_string(
	metac_json_c_serialization_t *p_serial)
{
	if (!p_serial || !p_serial->serialized) {
		return NULL;
	}
	return p_serial->p_json_string;
}
