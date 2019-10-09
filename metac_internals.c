/*
 * metac_internals.c
 *
 *  Created on: Jan 23, 2019
 *      Author: mralex
 */

#define METAC_DEBUG_ENABLE

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
struct precompile_type_container_element_type {
	struct cds_list_head							list;

	struct element_type *							p_element_type;
	struct element_type_hierarchy_member *			p_from_member;		/* info about member that contained this type*/
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
	struct element_type_hierarchy_member *			p_from_member;
};
/*****************************************************************************/
/*scheduling prototypes*/
int precompile_type_builder_schedule_element_type_from_array(
		struct precompile_type_builder *			p_precompile_type_builder,
		char * 										global_path,
		struct metac_type *							p_type,
		struct element_type_hierarchy_member *		p_from_member/*can be NULL*/);
int precompile_type_builder_schedule_element_type_from_pointer(
		struct precompile_type_builder *			p_precompile_type_builder,
		char * 										global_path,
		struct metac_type *							p_type,
		struct element_type_hierarchy_member *		p_from_member/*can be NULL*/);
/*****************************************************************************/
struct discriminator * create_discriminator(
		struct discriminator *						p_previous_discriminator,
		metac_discriminator_value_t					previous_expected_discriminator_value,
		metac_cb_discriminator_t					discriminator_cb,
		void *										discriminator_cb_context) {
	_create_(discriminator);

	if (p_previous_discriminator != NULL) {	/*copy precondition*/
		p_discriminator->precondition.p_discriminator = p_previous_discriminator;
		p_discriminator->precondition.expected_discriminator_value = previous_expected_discriminator_value;
	}

	p_discriminator->discriminator_cb = discriminator_cb;
	p_discriminator->discriminator_cb_context = discriminator_cb_context;

	return p_discriminator;
}
int delete_element_type(
		struct element_type **						pp_element_type) {
	_delete_(element_type);
	return 0;
}
static int init_element_type_array(
		struct precompile_type_builder *			p_precompile_type_builder,
		char * 										global_path,
		struct element_type *						p_element_type) {
	const metac_type_annotation_t * p_annotation;
	assert(	p_element_type && 
			p_element_type->actual_type &&
			p_element_type->actual_type->id == DW_TAG_array_type);
	/* todo: check if it's flexible */
	p_annotation = metac_type_annotation(p_precompile_type_builder->p_root_type, global_path, p_precompile_type_builder->p_override_annotations);

	if (p_element_type->array.array_elements_count.cb != NULL) {
		p_element_type->array.array_elements_count.cb = p_annotation->value->array_elements_count.cb;
		p_element_type->array.array_elements_count.data = p_annotation->value->array_elements_count.data;
	}
	if (p_annotation->value->generic_cast.cb != NULL) {
		msg_stderr("Annotations for array with global path %s defines generic_cast\n", global_path);
	}
	if (p_annotation->value->discriminator.cb != NULL) {
		msg_stderr("Annotations for array with global path %s defines discriminator\n", global_path);
	}
	return 0;
}
static int init_element_type_pointer(
		struct precompile_type_builder *			p_precompile_type_builder,
		char * 										global_path,
		struct element_type *						p_element_type) {
	const metac_type_annotation_t * p_annotation;
	assert(	p_element_type && 
			p_element_type->actual_type &&
			p_element_type->actual_type->id == DW_TAG_pointer_type);
	/* todo: based on the actual_type of the ptr different defaults can be selected */
	p_annotation = metac_type_annotation(p_precompile_type_builder->p_root_type, global_path, p_precompile_type_builder->p_override_annotations);

	if (p_element_type->pointer.generic_cast.cb != NULL) {
		p_element_type->pointer.generic_cast.cb = p_annotation->value->generic_cast.cb;
		p_element_type->pointer.generic_cast.data = p_annotation->value->generic_cast.data;
		p_element_type->pointer.generic_cast.types = p_annotation->value->generic_cast.types;
	}
	if (p_element_type->pointer.array_elements_count.cb != NULL) {
		p_element_type->pointer.array_elements_count.cb = p_annotation->value->array_elements_count.cb;
		p_element_type->pointer.array_elements_count.data = p_annotation->value->array_elements_count.data;
	}
	if (p_annotation->value->discriminator.cb != NULL) {
		msg_stderr("Annotations for pointer with global path %s defines discriminator\n", global_path);
	}
	return 0;
}
static int element_type_hierarhy_top(
		struct precompile_type_builder *			p_precompile_type_builder,
		char * 										global_path,
		struct element_type *						p_element_type) {
	const metac_type_annotation_t * p_annotation;
	assert(	p_element_type && 
			p_element_type->actual_type && (
				p_element_type->actual_type->id == DW_TAG_structure_type ||
				p_element_type->actual_type->id == DW_TAG_union_type
			));
	/*
		todo: create hierarhy_top builder (like element_type builder)
		finalize element_type with needed number of members and discriminators
		clear the builder
	*/
	return 0;
}
/*TODO: make create without args. move the content to a special func */
struct element_type * create_element_type(
		struct precompile_type_builder *			p_precompile_type_builder,
		char * 										global_path,
		struct metac_type *							p_type) {
	int res;
	_create_(element_type);
	p_element_type->type = p_type;
	p_element_type->actual_type = metac_type_actual_type(p_type);
	p_element_type->byte_size = metac_type_byte_size(p_type);
	
	switch(p_element_type->actual_type->id) {
	case DW_TAG_array_type:
		res = init_element_type_array(p_precompile_type_builder, global_path, p_element_type);
		break;
	case DW_TAG_pointer_type:
		res = init_element_type_pointer(p_precompile_type_builder, global_path, p_element_type);
		break;
	case DW_TAG_structure_type:
	case DW_TAG_union_type:
		res = element_type_hierarhy_top(p_precompile_type_builder, global_path, p_element_type);
		break;
	}
	if (res != 0) {
		msg_stddbg("initialization failed\n");
		delete_element_type(&p_element_type);
		return NULL;
	}
	return p_element_type;
}
int precompile_type_container_element_type_init(
		struct precompile_type_container_element_type *
													p_precompile_type_container_element_type,
	struct element_type *							p_element_type,
	struct element_type_hierarchy_member *			p_from_member) {
	if (p_element_type == NULL) {
		msg_stderr("Can't init precomple_type_container_element_type with NULL p_element_type \n");
		return (-EINVAL);
	}
	p_precompile_type_container_element_type->p_from_member = p_from_member;
	p_precompile_type_container_element_type->p_element_type = p_element_type;
	return 0;
}
void precompile_type_container_element_type_clean(
		struct precompile_type_container_element_type *
													p_precompile_type_container_element_type,
		metac_flag									keep_data
		) {
	if (keep_data == 0) {
		delete_element_type(&p_precompile_type_container_element_type->p_element_type);
	}
	p_precompile_type_container_element_type->p_element_type = NULL;
	p_precompile_type_container_element_type->p_from_member = NULL;
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
		precompile_type_container_element_type_clean(_element_type, keep_data);
		free(_element_type);
	}
	cds_list_for_each_entry_safe(_element_type, __element_type, &p_precompile_type_container->arrays_element_type_list, list) {
		cds_list_del(&_element_type->list);
		precompile_type_container_element_type_clean(_element_type, keep_data);
		free(_element_type);
	}
}
static struct precompile_type_container_element_type * precompile_type_builder_element_type_create(
		struct precompile_type_builder *			p_precompile_type_builder,
		char * 										global_path,
		struct metac_type *							p_type,
		struct element_type_hierarchy_member *		p_from_member) {
	int res;
	_create_(precompile_type_container_element_type);
	msg_stddbg("precompile_type_container_add_element_type\n");
	res = precompile_type_container_element_type_init(
		p_precompile_type_container_element_type,
		create_element_type(
			p_precompile_type_builder,
			global_path,
			p_type),
		p_from_member);
	if (res != 0) {
			msg_stddbg("precompile_type_container_element_type_init returned error - deleting\n");
			free(p_precompile_type_container_element_type);
			return NULL;		
	}
	/* TODO: generate new global_path schedule kids if they're pointers or arrays */
	switch(p_precompile_type_container_element_type->p_element_type->actual_type->id) {
	case DW_TAG_array_type:
		/*
			just add [] to global_path and schedule for array_type_info->type 
		*/
		break;
	case DW_TAG_pointer_type:
		/*
			for default type add [] to global_path and schedule for pointer_type_info->type if it's not NULL
			also generate <generic_cast(type)> for every type in p_element_type->pointer.generic_cast.types
		*/
		break;
	case DW_TAG_structure_type:
	case DW_TAG_union_type:
		/*
			scan via all members in hierarhy_top.members, look for arrays and pointers,
			generate paths appropriatly with member.path_within_hierarchy and [] at the end and 
			schedule all needed types (don't forget about pointers p_element_type->pointer.generic_cast.types)
		*/
		break;
	}
	msg_stddbg("precompile_type_container_add_element_type exit\n");
	return p_precompile_type_container_element_type;
}
int precompile_type_builder_add_element_type_from_array(
		struct precompile_type_builder *			p_precompile_type_builder,
		char * 										global_path,
		struct metac_type *							p_type,
		struct element_type_hierarchy_member *		p_from_member/*can be NULL*/) {
	struct precompile_type_container_element_type * p_precompile_type_container_element_type;
	/* create */
	p_precompile_type_container_element_type = precompile_type_builder_element_type_create(
			p_precompile_type_builder,
			global_path,
			p_type,
			p_from_member);
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
		struct element_type_hierarchy_member *		p_from_member/*can be NULL*/) {
	struct precompile_type_container_element_type * p_precompile_type_container_element_type;
	/* try to find this type */
	cds_list_for_each_entry(
		p_precompile_type_container_element_type,
		&p_precompile_type_builder->container.pointers_element_type_list,
		list) {
		if (p_precompile_type_container_element_type->p_element_type->type == p_type &&
			p_precompile_type_container_element_type->p_from_member == p_from_member) {
			msg_stddbg("found %p\n", p_precompile_type_container_element_type);
			return 0;
		}
	}
	/* create */
	p_precompile_type_container_element_type = precompile_type_builder_element_type_create(
			p_precompile_type_builder,
			global_path,
			p_type,
			p_from_member);
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
static int precompile_type_builder_element_type_from_array_task_fn(
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
		p_precompile_type_builder_element_type_task->p_from_member);
}
static int precompile_type_builder_element_type_from_pointer_task_fn(
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
		p_precompile_type_builder_element_type_task->p_from_member);
}
int precompile_type_builder_element_type_task_delete(
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
static struct precompile_type_builder_element_type_task * precompile_type_builder_schedule_element_type(
	struct precompile_type_builder *				p_precompile_type_builder,
	traversing_engine_task_fn_t 					fn,
	char * 											global_path,
	struct metac_type *								p_type,
	struct element_type_hierarchy_member *			p_from_member) {
	_create_(precompile_type_builder_element_type_task);
	
	p_precompile_type_builder_element_type_task->task.fn = fn;
	p_precompile_type_builder_element_type_task->global_path = strdup(global_path);
	p_precompile_type_builder_element_type_task->p_type = p_type;
	p_precompile_type_builder_element_type_task->p_from_member = p_from_member;
	if (p_precompile_type_builder_element_type_task->global_path == NULL) {
		msg_stderr("wasn't able to duplicate global_path\n");
		precompile_type_builder_element_type_task_delete(&p_precompile_type_builder_element_type_task);
		return NULL;
	}
	if (add_traversing_task_to_tail(
		&p_precompile_type_builder->element_type_traversing_engine,
		&p_precompile_type_builder_element_type_task->task) != 0) {
		msg_stderr("wasn't able to schedule task\n");
		precompile_type_builder_element_type_task_delete(&p_precompile_type_builder_element_type_task);
		return NULL;
	}
	return p_precompile_type_builder_element_type_task;
}
int precompile_type_builder_schedule_element_type_from_array(
		struct precompile_type_builder *			p_precompile_type_builder,
		char * 										global_path,
		struct metac_type *							p_type,
		struct element_type_hierarchy_member *		p_from_member/*can be NULL*/) {
	return (precompile_type_builder_schedule_element_type(
		p_precompile_type_builder,
		precompile_type_builder_element_type_from_array_task_fn,
		global_path,
		p_type,
		p_from_member)!=NULL)?0:(-EFAULT);
}
int precompile_type_builder_schedule_element_type_from_pointer(
		struct precompile_type_builder *			p_precompile_type_builder,
		char * 										global_path,
		struct metac_type *							p_type,
		struct element_type_hierarchy_member *		p_from_member/*can be NULL*/) {
	return (precompile_type_builder_schedule_element_type(
		p_precompile_type_builder,
		precompile_type_builder_element_type_from_pointer_task_fn,
		global_path,
		p_type,
		p_from_member)!=NULL)?0:(-EFAULT);
}
int precompile_type_builder_init(
		struct precompile_type_builder *			p_precompile_type_builder,
		struct metac_type *							p_root_type,
		metac_type_annotation_t *					p_override_annotations) {
	p_precompile_type_builder->p_root_type = p_root_type;
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
		precompile_type_builder_element_type_task_delete(&p_precompile_type_builder_element_type_task);
	}
	precompile_type_container_clean(&p_precompile_type_builder->container, keep_data);
	p_precompile_type_builder->p_root_type = NULL;
}
metac_precompiled_type_t * metac_precompile_type(
		struct metac_type *							p_root_type,
		metac_type_annotation_t *					p_override_annotations) {
	struct precompile_type_builder precompile_type_builder;
	msg_stddbg("metac_precompile_type\n");
	precompile_type_builder_init(&precompile_type_builder, p_root_type, p_override_annotations);
	if (precompile_type_builder_schedule_element_type_from_pointer(&precompile_type_builder, "ptr", p_root_type, NULL) != 0) {
		msg_stddbg("wasn't able to schedule the first task\n");
		precompile_type_builder_clean(&precompile_type_builder, 0);
		return NULL;
	}
	if (traversing_engine_run(&precompile_type_builder.element_type_traversing_engine) != 0) {
		msg_stddbg("tasks execution finished with fail\n");
		precompile_type_builder_clean(&precompile_type_builder, 0);
		return NULL;
	}
	precompile_type_builder_clean(&precompile_type_builder, 1);
	msg_stddbg("metac_precompile_type exit\n");
	return NULL;
}
int  metac_free_precompiled_type(metac_precompiled_type_t ** precompiled_type) {

}
