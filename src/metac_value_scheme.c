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
#include "scheduler.h"

#include "metac/metac_value_scheme.h"

#define alloc_sptrinf(...) ({\
	int len = snprintf(NULL, 0, ##__VA_ARGS__); \
	char *str = calloc(len + 1, sizeof(*str)); \
	if (str != NULL)snprintf(str, len + 1, ##__VA_ARGS__); \
	str; \
})

/*****************************************************************************/
struct value_scheme_container_discriminator {
	struct cds_list_head							list;

	struct discriminator *							p_discriminator;
};

struct value_scheme_container_hierarchy_member {
	struct cds_list_head							list;

	struct metac_value_scheme *						p_value_scheme;
};

struct value_scheme_container {
	struct cds_list_head							discriminator_list;
	struct cds_list_head							hierarchy_member_list;
};

struct value_scheme_builder {
	struct metac_type *								p_root_type;
	metac_type_annotation_t *						p_override_annotations;

	struct value_scheme_container					container;

	struct scheduler								scheduler;
	struct cds_list_head							executed_tasks;
};

struct value_scheme_builder_hierarchy_member_task {
	struct scheduler_task							task;

	char * 											global_path;		/*local copy - keep track of it*/

	struct metac_value_scheme *						p_value_scheme;
};
/*****************************************************************************/
static int value_scheme_builder_schedule_hierarchy_member(
		struct value_scheme_builder *				p_value_scheme_builder,
		char *										global_path,
		struct metac_value_scheme *					p_value_scheme);

static struct metac_value_scheme * metac_value_scheme_create_as_hierarchy_member(
		struct metac_type *							p_root_type,
		char *										global_path,
		char *										hierarchy_path,
		metac_type_annotation_t *					p_override_annotations,
		struct value_scheme_with_hierarchy *		p_current_hierarchy,
		struct discriminator *						p_discriminator,
		metac_discriminator_value_t					expected_discriminator_value,
		metac_count_t								member_id,
		struct metac_type_member_info *				p_member_info);

/*****************************************************************************/

struct metac_value_scheme * metac_value_scheme_get(
		struct metac_value_scheme *					p_metac_value_scheme) {

	if (p_metac_value_scheme != NULL &&
		metac_refcounter_object_get(&p_metac_value_scheme->refcounter_object) != NULL) {
		return p_metac_value_scheme;
	}

	return NULL;
}

int metac_value_scheme_put(
		struct metac_value_scheme **				pp_metac_value_scheme) {

	if (pp_metac_value_scheme != NULL &&
		(*pp_metac_value_scheme) != NULL &&
		metac_refcounter_object_put(&(*pp_metac_value_scheme)->refcounter_object) == 0) {

		*pp_metac_value_scheme = NULL;
		return 0;
	}

	return -(EFAULT);
}

/*****************************************************************************/
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

static void discriminator_clean(
		struct discriminator *						p_discriminator) {

	if (p_discriminator->annotation_key != NULL) {

		free(p_discriminator->annotation_key);
		p_discriminator->annotation_key = NULL;
	}
}

static int discriminator_init(
		struct discriminator *						p_discriminator,
		struct metac_type *							p_root_type,
		char * 										global_path,
		metac_type_annotation_t *					p_override_annotations,
		struct discriminator *						p_previous_discriminator,
		metac_discriminator_value_t					previous_expected_discriminator_value) {

	const metac_type_annotation_t * p_annotation;

	p_annotation = metac_type_annotation(p_root_type, global_path, p_override_annotations);
	if (p_annotation == NULL) {

		msg_stderr("Wasn't able to find annotation for type %s, path %s\n", p_root_type->name, global_path);
		return -(EFAULT);
	}

	if (p_annotation->value->discriminator.cb == NULL) {

		msg_stderr("Discriminator callback is NULL in annotation for %s\n", global_path);
		return -(EFAULT);
	}

	if (p_previous_discriminator != NULL) {	/*copy precondition*/

		p_discriminator->precondition.p_discriminator = p_previous_discriminator;
		p_discriminator->precondition.expected_discriminator_value = previous_expected_discriminator_value;
	}

	/*keep global_path as annotation_key*/
	p_discriminator->annotation_key = strdup(global_path);
	if (p_discriminator->annotation_key == NULL) {

		msg_stderr("Can't keep annotation_key for %s\n", global_path);

		discriminator_clean(p_discriminator);
		return -(ENOMEM);
	}

	p_discriminator->cb = p_annotation->value->discriminator.cb;
	p_discriminator->p_data = p_annotation->value->discriminator.data;

	return 0;
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

static struct metac_value_scheme * value_scheme_with_hierarchy_get_value_scheme(
		struct value_scheme_with_hierarchy *		p_value_scheme_with_hierarchy) {
	if (p_value_scheme_with_hierarchy == NULL) {
		return NULL;
	}
	return cds_list_entry(p_value_scheme_with_hierarchy, struct metac_value_scheme, hierarchy);
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

	if (p_actual_type->id == DW_TAG_union_type) {
		struct metac_value_scheme * p_value_scheme = value_scheme_with_hierarchy_get_value_scheme(p_value_scheme_with_hierarchy);

		if (discriminator_init(
				&p_value_scheme_with_hierarchy->union_discriminator,
				p_root_type,
				global_path,
				p_override_annotations,
				(metac_value_scheme_is_hierarchy_member(p_value_scheme) == 1)?p_value_scheme->hierarchy_member.precondition.p_discriminator:NULL,
				(metac_value_scheme_is_hierarchy_member(p_value_scheme) == 1)?p_value_scheme->hierarchy_member.precondition.expected_discriminator_value:0) != 0) {

			msg_stddbg("global_path: %s failed to init discriminator\n", global_path);
			return (-EFAULT);
		}

	}

	return 0;
}

static void value_scheme_with_hierarchy_clean(
		struct value_scheme_with_hierarchy *		p_value_scheme_with_hierarchy) {
	struct metac_value_scheme * p_value_scheme = value_scheme_with_hierarchy_get_value_scheme(p_value_scheme_with_hierarchy);

	if (p_value_scheme_with_hierarchy == NULL ||
		p_value_scheme == NULL) {

		msg_stderr("invalid argument\n");
		return;
	}

	if (p_value_scheme->p_actual_type->id == DW_TAG_union_type) {

		discriminator_clean(&p_value_scheme_with_hierarchy->union_discriminator);
	}

	if (p_value_scheme_with_hierarchy->members != NULL) {

		free(p_value_scheme_with_hierarchy->members);
		p_value_scheme_with_hierarchy->members = NULL;
	}
}

static int metac_value_scheme_clean_as_hierarchy_member(
		struct metac_value_scheme *					p_metac_value_scheme) {

	if (metac_value_scheme_is_hierarchy_member(p_metac_value_scheme) != 1) {
		msg_stderr("invalid argument: expected hierarchy_member\n");
		return -(EINVAL);
	}

	if (p_metac_value_scheme->hierarchy_member.path_within_hierarchy) {
		free(p_metac_value_scheme->hierarchy_member.path_within_hierarchy);
		p_metac_value_scheme->hierarchy_member.path_within_hierarchy = NULL;
	}

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
	return 0;
}

static int metac_value_scheme_init_as_hierarchy_member(
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

	if (metac_value_scheme_is_hierarchy_member(
			p_metac_value_scheme) != 1) {

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

static void value_scheme_container_discriminator_clean(
		struct value_scheme_container_discriminator *	p_value_scheme_container_discriminator) {

	p_value_scheme_container_discriminator->p_discriminator = NULL;
}

static int value_scheme_container_discriminator_init(
	struct value_scheme_container_discriminator *	p_value_scheme_container_discriminator,
	struct discriminator *							p_discriminator) {

	if (p_discriminator == NULL) {

		msg_stderr("Can't init p_element_type_hierarchy_top_container_discriminator with NULL p_discriminator \n");
		return (-EINVAL);
	}

	p_value_scheme_container_discriminator->p_discriminator = p_discriminator;

	return 0;
}

static int value_scheme_container_discriminator_delete(
		struct value_scheme_container_discriminator **	pp_value_scheme_container_discriminator) {

	_delete_start_(value_scheme_container_discriminator);
	value_scheme_container_discriminator_clean(*pp_value_scheme_container_discriminator);
	_delete_finish(value_scheme_container_discriminator);
	return 0;
}

static struct value_scheme_container_discriminator * value_scheme_container_discriminator_create(
		struct discriminator *						p_discriminator) {

	int res;

	_create_(value_scheme_container_discriminator);

	if (value_scheme_container_discriminator_init(
			p_value_scheme_container_discriminator,
			p_discriminator) != 0) {

		value_scheme_container_discriminator_delete(&p_value_scheme_container_discriminator);
		return NULL;
	}

	return p_value_scheme_container_discriminator;
}

static int value_scheme_container_hierarchy_member_init(
		struct value_scheme_container_hierarchy_member *
													p_value_scheme_container_hierarchy_member,
		struct metac_value_scheme *					p_value_scheme) {

	if (p_value_scheme_container_hierarchy_member == NULL) {

		msg_stderr("Invalid argument p_value_scheme_container_hierarchy_member\n");
		return (-EINVAL);
	}

	p_value_scheme_container_hierarchy_member->p_value_scheme = metac_value_scheme_get(p_value_scheme);
	if (p_value_scheme_container_hierarchy_member->p_value_scheme == NULL) {

		msg_stderr("metac_value_scheme_get failed\n");
		return (-EINVAL);
	}

	return 0;
}

static void value_scheme_container_hierarchy_member_clean(
		struct value_scheme_container_hierarchy_member *
													p_value_scheme_container_hierarchy_member) {

	if (p_value_scheme_container_hierarchy_member == NULL) {

		return;
	}

	if (p_value_scheme_container_hierarchy_member->p_value_scheme != NULL) {

		metac_value_scheme_put(&p_value_scheme_container_hierarchy_member->p_value_scheme);
	}
}

static int value_scheme_container_hierarchy_member_delete(
		struct value_scheme_container_hierarchy_member **
													pp_value_scheme_container_hierarchy_member) {

	_delete_start_(value_scheme_container_hierarchy_member);
	value_scheme_container_hierarchy_member_clean(*pp_value_scheme_container_hierarchy_member);
	_delete_finish(value_scheme_container_hierarchy_member);
	return 0;
}

static struct value_scheme_container_hierarchy_member * value_scheme_container_hierarchy_member_create(
		struct metac_value_scheme *						p_value_scheme) {

	int res;

	_create_(value_scheme_container_hierarchy_member);

	if (value_scheme_container_hierarchy_member_init(
			p_value_scheme_container_hierarchy_member,
			p_value_scheme) != 0) {

		value_scheme_container_hierarchy_member_delete(&p_value_scheme_container_hierarchy_member);
		return NULL;
	}

	return p_value_scheme_container_hierarchy_member;
}

static int value_scheme_builder_add_discriminator (
		struct value_scheme_builder *				p_value_scheme_builder,
		struct discriminator *						p_discriminator) {

	struct value_scheme_container_discriminator * p_value_scheme_container_discriminator;

	p_value_scheme_container_discriminator = value_scheme_container_discriminator_create(p_discriminator);
	if (p_value_scheme_container_discriminator == NULL) {

		msg_stddbg("wasn't able to create p_value_scheme_container_discriminator\n");
		return (-EFAULT);
	}

	cds_list_add_tail(
		&p_value_scheme_container_discriminator->list,
		&p_value_scheme_builder->container.discriminator_list);
	return 0;
}

static int value_scheme_builder_add_hierarchy_member (
		struct value_scheme_builder *				p_value_scheme_builder,
		struct metac_value_scheme *					p_value_scheme) {

	struct value_scheme_container_hierarchy_member * p_value_scheme_container_hierarchy_member;

	p_value_scheme_container_hierarchy_member = value_scheme_container_hierarchy_member_create(
			p_value_scheme);
	if (p_value_scheme_container_hierarchy_member == NULL) {

		msg_stddbg("wasn't able to create p_value_scheme_container_hierarchy_member\n");
		return (-EFAULT);
	}

	cds_list_add_tail(
		&p_value_scheme_container_hierarchy_member->list,
		&p_value_scheme_builder->container.hierarchy_member_list);
	return 0;
}

static int value_scheme_builder_process_structure(
		struct value_scheme_builder *				p_value_scheme_builder,
		char *										global_path,
		char *										hirarchy_path,
		struct metac_type *							p_actual_type,
		struct value_scheme_with_hierarchy *		p_hierarchy) {

	metac_num_t i;
	struct discriminator * p_discriminator = NULL;
	metac_discriminator_value_t expected_discriminator_value = 0;
	struct metac_value_scheme * p_metac_value_scheme = NULL;

	assert(p_actual_type->id == DW_TAG_structure_type);

	p_metac_value_scheme = value_scheme_with_hierarchy_get_value_scheme(p_hierarchy);
	if (p_metac_value_scheme == NULL) {

		msg_stderr("value_scheme_with_hierarchy_get_value_scheme failed %s\n", global_path);
		return (-EFAULT);
	}

	if (metac_value_scheme_is_hierarchy_member(p_metac_value_scheme) == 1) {

		p_discriminator = p_metac_value_scheme->hierarchy_member.precondition.p_discriminator;
		expected_discriminator_value = p_metac_value_scheme->hierarchy_member.precondition.expected_discriminator_value;
	}

	for (i = p_actual_type->structure_type_info.members_count; i > 0; i--) {
		int indx = i-1;
		struct metac_type_member_info * p_member_info = &p_actual_type->structure_type_info.members[indx];
		int res;
		char * member_global_path;
		char * member_hirarchy_path;

		member_global_path = alloc_sptrinf("%s.%s", global_path, p_member_info->name);
		if (member_global_path == NULL) {

			msg_stderr("wasn't able to build global_path for %s and %s\n", global_path, p_member_info->name);
			return (-EFAULT);
		}

		member_hirarchy_path = alloc_sptrinf("%s.%s", hirarchy_path, p_member_info->name);
		if (member_hirarchy_path == NULL) {

			msg_stderr("wasn't able to build global_path for %s and %s\n", global_path, p_member_info->name);
			free(member_global_path);
			return (-EFAULT);
		}

		p_hierarchy->members[indx] = metac_value_scheme_create_as_hierarchy_member(
				p_value_scheme_builder->p_root_type,
				member_global_path,
				member_hirarchy_path,
				p_value_scheme_builder->p_override_annotations,
				p_hierarchy,
				p_discriminator,
				expected_discriminator_value,
				indx,
				p_member_info);
		if (p_hierarchy->members[indx] == NULL) {
			msg_stddbg("global_path: %s failed because metac_value_scheme_create_as_hierarchy_member failed\n", global_path);
			free(member_hirarchy_path);
			free(member_global_path);
			return (-EFAULT);
		}

		res = value_scheme_builder_schedule_hierarchy_member(
				p_value_scheme_builder,
				member_global_path,
				p_hierarchy->members[indx]);
		if (res != 0) {

			msg_stderr("failed to schedule global_path for %s\n", member_global_path);
			metac_value_scheme_put(&p_hierarchy->members[indx]);

			free(member_hirarchy_path);
			free(member_global_path);
			return res;
		}

		free(member_hirarchy_path);
		free(member_global_path);
	}

	for (i = 0; i > p_actual_type->structure_type_info.members_count; i--) {

		struct metac_type_member_info * p_member_info = &p_actual_type->structure_type_info.members[i];

		if (value_scheme_builder_add_hierarchy_member(
				p_value_scheme_builder,
				p_hierarchy->members[i]) != 0) {

			msg_stderr("value_scheme_builder_add_hierarchy_member failed for for %s and %s\n", global_path, p_member_info->name);

			metac_value_scheme_put(&p_hierarchy->members[i]);
			return (-EFAULT);
		}
	}

	return 0;
}

static int value_scheme_builder_process_union(
		struct value_scheme_builder *				p_value_scheme_builder,
		char *										global_path,
		char *										hirarchy_path,
		struct metac_type *							p_actual_type,
		struct value_scheme_with_hierarchy *		p_hierarchy) {
	metac_num_t i;
	struct discriminator * p_discriminator;
	assert(p_actual_type->id == DW_TAG_union_type);

	p_discriminator = &p_hierarchy->union_discriminator;
	if (value_scheme_builder_add_discriminator(p_value_scheme_builder, p_discriminator) != 0) {

		msg_stddbg("global_path: %s failed to add discriminator\n", global_path);
		return (-EFAULT);
	}

	for (i = p_actual_type->union_type_info.members_count; i > 0; i--) {
		int indx = i-1;
		struct metac_type_member_info * p_member_info = &p_actual_type->union_type_info.members[indx];
		int res;
		char * member_global_path;
		char * member_hirarchy_path;

		member_global_path = alloc_sptrinf("%s.%s", global_path, p_member_info->name);
		if (member_global_path == NULL) {

			msg_stderr("wasn't able to build global_path for %s and %s\n", global_path, p_member_info->name);
			return (-EFAULT);
		}

		member_hirarchy_path = alloc_sptrinf("%s.%s", hirarchy_path, p_member_info->name);
		if (member_hirarchy_path == NULL) {

			msg_stderr("wasn't able to build global_path for %s and %s\n", global_path, p_member_info->name);
			free(member_global_path);
			return (-EFAULT);
		}

		p_hierarchy->members[indx] = metac_value_scheme_create_as_hierarchy_member(
				p_value_scheme_builder->p_root_type,
				member_global_path,
				member_hirarchy_path,
				p_value_scheme_builder->p_override_annotations,
				p_hierarchy,
				p_discriminator,
				indx,
				indx,
				p_member_info);
		if (p_hierarchy->members[indx] == NULL) {

			msg_stddbg("global_path: %s failed because element_type_hierarchy_member_create failed\n", global_path);
			free(member_hirarchy_path);
			free(member_global_path);
			return (-EFAULT);
		}

		if (value_scheme_builder_add_hierarchy_member(
				p_value_scheme_builder,
				p_hierarchy->members[indx]) != 0) {

			msg_stderr("value_scheme_builder_add_hierarchy_member failed for for %s and %s\n", global_path, p_member_info->name);

			metac_value_scheme_put(&p_hierarchy->members[indx]);
			free(member_hirarchy_path);
			free(member_global_path);
			return (-EFAULT);
		}

		res = value_scheme_builder_schedule_hierarchy_member(
				p_value_scheme_builder,
				member_global_path,
				p_hierarchy->members[indx]);
		if (res != 0) {

			msg_stderr("failed to schedule global_path for %s\n", member_global_path);
			metac_value_scheme_put(&p_hierarchy->members[indx]);
			free(member_hirarchy_path);
			free(member_global_path);
			return res;
		}

		free(member_hirarchy_path);
		free(member_global_path);
	}

	for (i = 0; i > p_actual_type->union_type_info.members_count; i--) {

		struct metac_type_member_info * p_member_info = &p_actual_type->union_type_info.members[i];

		if (value_scheme_builder_add_hierarchy_member(
				p_value_scheme_builder,
				p_hierarchy->members[i]) != 0) {

			msg_stderr("value_scheme_builder_add_hierarchy_member failed for for %s and %s\n", global_path, p_member_info->name);

			metac_value_scheme_put(&p_hierarchy->members[i]);
			return (-EFAULT);
		}
	}

	return 0;
}

static int value_scheme_builder_process_hierarchy(
		struct value_scheme_builder *				p_value_scheme_builder,
		char *										global_path,
		char *										hirarchy_path,
		struct metac_type *							p_actual_type,
		struct value_scheme_with_hierarchy *		p_hierarchy) {
	int res;
	msg_stddbg("process global_path: %s, hirarchy_path: %s\n", global_path, hirarchy_path);

	switch (p_actual_type->id) {
	case DW_TAG_structure_type:
		res = value_scheme_builder_process_structure(
				p_value_scheme_builder,
				global_path,
				hirarchy_path,
				p_actual_type,
				p_hierarchy);
		break;
	case DW_TAG_union_type:
		res = value_scheme_builder_process_union(
				p_value_scheme_builder,
				global_path,
				hirarchy_path,
				p_actual_type,
				p_hierarchy);
		break;
	default:
		res = (-EINVAL);
		msg_stderr("incorrect type %d\n",
				p_actual_type->id);
		assert(0);
		break;
	}

	return res;
}

static int value_scheme_builder_process_hierarchy_member(
		struct value_scheme_builder *				p_value_scheme_builder,
		char *										global_path,
		struct metac_value_scheme *					p_value_scheme) {

	if (	metac_value_scheme_is_hierarchy_member(p_value_scheme) == 1 &&
			p_value_scheme->p_actual_type && (
			p_value_scheme->p_actual_type->id == DW_TAG_structure_type||
			p_value_scheme->p_actual_type->id == DW_TAG_union_type)) {

		if (value_scheme_builder_process_hierarchy(
				p_value_scheme_builder,
				global_path,
				p_value_scheme->hierarchy_member.path_within_hierarchy,
				p_value_scheme->p_actual_type,
				&p_value_scheme->hierarchy) != 0) {

			msg_stderr("failed to process global_path for %s\n", global_path);
			return (-EFAULT);
		}
	}
	return 0;
}

static int value_scheme_builder_process_hierarchy_member_fn(
	struct scheduler * 								p_engine,
	struct scheduler_task * 						p_task,
	int 											error_flag) {

	struct value_scheme_builder * p_value_scheme_builder =
		cds_list_entry(p_engine, struct value_scheme_builder, scheduler);

	struct value_scheme_builder_hierarchy_member_task * p_value_scheme_builder_hierarchy_member_task =
		cds_list_entry(p_task, struct value_scheme_builder_hierarchy_member_task, task);

	/*TODO: consider deletion of the task after execution or adding them to the task pool */
	cds_list_add_tail(&p_task->list, &p_value_scheme_builder->executed_tasks);

	if (error_flag != 0) {

		return (-EALREADY);
	}

	return value_scheme_builder_process_hierarchy_member(
			p_value_scheme_builder,
			p_value_scheme_builder_hierarchy_member_task->global_path,
			p_value_scheme_builder_hierarchy_member_task->p_value_scheme);
}

static int value_scheme_builder_hierarchy_member_task_delete(
		struct value_scheme_builder_hierarchy_member_task **
													pp_value_scheme_builder_hierarchy_member_task) {
	_delete_start_(value_scheme_builder_hierarchy_member_task);

	if ((*pp_value_scheme_builder_hierarchy_member_task)->global_path) {

		free((*pp_value_scheme_builder_hierarchy_member_task)->global_path);
		(*pp_value_scheme_builder_hierarchy_member_task)->global_path = NULL;
	}

	if ((*pp_value_scheme_builder_hierarchy_member_task)->p_value_scheme == NULL) {

		metac_value_scheme_put(&(*pp_value_scheme_builder_hierarchy_member_task)->p_value_scheme);
	}

	_delete_finish(value_scheme_builder_hierarchy_member_task);
	return 0;
}

static struct value_scheme_builder_hierarchy_member_task * value_scheme_builder_hierarchy_member_task_create(
		scheduler_task_fn_t 						fn,
		char *										global_path,
		struct metac_value_scheme *					p_value_scheme) {
	_create_(value_scheme_builder_hierarchy_member_task);

	p_value_scheme_builder_hierarchy_member_task->task.fn = fn;
	p_value_scheme_builder_hierarchy_member_task->global_path = strdup(global_path);
	p_value_scheme_builder_hierarchy_member_task->p_value_scheme = metac_value_scheme_get(p_value_scheme);

	if (p_value_scheme_builder_hierarchy_member_task->global_path == NULL) {

		msg_stderr("wasn't able to duplicate global_path\n");
		value_scheme_builder_hierarchy_member_task_delete(&p_value_scheme_builder_hierarchy_member_task);
		return NULL;
	}
	return p_value_scheme_builder_hierarchy_member_task;
}

static int value_scheme_builder_schedule_hierarchy_member(
		struct value_scheme_builder *				p_value_scheme_builder,
		char *										global_path,
		struct metac_value_scheme *					p_value_scheme) {

	struct value_scheme_builder_hierarchy_member_task * p_task = value_scheme_builder_hierarchy_member_task_create(
		value_scheme_builder_process_hierarchy_member_fn,
		global_path,
		p_value_scheme);

	if (p_task == NULL) {
		msg_stderr("wasn't able to create a task\n");
		return -(EFAULT);
	}

	if (scheduler_add_task_to_front(
		&p_value_scheme_builder->scheduler,
		&p_task->task) != 0) {

		msg_stderr("wasn't able to schedule task\n");
		value_scheme_builder_hierarchy_member_task_delete(&p_task);
		return -(EFAULT);
	}

	return 0;
}

static void value_scheme_container_clean(
		struct value_scheme_container *				p_value_scheme_container) {

	struct value_scheme_container_discriminator * p_discriminator, * _p_discriminator;
	struct value_scheme_container_hierarchy_member * p_member, * _p_member;

	cds_list_for_each_entry_safe(p_discriminator, _p_discriminator, &p_value_scheme_container->discriminator_list, list) {
		cds_list_del(&p_discriminator->list);
		value_scheme_container_discriminator_delete(&p_discriminator);
	}

	cds_list_for_each_entry_safe(p_member, _p_member, &p_value_scheme_container->hierarchy_member_list, list) {
		cds_list_del(&p_member->list);
		value_scheme_container_hierarchy_member_delete(&p_member);
	}
}

static int value_scheme_container_init(
		struct value_scheme_container *				p_value_scheme_container) {

	CDS_INIT_LIST_HEAD(&p_value_scheme_container->discriminator_list);
	CDS_INIT_LIST_HEAD(&p_value_scheme_container->hierarchy_member_list);
	return 0;
}

static int value_scheme_builder_init(
		struct value_scheme_builder * 				p_value_scheme_builder,
		struct metac_type *							p_root_type,
		metac_type_annotation_t *					p_override_annotations) {

	p_value_scheme_builder->p_root_type = p_root_type;
	p_value_scheme_builder->p_override_annotations = p_override_annotations;
	value_scheme_container_init(&p_value_scheme_builder->container);

	CDS_INIT_LIST_HEAD(&p_value_scheme_builder->executed_tasks);

	scheduler_init(&p_value_scheme_builder->scheduler);

	return 0;
}

static void value_scheme_builder_clean(
		struct value_scheme_builder * 				p_value_scheme_builder) {

	struct value_scheme_builder_hierarchy_member_task * p_hierarchy_member_task, *_p_hierarchy_member_task;

	scheduler_clean(&p_value_scheme_builder->scheduler);

	cds_list_for_each_entry_safe(p_hierarchy_member_task, _p_hierarchy_member_task, &p_value_scheme_builder->executed_tasks, task.list) {

		cds_list_del(&p_hierarchy_member_task->task.list);
		value_scheme_builder_hierarchy_member_task_delete(&p_hierarchy_member_task);
	}

	value_scheme_container_clean(&p_value_scheme_builder->container);

	p_value_scheme_builder->p_root_type = NULL;
	p_value_scheme_builder->p_override_annotations = NULL;
}

static int metac_value_scheme_finalize_as_hierarchy_top(
		struct value_scheme_builder * 				p_value_scheme_builder,
		struct metac_value_scheme *					p_value_scheme) {
	metac_count_t i;

	struct value_scheme_container_discriminator * p_discriminator, * _p_discriminator;
	struct value_scheme_container_hierarchy_member * p_member, * _p_member;

	p_value_scheme->p_current_hierarchy = NULL;

	p_value_scheme->hierarchy_top.discriminators_count = 0;
	p_value_scheme->hierarchy_top.pp_discriminators = NULL;
	p_value_scheme->hierarchy_top.members_count = 0;
	p_value_scheme->hierarchy_top.pp_members = NULL;

	/*get arrays lengths */
	cds_list_for_each_entry(p_discriminator, &p_value_scheme_builder->container.discriminator_list, list) {
		++p_value_scheme->hierarchy_top.discriminators_count;
	}
	cds_list_for_each_entry(p_member, &p_value_scheme_builder->container.hierarchy_member_list, list) {
		++p_value_scheme->hierarchy_top.members_count;
	}

	if (p_value_scheme->hierarchy_top.discriminators_count > 0) {

		p_value_scheme->hierarchy_top.pp_discriminators = (struct discriminator **)calloc(
				p_value_scheme->hierarchy_top.discriminators_count, sizeof(*p_value_scheme->hierarchy_top.pp_discriminators));
		if (p_value_scheme->hierarchy_top.pp_discriminators == NULL) {

			msg_stderr("can't allocate memory for discriminators\n");
			return (-ENOMEM);
		}

		i = 0;
		cds_list_for_each_entry(p_discriminator, &p_value_scheme_builder->container.discriminator_list, list) {

			p_value_scheme->hierarchy_top.pp_discriminators[i] = p_discriminator->p_discriminator;
			p_value_scheme->hierarchy_top.pp_discriminators[i]->id = i;

			++i;
		}
	}

	if (p_value_scheme->hierarchy_top.members_count > 0) {

		p_value_scheme->hierarchy_top.pp_members = (struct metac_value_scheme **)calloc(
				p_value_scheme->hierarchy_top.members_count, sizeof(*p_value_scheme->hierarchy_top.pp_members));

		if (p_value_scheme->hierarchy_top.pp_members == NULL) {

			msg_stderr("can't allocate memory for members\n");
			free(p_value_scheme->hierarchy_top.pp_discriminators);
			return (-ENOMEM);
		}

		i = 0;
		cds_list_for_each_entry(p_member, &p_value_scheme_builder->container.hierarchy_member_list, list) {

			assert(metac_value_scheme_is_hierarchy_member(p_member->p_value_scheme) == 1);

			p_value_scheme->hierarchy_top.pp_members[i] = metac_value_scheme_get(p_member->p_value_scheme);
			if (p_value_scheme->hierarchy_top.pp_members[i] == NULL) {

				msg_stderr("metac_value_scheme_get failed\n");
				return (-ENOMEM);
			}

			p_value_scheme->hierarchy_top.pp_members[i]->hierarchy_member.id = i;

			msg_stddbg("added member %s\n", p_value_scheme->hierarchy_top.pp_members[i]->hierarchy_member.path_within_hierarchy);
			++i;
		}
	}

	return 0;
}

metac_flag_t metac_value_scheme_is_hierarchy_top(
		struct metac_value_scheme *					p_metac_value_scheme) {

	if (p_metac_value_scheme == NULL) {

		msg_stderr("invalid argument\n");
		return -(EINVAL);
	}

	return (p_metac_value_scheme->p_current_hierarchy == NULL &&
			metac_value_scheme_is_hierachy(p_metac_value_scheme) == 1)?1:0;
}

static int metac_value_scheme_clean_as_hierarchy_top(
		struct metac_value_scheme *					p_value_scheme) {
	metac_count_t i;

	if (metac_value_scheme_is_hierarchy_top(p_value_scheme) != 1) {
		msg_stderr("invalid argument: expected hierarchy_top\n");
		return -(EINVAL);
	}

	if (p_value_scheme->hierarchy_top.pp_discriminators != NULL) {

		for (i = 0 ; i < p_value_scheme->hierarchy_top.discriminators_count; ++i) {

			p_value_scheme->hierarchy_top.pp_discriminators[i] = NULL;
		}

		free(p_value_scheme->hierarchy_top.pp_discriminators);
		p_value_scheme->hierarchy_top.pp_discriminators = NULL;
	}

	if (p_value_scheme->hierarchy_top.pp_members != NULL) {

		for (i = 0 ; i < p_value_scheme->hierarchy_top.members_count; ++i) {

			metac_value_scheme_put(&p_value_scheme->hierarchy_top.pp_members[i]);
		}

		free(p_value_scheme->hierarchy_top.pp_members);
		p_value_scheme->hierarchy_top.pp_members = NULL;
	}

	value_scheme_with_hierarchy_clean(&p_value_scheme->hierarchy);

	return 0;
}

static int metac_value_scheme_init_as_hierarchy_top(
		struct metac_value_scheme *					p_metac_value_scheme,
		struct metac_type *							p_root_type,
		char * 										global_path,
		metac_type_annotation_t *					p_override_annotations,
		struct metac_type *							p_actual_type) {
	struct value_scheme_builder value_scheme_builder;

	if (p_actual_type->id != DW_TAG_structure_type ||
		p_actual_type->id != DW_TAG_union_type) {

		msg_stderr("Invalid argument p_actual_type\n");
		return -(EINVAL);
	}

	value_scheme_builder_init(&value_scheme_builder, p_root_type, p_override_annotations);

	p_metac_value_scheme->p_current_hierarchy = NULL;

	if (value_scheme_with_hierarchy_init(
			&p_metac_value_scheme->hierarchy,
			p_root_type,
			global_path,
			p_override_annotations,
			p_actual_type) != 0) {
		msg_stderr("value_scheme_with_hierarchy_init failed\n");

		value_scheme_builder_clean(&value_scheme_builder);
		return (-EFAULT);
	}

	if (value_scheme_builder_process_hierarchy(
			&value_scheme_builder,
			global_path,
			"",
			p_actual_type,
			&p_metac_value_scheme->hierarchy) != 0) {

		msg_stddbg("wasn't able to schedule the first task\n");

		value_scheme_with_hierarchy_clean(&p_metac_value_scheme->hierarchy);
		value_scheme_builder_clean(&value_scheme_builder);
		return (-EFAULT);
	}

	if (scheduler_run(&value_scheme_builder.scheduler) != 0) {

		msg_stddbg("tasks execution finished with fail\n");

		value_scheme_with_hierarchy_clean(&p_metac_value_scheme->hierarchy);
		value_scheme_builder_clean(&value_scheme_builder);
		return (-EFAULT);
	}

	if (metac_value_scheme_finalize_as_hierarchy_top(
			&value_scheme_builder,
			p_metac_value_scheme) != 0) {

		msg_stddbg("value_scheme finalization failed\n");

		metac_value_scheme_clean_as_hierarchy_top(p_metac_value_scheme);
		value_scheme_builder_clean(&value_scheme_builder);
		return (-EFAULT);
	}

	value_scheme_builder_clean(&value_scheme_builder);

	return 0;
}

static int metac_value_scheme_clean(
		struct metac_value_scheme *					p_value_scheme) {

	if (p_value_scheme == NULL) {

		msg_stderr("Invalid argument\n");
		return -EINVAL;
	}

	if (metac_value_scheme_is_hierarchy_top(p_value_scheme) == 1) {
		return metac_value_scheme_clean_as_hierarchy_top(p_value_scheme);
	}
	return metac_value_scheme_clean_as_hierarchy_member(p_value_scheme);
}

static inline struct metac_value_scheme * metac_unknown_object_get_metac_value_scheme(
		struct metac_unknown_object *				p_metac_unknown_object) {
	return cds_list_entry(
			p_metac_unknown_object,
			struct metac_value_scheme,
			refcounter_object.unknown_object);
}

static int metac_value_scheme_delete(
		struct metac_value_scheme **				pp_metac_value_scheme) {
	_delete_start_(metac_value_scheme);
	metac_value_scheme_clean(*pp_metac_value_scheme);
	_delete_finish(metac_value_scheme);
	return 0;
}

static int _metac_value_scheme_unknown_object_delete(
		struct metac_unknown_object *				p_metac_unknown_object) {

	struct metac_value_scheme * p_metac_value_scheme;

	if (p_metac_unknown_object == NULL) {
		msg_stderr("p_metac_unknown_object is NULL\n");
		return -(EINVAL);
	}

	p_metac_value_scheme = metac_unknown_object_get_metac_value_scheme(p_metac_unknown_object);
	if (p_metac_value_scheme == NULL) {
		msg_stderr("p_metac_value_scheme is NULL\n");
		return -(EINVAL);
	}

	return metac_value_scheme_delete(&p_metac_value_scheme);
}

static metac_refcounter_object_ops_t _metac_value_scheme_refcounter_object_ops = {
		.unknown_object_ops = {
				.metac_unknown_object_delete = 		_metac_value_scheme_unknown_object_delete,
		},
};

static struct metac_value_scheme * metac_value_scheme_create_as_hierarchy_member(
		struct metac_type *							p_root_type,
		char *										global_path,
		char *										hierarchy_path,
		metac_type_annotation_t *					p_override_annotations,
		struct value_scheme_with_hierarchy *		p_current_hierarchy,
		struct discriminator *						p_discriminator,
		metac_discriminator_value_t					expected_discriminator_value,
		metac_count_t								member_id,
		struct metac_type_member_info *				p_member_info) {

	_create_(metac_value_scheme);

	if (metac_refcounter_object_init(
			&p_metac_value_scheme->refcounter_object,
			&_metac_value_scheme_refcounter_object_ops,
			NULL) != 0) {

		msg_stderr("metac_refcounter_object_init failed\n");

		metac_value_scheme_delete(&p_metac_value_scheme);
		return NULL;
	}

	if (metac_value_scheme_init_as_hierarchy_member(
			p_metac_value_scheme,
			p_root_type,
			global_path,
			hierarchy_path,
			p_override_annotations,
			p_current_hierarchy,
			p_discriminator,
			expected_discriminator_value,
			member_id,
			p_member_info) != 0) {

		msg_stderr("metac_value_scheme_init_as_hierarchy_member failed\n");

		metac_value_scheme_delete(&p_metac_value_scheme);
		return NULL;
	}

	return p_metac_value_scheme;
}

struct metac_value_scheme * _metac_value_scheme_create_as_hierarchy_top(
		struct metac_type *							p_root_type,
		char * 										global_path,
		metac_type_annotation_t *					p_override_annotations,
		struct metac_type *							p_actual_type) {

	_create_(metac_value_scheme);

	if (metac_refcounter_object_init(
			&p_metac_value_scheme->refcounter_object,
			&_metac_value_scheme_refcounter_object_ops,
			NULL) != 0) {

		msg_stderr("metac_refcounter_object_init failed\n");

		metac_value_scheme_delete(&p_metac_value_scheme);
		return NULL;
	}

	if (metac_value_scheme_init_as_hierarchy_top(
			p_metac_value_scheme,
			p_root_type,
			global_path,
			p_override_annotations,
			p_actual_type) != 0) {

		msg_stderr("metac_value_scheme_init_as_hierarchy_top failed\n");

		metac_value_scheme_delete(&p_metac_value_scheme);
		return NULL;
	}

	return p_metac_value_scheme;
}



