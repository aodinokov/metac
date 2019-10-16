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
struct precompile_type_container_element_type {
	struct cds_list_head							list;

	struct element_type *							p_element_type;
};
struct precompile_type_container {
	struct cds_list_head							pointers_element_type_list;
	struct cds_list_head							arrays_element_type_list;
};
struct precompile_type_builder {
	struct metac_type *								p_root_type;
	metac_type_annotation_t *						p_override_annotations;

	struct precompile_type_container				container;
	
	struct traversing_engine						element_type_traversing_engine;
	struct cds_list_head							element_type_executed_tasks;
};
struct precompile_type_builder_element_type_task {
	struct traversing_engine_task					task;
	char * 											global_path;	/*local copy - keep track of it*/
	struct metac_type *								p_type;
	struct element_type *							p_from_element_type;
	struct element_type_hierarchy_member *			p_from_element_type_hierarchy_member;
	struct metac_type *								p_from_type;
};
/*****************************************************************************/
/*scheduling prototypes*/
int element_type_hierarchy_top_builder_schedule_element_type_hierarchy_member(
		struct element_type_hierarchy_top_builder * p_element_type_hierarchy_top_builder,
		char *										global_path,
		struct element_type_hierarchy_member *		p_element_type_hierarchy_member,
		struct element_type_hierarchy *				p_parent_hierarchy);
int precompile_type_builder_schedule_element_type_from_array(
		struct precompile_type_builder *			p_precompile_type_builder,
		char * 										global_path,
		struct metac_type *							p_type,
		struct element_type *						p_from_element_type,
		struct element_type_hierarchy_member *		p_from_element_type_hierarchy_member);
int precompile_type_builder_schedule_element_type_from_pointer(
		struct precompile_type_builder *			p_precompile_type_builder,
		char * 										global_path,
		struct metac_type *							p_type,
		struct element_type *						p_from_element_type,
		struct element_type_hierarchy_member *		p_from_element_type_hierarchy_member,
		struct metac_type *							p_from_type);
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
int delete_discriminator(
		struct discriminator **						pp_discriminator) {
	_delete_(discriminator);
	return 0;
}
struct discriminator * create_discriminator(
		struct metac_type *							p_root_type,
		char * 										global_path,
		metac_type_annotation_t *					p_override_annotations,
		struct discriminator *						p_previous_discriminator,
		metac_discriminator_value_t					previous_expected_discriminator_value) {
	const metac_type_annotation_t * p_annotation;
	p_annotation = metac_type_annotation(p_root_type, global_path, p_override_annotations);

	if (p_annotation == NULL) {
		msg_stderr("Wasn't able to find annotation for %s\n", global_path);
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

	p_discriminator->discriminator_cb = p_annotation->value->discriminator.cb;
	p_discriminator->discriminator_cb_context = p_annotation->value->discriminator.data;

	return p_discriminator;
}
int element_type_hierarchy_top_container_init_discriminator(
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
void element_type_hierarchy_top_container_clean_discriminator(
		struct element_type_hierarchy_top_container_discriminator *
													p_element_type_hierarchy_top_container_discriminator,
		metac_flag									keep_data
		) {
	if (keep_data == 0) {
		delete_discriminator(&p_element_type_hierarchy_top_container_discriminator->p_discriminator);
	}
	p_element_type_hierarchy_top_container_discriminator->p_discriminator = NULL;
}
static struct element_type_hierarchy_top_container_discriminator * element_type_hierarchy_top_container_create_discriminator(
		struct discriminator *						p_discriminator) {
	int res;
	_create_(element_type_hierarchy_top_container_discriminator);
	msg_stddbg("element_type_hierarchy_top_container_discriminator_create\n");
	if (element_type_hierarchy_top_container_init_discriminator(
			p_element_type_hierarchy_top_container_discriminator,
			p_discriminator) != 0) {
		free(p_element_type_hierarchy_top_container_discriminator);
		return NULL;
	}
	msg_stddbg("element_type_hierarchy_top_container_discriminator_create exit\n");
	return p_element_type_hierarchy_top_container_discriminator;
}
int element_type_hierarchy_top_builder_add_discriminator (
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
int init_element_type_array(
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
			p_element_type_array->array_elements_count.data = p_annotation->value->array_elements_count.data;
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
int clean_element_type_array(
		struct element_type_array *					p_element_type_array) {
	return 0;
}
static struct generic_cast_type * create_generic_cast_type(
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
int init_element_type_pointer(
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
		p_element_type_pointer->generic_cast.data = p_annotation->value->generic_cast.data;
		p_element_type_pointer->generic_cast.types = create_generic_cast_type(
				p_annotation->value->generic_cast.types,
				&p_element_type_pointer->generic_cast.types_count);

		p_element_type_pointer->array_elements_count.cb = p_annotation->value->array_elements_count.cb;
		p_element_type_pointer->array_elements_count.data = p_annotation->value->array_elements_count.data;

		if (p_annotation->value->discriminator.cb != NULL) {
			msg_stderr("Annotations for pointer with global path %s defines discriminator that won't be used\n", global_path);
		}
	}
	return 0;
}
int clean_element_type_pointer(
		struct element_type_pointer *				p_element_type_pointer) {
	if (p_element_type_pointer->generic_cast.types != NULL) {
		free(p_element_type_pointer->generic_cast.types);
		p_element_type_pointer->generic_cast.types = NULL;
	}
	return 0;
}
int init_element_type_hierarchy(
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
int clean_element_type_hierarchy(
		struct element_type_hierarchy *				p_element_type_hierarchy) {
	if (p_element_type_hierarchy->members != NULL) {
		free(p_element_type_hierarchy->members);
		p_element_type_hierarchy->members = NULL;
	}
	return 0;
}
int delete_element_type_hierarchy_member(
		struct element_type_hierarchy_member **		pp_element_type_hierarchy_member) {
	_delete_start_(element_type_hierarchy_member);
	if ((*pp_element_type_hierarchy_member)->actual_type)
		switch((*pp_element_type_hierarchy_member)->actual_type->id){
		case DW_TAG_structure_type:
		case DW_TAG_union_type:
			clean_element_type_hierarchy(&(*pp_element_type_hierarchy_member)->hierarchy);
			break;
		case DW_TAG_array_type:
			clean_element_type_array(&(*pp_element_type_hierarchy_member)->array);
			break;
		case DW_TAG_pointer_type:
			clean_element_type_pointer(&(*pp_element_type_hierarchy_member)->pointer);
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
struct element_type_hierarchy_member * create_element_type_hierarchy_member(
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
	p_element_type_hierarchy_member->precondition.p_discriminator = p_discriminator;
	p_element_type_hierarchy_member->precondition.expected_discriminator_value = expected_discriminator_value;
	p_element_type_hierarchy_member->p_hierarchy = p_hierarchy;

	p_element_type_hierarchy_member->offset = p_member_info->data_member_location;
	p_parent_element_type_hierarchy_member = element_type_hierarchy_member_get_parent_element_hierarchy_member(p_element_type_hierarchy_member);
	if (p_parent_element_type_hierarchy_member != NULL) {
		p_element_type_hierarchy_member->offset += p_parent_element_type_hierarchy_member->offset;
	}

	p_element_type_hierarchy_member->type = p_member_info->type;
	if (p_element_type_hierarchy_member->type != NULL) {
		p_element_type_hierarchy_member->actual_type = metac_type_actual_type(p_member_info->type);
	}

	p_element_type_hierarchy_member->path_within_hierarchy = strdup(hirarchy_path);
	if (p_element_type_hierarchy_member->path_within_hierarchy == NULL) {
		msg_stderr("wasn't able to build hirarchy_path for %s\n", hirarchy_path);
		delete_element_type_hierarchy_member(&p_element_type_hierarchy_member);
		return NULL;
	}
	/*msg_stddbg("local path: %s\n", p_element_type_hierarchy_member->path_within_hierarchy);*/

	if (p_element_type_hierarchy_member->actual_type != NULL) {
		if (p_element_type_hierarchy_member->actual_type->id == DW_TAG_base_type) {
			p_element_type_hierarchy_member->base_type.p_bit_offset = p_member_info->p_bit_offset;
			p_element_type_hierarchy_member->base_type.p_bit_size = p_member_info->p_bit_size;
		}else{
			int res;
			if (p_member_info->p_bit_offset != NULL || p_member_info->p_bit_size != NULL) {
				msg_stderr("incorrect type %d for p_bit_offset and p_bit_size\n", p_element_type_hierarchy_member->actual_type->id);
				assert(0);
			}

			switch(p_element_type_hierarchy_member->actual_type->id) {
			case DW_TAG_structure_type:
			case DW_TAG_union_type:
				res = init_element_type_hierarchy(p_root_type, global_path, p_override_annotations, p_element_type_hierarchy_member->actual_type, &p_element_type_hierarchy_member->hierarchy, p_hierarchy);
				break;
			case DW_TAG_array_type:
				res = init_element_type_array(p_root_type, global_path, p_override_annotations, p_element_type_hierarchy_member->actual_type, &p_element_type_hierarchy_member->array);
				break;
			case DW_TAG_pointer_type:
				res = init_element_type_pointer(p_root_type, global_path, p_override_annotations, p_element_type_hierarchy_member->actual_type, &p_element_type_hierarchy_member->pointer);
				break;
			}

			if (res != 0) {
				msg_stddbg("initialization failed\n");
				delete_element_type_hierarchy_member(&p_element_type_hierarchy_member);
				return NULL;
			}
		}
	}
	return p_element_type_hierarchy_member;
}
int element_type_hierarchy_top_container_init_element_type_hierarchy_member(
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
void element_type_hierarchy_top_container_clean_element_type_hierarchy_member(
		struct element_type_hierarchy_top_container_element_type_hierarchy_member *
													p_element_type_hierarchy_top_container_element_type_hierarchy_member,
		metac_flag									keep_data
		) {
	if (keep_data == 0) {
		delete_element_type_hierarchy_member(&p_element_type_hierarchy_top_container_element_type_hierarchy_member->p_element_type_hierarchy_member);
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
int element_type_hierarchy_top_builder_add_element_type_hierarchy_member (
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
	if (p_element_type_hierarchy_member->actual_type &&
		(p_element_type_hierarchy_member->actual_type->id == DW_TAG_structure_type ||p_element_type_hierarchy_member->actual_type->id == DW_TAG_union_type)){
	}
	/* add to list */
	cds_list_add_tail(
		&p_element_type_hierarchy_top_container_element_type_hierarchy_member->list,
		&p_element_type_hierarchy_top_builder->container.element_type_hierarchy_member_type_list);
	return 0;
}
int element_type_hierarchy_top_builder_process_structure(
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

		p_hierarchy->members[indx] = create_element_type_hierarchy_member(
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
			msg_stddbg("global_path: %s failed because create_element_type_hierarchy_member failed\n", global_path);
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
			delete_element_type_hierarchy_member(&p_hierarchy->members[indx]);
			return res;
		}
		free(member_hirarchy_path);
		free(member_global_path);
	}
	return 0;
}
int element_type_hierarchy_top_builder_process_union(
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

	p_discriminator = create_discriminator(
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
		delete_discriminator(&p_discriminator);
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

		p_hierarchy->members[indx] = create_element_type_hierarchy_member(
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
			msg_stddbg("global_path: %s failed because create_element_type_hierarchy_member failed\n", global_path);
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
			delete_element_type_hierarchy_member(&p_hierarchy->members[indx]);
			free(member_hirarchy_path);
			free(member_global_path);
			return res;
		}
		free(member_hirarchy_path);
		free(member_global_path);
	}
	return 0;
}
int element_type_hierarchy_top_builder_process_hierarchy(
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
		clean_element_type_hierarchy(p_hierarchy);
	}
	return res;
}
int element_type_hierarchy_top_builder_process_element_type_hierarchy_member(
		struct element_type_hierarchy_top_builder * p_element_type_hierarchy_top_builder,
		char *										global_path,
		struct element_type_hierarchy_member *		p_element_type_hierarchy_member,
		struct element_type_hierarchy *				p_parent_hierarchy) {

	if (p_element_type_hierarchy_member->actual_type && (
			p_element_type_hierarchy_member->actual_type->id == DW_TAG_structure_type||
			p_element_type_hierarchy_member->actual_type->id == DW_TAG_union_type)) {
		if (element_type_hierarchy_top_builder_process_hierarchy(
				p_element_type_hierarchy_top_builder,
				global_path,
				p_element_type_hierarchy_member->path_within_hierarchy,
				p_element_type_hierarchy_member->precondition.p_discriminator,
				p_element_type_hierarchy_member->precondition.expected_discriminator_value,
				p_element_type_hierarchy_member->actual_type,
				&p_element_type_hierarchy_member->hierarchy,
				p_parent_hierarchy) != 0) {
			msg_stderr("failed to process global_path for %s\n", global_path);
			return (-EFAULT);
		}
	}
	return 0;
}
int element_type_hierarchy_top_builder_delete_hierarchy_member_task(
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
int element_type_hierarchy_top_builder_schedule_element_type_hierarchy_member(
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
int element_type_hierarchy_top_container_init(
		struct element_type_hierarchy_top_container *
													p_element_type_hierarchy_top_container) {
	CDS_INIT_LIST_HEAD(&p_element_type_hierarchy_top_container->discriminator_type_list);
	CDS_INIT_LIST_HEAD(&p_element_type_hierarchy_top_container->element_type_hierarchy_member_type_list);
	return 0;
}
void element_type_hierarchy_top_container_clean(
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
int element_type_hierarchy_top_builder_init(
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
void element_type_hierarchy_top_builder_clean(
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
static int finalize_element_type_hierarchy_top(
		struct element_type_hierarchy_top_builder * p_element_type_hierarchy_top_builder,
		struct element_type_hierarchy_top *			p_element_type_hierarchy_top) {
	metac_count_t i;
	struct element_type_hierarchy_top_container_discriminator * _discriminator, * __discriminator;
	struct element_type_hierarchy_top_container_element_type_hierarchy_member * _member, * __member;

	p_element_type_hierarchy_top->members_count = 0;
	p_element_type_hierarchy_top->members = NULL;
	p_element_type_hierarchy_top->discriminators_count = 0;
	p_element_type_hierarchy_top->discriminators = NULL;

	/*get arrays lengths */
	cds_list_for_each_entry_safe(_member, __member, &p_element_type_hierarchy_top_builder->container.element_type_hierarchy_member_type_list, list) {
		++p_element_type_hierarchy_top->members_count;
	}
	cds_list_for_each_entry_safe(_discriminator, __discriminator, &p_element_type_hierarchy_top_builder->container.discriminator_type_list, list) {
		++p_element_type_hierarchy_top->discriminators_count;
	}

	if (p_element_type_hierarchy_top->members_count > 0) {
		p_element_type_hierarchy_top->members = (struct element_type_hierarchy_member **)calloc(
				p_element_type_hierarchy_top->members_count, sizeof(*p_element_type_hierarchy_top->members));
		if (p_element_type_hierarchy_top->members == NULL) {
			msg_stderr("can't allocate memory for members\n");
			return (-ENOMEM);
		}

		i = 0;
		cds_list_for_each_entry_safe(_member, __member, &p_element_type_hierarchy_top_builder->container.element_type_hierarchy_member_type_list, list) {
			p_element_type_hierarchy_top->members[i] = _member->p_element_type_hierarchy_member;
			p_element_type_hierarchy_top->members[i]->id = i;
			msg_stddbg("added member %s\n", _member->p_element_type_hierarchy_member->path_within_hierarchy);
			++i;
		}
	}
	if (p_element_type_hierarchy_top->discriminators_count > 0) {
		p_element_type_hierarchy_top->discriminators = (struct discriminator **)calloc(
				p_element_type_hierarchy_top->discriminators_count, sizeof(*p_element_type_hierarchy_top->discriminators));
		if (p_element_type_hierarchy_top->discriminators == NULL) {
			msg_stderr("can't allocate memory for discriminators\n");
			free(p_element_type_hierarchy_top->members);
			return (-ENOMEM);
		}

		i = 0;
		cds_list_for_each_entry_safe(_discriminator, __discriminator, &p_element_type_hierarchy_top_builder->container.discriminator_type_list, list) {
			p_element_type_hierarchy_top->discriminators[i] = _discriminator->p_discriminator;
			p_element_type_hierarchy_top->discriminators[i]->id = i;
			++i;
		}
	}
	return 0;
}
int clean_element_type_hierarchy_top(
		struct element_type_hierarchy_top *			p_element_type_hierarchy_top) {
	metac_count_t i;
	if (p_element_type_hierarchy_top->members != NULL) {
		/*to clean members*/
		for (i = 0 ; i < p_element_type_hierarchy_top->members_count; ++i) {
			delete_element_type_hierarchy_member(&p_element_type_hierarchy_top->members[i]);
		}
		free(p_element_type_hierarchy_top->members);
		p_element_type_hierarchy_top->members = NULL;
	}
	if (p_element_type_hierarchy_top->discriminators != NULL) {
		/*to clean discriminators*/
		for (i = 0 ; i < p_element_type_hierarchy_top->discriminators_count; ++i) {
			delete_discriminator(&p_element_type_hierarchy_top->discriminators[i]);
		}
		free(p_element_type_hierarchy_top->discriminators);
		p_element_type_hierarchy_top->discriminators = NULL;
	}
	clean_element_type_hierarchy(&p_element_type_hierarchy_top->hierarchy);
	return 0;
}
int init_element_type_hierarchy_top(
		struct metac_type *							p_root_type,
		char * 										global_path,
		metac_type_annotation_t *					p_override_annotations,
		struct metac_type *							p_actual_type,
		struct element_type_hierarchy_top *			p_element_type_hierarchy_top) {
	struct element_type_hierarchy_top_builder element_type_hierarchy_top_builder;

	element_type_hierarchy_top_builder_init(&element_type_hierarchy_top_builder, p_root_type, p_override_annotations);

	if (init_element_type_hierarchy(
			p_root_type,
			global_path,
			p_override_annotations,
			p_actual_type,
			&p_element_type_hierarchy_top->hierarchy,
			NULL) != 0) {
		msg_stderr("init_element_type_hierarchy failed\n");
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
		clean_element_type_hierarchy(&p_element_type_hierarchy_top->hierarchy);
		return (-EFAULT);
	}
	if (traversing_engine_run(&element_type_hierarchy_top_builder.hierarchy_traversing_engine) != 0) {
		msg_stddbg("tasks execution finished with fail\n");
		element_type_hierarchy_top_builder_clean(&element_type_hierarchy_top_builder, 0);
		clean_element_type_hierarchy(&p_element_type_hierarchy_top->hierarchy);
		return (-EFAULT);
	}
	/*fill in */
	if (finalize_element_type_hierarchy_top(
			&element_type_hierarchy_top_builder,
			p_element_type_hierarchy_top) != 0) {
		msg_stddbg("object finalization failed\n");
		element_type_hierarchy_top_builder_clean(&element_type_hierarchy_top_builder, 0);
		clean_element_type_hierarchy(&p_element_type_hierarchy_top->hierarchy);
		return (-EFAULT);
	}
	/*clean builder objects*/
	element_type_hierarchy_top_builder_clean(&element_type_hierarchy_top_builder, 1);
#if 0
	/*debug full clean function*/
	clean_element_type_hierarchy_top(p_element_type_hierarchy_top);
#endif
	return 0;
}
/*****************************************************************************/
static metac_flag element_type_is_potentially_flexible(
		struct element_type *						p_element_type) {
	switch(p_element_type->actual_type->id) {
	case DW_TAG_array_type:
		return p_element_type->actual_type->array_type_info.is_flexible;
	case DW_TAG_structure_type:
	case DW_TAG_union_type: {
		metac_count_t i;
		for (i = 0; i < p_element_type->hierarchy_top.members_count; ++i) {
			if (p_element_type->hierarchy_top.members[i]->actual_type != NULL &&
				p_element_type->hierarchy_top.members[i]->actual_type->id == DW_TAG_array_type &&
				p_element_type->hierarchy_top.members[i]->actual_type->array_type_info.is_flexible) {
				if (p_element_type->hierarchy_top.members[i]->offset == p_element_type->byte_size)
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
int delete_element_type(
		struct element_type **						pp_element_type) {
	_delete_start_(element_type);
	if ((*pp_element_type)->actual_type != NULL) {
		switch((*pp_element_type)->actual_type->id) {
		case DW_TAG_structure_type:
		case DW_TAG_union_type:
			clean_element_type_hierarchy_top(&(*pp_element_type)->hierarchy_top);
			break;
		case DW_TAG_array_type:
			clean_element_type_array(&(*pp_element_type)->array);
			break;
		case DW_TAG_pointer_type:
			clean_element_type_pointer(&(*pp_element_type)->pointer);
			break;
		}
	}
	_delete_finish(element_type);
	return 0;
}
struct element_type * create_element_type(
		struct metac_type *							p_root_type,
		char * 										global_path,
		metac_type_annotation_t *					p_override_annotations,
		struct metac_type *							p_type,
		struct element_type *						p_from_element_type,
		struct element_type_hierarchy_member *		p_from_element_type_hierarchy_member,
		struct metac_type *							p_from_type) {
	int res = 0;
	_create_(element_type);
	p_element_type->type = p_type;
	p_element_type->p_from_element_type = p_from_element_type;
	p_element_type->p_from_element_type_hierarchy_member = p_from_element_type_hierarchy_member;
	p_element_type->p_from_type = p_from_type;
	p_element_type->actual_type = metac_type_actual_type(p_type);
	p_element_type->byte_size = metac_type_byte_size(p_type);
	
	switch(p_element_type->actual_type->id) {
	case DW_TAG_structure_type:
	case DW_TAG_union_type:
		res = init_element_type_hierarchy_top(p_root_type, global_path, p_override_annotations, p_element_type->actual_type, &p_element_type->hierarchy_top);
		break;
	case DW_TAG_array_type:
		res = init_element_type_array(p_root_type, global_path, p_override_annotations, p_element_type->actual_type, &p_element_type->array);
		break;
	case DW_TAG_pointer_type:
		res = init_element_type_pointer(p_root_type, global_path, p_override_annotations, p_element_type->actual_type, &p_element_type->pointer);
		break;
	}
	if (res != 0) {
		msg_stddbg("initialization failed\n");
		delete_element_type(&p_element_type);
		return NULL;
	}
	p_element_type->is_potentially_flexible = element_type_is_potentially_flexible(p_element_type);
	return p_element_type;
}
int precompile_type_container_init_element_type(
		struct precompile_type_container_element_type *
													p_precompile_type_container_element_type,
	struct element_type *							p_element_type) {
	if (p_element_type == NULL) {
		msg_stderr("Can't init precomple_type_container_element_type with NULL p_element_type \n");
		return (-EINVAL);
	}
	p_precompile_type_container_element_type->p_element_type = p_element_type;
	return 0;
}
void precompile_type_container_clean_element_type(
		struct precompile_type_container_element_type *
													p_precompile_type_container_element_type,
		metac_flag									keep_data
		) {
	if (keep_data == 0) {
		delete_element_type(&p_precompile_type_container_element_type->p_element_type);
	}
	p_precompile_type_container_element_type->p_element_type = NULL;
}
int precompile_type_container_init(
		struct precompile_type_container *			p_precompile_type_container) {
	CDS_INIT_LIST_HEAD(&p_precompile_type_container->pointers_element_type_list);
	CDS_INIT_LIST_HEAD(&p_precompile_type_container->arrays_element_type_list);
	return 0;
}
void precompile_type_container_clean(
		struct precompile_type_container *			p_precompile_type_container,
		metac_flag									keep_data) {
	struct precompile_type_container_element_type * _element_type, * __element_type;
	cds_list_for_each_entry_safe(_element_type, __element_type, &p_precompile_type_container->pointers_element_type_list, list) {
		cds_list_del(&_element_type->list);
		precompile_type_container_clean_element_type(_element_type, keep_data);
		free(_element_type);
	}
	cds_list_for_each_entry_safe(_element_type, __element_type, &p_precompile_type_container->arrays_element_type_list, list) {
		cds_list_del(&_element_type->list);
		precompile_type_container_clean_element_type(_element_type, keep_data);
		free(_element_type);
	}
}
static int precompile_type_builder_process_element_type_array(
		struct precompile_type_builder *			p_precompile_type_builder,
		char * 										global_path,
		struct element_type *						p_from_element_type,
		struct element_type_hierarchy_member *		p_from_element_type_hierarchy_member) {
	int i = 0;
	int res = (-EINVAL);
	struct metac_type * p_actual_type;

	if (p_from_element_type_hierarchy_member != NULL) {
		p_actual_type = p_from_element_type_hierarchy_member->actual_type;
	} else {
		p_actual_type = p_from_element_type->actual_type;
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
		res = precompile_type_builder_schedule_element_type_from_array(
				p_precompile_type_builder,
				target_global_path,
				p_actual_type->array_type_info.type,
				p_from_element_type,
				p_from_element_type_hierarchy_member);
		free(target_global_path);
		if (res != 0){
			msg_stderr("Can't precompile_type_builder_schedule_element_type_from_array for %s\n", global_path);
			return (-EFAULT);
		}
	}

	return 0;
}
static int precompile_type_builder_process_element_type_pointer(
		struct precompile_type_builder *			p_precompile_type_builder,
		char * 										global_path,
		struct element_type *						p_from_element_type,
		struct element_type_hierarchy_member *		p_from_element_type_hierarchy_member) {
	int i = 0;
	int res = (-EINVAL);
	struct metac_type * p_actual_type;
	struct element_type_pointer *p_pointer;

	if (p_from_element_type_hierarchy_member != NULL) {
		p_actual_type = p_from_element_type_hierarchy_member->actual_type;
		p_pointer = &p_from_element_type_hierarchy_member->pointer;
	} else {
		p_actual_type = p_from_element_type->actual_type;
		p_pointer = &p_from_element_type->pointer;
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
		res = precompile_type_builder_schedule_element_type_from_pointer(
				p_precompile_type_builder,
				target_global_path,
				p_actual_type->pointer_type_info.type,
				p_from_element_type,
				p_from_element_type_hierarchy_member,
				NULL);
		free(target_global_path);
		if (res != 0){
			msg_stderr("Can't precompile_type_builder_schedule_element_type_from_pointer for %s\n", global_path);
			return (-EFAULT);
		}
	}

	for (i = 0; i < p_pointer->generic_cast.types_count; ++i) {
		char * target_global_path = alloc_sptrinf(
				"generic_cast<*%s>(%s)[]",
				p_pointer->generic_cast.types[i].p_type->name,
				global_path);
		if (target_global_path == NULL){
			msg_stderr("Can't build target_global_path for %s generic_cast %d\n", global_path, i);
			return (-EFAULT);
		}
		res = precompile_type_builder_schedule_element_type_from_pointer(
				p_precompile_type_builder,
				target_global_path,
				p_actual_type->pointer_type_info.type,
				p_from_element_type,
				p_from_element_type_hierarchy_member,
				p_pointer->generic_cast.types[i].p_type);
		free(target_global_path);
		if (res != 0){
			msg_stderr("Can't precompile_type_builder_schedule_element_type_from_pointer for %s generic_cast %d\n", global_path, i);
			return (-EFAULT);
		}
	}

	if (res != 0) {
		msg_stddbg("Didn't schedule anything for %s\n", global_path);
	}
	return 0;
}
static int precompile_type_builder_process_element_type_hierarchy(
		struct precompile_type_builder *			p_precompile_type_builder,
		char * 										global_path,
		struct element_type *						p_from_element_type) {
	int i = 0;
	int res = (-EINVAL);

	assert(	p_from_element_type->actual_type->id == DW_TAG_structure_type ||
			p_from_element_type->actual_type->id == DW_TAG_union_type);

	for (i = 0; i < p_from_element_type->hierarchy_top.members_count; ++i) {
		assert(p_from_element_type->hierarchy_top.members);
		if (	p_from_element_type->hierarchy_top.members[i]->actual_type && (
				p_from_element_type->hierarchy_top.members[i]->actual_type->id == DW_TAG_array_type ||
				p_from_element_type->hierarchy_top.members[i]->actual_type->id == DW_TAG_pointer_type)) {
			char * target_global_path = alloc_sptrinf(
					"%s.%s",
					global_path,
					p_from_element_type->hierarchy_top.members[i]->path_within_hierarchy);
			if (target_global_path == NULL){
				msg_stderr("Can't build target_global_path for %s member %d\n", global_path, i);
				return (-EFAULT);
			}
			switch(p_from_element_type->hierarchy_top.members[i]->actual_type->id){
			case DW_TAG_array_type:
				res = precompile_type_builder_process_element_type_array(
						p_precompile_type_builder,
						target_global_path,
						p_from_element_type,
						p_from_element_type->hierarchy_top.members[i]);
				if (res != 0){
					msg_stderr("Can't precompile_type_builder_schedule_element_type_from_array for %s member %d\n", global_path, i);
					free(target_global_path);
					return (-EFAULT);
				}
				break;
			case DW_TAG_pointer_type:
				res = precompile_type_builder_process_element_type_pointer(
						p_precompile_type_builder,
						target_global_path,
						p_from_element_type,
						p_from_element_type->hierarchy_top.members[i]);
				if (res != 0){
					msg_stderr("Can't precompile_type_builder_process_element_type_pointer for %s member %d\n", global_path, i);
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
static struct precompile_type_container_element_type * precompile_type_builder_create_element_type(
		struct precompile_type_builder *			p_precompile_type_builder,
		char * 										global_path,
		struct metac_type *							p_type,
		struct element_type *						p_from_element_type,
		struct element_type_hierarchy_member *		p_from_element_type_hierarchy_member,
		struct metac_type *							p_from_type) {
	int res;
	_create_(precompile_type_container_element_type);
	res = precompile_type_container_init_element_type(
		p_precompile_type_container_element_type,
		create_element_type(
			p_precompile_type_builder->p_root_type,
			global_path,
			p_precompile_type_builder->p_override_annotations,
			p_type,
			p_from_element_type,
			p_from_element_type_hierarchy_member,
			p_from_type));
	if (res != 0) {
			msg_stddbg("precompile_type_container_element_type_init returned error - deleting\n");
			free(p_precompile_type_container_element_type);
			return NULL;		
	}

	switch(p_precompile_type_container_element_type->p_element_type->actual_type->id) {
	case DW_TAG_array_type:
		res = precompile_type_builder_process_element_type_array(
				p_precompile_type_builder,
				global_path,
				p_precompile_type_container_element_type->p_element_type,
				NULL);
		break;
	case DW_TAG_pointer_type:
		res = precompile_type_builder_process_element_type_pointer(
				p_precompile_type_builder,
				global_path,
				p_precompile_type_container_element_type->p_element_type,
				NULL);
		break;
	case DW_TAG_structure_type:
	case DW_TAG_union_type:
		res = precompile_type_builder_process_element_type_hierarchy(
				p_precompile_type_builder,
				global_path,
				p_precompile_type_container_element_type->p_element_type);
		break;
	}
	return p_precompile_type_container_element_type;
}
int precompile_type_builder_add_element_type_from_array(
		struct precompile_type_builder *			p_precompile_type_builder,
		char * 										global_path,
		struct metac_type *							p_type,
		struct element_type *						p_from_element_type,
		struct element_type_hierarchy_member *		p_from_element_type_hierarchy_member) {
	struct precompile_type_container_element_type * p_precompile_type_container_element_type;
	/* create */
	p_precompile_type_container_element_type = precompile_type_builder_create_element_type(
			p_precompile_type_builder,
			global_path,
			p_type,
			p_from_element_type,
			p_from_element_type_hierarchy_member,
			NULL);
	if (p_precompile_type_container_element_type == NULL) {
		msg_stddbg("wasn't able to create p_precompile_type_container_element_type\n");
		return (-EFAULT);
	}
	/* add to pointers list */
	cds_list_add_tail(
		&p_precompile_type_container_element_type->list,
		&p_precompile_type_builder->container.arrays_element_type_list);
	return 0;
}
int precompile_type_builder_add_element_type_from_pointer(
		struct precompile_type_builder *			p_precompile_type_builder,
		char * 										global_path,
		struct metac_type *							p_type,
		struct element_type *						p_from_element_type,
		struct element_type_hierarchy_member *		p_from_element_type_hierarchy_member,
		struct metac_type *							p_from_type) {
	struct precompile_type_container_element_type * p_precompile_type_container_element_type;
	msg_stderr("LOOKING %p %p %p\n",
			p_from_element_type,
			p_from_element_type_hierarchy_member,
			p_from_type);

	/* try to find this type */
	cds_list_for_each_entry(
		p_precompile_type_container_element_type,
		&p_precompile_type_builder->container.pointers_element_type_list,
		list) {
		if (p_precompile_type_container_element_type->p_element_type->type == p_type) {
			msg_stderr("MATCHES %p %p %p\n",
					p_precompile_type_container_element_type->p_element_type->p_from_element_type,
					p_precompile_type_container_element_type->p_element_type->p_from_element_type_hierarchy_member,
					p_precompile_type_container_element_type->p_element_type->p_from_type);
		}
	}
	cds_list_for_each_entry(
		p_precompile_type_container_element_type,
		&p_precompile_type_builder->container.pointers_element_type_list,
		list) {
		if (p_precompile_type_container_element_type->p_element_type->type == p_type &&
			//p_precompile_type_container_element_type->p_element_type->p_from_element_type == p_from_element_type && /*???*/
			p_precompile_type_container_element_type->p_element_type->p_from_element_type_hierarchy_member == p_from_element_type_hierarchy_member) {
			msg_stddbg("found %p - skipping %p %p\n", p_precompile_type_container_element_type);

			msg_stderr("FOUND %p %p %p\n",
					p_precompile_type_container_element_type->p_element_type->p_from_element_type,
					p_precompile_type_container_element_type->p_element_type->p_from_element_type_hierarchy_member,
					p_precompile_type_container_element_type->p_element_type->p_from_type);

			return 0;
		}
	}
	/* create */
	p_precompile_type_container_element_type = precompile_type_builder_create_element_type(
			p_precompile_type_builder,
			global_path,
			p_type,
			p_from_element_type,
			p_from_element_type_hierarchy_member,
			p_from_type);
	if (p_precompile_type_container_element_type == NULL) {
		msg_stderr("wasn't able to create p_precompile_type_container_element_type\n");
		return (-EFAULT);
	}
	/* add to pointers list */
	cds_list_add_tail(
		&p_precompile_type_container_element_type->list,
		&p_precompile_type_builder->container.pointers_element_type_list);
	return 0;
}
static int precompile_type_builder_process_element_type_from_array_task_fn(
	struct traversing_engine * 						p_engine,
	struct traversing_engine_task * 				p_task,
	int 											error_flag) {
	struct precompile_type_builder * p_precompile_type_builder = 
		cds_list_entry(p_engine, struct precompile_type_builder, element_type_traversing_engine);
	struct precompile_type_builder_element_type_task * p_precompile_type_builder_element_type_task = 
		cds_list_entry(p_task, struct precompile_type_builder_element_type_task, task);
	cds_list_add_tail(&p_task->list, &p_precompile_type_builder->element_type_executed_tasks);
	if (error_flag != 0)return (-EALREADY);	
	return precompile_type_builder_add_element_type_from_array(
		p_precompile_type_builder,
		p_precompile_type_builder_element_type_task->global_path,
		p_precompile_type_builder_element_type_task->p_type,
		p_precompile_type_builder_element_type_task->p_from_element_type,
		p_precompile_type_builder_element_type_task->p_from_element_type_hierarchy_member);
}
static int precompile_type_builder_process_element_type_from_pointer_task_fn(
	struct traversing_engine * 						p_engine,
	struct traversing_engine_task * 				p_task,
	int 											error_flag) {
	struct precompile_type_builder * p_precompile_type_builder = 
		cds_list_entry(p_engine, struct precompile_type_builder, element_type_traversing_engine);
	struct precompile_type_builder_element_type_task * p_precompile_type_builder_element_type_task = 
		cds_list_entry(p_task, struct precompile_type_builder_element_type_task, task);
	cds_list_add_tail(&p_task->list, &p_precompile_type_builder->element_type_executed_tasks);
	if (error_flag != 0)return (-EALREADY);	
	return precompile_type_builder_add_element_type_from_pointer(
		p_precompile_type_builder,
		p_precompile_type_builder_element_type_task->global_path,
		p_precompile_type_builder_element_type_task->p_type,
		p_precompile_type_builder_element_type_task->p_from_element_type,
		p_precompile_type_builder_element_type_task->p_from_element_type_hierarchy_member,
		p_precompile_type_builder_element_type_task->p_from_type);
}
int precompile_type_builder_delete_element_type_task(
		struct precompile_type_builder_element_type_task **
													pp_precompile_type_builder_element_type_task) {
	_delete_start_(precompile_type_builder_element_type_task);
	if ((*pp_precompile_type_builder_element_type_task)->global_path) {
		free((*pp_precompile_type_builder_element_type_task)->global_path);
		(*pp_precompile_type_builder_element_type_task)->global_path = NULL;
	}
	_delete_finish(precompile_type_builder_element_type_task);
	return 0;
}
static struct precompile_type_builder_element_type_task * precompile_type_builder_schedule_element_type_with_fn(
		struct precompile_type_builder *			p_precompile_type_builder,
		traversing_engine_task_fn_t 				fn,
		char * 										global_path,
		struct metac_type *							p_type,
		struct element_type *						p_from_element_type,
		struct element_type_hierarchy_member *		p_from_element_type_hierarchy_member,
		struct metac_type *							p_from_type) {
	_create_(precompile_type_builder_element_type_task);
	
	p_precompile_type_builder_element_type_task->task.fn = fn;
	p_precompile_type_builder_element_type_task->global_path = strdup(global_path);
	p_precompile_type_builder_element_type_task->p_type = p_type;
	p_precompile_type_builder_element_type_task->p_from_element_type = p_from_element_type;
	p_precompile_type_builder_element_type_task->p_from_element_type_hierarchy_member = p_from_element_type_hierarchy_member;
	p_precompile_type_builder_element_type_task->p_from_type = p_from_type;
	if (p_precompile_type_builder_element_type_task->global_path == NULL) {
		msg_stderr("wasn't able to duplicate global_path\n");
		precompile_type_builder_delete_element_type_task(&p_precompile_type_builder_element_type_task);
		return NULL;
	}
	if (add_traversing_task_to_tail(
		&p_precompile_type_builder->element_type_traversing_engine,
		&p_precompile_type_builder_element_type_task->task) != 0) {
		msg_stderr("wasn't able to schedule task\n");
		precompile_type_builder_delete_element_type_task(&p_precompile_type_builder_element_type_task);
		return NULL;
	}
	return p_precompile_type_builder_element_type_task;
}
int precompile_type_builder_schedule_element_type_from_array(
		struct precompile_type_builder *			p_precompile_type_builder,
		char * 										global_path,
		struct metac_type *							p_type,
		struct element_type *						p_from_element_type,
		struct element_type_hierarchy_member *		p_from_element_type_hierarchy_member) {
	return (precompile_type_builder_schedule_element_type_with_fn(
		p_precompile_type_builder,
		precompile_type_builder_process_element_type_from_array_task_fn,
		global_path,
		p_type,
		p_from_element_type,
		p_from_element_type_hierarchy_member,
		NULL)!=NULL)?0:(-EFAULT);
}
int precompile_type_builder_schedule_element_type_from_pointer(
		struct precompile_type_builder *			p_precompile_type_builder,
		char * 										global_path,
		struct metac_type *							p_type,
		struct element_type *						p_from_element_type,
		struct element_type_hierarchy_member *		p_from_element_type_hierarchy_member,
		struct metac_type *							p_from_type) {
	msg_stderr("SHEDULE %p %p %p\n",
			p_from_element_type,
			p_from_element_type_hierarchy_member,
			p_from_type);
	return (precompile_type_builder_schedule_element_type_with_fn(
		p_precompile_type_builder,
		precompile_type_builder_process_element_type_from_pointer_task_fn,
		global_path,
		p_type,
		p_from_element_type,
		p_from_element_type_hierarchy_member,
		p_from_type)!=NULL)?0:(-EFAULT);
}
int precompile_type_builder_init(
		struct precompile_type_builder *			p_precompile_type_builder,
		struct metac_type *							p_root_type,
		metac_type_annotation_t *					p_override_annotations) {
	p_precompile_type_builder->p_root_type = p_root_type;
	p_precompile_type_builder->p_override_annotations = p_override_annotations;
	precompile_type_container_init(&p_precompile_type_builder->container);
	CDS_INIT_LIST_HEAD(&p_precompile_type_builder->element_type_executed_tasks);
	traversing_engine_init(&p_precompile_type_builder->element_type_traversing_engine);
	return 0;
}
void precompile_type_builder_clean(
		struct precompile_type_builder *			p_precompile_type_builder,
		metac_flag									keep_data) {
	struct traversing_engine_task * p_task, *_p_task;

	traversing_engine_clean(&p_precompile_type_builder->element_type_traversing_engine);
	cds_list_for_each_entry_safe(p_task, _p_task, &p_precompile_type_builder->element_type_executed_tasks, list) {
		struct precompile_type_builder_element_type_task * p_precompile_type_builder_element_type_task = cds_list_entry(
			p_task, 
			struct precompile_type_builder_element_type_task, task);
		cds_list_del(&p_task->list);
		precompile_type_builder_delete_element_type_task(&p_precompile_type_builder_element_type_task);
	}
	precompile_type_container_clean(&p_precompile_type_builder->container, keep_data);
	p_precompile_type_builder->p_root_type = NULL;
}
metac_precompiled_type_t * metac_precompile_type(
		struct metac_type *							p_root_type,
		metac_type_annotation_t *					p_override_annotations) {
	struct precompile_type_builder precompile_type_builder;
	precompile_type_builder_init(&precompile_type_builder, p_root_type, p_override_annotations);
	if (precompile_type_builder_schedule_element_type_from_pointer(&precompile_type_builder, "ptr", p_root_type, NULL, NULL, NULL) != 0) {
		msg_stderr("wasn't able to schedule the first task\n");
		precompile_type_builder_clean(&precompile_type_builder, 0);
		return NULL;
	}
	if (traversing_engine_run(&precompile_type_builder.element_type_traversing_engine) != 0) {
		msg_stderr("tasks execution finished with fail\n");
		precompile_type_builder_clean(&precompile_type_builder, 0);
		return NULL;
	}
//	precompile_type_builder_clean(&precompile_type_builder, 1);
	precompile_type_builder_clean(&precompile_type_builder, 0);
	return NULL;
}
int  metac_free_precompiled_type(metac_precompiled_type_t ** precompiled_type) {

}
