/*
 * memory_backend_pointer.c
 *
 *  Created on: Jan 22, 2020
 *      Author: mralex
 */

#define METAC_DEBUG_ENABLE

#include <assert.h>			/* assert */
#include <errno.h>			/* ENOMEM etc */
#include <stdlib.h>			/* calloc, free, NULL, qsort... */
#include <urcu/list.h>		/* struct cds_list_entry etc */

#include "metac_internals.h"
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
struct memory_backend_pointer {
	struct memory_backend_interface					memory_backend_interface;
	void *											ptr;
	/*TBD: caches, whatever if necessary */
};

static inline struct memory_backend_pointer * memory_backend_pointer(
		struct memory_backend_interface *			p_memory_backend_interface) {
	return cds_list_entry(
			p_memory_backend_interface,
			struct memory_backend_pointer,
			memory_backend_interface);
}
/*****************************************************************************/
static int memory_backend_pointer_delete(
		struct memory_backend_pointer **			pp_memory_backend_pointer) {
	_delete_start_(memory_backend_pointer);
	_delete_finish(memory_backend_pointer);
	return 0;
}
/*****************************************************************************/
static int _memory_backend_interface_delete(
		struct memory_backend_interface **			pp_memory_backend_interface) {
	struct memory_backend_pointer * p_memory_backend_pointer;

	assert(pp_memory_backend_interface);
	assert(*pp_memory_backend_interface);

	if ((*pp_memory_backend_interface) != NULL) {
		p_memory_backend_pointer = memory_backend_pointer(*pp_memory_backend_interface);
		memory_backend_pointer_delete(&p_memory_backend_pointer);
		*pp_memory_backend_interface = NULL;
	}

	return 0;
}

static int _element_get_memory_backend_interface(
		struct element *							p_element,
		struct memory_backend_interface **			pp_memory_backend_interface) {
	struct memory_backend_pointer * p_memory_backend_pointer;
	struct memory_backend_pointer * p_result_memory_backend_pointer;

	assert(p_element);
	assert(p_element->p_memory_block);
	assert(p_element->p_memory_block->p_memory_backend_interface);

	p_memory_backend_pointer = memory_backend_pointer(p_element->p_memory_block->p_memory_backend_interface);
	assert(p_memory_backend_pointer);

	p_result_memory_backend_pointer = memory_backend_interface_create_from_pointer(
			p_memory_backend_pointer->ptr +
			p_element->id * p_element->p_element_type->byte_size);

	if (p_result_memory_backend_pointer == NULL) {
		msg_stderr("memory_backend_interface_create_from_pointer failed\n");
		return -(EFAULT);
	}

	if (pp_memory_backend_interface) {
		*pp_memory_backend_interface = &p_result_memory_backend_pointer->memory_backend_interface;
	}

	return 0;
}

static int _element_read_memory_backend_interface(
		struct element *							p_element,
		struct memory_backend_interface **			pp_memory_backend_interface) {

	struct memory_backend_pointer * p_memory_backend_pointer;
	struct memory_backend_pointer * p_result_memory_backend_pointer;

	assert(p_element);
	assert(p_element->p_memory_block);
	assert(p_element->p_memory_block->p_memory_backend_interface);

	p_memory_backend_pointer = memory_backend_pointer(p_element->p_memory_block->p_memory_backend_interface);
	assert(p_memory_backend_pointer);

	if (p_memory_backend_pointer->ptr == NULL) {
		msg_stderr("memory block is initialized with NULL\n");
		return -(EFAULT);
	}

	p_result_memory_backend_pointer = memory_backend_interface_create_from_pointer(
			*((void**)(
					p_memory_backend_pointer->ptr +
					p_element->id * p_element->p_element_type->byte_size)));

	if (p_result_memory_backend_pointer == NULL) {
		msg_stderr("memory_backend_interface_create_from_pointer failed\n");
		return -(EFAULT);
	}

	if (pp_memory_backend_interface) {
		*pp_memory_backend_interface = &p_result_memory_backend_pointer->memory_backend_interface;
	}

	return 0;
}

static int _element_hierarchy_member_get_memory_backend_interface(
		struct element_hierarchy_member *			p_element_hierarchy_member,
		struct memory_backend_interface **			pp_memory_backend_interface) {

	struct memory_backend_pointer * p_memory_backend_pointer;
	struct memory_backend_pointer * p_result_memory_backend_pointer;

	assert(p_element_hierarchy_member);
	assert(p_element_hierarchy_member->p_element);
	assert(p_element_hierarchy_member->p_element->p_memory_block);
	assert(p_element_hierarchy_member->p_element->p_memory_block->p_memory_backend_interface);

	p_memory_backend_pointer = memory_backend_pointer(p_element_hierarchy_member->p_element->p_memory_block->p_memory_backend_interface);
	assert(p_memory_backend_pointer);

	p_result_memory_backend_pointer = memory_backend_interface_create_from_pointer(
			p_memory_backend_pointer->ptr +
			p_element_hierarchy_member->p_element->id * p_element_hierarchy_member->p_element->p_element_type->byte_size +
			p_element_hierarchy_member->p_element_type_hierarchy_member->offset);

	if (p_result_memory_backend_pointer == NULL) {
		msg_stderr("memory_backend_interface_create_from_pointer failed\n");
		return -(EFAULT);
	}

	if (pp_memory_backend_interface) {
		*pp_memory_backend_interface = &p_result_memory_backend_pointer->memory_backend_interface;
	}

	return 0;
}

static int _element_hierarchy_member_read_memory_backend_interface(
		struct element_hierarchy_member *			p_element_hierarchy_member,
		struct memory_backend_interface **			pp_memory_backend_interface) {

	struct memory_backend_pointer * p_memory_backend_pointer;
	struct memory_backend_pointer * p_result_memory_backend_pointer;

	assert(p_element_hierarchy_member);
	assert(p_element_hierarchy_member->p_element);
	assert(p_element_hierarchy_member->p_element->p_memory_block);
	assert(p_element_hierarchy_member->p_element->p_memory_block->p_memory_backend_interface);

	p_memory_backend_pointer = memory_backend_pointer(p_element_hierarchy_member->p_element->p_memory_block->p_memory_backend_interface);
	assert(p_memory_backend_pointer);

	if (p_memory_backend_pointer->ptr == NULL) {
		msg_stderr("memory block is initialized with NULL\n");
		return -(EFAULT);
	}

	p_result_memory_backend_pointer = memory_backend_interface_create_from_pointer(
			*((void**)(
					p_memory_backend_pointer->ptr +
					p_element_hierarchy_member->p_element->id * p_element_hierarchy_member->p_element->p_element_type->byte_size +
					p_element_hierarchy_member->p_element_type_hierarchy_member->offset)));

	if (p_result_memory_backend_pointer == NULL) {
		msg_stderr("memory_backend_interface_create_from_pointer failed\n");
		return -(EFAULT);
	}

	if (pp_memory_backend_interface) {
		*pp_memory_backend_interface = &p_result_memory_backend_pointer->memory_backend_interface;
	}

	return 0;
}

/*****************************************************************************/
static struct memory_backend_interface_ops ops = {
	.memory_backend_interface_delete = 				_memory_backend_interface_delete,
	.element_get_memory_backend_interface = 		_element_get_memory_backend_interface,
	.element_read_memory_backend_interface =		_element_read_memory_backend_interface,
	.element_hierarchy_member_read_memory_backend_interface =
													_element_hierarchy_member_read_memory_backend_interface,
	.element_hierarchy_member_get_memory_backend_interface =
													_element_hierarchy_member_get_memory_backend_interface,
};
/*****************************************************************************/
struct memory_backend_pointer * memory_backend_interface_create_from_pointer(
		void *										ptr) {
	_create_(memory_backend_pointer);

	p_memory_backend_pointer->memory_backend_interface.p_ops = &ops;
	p_memory_backend_pointer->ptr = ptr;

	return p_memory_backend_pointer;
}
