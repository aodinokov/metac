/*
 * metac_value_scheme.c
 *
 *  Created on: Feb 18, 2020
 *      Author: mralex
 */

#include <assert.h>			/* assert */
#include <errno.h>			/* ENOMEM etc */
#include <stdlib.h>			/* calloc, free, NULL, qsort... */
#include <string.h>			/* strdup */
#include <urcu/list.h>		/* struct cds_list_head etc */

#include "metac_debug.h"	/* msg_stderr, ...*/
#include "metac_oop.h"		/*_create, _delete, ...*/

#include "metac/metac_value_scheme.h"

//#define alloc_sptrinf(...) ({\
//	int len = snprintf(NULL, 0, ##__VA_ARGS__); \
//	char *str = calloc(len + 1, sizeof(*str)); \
//	if (str != NULL)snprintf(str, len + 1, ##__VA_ARGS__); \
//	str; \
//})

static struct generic_cast_type * generic_cast_type_create(
		struct metac_type **						types,
		metac_count_t *								p_types_count) {

	metac_count_t i = 0;
	struct generic_cast_type * p_generic_cast_type;

	if (types == NULL ||
		p_types_count == NULL)
		return NULL;

	while (types[i] != NULL)
		++i;
	if (i == 0)
		return NULL;

	p_generic_cast_type = calloc(i, sizeof(*p_generic_cast_type));
	if (p_generic_cast_type == NULL) {
		msg_stderr("Wasn't able to allocate memory for p_generic_cast_type\n");
		return NULL;
	}

	*p_types_count = i;
	for (i = 0 ; i < *p_types_count; ++i) {

		p_generic_cast_type[i].p_type = types[i];
	}

	return p_generic_cast_type;
}

static int value_scheme_with_array_init(
		struct value_scheme_with_array *			p_value_scheme_with_array,
		struct metac_type *							p_root_type,
		char * 										global_path,
		metac_type_annotation_t *					p_override_annotations,
		struct metac_type *							p_actual_type) {

	const metac_type_annotation_t * p_annotation;
	struct metac_type * p_target_actual_type;

	assert(p_actual_type->id == DW_TAG_array_type);

	p_annotation = metac_type_annotation(p_root_type, global_path, p_override_annotations);

	/*keep global_path as annotation_key*/
	p_value_scheme_with_array->annotation_key = strdup(global_path);
	if (p_value_scheme_with_array->annotation_key == NULL) {
		msg_stderr("Can't keep annotation_key for %s\n", global_path);
		return -ENOMEM;
	}

	/* defaults for char[] */
	if (p_actual_type->array_type_info.is_flexible != 0) {
		p_target_actual_type = metac_type_actual_type(p_actual_type->array_type_info.type);
		if (p_target_actual_type != NULL &&
			p_target_actual_type->id == DW_TAG_base_type && (
				p_target_actual_type->base_type_info.encoding == DW_ATE_signed_char ||
				p_target_actual_type->base_type_info.encoding == DW_ATE_unsigned_char) &&
				p_target_actual_type->name != NULL && (
				strcmp(p_target_actual_type->name, "char") == 0 ||
				strcmp(p_target_actual_type->name, "unsigned char") == 0)){

			p_value_scheme_with_array->array_elements_count.cb = metac_array_elements_1d_with_null;
		}
	}

	if (p_annotation != NULL) {
		if (p_actual_type->array_type_info.is_flexible != 0) {

			p_value_scheme_with_array->array_elements_count.cb = p_annotation->value->array_elements_count.cb;
			p_value_scheme_with_array->array_elements_count.p_data = p_annotation->value->array_elements_count.data;
		} else {

			if (p_annotation->value->array_elements_count.cb != NULL) {
				msg_stderr("Annotations for non-flexible array with global path %s defines array_elements_count that won't be used\n", global_path);
			}
		}

		if (p_annotation->value->generic_cast.cb != NULL) {
			msg_stderr("Annotations for array with global path %s defines generic_cast that won't be used\n", global_path);
		}
		if (p_annotation->value->discriminator.cb != NULL) {
			msg_stderr("Annotations for array with global path %s defines discriminator that won't be used\n", global_path);
		}
	}
	return 0;
}

static void value_scheme_with_array_clean(
		struct value_scheme_with_array *			p_value_scheme_with_array) {

	if (p_value_scheme_with_array->annotation_key != NULL) {

		free(p_value_scheme_with_array->annotation_key);
		p_value_scheme_with_array->annotation_key = NULL;
	}
}

static int value_scheme_with_pointer_init(
		struct value_scheme_with_pointer *			p_value_scheme_with_pointer,
		struct metac_type *							p_root_type,
		char * 										global_path,
		metac_type_annotation_t *					p_override_annotations,
		struct metac_type *							p_actual_type) {

	const metac_type_annotation_t * p_annotation;
	struct metac_type * p_target_actual_type;

	assert(p_actual_type->id == DW_TAG_pointer_type);

	p_annotation = metac_type_annotation(p_root_type, global_path, p_override_annotations);

	/*keep global_path as annotation_key*/
	p_value_scheme_with_pointer->annotation_key = strdup(global_path);
	if (p_value_scheme_with_pointer->annotation_key == NULL) {
		msg_stderr("Can't keep annotation_key for %s\n", global_path);
		return -ENOMEM;
	}

	/*defaults is metac_array_elements_single */
	p_value_scheme_with_pointer->array_elements_count.cb = metac_array_elements_single;

	/*default for char* is metac_array_elements_1d_with_null */
	if (p_actual_type->pointer_type_info.type != NULL) {

		p_target_actual_type = metac_type_actual_type(p_actual_type->pointer_type_info.type);
		if (p_target_actual_type != NULL &&
			p_target_actual_type->id == DW_TAG_base_type && (
				p_target_actual_type->base_type_info.encoding == DW_ATE_signed_char ||
				p_target_actual_type->base_type_info.encoding == DW_ATE_unsigned_char) &&
				p_target_actual_type->name != NULL && (
				strcmp(p_target_actual_type->name, "char") == 0 ||
				strcmp(p_target_actual_type->name, "unsigned char") == 0)) {

			p_value_scheme_with_pointer->array_elements_count.cb = metac_array_elements_1d_with_null;
		}
	}

	if (p_annotation != NULL) {
		p_value_scheme_with_pointer->generic_cast.cb = p_annotation->value->generic_cast.cb;
		p_value_scheme_with_pointer->generic_cast.p_data = p_annotation->value->generic_cast.data;
		p_value_scheme_with_pointer->generic_cast.p_types = generic_cast_type_create(
				p_annotation->value->generic_cast.types,
				&p_value_scheme_with_pointer->generic_cast.types_count);

		p_value_scheme_with_pointer->array_elements_count.cb = p_annotation->value->array_elements_count.cb;
		p_value_scheme_with_pointer->array_elements_count.p_data = p_annotation->value->array_elements_count.data;

		if (p_annotation->value->discriminator.cb != NULL) {
			msg_stderr("Annotations for pointer with global path %s defines discriminator that won't be used\n", global_path);
		}
	}
	return 0;
}

static void value_scheme_with_pointer_clean(
		struct value_scheme_with_pointer *			p_value_scheme_with_pointer) {

	if (p_value_scheme_with_pointer->annotation_key != NULL) {

		free(p_value_scheme_with_pointer->annotation_key);
		p_value_scheme_with_pointer->annotation_key = NULL;
	}
	if (p_value_scheme_with_pointer->generic_cast.p_types != NULL) {

		free(p_value_scheme_with_pointer->generic_cast.p_types);
		p_value_scheme_with_pointer->generic_cast.p_types = NULL;
	}
}

static int value_scheme_with_hierarchy_init(
		struct value_scheme_with_hierarchy *		p_value_scheme_with_hierarchy,
		struct metac_type *							p_root_type,
		char * 										global_path,
		metac_type_annotation_t *					p_override_annotations,
		struct metac_type *							p_actual_type) {

	const metac_type_annotation_t * p_annotation;

	assert(p_actual_type->id == DW_TAG_structure_type || p_actual_type->id == DW_TAG_union_type);

	p_annotation = metac_type_annotation(p_root_type, global_path, p_override_annotations);

	p_value_scheme_with_hierarchy->members_count = p_actual_type->structure_type_info.members_count;
	if (p_value_scheme_with_hierarchy->members_count > 0) {

		p_value_scheme_with_hierarchy->members =
				(struct metac_value_scheme **)calloc(
						p_value_scheme_with_hierarchy->members_count,
						sizeof(*p_value_scheme_with_hierarchy->members));

		if (p_value_scheme_with_hierarchy->members == NULL) {

			msg_stderr("Can't allocate memory for hierarchy\n");
			p_value_scheme_with_hierarchy->members_count = 0;

			return (-EFAULT);
		}
	}
	return 0;
}

static struct metac_value_scheme * value_scheme_with_hierarchy_get_value_scheme(
		struct value_scheme_with_hierarchy *		p_value_scheme_with_hierarchy) {
	if (p_value_scheme_with_hierarchy == NULL) {
		return NULL;
	}
	return cds_list_entry(p_value_scheme_with_hierarchy, struct metac_value_scheme, hierarchy);
}

static void value_scheme_with_hierarchy_clean(
		struct value_scheme_with_hierarchy *		p_value_scheme_with_hierarchy) {

	if (p_value_scheme_with_hierarchy->members != NULL) {

		free(p_value_scheme_with_hierarchy->members);
		p_value_scheme_with_hierarchy->members = NULL;
	}
}

int metac_value_scheme_clean_as_hierarchy_member(
		struct metac_value_scheme *					p_metac_value_scheme) {
	if (p_metac_value_scheme->p_actual_type)
		switch(p_metac_value_scheme->p_actual_type->id){
		case DW_TAG_structure_type:
		case DW_TAG_union_type:
			value_scheme_with_hierarchy_clean(&p_metac_value_scheme->hierarchy);
			break;
		case DW_TAG_array_type:
			value_scheme_with_array_clean(&p_metac_value_scheme->array);
			break;
		case DW_TAG_pointer_type:
			value_scheme_with_pointer_clean(&p_metac_value_scheme->pointer);
			break;
	}
	if (p_metac_value_scheme->hierarchy_member.path_within_hierarchy) {
		free(p_metac_value_scheme->hierarchy_member.path_within_hierarchy);
		p_metac_value_scheme->hierarchy_member.path_within_hierarchy = NULL;
	}
	return 0;
}

int metac_value_scheme_init_as_hierarchy_member(
		struct metac_value_scheme *					p_metac_value_scheme,
		struct metac_type *							p_root_type,
		char *										global_path,
		char *										hirarchy_path,
		metac_type_annotation_t *					p_override_annotations,
		struct value_scheme_with_hierarchy *		p_current_hierarchy,
		struct discriminator *						p_discriminator,
		metac_discriminator_value_t					expected_discriminator_value,
		metac_count_t								member_id,
		struct metac_type_member_info *				p_member_info) {
	struct metac_value_scheme * p_parent_metac_value_scheme;

	if (p_metac_value_scheme == NULL ||
		p_current_hierarchy == NULL) {
		msg_stderr("invalid argument for %s\n", hirarchy_path);
		return -(EINVAL);
	}

	p_metac_value_scheme->hierarchy_member.member_id = member_id;
	p_metac_value_scheme->hierarchy_member.p_member_info = p_member_info;

	p_metac_value_scheme->hierarchy_member.precondition.p_discriminator = p_discriminator;
	p_metac_value_scheme->hierarchy_member.precondition.expected_discriminator_value = expected_discriminator_value;
	p_metac_value_scheme->p_current_hierarchy = p_current_hierarchy;

	p_metac_value_scheme->hierarchy_member.offset = p_member_info->data_member_location;

	p_parent_metac_value_scheme = metac_value_scheme_get_parent_value_scheme(p_metac_value_scheme);
	if (p_parent_metac_value_scheme != NULL) {

		assert(metac_value_scheme_is_hierachy(p_parent_metac_value_scheme));

		p_metac_value_scheme->hierarchy_member.offset += p_parent_metac_value_scheme->hierarchy_member.offset;
	}

	p_metac_value_scheme->p_type = p_member_info->type;
	if (p_metac_value_scheme->p_type != NULL) {
		p_metac_value_scheme->p_actual_type = metac_type_actual_type(p_member_info->type);
		p_metac_value_scheme->byte_size = metac_type_byte_size(p_member_info->type);
	}

	p_metac_value_scheme->hierarchy_member.path_within_hierarchy = strdup(hirarchy_path);
	if (p_metac_value_scheme->hierarchy_member.path_within_hierarchy == NULL) {
		msg_stderr("wasn't able to build hirarchy_path for %s\n", hirarchy_path);
		metac_value_scheme_clean_as_hierarchy_member(p_metac_value_scheme);
		return -(ENOMEM);
	}

	/*msg_stddbg("local path: %s\n", p_metac_value_scheme->path_within_hierarchy);*/

	if (p_metac_value_scheme->p_actual_type != NULL) {
		int res = 0;

		switch(p_metac_value_scheme->p_actual_type->id) {
		case DW_TAG_structure_type:
		case DW_TAG_union_type:
			res = value_scheme_with_hierarchy_init(
					&p_metac_value_scheme->hierarchy,
					p_root_type,
					global_path,
					p_override_annotations,
					p_metac_value_scheme->p_actual_type);
			break;
		case DW_TAG_array_type:
			res = value_scheme_with_array_init(
					&p_metac_value_scheme->array,
					p_root_type,
					global_path,
					p_override_annotations,
					p_metac_value_scheme->p_actual_type);
			break;
		case DW_TAG_pointer_type:
			res = value_scheme_with_pointer_init(
					&p_metac_value_scheme->pointer,
					p_root_type,
					global_path,
					p_override_annotations,
					p_metac_value_scheme->p_actual_type);
			break;
		}

		if (res != 0) {

			msg_stddbg("initialization failed\n");

			metac_value_scheme_clean_as_hierarchy_member(p_metac_value_scheme);
			return -(EFAULT);
		}
	}

	return 0;
}

metac_flag_t metac_value_scheme_is_hierarchy_member(
		struct metac_value_scheme *					p_metac_value_scheme) {

	if (p_metac_value_scheme == NULL) {

		msg_stderr("invalid argument\n");
		return -(EINVAL);
	}

	return (p_metac_value_scheme->p_current_hierarchy == NULL)?0:1;
}

struct metac_value_scheme * metac_value_scheme_get_parent_value_scheme(
		struct metac_value_scheme *					p_metac_value_scheme) {

	if (p_metac_value_scheme == NULL ||
		p_metac_value_scheme->p_current_hierarchy == NULL) {

		return NULL;
	}

	return value_scheme_with_hierarchy_get_value_scheme(p_metac_value_scheme->p_current_hierarchy);
}

metac_flag_t metac_value_scheme_is_hierachy(
		struct metac_value_scheme *					p_metac_value_scheme) {

	if (p_metac_value_scheme != NULL &&
		p_metac_value_scheme->p_actual_type != NULL && (
			p_metac_value_scheme->p_actual_type->id == DW_TAG_structure_type ||
			p_metac_value_scheme->p_actual_type->id == DW_TAG_union_type)) {

		return 1;
	}

	return 0;
}

metac_flag_t metac_value_scheme_is_array(
		struct metac_value_scheme *					p_metac_value_scheme) {

	if (p_metac_value_scheme != NULL &&
		p_metac_value_scheme->p_actual_type != NULL && (
			p_metac_value_scheme->p_actual_type->id == DW_TAG_array_type)) {

		return 1;
	}

	return 0;
}

metac_flag_t metac_value_scheme_is_pointer(
		struct metac_value_scheme *					p_metac_value_scheme) {

	if (p_metac_value_scheme != NULL &&
		p_metac_value_scheme->p_actual_type != NULL && (
			p_metac_value_scheme->p_actual_type->id == DW_TAG_pointer_type
		)) {
		return 1;
	}

	return 0;
}

#if 0
int discriminator_delete(
		struct discriminator **						pp_discriminator) {
	_delete_start_(discriminator);
	if ((*pp_discriminator)->annotation_key != NULL) {
		free((*pp_discriminator)->annotation_key);
		(*pp_discriminator)->annotation_key = NULL;
	}
	(*pp_discriminator)->p_hierarchy = NULL;
	_delete_finish(discriminator);
	return 0;
}

struct discriminator * discriminator_create(
		struct metac_type *							p_root_type,
		char * 										global_path,
		metac_type_annotation_t *					p_override_annotations,
		struct discriminator *						p_previous_discriminator,
		metac_discriminator_value_t					previous_expected_discriminator_value,
		struct element_type_hierarchy *				p_hierarchy) {
	const metac_type_annotation_t * p_annotation;
	p_annotation = metac_type_annotation(p_root_type, global_path, p_override_annotations);

	if (p_annotation == NULL) {
		msg_stderr("Wasn't able to find annotation for type %s, path %s\n", p_root_type->name, global_path);
		return NULL;
	}
	if (p_annotation->value->discriminator.cb == NULL) {
		msg_stderr("Discriminator callback is NULL in annotation for %s\n", global_path);
		return NULL;
	}

	_create_(discriminator);
	if (p_previous_discriminator != NULL) {	/*copy precondition*/
		p_discriminator->precondition.p_discriminator = p_previous_discriminator;
		p_discriminator->precondition.expected_discriminator_value = previous_expected_discriminator_value;
	}

	/*keep global_path as annotation_key*/
	p_discriminator->annotation_key = strdup(global_path);
	if (p_discriminator->annotation_key == NULL) {
		msg_stderr("Can't keep annotation_key for %s\n", global_path);
		discriminator_delete(&p_discriminator);
		return NULL;
	}

	p_discriminator->cb = p_annotation->value->discriminator.cb;
	p_discriminator->p_data = p_annotation->value->discriminator.data;
	p_discriminator->p_hierarchy = p_hierarchy;

	return p_discriminator;
}
static int element_type_hierarchy_top_container_init_discriminator(
	struct element_type_hierarchy_top_container_discriminator *
													p_element_type_hierarchy_top_container_discriminator,
	struct discriminator *							p_discriminator) {
	if (p_discriminator == NULL) {
		msg_stderr("Can't init p_element_type_hierarchy_top_container_discriminator with NULL p_discriminator \n");
		return (-EINVAL);
	}
	p_element_type_hierarchy_top_container_discriminator->p_discriminator = p_discriminator;
	return 0;
}
static void element_type_hierarchy_top_container_clean_discriminator(
		struct element_type_hierarchy_top_container_discriminator *
													p_element_type_hierarchy_top_container_discriminator,
		metac_flag_t									keep_data
		) {
	if (keep_data == 0) {
		discriminator_delete(&p_element_type_hierarchy_top_container_discriminator->p_discriminator);
	}
	p_element_type_hierarchy_top_container_discriminator->p_discriminator = NULL;
}
static struct element_type_hierarchy_top_container_discriminator * element_type_hierarchy_top_container_create_discriminator(
		struct discriminator *						p_discriminator) {
	int res;
	_create_(element_type_hierarchy_top_container_discriminator);
	msg_stddbg("element_type_hierarchy_top_container_create_discriminator\n");
	if (element_type_hierarchy_top_container_init_discriminator(
			p_element_type_hierarchy_top_container_discriminator,
			p_discriminator) != 0) {
		free(p_element_type_hierarchy_top_container_discriminator);
		return NULL;
	}
	msg_stddbg("element_type_hierarchy_top_container_create_discriminator exit\n");
	return p_element_type_hierarchy_top_container_discriminator;
}
static int element_type_hierarchy_top_builder_add_discriminator (
		struct element_type_hierarchy_top_builder * p_element_type_hierarchy_top_builder,
		struct discriminator *						p_discriminator) {
	struct element_type_hierarchy_top_container_discriminator * p_element_type_hierarchy_top_container_discriminator;
	/* create */
	p_element_type_hierarchy_top_container_discriminator = element_type_hierarchy_top_container_create_discriminator(
			p_discriminator);
	if (p_element_type_hierarchy_top_container_discriminator == NULL) {
		msg_stddbg("wasn't able to create p_element_type_hierarchy_top_container_discriminator\n");
		return (-EFAULT);
	}
	/* add to list */
	cds_list_add_tail(
		&p_element_type_hierarchy_top_container_discriminator->list,
		&p_element_type_hierarchy_top_builder->container.discriminator_type_list);
	return 0;
}
#endif
