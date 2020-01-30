/*
 * memory_backend_json.c
 *
 *  Created on: Jan 29, 2020
 *      Author: mralex
 */

#define METAC_DEBUG_ENABLE

#include <assert.h>			/* assert */
#include <errno.h>			/* ENOMEM etc */
#include <stdlib.h>			/* calloc, free, NULL, qsort... */
#include <urcu/list.h>		/* struct cds_list_entry etc */
#include <json-c/json.h>	/* json_... */

#include "metac_internals.h"
#include "metac_debug.h"	/* msg_stderr, ...*/

/*****************************************************************************/
#define _create_with_error_result_(_type_, _error_result_) \
	struct _type_ * p_##_type_; \
	p_##_type_ = calloc(1, sizeof(*(p_##_type_))); \
	if (p_##_type_ == NULL) { \
		msg_stderr("Can't create " #_type_ ": no memory\n"); \
		return (_error_result_); \
	}

#define _create_(_type_) \
	_create_with_error_result_(_type_, NULL)

#define _delete_start_(_type_) \
	if (pp_##_type_ == NULL) { \
		msg_stderr("Can't delete " #_type_ ": invalid parameter\n"); \
		return -EINVAL; \
	} \
	if (*pp_##_type_ == NULL) { \
		msg_stderr("Can't delete " #_type_ ": already deleted\n"); \
		return -EALREADY; \
	}
#define _delete_finish(_type_) \
	free(*pp_##_type_); \
	*pp_##_type_ = NULL;
#define _delete_(_type_) \
	_delete_start_(_type_) \
	_delete_finish(_type_)

/*****************************************************************************/
int memory_backend_interface_create_from_json_ex(
		json_object *								p_master_json,
		const char *								override_current_object_path,		/* JSON pointer notation as defined in RFC 6901 */
		metac_data_member_location_t				override_current_object_extra_offset,
		struct memory_backend_interface **			pp_memory_backend_interface);
/*****************************************************************************/
struct memory_backend_json {
	struct memory_backend_interface					memory_backend_interface;
	json_object *									p_master_json;
	json_object *									p_current_object_json;
	metac_data_member_location_t					current_object_extra_offset
};
/*****************************************************************************/
static inline struct memory_backend_json * memory_backend_json(
		struct memory_backend_interface *			p_memory_backend_interface) {
	return cds_list_entry(
			p_memory_backend_interface,
			struct memory_backend_json,
			memory_backend_interface);
}
/*****************************************************************************/
static int memory_backend_json_delete(
		struct memory_backend_json **				pp_memory_backend_json) {
	_delete_start_(memory_backend_json);
	if ((*pp_memory_backend_json)->p_current_object_json) {
		json_object_put((*pp_memory_backend_json)->p_current_object_json);
		(*pp_memory_backend_json)->p_current_object_json = NULL;
	}
	if ((*pp_memory_backend_json)->p_master_json) {
		json_object_put((*pp_memory_backend_json)->p_master_json);
		(*pp_memory_backend_json)->p_master_json = NULL;
	}
	_delete_finish(memory_backend_json);
	return 0;
}
/*****************************************************************************/
static int _memory_backend_interface_delete(
		struct memory_backend_interface **			pp_memory_backend_interface) {
	struct memory_backend_json * p_memory_backend_json;

	assert(pp_memory_backend_interface);
	assert(*pp_memory_backend_interface);

	if ((*pp_memory_backend_interface) != NULL) {
		p_memory_backend_json = memory_backend_json(*pp_memory_backend_interface);
		memory_backend_json_delete(&p_memory_backend_json);
		*pp_memory_backend_interface = NULL;
	}

	return 0;
}

//static int _memory_backend_interface_equals(
//		struct memory_backend_interface *			p_memory_backend_interface0,
//		struct memory_backend_interface *			p_memory_backend_interface1) {
//	struct memory_backend_pointer * p_memory_backend_pointer0;
//	struct memory_backend_pointer * p_memory_backend_pointer1;
//
//	assert(p_memory_backend_interface0);
//	assert(p_memory_backend_interface1);
//
//	p_memory_backend_pointer0 = memory_backend_pointer(p_memory_backend_interface0);
//	p_memory_backend_pointer1 = memory_backend_pointer(p_memory_backend_interface1);
//
//	return p_memory_backend_pointer0 == p_memory_backend_pointer1;
//}


/*****************************************************************************/
static struct memory_backend_interface_ops ops = {
	/* Mandatory handlers */
	.memory_backend_interface_delete =				_memory_backend_interface_delete,
//	.memory_backend_interface_equals =				_memory_backend_interface_equals,
//
//	.element_get_memory_backend_interface =			_element_get_memory_backend_interface,
//	.element_read_memory_backend_interface =		_element_read_memory_backend_interface,
//	.element_get_array_subrange0 =					_element_get_array_subrange0,
//	.element_get_pointer_subrange0 =				_element_get_pointer_subrange0,
//	.element_cast_pointer =							_element_cast_pointer,
//	.element_calculate_hierarchy_top_discriminator_values =
//													_element_calculate_hierarchy_top_discriminator_values,
//
//	.element_hierarchy_member_read_memory_backend_interface =
//													_element_hierarchy_member_read_memory_backend_interface,
//	.element_hierarchy_member_get_memory_backend_interface =
//													_element_hierarchy_member_get_memory_backend_interface,
//	.element_hierarchy_member_get_array_subrange0 =	_element_hierarchy_member_get_array_subrange0,
//	.element_hierarchy_member_get_pointer_subrange0 =
//													_element_hierarchy_member_get_pointer_subrange0,
//	.element_hierarchy_member_cast_pointer =		_element_hierarchy_member_cast_pointer,
//	/* Optional handlers */
//	.object_root_validate =							_object_root_validate,
};
/*****************************************************************************/
int memory_backend_interface_create_from_json_ex(
		json_object *								p_master_json,
		const char *								override_current_object_json_path,
		metac_data_member_location_t				override_current_object_extra_offset,
		struct memory_backend_interface **			pp_memory_backend_interface) {

	json_object * p_json_root;
	json_object * p_json_pointer_object;

	if (p_master_json == NULL) {
		if (pp_memory_backend_interface != NULL) {
			*pp_memory_backend_interface = NULL;
		}
		return 0;
	}

	_create_with_error_result_(memory_backend_json, -(ENOMEM));
	memory_backend_interface(&p_memory_backend_json->memory_backend_interface, &ops);

	p_memory_backend_json->p_master_json = json_object_get(p_master_json);

	if (json_object_object_get_ex(
			p_memory_backend_json->p_master_json,
			"root",
			&p_json_root) == 0) {
		msg_stderr("root field wasn't found\n");
		memory_backend_json_delete(&p_memory_backend_json);
		return -(EFAULT);
	}

	if (json_object_object_get_ex(
			p_memory_backend_json->p_master_json,
			"entry_point",
			&p_json_pointer_object) == 0) {
		msg_stderr("entry_point field wasn't found\n");
		json_object_put(p_json_root);
		memory_backend_json_delete(&p_memory_backend_json);
		return -(EFAULT);
	}

	if (override_current_object_json_path == NULL) {

		json_object * p_json_json_path,
					* p_json_extra_offset;

		if (json_object_object_get_ex(
				p_json_pointer_object,
				"json_path",
				&p_json_json_path) == 0) {
			msg_stderr("json_path field wasn't found\n");
			json_object_put(p_json_pointer_object);
			json_object_put(p_json_root);
			memory_backend_json_delete(&p_memory_backend_json);
			return -(EFAULT);
		}

		override_current_object_json_path = json_object_get_string(p_json_json_path);

		if (json_object_object_get_ex(
				p_json_pointer_object,
				"extra_offset",
				&p_json_extra_offset) == 1) {
			override_current_object_extra_offset = (metac_data_member_location_t)json_object_get_int64(p_json_extra_offset);
			json_object_put(p_json_extra_offset);
		}

		if (json_pointer_get(
				p_json_root,
				override_current_object_json_path,
				&p_memory_backend_json->p_current_object_json) != 0){
			msg_stderr("wasn't able to find object by path %s\n", override_current_object_json_path);
			json_object_put(p_json_json_path);
			json_object_put(p_json_pointer_object);
			json_object_put(p_json_root);
			memory_backend_json_delete(&p_memory_backend_json);
			return -(EFAULT);
		}

		p_memory_backend_json->current_object_extra_offset = override_current_object_extra_offset;

		json_object_put(p_json_json_path);
	} else {

		if (json_pointer_get(
				p_json_root,
				override_current_object_json_path,
				&p_memory_backend_json->p_current_object_json) != 0){
			msg_stderr("wasn't able to find object by path %s\n", override_current_object_json_path);
			json_object_put(p_json_pointer_object);
			json_object_put(p_json_root);
			memory_backend_json_delete(&p_memory_backend_json);
			return -(EFAULT);
		}

		p_memory_backend_json->current_object_extra_offset = override_current_object_extra_offset;
	}

	json_object_put(p_json_pointer_object);
	json_object_put(p_json_root);

	if (pp_memory_backend_interface != NULL) {
		*pp_memory_backend_interface =  &p_memory_backend_json->memory_backend_interface;;
	}

	return 0;
}

