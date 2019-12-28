/*
 * metac_internals.c
 *
 *  Created on: Jan 23, 2019
 *      Author: mralex
 */

//#define METAC_DEBUG_ENABLE

#include <assert.h>			/* assert */
#include <errno.h>			/* ENOMEM etc */
#include <stdlib.h>			/* calloc, free, NULL... */
#include <string.h>			/* strdup */
#include <urcu/list.h>		/* struct cds_list_head etc */

#include "metac_internals.h"
#include "traversing_engine.h"
#include "metac_debug.h"	/* msg_stderr, ...*/

/*****************************************************************************/
#define _create_(_type_) \
	struct _type_ * p_##_type_; \
	p_##_type_ = calloc(1, sizeof(*(p_##_type_))); \
	if (p_##_type_ == NULL) { \
		msg_stderr("Can't create " #_type_ ": no memory\n"); \
		return NULL; \
	}
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
struct element_type_hierarchy_top_container_discriminator {
	struct cds_list_head							list;

	struct discriminator *							p_discriminator;
};
struct element_type_hierarchy_top_container_element_type_hierarchy_member {
	struct cds_list_head							list;

	struct element_type_hierarchy_member *			p_element_type_hierarchy_member;
};
struct element_type_hierarchy_top_container {
	struct cds_list_head							discriminator_type_list;
	struct cds_list_head							element_type_hierarchy_member_type_list;
};
struct element_type_hierarchy_top_builder {
	struct metac_type *								p_root_type;
	metac_type_annotation_t *						p_override_annotations;

	struct element_type_hierarchy_top_container		container;
	
	struct traversing_engine						hierarchy_traversing_engine;
	struct cds_list_head							hierarchy_executed_tasks;
};
struct element_type_hierarchy_top_builder_element_type_hierarchy_member_task {
	struct traversing_engine_task					task;
	char * 											global_path;		/*local copy - keep track of it*/
	struct element_type_hierarchy_member *			p_element_type_hierarchy_member;
	struct element_type_hierarchy *					p_parent_hierarchy;
};
/*****************************************************************************/
struct element_type_top_container_element_type {
	struct cds_list_head							list;

	struct metac_type_member_info *					p_from_member_info;
	struct element_type *							p_element_type;
};
struct element_type_top_container {
	struct cds_list_head							pointers_element_type_list;
	struct cds_list_head							arrays_element_type_list;
};
struct element_type_top_builder {
	struct metac_type *								p_root_type;
	metac_type_annotation_t *						p_override_annotations;

	struct element_type_top_container				container;
	
	struct traversing_engine						element_type_traversing_engine;
	struct cds_list_head							element_type_executed_tasks;
};
struct element_type_top_builder_element_type_task {
	struct traversing_engine_task					task;
	char * 											global_path;	/*local copy - keep track of it*/
	struct metac_type *								p_type;
	struct metac_type_member_info *					p_from_member_info;
	struct element_type **							pp_element_type_to_store;
};
/*****************************************************************************/
/*scheduling prototypes*/
static int element_type_hierarchy_top_builder_schedule_element_type_hierarchy_member(
		struct element_type_hierarchy_top_builder * p_element_type_hierarchy_top_builder,
		char *										global_path,
		struct element_type_hierarchy_member *		p_element_type_hierarchy_member,
		struct element_type_hierarchy *				p_parent_hierarchy);
static int element_type_top_builder_schedule_element_type_from_array(
		struct element_type_top_builder *			p_element_type_top_builder,
		char * 										global_path,
		struct metac_type *							p_type,
		struct metac_type_member_info *				p_member_info,
		struct element_type **						pp_element_type_to_store);
static int element_type_top_builder_schedule_element_type_from_pointer(
		struct element_type_top_builder *			p_element_type_top_builder,
		char * 										global_path,
		struct metac_type *							p_type,
		struct metac_type_member_info *				p_member_info,
		struct element_type **						pp_element_type_to_store);
/*****************************************************************************/
static char * build_member_path(char * path, char * name){
	char * member_path;
	size_t path_size;
	size_t name_size = 0;

	path_size = strlen(path);
	if (name != NULL)
		name_size = strlen(name);

	if (name_size == 0)
		return strdup(path);
	if (path_size == 0)
		return strdup(name);

	member_path = calloc(sizeof(char), path_size + (size_t)1 + name_size + (size_t)1);
	if (member_path == NULL)
		return NULL;
	strcpy(member_path, path);
	strcpy(member_path + path_size, ".");
	strcpy(member_path + path_size + 1, name);
	return member_path;
}
#define alloc_sptrinf(...) ({\
	int len = snprintf(NULL, 0, ##__VA_ARGS__); \
	char *str = calloc(len + 1, sizeof(*str)); \
	if (str != NULL)snprintf(str, len + 1, ##__VA_ARGS__); \
	str; \
})
/*****************************************************************************/
int discriminator_delete(
		struct discriminator **						pp_discriminator) {
	_delete_(discriminator);
	return 0;
}
struct discriminator * discriminator_create(
		struct metac_type *							p_root_type,
		char * 										global_path,
		metac_type_annotation_t *					p_override_annotations,
		struct discriminator *						p_previous_discriminator,
		metac_discriminator_value_t					previous_expected_discriminator_value) {
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

	p_discriminator->cb = p_annotation->value->discriminator.cb;
	p_discriminator->p_data = p_annotation->value->discriminator.data;

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
		metac_flag									keep_data
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
int element_type_array_init(
		struct metac_type *							p_root_type,
		char * 										global_path,
		metac_type_annotation_t *					p_override_annotations,
		struct metac_type *							p_actual_type,
		struct element_type_array *					p_element_type_array) {
	const metac_type_annotation_t * p_annotation;
	struct metac_type * p_target_actual_type;
	assert(p_actual_type->id == DW_TAG_array_type);
	p_annotation = metac_type_annotation(p_root_type, global_path, p_override_annotations);

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
			p_element_type_array->array_elements_count.cb = metac_array_elements_1d_with_null;
		}
	}

	if (p_annotation != NULL) {
		if (p_actual_type->array_type_info.is_flexible != 0) {
			p_element_type_array->array_elements_count.cb = p_annotation->value->array_elements_count.cb;
			p_element_type_array->array_elements_count.p_data = p_annotation->value->array_elements_count.data;
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
void element_type_array_clean(
		struct element_type_array *					p_element_type_array) {
}
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
int element_type_pointer_init(
		struct metac_type *							p_root_type,
		char * 										global_path,
		metac_type_annotation_t *					p_override_annotations,
		struct metac_type *							p_actual_type,
		struct element_type_pointer *				p_element_type_pointer) {
	const metac_type_annotation_t * p_annotation;
	struct metac_type * p_target_actual_type;
	assert(p_actual_type->id == DW_TAG_pointer_type);
	p_annotation = metac_type_annotation(p_root_type, global_path, p_override_annotations);

	/*defaults is metac_array_elements_single */
	p_element_type_pointer->array_elements_count.cb = metac_array_elements_single;

	/*default for char* is metac_array_elements_1d_with_null */
	if (p_actual_type->pointer_type_info.type != NULL) {
		p_target_actual_type = metac_type_actual_type(p_actual_type->pointer_type_info.type);
		if (p_target_actual_type != NULL &&
			p_target_actual_type->id == DW_TAG_base_type && (
				p_target_actual_type->base_type_info.encoding == DW_ATE_signed_char ||
				p_target_actual_type->base_type_info.encoding == DW_ATE_unsigned_char) &&
				p_target_actual_type->name != NULL && (
				strcmp(p_target_actual_type->name, "char") == 0 ||
				strcmp(p_target_actual_type->name, "unsigned char") == 0)){
			p_element_type_pointer->array_elements_count.cb = metac_array_elements_1d_with_null;
		}
	}

	if (p_annotation != NULL) {
		p_element_type_pointer->generic_cast.cb = p_annotation->value->generic_cast.cb;
		p_element_type_pointer->generic_cast.p_data = p_annotation->value->generic_cast.data;
		p_element_type_pointer->generic_cast.p_types = generic_cast_type_create(
				p_annotation->value->generic_cast.types,
				&p_element_type_pointer->generic_cast.types_count);

		p_element_type_pointer->array_elements_count.cb = p_annotation->value->array_elements_count.cb;
		p_element_type_pointer->array_elements_count.p_data = p_annotation->value->array_elements_count.data;

		if (p_annotation->value->discriminator.cb != NULL) {
			msg_stderr("Annotations for pointer with global path %s defines discriminator that won't be used\n", global_path);
		}
	}
	return 0;
}
void element_type_pointer_clean(
		struct element_type_pointer *				p_element_type_pointer) {
	if (p_element_type_pointer->generic_cast.p_types != NULL) {
		free(p_element_type_pointer->generic_cast.p_types);
		p_element_type_pointer->generic_cast.p_types = NULL;
	}
}
int element_type_hierarchy_init(
		struct metac_type *							p_root_type,
		char * 										global_path,
		metac_type_annotation_t *					p_override_annotations,
		struct metac_type *							p_actual_type,
		struct element_type_hierarchy *				p_element_type_hierarchy,
		struct element_type_hierarchy *				p_parent_hierarchy) {
	const metac_type_annotation_t * p_annotation;
	assert(p_actual_type->id == DW_TAG_structure_type || p_actual_type->id == DW_TAG_union_type);
	p_annotation = metac_type_annotation(p_root_type, global_path, p_override_annotations);

	p_element_type_hierarchy->parent = p_parent_hierarchy;
	p_element_type_hierarchy->members_count = p_actual_type->structure_type_info.members_count;
	if (p_element_type_hierarchy->members_count > 0) {
		p_element_type_hierarchy->members =
				(struct element_type_hierarchy_member **)calloc(
						p_element_type_hierarchy->members_count,
						sizeof(*p_element_type_hierarchy->members));
		if (p_element_type_hierarchy->members == NULL) {
			msg_stderr("Can't allocate memory for hierarchy\n");
			p_element_type_hierarchy->members_count = 0;
			return (-EFAULT);
		}
	}
	return 0;
}
void element_type_hierarchy_clean(
		struct element_type_hierarchy *				p_element_type_hierarchy) {
	if (p_element_type_hierarchy->members != NULL) {
		free(p_element_type_hierarchy->members);
		p_element_type_hierarchy->members = NULL;
	}
}
int element_type_hierarchy_member_delete(
		struct element_type_hierarchy_member **		pp_element_type_hierarchy_member) {
	_delete_start_(element_type_hierarchy_member);
	if ((*pp_element_type_hierarchy_member)->p_actual_type)
		switch((*pp_element_type_hierarchy_member)->p_actual_type->id){
		case DW_TAG_structure_type:
		case DW_TAG_union_type:
			element_type_hierarchy_clean(&(*pp_element_type_hierarchy_member)->hierarchy);
			break;
		case DW_TAG_array_type:
			element_type_array_clean(&(*pp_element_type_hierarchy_member)->array);
			break;
		case DW_TAG_pointer_type:
			element_type_pointer_clean(&(*pp_element_type_hierarchy_member)->pointer);
			break;
	}
	if ((*pp_element_type_hierarchy_member)->path_within_hierarchy) {
		free((*pp_element_type_hierarchy_member)->path_within_hierarchy);
		(*pp_element_type_hierarchy_member)->path_within_hierarchy = NULL;
	}
	_delete_finish(element_type_hierarchy_member);
	return 0;
}
static struct element_type_hierarchy_member * element_type_hierarchy_member_get_parent_element_hierarchy_member(
		struct element_type_hierarchy_member *		p_element_type_hierarchy_member) {
	if (p_element_type_hierarchy_member == NULL ||
		p_element_type_hierarchy_member->p_hierarchy == NULL) {
		return NULL;
	}
	if (p_element_type_hierarchy_member->p_hierarchy->parent != NULL)
		return cds_list_entry(p_element_type_hierarchy_member->p_hierarchy, struct element_type_hierarchy_member, hierarchy);
	return NULL;
}
struct element_type_hierarchy_member * element_type_hierarchy_member_create(
		struct metac_type *							p_root_type,
		char *										global_path,
		char *										hirarchy_path,
		metac_type_annotation_t *					p_override_annotations,
		metac_count_t								member_id,
		struct discriminator *						p_discriminator,
		metac_discriminator_value_t					expected_discriminator_value,
		struct element_type_hierarchy *				p_hierarchy,
		struct metac_type_member_info *				p_member_info) {
	struct element_type_hierarchy_member * p_parent_element_type_hierarchy_member;
	_create_(element_type_hierarchy_member);

	p_element_type_hierarchy_member->member_id = member_id;
	p_element_type_hierarchy_member->p_member_info = p_member_info;

	p_element_type_hierarchy_member->precondition.p_discriminator = p_discriminator;
	p_element_type_hierarchy_member->precondition.expected_discriminator_value = expected_discriminator_value;
	p_element_type_hierarchy_member->p_hierarchy = p_hierarchy;

	p_element_type_hierarchy_member->offset = p_member_info->data_member_location;
	p_parent_element_type_hierarchy_member = element_type_hierarchy_member_get_parent_element_hierarchy_member(p_element_type_hierarchy_member);
	if (p_parent_element_type_hierarchy_member != NULL) {
		p_element_type_hierarchy_member->offset += p_parent_element_type_hierarchy_member->offset;
	}

	p_element_type_hierarchy_member->p_type = p_member_info->type;
	if (p_element_type_hierarchy_member->p_type != NULL) {
		p_element_type_hierarchy_member->p_actual_type = metac_type_actual_type(p_member_info->type);
	}

	p_element_type_hierarchy_member->path_within_hierarchy = strdup(hirarchy_path);
	if (p_element_type_hierarchy_member->path_within_hierarchy == NULL) {
		msg_stderr("wasn't able to build hirarchy_path for %s\n", hirarchy_path);
		element_type_hierarchy_member_delete(&p_element_type_hierarchy_member);
		return NULL;
	}
	/*msg_stddbg("local path: %s\n", p_element_type_hierarchy_member->path_within_hierarchy);*/

	if (p_element_type_hierarchy_member->p_actual_type != NULL) {
		if (p_element_type_hierarchy_member->p_actual_type->id == DW_TAG_base_type) {
			p_element_type_hierarchy_member->base_type.p_bit_offset = p_member_info->p_bit_offset;
			p_element_type_hierarchy_member->base_type.p_bit_size = p_member_info->p_bit_size;
		}else{
			int res;
			if (p_member_info->p_bit_offset != NULL || p_member_info->p_bit_size != NULL) {
				msg_stderr("incorrect type %d for p_bit_offset and p_bit_size\n", p_element_type_hierarchy_member->p_actual_type->id);
				assert(0);
			}

			switch(p_element_type_hierarchy_member->p_actual_type->id) {
			case DW_TAG_structure_type:
			case DW_TAG_union_type:
				res = element_type_hierarchy_init(p_root_type, global_path, p_override_annotations, p_element_type_hierarchy_member->p_actual_type, &p_element_type_hierarchy_member->hierarchy, p_hierarchy);
				break;
			case DW_TAG_array_type:
				res = element_type_array_init(p_root_type, global_path, p_override_annotations, p_element_type_hierarchy_member->p_actual_type, &p_element_type_hierarchy_member->array);
				break;
			case DW_TAG_pointer_type:
				res = element_type_pointer_init(p_root_type, global_path, p_override_annotations, p_element_type_hierarchy_member->p_actual_type, &p_element_type_hierarchy_member->pointer);
				break;
			}

			if (res != 0) {
				msg_stddbg("initialization failed\n");
				element_type_hierarchy_member_delete(&p_element_type_hierarchy_member);
				return NULL;
			}
		}
	}
	return p_element_type_hierarchy_member;
}
static int element_type_hierarchy_top_container_init_element_type_hierarchy_member(
	struct element_type_hierarchy_top_container_element_type_hierarchy_member *
													p_element_type_hierarchy_top_container_element_type_hierarchy_member,
	struct element_type_hierarchy_member *			p_element_type_hierarchy_member) {
	if (p_element_type_hierarchy_member == NULL) {
		msg_stderr("Can't init p_element_type_hierarchy_top_container_element_type_hierarchy_member with NULL p_element_type_hierarchy_member \n");
		return (-EINVAL);
	}
	p_element_type_hierarchy_top_container_element_type_hierarchy_member->p_element_type_hierarchy_member = p_element_type_hierarchy_member;
	return 0;
}
static void element_type_hierarchy_top_container_clean_element_type_hierarchy_member(
		struct element_type_hierarchy_top_container_element_type_hierarchy_member *
													p_element_type_hierarchy_top_container_element_type_hierarchy_member,
		metac_flag									keep_data
		) {
	if (keep_data == 0) {
		element_type_hierarchy_member_delete(&p_element_type_hierarchy_top_container_element_type_hierarchy_member->p_element_type_hierarchy_member);
	}
	p_element_type_hierarchy_top_container_element_type_hierarchy_member->p_element_type_hierarchy_member = NULL;
}
static struct element_type_hierarchy_top_container_element_type_hierarchy_member * element_type_hierarchy_top_container_create_element_type_hierarchy_member(
		struct element_type_hierarchy_member *			p_element_type_hierarchy_member) {
	int res;
	_create_(element_type_hierarchy_top_container_element_type_hierarchy_member);
	if (element_type_hierarchy_top_container_init_element_type_hierarchy_member(
			p_element_type_hierarchy_top_container_element_type_hierarchy_member,
			p_element_type_hierarchy_member) != 0) {
		free(p_element_type_hierarchy_top_container_element_type_hierarchy_member);
		return NULL;
	}
	return p_element_type_hierarchy_top_container_element_type_hierarchy_member;
}
static int element_type_hierarchy_top_builder_add_element_type_hierarchy_member (
		struct element_type_hierarchy_top_builder * p_element_type_hierarchy_top_builder,
		struct element_type_hierarchy_member *		p_element_type_hierarchy_member) {
	struct element_type_hierarchy_top_container_element_type_hierarchy_member * p_element_type_hierarchy_top_container_element_type_hierarchy_member;
	/* create */
	p_element_type_hierarchy_top_container_element_type_hierarchy_member = element_type_hierarchy_top_container_create_element_type_hierarchy_member(
			p_element_type_hierarchy_member);
	if (p_element_type_hierarchy_top_container_element_type_hierarchy_member == NULL) {
		msg_stddbg("wasn't able to create p_element_type_hierarchy_top_container_element_type_hierarchy_member\n");
		return (-EFAULT);
	}
	if (p_element_type_hierarchy_member->p_actual_type &&
		(p_element_type_hierarchy_member->p_actual_type->id == DW_TAG_structure_type ||
		p_element_type_hierarchy_member->p_actual_type->id == DW_TAG_union_type)){
	}
	/* add to list */
	cds_list_add_tail(
		&p_element_type_hierarchy_top_container_element_type_hierarchy_member->list,
		&p_element_type_hierarchy_top_builder->container.element_type_hierarchy_member_type_list);
	return 0;
}
static int element_type_hierarchy_top_builder_process_structure(
		struct element_type_hierarchy_top_builder * p_element_type_hierarchy_top_builder,
		char *										global_path,
		char *										hirarchy_path,
		struct discriminator *						p_discriminator,
		metac_discriminator_value_t					expected_discriminator_value,
		struct metac_type *							p_actual_type,
		struct element_type_hierarchy *				p_hierarchy) {
	metac_num_t i;
	assert(p_actual_type->id == DW_TAG_structure_type);

	for (i = p_actual_type->structure_type_info.members_count; i > 0; i--) {
		int indx = i-1;
		struct metac_type_member_info * p_member_info = &p_actual_type->structure_type_info.members[indx];
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
				expected_discriminator_value,
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
		struct element_type_hierarchy_top_builder * p_element_type_hierarchy_top_builder,
		char *										global_path,
		char *										hirarchy_path,
		struct discriminator *						p_previous_discriminator,
		metac_discriminator_value_t					previous_expected_discriminator_value,
		struct metac_type *							p_actual_type,
		struct element_type_hierarchy *				p_hierarchy) {
	metac_num_t i;
	struct discriminator * p_discriminator;
	assert(p_actual_type->id == DW_TAG_union_type);

	p_discriminator = discriminator_create(
			p_element_type_hierarchy_top_builder->p_root_type,
			global_path,
			p_element_type_hierarchy_top_builder->p_override_annotations,
			p_previous_discriminator,
			previous_expected_discriminator_value);
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
		metac_flag									keep_data) {
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
		metac_flag									keep_data) {
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
/*****************************************************************************/
static metac_flag element_type_is_potentially_flexible(
		struct element_type *						p_element_type) {
	switch(p_element_type->p_actual_type->id) {
	case DW_TAG_array_type:
		return p_element_type->p_actual_type->array_type_info.is_flexible;
	case DW_TAG_structure_type:
	case DW_TAG_union_type: {
		metac_count_t i;
		for (i = 0; i < p_element_type->hierarchy_top.members_count; ++i) {
			if (p_element_type->hierarchy_top.pp_members[i]->p_actual_type != NULL &&
				p_element_type->hierarchy_top.pp_members[i]->p_actual_type->id == DW_TAG_array_type &&
				p_element_type->hierarchy_top.pp_members[i]->p_actual_type->array_type_info.is_flexible) {
				if (p_element_type->hierarchy_top.pp_members[i]->offset == p_element_type->byte_size)
					return 1;
				else {
					msg_stderr("flexible array isn't at the end of the structure\n");
					return 1;
				}
			}
		}
	}}
	return 0;
}
int element_type_delete(
		struct element_type **						pp_element_type) {
	_delete_start_(element_type);
	if ((*pp_element_type)->p_actual_type != NULL) {
		switch((*pp_element_type)->p_actual_type->id) {
		case DW_TAG_structure_type:
		case DW_TAG_union_type:
			element_type_hierarchy_top_clean(&(*pp_element_type)->hierarchy_top);
			break;
		case DW_TAG_array_type:
			element_type_array_clean(&(*pp_element_type)->array);
			break;
		case DW_TAG_pointer_type:
			element_type_pointer_clean(&(*pp_element_type)->pointer);
			break;
		}
	}
	_delete_finish(element_type);
	return 0;
}
struct element_type * element_type_create(
		struct metac_type *							p_root_type,
		char *										global_path,
		metac_type_annotation_t *					p_override_annotations,
		struct metac_type *							p_type) {
	int res = 0;
	_create_(element_type);
	p_element_type->p_type = p_type;
	p_element_type->p_actual_type = metac_type_actual_type(p_type);
	p_element_type->byte_size = metac_type_byte_size(p_type);
	
	switch(p_element_type->p_actual_type->id) {
	case DW_TAG_structure_type:
	case DW_TAG_union_type:
		res = element_type_hierarchy_top_init(p_root_type, global_path, p_override_annotations, p_element_type->p_actual_type, &p_element_type->hierarchy_top);
		break;
	case DW_TAG_array_type:
		res = element_type_array_init(p_root_type, global_path, p_override_annotations, p_element_type->p_actual_type, &p_element_type->array);
		break;
	case DW_TAG_pointer_type:
		res = element_type_pointer_init(p_root_type, global_path, p_override_annotations, p_element_type->p_actual_type, &p_element_type->pointer);
		break;
	}
	if (res != 0) {
		msg_stddbg("initialization failed\n");
		element_type_delete(&p_element_type);
		return NULL;
	}
	p_element_type->is_potentially_flexible = element_type_is_potentially_flexible(p_element_type);
	return p_element_type;
}
metac_flag element_type_is_hierachy(
		struct element_type *						p_element_type) {
	if (p_element_type != NULL &&
		p_element_type->p_actual_type != NULL && (
			p_element_type->p_actual_type->id == DW_TAG_structure_type ||
			p_element_type->p_actual_type->id == DW_TAG_union_type
		))
		return 1;
	return 0;
}
static int element_type_top_container_init_element_type(
		struct element_type_top_container_element_type *
													p_element_type_top_container_element_type,
	struct element_type *							p_element_type,
	struct metac_type_member_info *					p_from_member_info) {
	if (p_element_type == NULL) {
		msg_stderr("Can't init precomple_type_container_element_type with NULL p_element_type \n");
		return (-EINVAL);
	}
	p_element_type_top_container_element_type->p_from_member_info = p_from_member_info;
	p_element_type_top_container_element_type->p_element_type = p_element_type;
	return 0;
}
static void element_type_top_container_clean_element_type(
		struct element_type_top_container_element_type *
													p_element_type_top_container_element_type,
		metac_flag									keep_data
		) {
	if (keep_data == 0) {
		element_type_delete(&p_element_type_top_container_element_type->p_element_type);
	}
	p_element_type_top_container_element_type->p_element_type = NULL;
}
static int element_type_top_container_init(
		struct element_type_top_container *			p_element_type_top_container) {
	CDS_INIT_LIST_HEAD(&p_element_type_top_container->pointers_element_type_list);
	CDS_INIT_LIST_HEAD(&p_element_type_top_container->arrays_element_type_list);
	return 0;
}
static void element_type_top_container_clean(
		struct element_type_top_container *			p_element_type_top_container,
		metac_flag									keep_data) {
	struct element_type_top_container_element_type * _element_type, * __element_type;
	cds_list_for_each_entry_safe(_element_type, __element_type, &p_element_type_top_container->pointers_element_type_list, list) {
		cds_list_del(&_element_type->list);
		element_type_top_container_clean_element_type(_element_type, keep_data);
		free(_element_type);
	}
	cds_list_for_each_entry_safe(_element_type, __element_type, &p_element_type_top_container->arrays_element_type_list, list) {
		cds_list_del(&_element_type->list);
		element_type_top_container_clean_element_type(_element_type, keep_data);
		free(_element_type);
	}
}
static int element_type_top_builder_process_element_type_array(
		struct element_type_top_builder *			p_element_type_top_builder,
		char *										global_path,
		struct element_type *						p_element_type,
		struct element_type_hierarchy_member *		p_element_type_hierarchy_member) {
	int i = 0;
	int res = (-EINVAL);
	struct metac_type * p_actual_type;
	struct element_type_array *p_array;
	struct metac_type_member_info * p_member_info = NULL;

	if (p_element_type_hierarchy_member != NULL) {
		p_actual_type = p_element_type_hierarchy_member->p_actual_type;
		p_array = &p_element_type_hierarchy_member->array;
		p_member_info = p_element_type_hierarchy_member->p_member_info;
	} else {
		p_actual_type = p_element_type->p_actual_type;
		p_array = &p_element_type->array;
	}

	assert(p_actual_type->id == DW_TAG_array_type);

	/* schedule array */
	if (p_actual_type->array_type_info.type != NULL) {
		char * target_global_path;
		target_global_path = alloc_sptrinf("%s[]", global_path);
		if (target_global_path == NULL) {
			msg_stderr("Can't build target_global_path for %s\n", global_path);
			return (-EFAULT);
		}
		res = element_type_top_builder_schedule_element_type_from_array(
				p_element_type_top_builder,
				target_global_path,
				p_actual_type->array_type_info.type,
				p_member_info,
				&p_array->p_element_type);
		free(target_global_path);
		if (res != 0){
			msg_stderr("Can't element_type_top_builder_schedule_element_type_from_array for %s\n", global_path);
			return (-EFAULT);
		}
	}

	return 0;
}
static int element_type_top_builder_process_element_type_pointer(
		struct element_type_top_builder *			p_element_type_top_builder,
		char *										global_path,
		struct element_type *						p_element_type,
		struct element_type_hierarchy_member *		p_element_type_hierarchy_member) {
	int i = 0;
	int res = (-EINVAL);
	struct metac_type * p_actual_type;
	struct element_type_pointer *p_pointer;
	struct metac_type_member_info * p_member_info = NULL;

	if (p_element_type_hierarchy_member != NULL) {
		p_actual_type = p_element_type_hierarchy_member->p_actual_type;
		p_pointer = &p_element_type_hierarchy_member->pointer;
		p_member_info = p_element_type_hierarchy_member->p_member_info;
	} else {
		p_actual_type = p_element_type->p_actual_type;
		p_pointer = &p_element_type->pointer;
	}

	assert(p_actual_type->id == DW_TAG_pointer_type);

	/* schedule main pointer */
	if (p_actual_type->pointer_type_info.type != NULL) {
		char * target_global_path;
		target_global_path = alloc_sptrinf("%s[]", global_path);
		if (target_global_path == NULL) {
			msg_stderr("Can't build target_global_path for %s\n", global_path);
			return (-EFAULT);
		}
		res = element_type_top_builder_schedule_element_type_from_pointer(
				p_element_type_top_builder,
				target_global_path,
				p_actual_type->pointer_type_info.type,
				p_member_info,
				&p_pointer->p_element_type);
		free(target_global_path);
		if (res != 0){
			msg_stderr("Can't element_type_top_builder_schedule_element_type_from_pointer for %s\n", global_path);
			return (-EFAULT);
		}
	}

	for (i = 0; i < p_pointer->generic_cast.types_count; ++i) {
		char * target_global_path = alloc_sptrinf(
				"generic_cast<*%s>(%s)[]",
				p_pointer->generic_cast.p_types[i].p_type->name,
				global_path);
		if (target_global_path == NULL){
			msg_stderr("Can't build target_global_path for %s generic_cast %d\n", global_path, i);
			return (-EFAULT);
		}
		res = element_type_top_builder_schedule_element_type_from_pointer(
				p_element_type_top_builder,
				target_global_path,
				p_actual_type->pointer_type_info.type,
				p_member_info,
				&p_pointer->generic_cast.p_types[i].p_element_type
				);
		free(target_global_path);
		if (res != 0){
			msg_stderr("Can't element_type_top_builder_schedule_element_type_from_pointer for %s generic_cast %d\n", global_path, i);
			return (-EFAULT);
		}
	}

	if (res != 0) {
		msg_stddbg("Didn't schedule anything for %s\n", global_path);
	}
	return 0;
}
static int element_type_top_builder_process_element_type_hierarchy(
		struct element_type_top_builder *			p_element_type_top_builder,
		char *										global_path,
		struct element_type *						p_element_type) {
	int i = 0;
	int res = (-EINVAL);

	assert(	p_element_type->p_actual_type->id == DW_TAG_structure_type ||
			p_element_type->p_actual_type->id == DW_TAG_union_type);

	for (i = 0; i < p_element_type->hierarchy_top.members_count; ++i) {
		assert(p_element_type->hierarchy_top.pp_members);
		if (	p_element_type->hierarchy_top.pp_members[i]->p_actual_type && (
				p_element_type->hierarchy_top.pp_members[i]->p_actual_type->id == DW_TAG_array_type ||
				p_element_type->hierarchy_top.pp_members[i]->p_actual_type->id == DW_TAG_pointer_type)) {
			char * target_global_path = alloc_sptrinf(
					"%s.%s",
					global_path,
					p_element_type->hierarchy_top.pp_members[i]->path_within_hierarchy);
			if (target_global_path == NULL){
				msg_stderr("Can't build target_global_path for %s member %d\n", global_path, i);
				return (-EFAULT);
			}
			switch(p_element_type->hierarchy_top.pp_members[i]->p_actual_type->id){
			case DW_TAG_array_type:
				res = element_type_top_builder_process_element_type_array(
						p_element_type_top_builder,
						target_global_path,
						p_element_type,
						p_element_type->hierarchy_top.pp_members[i]);
				if (res != 0){
					msg_stderr("Can't element_type_top_builder_schedule_element_type_from_array for %s member %d\n", global_path, i);
					free(target_global_path);
					return (-EFAULT);
				}
				break;
			case DW_TAG_pointer_type:
				res = element_type_top_builder_process_element_type_pointer(
						p_element_type_top_builder,
						target_global_path,
						p_element_type,
						p_element_type->hierarchy_top.pp_members[i]);
				if (res != 0){
					msg_stderr("Can't element_type_top_builder_process_element_type_pointer for %s member %d\n", global_path, i);
					free(target_global_path);
					return (-EFAULT);
				}
				break;
			}
			free(target_global_path);
		}
	}
	return 0;
}
static struct element_type_top_container_element_type * element_type_top_builder_element_type_create(
		struct element_type_top_builder *			p_element_type_top_builder,
		char *										global_path,
		struct metac_type *							p_type,
		struct metac_type_member_info *				p_from_member_info) {
	int res;
	_create_(element_type_top_container_element_type);
	res = element_type_top_container_init_element_type(
		p_element_type_top_container_element_type,
		element_type_create(
			p_element_type_top_builder->p_root_type,
			global_path,
			p_element_type_top_builder->p_override_annotations,
			p_type),
			p_from_member_info);
	if (res != 0) {
			msg_stddbg("element_type_top_container_element_type_init returned error - deleting\n");
			free(p_element_type_top_container_element_type);
			return NULL;		
	}

	switch(p_element_type_top_container_element_type->p_element_type->p_actual_type->id) {
	case DW_TAG_array_type:
		res = element_type_top_builder_process_element_type_array(
				p_element_type_top_builder,
				global_path,
				p_element_type_top_container_element_type->p_element_type,
				NULL);
		break;
	case DW_TAG_pointer_type:
		res = element_type_top_builder_process_element_type_pointer(
				p_element_type_top_builder,
				global_path,
				p_element_type_top_container_element_type->p_element_type,
				NULL);
		break;
	case DW_TAG_structure_type:
	case DW_TAG_union_type:
		res = element_type_top_builder_process_element_type_hierarchy(
				p_element_type_top_builder,
				global_path,
				p_element_type_top_container_element_type->p_element_type);
		break;
	}
	return p_element_type_top_container_element_type;
}
static int element_type_top_builder_add_element_type_from_array(
		struct element_type_top_builder *			p_element_type_top_builder,
		char *										global_path,
		struct metac_type *							p_type,
		struct metac_type_member_info *				p_from_member_info,
		struct element_type **						pp_element_type_to_store) {
	struct element_type_top_container_element_type * p_element_type_top_container_element_type;
	/* create */
	p_element_type_top_container_element_type = element_type_top_builder_element_type_create(
			p_element_type_top_builder,
			global_path,
			p_type,
			p_from_member_info);
	if (p_element_type_top_container_element_type == NULL) {
		msg_stddbg("wasn't able to create p_element_type_top_container_element_type\n");
		return (-EFAULT);
	}

	if (pp_element_type_to_store) {
		*pp_element_type_to_store = p_element_type_top_container_element_type->p_element_type;
	}

	/* add to pointers list */
	cds_list_add_tail(
		&p_element_type_top_container_element_type->list,
		&p_element_type_top_builder->container.arrays_element_type_list);
	return 0;
}
static int element_type_top_builder_add_element_type_from_pointer(
		struct element_type_top_builder *			p_element_type_top_builder,
		char *										global_path,
		struct metac_type *							p_type,
		struct metac_type_member_info *				p_from_member_info,
		struct element_type **						pp_element_type_to_store) {
	struct element_type_top_container_element_type * p_element_type_top_container_element_type;

	msg_stddbg("ADDING %p %p\n",
			p_type,
			p_from_member_info);

	/* try to find this type */
	cds_list_for_each_entry(
		p_element_type_top_container_element_type,
		&p_element_type_top_builder->container.pointers_element_type_list,
		list) {

		msg_stddbg("COMPARING WITH %p %p\n",
				p_element_type_top_container_element_type->p_element_type->type,
				p_element_type_top_container_element_type->p_from_member_info);

		if (p_element_type_top_container_element_type->p_element_type->p_type == p_type &&
				p_element_type_top_container_element_type->p_from_member_info == p_from_member_info) {
			msg_stddbg("found %p - skipping\n", p_element_type_top_container_element_type);

			if (pp_element_type_to_store) {
				*pp_element_type_to_store = p_element_type_top_container_element_type->p_element_type;
			}
			return 0;
		}
	}
	/* create */
	p_element_type_top_container_element_type = element_type_top_builder_element_type_create(
			p_element_type_top_builder,
			global_path,
			p_type,
			p_from_member_info);
	if (p_element_type_top_container_element_type == NULL) {
		msg_stderr("wasn't able to create p_element_type_top_container_element_type\n");
		return (-EFAULT);
	}

	if (pp_element_type_to_store) {
		*pp_element_type_to_store = p_element_type_top_container_element_type->p_element_type;
	}

	/* add to pointers list */
	cds_list_add_tail(
		&p_element_type_top_container_element_type->list,
		&p_element_type_top_builder->container.pointers_element_type_list);
	return 0;
}
static int element_type_top_builder_process_element_type_from_array_task_fn(
	struct traversing_engine * 						p_engine,
	struct traversing_engine_task * 				p_task,
	int 											error_flag) {
	struct element_type_top_builder * p_element_type_top_builder =
		cds_list_entry(p_engine, struct element_type_top_builder, element_type_traversing_engine);
	struct element_type_top_builder_element_type_task * p_element_type_top_builder_element_type_task =
		cds_list_entry(p_task, struct element_type_top_builder_element_type_task, task);
	cds_list_add_tail(&p_task->list, &p_element_type_top_builder->element_type_executed_tasks);
	if (error_flag != 0)return (-EALREADY);	
	return element_type_top_builder_add_element_type_from_array(
		p_element_type_top_builder,
		p_element_type_top_builder_element_type_task->global_path,
		p_element_type_top_builder_element_type_task->p_type,
		p_element_type_top_builder_element_type_task->p_from_member_info,
		p_element_type_top_builder_element_type_task->pp_element_type_to_store);
}
static int element_type_top_builder_process_element_type_from_pointer_task_fn(
	struct traversing_engine * 						p_engine,
	struct traversing_engine_task * 				p_task,
	int 											error_flag) {
	struct element_type_top_builder * p_element_type_top_builder =
		cds_list_entry(p_engine, struct element_type_top_builder, element_type_traversing_engine);
	struct element_type_top_builder_element_type_task * p_element_type_top_builder_element_type_task =
		cds_list_entry(p_task, struct element_type_top_builder_element_type_task, task);
	cds_list_add_tail(&p_task->list, &p_element_type_top_builder->element_type_executed_tasks);
	if (error_flag != 0)return (-EALREADY);	
	return element_type_top_builder_add_element_type_from_pointer(
		p_element_type_top_builder,
		p_element_type_top_builder_element_type_task->global_path,
		p_element_type_top_builder_element_type_task->p_type,
		p_element_type_top_builder_element_type_task->p_from_member_info,
		p_element_type_top_builder_element_type_task->pp_element_type_to_store);
}
static int element_type_top_builder_delete_element_type_task(
		struct element_type_top_builder_element_type_task **
													pp_element_type_top_builder_element_type_task) {
	_delete_start_(element_type_top_builder_element_type_task);
	if ((*pp_element_type_top_builder_element_type_task)->global_path) {
		free((*pp_element_type_top_builder_element_type_task)->global_path);
		(*pp_element_type_top_builder_element_type_task)->global_path = NULL;
	}
	_delete_finish(element_type_top_builder_element_type_task);
	return 0;
}
static struct element_type_top_builder_element_type_task * element_type_top_builder_schedule_element_type_with_fn(
		struct element_type_top_builder *			p_element_type_top_builder,
		traversing_engine_task_fn_t 				fn,
		char *										global_path,
		struct metac_type *							p_type,
		struct metac_type_member_info *				p_from_member_info,
		struct element_type **						pp_element_type_to_store) {
	_create_(element_type_top_builder_element_type_task);
	
	p_element_type_top_builder_element_type_task->task.fn = fn;
	p_element_type_top_builder_element_type_task->global_path = strdup(global_path);
	p_element_type_top_builder_element_type_task->p_type = p_type;
	p_element_type_top_builder_element_type_task->p_from_member_info = p_from_member_info;
	p_element_type_top_builder_element_type_task->pp_element_type_to_store = pp_element_type_to_store;
	if (p_element_type_top_builder_element_type_task->global_path == NULL) {
		msg_stderr("wasn't able to duplicate global_path\n");
		element_type_top_builder_delete_element_type_task(&p_element_type_top_builder_element_type_task);
		return NULL;
	}
	if (add_traversing_task_to_tail(
		&p_element_type_top_builder->element_type_traversing_engine,
		&p_element_type_top_builder_element_type_task->task) != 0) {
		msg_stderr("wasn't able to schedule task\n");
		element_type_top_builder_delete_element_type_task(&p_element_type_top_builder_element_type_task);
		return NULL;
	}
	return p_element_type_top_builder_element_type_task;
}
static int element_type_top_builder_schedule_element_type_from_array(
		struct element_type_top_builder *			p_element_type_top_builder,
		char *										global_path,
		struct metac_type *							p_type,
		struct metac_type_member_info *				p_from_member_info,
		struct element_type **						pp_element_type_to_store) {
	return (element_type_top_builder_schedule_element_type_with_fn(
		p_element_type_top_builder,
		element_type_top_builder_process_element_type_from_array_task_fn,
		global_path,
		p_type,
		p_from_member_info,
		pp_element_type_to_store)!=NULL)?0:(-EFAULT);
}
static int element_type_top_builder_schedule_element_type_from_pointer(
		struct element_type_top_builder *			p_element_type_top_builder,
		char *										global_path,
		struct metac_type *							p_type,
		struct metac_type_member_info *				p_from_member_info,
		struct element_type **						pp_element_type_to_store) {
	msg_stddbg("SHEDULE %p %p %p\n",
			p_type,
			p_from_member_info,
			pp_element_type_to_store);
	return (element_type_top_builder_schedule_element_type_with_fn(
		p_element_type_top_builder,
		element_type_top_builder_process_element_type_from_pointer_task_fn,
		global_path,
		p_type,
		p_from_member_info,
		pp_element_type_to_store)!=NULL)?0:(-EFAULT);
}
static int element_type_top_builder_init(
		struct element_type_top_builder *			p_element_type_top_builder,
		struct metac_type *							p_root_type,
		metac_type_annotation_t *					p_override_annotations) {
	p_element_type_top_builder->p_root_type = p_root_type;
	p_element_type_top_builder->p_override_annotations = p_override_annotations;
	element_type_top_container_init(&p_element_type_top_builder->container);
	CDS_INIT_LIST_HEAD(&p_element_type_top_builder->element_type_executed_tasks);
	traversing_engine_init(&p_element_type_top_builder->element_type_traversing_engine);
	return 0;
}
static void element_type_top_builder_clean(
		struct element_type_top_builder *			p_element_type_top_builder,
		metac_flag									keep_data) {
	struct traversing_engine_task * p_task, *_p_task;

	traversing_engine_clean(&p_element_type_top_builder->element_type_traversing_engine);
	cds_list_for_each_entry_safe(p_task, _p_task, &p_element_type_top_builder->element_type_executed_tasks, list) {
		struct element_type_top_builder_element_type_task * p_element_type_top_builder_element_type_task = cds_list_entry(
			p_task, 
			struct element_type_top_builder_element_type_task, task);
		cds_list_del(&p_task->list);
		element_type_top_builder_delete_element_type_task(&p_element_type_top_builder_element_type_task);
	}
	element_type_top_container_clean(&p_element_type_top_builder->container, keep_data);
	p_element_type_top_builder->p_root_type = NULL;
}
/*****************************************************************************/
void element_type_top_clean(
		struct element_type_top *					p_element_type_top) {
	metac_count_t i;

	if (p_element_type_top->pp_element_types != NULL) {
		for (i = 0 ; i < p_element_type_top->element_types_count; ++i) {
			element_type_delete(&p_element_type_top->pp_element_types[i]);
		}
		free(p_element_type_top->pp_element_types);
		p_element_type_top->pp_element_types = NULL;
	}

	if (p_element_type_top->p_element_type_for_pointer)
		element_type_delete(&p_element_type_top->p_element_type_for_pointer);
	if (p_element_type_top->p_pointer_type)
		metac_type_free(&p_element_type_top->p_pointer_type);
}
static int element_type_top_builder_finalize(
		struct element_type_top_builder *			p_element_type_top_builder,
		struct element_type_top *					p_element_type_top) {

	metac_count_t i;
	struct element_type_top_container_element_type * _element_type;

	p_element_type_top->element_types_count = 0;
	p_element_type_top->pp_element_types = NULL;

	/*get arrays lengths */
	cds_list_for_each_entry(_element_type, &p_element_type_top_builder->container.pointers_element_type_list, list) {
		++p_element_type_top->element_types_count;
	}
	cds_list_for_each_entry(_element_type, &p_element_type_top_builder->container.arrays_element_type_list, list) {
		++p_element_type_top->element_types_count;
	}

	/*fill all the data*/
	if (p_element_type_top->element_types_count > 0) {
		p_element_type_top->pp_element_types = (struct element_type **)calloc(
				p_element_type_top->element_types_count, sizeof(*p_element_type_top->pp_element_types));
		if (p_element_type_top->pp_element_types == NULL) {
			msg_stderr("can't allocate memory for element_types from pointers\n");
			return (-ENOMEM);
		}
		i = 0;
		cds_list_for_each_entry(_element_type, &p_element_type_top_builder->container.pointers_element_type_list, list) {
			p_element_type_top->pp_element_types[i] = _element_type->p_element_type;
			++i;
		}
		cds_list_for_each_entry(_element_type, &p_element_type_top_builder->container.arrays_element_type_list, list) {
			p_element_type_top->pp_element_types[i] = _element_type->p_element_type;
			++i;
		}
	}
	return 0;
}
int element_type_top_init(
		struct metac_type *							p_root_type,
		char *										global_path,
		metac_type_annotation_t *					p_override_annotations,
		struct element_type_top *					p_element_type_top) {
	struct element_type_top_builder element_type_top_builder;
	element_type_top_builder_init(&element_type_top_builder, p_root_type, p_override_annotations);

	p_element_type_top->p_pointer_type = metac_type_create_pointer_for(p_root_type);
	if (p_element_type_top->p_pointer_type == NULL) {
		msg_stderr("metac_type_create_pointer_for failed\n");
		element_type_top_builder_clean(&element_type_top_builder, 0);
		return (-EFAULT);
	}

	p_element_type_top->p_element_type_for_pointer = element_type_create(p_root_type, global_path, p_override_annotations, p_element_type_top->p_pointer_type);
	if (p_element_type_top->p_element_type_for_pointer == NULL) {
		msg_stderr("element_type_create failed\n");
		element_type_top_clean(p_element_type_top);
		element_type_top_builder_clean(&element_type_top_builder, 0);
		return (-EFAULT);
	}

	if (element_type_top_builder_process_element_type_pointer(
			&element_type_top_builder,
			global_path,
			p_element_type_top->p_element_type_for_pointer,
			NULL) != 0) {
		msg_stderr("wasn't able to schedule the first task\n");
		element_type_top_clean(p_element_type_top);
		element_type_top_builder_clean(&element_type_top_builder, 0);
		return (-EFAULT);
	}
	if (traversing_engine_run(&element_type_top_builder.element_type_traversing_engine) != 0) {
		msg_stderr("tasks execution finished with fail\n");
		element_type_top_clean(p_element_type_top);
		element_type_top_builder_clean(&element_type_top_builder, 0);
		return (-EFAULT);
	}
	/*fill in */
	if (element_type_top_builder_finalize(
			&element_type_top_builder,
			p_element_type_top) != 0) {
		msg_stderr("object finalization failed\n");
		element_type_top_clean(p_element_type_top);
		element_type_top_builder_clean(&element_type_top_builder, 0);
		return (-EFAULT);
	}
	/*clean builder objects*/
	element_type_top_builder_clean(&element_type_top_builder, 1);
	return 0;
}
metac_precompiled_type_t * metac_precompile_type(
		struct metac_type *							p_root_type,
		metac_type_annotation_t *					p_override_annotations) {
	_create_(metac_precompiled_type);

	if (element_type_top_init(
			p_root_type,
			"ptr",
			p_override_annotations,
			&p_metac_precompiled_type->element_type_top)!= 0) {
		msg_stderr("element_type_top_init failed\n");
		free(p_metac_precompiled_type);
		return NULL;
	}
	return p_metac_precompiled_type;
}
int metac_free_precompiled_type(
		metac_precompiled_type_t **					precompiled_type) {
	if (precompiled_type != NULL) {
		if ((*precompiled_type) != NULL) {
			element_type_top_clean(&(*precompiled_type)->element_type_top);

			free(*precompiled_type);
			*precompiled_type = NULL;
		}
		return 0;
	}
	return (-EINVAL);
}
/*****************************************************************************/
struct object_root_container_pointer {
	struct cds_list_head							list;

	char *											global_path;
	struct element *								p_element;
	struct element_hierarchy_member *				p_element_hierarchy_member;
};
struct object_root_container_memory_block {
	struct cds_list_head							list;

	struct memory_block *							p_memory_block;

	//struct cds_list_head							references;
};
struct object_root_container_memory_block_top {
	struct cds_list_head							list;

	struct object_root_container_memory_block *		p_object_root_container_memory_block;

	struct cds_list_head							object_root_container_memory_block_list;
	struct cds_list_head							object_root_container_pointer_list;
};
/*****************************************************************************/
struct memory_block_top_container {
	/* contains object_root_container_memory_block */
	struct cds_list_head							memory_block_list;
	/* contains object_root_container_pointer */
	struct cds_list_head							pointer_list;
};
struct memory_block_top_builder {
	struct object_root_container_memory_block_top *	p_object_root_container_memory_block_top;

	struct memory_block_top_container				container;

	struct traversing_engine						memory_block_top_traversing_engine;
	struct cds_list_head							memory_block_top_executed_tasks;
};
struct memory_block_top_builder_memory_block_task {
	struct traversing_engine_task					task;
	char *											global_path;
	//TODO: struct object_root_container_memory_block *	p_object_root_container_memory_block;
};
/*****************************************************************************/
struct object_root_container {
	/* contains object_root_container_memory_block_top */
	struct cds_list_head							memory_block_top_list;
};
struct object_root_builder {
	/*TODO: add params??? eventually */

	struct object_root *							p_object_root;

	struct object_root_container					container;

	struct traversing_engine						object_root_traversing_engine;
	struct cds_list_head							object_root_executed_tasks;
};
struct object_root_builder_memory_block_top_task {
	struct traversing_engine_task					task;
	char *											global_path;
	struct object_root_container_memory_block_top *	p_object_root_container_memory_block_top;
};
/*****************************************************************************/
static int object_root_builder_schedule_object_root_container_memory_block_top(
		struct object_root_builder *				p_object_root_builder,
		char *										global_path,
		struct object_root_container_memory_block_top *
													p_object_root_container_memory_block_top);
/*****************************************************************************/
int discriminator_value_free(
		struct discriminator_value **				pp_discriminator_value) {
	_delete_(discriminator_value);
	return 0;
}
struct discriminator_value * discriminator_value_create(
		metac_discriminator_value_t					value) {
	_create_(discriminator_value);
	p_discriminator_value->value = value;
	return p_discriminator_value;
}
void element_hierarchy_top_clean(
		struct element_hierarchy_top *				p_element_hierarchy_top,
		struct element_type_hierarchy_top *			p_element_type_hierarchy_top) {
	metac_count_t i;
	if (p_element_hierarchy_top->pp_hierarchy_members) {
		for (i = 0; i < p_element_type_hierarchy_top->members_count; ++i) {
			if (p_element_hierarchy_top->pp_hierarchy_members[i] != NULL) {
				//element_hierarchy_member_free(&p_element_hierarchy_top->pp_hierarchy_members[i]);
			}
		}
		free(p_element_hierarchy_top->pp_hierarchy_members);
		p_element_hierarchy_top->pp_hierarchy_members = NULL;
	}
	if (p_element_hierarchy_top->pp_discriminator_values) {
		for (i = 0; i < p_element_type_hierarchy_top->discriminators_count; ++i) {
			if (p_element_hierarchy_top->pp_discriminator_values[i] != NULL) {
				discriminator_value_free(&p_element_hierarchy_top->pp_discriminator_values[i]);
			}
		}
		free(p_element_hierarchy_top->pp_discriminator_values);
		p_element_hierarchy_top->pp_discriminator_values = NULL;
	}
}
int element_hierarchy_top_init(
		struct element_hierarchy_top *				p_element_hierarchy_top,
		struct element_type_hierarchy_top *			p_element_type_hierarchy_top) {
	if (p_element_type_hierarchy_top->discriminators_count > 0) {
		p_element_hierarchy_top->pp_discriminator_values =
				(struct discriminator_value **)calloc(p_element_type_hierarchy_top->discriminators_count, sizeof(*p_element_hierarchy_top->pp_discriminator_values));
		if (p_element_hierarchy_top->pp_discriminator_values != NULL) {
			msg_stderr("wasn't able to get memory for pp_discriminator_values\n");
			element_hierarchy_top_clean(p_element_hierarchy_top, p_element_type_hierarchy_top);
			return (-EFAULT);
		}
	}
	if (p_element_type_hierarchy_top->members_count > 0) {
		p_element_hierarchy_top->pp_hierarchy_members =
				(struct element_hierarchy_member **)calloc(p_element_type_hierarchy_top->members_count, sizeof(*p_element_hierarchy_top->pp_hierarchy_members));
		if (p_element_hierarchy_top->pp_hierarchy_members == NULL) {
			msg_stderr("wasn't able to get memory for pp_hierarchy_members\n");
			element_hierarchy_top_clean(p_element_hierarchy_top, p_element_type_hierarchy_top);
			return (-EFAULT);
		}
	}
	return 0;
}
void element_clean(
		struct element *							p_element) {
	if (p_element->p_element_type != NULL &&
		element_type_is_hierachy(p_element->p_element_type)) {
		element_hierarchy_top_clean(&p_element->hierarchy_top, &p_element->p_element_type->hierarchy_top);
	}
}
int element_init(
		struct element *							p_element,
		metac_count_t								id,
		struct element *							p_local_parent_element,
		struct element_hierarchy_member *			p_local_parent_element_hierarchy_member,
		struct element_type *						p_element_type,
		struct memory_block *						p_memory_block) {

	p_element->id = id;
	p_element->local_parent.p_element = p_local_parent_element;
	p_element->local_parent.p_element_hierarchy_member = p_local_parent_element_hierarchy_member;
	p_element->p_element_type = p_element_type;
	/*p_element->offset = id * p_element->p_element_type->byte_size;*/
	p_element->p_memory_block = p_memory_block;

	if (element_type_is_hierachy(p_element_type)) {
		if (element_hierarchy_top_init(
				&p_element->hierarchy_top,
				&p_element_type->hierarchy_top) != 0) {
			msg_stderr("wasn't able to init hierarchy_top: %s\n", p_element_type->p_type->name);
			return (-EFAULT);
		}
	}

	return 0;
}
void memory_block_clean(
		struct memory_block *						p_memory_block) {
	if (p_memory_block->p_elements != NULL) {
		metac_count_t i;

		for (i = 0; i < p_memory_block->elements_count; ++i) {
			element_clean(&p_memory_block->p_elements[i]);
		}

		free(p_memory_block->p_elements);
		p_memory_block->p_elements = NULL;
	}
}
int memory_block_init(
		struct memory_block *						p_memory_block,
		struct element *							p_local_parent_element,
		struct element_hierarchy_member *			p_local_parent_element_hierarchy_member,
		void *										ptr,
		metac_count_t								byte_size,
		struct element_type *						p_element_type,
		metac_count_t								elements_count) {
	metac_count_t i;

	if (p_local_parent_element != NULL) {
		p_memory_block->p_parent_memory_block = p_local_parent_element->p_memory_block;
		p_memory_block->parent_memory_block_offset = p_local_parent_element->id * p_local_parent_element->p_element_type->byte_size;
		/*TODO: check boundaries (byte_size)*/
		if (p_local_parent_element_hierarchy_member != NULL) {
			p_memory_block->parent_memory_block_offset += p_local_parent_element_hierarchy_member->p_element_type_hierarchy_member->offset;
		}
	}
	p_memory_block->ptr = ptr;
	p_memory_block->byte_size = byte_size;
	p_memory_block->elements_count = elements_count;

	p_memory_block->p_elements = calloc(p_memory_block->elements_count, sizeof(*p_memory_block->p_elements));
	if (p_memory_block->p_elements == NULL) {
		msg_stderr("can't allocate memory for elements\n");
		memory_block_clean(p_memory_block);
		return -ENOMEM;
	}

	for (i = 0; i < p_memory_block->elements_count; ++i) {
		if (element_init(
				&p_memory_block->p_elements[i],
				i,
				p_local_parent_element,
				p_local_parent_element_hierarchy_member,
				p_element_type,
				p_memory_block) != 0) {
			msg_stderr("element_init failed %d\n", (int)i);
			memory_block_clean(p_memory_block);
			return -EFAULT;
		}
	}
	return 0;
}
int memory_block_delete(
		struct memory_block **						pp_memory_block) {
	_delete_start_(memory_block);
	memory_block_clean(*pp_memory_block);
	_delete_finish(memory_block);
	return 0;
}
struct memory_block * memory_block_create(
		struct element *							p_local_parent_element,
		struct element_hierarchy_member *			p_local_parent_element_hierarchy_member,
		void *										ptr,
		metac_count_t								byte_size,
		struct element_type *						p_element_type,
		metac_count_t								elements_count) {
	_create_(memory_block);
	if (memory_block_init(
			p_memory_block,
			p_local_parent_element,
			p_local_parent_element_hierarchy_member,
			ptr,
			byte_size,
			p_element_type,
			elements_count) != 0) {
		msg_stderr("element_init failed\n");
		memory_block_delete(&p_memory_block);
		return NULL;
	}
	return p_memory_block;
}
/*****************************************************************************/
/*TODO:*/
//object_root_container_pointer_delete(&_memory_block, keep_data);
//object_root_container_pointer_create();
/*****************************************************************************/
static void object_root_container_memory_block_clean(
		struct object_root_container_memory_block *	p_object_root_container_memory_block,
		metac_flag									keep_data) {
	if (p_object_root_container_memory_block->p_memory_block != NULL) {
		memory_block_delete(&p_object_root_container_memory_block->p_memory_block);
	}
	/*TODO: cleanup references list: */
}
static int object_root_container_memory_block_init(
		struct object_root_container_memory_block *	p_object_root_container_memory_block,
		struct memory_block *						p_memory_block) {
	assert(p_object_root_container_memory_block);
	if (p_memory_block == NULL) {
		msg_stderr("p_memory_block is invalid\n");
		return -EINVAL;
	}
	p_object_root_container_memory_block->p_memory_block = p_memory_block;
	/*TODO: init references list */
	return 0;
}
static int object_root_container_memory_block_delete(
		struct object_root_container_memory_block **pp_object_root_container_memory_block,
		metac_flag									keep_data) {
	_delete_start_(object_root_container_memory_block);
	object_root_container_memory_block_clean(*pp_object_root_container_memory_block, keep_data);
	_delete_finish(object_root_container_memory_block);
	return 0;
}
static struct object_root_container_memory_block * object_root_container_memory_block_create(
		struct element *							p_local_parent_element,
		struct element_hierarchy_member *			p_local_parent_element_hierarchy_member,
		void *										ptr,
		metac_count_t								byte_size,
		struct element_type *						p_element_type,
		metac_count_t								elements_count
		) {
	_create_(object_root_container_memory_block);
	if (object_root_container_memory_block_init(
			p_object_root_container_memory_block,
				memory_block_create(
						p_local_parent_element,
						p_local_parent_element_hierarchy_member,
						ptr,
						byte_size,
						p_element_type,
						elements_count)) != 0) {
		object_root_container_memory_block_delete(&p_object_root_container_memory_block, 0);
		msg_stderr("object_root_container_memory_block_init failed\n");
		return NULL;
	}
	return p_object_root_container_memory_block;
}
/*****************************************************************************/
static void object_root_container_memory_block_top_clean(
		struct object_root_container_memory_block_top *
													p_object_root_container_memory_block_top,
		metac_flag									keep_data) {
	struct object_root_container_memory_block * _memory_block, *__memory_block;
	struct object_root_container_pointer * _pointer, * __pointer;
	if (p_object_root_container_memory_block_top->p_object_root_container_memory_block != NULL) {
		object_root_container_memory_block_delete(&p_object_root_container_memory_block_top->p_object_root_container_memory_block, keep_data);
	}

	/*this part will be used only when p_object_root_container_memory_block_top already filled-in (see object_root_builder_process_object_root_container_memory_block_top) */
	cds_list_for_each_entry_safe(_memory_block, __memory_block, &p_object_root_container_memory_block_top->object_root_container_memory_block_list, list) {
		cds_list_del(&_memory_block->list);
		object_root_container_memory_block_delete(&_memory_block, keep_data);
	}
	cds_list_for_each_entry_safe(_pointer, __pointer, &p_object_root_container_memory_block_top->object_root_container_pointer_list, list) {
		cds_list_del(&_pointer->list);
		/*TODO: cleanup: */
		//object_root_container_pointer_delete(&_memory_block, keep_data);
	}
}
static int object_root_container_memory_block_top_init(
		struct object_root_container_memory_block_top *
													p_object_root_container_memory_block_top,
		struct object_root_container_memory_block *	p_object_root_container_memory_block) {
	assert(p_object_root_container_memory_block_top);
	if (p_object_root_container_memory_block == NULL) {
		msg_stderr("p_object_root_container_memory_block is invalid\n");
		return -EINVAL;
	}
	p_object_root_container_memory_block_top->p_object_root_container_memory_block = p_object_root_container_memory_block;
	CDS_INIT_LIST_HEAD(&p_object_root_container_memory_block_top->object_root_container_pointer_list);
	CDS_INIT_LIST_HEAD(&p_object_root_container_memory_block_top->object_root_container_memory_block_list);

	return 0;
}
static int object_root_container_memory_block_top_delete(
		struct object_root_container_memory_block_top **
													pp_object_root_container_memory_block_top) {
	_delete_start_(object_root_container_memory_block_top);
	object_root_container_memory_block_top_clean(*pp_object_root_container_memory_block_top, 0);
	_delete_finish(object_root_container_memory_block_top);
	return 0;
}
static struct object_root_container_memory_block_top * object_root_container_memory_block_top_create(
		struct element *							p_local_parent_element,
		struct element_hierarchy_member *			p_local_parent_element_hierarchy_member,
		void *										ptr,
		metac_count_t								byte_size,
		struct element_type *						p_element_type,
		metac_count_t								elements_count
		) {
	_create_(object_root_container_memory_block_top);
	if (object_root_container_memory_block_top_init(
			p_object_root_container_memory_block_top,
			object_root_container_memory_block_create(
					p_local_parent_element,
					p_local_parent_element_hierarchy_member,
					ptr,
					byte_size,
					p_element_type,
					elements_count)) != 0) {
		object_root_container_memory_block_top_delete(&p_object_root_container_memory_block_top);
		msg_stderr("object_root_container_memory_block_top_init failed\n");
		return NULL;
	}
	return p_object_root_container_memory_block_top;
}
/*****************************************************************************/
static int memory_block_top_container_init(
		struct memory_block_top_container *			p_memory_block_top_container) {
	CDS_INIT_LIST_HEAD(&p_memory_block_top_container->memory_block_list);
	CDS_INIT_LIST_HEAD(&p_memory_block_top_container->pointer_list);
	return 0;
}
static void memory_block_top_container_clean(
		struct memory_block_top_container *			p_memory_block_top_container,
		metac_flag									keep_data) {
	struct object_root_container_memory_block * _memory_block, * __memory_block;
	struct object_root_container_pointer * _pointer, * __pointer;
	cds_list_for_each_entry_safe(_memory_block, __memory_block, &p_memory_block_top_container->memory_block_list, list) {
		cds_list_del(&_memory_block->list);
		object_root_container_memory_block_clean(_memory_block, keep_data);
		free(_memory_block);
	}
	cds_list_for_each_entry_safe(_pointer, __pointer, &p_memory_block_top_container->pointer_list, list) {
		cds_list_del(&_pointer->list);
		//TODO: array_top_container_clean_pointer(_element_type, keep_data);
		free(_pointer);
	}
}
static int memory_block_top_builder_init(
		struct memory_block_top_builder *			p_memory_block_top_builder,
		struct object_root_container_memory_block_top *
													p_object_root_container_memory_block_top
		) {
	p_memory_block_top_builder->p_object_root_container_memory_block_top = p_object_root_container_memory_block_top;

	memory_block_top_container_init(&p_memory_block_top_builder->container);
	CDS_INIT_LIST_HEAD(&p_memory_block_top_builder->memory_block_top_executed_tasks);
	traversing_engine_init(&p_memory_block_top_builder->memory_block_top_traversing_engine);
	return 0;
}
static void memory_block_top_builder_clean(
		struct memory_block_top_builder *			p_memory_block_top_builder,
		metac_flag									keep_data) {
	struct traversing_engine_task * p_task, *_p_task;

	traversing_engine_clean(&p_memory_block_top_builder->memory_block_top_traversing_engine);
	cds_list_for_each_entry_safe(p_task, _p_task, &p_memory_block_top_builder->memory_block_top_executed_tasks, list) {
		struct memory_block_top_builder_memory_block_task * p_memory_block_top_builder_memory_block_task = cds_list_entry(
			p_task,
			struct memory_block_top_builder_memory_block_task, task);
		cds_list_del(&p_task->list);
		//memory_block_top_builder_delete_memory_block_task(&p_memory_block_top_builder_memory_block_task);
	}
	memory_block_top_container_clean(&p_memory_block_top_builder->container, keep_data);
	p_memory_block_top_builder->p_object_root_container_memory_block_top = NULL;
}
static int memory_block_top_builder_process_object_root_container_memory_block(
		struct memory_block_top_builder *			p_memory_block_top_builder,
		char *										global_path,
		struct object_root_container_memory_block *	p_object_root_container_memory_block) {
	struct memory_block * p_memory_block = p_object_root_container_memory_block->p_memory_block;

	assert(p_memory_block != NULL);

	msg_stderr("PROCESSING %s: ptr=%p byte_size=%d type=%s elements_count=%d \n",
			global_path,
			p_memory_block->ptr,
			(int)p_memory_block->byte_size,
			p_memory_block->p_elements?p_memory_block->p_elements[0].p_element_type->p_type->name:"<invalid>",
			(int)p_memory_block->elements_count);

	metac_count_t i;

	for (i = 0; i < p_memory_block->elements_count; ++i) {
		struct metac_type * p_actual_type = metac_type_actual_type(p_memory_block->p_elements[i].p_element_type->p_type);
		if (p_actual_type != NULL) {
			switch (p_actual_type->id) {
			case DW_TAG_pointer_type:
//				/*TODO: update global path with [<i>]... or do it inside the function, so we can add there generic_cast info as well*/
//				object_root_builder_process_pointer(p_object_root_builder, global_path, &p_memory_block->p_elements[i], NULL);
				break;
			case DW_TAG_array_type:
				/*TODO: _process_array(p_object_root_builder, global_path, &p_memory_block->p_elements[i]);*/
				break;
			case DW_TAG_structure_type:
			case DW_TAG_union_type:
				/*TODO: _process_hierarchy_top(p_object_root_builder, global_path, &p_memory_block->p_elements[i]);*/
				break;
			}
		}
	}
	return 0;
}
/*****************************************************************************/
static int object_root_builder_delete_memory_block_top_task(
		struct object_root_builder_memory_block_top_task **		pp_object_root_builder_memory_block_top_task) {
	_delete_start_(object_root_builder_memory_block_top_task);
	if ((*pp_object_root_builder_memory_block_top_task)->global_path) {
		free((*pp_object_root_builder_memory_block_top_task)->global_path);
		(*pp_object_root_builder_memory_block_top_task)->global_path = NULL;
	}
	_delete_finish(object_root_builder_memory_block_top_task);
	return 0;
}
static struct object_root_builder_memory_block_top_task * object_root_builder_schedule_memory_block_top_with_fn(
		struct object_root_builder *				p_object_root_builder,
		traversing_engine_task_fn_t 				fn,
		char *										global_path,
		struct object_root_container_memory_block_top *	p_object_root_container_memory_block_top) {
	_create_(object_root_builder_memory_block_top_task);

	p_object_root_builder_memory_block_top_task->task.fn = fn;
	p_object_root_builder_memory_block_top_task->global_path = strdup(global_path);
	p_object_root_builder_memory_block_top_task->p_object_root_container_memory_block_top = p_object_root_container_memory_block_top;
	if (p_object_root_builder_memory_block_top_task->global_path == NULL) {
		msg_stderr("wasn't able to duplicate global_path\n");
		object_root_builder_delete_memory_block_top_task(&p_object_root_builder_memory_block_top_task);
		return NULL;
	}
	if (add_traversing_task_to_tail(
		&p_object_root_builder->object_root_traversing_engine,
		&p_object_root_builder_memory_block_top_task->task) != 0) {
		msg_stderr("wasn't able to schedule task\n");
		object_root_builder_delete_memory_block_top_task(&p_object_root_builder_memory_block_top_task);
		return NULL;
	}
	return p_object_root_builder_memory_block_top_task;
}
static int object_root_builder_process_pointer(
		struct object_root_builder *				p_object_root_builder,
		char *										global_path,
		struct element *							p_element,
		/*TODO: add support of v*/
		struct element_hierarchy_member *			p_element_hierarchy_member
) {
	/*TODO: VVV support several cases pointer and pointer as a member of hierarchy VVV */
	assert(p_element_hierarchy_member == NULL);

	/*TODO: recalculate global_path*/
	struct element_type * p_element_type;
	assert(p_element->p_element_type->p_type->id == DW_TAG_pointer_type);

	/*****************************************************************************/
	/*read pointer from memory_block*/
	p_element->pointer.ptr = ((void**)p_element->p_memory_block->ptr)[p_element->id]; /*TODO: emm... it's ok, but... add some checks */
	/* exit normally if pointer is NULL */
	if (p_element->pointer.ptr == NULL) {
		return 0;
	}
	/*****************************************************************************/

	/*type cast if needed*/
	p_element->pointer.actual_ptr = p_element->pointer.ptr;
	p_element_type = p_element->p_element_type->pointer.p_element_type;

	if (p_element->p_element_type->pointer.generic_cast.cb != NULL) {
		int res = p_element->p_element_type->pointer.generic_cast.cb(
				global_path,
				0,
				&p_element->pointer.use_cast,
				&p_element->pointer.generic_cast_type_id,
				&p_element->pointer.ptr,
				&p_element->pointer.actual_ptr,
				p_element->p_element_type->pointer.generic_cast.p_data);
		if (res != 0) {
			msg_stderr("generic_cast.cb returned error %d for %s\n", res, global_path);
			return res;
		}
		if (p_element->pointer.generic_cast_type_id >= p_element->p_element_type->pointer.generic_cast.types_count) {
			msg_stderr("generic_cast.cb set incorrect generic_cast_type_id %d for %s. Maximum value is %d\n",
					p_element->pointer.generic_cast_type_id,
					global_path,
					p_element->p_element_type->pointer.generic_cast.types_count);
			return -EINVAL;
		}
		p_element_type = p_element->p_element_type->pointer.generic_cast.p_types[p_element->pointer.generic_cast_type_id].p_element_type;
	}
	/* need to get array length*/
	if (p_element->p_element_type->pointer.array_elements_count.cb == NULL) {
		msg_stderr("Pointer length callback isn't defined for %s\n", global_path);
		return (-EFAULT);
	}
	if (p_element->p_element_type->pointer.array_elements_count.cb(
			global_path,
			0,
			p_element->pointer.actual_ptr,
			p_element->p_element_type->p_type,
			/*TODO: take the next params from the global context*/
			/*FIXME:*/p_element->pointer.actual_ptr,
			/*FIXME:*/p_element->p_element_type->p_type,
			&p_element->pointer.subrange0_count,
			p_element->p_element_type->pointer.array_elements_count.p_data) != 0) {
		msg_stderr("Pointer length callback failed for %s\n", global_path);
		return (-EFAULT);
	}
	/*init p_array_info*/
	p_element->pointer.p_array_info = metac_array_info_create_from_type(p_element->p_element_type->p_type, p_element->pointer.subrange0_count);
	if (p_element->pointer.p_array_info == NULL) {
		msg_stderr("metac_array_info_create_from_type failed for %s\n", global_path);
		return (-EFAULT);
	}
	/*create memory_block_reference and schedule processing of the memory_block_reference->p_memory_block*/
	p_element->pointer.memory_block_reference.reference_location.p_element = p_element;
	p_element->pointer.memory_block_reference.reference_location.p_element_hierarchy_member = p_element_hierarchy_member;

	/*try to find the existing block first*/
	{
		/*TODO: move to a separate function */
		struct object_root_container_memory_block_top * p_object_root_container_memory_block_top;
		metac_count_t elements_count = metac_array_info_get_element_count(p_element->pointer.p_array_info);
		metac_count_t byte_size = p_element_type->byte_size * elements_count;

		cds_list_for_each_entry(
			p_object_root_container_memory_block_top,
			&p_object_root_builder->container.memory_block_top_list,
			list) {
			struct memory_block * p_memory_block = p_object_root_container_memory_block_top->p_object_root_container_memory_block->p_memory_block;
			msg_stddbg("COMPARING WITH %p\n",
					p_object_root_container_memory_block->p_memory_block);

			if (	p_memory_block->ptr == p_element->pointer.actual_ptr &&
					p_memory_block->byte_size == byte_size &&
					p_memory_block->elements_count == elements_count) {
				msg_stddbg("found %p - skipping\n", p_object_root_container_memory_block_top);

				p_element->pointer.memory_block_reference.p_memory_block = p_memory_block;
				/*TODO: add info about back-reference p_object_root_container_memory_block->p_memory_block*/
				return 0;
			}
		}
		p_object_root_container_memory_block_top = object_root_container_memory_block_top_create(
				NULL,
				NULL,
				p_element->pointer.actual_ptr,
				byte_size,
				p_element_type,
				elements_count);
		if (p_object_root_container_memory_block_top == NULL) {
			msg_stderr("object_root_container_memory_block_top_create failed for %s\n", global_path);
			return (-EFAULT);
		}

		if (object_root_builder_schedule_object_root_container_memory_block_top(
				p_object_root_builder,
				global_path,
				p_object_root_container_memory_block_top) != 0) {
			msg_stderr("object_root_builder_schedule_object_root_container_memory_block_top failed for %s\n", global_path);
			object_root_container_memory_block_top_delete(&p_object_root_container_memory_block_top);
			return (-EFAULT);
		}
		/*TODO: add info about back-reference p_object_root_container_memory_block->p_memory_block*/

		/* add to the list - after this point container will take care of deleting this item */
		cds_list_add_tail(
			&p_object_root_container_memory_block_top->list,
			&p_object_root_builder->container.memory_block_top_list);
	}
	return 0;
}
static int object_root_builder_process_object_root_container_memory_block_top(
		struct object_root_builder *				p_object_root_builder,
		char *										global_path,
		struct object_root_container_memory_block_top *
													p_object_root_container_memory_block_top) {
	struct object_root_container_pointer * _pointer;
	struct memory_block_top_builder memory_block_top_builder;
	struct memory_block * p_memory_block = p_object_root_container_memory_block_top->p_object_root_container_memory_block->p_memory_block;

	assert(p_memory_block != NULL);
	msg_stderr("PROCESSING_TOP %s: ptr=%p byte_size=%d type=%s elements_count=%d \n",
			global_path,
			p_memory_block->ptr,
			(int)p_memory_block->byte_size,
			p_memory_block->p_elements?p_memory_block->p_elements[0].p_element_type->p_type->name:"<invalid>",
			(int)p_memory_block->elements_count);

	/* create a logic to put all information about pointers in order of appearance to p_object_root_container_memory_block */
	if (memory_block_top_builder_init(
			&memory_block_top_builder,
			p_object_root_container_memory_block_top) != 0) {
		msg_stderr("memory_block_top_builder_init failed for %s\n", global_path);
		return (-EFAULT);
	}
	/* process main p_object_root_container_memory_block */
	if (memory_block_top_builder_process_object_root_container_memory_block(
			&memory_block_top_builder,
			global_path,
			p_object_root_container_memory_block_top->p_object_root_container_memory_block) != 0) {
		msg_stderr("tasks object_root_container_memory_block_builder_process_object_root_container_memory_block finished with fail\n");
		memory_block_top_builder_clean(&memory_block_top_builder, 0);
		return (-EFAULT);
	}
	/*process all scheduled memory blocks*/
	if (traversing_engine_run(&memory_block_top_builder.memory_block_top_traversing_engine) != 0) {
		msg_stderr("tasks execution finished with fail\n");
		memory_block_top_builder_clean(&memory_block_top_builder, 0);
		return (-EFAULT);
	}
	/*process each pointer*/
	cds_list_for_each_entry(_pointer, &memory_block_top_builder.container.pointer_list, list) {
		if (object_root_builder_process_pointer(
				p_object_root_builder,
				_pointer->global_path,
				_pointer->p_element,
				_pointer->p_element_hierarchy_member) != 0) {
			msg_stderr("object_root_builder_process_pointer finished with fail\n");
			memory_block_top_builder_clean(&memory_block_top_builder, 0);
			return (-EFAULT);
		}
	}
	/*TODO:fill in */

	/*clean builder objects*/
	memory_block_top_builder_clean(&memory_block_top_builder, 1);
	return 0;
}
static int object_root_builder_process_object_root_container_memory_block_task_fn(
	struct traversing_engine * 						p_engine,
	struct traversing_engine_task * 				p_task,
	int 											error_flag) {
	struct object_root_builder * p_object_root_builder =
		cds_list_entry(p_engine, struct object_root_builder, object_root_traversing_engine);
	struct object_root_builder_memory_block_top_task * p_object_root_builder_memory_block_top_task =
		cds_list_entry(p_task, struct object_root_builder_memory_block_top_task, task);
	cds_list_add_tail(&p_task->list, &p_object_root_builder->object_root_executed_tasks);
	if (error_flag != 0)return (-EALREADY);

	return object_root_builder_process_object_root_container_memory_block_top(
			p_object_root_builder,
			p_object_root_builder_memory_block_top_task->global_path,
			p_object_root_builder_memory_block_top_task->p_object_root_container_memory_block_top);
}
static int object_root_builder_schedule_object_root_container_memory_block_top(
		struct object_root_builder *				p_object_root_builder,
		char *										global_path,
		struct object_root_container_memory_block_top *
													p_object_root_container_memory_block_top) {
	return (object_root_builder_schedule_memory_block_top_with_fn(
			p_object_root_builder,
			object_root_builder_process_object_root_container_memory_block_task_fn,
			global_path,
			p_object_root_container_memory_block_top) != NULL)?0:(-EFAULT);
}
static int object_root_container_init(
		struct object_root_container *				p_object_root_container) {
	CDS_INIT_LIST_HEAD(&p_object_root_container->memory_block_top_list);
	return 0;
}
static void object_root_container_clean(
		struct object_root_container *				p_object_root_container,
		metac_flag									keep_data) {
	struct object_root_container_memory_block_top * _memory_block_top, * __memory_block_top;
	cds_list_for_each_entry_safe(_memory_block_top, __memory_block_top, &p_object_root_container->memory_block_top_list, list) {
		cds_list_del(&_memory_block_top->list);
		object_root_container_memory_block_top_clean(_memory_block_top, keep_data);
		free(_memory_block_top);
	}
}
static int object_root_builder_init(
		struct object_root_builder *				p_object_root_builder,
		struct object_root *						p_object_root
		) {
	p_object_root_builder->p_object_root = p_object_root;

	object_root_container_init(&p_object_root_builder->container);
	CDS_INIT_LIST_HEAD(&p_object_root_builder->object_root_executed_tasks);
	traversing_engine_init(&p_object_root_builder->object_root_traversing_engine);
	return 0;
}
static void object_root_builder_clean(
		struct object_root_builder *				p_object_root_builder,
		metac_flag									keep_data) {
	struct traversing_engine_task * p_task, *_p_task;

	traversing_engine_clean(&p_object_root_builder->object_root_traversing_engine);
	cds_list_for_each_entry_safe(p_task, _p_task, &p_object_root_builder->object_root_executed_tasks, list) {
		struct object_root_builder_memory_block_top_task * p_object_root_builder_memory_block_top_task = cds_list_entry(
			p_task,
			struct object_root_builder_memory_block_top_task, task);
		cds_list_del(&p_task->list);
		object_root_builder_delete_memory_block_top_task(&p_object_root_builder_memory_block_top_task);
	}
	object_root_container_clean(&p_object_root_builder->container, keep_data);
	p_object_root_builder->p_object_root = NULL;
}
static int object_root_init(
		void *										ptr,
		struct element_type_top *					p_element_type_top,
		struct object_root *						p_object_root) {
	struct object_root_builder object_root_builder;
	object_root_builder_init(&object_root_builder, p_object_root/*, TODO:*/);

	p_object_root->ptr = ptr;

	/*init memory_block_for_pointer and ptr element */
	if (memory_block_init(
			&p_object_root->memory_block_for_pointer,
			NULL,
			NULL,
			&p_object_root->ptr,
			sizeof(p_object_root->ptr),
			p_element_type_top->p_element_type_for_pointer,
			1) != 0) {
		msg_stderr("memory_block_init failed\n");
		object_root_builder_clean(&object_root_builder, 0);
		return -EFAULT;
	}
	/*process memory_block to schedule all necessary memory blocks etc*/
	if (object_root_builder_process_pointer(
			&object_root_builder,
			"ptr",
			&p_object_root->memory_block_for_pointer.p_elements[0],
			NULL) != 0) {
		msg_stderr("object_root_builder_process_object_root_container_memory_block failed\n");
		object_root_builder_clean(&object_root_builder, 0);
		return -EFAULT;
	}
	/*process all scheduled memory blocks*/
	if (traversing_engine_run(&object_root_builder.object_root_traversing_engine) != 0) {
		msg_stderr("tasks execution finished with fail\n");
		object_root_builder_clean(&object_root_builder, 0);
		return (-EFAULT);
	}
	/*fill in */
	/*TBD*/

	/*clean builder objects*/
	object_root_builder_clean(&object_root_builder, 1);
	return 0;
}
struct metac_runtime_object * metac_runtime_object_create(
		void *										ptr,
		struct metac_precompiled_type *				p_metac_precompiled_type
		) {
	msg_stderr("PARSING OBJECT: ptr = %p\n", ptr);
	_create_(metac_runtime_object);
	if (object_root_init(
			ptr,
			&p_metac_precompiled_type->element_type_top,
			&p_metac_runtime_object->object_root) != 0) {
		msg_stderr("object_root_init failed\n");
		/*TODO: call metac_runtime_object_free*/
		return NULL;
	}
	return p_metac_runtime_object;
}
/*****************************************************************************/
#define DUMPPREFIX "    "
#define NEXT_DUMPPREFIX_ALLOC(next_prefix, prefix) \
		char *next_prefix = alloc_sptrinf("%s%s", prefix, DUMPPREFIX); \
		if (next_prefix == NULL) return
#define NEXT_DUMPPREFIX_FREE(next_prefix) \
		free(next_prefix);
/*****************************************************************************/
void metac_type_ptr_dump(
		FILE *										stream,
		char*										prefix,
		char *										field,
		struct metac_type *							p_metac_type);
/*****************************************************************************/
static char* metac_type_id_to_char(metac_type_id_t id) {
	switch(id){
		case DW_TAG_typedef:
			return "DW_TAG_typedef";
		case DW_TAG_const_type:
			return "DW_TAG_const_type";
		case DW_TAG_base_type:
			return "DW_TAG_base_type";
		case DW_TAG_pointer_type:
			return "DW_TAG_pointer_type";
		case DW_TAG_enumeration_type:
			return "DW_TAG_enumeration_type";
		case DW_TAG_subprogram:
			return "DW_TAG_subprogram";
		case DW_TAG_structure_type:
			return "DW_TAG_structure_type";
		case DW_TAG_union_type:
			return "DW_TAG_union_type";
		case DW_TAG_array_type:
			return "DW_TAG_array_type";
	}
	return "check dwarf.h";
}
static char * std_cb_ptr_to_char(void * ptr) {
	if (ptr == metac_array_elements_single) {
		return "metac_array_elements_single";
	}
	if (ptr == metac_array_elements_1d_with_null) {
		return "metac_array_elements_1d_with_null";
	}
	return "";
}
void metac_type_dump(
		FILE *										stream,
		char *										prefix,
		struct metac_type *							p_metac_type) {
	NEXT_DUMPPREFIX_ALLOC(next_prefix, prefix);
	fprintf(stream, "%smetac_type: {\n", prefix);
	fprintf(stream, "%sid: 0x%x(%s)\n", next_prefix, p_metac_type->id, metac_type_id_to_char(p_metac_type->id));
	fprintf(stream, "%sname: %s\n", next_prefix, p_metac_type->name);
	switch(p_metac_type->id) {
	case DW_TAG_pointer_type:
		metac_type_ptr_dump(stream, next_prefix, "pointer_type_info.type: ", p_metac_type->pointer_type_info.type);
		break;
	case DW_TAG_const_type:
		metac_type_ptr_dump(stream, next_prefix, "const_type_info.type: ", p_metac_type->const_type_info.type);
		break;
	case DW_TAG_array_type:
		metac_type_ptr_dump(stream, next_prefix, "array_type_info.type: ", p_metac_type->array_type_info.type);
		break;
	}
	fprintf(stream, "%s}\n", prefix);
	NEXT_DUMPPREFIX_FREE(next_prefix);
}
void metac_type_ptr_dump(
		FILE *										stream,
		char *										prefix,
		char *										field,
		struct metac_type *							p_metac_type) {
	NEXT_DUMPPREFIX_ALLOC(next_prefix, prefix);
	fprintf(stream, "%s%s%p {\n", prefix, field?field:"", p_metac_type);
	if (p_metac_type != NULL) {
		metac_type_dump(stream, next_prefix, p_metac_type);
	}
	fprintf(stream, "%s}\n", prefix);
	NEXT_DUMPPREFIX_FREE(next_prefix);
}
void discriminator_dump(
		FILE *										stream,
		char *										prefix,
		struct discriminator *						p_discriminator) {
	metac_count_t i;
	NEXT_DUMPPREFIX_ALLOC(next_prefix, prefix);
	fprintf(stream, "%spointer: %p: {\n", prefix, p_discriminator);
	fprintf(stream, "%sid: %d\n", next_prefix, p_discriminator->id);
	fprintf(stream, "%sprecondition.expected_discriminator_value: %d\n", next_prefix, p_discriminator->precondition.expected_discriminator_value);
	fprintf(stream, "%sprecondition.p_discriminator: %p\n", next_prefix, p_discriminator->precondition.p_discriminator);
	fprintf(stream, "%sprecondition.cb: %p(%s)\n", next_prefix, p_discriminator->cb, std_cb_ptr_to_char(p_discriminator->cb));
	fprintf(stream, "%sprecondition.p_data: %p\n", next_prefix, p_discriminator->p_data);
	fprintf(stream, "%s}\n", prefix);
	NEXT_DUMPPREFIX_FREE(next_prefix);
}
void element_type_hierarchy_top_discriminators_dump(
		FILE *										stream,
		char *										prefix,
		metac_count_t								discriminators_count,
		struct discriminator **						pp_discriminators) {
	metac_count_t i;
	NEXT_DUMPPREFIX_ALLOC(next_prefix, prefix);
	fprintf(stream, "%spp_discriminators: [\n", prefix);
	for (i = 0; i < discriminators_count; ++i){
		discriminator_dump(stream, next_prefix, pp_discriminators[i]);
	}
	fprintf(stream, "%s]\n", prefix);
	NEXT_DUMPPREFIX_FREE(next_prefix);
}
void element_type_array_dump(
		FILE *										stream,
		char *										prefix,
		struct element_type_array *					p_element_type_array) {
	NEXT_DUMPPREFIX_ALLOC(next_prefix, prefix);
	fprintf(stream, "%sarray: {\n", prefix);
	fprintf(stream, "%sp_element_type: %p\n", next_prefix, p_element_type_array->p_element_type);
	fprintf(stream, "%sarray_elements_count.cb: %p(%s)\n", next_prefix, p_element_type_array->array_elements_count.cb, std_cb_ptr_to_char(p_element_type_array->array_elements_count.cb));
	fprintf(stream, "%sarray_elements_count.p_data: %p\n", next_prefix, p_element_type_array->array_elements_count.p_data);
	fprintf(stream, "%s}\n", prefix);
	NEXT_DUMPPREFIX_FREE(next_prefix);
}
void generic_cast_type_dump(
		FILE *										stream,
		char *										prefix,
		struct generic_cast_type *					generic_cast_type) {
	NEXT_DUMPPREFIX_ALLOC(next_prefix, prefix);
	fprintf(stream, "%sgeneric_cast_type: {\n", prefix);
	fprintf(stream, "%sp_element_type: %p\n", next_prefix, generic_cast_type->p_element_type);
	metac_type_ptr_dump(stream, next_prefix, "p_type: ", generic_cast_type->p_type);
	fprintf(stream, "%s}\n", prefix);
	NEXT_DUMPPREFIX_FREE(next_prefix);
}
void generic_cast_types_dump(
		FILE *										stream,
		char *										prefix,
		metac_count_t								types_count,
		struct generic_cast_type *					p_generic_cast_type) {
	metac_count_t i;
	NEXT_DUMPPREFIX_ALLOC(next_prefix, prefix);
	fprintf(stream, "%sp_types: [\n", prefix);
	for (i = 0; i < types_count; ++i){
		generic_cast_type_dump(stream, next_prefix, &p_generic_cast_type[i]);
	}
	fprintf(stream, "%s]\n", prefix);
	NEXT_DUMPPREFIX_FREE(next_prefix);
}
void element_type_pointer_dump(
		FILE *										stream,
		char *										prefix,
		struct element_type_pointer *				p_element_type_pointer) {
	NEXT_DUMPPREFIX_ALLOC(next_prefix, prefix);
	fprintf(stream, "%selement_type_pointer: {\n", prefix);
	fprintf(stream, "%sp_element_type: %p\n", next_prefix, p_element_type_pointer->p_element_type);
	fprintf(stream, "%sgeneric_cast.cb: %p(%s)\n", next_prefix, p_element_type_pointer->generic_cast.cb, std_cb_ptr_to_char(p_element_type_pointer->generic_cast.cb));
	fprintf(stream, "%sgeneric_cast.p_data: %p\n", next_prefix, p_element_type_pointer->generic_cast.p_data);
	fprintf(stream, "%sgeneric_cast.types_count: %d\n", next_prefix, p_element_type_pointer->generic_cast.types_count);
	if (p_element_type_pointer->generic_cast.p_types) {
		generic_cast_types_dump(stream, next_prefix, p_element_type_pointer->generic_cast.types_count, p_element_type_pointer->generic_cast.p_types);
	}
	fprintf(stream, "%sarray_elements_count.cb: %p(%s)\n", next_prefix, p_element_type_pointer->array_elements_count.cb, std_cb_ptr_to_char(p_element_type_pointer->array_elements_count.cb));
	fprintf(stream, "%sarray_elements_count.p_data: %p\n", next_prefix, p_element_type_pointer->array_elements_count.p_data);
	fprintf(stream, "%s}\n", prefix);
	NEXT_DUMPPREFIX_FREE(next_prefix);
}
void element_type_hierarchy_member_dump(
		FILE *										stream,
		char *										prefix,
		struct element_type_hierarchy_member *		p_member) {
	metac_count_t i;
	NEXT_DUMPPREFIX_ALLOC(next_prefix, prefix);
	fprintf(stream, "%selement_type_hierarchy_member: {\n", prefix);
	fprintf(stream, "%sid: %d\n", next_prefix, p_member->id);
	if (p_member->precondition.p_discriminator != NULL) {
		fprintf(stream, "%sprecondition.p_discriminator: %p\n", next_prefix, p_member->precondition.p_discriminator);
		fprintf(stream, "%sprecondition.expected_discriminator_value: %d\n", next_prefix, p_member->precondition.expected_discriminator_value);
	}
	fprintf(stream, "%spath_within_hierarchy: %s\n", next_prefix, p_member->path_within_hierarchy);
	fprintf(stream, "%smember_id: %d\n", next_prefix, p_member->member_id);
	fprintf(stream, "%sbyte_size: %d\n", next_prefix, p_member->byte_size);
	fprintf(stream, "%soffset: %d\n", next_prefix, p_member->offset);
	if (p_member->p_type) metac_type_ptr_dump(stream, next_prefix, "p_type: ", p_member->p_type);
	if (p_member->p_actual_type) metac_type_ptr_dump(stream, next_prefix, "p_actual_type: ", p_member->p_actual_type);
	if (p_member->p_actual_type) {
		switch(p_member->p_actual_type->id){
		case DW_TAG_array_type:
			element_type_array_dump(stream, next_prefix, &p_member->array);
			break;
		case DW_TAG_pointer_type:
			element_type_pointer_dump(stream, next_prefix, &p_member->pointer);
			break;
		case DW_TAG_structure_type:
		case DW_TAG_union_type:
			fprintf(stream, "%shierarchy: <skipped>\n", next_prefix);
			break;
		}
	}
	fprintf(stream, "%s}\n", prefix);
	NEXT_DUMPPREFIX_FREE(next_prefix);
}
void element_type_hierarchy_top_members_dump(
		FILE *										stream,
		char *										prefix,
		metac_count_t								members_count,
		struct element_type_hierarchy_member **		pp_members) {
	metac_count_t i;
	NEXT_DUMPPREFIX_ALLOC(next_prefix, prefix);
	fprintf(stream, "%spp_members: [\n", prefix);
	for (i = 0; i < members_count; ++i){
		element_type_hierarchy_member_dump(stream, next_prefix, pp_members[i]);
	}
	fprintf(stream, "%s]\n", prefix);
	NEXT_DUMPPREFIX_FREE(next_prefix);
}
void element_type_hierarchy_top_dump(
		FILE *										stream,
		char *										prefix,
		struct element_type_hierarchy_top *			p_element_type_hierarchy_top) {
	metac_count_t i;
	NEXT_DUMPPREFIX_ALLOC(next_prefix, prefix);
	fprintf(stream, "%shierarchy_top: {\n", prefix);
	fprintf(stream, "%sdiscriminators_count: %d\n", next_prefix, p_element_type_hierarchy_top->discriminators_count);
	element_type_hierarchy_top_discriminators_dump(stream, next_prefix, p_element_type_hierarchy_top->discriminators_count, p_element_type_hierarchy_top->pp_discriminators);
	fprintf(stream, "%smembers_count: %d\n", next_prefix, p_element_type_hierarchy_top->members_count);
	element_type_hierarchy_top_members_dump(stream, next_prefix, p_element_type_hierarchy_top->members_count, p_element_type_hierarchy_top->pp_members);
	fprintf(stream, "%s}\n", prefix);
	NEXT_DUMPPREFIX_FREE(next_prefix);
}
void element_type_dump(
		FILE *										stream,
		char *										prefix,
		struct element_type *						p_element_type) {
	NEXT_DUMPPREFIX_ALLOC(next_prefix, prefix);
	fprintf(stream, "%selement_type: {\n", prefix);
	if (p_element_type->p_type) {
		metac_type_ptr_dump(stream, next_prefix, "p_type: ", p_element_type->p_type);
	}
	if (p_element_type->p_actual_type) {
		metac_type_ptr_dump(stream, next_prefix, "p_actual_type: ", p_element_type->p_actual_type);
	}
	fprintf(stream, "%sbyte_size: %d\n", next_prefix, p_element_type->byte_size);
	fprintf(stream, "%sis_potentially_flexible: %d\n", next_prefix, p_element_type->is_potentially_flexible);
	if (p_element_type->p_actual_type) {
		switch(p_element_type->p_actual_type->id){
		case DW_TAG_array_type:
			element_type_array_dump(stream, next_prefix, &p_element_type->array);
			break;
		case DW_TAG_pointer_type:
			element_type_pointer_dump(stream, next_prefix, &p_element_type->pointer);
			break;
		case DW_TAG_structure_type:
		case DW_TAG_union_type:
			element_type_hierarchy_top_dump(stream, next_prefix, &p_element_type->hierarchy_top);
			break;
		}
	}
	fprintf(stream, "%s}\n", prefix);
	NEXT_DUMPPREFIX_FREE(next_prefix);
}
void element_type_ptr_dump(
		FILE *										stream,
		char *										prefix,
		char *										field,
		struct element_type *						p_element_type) {
	NEXT_DUMPPREFIX_ALLOC(next_prefix, prefix);
	fprintf(stream, "%s%s%p {\n", prefix, field?field:"pointer: ", p_element_type);
	element_type_dump(stream, next_prefix, p_element_type);
	fprintf(stream, "%s}\n", prefix);
	NEXT_DUMPPREFIX_FREE(next_prefix);
}
void _element_types_array_members_dump(
		FILE *										stream,
		char *										prefix,
		metac_count_t								element_types_count,
		struct element_type **						pp_element_types) {
	metac_count_t i;
	NEXT_DUMPPREFIX_ALLOC(next_prefix, prefix);
	fprintf(stream, "%spp_element_types: [\n", prefix);
	for (i = 0; i < element_types_count; ++i){
		element_type_ptr_dump(stream, next_prefix, NULL, pp_element_types[i]);
	}
	fprintf(stream, "%s]\n", prefix);
	NEXT_DUMPPREFIX_FREE(next_prefix);
}
void element_type_top_dump(
		FILE *										stream,
		char *										prefix,
		struct element_type_top*					p_element_type_top) {
	NEXT_DUMPPREFIX_ALLOC(next_prefix, prefix);
	fprintf(stream, "%selement_type_top: {\n", prefix);
	if (p_element_type_top->p_pointer_type) {
		metac_type_ptr_dump(stream, next_prefix, "p_pointer_type: ", p_element_type_top->p_pointer_type);
	}
	if (p_element_type_top->p_element_type_for_pointer) {
		element_type_ptr_dump(stream, next_prefix, "p_element_type_for_pointer: ", p_element_type_top->p_element_type_for_pointer);
	}
	fprintf(stream, "%selement_types_count: %d\n", next_prefix, p_element_type_top->element_types_count);
	_element_types_array_members_dump(stream, next_prefix,  p_element_type_top->element_types_count, p_element_type_top->pp_element_types);
	fprintf(stream, "%s}\n", prefix);
	NEXT_DUMPPREFIX_FREE(next_prefix);
}
void metac_dump_precompiled_type(metac_precompiled_type_t * precompiled_type) {
	element_type_top_dump(stderr, "", &precompiled_type->element_type_top);
}
