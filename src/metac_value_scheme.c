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
	struct element_type_hierarchy_member *			p_element_type_hierarchy_member;
	struct element_type_hierarchy *					p_parent_hierarchy;
};
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

static int metac_value_scheme_clean_as_hierarchy_member(
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
	/*TODO: move this to a separate re-usable function */
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

	p_value_scheme_container_hierarchy_member->p_value_scheme = p_value_scheme;
	return 0;
}

static void value_scheme_container_hierarchy_member_clean(
		struct value_scheme_container_hierarchy_member *
													p_value_scheme_container_hierarchy_member) {

	//TODO: value_scheme_put(&p_value_scheme_container_hierarchy_member->p_value_scheme);
	p_value_scheme_container_hierarchy_member->p_value_scheme = NULL;
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

#if 0
static int value_scheme_builder_process_structure(
		struct value_scheme_builder *				p_value_scheme_builder,
		char *										global_path,
		char *										hirarchy_path,
		struct discriminator *						p_discriminator,
		metac_discriminator_value_t					expected_discriminator_value,
		struct metac_type *							p_actual_type,
		struct value_scheme_with_hierarchy *		p_hierarchy) {

	metac_num_t i;
	assert(p_actual_type->id == DW_TAG_structure_type);

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
				indx,
				p_discriminator,
				expected_discriminator_value,
				p_hierarchy,
				p_member_info);
		if (p_hierarchy->members[indx] == NULL) {
			msg_stddbg("global_path: %s failed because metac_value_scheme_create_as_hierarchy_member failed\n", global_path);
			free(member_hirarchy_path);
			free(member_global_path);
			return (-EFAULT);
		}

		res = element_type_hierarchy_top_builder_schedule_element_type_hierarchy_member(
				p_value_scheme_builder,
				member_global_path,
				p_hierarchy->members[indx],
				p_hierarchy);
		if (res != 0) {
			msg_stderr("failed to schedule global_path for %s\n", member_global_path);
			free(member_hirarchy_path);
			free(member_global_path);
			element_type_hierarchy_member_delete(&p_hierarchy->members[indx]);
			return res;
		}

		free(member_hirarchy_path);
		free(member_global_path);
	}
	return 0;
}
static int element_type_hierarchy_top_builder_process_union(
		struct value_scheme_builder *				p_value_scheme_builder,
		char *										global_path,
		char *										hirarchy_path,
		struct discriminator *						p_previous_discriminator,
		metac_discriminator_value_t					previous_expected_discriminator_value,
		struct metac_type *							p_actual_type,
		struct value_scheme_with_hierarchy *		p_hierarchy) {
	metac_num_t i;
	struct discriminator * p_discriminator;
	assert(p_actual_type->id == DW_TAG_union_type);

	p_discriminator = discriminator_create(
			p_element_type_hierarchy_top_builder->p_root_type,
			global_path,
			p_element_type_hierarchy_top_builder->p_override_annotations,
			p_previous_discriminator,
			previous_expected_discriminator_value,
			p_hierarchy);
	if (p_discriminator == NULL){
		msg_stddbg("global_path: %s failed to create discriminator\n", global_path);
		return (-EFAULT);
	}
	if (element_type_hierarchy_top_builder_add_discriminator(p_element_type_hierarchy_top_builder, p_discriminator) != 0) {
		msg_stddbg("global_path: %s failed to add discriminator\n", global_path);
		discriminator_delete(&p_discriminator);
		return (-EFAULT);
	}

	for (i = p_actual_type->union_type_info.members_count; i > 0; i--) {
		int indx = i-1;
		struct metac_type_member_info * p_member_info = &p_actual_type->union_type_info.members[indx];
		int res;
		char * member_global_path;
		char * member_hirarchy_path;

		member_global_path = build_member_path(global_path, p_member_info->name);
		if (member_global_path == NULL) {
			msg_stderr("wasn't able to build global_path for %s and %s\n", global_path, p_member_info->name);
			return (-EFAULT);
		}
		member_hirarchy_path = build_member_path(hirarchy_path, p_member_info->name);
		if (member_hirarchy_path == NULL) {
			msg_stderr("wasn't able to build global_path for %s and %s\n", global_path, p_member_info->name);
			free(member_global_path);
			return (-EFAULT);
		}

		p_hierarchy->members[indx] = element_type_hierarchy_member_create(
				p_element_type_hierarchy_top_builder->p_root_type,
				member_global_path,
				member_hirarchy_path,
				p_element_type_hierarchy_top_builder->p_override_annotations,
				indx,
				p_discriminator,
				indx,
				p_hierarchy,
				p_member_info);
		if (p_hierarchy->members[indx] == NULL) {
			msg_stddbg("global_path: %s failed because element_type_hierarchy_member_create failed\n", global_path);
			free(member_hirarchy_path);
			free(member_global_path);
			return (-EFAULT);
		}

		res = element_type_hierarchy_top_builder_schedule_element_type_hierarchy_member(
				p_element_type_hierarchy_top_builder,
				member_global_path,
				p_hierarchy->members[indx],
				p_hierarchy);
		if (res != 0) {
			msg_stderr("failed to schedule global_path for %s\n", member_global_path);
			element_type_hierarchy_member_delete(&p_hierarchy->members[indx]);
			free(member_hirarchy_path);
			free(member_global_path);
			return res;
		}
		free(member_hirarchy_path);
		free(member_global_path);
	}
	return 0;
}

static int element_type_hierarchy_top_builder_process_hierarchy(
		struct element_type_hierarchy_top_builder * p_element_type_hierarchy_top_builder,
		char *										global_path,
		char *										hirarchy_path,
		struct discriminator *						p_discriminator,
		metac_discriminator_value_t					expected_discriminator_value,
		struct metac_type *							p_actual_type,
		struct element_type_hierarchy *				p_hierarchy,
		struct element_type_hierarchy *				p_parent_hierarchy) {
	int res;
	msg_stddbg("process global_path: %s, hirarchy_path: %s\n", global_path, hirarchy_path);

	switch (p_actual_type->id) {
	case DW_TAG_structure_type:
		res = element_type_hierarchy_top_builder_process_structure(
				p_element_type_hierarchy_top_builder,
				global_path,
				hirarchy_path,
				p_discriminator,
				expected_discriminator_value,
				p_actual_type,
				p_hierarchy);
		break;
	case DW_TAG_union_type:
		res = element_type_hierarchy_top_builder_process_union(
				p_element_type_hierarchy_top_builder,
				global_path,
				hirarchy_path,
				p_discriminator,
				expected_discriminator_value,
				p_actual_type,
				p_hierarchy);
		break;
	default:
		res = (-EINVAL);
		msg_stderr("incorrect type %d element_type_hierarchy_top_builder_process_hierarchy\n",
				p_actual_type->id);
		assert(0);
		break;
	}
	if (res != 0){
		element_type_hierarchy_clean(p_hierarchy);
	}
	return res;
}
static int element_type_hierarchy_top_builder_process_element_type_hierarchy_member(
		struct element_type_hierarchy_top_builder * p_element_type_hierarchy_top_builder,
		char *										global_path,
		struct element_type_hierarchy_member *		p_element_type_hierarchy_member,
		struct element_type_hierarchy *				p_parent_hierarchy) {

	if (p_element_type_hierarchy_member->p_actual_type && (
			p_element_type_hierarchy_member->p_actual_type->id == DW_TAG_structure_type||
			p_element_type_hierarchy_member->p_actual_type->id == DW_TAG_union_type)) {
		if (element_type_hierarchy_top_builder_process_hierarchy(
				p_element_type_hierarchy_top_builder,
				global_path,
				p_element_type_hierarchy_member->path_within_hierarchy,
				p_element_type_hierarchy_member->precondition.p_discriminator,
				p_element_type_hierarchy_member->precondition.expected_discriminator_value,
				p_element_type_hierarchy_member->p_actual_type,
				&p_element_type_hierarchy_member->hierarchy,
				p_parent_hierarchy) != 0) {
			msg_stderr("failed to process global_path for %s\n", global_path);
			return (-EFAULT);
		}
	}
	return 0;
}
static int element_type_hierarchy_top_builder_delete_hierarchy_member_task(
		struct element_type_hierarchy_top_builder_element_type_hierarchy_member_task **
													pp_element_type_hierarchy_top_builder_element_type_hierarchy_member_task) {
	_delete_start_(element_type_hierarchy_top_builder_element_type_hierarchy_member_task);
	if ((*pp_element_type_hierarchy_top_builder_element_type_hierarchy_member_task)->global_path) {
		free((*pp_element_type_hierarchy_top_builder_element_type_hierarchy_member_task)->global_path);
		(*pp_element_type_hierarchy_top_builder_element_type_hierarchy_member_task)->global_path = NULL;
	}
	_delete_finish(element_type_hierarchy_top_builder_element_type_hierarchy_member_task);
	return 0;
}
static int element_type_hierarchy_top_builder_process_element_type_hierarchy_member_fn(
	struct traversing_engine * 						p_engine,
	struct traversing_engine_task * 				p_task,
	int 											error_flag) {
	struct element_type_hierarchy_top_builder * p_element_type_hierarchy_top_builder =
		cds_list_entry(p_engine, struct element_type_hierarchy_top_builder, hierarchy_traversing_engine);
	struct element_type_hierarchy_top_builder_element_type_hierarchy_member_task * p_element_type_hierarchy_top_builder_element_type_hierarchy_member_task =
		cds_list_entry(p_task, struct element_type_hierarchy_top_builder_element_type_hierarchy_member_task, task);
	cds_list_add_tail(&p_task->list, &p_element_type_hierarchy_top_builder->hierarchy_executed_tasks);

	/* add the object anyway even though it's error happened */
	if (element_type_hierarchy_top_builder_add_element_type_hierarchy_member(
			p_element_type_hierarchy_top_builder,
			p_element_type_hierarchy_top_builder_element_type_hierarchy_member_task->p_element_type_hierarchy_member) != 0){
		msg_stddbg("global_path: %s failed to add hierarchy_member\n", p_element_type_hierarchy_top_builder_element_type_hierarchy_member_task->global_path);
		return (-EFAULT);
	}
	if (error_flag != 0)return (-EALREADY);

	return element_type_hierarchy_top_builder_process_element_type_hierarchy_member(
		p_element_type_hierarchy_top_builder,
		p_element_type_hierarchy_top_builder_element_type_hierarchy_member_task->global_path,
		p_element_type_hierarchy_top_builder_element_type_hierarchy_member_task->p_element_type_hierarchy_member,
		p_element_type_hierarchy_top_builder_element_type_hierarchy_member_task->p_parent_hierarchy);
}
static struct element_type_hierarchy_top_builder_element_type_hierarchy_member_task * element_type_hierarchy_top_builder_schedule_element_type_hierarchy_member_with_fn(
		struct element_type_hierarchy_top_builder * p_element_type_hierarchy_top_builder,
		traversing_engine_task_fn_t 				fn,
		char *										global_path,
		struct element_type_hierarchy_member *		p_element_type_hierarchy_member,
		struct element_type_hierarchy *				p_parent_hierarchy) {
	_create_(element_type_hierarchy_top_builder_element_type_hierarchy_member_task);

	p_element_type_hierarchy_top_builder_element_type_hierarchy_member_task->task.fn = fn;
	p_element_type_hierarchy_top_builder_element_type_hierarchy_member_task->global_path = strdup(global_path);
	p_element_type_hierarchy_top_builder_element_type_hierarchy_member_task->p_element_type_hierarchy_member = p_element_type_hierarchy_member;
	p_element_type_hierarchy_top_builder_element_type_hierarchy_member_task->p_parent_hierarchy = p_parent_hierarchy;
	if (p_element_type_hierarchy_top_builder_element_type_hierarchy_member_task->global_path == NULL) {
		msg_stderr("wasn't able to duplicate global_path\n");
		element_type_hierarchy_top_builder_delete_hierarchy_member_task(&p_element_type_hierarchy_top_builder_element_type_hierarchy_member_task);
		return NULL;
	}
	if (add_traversing_task_to_front(
		&p_element_type_hierarchy_top_builder->hierarchy_traversing_engine,
		&p_element_type_hierarchy_top_builder_element_type_hierarchy_member_task->task) != 0) {
		msg_stderr("wasn't able to schedule task\n");
		element_type_hierarchy_top_builder_delete_hierarchy_member_task(&p_element_type_hierarchy_top_builder_element_type_hierarchy_member_task);
		return NULL;
	}
	return p_element_type_hierarchy_top_builder_element_type_hierarchy_member_task;
}
static int element_type_hierarchy_top_builder_schedule_element_type_hierarchy_member(
		struct element_type_hierarchy_top_builder * p_element_type_hierarchy_top_builder,
		char *										global_path,
		struct element_type_hierarchy_member *		p_element_type_hierarchy_member,
		struct element_type_hierarchy *				p_parent_hierarchy) {
	return (element_type_hierarchy_top_builder_schedule_element_type_hierarchy_member_with_fn(
		p_element_type_hierarchy_top_builder,
		element_type_hierarchy_top_builder_process_element_type_hierarchy_member_fn,
		global_path,
		p_element_type_hierarchy_member,
		p_parent_hierarchy)!=NULL)?0:(-EFAULT);

}
static int element_type_hierarchy_top_container_init(
		struct element_type_hierarchy_top_container *
													p_element_type_hierarchy_top_container) {
	CDS_INIT_LIST_HEAD(&p_element_type_hierarchy_top_container->discriminator_type_list);
	CDS_INIT_LIST_HEAD(&p_element_type_hierarchy_top_container->element_type_hierarchy_member_type_list);
	return 0;
}
static void element_type_hierarchy_top_container_clean(
		struct element_type_hierarchy_top_container *
													p_element_type_hierarchy_top_container,
		metac_flag_t									keep_data) {
	struct element_type_hierarchy_top_container_discriminator * _discriminator, * __discriminator;
	struct element_type_hierarchy_top_container_element_type_hierarchy_member * _member, * __member;
	cds_list_for_each_entry_safe(_member, __member, &p_element_type_hierarchy_top_container->element_type_hierarchy_member_type_list, list) {
		cds_list_del(&_member->list);
		element_type_hierarchy_top_container_clean_element_type_hierarchy_member(_member, keep_data);
		free(_member);
	}
	cds_list_for_each_entry_safe(_discriminator, __discriminator, &p_element_type_hierarchy_top_container->discriminator_type_list, list) {
		cds_list_del(&_discriminator->list);
		element_type_hierarchy_top_container_clean_discriminator(_discriminator, keep_data);
		free(_discriminator);
	}
}
static int element_type_hierarchy_top_builder_init(
		struct element_type_hierarchy_top_builder * p_element_type_hierarchy_top_builder,
		struct metac_type *							p_root_type,
		metac_type_annotation_t *					p_override_annotations) {
	p_element_type_hierarchy_top_builder->p_root_type = p_root_type;
	p_element_type_hierarchy_top_builder->p_override_annotations = p_override_annotations;
	element_type_hierarchy_top_container_init(&p_element_type_hierarchy_top_builder->container);
	CDS_INIT_LIST_HEAD(&p_element_type_hierarchy_top_builder->hierarchy_executed_tasks);
	traversing_engine_init(&p_element_type_hierarchy_top_builder->hierarchy_traversing_engine);
	return 0;
}
static void element_type_hierarchy_top_builder_clean(
		struct element_type_hierarchy_top_builder * p_element_type_hierarchy_top_builder,
		metac_flag_t									keep_data) {
	struct traversing_engine_task * p_task, *_p_task;

	traversing_engine_clean(&p_element_type_hierarchy_top_builder->hierarchy_traversing_engine);
	cds_list_for_each_entry_safe(p_task, _p_task, &p_element_type_hierarchy_top_builder->hierarchy_executed_tasks, list) {
		struct element_type_hierarchy_top_builder_element_type_hierarchy_member_task * p_element_type_hierarchy_top_builder_element_type_hierarchy_member_task = cds_list_entry(
			p_task,
			struct element_type_hierarchy_top_builder_element_type_hierarchy_member_task, task);
		cds_list_del(&p_task->list);
		element_type_hierarchy_top_builder_delete_hierarchy_member_task(&p_element_type_hierarchy_top_builder_element_type_hierarchy_member_task);
	}
	element_type_hierarchy_top_container_clean(&p_element_type_hierarchy_top_builder->container, keep_data);
	p_element_type_hierarchy_top_builder->p_root_type = NULL;
}
static int element_type_hierarchy_top_builder_finalize(
		struct element_type_hierarchy_top_builder * p_element_type_hierarchy_top_builder,
		struct element_type_hierarchy_top *			p_element_type_hierarchy_top) {
	metac_count_t i;
	struct element_type_hierarchy_top_container_discriminator * _discriminator, * __discriminator;
	struct element_type_hierarchy_top_container_element_type_hierarchy_member * _member, * __member;

	p_element_type_hierarchy_top->members_count = 0;
	p_element_type_hierarchy_top->pp_members = NULL;
	p_element_type_hierarchy_top->discriminators_count = 0;
	p_element_type_hierarchy_top->pp_discriminators = NULL;

	/*get arrays lengths */
	cds_list_for_each_entry_safe(_member, __member, &p_element_type_hierarchy_top_builder->container.element_type_hierarchy_member_type_list, list) {
		++p_element_type_hierarchy_top->members_count;
	}
	cds_list_for_each_entry_safe(_discriminator, __discriminator, &p_element_type_hierarchy_top_builder->container.discriminator_type_list, list) {
		++p_element_type_hierarchy_top->discriminators_count;
	}

	if (p_element_type_hierarchy_top->members_count > 0) {
		p_element_type_hierarchy_top->pp_members = (struct element_type_hierarchy_member **)calloc(
				p_element_type_hierarchy_top->members_count, sizeof(*p_element_type_hierarchy_top->pp_members));
		if (p_element_type_hierarchy_top->pp_members == NULL) {
			msg_stderr("can't allocate memory for members\n");
			return (-ENOMEM);
		}

		i = 0;
		cds_list_for_each_entry_safe(_member, __member, &p_element_type_hierarchy_top_builder->container.element_type_hierarchy_member_type_list, list) {
			p_element_type_hierarchy_top->pp_members[i] = _member->p_element_type_hierarchy_member;
			p_element_type_hierarchy_top->pp_members[i]->id = i;
			msg_stddbg("added member %s\n", _member->p_element_type_hierarchy_member->path_within_hierarchy);
			++i;
		}
	}
	if (p_element_type_hierarchy_top->discriminators_count > 0) {
		p_element_type_hierarchy_top->pp_discriminators = (struct discriminator **)calloc(
				p_element_type_hierarchy_top->discriminators_count, sizeof(*p_element_type_hierarchy_top->pp_discriminators));
		if (p_element_type_hierarchy_top->pp_discriminators == NULL) {
			msg_stderr("can't allocate memory for discriminators\n");
			free(p_element_type_hierarchy_top->pp_members);
			return (-ENOMEM);
		}

		i = 0;
		cds_list_for_each_entry_safe(_discriminator, __discriminator, &p_element_type_hierarchy_top_builder->container.discriminator_type_list, list) {
			p_element_type_hierarchy_top->pp_discriminators[i] = _discriminator->p_discriminator;
			p_element_type_hierarchy_top->pp_discriminators[i]->id = i;
			++i;
		}
	}
	return 0;
}
void element_type_hierarchy_top_clean(
		struct element_type_hierarchy_top *			p_element_type_hierarchy_top) {
	metac_count_t i;
	if (p_element_type_hierarchy_top->pp_members != NULL) {
		/*to clean members*/
		for (i = 0 ; i < p_element_type_hierarchy_top->members_count; ++i) {
			element_type_hierarchy_member_delete(&p_element_type_hierarchy_top->pp_members[i]);
		}
		free(p_element_type_hierarchy_top->pp_members);
		p_element_type_hierarchy_top->pp_members = NULL;
	}
	if (p_element_type_hierarchy_top->pp_discriminators != NULL) {
		/*to clean discriminators*/
		for (i = 0 ; i < p_element_type_hierarchy_top->discriminators_count; ++i) {
			discriminator_delete(&p_element_type_hierarchy_top->pp_discriminators[i]);
		}
		free(p_element_type_hierarchy_top->pp_discriminators);
		p_element_type_hierarchy_top->pp_discriminators = NULL;
	}
	element_type_hierarchy_clean(&p_element_type_hierarchy_top->hierarchy);
}
int element_type_hierarchy_top_init(
		struct metac_type *							p_root_type,
		char * 										global_path,
		metac_type_annotation_t *					p_override_annotations,
		struct metac_type *							p_actual_type,
		struct element_type_hierarchy_top *			p_element_type_hierarchy_top) {
	struct element_type_hierarchy_top_builder element_type_hierarchy_top_builder;

	element_type_hierarchy_top_builder_init(&element_type_hierarchy_top_builder, p_root_type, p_override_annotations);

	if (element_type_hierarchy_init(
			p_root_type,
			global_path,
			p_override_annotations,
			p_actual_type,
			&p_element_type_hierarchy_top->hierarchy,
			NULL) != 0) {
		msg_stderr("element_type_hierarchy_init failed\n");
		element_type_hierarchy_top_builder_clean(&element_type_hierarchy_top_builder, 0);
		return (-EFAULT);
	}

	if (element_type_hierarchy_top_builder_process_hierarchy(
			&element_type_hierarchy_top_builder,
			global_path,
			"",
			NULL,
			0,
			p_actual_type,
			&p_element_type_hierarchy_top->hierarchy,
			NULL) != 0) {
		msg_stddbg("wasn't able to schedule the first task\n");
		element_type_hierarchy_top_builder_clean(&element_type_hierarchy_top_builder, 0);
		element_type_hierarchy_clean(&p_element_type_hierarchy_top->hierarchy);
		return (-EFAULT);
	}
	if (traversing_engine_run(&element_type_hierarchy_top_builder.hierarchy_traversing_engine) != 0) {
		msg_stddbg("tasks execution finished with fail\n");
		element_type_hierarchy_top_builder_clean(&element_type_hierarchy_top_builder, 0);
		element_type_hierarchy_clean(&p_element_type_hierarchy_top->hierarchy);
		return (-EFAULT);
	}
	/*fill in */
	if (element_type_hierarchy_top_builder_finalize(
			&element_type_hierarchy_top_builder,
			p_element_type_hierarchy_top) != 0) {
		msg_stddbg("object finalization failed\n");
		element_type_hierarchy_top_builder_clean(&element_type_hierarchy_top_builder, 0);
		element_type_hierarchy_clean(&p_element_type_hierarchy_top->hierarchy);
		return (-EFAULT);
	}
	/*clean builder objects*/
	element_type_hierarchy_top_builder_clean(&element_type_hierarchy_top_builder, 1);
	return 0;
}
#endif
