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

struct element_type * create_element_type(
		struct metac_type *							p_type) {
	_create_(element_type);
	return p_element_type;
}
/*****************************************************************************/
/*****************************************************************************/
struct precompiled_type_builder_discriminator {
	struct cds_list_head				list;

	struct discriminator *				p_discriminator;
};

struct precompiled_type_builder_member {
	struct cds_list_head				list;

	struct element_type_hierarchy_member *
										p_member;
};

struct precompiled_type_builder_element_type {
	struct cds_list_head				list;

	struct element_type *				p_element_type;

	struct element_type_hierarchy_member *
										p_from_member;		/* info about member that contained this type*/

//	/*???? don't want to have it here*/
//	struct traversing_engine			traversing_engine;
//	struct cds_list_head				executed_tasks_queue;

	struct cds_list_head 				discriminator_list;
	struct cds_list_head 				members_list;
};

struct precompiled_type_builder;
typedef int (*onElementTypeAdd_cb_t) (
		struct precompiled_type_builder *
									p_precompiled_type_builder,
		struct precompiled_type_builder_element_type *
									p_precompiled_type_builder_element_type,
		void *						p_cb_data);

struct precompiled_type_builder {
	struct cds_list_head				pointers_element_type_list;
	struct cds_list_head				arrays_element_type_list;
	onElementTypeAdd_cb_t				on_element_type_add_cb;
	void *								p_cb_data;
};
/*****************************************************************************/
int precompiled_type_builder_discriminator_init(
		struct precompiled_type_builder_discriminator *p_precompiled_type_builder_discriminator) {
	return 0;
}
int precompiled_type_builder_discriminator_clean(
		struct precompiled_type_builder_discriminator *p_precompiled_type_builder_discriminator,
		metac_flag									keep_data) {
	return 0;
}
int precompiled_type_builder_member_init(
		struct precompiled_type_builder_member *	p_precompiled_type_builder_member) {
	return 0;
}

int precompiled_type_builder_member_clean(
		struct precompiled_type_builder_member *	p_precompiled_type_builder_member,
		metac_flag									keep_data) {
	return 0;
}

int precompiled_type_builder_element_type_init(
		struct precompiled_type_builder_element_type *
													p_precompiled_type_builder_element_type,
		struct metac_type *							p_type,
		struct element_type_hierarchy_member *		p_from_member
		) {
	CDS_INIT_LIST_HEAD(&p_precompiled_type_builder_element_type->discriminator_list);
	CDS_INIT_LIST_HEAD(&p_precompiled_type_builder_element_type->members_list);
	p_precompiled_type_builder_element_type->p_from_member = p_from_member;
	p_precompiled_type_builder_element_type->p_element_type = create_element_type(p_type);
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

	cds_list_for_each_entry_safe(_member, __member, &p_precompiled_type_builder_element_type->members_list, list) {
		cds_list_del(&_discriminator->list);
		precompiled_type_builder_member_clean(_member, keep_data);
		free(_discriminator);
	}
	cds_list_for_each_entry_safe(_discriminator, __discriminator, &p_precompiled_type_builder_element_type->discriminator_list, list) {
		cds_list_del(&_discriminator->list);
		precompiled_type_builder_discriminator_clean(_discriminator, keep_data);
		free(_discriminator);
	}
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
		char * 										global_path,
		struct metac_type *							p_type,
		struct element_type_hierarchy_member *		p_from_member/*optional*/) {
	_create_(precompiled_type_builder_element_type);
	msg_stddbg("precompiled_type_builder_add_element_type\n");

	precompiled_type_builder_element_type_init(p_precompiled_type_builder_element_type, p_type, p_from_member);

	if (p_precompiled_type_builder->on_element_type_add_cb != NULL) {
		p_precompiled_type_builder->on_element_type_add_cb(
				p_precompiled_type_builder,
				p_precompiled_type_builder_element_type,
				p_precompiled_type_builder->p_cb_data);
	}

	msg_stddbg("precompiled_type_builder_add_element_type exit\n");
	return p_precompiled_type_builder_element_type;
}

int precompiled_type_builder_add_element_type_from_ptr(
		struct precompiled_type_builder *			p_precompiled_type_builder,
		char * 										global_path,
		struct metac_type *							p_type,
		struct element_type_hierarchy_member *		p_from_member/*optional*/) {
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
			global_path,
			p_type,
			p_from_member) != NULL)?0:(-EFAULT);
}

/*****************************************************************************/
struct precompile_context {
	struct metac_type *					type;
	struct traversing_engine			traversing_engine;
	struct cds_list_head				executed_tasks_queue;
	struct precompiled_type_builder		builder;
};
/*****************************************************************************/
static int _onElementTypeAdd_cb (
		struct precompiled_type_builder *
									p_precompiled_type_builder,
		struct precompiled_type_builder_element_type *
									p_precompiled_type_builder_element_type,
		void *						p_cb_data) {
	struct precompile_context * p_precompile_context = (struct precompile_context *)p_cb_data;
	/**/
	return 0;
}

int precompile_context_init(
		struct precompile_context *					p_precompile_context,
		struct metac_type *							type) {
	p_precompile_context->type = type;
	precompiled_type_builder_init(
			&p_precompile_context->builder,
			_onElementTypeAdd_cb,
			(void*)p_precompile_context);
	CDS_INIT_LIST_HEAD(&p_precompile_context->executed_tasks_queue);
	traversing_engine_init(&p_precompile_context->traversing_engine);
	return 0;
}

void precompile_context_clean(
		struct precompile_context *					p_precompile_context,
		metac_flag									keep_data) {
	struct traversing_engine_task * task, *_task;

	traversing_engine_clean(&p_precompile_context->traversing_engine);
	cds_list_for_each_entry_safe(task, _task, &p_precompile_context->executed_tasks_queue, list) {
//		struct precompile_task * p_precompile_task = cds_list_entry(task, struct precompile_task, traversing_engine_task);
		cds_list_del(&task->list);
//		delete_precompile_task(&p_precompile_task);
	}
	precompiled_type_builder_clean(&p_precompile_context->builder, keep_data);
	p_precompile_context->type = NULL;
}

metac_precompiled_type_t * metac_precompile_type(
													struct metac_type *type) {
	struct precompile_context precompile_context;
	msg_stddbg("metac_precompile_type\n");
	precompile_context_init(&precompile_context, type);
	precompiled_type_builder_add_element_type_from_ptr(&precompile_context.builder, "ptr", type, NULL);
	traversing_engine_run(&precompile_context.traversing_engine);
	precompile_context_clean(&precompile_context, 1);
	msg_stddbg("metac_precompile_type exit\n");
	return NULL;
}

