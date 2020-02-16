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
#include "metac_oop.h"		/*_create, _delete, ...*/
#include "metac_debug.h"	/* msg_stderr, ...*/


/*****************************************************************************/
struct memory_backend_pointer {
	struct memory_backend_interface					memory_backend_interface;
	void *											ptr;
	/*TBD: caches, whatever if necessary */
};
/*****************************************************************************/
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
static int _memory_backend_interface_create_from_pointer(
		void *										ptr,
		struct memory_backend_interface **			pp_memory_backend_interface) {

	struct memory_backend_interface * p_memory_backend_interface;

	if (ptr == NULL) {
		if (pp_memory_backend_interface != NULL) {
			*pp_memory_backend_interface = NULL;
		}
		return 0;
	}

	p_memory_backend_interface = memory_backend_interface_create_from_pointer(ptr);
	if (p_memory_backend_interface == NULL) {
		msg_stderr("memory_backend_interface_create_from_pointer failed\n");
		return -(ENOMEM);
	}

	if (pp_memory_backend_interface != NULL) {
		*pp_memory_backend_interface = p_memory_backend_interface;
	}

	return 0;
}

static int element_pointer_cast_pointer(
		struct element_pointer *					p_element_pointer,
		struct element_type_pointer *				p_element_type_pointer) {

	struct memory_backend_pointer * p_original_memory_backend_pointer = memory_backend_pointer(p_element_pointer->p_original_memory_backend_interface);
	void * p_original;
	void * p_casted;

	p_original = p_original_memory_backend_pointer->ptr;
	if (p_original == NULL) {
		return -(EINVAL);
	}

	/*type cast if needed*/
	p_casted = p_original;
	p_element_pointer->p_casted_element_type = p_element_type_pointer->p_element_type;

	if (p_element_type_pointer->generic_cast.cb != NULL) {
		int res = p_element_type_pointer->generic_cast.cb(
				p_element_type_pointer->annotation_key,
				0,
				&p_element_pointer->use_cast,
				&p_element_pointer->generic_cast_type_id,
				&p_original,
				&p_casted,
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
		p_element_pointer->p_casted_element_type = p_element_type_pointer->generic_cast.p_types[p_element_pointer->generic_cast_type_id].p_element_type;
	}

	p_element_pointer->casted_based_original_offset = 0;
	if (p_original > p_casted) {
		p_element_pointer->casted_based_original_offset = p_original - p_casted;
	}

	if (_memory_backend_interface_create_from_pointer(
			p_casted,
			&p_element_pointer->p_casted_memory_backend_interface) != 0) {
		msg_stderr("_memory_backend_interface_create_from_pointer failed\n");
		return -EINVAL;
	}

	return 0;
}

static int element_array_get_array_subrange0(
		struct element_array *						p_element_array,
		struct element_type_array *					p_element_type_array) {
	struct memory_backend_pointer * p_memory_backend_pointer = memory_backend_pointer(p_element_array->p_memory_backend_interface);
	void * ptr;

	if (p_element_type_array->array_elements_count.cb == NULL) {
		msg_stderr("Flexible array length callback isn't defined\n");
		return (-EFAULT);
	}

	ptr = p_memory_backend_pointer->ptr;

	if (p_element_type_array->array_elements_count.cb(
			p_element_type_array->annotation_key,
			0,
			ptr,
			p_element_type_array->p_element_type->p_actual_type,
			/*TODO: take the next params from the global context*/
			/*FIXME:*/NULL,
			/*FIXME:*/NULL,
			&p_element_array->subrange0_count,
			p_element_type_array->array_elements_count.p_data) != 0) {
		msg_stderr("Flexible array length callback failed\n");
		return (-EFAULT);
	}

	return 0;
}

static int element_pointer_get_pointer_subrange0(
		struct element_pointer *					p_element_pointer,
		struct element_type_pointer *				p_element_type_pointer) {
	struct memory_backend_pointer * p_memory_backend_pointer = memory_backend_pointer(p_element_pointer->p_casted_memory_backend_interface);
	void * ptr;

	if (p_element_type_pointer->array_elements_count.cb == NULL) {
		msg_stderr("Pointer length callback isn't defined\n");
		return (-EFAULT);
	}

	ptr = p_memory_backend_pointer->ptr;

	if (p_element_type_pointer->array_elements_count.cb(
			p_element_type_pointer->annotation_key,
			0,
			ptr,
			p_element_type_pointer->p_element_type->p_actual_type,
			/*TODO: take the next params from the global context*/
			/*FIXME:*/NULL,
			/*FIXME:*/NULL,
			&p_element_pointer->subrange0_count,
			p_element_type_pointer->array_elements_count.p_data) != 0) {
		msg_stderr("Pointer array length callback failed\n");
		return (-EFAULT);
	}

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

	if (_memory_backend_interface_create_from_pointer(
			p_memory_backend_pointer->ptr +
			p_element->id * p_element->p_element_type->byte_size,
			pp_memory_backend_interface) != 0) {
		msg_stderr("_memory_backend_interface_create_from_pointer failed\n");
		return -(EFAULT);
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

	if (_memory_backend_interface_create_from_pointer(
			*((void**)(
					p_memory_backend_pointer->ptr +
					p_element->id * p_element->p_element_type->byte_size)),
			pp_memory_backend_interface) != 0) {
		msg_stderr("_memory_backend_interface_create_from_pointer failed\n");
		return -(EFAULT);
	}

	return 0;
}

static int _element_get_array_subrange0(
		struct element *							p_element) {

	assert(p_element);
	assert(p_element->p_element_type);

	return element_array_get_array_subrange0(
			&p_element->array,
			&p_element->p_element_type->array);
}

static int _element_get_pointer_subrange0(
		struct element *							p_element) {

	assert(p_element);
	assert(p_element->p_element_type);


	return element_pointer_get_pointer_subrange0(
			&p_element->pointer,
			&p_element->p_element_type->pointer);
}

static int _element_cast_pointer(
		struct element *							p_element) {

	assert(p_element);
	assert(p_element->p_element_type);

	return element_pointer_cast_pointer(
			&p_element->pointer,
			&p_element->p_element_type->pointer);
}

static int _element_calculate_hierarchy_top_discriminator_values(
		struct element *							p_element) {

	int i;
	struct memory_backend_pointer * p_memory_backend_pointer;

	assert(p_element);
	assert(p_element->p_memory_block);
	assert(p_element->p_memory_block->p_memory_backend_interface);

	p_memory_backend_pointer = memory_backend_pointer(p_element->p_memory_block->p_memory_backend_interface);
	assert(p_memory_backend_pointer);

	for (i = 0; i < p_element->p_element_type->hierarchy_top.discriminators_count; ++i) {
		if (p_element->hierarchy_top.pp_discriminator_values[i]) {
			if (p_element->p_element_type->hierarchy_top.pp_discriminators[i]->cb(
					p_element->p_element_type->hierarchy_top.pp_discriminators[i]->annotation_key,
					0,
					p_memory_backend_pointer->ptr + p_element->id * p_element->p_element_type->byte_size,
					p_element->p_element_type->p_type,
					&p_element->hierarchy_top.pp_discriminator_values[i]->value,
					p_element->p_element_type->hierarchy_top.pp_discriminators[i]->p_data) < 0) {
				msg_stderr("discriminator %d returned negative result\n", (int)i);
				return (-EFAULT);
			}
		}
	}

	return 0;
}
/*****************************************************************************/
static int _memory_block_top_free_memory(
		struct memory_block_top *					p_memory_block_top) {

	int i;

	assert(p_memory_block_top);
	struct memory_backend_pointer * p_memory_backend_pointer =
		memory_backend_pointer(p_memory_block_top->memory_block.p_memory_backend_interface);

	if (p_memory_backend_pointer->ptr != NULL) {
		free(p_memory_backend_pointer->ptr);
		p_memory_backend_pointer->ptr = NULL;
	}

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

	if (_memory_backend_interface_create_from_pointer(
			p_memory_backend_pointer->ptr +
			p_element_hierarchy_member->p_element->id * p_element_hierarchy_member->p_element->p_element_type->byte_size +
			p_element_hierarchy_member->p_element_type_hierarchy_member->offset,
			pp_memory_backend_interface) != 0) {
		msg_stderr("_memory_backend_interface_create_from_pointer failed\n");
		return -(EFAULT);
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

	if (_memory_backend_interface_create_from_pointer(
			*((void**)(
					p_memory_backend_pointer->ptr +
					p_element_hierarchy_member->p_element->id * p_element_hierarchy_member->p_element->p_element_type->byte_size +
					p_element_hierarchy_member->p_element_type_hierarchy_member->offset)),
			pp_memory_backend_interface) != 0) {
		msg_stderr("_memory_backend_interface_create_from_pointer failed\n");
		return -(EFAULT);
	}

	return 0;
}

static int _element_hierarchy_member_get_array_subrange0(
		struct element_hierarchy_member *			p_element_hierarchy_member) {
	assert(p_element_hierarchy_member);
	assert(p_element_hierarchy_member->p_element_type_hierarchy_member);

	return element_array_get_array_subrange0(
			&p_element_hierarchy_member->array,
			&p_element_hierarchy_member->p_element_type_hierarchy_member->array);
}

static int _element_hierarchy_member_get_pointer_subrange0(
		struct element_hierarchy_member *			p_element_hierarchy_member) {
	assert(p_element_hierarchy_member);
	assert(p_element_hierarchy_member->p_element_type_hierarchy_member);

	return element_pointer_get_pointer_subrange0(
			&p_element_hierarchy_member->pointer,
			&p_element_hierarchy_member->p_element_type_hierarchy_member->pointer);
}

static int _element_hierarchy_member_cast_pointer(
		struct element_hierarchy_member *			p_element_hierarchy_member) {
	assert(p_element_hierarchy_member);
	assert(p_element_hierarchy_member->p_element_type_hierarchy_member);

	return element_pointer_cast_pointer(
			&p_element_hierarchy_member->pointer,
			&p_element_hierarchy_member->p_element_type_hierarchy_member->pointer);

}
/*****************************************************************************/
static inline void * memory_block_top_get_ptr(
		struct memory_block_top *					p_memory_block_top) {
	assert(p_memory_block_top);
	struct memory_backend_pointer * p_memory_backend_pointer =
			memory_backend_pointer(p_memory_block_top->memory_block.p_memory_backend_interface);
	return p_memory_backend_pointer?p_memory_backend_pointer->ptr:NULL;
}

static int memory_block_top_compare4qsort(
		const void *								_a,
		const void *								_b) {
	struct memory_block_top *a = *((struct memory_block_top **)_a);
	struct memory_block_top *b = *((struct memory_block_top **)_b);

	if (memory_block_top_get_ptr(a) < memory_block_top_get_ptr(b)) {
		return -1;
	}

	if (memory_block_top_get_ptr(a) == memory_block_top_get_ptr(b)) {

		if (memory_block_top_get_ptr(a) > memory_block_top_get_ptr(b)) {
			return -1;
		}

		if (memory_block_top_get_ptr(a) == memory_block_top_get_ptr(b)) {
			return 0;
		}

		return 1;
	}

	return 1;
}

static int _object_root_validate(
		struct object_root *						p_object_root) {

	int result = 0;
	metac_count_t i = 0;

	struct memory_block_top	** _tops;

	if (p_object_root->memory_block_tops_count == 0) {
		/*TODO: actually it's strange*/
		return 0;
	}

	_tops = (struct memory_block_top **)calloc(p_object_root->memory_block_tops_count, sizeof(*_tops));
	if (_tops == NULL) {
		msg_stderr("can't allocate memory for _tops\n");
		return (-ENOMEM);
	}

	for (i = 0; i < p_object_root->memory_block_tops_count; ++i) {
		_tops[i] = p_object_root->pp_memory_block_tops[i];
	}

	/*find unique regions*/
	qsort(_tops, p_object_root->memory_block_tops_count,
			sizeof(*_tops),
			memory_block_top_compare4qsort); /*TODO: change to hsort? - low prio so far*/

	for (i = 1; i < p_object_root->memory_block_tops_count; ++i) {
		if (memory_block_top_get_ptr(_tops[i-1]) == memory_block_top_get_ptr(_tops[i])) {
			msg_stderr("found the same ptr twice- error\n");
			result = -EFAULT;
			break;
		}
		if (	memory_block_top_get_ptr(_tops[i]) > memory_block_top_get_ptr(_tops[i-1]) &&
				memory_block_top_get_ptr(_tops[i]) < memory_block_top_get_ptr(_tops[i-1]) + _tops[i-1]->memory_block.byte_size) {
			msg_stderr("memory_block_tops are overlapping - error\n");
			result = -EFAULT;
			break;
		}
	}


	free(_tops);
	return result;
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

	.memory_block_top_free_memory =					_memory_block_top_free_memory,
	/* Optional handlers */
	.object_root_validate =							_object_root_validate,
};
/*****************************************************************************/
struct memory_backend_interface * memory_backend_interface_create_from_pointer(
		void *										ptr) {
	if (ptr == NULL) {
		return NULL;
	}

	_create_(memory_backend_pointer);

	memory_backend_interface(&p_memory_backend_pointer->memory_backend_interface, &ops);
	p_memory_backend_pointer->ptr = ptr;

	return &p_memory_backend_pointer->memory_backend_interface;
}
