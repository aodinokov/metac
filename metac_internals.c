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
#define _delete_(_type_) \
	if (pp_##_type_ == NULL) { \
		msg_stderr("Can't delete " #_type_ ": invalid parameter\n"); \
		return -EINVAL; \
	} \
	if (*pp_##_type_ == NULL) { \
		msg_stderr("Can't delete " #_type_ ": already deleted\n"); \
		return -EALREADY; \
	} \
	free(*pp_##_type_); \
	*pp_##_type_ = NULL;
/*****************************************************************************/
struct precompiled_type_builder_element_type {
	struct cds_list_head							list;

	struct element_type *							p_element_type;
	struct element_type_hierarchy_member *			p_from_member;		/* info about member that contained this type*/
};

struct precompiled_type_builder {
	struct cds_list_head							pointers_element_type_list;
	struct cds_list_head							arrays_element_type_list;
};
/*****************************************************************************/

int delete_discriminator(struct discriminator **	pp_discriminator) {
	_delete_(discriminator);
	return 0;
}

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
/*****************************************************************************/
int delete_element_type(struct element_type **		pp_element_type) {
	_delete_(element_type);
	return 0;
}

static int create_element_type_pointer(
		struct metac_type *							p_root_type,
		metac_type_annotation_t *					override_annotations,
		char * 										global_path,
		struct element_type *						p_element_type) {
	const metac_type_annotation_t * p_annotation;
	assert(	p_element_type && 
			p_element_type->actual_type &&
			p_element_type->actual_type->id == DW_TAG_pointer_type);
	/* todo: based on the actual_type of the ptr different defaults can be selected */
	p_annotation = metac_type_annotation(p_root_type, global_path, override_annotations);
	return 0;
}

struct element_type * create_element_type(
		struct metac_type *							p_root_type,
		metac_type_annotation_t *					override_annotations,
		char * 										global_path,
		struct metac_type *							p_type) {
	int res;
	_create_(element_type);
	p_element_type->type = p_type;
	p_element_type->actual_type = metac_type_actual_type(p_type);
	p_element_type->byte_size = metac_type_byte_size(p_type);
	
	switch(p_element_type->actual_type->id) {
	case DW_TAG_pointer_type:
		res = create_element_type_pointer(p_root_type, override_annotations, global_path, p_element_type);
		break;
	case DW_TAG_array_type:
		break;
	case DW_TAG_structure_type:
	case DW_TAG_union_type:
		break;
	}
	return p_element_type;
}

int precompiled_type_builder_element_type_init(
		struct precompiled_type_builder_element_type *
													p_precompiled_type_builder_element_type,
		struct metac_type *							p_root_type,
		metac_type_annotation_t *					override_annotations,
		char * 										global_path,
		struct metac_type *							p_type,
		struct element_type_hierarchy_member *		p_from_member) {

	p_precompiled_type_builder_element_type->p_from_member = p_from_member;
	p_precompiled_type_builder_element_type->p_element_type = create_element_type(p_root_type, override_annotations, global_path, p_type);
	if (p_precompiled_type_builder_element_type->p_element_type == NULL) {
		return -EFAULT;
	}
	return 0;
}
void precompiled_type_builder_element_type_clean(
		struct precompiled_type_builder_element_type *
													p_precompiled_type_builder_element_type,
		metac_flag									keep_data
		) {
	struct precompiled_type_builder_member * _member, * __member;
	struct precompiled_type_builder_discriminator * _discriminator, * __discriminator;

	if (keep_data == 0) {
		delete_element_type(&p_precompiled_type_builder_element_type->p_element_type);
	}else {
		p_precompiled_type_builder_element_type->p_element_type = NULL;
	}

	p_precompiled_type_builder_element_type->p_from_member = NULL;
}
int precompiled_type_builder_init(
		struct precompiled_type_builder *			p_precompiled_type_builder,
		onElementTypeAdd_cb_t						on_element_type_add_cb,
		void *										p_cb_data) {
	CDS_INIT_LIST_HEAD(&p_precompiled_type_builder->pointers_element_type_list);
	CDS_INIT_LIST_HEAD(&p_precompiled_type_builder->arrays_element_type_list);
	p_precompiled_type_builder->on_element_type_add_cb = on_element_type_add_cb;
	p_precompiled_type_builder->p_cb_data = p_cb_data;
	return 0;
}
void precompiled_type_builder_clean(
		struct precompiled_type_builder *			p_precompiled_type_builder,
		metac_flag									keep_data) {
	struct precompiled_type_builder_element_type * _element_type, * __element_type;
	cds_list_for_each_entry_safe(_element_type, __element_type, &p_precompiled_type_builder->pointers_element_type_list, list) {
		cds_list_del(&_element_type->list);
		precompiled_type_builder_element_type_clean(_element_type, keep_data);
		free(_element_type);
	}
	cds_list_for_each_entry_safe(_element_type, __element_type, &p_precompiled_type_builder->arrays_element_type_list, list) {
		cds_list_del(&_element_type->list);
		precompiled_type_builder_element_type_clean(_element_type, keep_data);
		free(_element_type);
	}
}
static struct precompiled_type_builder_element_type * precompiled_type_builder_element_type_create(
		struct precompiled_type_builder *			p_precompiled_type_builder,
		struct metac_type *							p_root_type,
		metac_type_annotation_t *					override_annotations,
		char * 										global_path,
		struct metac_type *							p_type,
		struct element_type_hierarchy_member *		p_from_member/*optional*/) {
	int res;
	_create_(precompiled_type_builder_element_type);
	msg_stddbg("precompiled_type_builder_add_element_type\n");

	res = precompiled_type_builder_element_type_init(p_precompiled_type_builder_element_type, p_root_type, override_annotations, global_path, p_type, p_from_member);
	if (res != 0) {
			msg_stddbg("precompiled_type_builder_element_type_init returned error - deleting\n");
			free(p_precompiled_type_builder_element_type);
			return NULL;		
	}

	if (p_precompiled_type_builder->on_element_type_add_cb != NULL) {
		res = p_precompiled_type_builder->on_element_type_add_cb(
				p_precompiled_type_builder,
				p_precompiled_type_builder_element_type->p_element_type,
				p_precompiled_type_builder->p_cb_data);
		if (res != 0) {
			msg_stddbg("on_element_type_add_cb returned error - deleting\n");
			precompiled_type_builder_element_type_clean(p_precompiled_type_builder_element_type, 0);
			free(p_precompiled_type_builder_element_type);
			return NULL;
		}
	}

	msg_stddbg("precompiled_type_builder_add_element_type exit\n");
	return p_precompiled_type_builder_element_type;
}

int precompiled_type_builder_add_element_type_from_ptr(
		struct precompiled_type_builder *			p_precompiled_type_builder,
		metac_type_annotation_t *					override_annotations,
		struct metac_type *							p_root_type,
		char * 										global_path,
		struct metac_type *							p_type,
		struct element_type_hierarchy_member *		p_from_member/*can be NULL*/) {
	struct precompiled_type_builder_element_type * _element_type;

	/* try to find this type */
	cds_list_for_each_entry(_element_type, &p_precompiled_type_builder->pointers_element_type_list, list) {
		if (_element_type->p_element_type->type == p_type &&
			_element_type->p_from_member == p_from_member) {
			msg_stddbg("found %p\n", _element_type);
			return 0;
		}
	}
	/* create */
	return (precompiled_type_builder_element_type_create(
			p_precompiled_type_builder,
			p_root_type,
			override_annotations,
			global_path,
			p_type,
			p_from_member) != NULL)?0:(-EFAULT);
}

/*****************************************************************************/
struct precompiled_type_context {
	struct metac_type *							p_root_type;
	metac_type_annotation_t *					p_override_annotations;

	struct traversing_engine					traversing_engine;
	struct precompiled_type_builder				builder;
	struct cds_list_head						executed_tasks_queue;
};
/*****************************************************************************/
static int _onElementTypeAdd_cb (
		struct precompiled_type_builder *
									p_precompiled_type_builder,
//		struct precompiled_type_builder_element_type *
//									p_precompiled_type_builder_element_type,
		struct element_type *		p_element_type,
		void *						p_cb_data) {
	struct precompiled_type_context * p_precompiled_type_context = (struct precompiled_type_context *)p_cb_data;
	msg_stddbg("_onElementTypeAdd_cb\n");
	/*TODO: create fn get_current*/
	return 0;
}

int precompiled_type_context_init(
		struct precompiled_type_context *			p_precompiled_type_context,
		struct metac_type *							p_root_type,
		metac_type_annotation_t *					p_override_annotations) {
	p_precompiled_type_context->p_root_type = p_root_type;
	precompiled_type_builder_init(
			&p_precompiled_type_context->builder,
			_onElementTypeAdd_cb,
			(void*)p_precompiled_type_context);
	CDS_INIT_LIST_HEAD(&p_precompiled_type_context->executed_tasks_queue);
	traversing_engine_init(&p_precompiled_type_context->traversing_engine);
	return 0;
}

void precompiled_type_context_clean(
		struct precompiled_type_context *			p_precompiled_type_context,
		metac_flag									keep_data) {
	struct traversing_engine_task * task, *_task;

	traversing_engine_clean(&p_precompiled_type_context->traversing_engine);
	cds_list_for_each_entry_safe(task, _task, &p_precompiled_type_context->executed_tasks_queue, list) {
//		struct precompile_task * p_precompile_task = cds_list_entry(task, struct precompile_task, traversing_engine_task);
		cds_list_del(&task->list);
//		delete_precompile_task(&p_precompile_task);
	}
	precompiled_type_builder_clean(&p_precompiled_type_context->builder, keep_data);
	p_precompiled_type_context->p_root_type = NULL;
}

metac_precompiled_type_t * metac_precompile_type(
		struct metac_type *							p_root_type,
		metac_type_annotation_t *					p_override_annotations) {
	struct precompiled_type_context precompiled_type_context;
	msg_stddbg("metac_precompile_type\n");
	precompiled_type_context_init(&precompiled_type_context, p_root_type, p_override_annotations);
	precompiled_type_builder_add_element_type_from_ptr(&precompiled_type_context.builder, p_override_annotations, p_root_type, "ptr", p_root_type, NULL);
	traversing_engine_run(&precompiled_type_context.traversing_engine);
	precompiled_type_context_clean(&precompiled_type_context, 1);
	msg_stddbg("metac_precompile_type exit\n");
	return NULL;
}

