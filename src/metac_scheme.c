/*
 * metac_scheme.c
 *
 *  Created on: Feb 18, 2020
 *      Author: mralex
 */

#include <assert.h>			/* assert */
#include <errno.h>			/* ENOMEM etc */
#include <metac/metac_scheme.h>
#include <stdlib.h>			/* calloc, free, NULL, qsort... */
#include <string.h>			/* strdup */
#include <urcu/list.h>		/* struct cds_list_head etc */

//#define METAC_DEBUG_ENABLE

#include "metac_debug.h"	/* msg_stderr, ...*/
#include "metac_oop.h"		/*_create, _delete, ...*/
#include "scheduler.h"


#define alloc_sptrinf(...) ({\
	int len = snprintf(NULL, 0, ##__VA_ARGS__); \
	char *str = calloc(len + 1, sizeof(*str)); \
	if (str != NULL)snprintf(str, len + 1, ##__VA_ARGS__); \
	str; \
})
#define build_member_path(path, member_name) ({ \
	char * member_path = NULL; \
	if ((path) != NULL && strcmp((path), "") != 0 ) { \
		member_path = alloc_sptrinf("%s.%s", (path), (member_name)); \
	} else { \
		member_path = alloc_sptrinf("%s", (member_name)); \
	} \
	member_path; \
})


/*****************************************************************************/

static int metac_scheme_clean(
		struct metac_scheme *						p_value_scheme);

/*****************************************************************************/

static int metac_scheme_delete(
		struct metac_scheme **						pp_metac_scheme) {

	_delete_start_(metac_scheme);
	metac_scheme_clean(*pp_metac_scheme);
	_delete_finish(metac_scheme);
	return 0;
}


static inline struct metac_scheme * metac_unknown_object_get_metac_scheme(
		struct metac_unknown_object *				p_metac_unknown_object) {
	return cds_list_entry(
			p_metac_unknown_object,
			struct metac_scheme,
			refcounter_object.unknown_object);
}

static int _metac_scheme_unknown_object_delete(
		struct metac_unknown_object *				p_metac_unknown_object) {

	struct metac_scheme * p_metac_scheme;

	if (p_metac_unknown_object == NULL) {
		msg_stderr("p_metac_unknown_object is NULL\n");
		return -(EINVAL);
	}

	p_metac_scheme = metac_unknown_object_get_metac_scheme(p_metac_unknown_object);
	if (p_metac_scheme == NULL) {
		msg_stderr("p_metac_scheme is NULL\n");
		return -(EINVAL);
	}

	return metac_scheme_delete(&p_metac_scheme);
}

static metac_refcounter_object_ops_t _metac_scheme_refcounter_object_ops = {
		.unknown_object_ops = {
				.metac_unknown_object_delete = 		_metac_scheme_unknown_object_delete,
		},
};

/*****************************************************************************/

struct metac_scheme * metac_scheme_get(
		struct metac_scheme *						p_metac_scheme) {

	if (p_metac_scheme != NULL &&
		metac_refcounter_object_get(&p_metac_scheme->refcounter_object) != NULL) {
		return p_metac_scheme;
	}

	return NULL;
}

int metac_scheme_put(
		struct metac_scheme **						pp_metac_scheme) {

	if (pp_metac_scheme != NULL &&
		(*pp_metac_scheme) != NULL &&
		metac_refcounter_object_put(&(*pp_metac_scheme)->refcounter_object) == 0) {

		*pp_metac_scheme = NULL;
		return 0;
	}

	return -(EFAULT);
}

/*****************************************************************************/
static int generic_cast_type_delete(
		struct generic_cast_type **					pp_generic_cast_type,
		metac_count_t								types_count) {

	metac_count_t i = 0;

	if (pp_generic_cast_type == NULL) {
		return -EINVAL;
	}

	if ((*pp_generic_cast_type) == NULL) {
		return -EALREADY;
	}

	for (i = 0; i < types_count; ++i) {
		if ((*pp_generic_cast_type)[i].p_type != NULL) {

			metac_type_put(&(*pp_generic_cast_type)[i].p_type);
		}
	}

	free((*pp_generic_cast_type));
	(*pp_generic_cast_type) = NULL;

	return 0;
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

		p_generic_cast_type[i].p_type = metac_type_get(types[i]);
		if (p_generic_cast_type[i].p_type == NULL) {

			msg_stderr("metac_type_get failed\n");
			generic_cast_type_delete(&p_generic_cast_type, *p_types_count);

			return NULL;
		}
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

static int scheme_with_array_init(
		struct scheme_with_array *					p_scheme_with_array,
		struct metac_type *							p_root_type,
		char * 										global_path,
		char *										value_path,
		metac_type_annotation_t *					p_override_annotations,
		struct metac_type *							p_actual_type) {

	const metac_type_annotation_t * p_annotation;
	struct metac_type * p_target_actual_type;

	assert(p_actual_type->id == DW_TAG_array_type);

	p_annotation = metac_type_annotation(p_root_type, global_path, p_override_annotations);

	/*keep global_path as annotation_key*/
	p_scheme_with_array->annotation_key = strdup(global_path);
	if (p_scheme_with_array->annotation_key == NULL) {
		msg_stderr("Can't keep annotation_key for %s\n", global_path);
		return -ENOMEM;
	}

	/* defaults for char[] */
	if (p_actual_type->array_type_info.is_flexible != 0) {

		p_target_actual_type = metac_type_get_actual_type(p_actual_type->array_type_info.type);
		if (p_target_actual_type != NULL &&
			p_target_actual_type->id == DW_TAG_base_type && (
				p_target_actual_type->base_type_info.encoding == DW_ATE_signed_char ||
				p_target_actual_type->base_type_info.encoding == DW_ATE_unsigned_char) &&
				p_target_actual_type->name != NULL && (
				strcmp(p_target_actual_type->name, "char") == 0 ||
				strcmp(p_target_actual_type->name, "unsigned char") == 0)){

			p_scheme_with_array->array_elements_count.cb = metac_array_elements_1d_with_null;
		}
	}

	if (p_annotation != NULL) {
		if (p_actual_type->array_type_info.is_flexible != 0) {

			p_scheme_with_array->array_elements_count.cb = p_annotation->value->array_elements_count.cb;
			p_scheme_with_array->array_elements_count.p_data = p_annotation->value->array_elements_count.data;
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

static void scheme_with_array_clean(
		struct scheme_with_array *					p_scheme_with_array) {

	if (p_scheme_with_array->annotation_key != NULL) {

		free(p_scheme_with_array->annotation_key);
		p_scheme_with_array->annotation_key = NULL;
	}

	if (p_scheme_with_array->p_child_value_scheme != NULL) {

		metac_scheme_put(&p_scheme_with_array->p_child_value_scheme);
	}

}

static int scheme_with_pointer_init(
		struct scheme_with_pointer *				p_scheme_with_pointer,
		struct metac_type *							p_root_type,
		char * 										global_path,
		char *										value_path,
		metac_type_annotation_t *					p_override_annotations,
		struct metac_type *							p_actual_type) {

	const metac_type_annotation_t * p_annotation;
	struct metac_type * p_target_actual_type;

	assert(p_actual_type->id == DW_TAG_pointer_type);

	p_annotation = metac_type_annotation(p_root_type, global_path, p_override_annotations);

	/*keep global_path as annotation_key*/
	p_scheme_with_pointer->annotation_key = strdup(global_path);
	if (p_scheme_with_pointer->annotation_key == NULL) {
		msg_stderr("Can't keep annotation_key for %s\n", global_path);
		return -ENOMEM;
	}

	/*defaults is metac_array_elements_single */
	p_scheme_with_pointer->array_elements_count.cb = metac_array_elements_single;

	/*default for char* is metac_array_elements_1d_with_null */
	if (p_actual_type->pointer_type_info.type != NULL) {

		p_target_actual_type = metac_type_get_actual_type(p_actual_type->pointer_type_info.type);
		if (p_target_actual_type != NULL &&
			p_target_actual_type->id == DW_TAG_base_type && (
				p_target_actual_type->base_type_info.encoding == DW_ATE_signed_char ||
				p_target_actual_type->base_type_info.encoding == DW_ATE_unsigned_char) &&
				p_target_actual_type->name != NULL && (
				strcmp(p_target_actual_type->name, "char") == 0 ||
				strcmp(p_target_actual_type->name, "unsigned char") == 0)) {

			p_scheme_with_pointer->array_elements_count.cb = metac_array_elements_1d_with_null;
		}
	}

	if (p_annotation != NULL) {
		p_scheme_with_pointer->generic_cast.cb = p_annotation->value->generic_cast.cb;
		p_scheme_with_pointer->generic_cast.p_data = p_annotation->value->generic_cast.data;
		p_scheme_with_pointer->generic_cast.p_types = generic_cast_type_create(
				p_annotation->value->generic_cast.types,
				&p_scheme_with_pointer->generic_cast.types_count);

		p_scheme_with_pointer->array_elements_count.cb = p_annotation->value->array_elements_count.cb;
		p_scheme_with_pointer->array_elements_count.p_data = p_annotation->value->array_elements_count.data;

		if (p_annotation->value->discriminator.cb != NULL) {
			msg_stderr("Annotations for pointer with global path %s defines discriminator that won't be used\n", global_path);
		}
	}
	return 0;
}

static void scheme_with_pointer_clean(
		struct scheme_with_pointer *				p_scheme_with_pointer) {

	if (p_scheme_with_pointer->annotation_key != NULL) {

		free(p_scheme_with_pointer->annotation_key);
		p_scheme_with_pointer->annotation_key = NULL;
	}
	if (p_scheme_with_pointer->generic_cast.p_types != NULL) {
		int i;

		for (i = 0; i < p_scheme_with_pointer->generic_cast.types_count; ++i) {

			if (p_scheme_with_pointer->generic_cast.p_types[i].p_child_value_scheme != NULL) {

				metac_scheme_put(&p_scheme_with_pointer->generic_cast.p_types[i].p_child_value_scheme);
			}
		}

		generic_cast_type_delete(
				&p_scheme_with_pointer->generic_cast.p_types,
				p_scheme_with_pointer->generic_cast.types_count);
	}

	if (p_scheme_with_pointer->p_default_child_value_scheme != NULL) {

		metac_scheme_put(&p_scheme_with_pointer->p_default_child_value_scheme);
	}
}

static struct metac_scheme * scheme_with_hierarchy_get_value_scheme(
		struct scheme_with_hierarchy *				p_scheme_with_hierarchy) {
	if (p_scheme_with_hierarchy == NULL) {
		return NULL;
	}
	return cds_list_entry(p_scheme_with_hierarchy, struct metac_scheme, hierarchy);
}

static int scheme_with_hierarchy_init(
		struct scheme_with_hierarchy *				p_scheme_with_hierarchy,
		struct metac_type *							p_root_type,
		char * 										global_path,
		char *										value_path,
		metac_type_annotation_t *					p_override_annotations,
		struct metac_type *							p_actual_type) {

	const metac_type_annotation_t * p_annotation;

	assert(p_actual_type->id == DW_TAG_structure_type || p_actual_type->id == DW_TAG_union_type);

	p_annotation = metac_type_annotation(p_root_type, global_path, p_override_annotations);

	p_scheme_with_hierarchy->members_count = p_actual_type->structure_type_info.members_count;
	if (p_scheme_with_hierarchy->members_count > 0) {

		p_scheme_with_hierarchy->members =
				(struct metac_scheme **)calloc(
						p_scheme_with_hierarchy->members_count,
						sizeof(*p_scheme_with_hierarchy->members));

		if (p_scheme_with_hierarchy->members == NULL) {

			msg_stderr("Can't allocate memory for hierarchy\n");
			p_scheme_with_hierarchy->members_count = 0;

			return (-EFAULT);
		}
	}

	if (p_actual_type->id == DW_TAG_union_type) {
		struct metac_scheme * p_value_scheme = scheme_with_hierarchy_get_value_scheme(p_scheme_with_hierarchy);

		if (discriminator_init(
				&p_scheme_with_hierarchy->union_discriminator,
				p_root_type,
				global_path,
				p_override_annotations,
				(metac_scheme_is_hierarchy_member_scheme(p_value_scheme) == 1)?p_value_scheme->hierarchy_member.precondition.p_discriminator:NULL,
				(metac_scheme_is_hierarchy_member_scheme(p_value_scheme) == 1)?p_value_scheme->hierarchy_member.precondition.expected_discriminator_value:0) != 0) {

			msg_stddbg("global_path: %s failed to init discriminator\n", global_path);
			return (-EFAULT);
		}

	}

	return 0;
}

static void scheme_with_hierarchy_clean(
		struct scheme_with_hierarchy *				p_scheme_with_hierarchy) {
	struct metac_scheme * p_value_scheme = scheme_with_hierarchy_get_value_scheme(p_scheme_with_hierarchy);

	if (p_scheme_with_hierarchy == NULL ||
		p_value_scheme == NULL) {

		msg_stderr("invalid argument\n");
		return;
	}

	if (p_value_scheme->p_actual_type->id == DW_TAG_union_type) {

		discriminator_clean(&p_scheme_with_hierarchy->union_discriminator);
	}

	if (p_scheme_with_hierarchy->members != NULL) {
		int i;

		for (i = 0; i < p_scheme_with_hierarchy->members_count; ++i ) {

			if (p_scheme_with_hierarchy->members[i] != NULL){
				metac_scheme_put(&p_scheme_with_hierarchy->members[i]);
			}
		}

		free(p_scheme_with_hierarchy->members);
		p_scheme_with_hierarchy->members = NULL;
	}
}

static int metac_scheme_clean_as_hierarchy_member_scheme(
		struct metac_scheme *						p_metac_scheme) {

	if (metac_scheme_is_hierarchy_member_scheme(p_metac_scheme) != 1) {
		msg_stderr("invalid argument: expected hierarchy_member\n");
		return -(EINVAL);
	}

	if (p_metac_scheme->hierarchy_member.path_within_hierarchy) {
		free(p_metac_scheme->hierarchy_member.path_within_hierarchy);
		p_metac_scheme->hierarchy_member.path_within_hierarchy = NULL;
	}

	if (p_metac_scheme->path_within_value != NULL) {
		free(p_metac_scheme->path_within_value);
		p_metac_scheme->path_within_value = NULL;
	}

	if (p_metac_scheme->p_actual_type)
		switch(p_metac_scheme->p_actual_type->id){
		case DW_TAG_structure_type:
		case DW_TAG_union_type:
			scheme_with_hierarchy_clean(&p_metac_scheme->hierarchy);
			break;
		case DW_TAG_array_type:
			scheme_with_array_clean(&p_metac_scheme->array);
			break;
		case DW_TAG_pointer_type:
			scheme_with_pointer_clean(&p_metac_scheme->pointer);
			break;
	}
	return 0;
}

static int metac_scheme_init_as_hierarchy_member_scheme(
		struct metac_scheme *						p_metac_scheme,
		struct metac_type *							p_root_type,
		char *										global_path,
		char *										value_path,
		char *										hirarchy_path,
		metac_type_annotation_t *					p_override_annotations,
		struct scheme_with_hierarchy *				p_current_hierarchy,
		struct discriminator *						p_discriminator,
		metac_discriminator_value_t					expected_discriminator_value,
		metac_count_t								member_id,
		struct metac_type_member_info *				p_member_info) {
	struct metac_scheme * p_parent_metac_scheme;

	if (p_metac_scheme == NULL ||
		p_current_hierarchy == NULL) {
		msg_stderr("invalid argument for %s\n", hirarchy_path);
		return -(EINVAL);
	}

	p_metac_scheme->hierarchy_member.member_id = member_id;
	p_metac_scheme->hierarchy_member.p_member_info = p_member_info;

	p_metac_scheme->hierarchy_member.precondition.p_discriminator = p_discriminator;
	p_metac_scheme->hierarchy_member.precondition.expected_discriminator_value = expected_discriminator_value;
	p_metac_scheme->p_current_hierarchy = p_current_hierarchy;

	p_metac_scheme->hierarchy_member.offset = p_member_info->data_member_location;

	p_parent_metac_scheme = metac_hierarchy_member_scheme_get_parent_scheme(p_metac_scheme);
	if (p_parent_metac_scheme != NULL) {

		assert(metac_scheme_is_hierachy_scheme(p_parent_metac_scheme));

		p_metac_scheme->hierarchy_member.offset += p_parent_metac_scheme->hierarchy_member.offset;
	}

	if (p_member_info->type != NULL) {

		p_metac_scheme->p_type = metac_type_get(p_member_info->type);
		if (p_metac_scheme->p_type == NULL) {

			msg_stderr("metac_type_get failed for %s\n", value_path);
			return -(EFAULT);
		}

		p_metac_scheme->p_actual_type = metac_type_get_actual_type(p_member_info->type);
		p_metac_scheme->byte_size = metac_type_byte_size(p_member_info->type);
	}

	p_metac_scheme->path_within_value = strdup(value_path);
	if (p_metac_scheme->path_within_value == NULL) {
		msg_stderr("wasn't able to build value_path for %s\n", value_path);
		metac_scheme_clean_as_hierarchy_member_scheme(p_metac_scheme);
		return -(ENOMEM);
	}

	p_metac_scheme->hierarchy_member.path_within_hierarchy = strdup(hirarchy_path);
	if (p_metac_scheme->hierarchy_member.path_within_hierarchy == NULL) {
		msg_stderr("wasn't able to build hirarchy_path for %s\n", hirarchy_path);
		metac_scheme_clean_as_hierarchy_member_scheme(p_metac_scheme);
		return -(ENOMEM);
	}

	/*msg_stddbg("local path: %s\n", p_metac_scheme->path_within_hierarchy);*/

	if (p_metac_scheme->p_actual_type != NULL) {
		int res = 0;

		switch(p_metac_scheme->p_actual_type->id) {
		case DW_TAG_structure_type:
		case DW_TAG_union_type:
			res = scheme_with_hierarchy_init(
					&p_metac_scheme->hierarchy,
					p_root_type,
					global_path,
					value_path,
					p_override_annotations,
					p_metac_scheme->p_actual_type);
			break;
		case DW_TAG_array_type:
			res = scheme_with_array_init(
					&p_metac_scheme->array,
					p_root_type,
					global_path,
					value_path,
					p_override_annotations,
					p_metac_scheme->p_actual_type);
			break;
		case DW_TAG_pointer_type:
			res = scheme_with_pointer_init(
					&p_metac_scheme->pointer,
					p_root_type,
					global_path,
					value_path,
					p_override_annotations,
					p_metac_scheme->p_actual_type);
			break;
		}

		if (res != 0) {

			msg_stddbg("initialization failed\n");

			metac_scheme_clean_as_hierarchy_member_scheme(p_metac_scheme);
			return -(EFAULT);
		}
	}

	return 0;
}

static struct metac_scheme * metac_scheme_create_as_hierarchy_member_scheme(
		struct metac_type *							p_root_type,
		char *										global_path,
		char *										value_path,
		char *										hierarchy_path,
		metac_type_annotation_t *					p_override_annotations,
		struct scheme_with_hierarchy *				p_current_hierarchy,
		struct discriminator *						p_discriminator,
		metac_discriminator_value_t					expected_discriminator_value,
		metac_count_t								member_id,
		struct metac_type_member_info *				p_member_info) {

	_create_(metac_scheme);

	if (metac_refcounter_object_init(
			&p_metac_scheme->refcounter_object,
			&_metac_scheme_refcounter_object_ops,
			NULL) != 0) {

		msg_stderr("metac_refcounter_object_init failed\n");

		metac_scheme_delete(&p_metac_scheme);
		return NULL;
	}

	if (metac_scheme_init_as_hierarchy_member_scheme(
			p_metac_scheme,
			p_root_type,
			global_path,
			value_path,
			hierarchy_path,
			p_override_annotations,
			p_current_hierarchy,
			p_discriminator,
			expected_discriminator_value,
			member_id,
			p_member_info) != 0) {

		msg_stderr("metac_scheme_init_as_hierarchy_member failed\n");

		metac_scheme_delete(&p_metac_scheme);
		return NULL;
	}

	return p_metac_scheme;
}

metac_flag_t metac_scheme_is_hierarchy_member_scheme(
		struct metac_scheme *						p_metac_scheme) {

	if (p_metac_scheme == NULL) {

		msg_stderr("invalid argument\n");
		return -(EINVAL);
	}

	return (p_metac_scheme->p_current_hierarchy == NULL)?0:1;
}

struct metac_scheme * metac_hierarchy_member_scheme_get_parent_scheme(
		struct metac_scheme *						p_metac_scheme) {

	if (metac_scheme_is_hierarchy_member_scheme(
			p_metac_scheme) != 1) {

		return NULL;
	}

	return scheme_with_hierarchy_get_value_scheme(p_metac_scheme->p_current_hierarchy);
}

metac_flag_t metac_scheme_is_hierachy_scheme(
		struct metac_scheme *						p_metac_scheme) {

	if (p_metac_scheme != NULL &&
		p_metac_scheme->p_actual_type != NULL && (
			p_metac_scheme->p_actual_type->id == DW_TAG_structure_type ||
			p_metac_scheme->p_actual_type->id == DW_TAG_union_type)) {

		return 1;
	}

	return 0;
}

metac_flag_t metac_scheme_is_array_scheme(
		struct metac_scheme *						p_metac_scheme) {

	if (p_metac_scheme != NULL &&
		p_metac_scheme->p_actual_type != NULL && (
			p_metac_scheme->p_actual_type->id == DW_TAG_array_type)) {

		return 1;
	}

	return 0;
}

metac_flag_t metac_scheme_is_pointer_scheme(
		struct metac_scheme *						p_metac_scheme) {

	if (p_metac_scheme != NULL &&
		p_metac_scheme->p_actual_type != NULL && (
			p_metac_scheme->p_actual_type->id == DW_TAG_pointer_type
		)) {
		return 1;
	}

	return 0;
}
/*****************************************************************************/
struct hierarchy_top_scheme_container_discriminator {
	struct cds_list_head							list;

	struct discriminator *							p_discriminator;
};

struct hierarchy_top_scheme_container_hierarchy_member {
	struct cds_list_head							list;

	struct metac_scheme *							p_value_scheme;
};

struct hierarchy_top_scheme_container {
	struct cds_list_head							discriminator_list;
	struct cds_list_head							hierarchy_member_list;
};

struct hierarchy_top_scheme_builder {
	struct metac_type *								p_root_type;
	metac_type_annotation_t *						p_override_annotations;

	struct hierarchy_top_scheme_container			container;

	struct scheduler								scheduler;
	struct cds_list_head							executed_tasks;
};

struct hierarchy_top_scheme_builder_hierarchy_member_task {
	struct scheduler_task							task;

	char * 											global_path;		/*local copy - keep track of it*/

	struct metac_scheme *							p_value_scheme;
};
/*****************************************************************************/

static int hierarchy_top_scheme_builder_schedule_hierarchy_member(
		struct hierarchy_top_scheme_builder *		p_hierarchy_top_scheme_scheme_builder,
		char *										global_path,
		struct metac_scheme *						p_value_scheme);

/*****************************************************************************/

static void hierarchy_top_scheme_container_discriminator_clean(
		struct hierarchy_top_scheme_container_discriminator *
													p_hierarchy_top_scheme_container_discriminator) {

	p_hierarchy_top_scheme_container_discriminator->p_discriminator = NULL;
}

static int hierarchy_top_scheme_container_discriminator_init(
	struct hierarchy_top_scheme_container_discriminator *
													p_hierarchy_top_scheme_container_discriminator,
	struct discriminator *							p_discriminator) {

	if (p_discriminator == NULL) {

		msg_stderr("Can't init p_element_type_hierarchy_top_scheme_container_discriminator with NULL p_discriminator \n");
		return (-EINVAL);
	}

	p_hierarchy_top_scheme_container_discriminator->p_discriminator = p_discriminator;

	return 0;
}

static int hierarchy_top_scheme_container_discriminator_delete(
		struct hierarchy_top_scheme_container_discriminator **
													pp_hierarchy_top_scheme_container_discriminator) {

	_delete_start_(hierarchy_top_scheme_container_discriminator);
	hierarchy_top_scheme_container_discriminator_clean(*pp_hierarchy_top_scheme_container_discriminator);
	_delete_finish(hierarchy_top_scheme_container_discriminator);
	return 0;
}

static struct hierarchy_top_scheme_container_discriminator * hierarchy_top_scheme_container_discriminator_create(
		struct discriminator *						p_discriminator) {

	int res;

	_create_(hierarchy_top_scheme_container_discriminator);

	if (hierarchy_top_scheme_container_discriminator_init(
			p_hierarchy_top_scheme_container_discriminator,
			p_discriminator) != 0) {

		hierarchy_top_scheme_container_discriminator_delete(&p_hierarchy_top_scheme_container_discriminator);
		return NULL;
	}

	return p_hierarchy_top_scheme_container_discriminator;
}

static int hierarchy_top_scheme_container_hierarchy_member_init(
		struct hierarchy_top_scheme_container_hierarchy_member *
													p_hierarchy_top_scheme_container_hierarchy_member,
		struct metac_scheme *						p_value_scheme) {

	if (p_hierarchy_top_scheme_container_hierarchy_member == NULL) {

		msg_stderr("Invalid argument p_value_scheme_container_hierarchy_member\n");
		return (-EINVAL);
	}

	p_hierarchy_top_scheme_container_hierarchy_member->p_value_scheme = metac_scheme_get(p_value_scheme);
	if (p_hierarchy_top_scheme_container_hierarchy_member->p_value_scheme == NULL) {

		msg_stderr("metac_scheme_get failed\n");
		return (-EINVAL);
	}

	return 0;
}

static void hierarchy_top_scheme_container_hierarchy_member_clean(
		struct hierarchy_top_scheme_container_hierarchy_member *
													p_hierarchy_top_scheme_container_hierarchy_member) {

	if (p_hierarchy_top_scheme_container_hierarchy_member == NULL) {

		return;
	}

	if (p_hierarchy_top_scheme_container_hierarchy_member->p_value_scheme != NULL) {

		metac_scheme_put(&p_hierarchy_top_scheme_container_hierarchy_member->p_value_scheme);
	}
}

static int hierarchy_top_scheme_container_hierarchy_member_delete(
		struct hierarchy_top_scheme_container_hierarchy_member **
													pp_hierarchy_top_scheme_container_hierarchy_member) {

	_delete_start_(hierarchy_top_scheme_container_hierarchy_member);
	hierarchy_top_scheme_container_hierarchy_member_clean(*pp_hierarchy_top_scheme_container_hierarchy_member);
	_delete_finish(hierarchy_top_scheme_container_hierarchy_member);
	return 0;
}

static struct hierarchy_top_scheme_container_hierarchy_member * hierarchy_top_scheme_container_hierarchy_member_create(
		struct metac_scheme *						p_metac_scheme) {

	int res;

	_create_(hierarchy_top_scheme_container_hierarchy_member);

	if (hierarchy_top_scheme_container_hierarchy_member_init(
			p_hierarchy_top_scheme_container_hierarchy_member,
			p_metac_scheme) != 0) {

		hierarchy_top_scheme_container_hierarchy_member_delete(&p_hierarchy_top_scheme_container_hierarchy_member);
		return NULL;
	}

	return p_hierarchy_top_scheme_container_hierarchy_member;
}

static int hierarchy_top_scheme_builder_add_discriminator (
		struct hierarchy_top_scheme_builder *		p_hierarchy_top_scheme_builder,
		struct discriminator *						p_discriminator) {

	struct hierarchy_top_scheme_container_discriminator * p_value_scheme_container_discriminator;

	p_value_scheme_container_discriminator = hierarchy_top_scheme_container_discriminator_create(p_discriminator);
	if (p_value_scheme_container_discriminator == NULL) {

		msg_stddbg("wasn't able to create p_value_scheme_container_discriminator\n");
		return (-EFAULT);
	}

	cds_list_add_tail(
		&p_value_scheme_container_discriminator->list,
		&p_hierarchy_top_scheme_builder->container.discriminator_list);
	return 0;
}

static int hierarchy_top_scheme_builder_add_hierarchy_member (
		struct hierarchy_top_scheme_builder *		p_hierarchy_top_scheme_builder,
		struct metac_scheme *						p_value_scheme) {

	struct hierarchy_top_scheme_container_hierarchy_member * p_value_scheme_container_hierarchy_member;

	p_value_scheme_container_hierarchy_member = hierarchy_top_scheme_container_hierarchy_member_create(
			p_value_scheme);
	if (p_value_scheme_container_hierarchy_member == NULL) {

		msg_stddbg("wasn't able to create p_value_scheme_container_hierarchy_member\n");
		return (-EFAULT);
	}

	cds_list_add_tail(
		&p_value_scheme_container_hierarchy_member->list,
		&p_hierarchy_top_scheme_builder->container.hierarchy_member_list);
	return 0;
}

static int hierarchy_top_scheme_builder_process_structure(
		struct hierarchy_top_scheme_builder *		p_hierarchy_top_scheme_builder,
		char *										global_path,
		char *										value_path,
		char *										hirarchy_path,
		struct metac_type *							p_actual_type,
		struct scheme_with_hierarchy *				p_scheme_with_hierarchy) {

	metac_num_t i;
	struct discriminator * p_discriminator = NULL;
	metac_discriminator_value_t expected_discriminator_value = 0;
	struct metac_scheme * p_metac_scheme = NULL;

	assert(p_actual_type->id == DW_TAG_structure_type);

	p_metac_scheme = scheme_with_hierarchy_get_value_scheme(p_scheme_with_hierarchy);
	if (p_metac_scheme == NULL) {

		msg_stderr("value_scheme_with_hierarchy_get_value_scheme failed %s\n", global_path);
		return (-EFAULT);
	}

	if (metac_scheme_is_hierarchy_member_scheme(p_metac_scheme) == 1) {

		p_discriminator = p_metac_scheme->hierarchy_member.precondition.p_discriminator;
		expected_discriminator_value = p_metac_scheme->hierarchy_member.precondition.expected_discriminator_value;
	}

	for (i = p_actual_type->structure_type_info.members_count; i > 0; i--) {
		int indx = i-1;
		struct metac_type_member_info * p_member_info = &p_actual_type->structure_type_info.members[indx];
		int res;
		char * member_global_path;
		char * member_value_path;
		char * member_hirarchy_path;

		member_global_path = build_member_path(global_path, p_member_info->name);
		if (member_global_path == NULL) {

			msg_stderr("wasn't able to build global_path for %s and %s\n", global_path, p_member_info->name);
			return (-EFAULT);
		}

		member_value_path = build_member_path(value_path, p_member_info->name);
		if (member_value_path == NULL) {

			msg_stderr("wasn't able to build value_path for %s and %s\n", global_path, p_member_info->name);
			free(member_global_path);
			return (-EFAULT);
		}

		member_hirarchy_path = build_member_path(hirarchy_path, p_member_info->name);
		if (member_hirarchy_path == NULL) {

			msg_stderr("wasn't able to build hirarchy_path for %s and %s\n", global_path, p_member_info->name);
			free(member_value_path);
			free(member_global_path);
			return (-EFAULT);
		}

		p_scheme_with_hierarchy->members[indx] = metac_scheme_create_as_hierarchy_member_scheme(
				p_hierarchy_top_scheme_builder->p_root_type,
				member_global_path,
				member_value_path,
				member_hirarchy_path,
				p_hierarchy_top_scheme_builder->p_override_annotations,
				p_scheme_with_hierarchy,
				p_discriminator,
				expected_discriminator_value,
				indx,
				p_member_info);
		if (p_scheme_with_hierarchy->members[indx] == NULL) {
			msg_stddbg("global_path: %s failed because metac_scheme_create_as_hierarchy_member failed\n", global_path);
			free(member_hirarchy_path);
			free(member_value_path);
			free(member_global_path);
			return (-EFAULT);
		}

		res = hierarchy_top_scheme_builder_schedule_hierarchy_member(
				p_hierarchy_top_scheme_builder,
				member_global_path,
				p_scheme_with_hierarchy->members[indx]);
		if (res != 0) {

			msg_stderr("failed to schedule global_path for %s\n", member_global_path);
			metac_scheme_put(&p_scheme_with_hierarchy->members[indx]);

			free(member_hirarchy_path);
			free(member_value_path);
			free(member_global_path);
			return res;
		}

		free(member_hirarchy_path);
		free(member_value_path);
		free(member_global_path);
	}

	for (i = 0; i < p_actual_type->structure_type_info.members_count; ++i) {

		struct metac_type_member_info * p_member_info = &p_actual_type->structure_type_info.members[i];

		if (hierarchy_top_scheme_builder_add_hierarchy_member(
				p_hierarchy_top_scheme_builder,
				p_scheme_with_hierarchy->members[i]) != 0) {

			msg_stderr("hierarchy_top_scheme_builder_add_hierarchy_member failed for for %s and %s\n", global_path, p_member_info->name);

			metac_scheme_put(&p_scheme_with_hierarchy->members[i]);
			return (-EFAULT);
		}
	}

	return 0;
}

static int hierarchy_top_scheme_builder_process_union(
		struct hierarchy_top_scheme_builder *		p_hierarchy_top_scheme_builder,
		char *										global_path,
		char *										value_path,
		char *										hirarchy_path,
		struct metac_type *							p_actual_type,
		struct scheme_with_hierarchy *				p_scheme_with_hierarchy) {
	metac_num_t i;
	struct discriminator * p_discriminator;
	assert(p_actual_type->id == DW_TAG_union_type);

	p_discriminator = &p_scheme_with_hierarchy->union_discriminator;
	if (hierarchy_top_scheme_builder_add_discriminator(p_hierarchy_top_scheme_builder, p_discriminator) != 0) {

		msg_stddbg("global_path: %s failed to add discriminator\n", global_path);
		return (-EFAULT);
	}

	for (i = p_actual_type->union_type_info.members_count; i > 0; i--) {
		int indx = i-1;
		struct metac_type_member_info * p_member_info = &p_actual_type->union_type_info.members[indx];
		int res;
		char * member_global_path;
		char * member_value_path;
		char * member_hirarchy_path;

		member_global_path = build_member_path(global_path, p_member_info->name);
		if (member_global_path == NULL) {

			msg_stderr("wasn't able to build global_path for %s and %s\n", global_path, p_member_info->name);
			return (-EFAULT);
		}

		member_value_path = build_member_path(value_path, p_member_info->name);
		if (member_value_path == NULL) {

			msg_stderr("wasn't able to build value_path for %s and %s\n", global_path, p_member_info->name);
			free(member_global_path);
			return (-EFAULT);
		}

		member_hirarchy_path = build_member_path(hirarchy_path, p_member_info->name);
		if (member_hirarchy_path == NULL) {

			msg_stderr("wasn't able to build hirarchy_path for %s and %s\n", global_path, p_member_info->name);
			free(member_value_path);
			free(member_global_path);
			return (-EFAULT);
		}

		p_scheme_with_hierarchy->members[indx] = metac_scheme_create_as_hierarchy_member_scheme(
				p_hierarchy_top_scheme_builder->p_root_type,
				member_global_path,
				member_value_path,
				member_hirarchy_path,
				p_hierarchy_top_scheme_builder->p_override_annotations,
				p_scheme_with_hierarchy,
				p_discriminator,
				indx,
				indx,
				p_member_info);
		if (p_scheme_with_hierarchy->members[indx] == NULL) {

			msg_stddbg("global_path: %s failed because element_type_hierarchy_member_create failed\n", global_path);
			free(member_hirarchy_path);
			free(member_value_path);
			free(member_global_path);
			return (-EFAULT);
		}

		if (hierarchy_top_scheme_builder_add_hierarchy_member(
				p_hierarchy_top_scheme_builder,
				p_scheme_with_hierarchy->members[indx]) != 0) {

			msg_stderr("hierarchy_top_scheme_builder_add_hierarchy_member failed for for %s and %s\n", global_path, p_member_info->name);

			metac_scheme_put(&p_scheme_with_hierarchy->members[indx]);
			free(member_hirarchy_path);
			free(member_value_path);
			free(member_global_path);
			return (-EFAULT);
		}

		res = hierarchy_top_scheme_builder_schedule_hierarchy_member(
				p_hierarchy_top_scheme_builder,
				member_global_path,
				p_scheme_with_hierarchy->members[indx]);
		if (res != 0) {

			msg_stderr("failed to schedule global_path for %s\n", member_global_path);
			metac_scheme_put(&p_scheme_with_hierarchy->members[indx]);
			free(member_hirarchy_path);
			free(member_value_path);
			free(member_global_path);
			return res;
		}

		free(member_hirarchy_path);
		free(member_value_path);
		free(member_global_path);
	}

	for (i = 0; i < p_actual_type->union_type_info.members_count; ++i) {

		struct metac_type_member_info * p_member_info = &p_actual_type->union_type_info.members[i];

		if (hierarchy_top_scheme_builder_add_hierarchy_member(
				p_hierarchy_top_scheme_builder,
				p_scheme_with_hierarchy->members[i]) != 0) {

			msg_stderr("hierarchy_top_scheme_builder_add_hierarchy_member failed for for %s and %s\n", global_path, p_member_info->name);

			metac_scheme_put(&p_scheme_with_hierarchy->members[i]);
			return (-EFAULT);
		}
	}

	return 0;
}

static int hierarchy_top_scheme_builder_process_hierarchy(
		struct hierarchy_top_scheme_builder *		p_hierarchy_top_scheme_builder,
		char *										global_path,
		char *										value_path,
		char *										hirarchy_path,
		struct metac_type *							p_actual_type,
		struct scheme_with_hierarchy *				p_scheme_with_hierarchy) {
	int res;
	msg_stddbg("process global_path: %s, value_path: %s, hirarchy_path: %s\n",
			global_path,
			value_path,
			hirarchy_path);

	switch (p_actual_type->id) {
	case DW_TAG_structure_type:
		res = hierarchy_top_scheme_builder_process_structure(
				p_hierarchy_top_scheme_builder,
				global_path,
				value_path,
				hirarchy_path,
				p_actual_type,
				p_scheme_with_hierarchy);
		break;
	case DW_TAG_union_type:
		res = hierarchy_top_scheme_builder_process_union(
				p_hierarchy_top_scheme_builder,
				global_path,
				value_path,
				hirarchy_path,
				p_actual_type,
				p_scheme_with_hierarchy);
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

static int hierarchy_top_scheme_builder_process_hierarchy_member(
		struct hierarchy_top_scheme_builder *		p_hierarchy_top_scheme_builder,
		char *										global_path,
		struct metac_scheme *						p_value_scheme) {

	if (	metac_scheme_is_hierarchy_member_scheme(p_value_scheme) == 1 &&
			p_value_scheme->p_actual_type && (
			p_value_scheme->p_actual_type->id == DW_TAG_structure_type||
			p_value_scheme->p_actual_type->id == DW_TAG_union_type)) {

		if (hierarchy_top_scheme_builder_process_hierarchy(
				p_hierarchy_top_scheme_builder,
				global_path,
				p_value_scheme->path_within_value,
				p_value_scheme->hierarchy_member.path_within_hierarchy,
				p_value_scheme->p_actual_type,
				&p_value_scheme->hierarchy) != 0) {

			msg_stderr("failed to process global_path for %s\n", global_path);
			return (-EFAULT);
		}
	}
	return 0;
}

static int hierarchy_top_scheme_builder_process_hierarchy_member_fn(
	struct scheduler * 								p_engine,
	struct scheduler_task * 						p_task,
	int 											error_flag) {

	struct hierarchy_top_scheme_builder * p_hierarchy_top_scheme_builder =
		cds_list_entry(p_engine, struct hierarchy_top_scheme_builder, scheduler);

	struct hierarchy_top_scheme_builder_hierarchy_member_task * p_hierarchy_top_scheme_builder_hierarchy_member_task =
		cds_list_entry(p_task, struct hierarchy_top_scheme_builder_hierarchy_member_task, task);

	/*TODO: consider deletion of the task after execution or adding them to the task pool */
	cds_list_add_tail(&p_task->list, &p_hierarchy_top_scheme_builder->executed_tasks);

	if (error_flag != 0) {

		return (-EALREADY);
	}

	return hierarchy_top_scheme_builder_process_hierarchy_member(
			p_hierarchy_top_scheme_builder,
			p_hierarchy_top_scheme_builder_hierarchy_member_task->global_path,
			p_hierarchy_top_scheme_builder_hierarchy_member_task->p_value_scheme);
}

static int hierarchy_top_scheme_builder_hierarchy_member_task_delete(
		struct hierarchy_top_scheme_builder_hierarchy_member_task **
													pp_hierarchy_top_scheme_builder_hierarchy_member_task) {
	_delete_start_(hierarchy_top_scheme_builder_hierarchy_member_task);

	if ((*pp_hierarchy_top_scheme_builder_hierarchy_member_task)->global_path) {

		free((*pp_hierarchy_top_scheme_builder_hierarchy_member_task)->global_path);
		(*pp_hierarchy_top_scheme_builder_hierarchy_member_task)->global_path = NULL;
	}

	if ((*pp_hierarchy_top_scheme_builder_hierarchy_member_task)->p_value_scheme != NULL) {

		metac_scheme_put(&(*pp_hierarchy_top_scheme_builder_hierarchy_member_task)->p_value_scheme);
	}

	_delete_finish(hierarchy_top_scheme_builder_hierarchy_member_task);
	return 0;
}

static struct hierarchy_top_scheme_builder_hierarchy_member_task * hierarchy_top_scheme_builder_hierarchy_member_task_create(
		scheduler_task_fn_t 						fn,
		char *										global_path,
		struct metac_scheme *						p_value_scheme) {
	_create_(hierarchy_top_scheme_builder_hierarchy_member_task);

	p_hierarchy_top_scheme_builder_hierarchy_member_task->task.fn = fn;
	p_hierarchy_top_scheme_builder_hierarchy_member_task->global_path = strdup(global_path);
	p_hierarchy_top_scheme_builder_hierarchy_member_task->p_value_scheme = metac_scheme_get(p_value_scheme);

	if (p_hierarchy_top_scheme_builder_hierarchy_member_task->global_path == NULL ||
		p_hierarchy_top_scheme_builder_hierarchy_member_task->p_value_scheme == NULL) {

		msg_stderr("wasn't able to duplicate global_path or p_value_scheme\n");
		hierarchy_top_scheme_builder_hierarchy_member_task_delete(&p_hierarchy_top_scheme_builder_hierarchy_member_task);
		return NULL;
	}
	return p_hierarchy_top_scheme_builder_hierarchy_member_task;
}

static int hierarchy_top_scheme_builder_schedule_hierarchy_member(
		struct hierarchy_top_scheme_builder *		p_hierarchy_top_scheme_builder,
		char *										global_path,
		struct metac_scheme *						p_value_scheme) {

	struct hierarchy_top_scheme_builder_hierarchy_member_task * p_task = hierarchy_top_scheme_builder_hierarchy_member_task_create(
		hierarchy_top_scheme_builder_process_hierarchy_member_fn,
		global_path,
		p_value_scheme);

	if (p_task == NULL) {
		msg_stderr("wasn't able to create a task\n");
		return -(EFAULT);
	}

	if (scheduler_add_task_to_front(
		&p_hierarchy_top_scheme_builder->scheduler,
		&p_task->task) != 0) {

		msg_stderr("wasn't able to schedule task\n");
		hierarchy_top_scheme_builder_hierarchy_member_task_delete(&p_task);
		return -(EFAULT);
	}

	return 0;
}

static void hierarchy_top_scheme_container_clean(
		struct hierarchy_top_scheme_container *		p_value_scheme_container) {

	struct hierarchy_top_scheme_container_discriminator * p_discriminator, * _p_discriminator;
	struct hierarchy_top_scheme_container_hierarchy_member * p_member, * _p_member;

	cds_list_for_each_entry_safe(p_discriminator, _p_discriminator, &p_value_scheme_container->discriminator_list, list) {
		cds_list_del(&p_discriminator->list);
		hierarchy_top_scheme_container_discriminator_delete(&p_discriminator);
	}

	cds_list_for_each_entry_safe(p_member, _p_member, &p_value_scheme_container->hierarchy_member_list, list) {
		cds_list_del(&p_member->list);
		hierarchy_top_scheme_container_hierarchy_member_delete(&p_member);
	}
}

static int hierarchy_top_scheme_container_init(
		struct hierarchy_top_scheme_container *		p_value_scheme_container) {

	CDS_INIT_LIST_HEAD(&p_value_scheme_container->discriminator_list);
	CDS_INIT_LIST_HEAD(&p_value_scheme_container->hierarchy_member_list);

	return 0;
}

static int hierarchy_top_scheme_builder_init(
		struct hierarchy_top_scheme_builder * 		p_hierarchy_top_scheme_builder,
		struct metac_type *							p_root_type,
		metac_type_annotation_t *					p_override_annotations) {

	p_hierarchy_top_scheme_builder->p_root_type = p_root_type;
	p_hierarchy_top_scheme_builder->p_override_annotations = p_override_annotations;
	hierarchy_top_scheme_container_init(&p_hierarchy_top_scheme_builder->container);

	CDS_INIT_LIST_HEAD(&p_hierarchy_top_scheme_builder->executed_tasks);

	scheduler_init(&p_hierarchy_top_scheme_builder->scheduler);

	return 0;
}

static void hierarchy_top_scheme_builder_clean(
		struct hierarchy_top_scheme_builder * 				p_hierarchy_top_scheme_builder) {

	struct hierarchy_top_scheme_builder_hierarchy_member_task * p_hierarchy_member_task, *_p_hierarchy_member_task;

	scheduler_clean(&p_hierarchy_top_scheme_builder->scheduler);

	cds_list_for_each_entry_safe(p_hierarchy_member_task, _p_hierarchy_member_task, &p_hierarchy_top_scheme_builder->executed_tasks, task.list) {

		cds_list_del(&p_hierarchy_member_task->task.list);
		hierarchy_top_scheme_builder_hierarchy_member_task_delete(&p_hierarchy_member_task);
	}

	hierarchy_top_scheme_container_clean(&p_hierarchy_top_scheme_builder->container);

	p_hierarchy_top_scheme_builder->p_root_type = NULL;
	p_hierarchy_top_scheme_builder->p_override_annotations = NULL;
}

static int metac_scheme_finalize_as_hierarchy_top_scheme(
		struct hierarchy_top_scheme_builder * 		p_hierarchy_top_scheme_builder,
		struct metac_scheme *						p_value_scheme) {
	metac_count_t i;

	struct hierarchy_top_scheme_container_discriminator * p_discriminator, * _p_discriminator;
	struct hierarchy_top_scheme_container_hierarchy_member * p_member, * _p_member;

	p_value_scheme->p_current_hierarchy = NULL;

	p_value_scheme->hierarchy_top.discriminators_count = 0;
	p_value_scheme->hierarchy_top.pp_discriminators = NULL;
	p_value_scheme->hierarchy_top.members_count = 0;
	p_value_scheme->hierarchy_top.pp_members = NULL;

	/*get arrays lengths */
	cds_list_for_each_entry(p_discriminator, &p_hierarchy_top_scheme_builder->container.discriminator_list, list) {
		++p_value_scheme->hierarchy_top.discriminators_count;
	}
	cds_list_for_each_entry(p_member, &p_hierarchy_top_scheme_builder->container.hierarchy_member_list, list) {
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
		cds_list_for_each_entry(p_discriminator, &p_hierarchy_top_scheme_builder->container.discriminator_list, list) {

			p_value_scheme->hierarchy_top.pp_discriminators[i] = p_discriminator->p_discriminator;
			p_value_scheme->hierarchy_top.pp_discriminators[i]->id = i;

			++i;
		}
	}

	if (p_value_scheme->hierarchy_top.members_count > 0) {

		p_value_scheme->hierarchy_top.pp_members = (struct metac_scheme **)calloc(
				p_value_scheme->hierarchy_top.members_count, sizeof(*p_value_scheme->hierarchy_top.pp_members));

		if (p_value_scheme->hierarchy_top.pp_members == NULL) {

			msg_stderr("can't allocate memory for members\n");
			free(p_value_scheme->hierarchy_top.pp_discriminators);
			return (-ENOMEM);
		}

		i = 0;
		cds_list_for_each_entry(p_member, &p_hierarchy_top_scheme_builder->container.hierarchy_member_list, list) {

			assert(metac_scheme_is_hierarchy_member_scheme(p_member->p_value_scheme) == 1);

			p_value_scheme->hierarchy_top.pp_members[i] = metac_scheme_get(p_member->p_value_scheme);
			if (p_value_scheme->hierarchy_top.pp_members[i] == NULL) {

				msg_stderr("metac_scheme_get failed\n");
				return (-ENOMEM);
			}

			p_value_scheme->hierarchy_top.pp_members[i]->hierarchy_member.id = i;

			msg_stddbg("added member %s\n", p_value_scheme->hierarchy_top.pp_members[i]->hierarchy_member.path_within_hierarchy);
			++i;
		}
	}

	return 0;
}

metac_flag_t metac_scheme_is_hierarchy_top_scheme(
		struct metac_scheme *						p_metac_scheme) {

	if (p_metac_scheme == NULL) {

		msg_stderr("invalid argument\n");
		return -(EINVAL);
	}

	return (p_metac_scheme->p_current_hierarchy == NULL &&
			metac_scheme_is_hierachy_scheme(p_metac_scheme) == 1)?1:0;
}

static int metac_scheme_clean_as_hierarchy_top_scheme(
		struct metac_scheme *						p_value_scheme) {
	metac_count_t i;

	if (metac_scheme_is_hierarchy_top_scheme(p_value_scheme) != 1) {
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

			metac_scheme_put(&p_value_scheme->hierarchy_top.pp_members[i]);
		}

		free(p_value_scheme->hierarchy_top.pp_members);
		p_value_scheme->hierarchy_top.pp_members = NULL;
	}

	scheme_with_hierarchy_clean(&p_value_scheme->hierarchy);

	return 0;
}

static int metac_scheme_init_as_hierarchy_top_scheme(
		struct metac_scheme *						p_metac_scheme,
		struct metac_type *							p_root_type,
		char * 										global_path,
		char *										value_path,
		metac_type_annotation_t *					p_override_annotations) {
	struct hierarchy_top_scheme_builder hierarchy_top_scheme_builder;

	if (p_metac_scheme->p_actual_type->id != DW_TAG_structure_type &&
		p_metac_scheme->p_actual_type->id != DW_TAG_union_type) {

		msg_stderr("Invalid argument p_actual_type\n");
		return -(EINVAL);
	}

	hierarchy_top_scheme_builder_init(&hierarchy_top_scheme_builder, p_root_type, p_override_annotations);

	p_metac_scheme->p_current_hierarchy = NULL;

	if (scheme_with_hierarchy_init(
			&p_metac_scheme->hierarchy,
			p_root_type,
			global_path,
			value_path,
			p_override_annotations,
			p_metac_scheme->p_actual_type) != 0) {
		msg_stderr("value_scheme_with_hierarchy_init failed\n");

		hierarchy_top_scheme_builder_clean(&hierarchy_top_scheme_builder);
		return (-EFAULT);
	}

	if (hierarchy_top_scheme_builder_process_hierarchy(
			&hierarchy_top_scheme_builder,
			global_path,
			value_path,
			"",
			p_metac_scheme->p_actual_type,
			&p_metac_scheme->hierarchy) != 0) {

		msg_stddbg("wasn't able to schedule the first task\n");

		scheme_with_hierarchy_clean(&p_metac_scheme->hierarchy);
		hierarchy_top_scheme_builder_clean(&hierarchy_top_scheme_builder);
		return (-EFAULT);
	}

	if (scheduler_run(&hierarchy_top_scheme_builder.scheduler) != 0) {

		msg_stddbg("tasks execution finished with fail\n");

		scheme_with_hierarchy_clean(&p_metac_scheme->hierarchy);
		hierarchy_top_scheme_builder_clean(&hierarchy_top_scheme_builder);
		return (-EFAULT);
	}

	if (metac_scheme_finalize_as_hierarchy_top_scheme(
			&hierarchy_top_scheme_builder,
			p_metac_scheme) != 0) {

		msg_stddbg("value_scheme finalization failed\n");

		metac_scheme_clean_as_hierarchy_top_scheme(p_metac_scheme);
		hierarchy_top_scheme_builder_clean(&hierarchy_top_scheme_builder);
		return (-EFAULT);
	}

	hierarchy_top_scheme_builder_clean(&hierarchy_top_scheme_builder);

	return 0;
}

static int metac_scheme_init_as_indexable(
		struct metac_scheme *						p_metac_scheme,
		struct metac_type *							p_root_type,
		char * 										global_path,
		char *										value_path,
		metac_type_annotation_t *					p_override_annotations,
		struct metac_type *							p_type) {

	int res = 0;

	if (p_type == NULL) {

		msg_stderr("invalid argument for %s\n", value_path);
		return -(EINVAL);
	}

	p_metac_scheme->p_current_hierarchy = NULL;

	p_metac_scheme->p_type = metac_type_get(p_type);
	if (p_metac_scheme->p_type == NULL) {

		msg_stderr("metac_type_get failed for %s\n", value_path);
		metac_scheme_clean(p_metac_scheme);
		return -(EFAULT);
	}

	p_metac_scheme->p_actual_type = metac_type_get_actual_type(p_type);
	if (p_metac_scheme->p_actual_type == NULL) {

		msg_stderr("p_actual_type is NULL for %s\n", value_path);
		metac_scheme_clean(p_metac_scheme);
		return -(EFAULT);
	}

	p_metac_scheme->byte_size = metac_type_byte_size(p_type);

	p_metac_scheme->path_within_value = strdup(value_path);
	if (p_metac_scheme->path_within_value == NULL) {

		msg_stderr("wasn't able to build value_path for %s\n", value_path);
		metac_scheme_clean(p_metac_scheme);
		return -(ENOMEM);
	}

	switch(p_metac_scheme->p_actual_type->id) {
	case DW_TAG_structure_type:
	case DW_TAG_union_type:
		res = metac_scheme_init_as_hierarchy_top_scheme(
				p_metac_scheme,
				p_root_type,
				global_path,
				value_path,
				p_override_annotations);
		break;
	case DW_TAG_array_type:
		res = scheme_with_array_init(
				&p_metac_scheme->array,
				p_root_type,
				global_path,
				value_path,
				p_override_annotations,
				p_metac_scheme->p_actual_type);
		break;
	case DW_TAG_pointer_type:
		res = scheme_with_pointer_init(
				&p_metac_scheme->pointer,
				p_root_type,
				global_path,
				value_path,
				p_override_annotations,
				p_metac_scheme->p_actual_type);
		break;
	}

	if (res != 0) {

		metac_scheme_clean(p_metac_scheme);
	}

	return res;
}

metac_flag_t metac_scheme_is_indexable(
		struct metac_scheme *						p_metac_scheme) {

	if (p_metac_scheme == NULL) {

		msg_stderr("invalid argument\n");
		return -(EINVAL);
	}

	return (p_metac_scheme->p_current_hierarchy == NULL)?1:0;
}

static struct metac_scheme * metac_scheme_create_as_indexable(
		struct metac_type *							p_type,
		char * 										global_path,
		char *										value_path,
		struct metac_type *							p_root_type,
		metac_type_annotation_t *					p_override_annotations) {

	_create_(metac_scheme);

	if (metac_refcounter_object_init(
			&p_metac_scheme->refcounter_object,
			&_metac_scheme_refcounter_object_ops,
			NULL) != 0) {

		msg_stderr("metac_refcounter_object_init failed\n");

		metac_scheme_delete(&p_metac_scheme);
		return NULL;
	}

	if (metac_scheme_init_as_indexable(
			p_metac_scheme,
			p_root_type,
			global_path,
			value_path,
			p_override_annotations,
			p_type) != 0) {

		msg_stderr("metac_scheme_init failed\n");

		metac_scheme_delete(&p_metac_scheme);
		return NULL;
	}

	return p_metac_scheme;
}

/*****************************************************************************/
struct value_scheme_container_value_scheme {
	struct cds_list_head							list;
	struct metac_scheme *							p_metac_scheme;
};
struct value_scheme_container {
	struct cds_list_head							arrays;

	struct cds_list_head							pointers;
};
struct value_scheme_builder {
	struct metac_type *								p_root_type;
	metac_type_annotation_t *						p_override_annotations;

	struct value_scheme_container					container;

	struct scheduler								scheduler;
	struct cds_list_head							executed_tasks;
};
struct value_scheme_builder_value_scheme_task {
	struct scheduler_task							task;
	char * 											global_path;	/*local copy - keep track of it*/

	struct metac_scheme *							p_metac_scheme;
};

/*****************************************************************************/

static int value_scheme_builder_schedule_value_scheme(
		struct value_scheme_builder *				p_value_scheme_builder,
		char *										global_path,
		struct metac_scheme *						p_metac_scheme);

/*****************************************************************************/

static void value_scheme_container_value_scheme_clean(
		struct value_scheme_container_value_scheme *
													p_value_scheme_container_value_scheme) {

	if (p_value_scheme_container_value_scheme == NULL) {

		msg_stderr("p_value_scheme_container_value_scheme is invalid\n");
		return;
	}

	metac_scheme_put(&p_value_scheme_container_value_scheme->p_metac_scheme);

}
static int value_scheme_container_value_scheme_init(
		struct value_scheme_container_value_scheme *
													p_value_scheme_container_value_scheme,
		struct metac_scheme *						p_metac_scheme) {

	if (p_value_scheme_container_value_scheme == NULL) {

		msg_stderr("p_value_scheme_container_value_scheme is invalid\n");
		return (-EINVAL);
	}

	if (p_metac_scheme == NULL) {

		msg_stderr("Can't init value_scheme_container_value_scheme with NULL p_metac_scheme\n");
		return (-EINVAL);
	}

	p_value_scheme_container_value_scheme->p_metac_scheme = metac_scheme_get(p_metac_scheme);
	if (p_value_scheme_container_value_scheme->p_metac_scheme == NULL) {

		msg_stderr("metac_scheme_get failed\n");
		return (-EINVAL);
	}

	return 0;
}

static int value_scheme_container_value_scheme_delete(
		struct value_scheme_container_value_scheme **
													pp_value_scheme_container_value_scheme) {

	_delete_start_(value_scheme_container_value_scheme);
	value_scheme_container_value_scheme_clean(*pp_value_scheme_container_value_scheme);
	_delete_finish(value_scheme_container_value_scheme);
	return 0;
}

static struct value_scheme_container_value_scheme * value_scheme_container_value_scheme_create(
		struct metac_scheme *						p_metac_scheme) {

	int res;

	_create_(value_scheme_container_value_scheme);

	if (value_scheme_container_value_scheme_init(
			p_value_scheme_container_value_scheme,
			p_metac_scheme) != 0) {

		value_scheme_container_value_scheme_delete(&p_value_scheme_container_value_scheme);
		return NULL;
	}

	return p_value_scheme_container_value_scheme;
}

static int value_scheme_builder_add_array (
		struct value_scheme_builder *				p_value_scheme_builder,
		struct metac_scheme *						p_metac_scheme) {

	struct value_scheme_container_value_scheme * p_value_scheme_container_value_scheme;

	p_value_scheme_container_value_scheme = value_scheme_container_value_scheme_create(
			p_metac_scheme);
	if (p_value_scheme_container_value_scheme == NULL) {

		msg_stddbg("wasn't able to create p_value_scheme_container_value_scheme\n");
		return (-EFAULT);
	}

	cds_list_add_tail(
		&p_value_scheme_container_value_scheme->list,
		&p_value_scheme_builder->container.arrays);

	return 0;
}

static int value_scheme_builder_add_pointer (
		struct value_scheme_builder *				p_value_scheme_builder,
		struct metac_scheme *						p_metac_scheme) {

	struct value_scheme_container_value_scheme * p_value_scheme_container_value_scheme;

	p_value_scheme_container_value_scheme = value_scheme_container_value_scheme_create(
			p_metac_scheme);
	if (p_value_scheme_container_value_scheme == NULL) {

		msg_stddbg("wasn't able to create p_value_scheme_container_value_scheme\n");
		return (-EFAULT);
	}

	cds_list_add_tail(
		&p_value_scheme_container_value_scheme->list,
		&p_value_scheme_builder->container.pointers);

	return 0;
}

static int value_scheme_builder_process_value_scheme_array(
		struct value_scheme_builder *				p_value_scheme_builder,
		char *										global_path,
		char *										value_path,
		struct metac_scheme *						p_metac_scheme) {

	int res = 0;
	char * target_global_path;
	char * target_value_path;

	assert(p_metac_scheme->p_actual_type->id == DW_TAG_array_type);
	assert(p_metac_scheme->array.p_child_value_scheme == NULL);

	msg_stddbg("value_path %s global_path %s\n", value_path, global_path);

	if (value_scheme_builder_add_array(
			p_value_scheme_builder,
			p_metac_scheme) != 0) {

		msg_stderr("value_scheme_builder_add_array failed %s\n", global_path);
	}

	if (p_metac_scheme->p_actual_type->array_type_info.type == NULL) {

		return 0;
	}

	target_global_path = alloc_sptrinf("%s[]", global_path);
	if (target_global_path == NULL) {

		return -(EFAULT);
	}

	target_value_path = alloc_sptrinf("%s[]", value_path);
	if (target_value_path == NULL) {

		free(target_global_path);
		return -(EFAULT);
	}

	p_metac_scheme->array.p_child_value_scheme = metac_scheme_create_as_indexable(
			p_metac_scheme->p_actual_type->array_type_info.type,
			target_global_path,
			target_value_path,
			p_value_scheme_builder->p_root_type,
			p_value_scheme_builder->p_override_annotations);
	if (p_metac_scheme->array.p_child_value_scheme == NULL) {

		msg_stderr("metac_scheme_create_as_indexable failed %s\n", global_path);
		res = -(EFAULT);

	} else if (value_scheme_builder_schedule_value_scheme(
			p_value_scheme_builder,
			target_global_path,
			p_metac_scheme->array.p_child_value_scheme) != 0) {

		msg_stderr("value_scheme_builder_schedule_value_scheme failed %s\n", global_path);
		res = -(EFAULT);
	}

	msg_stddbg("added %s %s\n", target_value_path, target_global_path);

	free(target_value_path);
	free(target_global_path);

	return res;
}

static int value_scheme_builder_process_value_scheme_pointer(
		struct value_scheme_builder *				p_value_scheme_builder,
		char *										global_path,
		char *										value_path,
		struct metac_scheme *						p_metac_scheme) {

	assert(p_metac_scheme->p_actual_type->id == DW_TAG_pointer_type);

	if (value_scheme_builder_add_pointer(
			p_value_scheme_builder,
			p_metac_scheme) != 0) {

		msg_stderr("value_scheme_builder_add_pointer failed %s\n", global_path);
	}

	return 0;
}
static int value_scheme_builder_process_value_scheme_hierarchy_top(
		struct value_scheme_builder *				p_value_scheme_builder,
		char *										global_path,
		char *										value_path,
		struct metac_scheme *						p_metac_scheme) {
	int i = 0;

	assert(metac_scheme_is_hierarchy_top_scheme(p_metac_scheme) == 1);

	for (i = 0; i < p_metac_scheme->hierarchy_top.members_count; ++i) {

		assert(p_metac_scheme->hierarchy_top.pp_members);
		assert(metac_scheme_is_hierarchy_member_scheme(p_metac_scheme->hierarchy_top.pp_members[i]) == 1);

		if (	p_metac_scheme->hierarchy_top.pp_members[i]->p_actual_type != NULL && (
				p_metac_scheme->hierarchy_top.pp_members[i]->p_actual_type->id == DW_TAG_array_type ||
				p_metac_scheme->hierarchy_top.pp_members[i]->p_actual_type->id == DW_TAG_pointer_type)) {

			char * target_global_path;
			char * target_value_path;

			target_global_path = build_member_path(
					global_path,
					p_metac_scheme->hierarchy_top.pp_members[i]->hierarchy_member.path_within_hierarchy);
			if (target_global_path == NULL) {

				msg_stderr("Can't build target_global_path for %s member %d\n", global_path, i);
				return (-EFAULT);
			}

			target_value_path = build_member_path(
					value_path,
					p_metac_scheme->hierarchy_top.pp_members[i]->hierarchy_member.path_within_hierarchy);
			if (target_value_path == NULL) {

				msg_stderr("Can't build target_value_path for %s member %d\n", global_path, i);

				free(target_global_path);
				return (-EFAULT);
			}

			switch(p_metac_scheme->hierarchy_top.pp_members[i]->p_actual_type->id){
			case DW_TAG_array_type:

				if (value_scheme_builder_process_value_scheme_array(
						p_value_scheme_builder,
						target_global_path,
						target_value_path,
						p_metac_scheme->hierarchy_top.pp_members[i]) != 0) {

					msg_stderr("value_scheme_builder_process_value_scheme_array failed for %s\n", target_global_path);

					free(target_value_path);
					free(target_global_path);
					return (-EFAULT);
				}
				break;
			case DW_TAG_pointer_type:

				if (value_scheme_builder_process_value_scheme_pointer(
						p_value_scheme_builder,
						target_global_path,
						target_value_path,
						p_metac_scheme->hierarchy_top.pp_members[i]) != 0){

					msg_stderr("value_scheme_builder_process_value_scheme_pointer failed for %s\n", target_global_path);

					free(target_value_path);
					free(target_global_path);
					return (-EFAULT);
				}
				break;
			}

			free(target_value_path);
			free(target_global_path);
		}
	}
	return 0;
}

static int value_scheme_builder_process_value_scheme(
		struct value_scheme_builder *				p_value_scheme_builder,
		char *										global_path,
		struct metac_scheme *						p_metac_scheme) {
	int res = 0;

	switch(p_metac_scheme->p_actual_type->id) {
	case DW_TAG_array_type:
		res = value_scheme_builder_process_value_scheme_array(
				p_value_scheme_builder,
				global_path,
				p_metac_scheme->path_within_value,
				p_metac_scheme);
		break;
	case DW_TAG_pointer_type:
		res = value_scheme_builder_process_value_scheme_pointer(
				p_value_scheme_builder,
				global_path,
				p_metac_scheme->path_within_value,
				p_metac_scheme);
		break;
	case DW_TAG_structure_type:
	case DW_TAG_union_type:
		res = value_scheme_builder_process_value_scheme_hierarchy_top(
				p_value_scheme_builder,
				global_path,
				p_metac_scheme->path_within_value,
				p_metac_scheme);
		break;
	}

	return res;
}

static int value_scheme_builder_process_value_scheme_fn(
	struct scheduler * 								p_engine,
	struct scheduler_task * 						p_task,
	int 											error_flag) {

	struct value_scheme_builder * p_value_scheme_builder =
		cds_list_entry(p_engine, struct value_scheme_builder, scheduler);

	struct value_scheme_builder_value_scheme_task * p_value_scheme_builder_value_scheme_task =
		cds_list_entry(p_task, struct value_scheme_builder_value_scheme_task, task);

	/*TODO: consider deletion of the task after execution or adding them to the task pool */
	cds_list_add_tail(&p_task->list, &p_value_scheme_builder->executed_tasks);

	if (error_flag != 0) {

		return (-EALREADY);
	}

	return value_scheme_builder_process_value_scheme(
			p_value_scheme_builder,
			p_value_scheme_builder_value_scheme_task->global_path,
			p_value_scheme_builder_value_scheme_task->p_metac_scheme);
}

static int value_scheme_builder_value_scheme_task_delete(
		struct value_scheme_builder_value_scheme_task **
													pp_value_scheme_builder_value_scheme_task) {
	_delete_start_(value_scheme_builder_value_scheme_task);

	if ((*pp_value_scheme_builder_value_scheme_task)->global_path) {

		free((*pp_value_scheme_builder_value_scheme_task)->global_path);
		(*pp_value_scheme_builder_value_scheme_task)->global_path = NULL;
	}

	if ((*pp_value_scheme_builder_value_scheme_task)->p_metac_scheme != NULL) {

		metac_scheme_put(&(*pp_value_scheme_builder_value_scheme_task)->p_metac_scheme);
	}

	_delete_finish(value_scheme_builder_value_scheme_task);
	return 0;
}

static struct value_scheme_builder_value_scheme_task * value_scheme_builder_value_scheme_task_create(
		scheduler_task_fn_t 						fn,
		char *										global_path,
		struct metac_scheme *						p_metac_scheme) {
	_create_(value_scheme_builder_value_scheme_task);

	p_value_scheme_builder_value_scheme_task->task.fn = fn;
	p_value_scheme_builder_value_scheme_task->global_path = strdup(global_path);
	p_value_scheme_builder_value_scheme_task->p_metac_scheme = metac_scheme_get(p_metac_scheme);

	if (p_value_scheme_builder_value_scheme_task->global_path == NULL ||
		p_value_scheme_builder_value_scheme_task->p_metac_scheme == NULL) {

		msg_stderr("wasn't able to duplicate global_path or p_metac_scheme\n");
		value_scheme_builder_value_scheme_task_delete(&p_value_scheme_builder_value_scheme_task);
		return NULL;
	}
	return p_value_scheme_builder_value_scheme_task;
}

static int value_scheme_builder_schedule_value_scheme(
		struct value_scheme_builder *				p_value_scheme_builder,
		char *										global_path,
		struct metac_scheme *						p_metac_scheme) {

	struct value_scheme_builder_value_scheme_task * p_task = value_scheme_builder_value_scheme_task_create(
		value_scheme_builder_process_value_scheme_fn,
		global_path,
		p_metac_scheme);
	if (p_task == NULL) {

		msg_stderr("wasn't able to create a task\n");
		return -(EFAULT);
	}

	if (scheduler_add_task_to_front(
		&p_value_scheme_builder->scheduler,
		&p_task->task) != 0) {

		msg_stderr("wasn't able to schedule task\n");
		value_scheme_builder_value_scheme_task_delete(&p_task);
		return -(EFAULT);
	}

	return 0;
}

static void value_scheme_container_clean(
		struct value_scheme_container *				p_value_scheme_container) {

	struct value_scheme_container_value_scheme * value_scheme, * _value_scheme;

	cds_list_for_each_entry_safe(value_scheme, _value_scheme, &p_value_scheme_container->pointers, list) {

		cds_list_del(&value_scheme->list);
		value_scheme_container_value_scheme_delete(&value_scheme);
	}

	cds_list_for_each_entry_safe(value_scheme, _value_scheme, &p_value_scheme_container->arrays, list) {

		cds_list_del(&value_scheme->list);
		value_scheme_container_value_scheme_delete(&value_scheme);
	}
}

static int value_scheme_container_init(
		struct value_scheme_container *				p_value_scheme_container) {

	CDS_INIT_LIST_HEAD(&p_value_scheme_container->arrays);
	CDS_INIT_LIST_HEAD(&p_value_scheme_container->pointers);

	return 0;
}

static void value_scheme_builder_clean(
		struct value_scheme_builder *				p_value_scheme_builder) {
	struct value_scheme_builder_value_scheme_task * p_task, *_p_task;

	scheduler_clean(&p_value_scheme_builder->scheduler);

	cds_list_for_each_entry_safe(p_task, _p_task, &p_value_scheme_builder->executed_tasks, task.list) {

		cds_list_del(&p_task->task.list);
		value_scheme_builder_value_scheme_task_delete(&p_task);
	}

	value_scheme_container_clean(&p_value_scheme_builder->container);

	p_value_scheme_builder->p_root_type = NULL;
	p_value_scheme_builder->p_override_annotations = NULL;
}

static int value_scheme_builder_init(
		struct value_scheme_builder *				p_value_scheme_builder,
		struct metac_type *							p_root_type,
		metac_type_annotation_t *					p_override_annotations) {

	p_value_scheme_builder->p_root_type = p_root_type;
	p_value_scheme_builder->p_override_annotations = p_override_annotations;
	value_scheme_container_init(&p_value_scheme_builder->container);

	CDS_INIT_LIST_HEAD(&p_value_scheme_builder->executed_tasks);

	scheduler_init(&p_value_scheme_builder->scheduler);

	return 0;
}

static void value_scheme_clean(
		struct value_scheme *						p_value_scheme) {

	if (p_value_scheme->pp_pointers_value_schemes != NULL) {

		metac_count_t i = 0;

		for (i = 0; i < p_value_scheme->pointers_value_schemes_count; ++i) {

			metac_scheme_put(&p_value_scheme->pp_pointers_value_schemes[i]);
		}

		p_value_scheme->pointers_value_schemes_count = 0;

		free(p_value_scheme->pp_pointers_value_schemes);
		p_value_scheme->pp_pointers_value_schemes = NULL;
	}

	if (p_value_scheme->pp_arrays_value_schemes != NULL) {

		metac_count_t i = 0;

		for (i = 0; i < p_value_scheme->arrays_value_schemes_count; ++i) {

			metac_scheme_put(&p_value_scheme->pp_arrays_value_schemes[i]);
		}

		p_value_scheme->arrays_value_schemes_count = 0;

		free(p_value_scheme->pp_arrays_value_schemes);
		p_value_scheme->pp_arrays_value_schemes = NULL;
	}
}

static int value_scheme_init(
		struct value_scheme *						p_value_scheme,
		struct value_scheme_builder *				p_value_scheme_builder) {

	struct value_scheme_container_value_scheme * _value_scheme;

	p_value_scheme->arrays_value_schemes_count = 0;
	p_value_scheme->pointers_value_schemes_count = 0;

	/*get arrays lengths */
	cds_list_for_each_entry(_value_scheme, &p_value_scheme_builder->container.arrays, list) {
		++p_value_scheme->arrays_value_schemes_count;
	}
	cds_list_for_each_entry(_value_scheme, &p_value_scheme_builder->container.pointers, list) {
		++p_value_scheme->pointers_value_schemes_count;
	}

	/*fill all the data*/
	if (p_value_scheme->arrays_value_schemes_count > 0) {

		metac_count_t i = 0;

		p_value_scheme->pp_arrays_value_schemes = (struct metac_scheme **)calloc(
				p_value_scheme->arrays_value_schemes_count, sizeof(*p_value_scheme->pp_arrays_value_schemes));
		if (p_value_scheme->pp_arrays_value_schemes == NULL) {

			msg_stderr("pp_arrays_value_schemes is NULL\n");
			return (-ENOMEM);
		}

		cds_list_for_each_entry(_value_scheme, &p_value_scheme_builder->container.arrays, list) {

			p_value_scheme->pp_arrays_value_schemes[i] = metac_scheme_get(_value_scheme->p_metac_scheme);
			if (p_value_scheme->pp_arrays_value_schemes[i] == NULL) {

				msg_stderr("metac_scheme_get failed\n");
				return (-ENOMEM);
			}

			++i;
		}
	}

	if (p_value_scheme->pointers_value_schemes_count > 0) {

		metac_count_t i = 0;

		p_value_scheme->pp_pointers_value_schemes = (struct metac_scheme **)calloc(
				p_value_scheme->pointers_value_schemes_count, sizeof(*p_value_scheme->pp_pointers_value_schemes));
		if (p_value_scheme->pp_pointers_value_schemes == NULL) {

			msg_stderr("pp_pointers_value_schemes is NULL\n");
			return (-ENOMEM);
		}

		cds_list_for_each_entry(_value_scheme, &p_value_scheme_builder->container.pointers, list) {

			p_value_scheme->pp_pointers_value_schemes[i] = metac_scheme_get(_value_scheme->p_metac_scheme);
			if (p_value_scheme->pp_pointers_value_schemes[i] == NULL) {

				msg_stderr("metac_scheme_get failed\n");
				return (-ENOMEM);
			}

			++i;
		}
	}

	return 0;
}

static int value_scheme_delete(
		struct value_scheme **						pp_value_scheme) {
	_delete_start_(value_scheme);
	value_scheme_clean(*pp_value_scheme);
	_delete_finish(value_scheme);
	return 0;
}

static struct value_scheme * value_scheme_create(
		struct value_scheme_builder *				p_value_scheme_builder) {
	_create_(value_scheme);

	if (value_scheme_init(
			p_value_scheme,
			p_value_scheme_builder) != 0){

		msg_stderr("value_scheme_init failed\n");

		value_scheme_delete(&p_value_scheme);
		return NULL;
	}

	return p_value_scheme;
}

metac_flag_t metac_scheme_is_value_scheme(
		struct metac_scheme *						p_metac_scheme) {

	if (p_metac_scheme == NULL) {

		msg_stderr("invalid argument\n");
		return -(EINVAL);
	}

	return (p_metac_scheme->p_value_scheme != NULL)?1:0;
}

static int metac_scheme_clean_as_value_scheme(
		struct metac_scheme *						p_metac_scheme) {

	if (metac_scheme_is_value_scheme(p_metac_scheme) != 1) {

		return -(EFAULT);
	}

	return value_scheme_delete(&p_metac_scheme->p_value_scheme);
}

static int value_scheme_builder_finalize(
		struct value_scheme_builder *				p_value_scheme_builder,
		struct metac_scheme *						p_metac_scheme) {

	p_metac_scheme->p_value_scheme = value_scheme_create(p_value_scheme_builder);
	if (p_metac_scheme->p_value_scheme == NULL) {

		msg_stderr("value_scheme_create failed\n");
		return -(EFAULT);
	}

	return 0;
}

static int metac_scheme_init_as_value_scheme(
		struct metac_scheme *						p_metac_scheme,
		struct metac_type *							p_root_type,
		char * 										global_path,
		metac_type_annotation_t *					p_override_annotations,
		struct metac_type *							p_type) {


	struct value_scheme_builder value_scheme_builder;

	value_scheme_builder_init(&value_scheme_builder, p_root_type, p_override_annotations);

	if (metac_scheme_init_as_indexable(
			p_metac_scheme,
			p_root_type,
			global_path,
			"",
			p_override_annotations,
			p_type) != 0) {

		msg_stderr("metac_scheme_init_as_indexable failed\n");

		metac_scheme_clean(p_metac_scheme);
		value_scheme_builder_clean(&value_scheme_builder);
		return (-EFAULT);
	}

	if (value_scheme_builder_process_value_scheme(
			&value_scheme_builder,
			global_path,
			p_metac_scheme) != 0) {

		msg_stddbg("value_scheme_builder_process_value_scheme failed\n");

		metac_scheme_clean(p_metac_scheme);
		value_scheme_builder_clean(&value_scheme_builder);
		return (-EFAULT);
	}

	if (scheduler_run(&value_scheme_builder.scheduler) != 0) {

		msg_stddbg("tasks execution finished with fail\n");

		metac_scheme_clean(p_metac_scheme);
		value_scheme_builder_clean(&value_scheme_builder);
		return (-EFAULT);
	}

	if (value_scheme_builder_finalize(
			&value_scheme_builder,
			p_metac_scheme) != 0) {

		msg_stddbg("value_scheme_builder_finalize failed\n");

		metac_scheme_clean(p_metac_scheme);
		value_scheme_builder_clean(&value_scheme_builder);
		return (-EFAULT);
	}

	value_scheme_builder_clean(&value_scheme_builder);

	return 0;
}

static struct metac_scheme * metac_scheme_create_as_value_scheme(
		struct metac_type *							p_type,
		char * 										global_path,
		struct metac_type *							p_root_type,
		metac_type_annotation_t *					p_override_annotations) {

	_create_(metac_scheme);

	if (metac_refcounter_object_init(
			&p_metac_scheme->refcounter_object,
			&_metac_scheme_refcounter_object_ops,
			NULL) != 0) {

		msg_stderr("metac_refcounter_object_init failed\n");

		metac_scheme_delete(&p_metac_scheme);
		return NULL;
	}

	if (metac_scheme_init_as_value_scheme(
			p_metac_scheme,
			p_root_type,
			global_path,
			p_override_annotations,
			p_type) != 0) {

		msg_stderr("metac_scheme_init_as_value_scheme failed\n");

		metac_scheme_delete(&p_metac_scheme);
		return NULL;
	}

	return p_metac_scheme;
}

/*****************************************************************************/
struct deep_value_scheme_container_pointer_scheme {
	struct cds_list_head							list;

	struct metac_scheme *							p_pointer_metac_scheme;
};

struct deep_value_scheme_container_value_scheme {
	struct cds_list_head							list;

	struct metac_scheme *							p_object_metac_scheme;

	struct cds_list_head							pointers;
};

struct deep_value_scheme_container {
	struct cds_list_head							objects;
};

struct deep_value_scheme_builder {
	struct metac_type *								p_root_type;
	metac_type_annotation_t *						p_override_annotations;

	struct deep_value_scheme_container				container;

	struct scheduler								scheduler;
	struct cds_list_head							executed_tasks;
};

struct deep_value_scheme_builder_value_scheme_task {
	struct scheduler_task							task;
	char * 											global_path;	/*local copy - keep track of it*/

	struct metac_scheme *							p_metac_scheme;
};

/*****************************************************************************/
static int deep_value_scheme_builder_schedule_value_scheme(
		struct deep_value_scheme_builder *			p_deep_value_scheme_builder,
		char *										global_path,
		struct metac_scheme *						p_metac_scheme);
/*****************************************************************************/

static void deep_value_scheme_container_pointer_scheme_clean(
		struct deep_value_scheme_container_pointer_scheme *
													p_deep_value_scheme_container_pointer_scheme) {

	if (p_deep_value_scheme_container_pointer_scheme == NULL) {

		msg_stderr("p_deep_value_scheme_container_pointer_scheme is invalid\n");
		return;
	}

	if (p_deep_value_scheme_container_pointer_scheme->p_pointer_metac_scheme) {

		metac_scheme_put(&p_deep_value_scheme_container_pointer_scheme->p_pointer_metac_scheme);
	}
}

static int deep_value_scheme_container_pointer_scheme_init(
		struct deep_value_scheme_container_pointer_scheme *
													p_deep_value_scheme_container_pointer_scheme,
		struct metac_scheme *						p_pointer_metac_scheme) {

	if (p_deep_value_scheme_container_pointer_scheme == NULL) {

		msg_stderr("p_deep_value_scheme_container_pointer_scheme is invalid\n");
		return (-EINVAL);
	}

	if (p_pointer_metac_scheme == NULL) {

		msg_stderr("Can't init deep_value_scheme_container_pointer_scheme with NULL\n");
		return (-EINVAL);
	}


	p_deep_value_scheme_container_pointer_scheme->p_pointer_metac_scheme = metac_scheme_get(p_pointer_metac_scheme);
	if (p_deep_value_scheme_container_pointer_scheme->p_pointer_metac_scheme == NULL) {

		msg_stderr("metac_scheme_get failed\n");
		deep_value_scheme_container_pointer_scheme_clean(p_deep_value_scheme_container_pointer_scheme);
		return (-EINVAL);
	}

	return 0;
}

static int deep_value_scheme_container_pointer_scheme_delete(
		struct deep_value_scheme_container_pointer_scheme **
													pp_deep_value_scheme_container_pointer_scheme) {

	_delete_start_(deep_value_scheme_container_pointer_scheme);
	deep_value_scheme_container_pointer_scheme_clean(*pp_deep_value_scheme_container_pointer_scheme);
	_delete_finish(deep_value_scheme_container_pointer_scheme);
	return 0;
}

static struct deep_value_scheme_container_pointer_scheme * deep_value_scheme_container_pointer_scheme_create(
		struct metac_scheme *						p_pointer_metac_scheme) {

	_create_(deep_value_scheme_container_pointer_scheme);

	if (deep_value_scheme_container_pointer_scheme_init(
			p_deep_value_scheme_container_pointer_scheme,
			p_pointer_metac_scheme) != 0) {

		deep_value_scheme_container_pointer_scheme_delete(&p_deep_value_scheme_container_pointer_scheme);
		return NULL;
	}

	return p_deep_value_scheme_container_pointer_scheme;
}

/*****************************************************************************/

static void deep_value_scheme_container_value_scheme_clean(
		struct deep_value_scheme_container_value_scheme *
													p_deep_value_scheme_container_value_scheme) {

	struct deep_value_scheme_container_pointer_scheme * p_container_pointer_scheme, * _p_container_pointer_scheme;

	if (p_deep_value_scheme_container_value_scheme == NULL) {

		msg_stderr("p_deep_value_scheme_container_value_scheme is invalid\n");
		return;
	}

	if (p_deep_value_scheme_container_value_scheme->p_object_metac_scheme) {

		metac_scheme_put(&p_deep_value_scheme_container_value_scheme->p_object_metac_scheme);
	}

	cds_list_for_each_entry_safe(p_container_pointer_scheme, _p_container_pointer_scheme, &p_deep_value_scheme_container_value_scheme->pointers, list) {

		cds_list_del(&p_container_pointer_scheme->list);
		deep_value_scheme_container_pointer_scheme_delete(&p_container_pointer_scheme);
	}
}

static int deep_value_scheme_container_value_scheme_add_pointer(
		struct deep_value_scheme_container_value_scheme *
													p_deep_value_scheme_container_value_scheme,
		struct metac_scheme *						p_pointer_metac_scheme) {

	struct deep_value_scheme_container_pointer_scheme * p_container_pointer_scheme;

	if (p_deep_value_scheme_container_value_scheme == NULL) {

		msg_stderr("p_deep_value_scheme_container_value_scheme is invalid\n");
		return (-EINVAL);
	}

	if (p_pointer_metac_scheme == NULL) {

		msg_stderr("Invalid arguments\n");
		return (-EINVAL);
	}

	p_container_pointer_scheme = deep_value_scheme_container_pointer_scheme_create(p_pointer_metac_scheme);
	if (p_container_pointer_scheme == NULL) {

		msg_stderr("deep_value_scheme_container_pointer_scheme_create failed\n");
		return (-EINVAL);
	}

	cds_list_add_tail(
		&p_container_pointer_scheme->list,
		&p_deep_value_scheme_container_value_scheme->pointers);

	return 0;
}

static int deep_value_scheme_container_value_scheme_init(
		struct deep_value_scheme_container_value_scheme *
													p_deep_value_scheme_container_value_scheme,
		struct metac_scheme *						p_pointer_metac_scheme,
		struct metac_scheme *						p_object_metac_scheme) {


	if (p_deep_value_scheme_container_value_scheme == NULL) {

		msg_stderr("p_deep_value_scheme_container_value_scheme is invalid\n");
		return (-EINVAL);
	}

	if (p_pointer_metac_scheme == NULL ||
		p_object_metac_scheme == NULL) {

		msg_stderr("Invalid arguments\n");
		return (-EINVAL);
	}

	CDS_INIT_LIST_HEAD(&p_deep_value_scheme_container_value_scheme->pointers);

	p_deep_value_scheme_container_value_scheme->p_object_metac_scheme = metac_scheme_get(p_object_metac_scheme);
	if (p_deep_value_scheme_container_value_scheme->p_object_metac_scheme == NULL) {

		msg_stderr("metac_scheme_get failed\n");
		deep_value_scheme_container_value_scheme_clean(p_deep_value_scheme_container_value_scheme);
		return (-EINVAL);
	}

	if (deep_value_scheme_container_value_scheme_add_pointer(
			p_deep_value_scheme_container_value_scheme,
			p_pointer_metac_scheme) != 0) {

		msg_stderr("deep_value_scheme_container_value_scheme_add_pointer failed\n");
		deep_value_scheme_container_value_scheme_clean(p_deep_value_scheme_container_value_scheme);
		return (-EINVAL);
	}

	return 0;
}

static int deep_value_scheme_container_value_scheme_delete(
		struct deep_value_scheme_container_value_scheme **
													pp_deep_value_scheme_container_value_scheme) {

	_delete_start_(deep_value_scheme_container_value_scheme);
	deep_value_scheme_container_value_scheme_clean(*pp_deep_value_scheme_container_value_scheme);
	_delete_finish(deep_value_scheme_container_value_scheme);
	return 0;
}

static struct deep_value_scheme_container_value_scheme * deep_value_scheme_container_value_scheme_create(
		struct metac_scheme *						p_pointer_metac_scheme,
		struct metac_scheme *						p_object_metac_scheme) {

	_create_(deep_value_scheme_container_value_scheme);

	if (deep_value_scheme_container_value_scheme_init(
			p_deep_value_scheme_container_value_scheme,
			p_pointer_metac_scheme,
			p_object_metac_scheme) != 0) {

		deep_value_scheme_container_value_scheme_delete(&p_deep_value_scheme_container_value_scheme);
		return NULL;
	}

	return p_deep_value_scheme_container_value_scheme;
}

static struct deep_value_scheme_container_value_scheme * deep_value_scheme_builder_find_object(
		struct deep_value_scheme_builder *			p_deep_value_scheme_builder,
		struct metac_scheme *						p_pointer_metac_scheme,
		struct metac_type *							p_new_object_type) {

	struct deep_value_scheme_container_value_scheme * _value_scheme;
	struct metac_type *	p_new_pointer_type = p_pointer_metac_scheme->p_type;
	struct metac_type_member_info * p_new_pointer_member_info = NULL;

	if (metac_scheme_is_hierarchy_member_scheme(p_pointer_metac_scheme) == 1) {

		p_new_pointer_member_info = p_pointer_metac_scheme->hierarchy_member.p_member_info;
	}

	cds_list_for_each_entry(_value_scheme, &p_deep_value_scheme_builder->container.objects, list) {

		assert(cds_list_empty(&_value_scheme->pointers) == 0);
		if (cds_list_empty(&_value_scheme->pointers)) {
			/*actually we should panic*/
			continue;
		}

		struct metac_scheme *	p_entry_pointer_metac_scheme = cds_list_first_entry(
				&_value_scheme->pointers, struct deep_value_scheme_container_pointer_scheme, list)->p_pointer_metac_scheme;
		struct metac_type *	p_entry_object_type = _value_scheme->p_object_metac_scheme->p_type;
		struct metac_type *	p_entry_pointer_type = p_entry_pointer_metac_scheme->p_type;
		struct metac_type_member_info * p_entry_pointer_member_info = NULL;

		if (metac_scheme_is_hierarchy_member_scheme(p_pointer_metac_scheme) == 1) {

			p_new_pointer_member_info = p_entry_pointer_metac_scheme->hierarchy_member.p_member_info;
		}

		 if (	p_new_object_type == p_entry_object_type &&
				p_new_pointer_type == p_entry_pointer_type &&
				p_new_pointer_member_info == p_entry_pointer_member_info) {

			 return _value_scheme;
		 }
	}

	return NULL;
}

static int deep_value_scheme_builder_add_value_scheme (
		struct deep_value_scheme_builder *			p_deep_value_scheme_builder,
		struct metac_scheme *						p_pointer_metac_scheme,
		struct metac_scheme *						p_object_metac_scheme) {

	struct deep_value_scheme_container_value_scheme * p_deep_value_scheme_container_value_scheme;

	p_deep_value_scheme_container_value_scheme = deep_value_scheme_container_value_scheme_create(
			p_pointer_metac_scheme,
			p_object_metac_scheme);
	if (p_deep_value_scheme_container_value_scheme == NULL) {

		msg_stddbg("wasn't able to create p_deep_value_scheme_container_value_scheme\n");
		return (-EFAULT);
	}

	cds_list_add_tail(
		&p_deep_value_scheme_container_value_scheme->list,
		&p_deep_value_scheme_builder->container.objects);

	return 0;
}

static struct metac_scheme * deep_value_scheme_builder_update_record_or_create_new_and_schedule(
		struct deep_value_scheme_builder *			p_deep_value_scheme_builder,
		char *										global_path,
		struct metac_scheme *						p_metac_scheme,
		struct metac_type *							p_type) {

	struct metac_scheme * p_result;
	struct deep_value_scheme_container_value_scheme * p_container_value_scheme;

	p_container_value_scheme = deep_value_scheme_builder_find_object(
			p_deep_value_scheme_builder,
			p_metac_scheme,
			p_type);

	if (p_container_value_scheme != NULL) {

		p_result = metac_scheme_get(
				p_container_value_scheme->p_object_metac_scheme);

		if (p_result == NULL) {

			msg_stderr("Can't build target_global_path for %s\n", global_path);
			return NULL;
		}

		/* update list of pointers for this object */
		if (deep_value_scheme_container_value_scheme_add_pointer (
				p_container_value_scheme,
				p_metac_scheme) != 0) {

			msg_stderr("Can't build target_global_path for %s\n", global_path);

			metac_scheme_put(&p_result);
			return NULL;
		}

		return p_result;
	}

	p_result = metac_scheme_create_as_value_scheme(
			p_type,
			global_path,
			p_deep_value_scheme_builder->p_root_type,
			p_deep_value_scheme_builder->p_override_annotations);
	if (p_result == NULL) {

		msg_stderr("metac_scheme_create_as_value_scheme failed for %s\n", global_path);
		return NULL;
	}

	if (deep_value_scheme_builder_add_value_scheme(
			p_deep_value_scheme_builder,
			p_metac_scheme,
			p_result) != 0) {

		msg_stderr("deep_value_scheme_builder_add_value_scheme failed for %s\n", global_path);
		metac_scheme_put(&p_result);
		return NULL;
	}

	if (deep_value_scheme_builder_schedule_value_scheme(
			p_deep_value_scheme_builder,
			global_path,
			p_result) != 0) {

		msg_stderr("deep_value_scheme_builder_schedule_value_scheme failed %s\n", global_path);
		metac_scheme_put(&p_result);
		return NULL;
	}

	return p_result;
}

static int _filter_pointer(
		struct metac_type *							p_metac_type) {

	int res = 0;

	struct metac_type * p_actual_metac_type;
	struct metac_type * p_actual_target_metac_type;

	if (p_metac_type == NULL) {

		return -EINVAL;
	}

	p_actual_metac_type = metac_type_get_actual_type(p_metac_type);

	if (p_actual_metac_type == NULL) {

		return -EFAULT;
	}

	if (p_actual_metac_type->id != DW_TAG_pointer_type) {
		res = 1;
	} else if (p_actual_metac_type->declaration == 1) {
		res = 1;
	} else if (p_actual_metac_type->pointer_type_info.type == NULL) {
		res = 1;
	} else if (p_actual_metac_type->pointer_type_info.type->declaration == 1) {
		res = 1;
	} else {
		p_actual_target_metac_type = metac_type_get_actual_type(p_actual_metac_type->pointer_type_info.type);
	}
	if (p_actual_target_metac_type == NULL) {
		res = 1;
	} else if (p_actual_target_metac_type->id == DW_TAG_subprogram) {
		res = 1;
	}

	if (p_actual_target_metac_type != NULL) {
		metac_type_put(&p_actual_target_metac_type);
	}
	if (p_actual_metac_type != NULL) {
		metac_type_put(&p_actual_metac_type);
	}

	return res;
}

static int deep_value_scheme_builder_process_pointer(
		struct deep_value_scheme_builder *			p_deep_value_scheme_builder,
		char *										global_path,
		struct metac_scheme *						p_metac_scheme) {

	int i = 0;
	struct deep_value_scheme_container_value_scheme * p_value_scheme;

	assert(p_metac_scheme->p_actual_type->id == DW_TAG_pointer_type);

	/* main pointer */
	if (p_metac_scheme->p_actual_type->pointer_type_info.type != NULL &&
		_filter_pointer(p_metac_scheme->p_actual_type) == 0) {

		char * target_global_path = alloc_sptrinf("%s[]", global_path);

		if (target_global_path == NULL) {

			msg_stderr("Can't build target_global_path for %s\n", global_path);
			return (-EFAULT);
		}

		p_metac_scheme->pointer.p_default_child_value_scheme = deep_value_scheme_builder_update_record_or_create_new_and_schedule(
				p_deep_value_scheme_builder,
				target_global_path,
				p_metac_scheme,
				p_metac_scheme->p_actual_type->pointer_type_info.type);

		if (p_metac_scheme->pointer.p_default_child_value_scheme == NULL) {

			msg_stderr("deep_value_scheme_builder_update_record_or_create_new_and_schedule failed for %s\n", global_path);
			free(target_global_path);
			return (-EFAULT);
		}

		free(target_global_path);
	}

	for (i = 0; i < p_metac_scheme->pointer.generic_cast.types_count; ++i) {

		char * target_global_path;

		if (_filter_pointer(p_metac_scheme->p_actual_type) != 0) {
			continue;
		}

		target_global_path = alloc_sptrinf(
						"generic_cast<*%s>(%s)[]",
						p_metac_scheme->pointer.generic_cast.p_types[i].p_type->name,
						global_path);
		if (target_global_path == NULL) {

			msg_stderr("Can't build target_global_path for %s generic_cast %d\n", global_path, i);
			return (-EFAULT);
		}

		p_metac_scheme->pointer.generic_cast.p_types[i].p_child_value_scheme = deep_value_scheme_builder_update_record_or_create_new_and_schedule(
				p_deep_value_scheme_builder,
				target_global_path,
				p_metac_scheme,
				p_metac_scheme->pointer.generic_cast.p_types[i].p_type);

		if (p_metac_scheme->pointer.generic_cast.p_types[i].p_child_value_scheme == NULL) {

			msg_stderr("deep_value_scheme_builder_schedule_value_scheme failed for %s generic_cast %d\n", global_path, i);

			free(target_global_path);
			return (-EFAULT);
		}

		free(target_global_path);
	}

	return 0;
}

static int deep_value_scheme_unprocess_pointer(
		struct metac_scheme *						p_metac_scheme) {

	int i = 0;

	assert(p_metac_scheme->p_actual_type->id == DW_TAG_pointer_type);

	for (i = 0; i < p_metac_scheme->pointer.generic_cast.types_count; ++i) {
		if (p_metac_scheme->pointer.generic_cast.p_types[i].p_child_value_scheme != NULL) {
			metac_scheme_put(&p_metac_scheme->pointer.generic_cast.p_types[i].p_child_value_scheme);
		}
	}

	if (p_metac_scheme->pointer.p_default_child_value_scheme != NULL) {
		metac_scheme_put(&p_metac_scheme->pointer.p_default_child_value_scheme);
	}

	return 0;
}

static int deep_value_scheme_builder_process_value_scheme(
		struct deep_value_scheme_builder *			p_deep_value_scheme_builder,
		char *										global_path,
		struct metac_scheme *						p_metac_scheme) {

	metac_count_t i;

	if (metac_scheme_is_value_scheme(p_metac_scheme) != 1) {
		return 0;
	}

	for (i = 0; i < p_metac_scheme->p_value_scheme->pointers_value_schemes_count; ++i) {

		char * target_global_path;

		target_global_path = build_member_path(
				global_path,
				p_metac_scheme->p_value_scheme->pp_pointers_value_schemes[i]->path_within_value);
		if (target_global_path == NULL) {

			msg_stderr("Can't build target_global_path for %s pointer %d\n", global_path, i);

			return (-ENOMEM);
		}

		msg_stddbg("%s\n", target_global_path);

		if (deep_value_scheme_builder_process_pointer(
				p_deep_value_scheme_builder,
				target_global_path,
				p_metac_scheme->p_value_scheme->pp_pointers_value_schemes[i]) != 0) {

			msg_stderr("deep_value_scheme_builder_process_pointer failed for %s pointer %d\n", global_path, i);

			free(target_global_path);
			return (-EFAULT);
		}

		free(target_global_path);
	}

	return 0;
}

static int deep_value_scheme_builder_process_value_scheme_fn(
	struct scheduler * 								p_engine,
	struct scheduler_task * 						p_task,
	int 											error_flag) {

	struct deep_value_scheme_builder * p_deep_value_scheme_builder =
		cds_list_entry(p_engine, struct deep_value_scheme_builder, scheduler);

	struct deep_value_scheme_builder_value_scheme_task * p_deep_value_scheme_builder_value_scheme_task =
		cds_list_entry(p_task, struct deep_value_scheme_builder_value_scheme_task, task);

	/*TODO: consider deletion of the task after execution or adding them to the task pool */
	cds_list_add_tail(&p_task->list, &p_deep_value_scheme_builder->executed_tasks);

	if (error_flag != 0) {

		return (-EALREADY);
	}

	return deep_value_scheme_builder_process_value_scheme(
			p_deep_value_scheme_builder,
			p_deep_value_scheme_builder_value_scheme_task->global_path,
			p_deep_value_scheme_builder_value_scheme_task->p_metac_scheme);
}


static int deep_value_scheme_builder_value_scheme_task_delete(
		struct deep_value_scheme_builder_value_scheme_task **
													pp_deep_value_scheme_builder_value_scheme_task) {

	_delete_start_(deep_value_scheme_builder_value_scheme_task);

	if ((*pp_deep_value_scheme_builder_value_scheme_task)->global_path) {

		free((*pp_deep_value_scheme_builder_value_scheme_task)->global_path);
		(*pp_deep_value_scheme_builder_value_scheme_task)->global_path = NULL;
	}

	if ((*pp_deep_value_scheme_builder_value_scheme_task)->p_metac_scheme != NULL) {

		metac_scheme_put(&(*pp_deep_value_scheme_builder_value_scheme_task)->p_metac_scheme);
	}

	_delete_finish(deep_value_scheme_builder_value_scheme_task);
	return 0;
}

static struct deep_value_scheme_builder_value_scheme_task * deep_value_scheme_builder_value_scheme_task_create(
		scheduler_task_fn_t 						fn,
		char *										global_path,
		struct metac_scheme *						p_metac_scheme) {

	_create_(deep_value_scheme_builder_value_scheme_task);

	p_deep_value_scheme_builder_value_scheme_task->task.fn = fn;
	p_deep_value_scheme_builder_value_scheme_task->global_path = strdup(global_path);
	p_deep_value_scheme_builder_value_scheme_task->p_metac_scheme = metac_scheme_get(p_metac_scheme);

	if (p_deep_value_scheme_builder_value_scheme_task->global_path == NULL ||
		p_deep_value_scheme_builder_value_scheme_task->p_metac_scheme == NULL) {

		msg_stderr("wasn't able to duplicate global_path or p_metac_scheme\n");
		deep_value_scheme_builder_value_scheme_task_delete(&p_deep_value_scheme_builder_value_scheme_task);
		return NULL;
	}
	return p_deep_value_scheme_builder_value_scheme_task;
}

static int deep_value_scheme_builder_schedule_value_scheme(
		struct deep_value_scheme_builder *			p_deep_value_scheme_builder,
		char *										global_path,
		struct metac_scheme *						p_metac_scheme) {

	struct deep_value_scheme_builder_value_scheme_task * p_task = deep_value_scheme_builder_value_scheme_task_create(
		deep_value_scheme_builder_process_value_scheme_fn,
		global_path,
		p_metac_scheme);
	if (p_task == NULL) {

		msg_stderr("wasn't able to create a task\n");
		return -(EFAULT);
	}

	if (scheduler_add_task_to_front(
		&p_deep_value_scheme_builder->scheduler,
		&p_task->task) != 0) {

		msg_stderr("wasn't able to schedule task\n");
		deep_value_scheme_builder_value_scheme_task_delete(&p_task);
		return -(EFAULT);
	}

	return 0;
}

static void deep_value_scheme_container_clean(
		struct deep_value_scheme_container *		p_deep_value_scheme_container) {

	struct deep_value_scheme_container_value_scheme * value_scheme, * _value_scheme;

	cds_list_for_each_entry_safe(value_scheme, _value_scheme, &p_deep_value_scheme_container->objects, list) {

		cds_list_del(&value_scheme->list);
		deep_value_scheme_container_value_scheme_delete(&value_scheme);
	}
}

static int deep_value_scheme_container_init(
		struct deep_value_scheme_container *		p_deep_value_scheme_container) {

	CDS_INIT_LIST_HEAD(&p_deep_value_scheme_container->objects);

	return 0;
}

static void deep_value_scheme_builder_clean(
		struct deep_value_scheme_builder *			p_deep_value_scheme_builder) {
	struct deep_value_scheme_builder_value_scheme_task * p_task, *_p_task;

	scheduler_clean(&p_deep_value_scheme_builder->scheduler);

	cds_list_for_each_entry_safe(p_task, _p_task, &p_deep_value_scheme_builder->executed_tasks, task.list) {

		cds_list_del(&p_task->task.list);
		deep_value_scheme_builder_value_scheme_task_delete(&p_task);
	}

	deep_value_scheme_container_clean(&p_deep_value_scheme_builder->container);

	p_deep_value_scheme_builder->p_root_type = NULL;
	p_deep_value_scheme_builder->p_override_annotations = NULL;
}

static int deep_value_scheme_builder_init(
		struct deep_value_scheme_builder *			p_deep_value_scheme_builder,
		struct metac_type *							p_root_type,
		metac_type_annotation_t *					p_override_annotations) {

	p_deep_value_scheme_builder->p_root_type = p_root_type;
	p_deep_value_scheme_builder->p_override_annotations = p_override_annotations;
	deep_value_scheme_container_init(&p_deep_value_scheme_builder->container);

	CDS_INIT_LIST_HEAD(&p_deep_value_scheme_builder->executed_tasks);

	scheduler_init(&p_deep_value_scheme_builder->scheduler);

	return 0;
}

static void deep_value_scheme_clean(
		struct deep_value_scheme *					p_deep_value_scheme) {

	if (p_deep_value_scheme->p_value_schemes != NULL) {

		metac_count_t i = 0;

		for (i = 0; i < p_deep_value_scheme->value_schemes_count; ++i) {

			if (p_deep_value_scheme->p_value_schemes[i].pp_pointers != NULL) {

				metac_count_t j = 0;

				for (j = 0; j < p_deep_value_scheme->p_value_schemes[i].pointers_count; ++j) {

					if (p_deep_value_scheme->p_value_schemes[i].pp_pointers[j] != NULL) {

						deep_value_scheme_unprocess_pointer(p_deep_value_scheme->p_value_schemes[i].pp_pointers[j]);
						metac_scheme_put(&p_deep_value_scheme->p_value_schemes[i].pp_pointers[j]);
					}
				}

				p_deep_value_scheme->p_value_schemes[i].pointers_count = 0;

				free(p_deep_value_scheme->p_value_schemes[i].pp_pointers);
				p_deep_value_scheme->p_value_schemes[i].pp_pointers = NULL;
			}
		}

		/*don't put it to the previous cycle, because it may cause long recursive calls and we want to avoid that for big data structures*/
		for (i = 0; i < p_deep_value_scheme->value_schemes_count; ++i) {

			if (p_deep_value_scheme->p_value_schemes[i].p_value_scheme != NULL) {
				metac_scheme_put(&p_deep_value_scheme->p_value_schemes[i].p_value_scheme);
			}

		}

		p_deep_value_scheme->value_schemes_count = 0;

		free(p_deep_value_scheme->p_value_schemes);
		p_deep_value_scheme->p_value_schemes = NULL;
	}
}

static int deep_value_scheme_init(
		struct deep_value_scheme *					p_deep_value_scheme,
		struct deep_value_scheme_builder *			p_deep_value_scheme_builder) {

	struct deep_value_scheme_container_value_scheme * _value_scheme;
	struct deep_value_scheme_container_pointer_scheme * _pointer_scheme;

	p_deep_value_scheme->value_schemes_count = 0;

	/*get arrays lengths */
	cds_list_for_each_entry(_value_scheme, &p_deep_value_scheme_builder->container.objects, list) {
		++p_deep_value_scheme->value_schemes_count;
	}

	/*fill all the data*/
	if (p_deep_value_scheme->value_schemes_count > 0) {

		metac_count_t i = 0;
		metac_count_t j = 0;

		p_deep_value_scheme->p_value_schemes = (struct deep_value_scheme_value_scheme *)calloc(
				p_deep_value_scheme->value_schemes_count, sizeof(*p_deep_value_scheme->p_value_schemes));
		if (p_deep_value_scheme->p_value_schemes == NULL) {

			msg_stderr("p_objects is NULL\n");
			return (-ENOMEM);
		}

		cds_list_for_each_entry(_value_scheme, &p_deep_value_scheme_builder->container.objects, list) {

			p_deep_value_scheme->p_value_schemes[i].p_value_scheme = metac_scheme_get(_value_scheme->p_object_metac_scheme);
			if (p_deep_value_scheme->p_value_schemes[i].p_value_scheme == NULL) {

				msg_stderr("metac_scheme_get failed\n");
				return (-ENOMEM);
			}

			/* init pointers */
			p_deep_value_scheme->p_value_schemes[i].pointers_count = 0;

			/*get arrays lengths */
			cds_list_for_each_entry(_pointer_scheme, &_value_scheme->pointers, list) {
				++p_deep_value_scheme->p_value_schemes[i].pointers_count;
			}

			assert(p_deep_value_scheme->p_value_schemes[i].pointers_count > 0);

			if (p_deep_value_scheme->p_value_schemes[i].pointers_count > 0) {

				p_deep_value_scheme->p_value_schemes[i].pp_pointers = (struct metac_scheme **)calloc(
						p_deep_value_scheme->p_value_schemes[i].pointers_count, sizeof(*p_deep_value_scheme->p_value_schemes[i].pp_pointers));
				if (p_deep_value_scheme->p_value_schemes[i].pp_pointers == NULL) {

					msg_stderr("p_pointers is NULL\n");
					return (-ENOMEM);
				}

				j = 0;
				cds_list_for_each_entry(_pointer_scheme, &_value_scheme->pointers, list) {
					p_deep_value_scheme->p_value_schemes[i].pp_pointers[j] = metac_scheme_get(_pointer_scheme->p_pointer_metac_scheme);

					if (p_deep_value_scheme->p_value_schemes[i].pp_pointers[j] == NULL) {

						msg_stderr("metac_scheme_get failed\n");
						return (-ENOMEM);
					}

					++j;
				}
			}

			++i;
		}
	}

	return 0;
}

static int deep_value_scheme_delete(
		struct deep_value_scheme **					pp_deep_value_scheme) {
	_delete_start_(deep_value_scheme);
	deep_value_scheme_clean(*pp_deep_value_scheme);
	_delete_finish(deep_value_scheme);
	return 0;
}

static struct deep_value_scheme * deep_value_scheme_create(
		struct deep_value_scheme_builder *			p_deep_value_scheme_builder) {
	_create_(deep_value_scheme);

	if (deep_value_scheme_init(
			p_deep_value_scheme,
			p_deep_value_scheme_builder) != 0){

		msg_stderr("deep_value_scheme_init failed\n");

		deep_value_scheme_delete(&p_deep_value_scheme);
		return NULL;
	}

	return p_deep_value_scheme;
}

metac_flag_t metac_scheme_is_deep_value_scheme(
		struct metac_scheme *						p_metac_scheme) {

	if (metac_scheme_is_value_scheme(p_metac_scheme) != 1) {

		msg_stderr("metac_scheme_is_value_scheme isn't true\n");
		return -(EINVAL);
	}

	return (p_metac_scheme->p_value_scheme->p_deep_value_scheme != NULL)?1:0;
}


static int metac_scheme_clean_as_deep_value_scheme(
		struct metac_scheme *						p_metac_scheme) {


	if (metac_scheme_is_deep_value_scheme(p_metac_scheme) != 1) {

		return -(EFAULT);
	}

	return deep_value_scheme_delete(&p_metac_scheme->p_value_scheme->p_deep_value_scheme);
}

static int deep_value_scheme_builder_finalize(
		struct deep_value_scheme_builder *			p_deep_value_scheme_builder,
		struct metac_scheme *						p_metac_scheme) {

	assert(p_metac_scheme);
	assert(p_metac_scheme->p_value_scheme);

	p_metac_scheme->p_value_scheme->p_deep_value_scheme = deep_value_scheme_create(p_deep_value_scheme_builder);
	if (p_metac_scheme->p_value_scheme->p_deep_value_scheme == NULL) {

		msg_stderr("value_scheme_create failed\n");
		return -(EFAULT);
	}

	return 0;
}

static int metac_scheme_init_as_deep_value_scheme(
		struct metac_scheme *						p_metac_scheme,
		struct metac_type *							p_root_type,
		char * 										global_path,
		metac_type_annotation_t *					p_override_annotations,
		struct metac_type *							p_type) {


	struct deep_value_scheme_builder deep_value_scheme_builder;

	deep_value_scheme_builder_init(&deep_value_scheme_builder, p_root_type, p_override_annotations);

	if (metac_scheme_init_as_value_scheme(
			p_metac_scheme,
			p_root_type,
			global_path,
			p_override_annotations,
			p_type) != 0) {

		msg_stderr("metac_scheme_init_as_value_scheme failed\n");

		metac_scheme_clean(p_metac_scheme);
		deep_value_scheme_builder_clean(&deep_value_scheme_builder);
		return (-EFAULT);
	}

	if (deep_value_scheme_builder_process_value_scheme(
			&deep_value_scheme_builder,
			global_path,
			p_metac_scheme) != 0) {

		msg_stddbg("deep_value_scheme_builder_process_value_scheme failed\n");

		metac_scheme_clean(p_metac_scheme);
		deep_value_scheme_builder_clean(&deep_value_scheme_builder);
		return (-EFAULT);
	}

	if (scheduler_run(&deep_value_scheme_builder.scheduler) != 0) {

		msg_stddbg("tasks execution finished with fail\n");

		metac_scheme_clean(p_metac_scheme);
		deep_value_scheme_builder_clean(&deep_value_scheme_builder);
		return (-EFAULT);
	}

	if (deep_value_scheme_builder_finalize(
			&deep_value_scheme_builder,
			p_metac_scheme) != 0) {

		msg_stddbg("deep_value_scheme_builder_finalize failed\n");

		metac_scheme_clean(p_metac_scheme);
		deep_value_scheme_builder_clean(&deep_value_scheme_builder);
		return (-EFAULT);
	}

	deep_value_scheme_builder_clean(&deep_value_scheme_builder);

	return 0;
}

static struct metac_scheme * metac_scheme_create_as_deep_value_scheme(
		struct metac_type *							p_type,
		char * 										global_path,
		struct metac_type *							p_root_type,
		metac_type_annotation_t *					p_override_annotations) {

	_create_(metac_scheme);

	if (metac_refcounter_object_init(
			&p_metac_scheme->refcounter_object,
			&_metac_scheme_refcounter_object_ops,
			NULL) != 0) {

		msg_stderr("metac_refcounter_object_init failed\n");

		metac_scheme_delete(&p_metac_scheme);
		return NULL;
	}

	if (metac_scheme_init_as_deep_value_scheme(
			p_metac_scheme,
			p_root_type,
			global_path,
			p_override_annotations,
			p_type) != 0) {

		msg_stderr("metac_scheme_init_as_deep_value_scheme failed\n");

		metac_scheme_delete(&p_metac_scheme);
		return NULL;
	}

	return p_metac_scheme;
}
/*****************************************************************************/
static int metac_scheme_clean(
		struct metac_scheme *						p_value_scheme) {

	int res = 0;

	if (p_value_scheme == NULL) {

		msg_stderr("Invalid argument\n");
		return -EINVAL;
	}

	if (metac_scheme_is_value_scheme(p_value_scheme) == 1) {

		if (metac_scheme_is_deep_value_scheme(p_value_scheme) == 1) {

			res = metac_scheme_clean_as_deep_value_scheme(p_value_scheme);
		}

		res = metac_scheme_clean_as_value_scheme(p_value_scheme);

	}

	if (metac_scheme_is_hierarchy_member_scheme(p_value_scheme) == 1) {

		res = metac_scheme_clean_as_hierarchy_member_scheme(p_value_scheme);

	} else if (metac_scheme_is_hierarchy_top_scheme(p_value_scheme) == 1) {

		res = metac_scheme_clean_as_hierarchy_top_scheme(p_value_scheme);

	} else {

		assert(metac_scheme_is_indexable(p_value_scheme) == 1);

		if (p_value_scheme->p_actual_type != NULL) {

			switch(p_value_scheme->p_actual_type->id) {
			case DW_TAG_structure_type:
			case DW_TAG_union_type:
				res = metac_scheme_clean_as_hierarchy_top_scheme(p_value_scheme);
				break;
			case DW_TAG_array_type:
				scheme_with_array_clean(&p_value_scheme->array);
				break;
			case DW_TAG_pointer_type:
				scheme_with_pointer_clean(&p_value_scheme->pointer);
				break;
			}
		}
	}

	if (p_value_scheme->path_within_value != NULL) {
		free(p_value_scheme->path_within_value);
		p_value_scheme->path_within_value = NULL;
	}
	p_value_scheme->byte_size = 0;

	if (p_value_scheme->p_actual_type != NULL) {

		metac_type_put(&p_value_scheme->p_actual_type);
	}

	if (p_value_scheme->p_type != NULL) {

		metac_type_put(&p_value_scheme->p_type);
	}

	return res;
}

struct metac_scheme * metac_scheme_create(
		struct metac_type *							p_type,
		metac_type_annotation_t *					p_override_annotations,
		metac_flag_t								go_deep) {

	if (go_deep == 0) {
		return metac_scheme_create_as_value_scheme(
				p_type,
				"",
				p_type,
				p_override_annotations);
	}

	return metac_scheme_create_as_deep_value_scheme(
			p_type,
			"",
			p_type,
			p_override_annotations);
}


