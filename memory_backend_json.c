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
#include <string.h>			/* strdup */
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
struct memory_backend_json {
	struct memory_backend_interface					memory_backend_interface;
	json_object *									p_master_json;
	json_object *									p_current_object_json;
};
/*****************************************************************************/
static struct memory_backend_json * memory_backend_json_create(
		json_object *								p_master_json,
		json_object *								p_current_object_json);
int memory_backend_interface_create_from_json(
		json_object *								p_master_json,
		struct memory_backend_interface **			pp_memory_backend_interface);
int memory_backend_interface_create_from_json_ex(
		json_object *								p_master_json,
		const char *								override_current_object_path,		/* JSON pointer notation as defined in RFC 6901 */
		struct memory_backend_interface **			pp_memory_backend_interface);
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

static json_object * _get_root(
		json_object *								p_master_json) {
	json_object * p_json_root;

	if (json_object_object_get_ex(
			p_master_json,
			"root",
			&p_json_root) == 0) {
		msg_stderr("root field wasn't found\n");
		return NULL;
	}
	return p_json_root;
}

static json_object * _get_pointer_destination(
		json_object *								p_json_pointer_object,
		json_object *								p_json_root) {
	json_object * p_current_object_json;
	json_object * p_json_json_path;
	const char * json_path;

	if (json_object_object_get_ex(
			p_json_pointer_object,
			"json_path",
			&p_json_json_path) == 0) {
		msg_stderr("json_path field wasn't found\n");
		return NULL;
	}

	json_path = json_object_get_string(p_json_json_path);

	if (json_pointer_get(
			p_json_root,
			json_path,
			&p_current_object_json) != 0){
		msg_stderr("wasn't able to find object by path %s\n", json_path);
		json_object_put(p_json_json_path);
		return NULL;
	}

	json_object_put(p_json_json_path);

	return p_current_object_json;
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

static int _memory_backend_interface_equals(
		struct memory_backend_interface *			p_memory_backend_interface0,
		struct memory_backend_interface *			p_memory_backend_interface1) {
	struct memory_backend_json * p_memory_backend_json0;
	struct memory_backend_json * p_memory_backend_json1;

	assert(p_memory_backend_interface0);
	assert(p_memory_backend_interface1);

	p_memory_backend_json0 = memory_backend_json(p_memory_backend_interface0);
	p_memory_backend_json1 = memory_backend_json(p_memory_backend_interface1);

	return
			p_memory_backend_json0->p_master_json == p_memory_backend_json0->p_master_json &&
			p_memory_backend_json0->p_current_object_json == p_memory_backend_json1->p_current_object_json;
}
/*****************************************************************************/
static int _element_get_memory_backend_interface(
		struct element *							p_element,
		struct memory_backend_interface **			pp_memory_backend_interface) {

	int i;
	metac_array_info_t * p_array_indexes;

	json_object * p_current_object_json;
	struct memory_backend_json * p_memory_backend_json;
	struct memory_backend_json * p_result_memory_backend_json;

	assert(p_element);
	assert(p_element->p_memory_block);
	assert(p_element->p_memory_block->p_memory_backend_interface);

	p_memory_backend_json = memory_backend_json(p_element->p_memory_block->p_memory_backend_interface);
	assert(p_memory_backend_json);

	p_array_indexes = metac_array_info_convert_linear_id_2_subranges(
			p_element->p_memory_block->p_array_info,
			p_element->id);
	if (p_array_indexes == NULL) {
		msg_stderr("metac_array_info_convert_linear_id_2_subranges failed\n");
		return -(EFAULT);
	}

	p_current_object_json = json_object_get(p_memory_backend_json->p_current_object_json);
	for (i = 0; i < p_array_indexes->subranges_count; ++i) {

		json_object * p_element_object_json;

		/*TODO: compare len with p_element->p_element_type->p_actual_type->array_type_info.subranges[0].count */

		if (json_object_get_type(p_current_object_json) != json_type_array) {
			msg_stderr("expected array and found %s\n", json_object_to_json_string(p_current_object_json));
			metac_array_info_delete(&p_array_indexes);
			json_object_put(p_current_object_json);
			return -(EFAULT);
		}

		p_element_object_json = json_object_array_get_idx(p_current_object_json, p_array_indexes->subranges[i].count);
		json_object_put(p_current_object_json);
		p_current_object_json = p_element_object_json;
	}

	/* ok, let's create memory_backend_json with this current_object_json*/
	p_result_memory_backend_json = memory_backend_json_create(
			p_memory_backend_json->p_master_json,
			p_current_object_json);
	json_object_put(p_current_object_json);

	if (p_result_memory_backend_json == NULL) {
		msg_stderr("memory_backend_json_create failed\n");
		return -(EFAULT);
	}

	if (pp_memory_backend_interface != NULL) {
		*pp_memory_backend_interface =  &p_memory_backend_json->memory_backend_interface;
	}

	return 0;
}

static int _element_read_memory_backend_interface(
		struct element *							p_element,
		struct memory_backend_interface **			pp_memory_backend_interface) {

	json_object * p_json_root;
	json_object * p_current_object_json;

	struct memory_backend_json * p_memory_backend_json;
	struct memory_backend_json * p_result_memory_backend_json;

	struct memory_backend_interface * p_memory_backend_interface;

	if (_element_get_memory_backend_interface(
			p_element,
			&p_memory_backend_interface) != 0) {
		msg_stderr("_element_get_memory_backend_interface failed\n");
		return -(EFAULT);
	}

	p_memory_backend_json = memory_backend_json(p_memory_backend_interface);
	assert(p_memory_backend_json);

	/* p_memory_backend_json->p_current_object_json contains pointer */
	p_json_root = _get_root(p_memory_backend_json->p_master_json);
	if (p_json_root == NULL) {
		msg_stderr("root field wasn't found\n");
		return -(EFAULT);
	}

	p_current_object_json = _get_pointer_destination(
			p_memory_backend_json->p_current_object_json,
			p_json_root);

	json_object_put(p_json_root);
	memory_backend_interface_put(&p_memory_backend_interface);

	if (p_current_object_json == NULL) {
		msg_stderr("_get_pointer_destination failed\n");
		return -(EFAULT);
	}

	/* ok, let's create the final memory_backend_json */
	p_result_memory_backend_json = memory_backend_json_create(
			p_memory_backend_json->p_master_json,
			p_current_object_json);
	json_object_put(p_current_object_json);

	if (p_result_memory_backend_json == NULL) {
		msg_stderr("memory_backend_json_create failed\n");
		return -(EFAULT);
	}

	if (pp_memory_backend_interface != NULL) {
		*pp_memory_backend_interface =  &p_memory_backend_json->memory_backend_interface;
	}

	return 0;
}

static int _element_get_array_subrange0(
		struct element *							p_element) {

	struct memory_backend_json * p_memory_backend_json;
	struct memory_backend_interface * p_memory_backend_interface;

	if (_element_get_memory_backend_interface(
			p_element,
			&p_memory_backend_interface) != 0) {
		msg_stderr("_element_get_memory_backend_interface failed\n");
		return -(EFAULT);
	}

	p_memory_backend_json = memory_backend_json(p_memory_backend_interface);
	assert(p_memory_backend_json);

	/* p_memory_backend_json->p_current_object_json contains array */
	assert(p_element->array.is_flexible != 0);
	p_element->array.subrange0_count = json_object_array_length(p_memory_backend_json->p_current_object_json);

	memory_backend_interface_put(&p_memory_backend_interface);

	return 0;
}

static int _element_get_pointer_subrange0(
		struct element *							p_element) {

	struct memory_backend_json * p_memory_backend_json;
	struct memory_backend_interface * p_memory_backend_interface;

	if (_element_read_memory_backend_interface(
			p_element,
			&p_memory_backend_interface) != 0) {
		msg_stderr("_element_read_memory_backend_interface failed\n");
		return -(EFAULT);
	}

	p_memory_backend_json = memory_backend_json(p_memory_backend_interface);
	assert(p_memory_backend_json);

	/* p_memory_backend_json->p_current_object_json contains array */
	p_element->pointer.subrange0_count = json_object_array_length(p_memory_backend_json->p_current_object_json);

	memory_backend_interface_put(&p_memory_backend_interface);
	return 0;
}

static int _element_cast_pointer(
		struct element *							p_element) {

	json_object * json_object_cast_id;
	json_object * json_object_casted_based_original_offset;

	struct memory_backend_json * p_memory_backend_json;
	struct memory_backend_interface * p_memory_backend_interface;

	if (_element_read_memory_backend_interface(
			p_element,
			&p_memory_backend_interface) != 0) {
		msg_stderr("_element_read_memory_backend_interface failed\n");
		return -(EFAULT);
	}

	p_memory_backend_json = memory_backend_json(p_memory_backend_interface);
	assert(p_memory_backend_json);

	p_element->pointer.use_cast = 0;
	p_element->pointer.casted_based_original_offset = 0;
	p_element->pointer.p_casted_element_type = p_element->p_element_type->pointer.p_element_type;

	if (json_object_object_get_ex(
			p_memory_backend_json->p_current_object_json,
			"cast_id",
			&json_object_cast_id) == 0) {
		/*not casted*/
		memory_backend_interface_get(p_element->pointer.p_original_memory_backend_interface, &p_element->pointer.p_casted_memory_backend_interface);
		memory_backend_interface_put(&p_memory_backend_interface);
		return 0;
	}
	p_element->pointer.use_cast = 1;

	/*TDOO: check json_object_cast_id type*/
	p_element->pointer.generic_cast_type_id = json_object_get_int(json_object_cast_id);
	json_object_put(json_object_cast_id);

	if (p_element->pointer.generic_cast_type_id >= p_element->p_element_type->pointer.generic_cast.types_count) {
		msg_stderr("generic_cast.cb set incorrect generic_cast_type_id %d. Maximum value is %d\n",
				p_element->pointer.generic_cast_type_id,
				p_element->p_element_type->pointer.generic_cast.types_count);
		memory_backend_interface_put(&p_memory_backend_interface);
		return -EINVAL;
	}

	p_element->pointer.p_casted_element_type = p_element->p_element_type->pointer.generic_cast.p_types[p_element->pointer.generic_cast_type_id].p_element_type;

	if (json_object_object_get_ex(
			p_memory_backend_json->p_current_object_json,
			"casted_based_original_offset",
			&json_object_casted_based_original_offset) == 0) {
		/*no offset - exiting*/
		memory_backend_interface_put(&p_memory_backend_interface);
		return 0;
	}

	/*TDOO: check json_object_casted_based_original_offset type*/
	p_element->pointer.casted_based_original_offset = json_object_get_int(json_object_casted_based_original_offset);

	json_object_put(json_object_casted_based_original_offset);

	memory_backend_interface_put(&p_memory_backend_interface);
	return 0;
}
/*****************************************************************************/
static int _get_hierarchy_related_json(
		json_object *								p_json_root,
		struct element_type_hierarchy *				p_hierarchy,
		json_object **								pp_json_hierarchy_related) {

	int i = 0,
		count = 0;
	struct element_type_hierarchy * p_current_hierarchy;
	json_object * p_current_json_root;

	metac_name_t * p_names;

	/* calculate all field_names till element top */
	p_current_hierarchy = p_hierarchy;
	while (p_current_hierarchy->parent != NULL) {
		struct element_type_hierarchy_member * p_element_type_hierarchy_member =
				element_type_hierarchy_get_element_hierarchy_member(p_current_hierarchy);
		if (p_current_hierarchy == NULL) {
			break;
		}

		if (p_element_type_hierarchy_member->p_member_info->name != NULL &&
			strcmp(p_element_type_hierarchy_member->p_member_info->name, METAC_ANON_MEMBER_NAME) != 0) {
			++count;
		}

		p_current_hierarchy = p_element_type_hierarchy_member->p_hierarchy;
	}

	if (count == 0) {

		if (pp_json_hierarchy_related) {
			*pp_json_hierarchy_related = json_object_get(p_json_root);
		}

		return 0;
	}

	p_names = (metac_name_t *)calloc(count, sizeof(*p_names));
	if (p_names == NULL) {
		msg_stderr("can't allocate mem\n");
		return -(ENOMEM);
	}

	/* repeat the same: calculate all field_names till element top, but now store the data (yeah... TODO: some dynamic types like stack would simplify the code) */
	p_current_hierarchy = p_hierarchy;
	while (p_current_hierarchy->parent != NULL) {
		struct element_type_hierarchy_member * p_element_type_hierarchy_member =
				element_type_hierarchy_get_element_hierarchy_member(p_current_hierarchy);
		if (p_current_hierarchy == NULL) {
			break;
		}

		if (p_element_type_hierarchy_member->p_member_info->name != NULL &&
			strcmp(p_element_type_hierarchy_member->p_member_info->name, METAC_ANON_MEMBER_NAME) != 0) {

			p_names[count - i - 1] = p_element_type_hierarchy_member->p_member_info->name;
			++i;
		}

		p_current_hierarchy = p_element_type_hierarchy_member->p_hierarchy;
	}

	p_current_json_root = json_object_get(p_json_root);
	for (i = 0; i < count; ++i) {
		json_object * p_json;

		if (json_object_get_type(p_current_json_root) != json_type_object) {
			msg_stderr("expected object and found %s\n", json_object_to_json_string(p_current_json_root));
			json_object_put(p_current_json_root);
			return -(EFAULT);
		}

		p_json = json_object_object_get(p_current_json_root, p_names[i]);
		if (p_json == NULL) {
			msg_stderr("can't find object %s in json %s\n",
					p_names[i],
					json_object_to_json_string(p_current_json_root));
			json_object_put(p_current_json_root);
			return -(ENOKEY);
		}

		json_object_put(p_current_json_root);
		p_current_json_root = json_object_get(p_json);
		json_object_put(p_json);
	}

	if (pp_json_hierarchy_related) {
		*pp_json_hierarchy_related = p_current_json_root;
	}

	return 0;
}

static metac_flag _is_hierarchy_closest_non_anon_member(
		struct element_type_hierarchy *				p_hierarchy,
		struct element_type_hierarchy_member *		p_hierarchy_member,
		metac_discriminator_value_t *				p_value) {

	struct element_type_hierarchy_member * p_current_hierarchy_member;
	metac_discriminator_value_t value;

	p_current_hierarchy_member = p_hierarchy_member;
	/* TODO: create metac_type_member_info_is_anonymous or similar */
	if (p_current_hierarchy_member->p_member_info->name == NULL ||
		strcmp(p_current_hierarchy_member->p_member_info->name, METAC_ANON_MEMBER_NAME) == 0) {
		return 0;
	}

	while (p_current_hierarchy_member != NULL) {
		if (p_current_hierarchy_member->p_hierarchy == p_hierarchy) {
			if (p_value) {
				*p_value = p_current_hierarchy_member->member_id;
			}
			return 1;
		}

		p_current_hierarchy_member = element_type_hierarchy_member_get_parent_element_hierarchy_member(p_current_hierarchy_member);

		if (p_current_hierarchy_member->p_member_info->name != NULL &&
			strcmp(p_current_hierarchy_member->p_member_info->name, METAC_ANON_MEMBER_NAME) != 0) {
			return 0;
		}
	}

	return 0;
}

static int _guess_discriminator_value(
		struct element_type *						p_element_type,
		struct element_type_hierarchy *				p_hierarchy,
		json_object *								p_hierarchy_json,
		metac_discriminator_value_t *				p_value) {

	int i;
	metac_flag value_is_set = 0;
	metac_discriminator_value_t value = 0;

	assert(p_element_type->p_actual_type->id == DW_TAG_union_type || p_element_type->p_actual_type->id == DW_TAG_structure_type);

	for (i = 0; i < p_element_type->hierarchy_top.members_count; ++i) {

		json_object * p_json;
		metac_discriminator_value_t current_value = 0;

		if (_is_hierarchy_closest_non_anon_member(
				p_hierarchy,
				p_element_type->hierarchy_top.pp_members[i],
				&current_value) == 1) {

			/*check if exists*/
			p_json = json_object_object_get(p_hierarchy_json, p_element_type->hierarchy_top.pp_members[i]->p_member_info->name);
			if (p_json != NULL) {	/*found the field*/

				if (value_is_set != 0) {
					if (value != current_value) {
						json_object_put(p_json);
						msg_stderr("wasn't able to identify discriminator value, because of ambiguity: already had %d, but now getting %d\n",
								(int)value,
								(int)current_value);
						return -(EFAULT);
					}
				} else {
					value_is_set = 1;
					value = current_value;
				}

				json_object_put(p_json);
			}
		}
	}

	if (value_is_set &&
		p_value != NULL) {
		*p_value = value;
	}

	return 0;
}

static int _element_calculate_hierarchy_top_discriminator_values(
		struct element *							p_element) {

	int i;

	json_object * p_json_root;
	json_object * p_current_object_json;

	struct memory_backend_json * p_memory_backend_json;
	struct memory_backend_json * p_result_memory_backend_json;

	struct memory_backend_interface * p_memory_backend_interface;

	if (_element_get_memory_backend_interface(
			p_element,
			&p_memory_backend_interface) != 0) {
		msg_stderr("_element_get_memory_backend_interface failed\n");
		return -(EFAULT);
	}

	p_memory_backend_json = memory_backend_json(p_memory_backend_interface);
	assert(p_memory_backend_json);

	if (p_memory_backend_json->p_current_object_json == NULL) {
		/* there is nothing to calc - this element isn't defined in json */
		return 0;
	}

	/* now check p_memory_backend_json->p_current_object_json and try to compare with
	 * p_element->p_element_type->hierarchy_top
	 */
	for (i = 0; i < p_element->p_element_type->hierarchy_top.discriminators_count; ++i) {

		int result;
		int j;
		json_object * p_hierarchy_json;

		if (p_element->hierarchy_top.pp_discriminator_values[i] == NULL) {
			continue;
		}

		result = _get_hierarchy_related_json(
				p_memory_backend_json->p_current_object_json,
				p_element->p_element_type->hierarchy_top.pp_discriminators[i]->p_hierarchy,
				&p_hierarchy_json);

		if (result == -(ENOENT)) {
			/*just skip*/
			continue;
		} else if (result != 0) {
			msg_stderr("_element_get_memory_backend_interface failed\n");
			memory_backend_interface_put(&p_memory_backend_interface);
			return result;

		}

		/* ok p_hierarchy_json contains json corresponding to p_element->p_element_type->hierarchy_top.pp_discriminators[i]->p_hierarchy
		 * try to guess union member id by names of fields
		 */
		if (_guess_discriminator_value(
				p_element->p_element_type,
				p_element->p_element_type->hierarchy_top.pp_discriminators[i]->p_hierarchy,
				p_hierarchy_json,
				&p_element->hierarchy_top.pp_discriminator_values[i]->value) != 0) {
			msg_stderr("_guess_discriminator_value failed\n");
			json_object_put(p_hierarchy_json);
			memory_backend_interface_put(&p_memory_backend_interface);
			return -(EFAULT);
		}

		json_object_put(p_hierarchy_json);
	}

	memory_backend_interface_put(&p_memory_backend_interface);

	return 0;
}

/*****************************************************************************/
static int _element_hierarchy_member_get_memory_backend_interface(
		struct element_hierarchy_member *			p_element_hierarchy_member,
		struct memory_backend_interface **			pp_memory_backend_interface) {

	int i;
	metac_array_info_t * p_array_indexes;

	json_object * p_current_object_json;
	struct memory_backend_json * p_memory_backend_json;
	struct memory_backend_json * p_result_memory_backend_json;

	struct memory_backend_interface * p_element_memory_backend_interface;

	assert(p_element_hierarchy_member);
	assert(p_element_hierarchy_member->p_element);
	assert(p_element_hierarchy_member->p_element->p_memory_block);
	assert(p_element_hierarchy_member->p_element->p_memory_block->p_memory_backend_interface);

	if (_element_get_memory_backend_interface(
			p_element_hierarchy_member->p_element,
			&p_element_memory_backend_interface) != 0) {
		msg_stderr("_element_get_memory_backend_interface failed\n");
		return -(EFAULT);
	}

	/*this is json for element*/
	p_memory_backend_json = memory_backend_json(p_element_memory_backend_interface);
	assert(p_memory_backend_json);



	memory_backend_interface_put(&p_element_memory_backend_interface);

//	p_memory_backend_json = memory_backend_json(p_element_hierarchy_member->p_element->p_memory_block->p_memory_backend_interface);
//	assert(p_memory_backend_json);
//
//	p_array_indexes = metac_array_info_convert_linear_id_2_subranges(
//			p_element->p_memory_block->p_array_info,
//			p_element->id);
//	if (p_array_indexes == NULL) {
//		msg_stderr("metac_array_info_convert_linear_id_2_subranges failed\n");
//		return -(EFAULT);
//	}
//
//	p_current_object_json = json_object_get(p_memory_backend_json->p_current_object_json);
//	for (i = 0; i < p_array_indexes->subranges_count; ++i) {
//
//		json_object * p_element_object_json;
//
//		/*TODO: compare len with p_element->p_element_type->p_actual_type->array_type_info.subranges[0].count */
//
//		if (json_object_get_type(p_current_object_json) != json_type_array) {
//			msg_stderr("expected array and found %s\n", json_object_to_json_string(p_current_object_json));
//			metac_array_info_delete(&p_array_indexes);
//			json_object_put(p_current_object_json);
//			return -(EFAULT);
//		}
//
//		p_element_object_json = json_object_array_get_idx(p_current_object_json, p_array_indexes->subranges[i].count);
//		json_object_put(p_current_object_json);
//		p_current_object_json = p_element_object_json;
//	}
//
//	/* ok, let's create memory_backend_json with this current_object_json*/
//	p_result_memory_backend_json = memory_backend_json_create(
//			p_memory_backend_json->p_master_json,
//			p_current_object_json);
//	json_object_put(p_current_object_json);
//
//	if (p_result_memory_backend_json == NULL) {
//		msg_stderr("memory_backend_json_create failed\n");
//		return -(EFAULT);
//	}
//
//	if (pp_memory_backend_interface != NULL) {
//		*pp_memory_backend_interface =  &p_memory_backend_json->memory_backend_interface;
//	}

	return 0;
}

static int _element_hierarchy_member_read_memory_backend_interface(
		struct element_hierarchy_member *			p_element_hierarchy_member,
		struct memory_backend_interface **			pp_memory_backend_interface) {

	json_object * p_json_root;
	json_object * p_current_object_json;

	struct memory_backend_json * p_memory_backend_json;
	struct memory_backend_json * p_result_memory_backend_json;

	struct memory_backend_interface * p_memory_backend_interface;

	if (_element_hierarchy_member_get_memory_backend_interface(
			p_element_hierarchy_member,
			&p_memory_backend_interface) != 0) {
		msg_stderr("_element_hierarchy_member_get_memory_backend_interface failed\n");
		return -(EFAULT);
	}

	p_memory_backend_json = memory_backend_json(p_memory_backend_interface);
	assert(p_memory_backend_json);

	/* p_memory_backend_json->p_current_object_json contains pointer */
	p_json_root = _get_root(p_memory_backend_json->p_master_json);
	if (p_json_root == NULL) {
		msg_stderr("root field wasn't found\n");
		return -(EFAULT);
	}

	p_current_object_json = _get_pointer_destination(
			p_memory_backend_json->p_current_object_json,
			p_json_root);

	json_object_put(p_json_root);
	memory_backend_interface_put(&p_memory_backend_interface);

	if (p_current_object_json == NULL) {
		msg_stderr("_get_pointer_destination failed\n");
		return -(EFAULT);
	}

	/* ok, let's create the final memory_backend_json */
	p_result_memory_backend_json = memory_backend_json_create(
			p_memory_backend_json->p_master_json,
			p_current_object_json);
	json_object_put(p_current_object_json);

	if (p_result_memory_backend_json == NULL) {
		msg_stderr("memory_backend_json_create failed\n");
		return -(EFAULT);
	}

	if (pp_memory_backend_interface != NULL) {
		*pp_memory_backend_interface =  &p_memory_backend_json->memory_backend_interface;
	}

	return 0;
}

static int _element_hierarchy_member_get_array_subrange0(
		struct element_hierarchy_member *			p_element_hierarchy_member) {

	struct memory_backend_json * p_memory_backend_json;
	struct memory_backend_interface * p_memory_backend_interface;

	if (_element_hierarchy_member_get_memory_backend_interface(
			p_element_hierarchy_member,
			&p_memory_backend_interface) != 0) {
		msg_stderr("_element_hierarchy_member_get_memory_backend_interface failed\n");
		return -(EFAULT);
	}

	p_memory_backend_json = memory_backend_json(p_memory_backend_interface);
	assert(p_memory_backend_json);

	/* p_memory_backend_json->p_current_object_json contains array */
	assert(p_element_hierarchy_member->array.is_flexible != 0);
	p_element_hierarchy_member->array.subrange0_count = json_object_array_length(p_memory_backend_json->p_current_object_json);

	memory_backend_interface_put(&p_memory_backend_interface);

	return 0;
}

static int _element_hierarchy_member_get_pointer_subrange0(
		struct element_hierarchy_member *			p_element_hierarchy_member) {

	struct memory_backend_json * p_memory_backend_json;
	struct memory_backend_interface * p_memory_backend_interface;

	if (_element_hierarchy_member_read_memory_backend_interface(
			p_element_hierarchy_member,
			&p_memory_backend_interface) != 0) {
		msg_stderr("_element_hierarchy_member_read_memory_backend_interface failed\n");
		return -(EFAULT);
	}

	p_memory_backend_json = memory_backend_json(p_memory_backend_interface);
	assert(p_memory_backend_json);

	/* p_memory_backend_json->p_current_object_json contains array */
	p_element_hierarchy_member->pointer.subrange0_count = json_object_array_length(p_memory_backend_json->p_current_object_json);

	memory_backend_interface_put(&p_memory_backend_interface);
	return 0;
}

static int _element_hierarchy_member_cast_pointer(
		struct element_hierarchy_member *			p_element_hierarchy_member) {

	json_object * json_object_cast_id;
	json_object * json_object_casted_based_original_offset;

	struct memory_backend_json * p_memory_backend_json;
	struct memory_backend_interface * p_memory_backend_interface;

	if (_element_hierarchy_member_read_memory_backend_interface(
			p_element_hierarchy_member,
			&p_memory_backend_interface) != 0) {
		msg_stderr("_element_hierarchy_member_read_memory_backend_interface failed\n");
		return -(EFAULT);
	}

	p_memory_backend_json = memory_backend_json(p_memory_backend_interface);
	assert(p_memory_backend_json);

	p_element_hierarchy_member->pointer.use_cast = 0;
	p_element_hierarchy_member->pointer.casted_based_original_offset = 0;
	p_element_hierarchy_member->pointer.p_casted_element_type = p_element_hierarchy_member->p_element_type_hierarchy_member->pointer.p_element_type;

	if (json_object_object_get_ex(
			p_memory_backend_json->p_current_object_json,
			"cast_id",
			&json_object_cast_id) == 0) {
		/*not casted*/
		memory_backend_interface_get(p_element_hierarchy_member->pointer.p_original_memory_backend_interface, &p_element_hierarchy_member->pointer.p_casted_memory_backend_interface);
		memory_backend_interface_put(&p_memory_backend_interface);
		return 0;
	}
	p_element_hierarchy_member->pointer.use_cast = 1;

	/*TDOO: check json_object_cast_id type*/
	p_element_hierarchy_member->pointer.generic_cast_type_id = json_object_get_int(json_object_cast_id);
	json_object_put(json_object_cast_id);

	if (p_element_hierarchy_member->pointer.generic_cast_type_id >= p_element_hierarchy_member->p_element_type_hierarchy_member->pointer.generic_cast.types_count) {
		msg_stderr("generic_cast.cb set incorrect generic_cast_type_id %d. Maximum value is %d\n",
				p_element_hierarchy_member->pointer.generic_cast_type_id,
				p_element_hierarchy_member->p_element_type_hierarchy_member->pointer.generic_cast.types_count);
		memory_backend_interface_put(&p_memory_backend_interface);
		return -EINVAL;
	}

	p_element_hierarchy_member->pointer.p_casted_element_type = p_element_hierarchy_member->p_element_type_hierarchy_member->pointer.generic_cast.p_types[p_element_hierarchy_member->pointer.generic_cast_type_id].p_element_type;

	if (json_object_object_get_ex(
			p_memory_backend_json->p_current_object_json,
			"casted_based_original_offset",
			&json_object_casted_based_original_offset) == 0) {
		/*no offset - exiting*/
		memory_backend_interface_put(&p_memory_backend_interface);
		return 0;
	}

	/*TDOO: check json_object_casted_based_original_offset type*/
	p_element_hierarchy_member->pointer.casted_based_original_offset = json_object_get_int(json_object_casted_based_original_offset);

	json_object_put(json_object_casted_based_original_offset);

	memory_backend_interface_put(&p_memory_backend_interface);
	return 0;
}
/*****************************************************************************/
static struct memory_backend_interface_ops ops = {
	/* Mandatory handlers */
	.memory_backend_interface_delete =				_memory_backend_interface_delete,
	.memory_backend_interface_equals =				_memory_backend_interface_equals,

	.element_get_memory_backend_interface =			_element_get_memory_backend_interface,
	.element_read_memory_backend_interface =		_element_read_memory_backend_interface,
	.element_get_array_subrange0 =					_element_get_array_subrange0,
	.element_get_pointer_subrange0 =				_element_get_pointer_subrange0,
	.element_cast_pointer =							_element_cast_pointer,
	.element_calculate_hierarchy_top_discriminator_values =
													_element_calculate_hierarchy_top_discriminator_values,

	.element_hierarchy_member_read_memory_backend_interface =
													_element_hierarchy_member_read_memory_backend_interface,
	.element_hierarchy_member_get_memory_backend_interface =
													_element_hierarchy_member_get_memory_backend_interface,
	.element_hierarchy_member_get_array_subrange0 =	_element_hierarchy_member_get_array_subrange0,
	.element_hierarchy_member_get_pointer_subrange0 =
													_element_hierarchy_member_get_pointer_subrange0,
	.element_hierarchy_member_cast_pointer =		_element_hierarchy_member_cast_pointer,
	/* Optional handlers */
	/*.object_root_validate =							_object_root_validate,*/
};
/*****************************************************************************/
static struct memory_backend_json * memory_backend_json_create(
		json_object *								p_master_json,
		json_object *								p_current_object_json) {

	_create_(memory_backend_json);
	memory_backend_interface(&p_memory_backend_json->memory_backend_interface, &ops);
	p_memory_backend_json->p_master_json = json_object_get(p_master_json);
	p_memory_backend_json->p_current_object_json = json_object_get(p_current_object_json);

	return p_memory_backend_json;
}

int memory_backend_interface_create_from_json_ex(
		json_object *								p_master_json,
		const char *								override_current_object_json_path,
		struct memory_backend_interface **			pp_memory_backend_interface) {

	struct memory_backend_json * p_memory_backend_json;

	json_object * p_json_root;
	json_object * p_current_object_json;

	if (p_master_json == NULL) {
		if (pp_memory_backend_interface != NULL) {
			*pp_memory_backend_interface = NULL;
		}
		return 0;
	}

	p_json_root = _get_root(p_master_json);
	if (p_json_root == NULL) {
		msg_stderr("root field wasn't found\n");
		return -(EFAULT);
	}

	if (override_current_object_json_path == NULL) {

		json_object * p_json_pointer_object;

		if (json_object_object_get_ex(
				p_master_json,
				"entry_point",
				&p_json_pointer_object) == 0) {
			msg_stderr("entry_point field wasn't found\n");
			json_object_put(p_json_root);
			return -(EFAULT);
		}

		p_current_object_json = _get_pointer_destination(
				p_json_pointer_object,
				p_json_root);

		json_object_put(p_json_pointer_object);

		if (p_current_object_json == NULL) {
			msg_stderr("_get_pointer_destination failed\n");
			json_object_put(p_json_root);
			return -(EFAULT);
		}

	} else {

		if (json_pointer_get(
				p_json_root,
				override_current_object_json_path,
				&p_current_object_json) != 0){
			msg_stderr("wasn't able to find object by path %s\n", override_current_object_json_path);
			json_object_put(p_json_root);
			return -(EFAULT);
		}
	}

	json_object_put(p_json_root);

	p_memory_backend_json = memory_backend_json_create(
			p_master_json,
			p_current_object_json);
	json_object_put(p_current_object_json);

	if (p_memory_backend_json == NULL) {
		msg_stderr("memory_backend_json_create failed\n");
		return -(EFAULT);
	}

	if (pp_memory_backend_interface != NULL) {
		*pp_memory_backend_interface =  &p_memory_backend_json->memory_backend_interface;;
	}

	return 0;
}

int memory_backend_interface_create_from_json(
		json_object *								p_master_json,
		struct memory_backend_interface **			pp_memory_backend_interface) {
	return memory_backend_interface_create_from_json_ex(
			p_master_json,
			NULL,
			pp_memory_backend_interface);
}
