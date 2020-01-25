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
//static struct memory_backend_pointer * memory_backend_pointer_create_from_pointer(
//		void *										ptr);
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
#if 0
static int element_array_init(
		struct element_array *						p_element_array,
		struct element_type_array *					p_element_type_array,
		char *										path_within_memory_block_top,
		struct element *							p_containing_element,
		struct element_hierarchy_member *			p_containing_element_hierarchy_member,
//		void *										actual_ptr,

		struct metac_type *							p_actual_type) {
	p_element_array->actual_ptr = actual_ptr;

	if (p_actual_type->array_type_info.is_flexible != 0) {

		if (p_element_type_array->array_elements_count.cb == NULL) {
			msg_stderr("Flexible array length callback isn't defined for %s\n", path_within_memory_block_top);
			return (-EFAULT);
		}

		p_element_array->is_flexible = 1;

		if (p_element_type_array->array_elements_count.cb(
				p_element_type_array->annotation_key,
				0,
				actual_ptr,
				p_element_array->actual_ptr,
				/*TODO: take the next params from the global context*/
				/*FIXME:*/p_element_array->actual_ptr,
				/*FIXME:*/p_actual_type,
				&p_element_array->subrange0_count,
				p_element_type_array->array_elements_count.p_data) != 0) {
			msg_stderr("Flexible array length callback failed for %s\n", path_within_memory_block_top);
			return (-EFAULT);
		}

		msg_stddbg("cb returned subrange0_count %d", (int)p_element_array->subrange0_count);
		/*
		 * flexible will work only if p_memory_block->element_count == 1 all the way here
		 * check for the incorrect combinations
		 * */
		if (element_array_check_parents(
				p_element_array,
				path_within_memory_block_top,
				p_containing_element,
				p_containing_element_hierarchy_member) != 0) {

			if (p_element_array->subrange0_count > 0) {
				msg_stderr("Flexible array can't have size %d, that is > 0, because parent arrays already had size >1 %s\n",
						(int)p_element_array->subrange0_count,
						path_within_memory_block_top);
				return (-EFAULT);
			}
		}

	}else{
		  metac_type_array_subrange_count(p_actual_type, 0, &p_element_array->subrange0_count);
	}

	/*init p_array_info*/
	p_element_array->p_array_info = metac_array_info_create_from_type(p_actual_type, p_element_array->subrange0_count);
	if (p_element_array->p_array_info == NULL) {
		msg_stderr("metac_array_info_create_from_type failed for %s\n", path_within_memory_block_top);
		return (-EFAULT);
	}

	return 0;
}
#endif
#if 0
for (i = 0; i < p_element->p_element_type->hierarchy_top.discriminators_count; ++i) {
	metac_flag allocate = 0;

	assert(p_element->p_element_type->hierarchy_top.pp_discriminators[i] != NULL);
	/*assumption: if we initialize all discriminators in order of their appearance - we won't need to resolve dependencides*/
	assert(
		p_element->p_element_type->hierarchy_top.pp_discriminators[i]->precondition.p_discriminator == NULL ||
		p_element->p_element_type->hierarchy_top.pp_discriminators[i]->precondition.p_discriminator->id < i);
	/*Not needed since it's checked with the previous assert:*/
	/*assert(
		p_element->p_element_type->hierarchy_top.pp_discriminators[i]->precondition.p_discriminator == NULL ||
		p_element->p_element_type->hierarchy_top.pp_discriminators[i]->precondition.p_discriminator->id < p_element->p_element_type->hierarchy_top.discriminators_count);*/

	if (p_element->p_element_type->hierarchy_top.pp_discriminators[i]->cb == NULL) {
		msg_stderr("discriminator %d for path %s is NULL\n", (int)i, path_within_memory_block_top);
		element_clean_hierarchy_top(p_element);
		return (-EFAULT);
	}

	if (	allocate == 0 &&
			p_element->p_element_type->hierarchy_top.pp_discriminators[i]->precondition.p_discriminator == NULL) {
		allocate = 1;
	}

	if (	allocate == 0 &&
			p_element->p_element_type->hierarchy_top.pp_discriminators[i]->precondition.p_discriminator != NULL) {
		struct discriminator_value * p_value =
				p_element->hierarchy_top.pp_discriminator_values[p_element->p_element_type->hierarchy_top.pp_discriminators[i]->precondition.p_discriminator->id];
		if (	p_value != NULL &&
				p_value->value == p_element->p_element_type->hierarchy_top.pp_discriminators[i]->precondition.expected_discriminator_value) {
			allocate = 1;
		}
	}

	if (allocate != 0) {
		p_element->hierarchy_top.pp_discriminator_values[i] = discriminator_value_create(0);
		if (p_element->hierarchy_top.pp_discriminator_values[i] == NULL) {
			msg_stderr("can't allocated pp_discriminator_values %d for path %s\n", (int)i, path_within_memory_block_top);
			element_clean_hierarchy_top(p_element);
			return (-EFAULT);
		}
		if (p_element->p_element_type->hierarchy_top.pp_discriminators[i]->cb(
				p_element->p_element_type->hierarchy_top.pp_discriminators[i]->annotation_key,
				0,
				ptr,
				p_element->p_element_type->p_type,
				&p_element->hierarchy_top.pp_discriminator_values[i]->value,
				p_element->p_element_type->hierarchy_top.pp_discriminators[i]->p_data) < 0) {
			msg_stderr("discriminator %d returned negative result for path %s\n", (int)i, path_within_memory_block_top);
			element_clean_hierarchy_top(p_element);
			return (-EFAULT);
		}
	}
}
#endif
#if 0
//static int memory_block_top_compare4qsort(
//		const void *								_a,
//		const void *								_b) {
//	struct memory_block_top *a = *((struct memory_block_top **)_a);
//	struct memory_block_top *b = *((struct memory_block_top **)_b);
//
//	if (a->memory_block.ptr < b->memory_block.ptr) {
//		return -1;
//	}
//
//	if (a->memory_block.ptr == b->memory_block.ptr) {
//
//		if (a->memory_block.byte_size > b->memory_block.byte_size) {
//			return -1;
//		}
//
//		if (a->memory_block.byte_size == b->memory_block.byte_size) {
//			return 0;
//		}
//
//		return 1;
//	}
//
//	return 1;
//}
//static int object_root_builder_validate(
//		struct object_root_builder *				p_object_root_builder) {
//	/* TODO: validation works only for real pointers - should be taken to drivers */
//	int result = 0;
//	metac_count_t i = 0;
//	metac_count_t _count = 0;
//	struct memory_block_top	** _tops;
//	struct object_root_container_memory_block_top * _memory_block_top;
//
//	/*get arrays lengths */
//	cds_list_for_each_entry(_memory_block_top, &p_object_root_builder->container.memory_block_top_list, list) {
//		++_count;
//	}
//	if (_count == 0) {
//		return 0;
//	}
//
//	_tops = (struct memory_block_top	**)calloc(_count, sizeof(*_tops));
//	if (_tops == NULL) {
//		msg_stderr("can't allocate memory for _tops\n");
//		return (-ENOMEM);
//	}
//	cds_list_for_each_entry(_memory_block_top, &p_object_root_builder->container.memory_block_top_list, list) {
//		_tops[i] = _memory_block_top->p_memory_block_top;
//		++i;
//	}
//
//	/*find unique regions*/
//	qsort(_tops, _count,
//			sizeof(*_tops),
//			memory_block_top_compare4qsort); /*TODO: change to hsort? - low prio so far*/
//
//	for (i = 1; i < _count; ++i) {
//		if (_tops[i-1]->memory_block.ptr == _tops[i]->memory_block.ptr) {
//			msg_stderr("found the same ptr twice- error\n");
//			result = -EFAULT;
//			break;
//		}
//		if (	_tops[i]->memory_block.ptr > _tops[i-1]->memory_block.ptr &&
//				_tops[i]->memory_block.ptr < _tops[i-1]->memory_block.ptr + _tops[i-1]->memory_block.byte_size) {
//			msg_stderr("memory_block_tops are overlapping - error\n");
//			result = -EFAULT;
//			break;
//		}
//	}
//
//	free(_tops);
//	return result;
//}
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
