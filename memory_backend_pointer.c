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
#if 0
/*WIP*/
static int element_pointer_cast_pointer(
		struct element_pointer *					p_element_pointer,
		struct element_type_pointer *				p_element_type_pointer) {

	/* exit normally if pointer is NULL */
	if (p_element_pointer->ptr == NULL) {
		return 0;
	}

	/*type cast if needed*/
	p_element_pointer->actual_ptr = p_element_pointer->ptr;
	p_element_pointer->p_actual_element_type = p_element_type_pointer->p_element_type;

	if (p_element_type_pointer->generic_cast.cb != NULL) {
		int res = p_element_type_pointer->generic_cast.cb(
				p_element_type_pointer->annotation_key,
				0,
				&p_element_pointer->use_cast,
				&p_element_pointer->generic_cast_type_id,
				&p_element_pointer->ptr,
				&p_element_pointer->actual_ptr,
				p_element_type_pointer->generic_cast.p_data);
		if (res != 0) {
			msg_stderr("generic_cast.cb returned error %d\n", res);
			return res;
		}
		if (p_element_pointer->generic_cast_type_id >= p_element_type_pointer->generic_cast.types_count) {
			msg_stderr("generic_cast.cb set incorrect generic_cast_type_id %d. Maximum value is %d\n",
					p_element_pointer->generic_cast_type_id,
					p_element_type_pointer->generic_cast.types_count);
			return -EINVAL;
		}
		p_element_pointer->p_actual_element_type = p_element_type_pointer->generic_cast.p_types[p_element_pointer->generic_cast_type_id].p_element_type;
	}

	p_element_pointer->ptr_offset = 0;
	if (p_element_pointer->ptr > p_element_pointer->actual_ptr) {
		p_element_pointer->ptr_offset = p_element_pointer->ptr - p_element_pointer->actual_ptr;
	}

	return 0;
}
#endif
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

static int _memory_backend_interface_equals(
		struct memory_backend_interface *			p_memory_backend_interface0,
		struct memory_backend_interface *			p_memory_backend_interface1) {
	struct memory_backend_pointer * p_memory_backend_pointer0;
	struct memory_backend_pointer * p_memory_backend_pointer1;

	assert(p_memory_backend_interface0);
	assert(p_memory_backend_interface1);

	p_memory_backend_pointer0 = memory_backend_pointer(p_memory_backend_interface0);
	p_memory_backend_pointer1 = memory_backend_pointer(p_memory_backend_interface1);

	return p_memory_backend_pointer0 == p_memory_backend_pointer1;
}
/*****************************************************************************/
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

static int _element_get_array_subrange0(
		struct element *							p_element) {
	return 0;
}

static int _element_get_pointer_subrange0(
		struct element *							p_element) {
	return 0;
}

static int _element_cast_pointer(
		struct element *							p_element) {

	assert(p_element);
	assert(p_element->p_element_type);


	return 0;
}

static int _element_calculate_hierarchy_top_discriminator_values(
		struct element *							p_element) {
	return 0;
}

/*****************************************************************************/
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

static int _element_hierarchy_member_get_array_subrange0(
		struct element_hierarchy_member *			p_element_hierarchy_member) {
	return 0;
}

static int _element_hierarchy_member_get_pointer_subrange0(
		struct element_hierarchy_member *			p_element_hierarchy_member) {
	return 0;
}

static int _element_hierarchy_member_cast_pointer(
		struct element_hierarchy_member *			p_element_hierarchy_member) {
	return 0;
}
/*****************************************************************************/
static struct memory_backend_interface_ops ops = {
	/* Mandatory handlers */
	.memory_backend_interface_delete =				_memory_backend_interface_delete,
	.memory_backend_interface_equals =				_memory_backend_interface_equals,

	.element_get_memory_backend_interface =			_element_get_memory_backend_interface,
	.element_read_memory_backend_interface =		_element_read_memory_backend_interface,
	.element_get_array_subrange0 =					_element_get_array_subrange0,
	.element_get_pointer_subrange0 =				_element_get_pointer_subrange0,
	.element_cast_pointer =							_element_cast_pointer,
	.element_calculate_hierarchy_top_discriminator_values =
													_element_calculate_hierarchy_top_discriminator_values,

	.element_hierarchy_member_read_memory_backend_interface =
													_element_hierarchy_member_read_memory_backend_interface,
	.element_hierarchy_member_get_memory_backend_interface =
													_element_hierarchy_member_get_memory_backend_interface,
	.element_hierarchy_member_get_array_subrange0 =	_element_hierarchy_member_get_array_subrange0,
	.element_hierarchy_member_get_pointer_subrange0 =
													_element_hierarchy_member_get_pointer_subrange0,
	.element_hierarchy_member_cast_pointer =		_element_hierarchy_member_cast_pointer,
	/* Optional handlers */
};
/*****************************************************************************/
struct memory_backend_pointer * memory_backend_interface_create_from_pointer(
		void *										ptr) {
	_create_(memory_backend_pointer);

	p_memory_backend_pointer->memory_backend_interface.p_ops = &ops;
	p_memory_backend_pointer->ptr = ptr;

	return p_memory_backend_pointer;
}
