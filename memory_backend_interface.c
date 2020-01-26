/*
 * memory_backend_interface.c
 *
 *  Created on: Jan 22, 2020
 *      Author: mralex
 */

#define METAC_DEBUG_ENABLE

#include <assert.h>			/* assert */
#include <errno.h>			/* ENOMEM etc */
#include <stdint.h>			/* UINT32_MAX */

#include "metac_internals.h"
#include "metac_debug.h"	/* msg_stderr, ...*/

int memory_backend_interface(
		struct memory_backend_interface *			p_memory_backend_interface,
		struct memory_backend_interface_ops *		p_memory_backend_interface_ops) {

	if (p_memory_backend_interface == NULL) {
		msg_stderr("p_memory_backend_interface is NULL\n");
		return -(EINVAL);
	}

	if (p_memory_backend_interface_ops == NULL) {
		msg_stderr("p_memory_backend_interface_ops is NULL\n");
		return -(EINVAL);
	}

	p_memory_backend_interface->p_ops = p_memory_backend_interface_ops;
	p_memory_backend_interface->_ref_count = 1;

	return 0;
}

int memory_backend_interface_get(
		struct memory_backend_interface *			p_memory_backend_interface,
		struct memory_backend_interface **			pp_memory_backend_interface) {

	if (p_memory_backend_interface == NULL) {
		msg_stderr("p_memory_backend_interface is NULL\n");
		return -(EINVAL);
	}

	assert(p_memory_backend_interface->_ref_count < UINT32_MAX);

	if (p_memory_backend_interface->_ref_count == UINT32_MAX) {
		msg_stderr("too many references\n");
		return -(EFAULT);
	}

	++p_memory_backend_interface->_ref_count;

	if (pp_memory_backend_interface != NULL) {
		*pp_memory_backend_interface = p_memory_backend_interface;
	}

	return 0;
}

int memory_backend_interface_put(
		struct memory_backend_interface **			pp_memory_backend_interface) {

	if (pp_memory_backend_interface == NULL) {
		msg_stderr("pp_memory_backend_interface is NULL\n");
		return -(EINVAL);
	}

	if ((*pp_memory_backend_interface) == NULL) {
		msg_stddbg("p_memory_backend_interface is NULL\n");
		return -EALREADY;
	}

	if ((--(*pp_memory_backend_interface)->_ref_count) > 0) {
		*pp_memory_backend_interface = NULL;	/*???*/
		return 0;
	}

	if ((*pp_memory_backend_interface)->p_ops->memory_backend_interface_delete == NULL) {
		msg_stderr("memory_backend_interface_delete isn't defined\n");
		return -(ENOENT);
	}

	return (*pp_memory_backend_interface)->p_ops->memory_backend_interface_delete(pp_memory_backend_interface);
}

int memory_backend_interface_equals(
		struct memory_backend_interface *			p_memory_backend_interface0,
		struct memory_backend_interface *			p_memory_backend_interface1) {

	if (p_memory_backend_interface0 == NULL ||
		p_memory_backend_interface1 == NULL) {
		msg_stderr("at least one off p_memory_backend_interfaces are NULL\n");
		return -(EINVAL);
	}

	if (p_memory_backend_interface0->p_ops != p_memory_backend_interface1->p_ops) {
		msg_stderr("backend interfaces have different types\n");
		return -(EINVAL);
	}

	if (p_memory_backend_interface0->p_ops->memory_backend_interface_equals == NULL) {
		msg_stderr("memory_backend_interface_equals isn't defined\n");
		return -(ENOENT);
	}

	return p_memory_backend_interface0->p_ops->memory_backend_interface_equals(
			p_memory_backend_interface0,
			p_memory_backend_interface1);
}

int element_get_memory_backend_interface(
		struct element *							p_element,
		struct memory_backend_interface **			pp_memory_backend_interface) {
	struct memory_backend_interface * p_memory_backend_interface;

	if (p_element == NULL) {
		msg_stderr("p_element is NULL\n");
		return -(EINVAL);
	}
	if (pp_memory_backend_interface == NULL) {
		msg_stderr("pp_memory_backend_interface is NULL\n");
		return -(EINVAL);
	}

	assert(p_element->p_memory_block);

	if (p_element->id >= p_element->p_memory_block->elements_count) {
		msg_stderr("id %d is invalid. elements_count is %d\n",
				(int)p_element->id,
				(int)p_element->p_memory_block->elements_count);
		return -(EFAULT);
	}

	assert(p_element->p_memory_block->elements_count > 0);
	assert(p_element->p_memory_block->p_memory_backend_interface);

	p_memory_backend_interface = p_element->p_memory_block->p_memory_backend_interface;
	if (p_memory_backend_interface->p_ops->element_get_memory_backend_interface == NULL) {
		msg_stderr("element_get_memory_backend_interface isn't defined\n");
		return -(ENOENT);
	}

	return p_memory_backend_interface->p_ops->element_get_memory_backend_interface(
			p_element,
			pp_memory_backend_interface);
}

int element_read_memory_backend_interface(
		struct element *							p_element,
		struct memory_backend_interface **			pp_memory_backend_interface) {
	struct memory_backend_interface * p_memory_backend_interface;

	if (p_element == NULL) {
		msg_stderr("p_element is NULL\n");
		return -(EINVAL);
	}
	if (pp_memory_backend_interface == NULL) {
		msg_stderr("pp_memory_backend_interface is NULL\n");
		return -(EINVAL);
	}

	assert(p_element->p_element_type);
	assert(p_element->p_element_type->p_actual_type);

	if (p_element->p_element_type->p_actual_type->id != DW_TAG_pointer_type) {
		msg_stderr("p_element isn't pointer\n");
		return -(EINVAL);
	}

	assert(p_element->p_memory_block);
	assert(p_element->p_memory_block->p_memory_backend_interface);

	p_memory_backend_interface = p_element->p_memory_block->p_memory_backend_interface;

	if (p_memory_backend_interface->p_ops->element_read_memory_backend_interface == NULL) {
		msg_stderr("element_get_memory_backend_interface isn't defined\n");
		return -(ENOENT);
	}

	return p_memory_backend_interface->p_ops->element_read_memory_backend_interface(
			p_element,
			pp_memory_backend_interface);
}

int	element_get_array_subrange0(
		struct element *							p_element) {
	struct memory_backend_interface * p_memory_backend_interface;

	if (p_element == NULL) {
		msg_stderr("p_element is NULL\n");
		return -(EINVAL);
	}

	assert(p_element->p_element_type);
	assert(p_element->p_element_type->p_actual_type);

	if (p_element->p_element_type->p_actual_type->id != DW_TAG_array_type) {
		msg_stderr("p_element isn't array\n");
		return -(EINVAL);
	}

	if (p_element->p_element_type->p_actual_type->array_type_info.is_flexible == 0) {
		msg_stderr("p_element isn't flexible array\n");
		return -(EINVAL);
	}

	assert(p_element->p_memory_block);
	assert(p_element->p_memory_block->p_memory_backend_interface);

	p_memory_backend_interface = p_element->p_memory_block->p_memory_backend_interface;

	if (p_memory_backend_interface->p_ops->element_get_array_subrange0 == NULL) {
		msg_stderr("element_get_array_subrange0 isn't defined\n");
		return -(ENOENT);
	}

	return p_memory_backend_interface->p_ops->element_get_array_subrange0(
			p_element);
}

int element_get_pointer_subrange0(
		struct element *							p_element) {
	struct memory_backend_interface * p_memory_backend_interface;

	if (p_element == NULL) {
		msg_stderr("p_element is NULL\n");
		return -(EINVAL);
	}

	assert(p_element->p_element_type);
	assert(p_element->p_element_type->p_actual_type);

	if (p_element->p_element_type->p_actual_type->id != DW_TAG_pointer_type) {
		msg_stderr("p_element isn't pointer\n");
		return -(EINVAL);
	}

	assert(p_element->p_memory_block);
	assert(p_element->p_memory_block->p_memory_backend_interface);

	p_memory_backend_interface = p_element->p_memory_block->p_memory_backend_interface;

	if (p_memory_backend_interface->p_ops->element_get_pointer_subrange0 == NULL) {
		msg_stderr("element_get_pointer_subrange0 isn't defined\n");
		return -(ENOENT);
	}

	return p_memory_backend_interface->p_ops->element_get_pointer_subrange0(
			p_element);
}

int element_cast_pointer(
		struct element *							p_element) {

	struct memory_backend_interface * p_memory_backend_interface;

	if (p_element == NULL) {
		msg_stderr("p_element is NULL\n");
		return -(EINVAL);
	}

	assert(p_element->p_element_type);
	assert(p_element->p_element_type->p_actual_type);

	if (p_element->p_element_type->p_actual_type->id != DW_TAG_pointer_type) {
		msg_stderr("p_element isn't pointer\n");
		return -(EINVAL);
	}

	assert(p_element->p_memory_block);
	assert(p_element->p_memory_block->p_memory_backend_interface);

	p_memory_backend_interface = p_element->p_memory_block->p_memory_backend_interface;

	if (p_memory_backend_interface->p_ops->element_cast_pointer == NULL) {
		msg_stderr("element_cast_pointer isn't defined\n");
		return -(ENOENT);
	}

	if (p_memory_backend_interface->p_ops->element_cast_pointer(
			p_element) != 0){
		msg_stderr("element_cast_pointer call failed\n");
		return -(EFAULT);
	}

	if (p_element->pointer.p_casted_element_type == NULL ||
		p_element->pointer.p_casted_memory_backend_interface == NULL) {
		msg_stderr("element_cast_pointer call was successful, but some mandatory fields were still empty\n");
		return -(EFAULT);
	}

	return 0;
}

int element_calculate_hierarchy_top_discriminator_values(
		struct element *							p_element) {
	struct memory_backend_interface * p_memory_backend_interface;

	if (p_element == NULL) {
		msg_stderr("p_element is NULL\n");
		return -(EINVAL);
	}

	assert(p_element->p_element_type);
	assert(p_element->p_element_type->p_actual_type);

	if (p_element->p_element_type->p_actual_type->id != DW_TAG_structure_type &&
		p_element->p_element_type->p_actual_type->id != DW_TAG_union_type) {
		msg_stderr("p_element isn't hierarchy(structure or union)\n");
		return -(EINVAL);
	}

	assert(p_element->p_memory_block);
	assert(p_element->p_memory_block->p_memory_backend_interface);

	p_memory_backend_interface = p_element->p_memory_block->p_memory_backend_interface;

	if (p_memory_backend_interface->p_ops->element_calculate_hierarchy_top_discriminator_values == NULL) {
		msg_stderr("element_calculate_discriminators isn't defined\n");
		return -(ENOENT);
	}

	return p_memory_backend_interface->p_ops->element_calculate_hierarchy_top_discriminator_values(
			p_element);
}

int element_hierarchy_member_get_memory_backend_interface(
		struct element_hierarchy_member *			p_element_hierarchy_member,
		struct memory_backend_interface **			pp_memory_backend_interface) {
	struct memory_backend_interface * p_memory_backend_interface;

	if (p_element_hierarchy_member == NULL) {
		msg_stderr("p_element_hierarchy_member is NULL\n");
		return -(EINVAL);
	}
	if (pp_memory_backend_interface == NULL) {
		msg_stderr("pp_memory_backend_interface is NULL\n");
		return -(EINVAL);
	}

	assert(p_element_hierarchy_member->p_element);
	assert(p_element_hierarchy_member->p_element->p_memory_block);

	if (p_element_hierarchy_member->p_element->id >= p_element_hierarchy_member->p_element->p_memory_block->elements_count) {
		msg_stderr("id %d is invalid. elements_count is %d\n",
				(int)p_element_hierarchy_member->p_element->id,
				(int)p_element_hierarchy_member->p_element->p_memory_block->elements_count);
		return -(EFAULT);
	}

	assert(p_element_hierarchy_member->p_element->p_memory_block->elements_count > 0);
	assert(p_element_hierarchy_member->p_element->p_memory_block->p_memory_backend_interface);

	p_memory_backend_interface = p_element_hierarchy_member->p_element->p_memory_block->p_memory_backend_interface;

	if (p_memory_backend_interface->p_ops->element_hierarchy_member_get_memory_backend_interface == NULL) {
		msg_stderr("element_hierarchy_member_get_memory_backend_interface isn't defined\n");
		return -(ENOENT);
	}

	return p_memory_backend_interface->p_ops->element_hierarchy_member_get_memory_backend_interface(
			p_element_hierarchy_member,
			pp_memory_backend_interface);
}

int element_hierarchy_member_read_memory_backend_interface(
		struct element_hierarchy_member *			p_element_hierarchy_member,
		struct memory_backend_interface **			pp_memory_backend_interface) {
	struct memory_backend_interface * p_memory_backend_interface;

	if (p_element_hierarchy_member == NULL) {
		msg_stderr("p_element_hierarchy_member is NULL\n");
		return -(EINVAL);
	}
	if (pp_memory_backend_interface == NULL) {
		msg_stderr("pp_memory_backend_interface is NULL\n");
		return -(EINVAL);
	}

	assert(p_element_hierarchy_member->p_element_type_hierarchy_member);
	assert(p_element_hierarchy_member->p_element_type_hierarchy_member->p_actual_type);

	if (p_element_hierarchy_member->p_element_type_hierarchy_member->p_actual_type->id != DW_TAG_pointer_type) {
		msg_stderr("p_element_hierarchy_member isn't pointer\n");
		return -(EINVAL);
	}

	assert(p_element_hierarchy_member->p_element);
	assert(p_element_hierarchy_member->p_element->p_memory_block);
	assert(p_element_hierarchy_member->p_element->p_memory_block->p_memory_backend_interface);

	p_memory_backend_interface = p_element_hierarchy_member->p_element->p_memory_block->p_memory_backend_interface;

	if (p_memory_backend_interface->p_ops->element_hierarchy_member_read_memory_backend_interface == NULL) {
		msg_stderr("element_hierarchy_member_get_memory_backend_interface isn't defined\n");
		return -(ENOENT);
	}

	return p_memory_backend_interface->p_ops->element_hierarchy_member_read_memory_backend_interface(
			p_element_hierarchy_member,
			pp_memory_backend_interface);
}

int element_hierarchy_member_get_array_subrange0(
		struct element_hierarchy_member *			p_element_hierarchy_member) {
	struct memory_backend_interface * p_memory_backend_interface;

	if (p_element_hierarchy_member == NULL) {
		msg_stderr("p_element_hierarchy_member is NULL\n");
		return -(EINVAL);
	}

	assert(p_element_hierarchy_member->p_element_type_hierarchy_member);
	assert(p_element_hierarchy_member->p_element_type_hierarchy_member->p_actual_type);

	if (p_element_hierarchy_member->p_element_type_hierarchy_member->p_actual_type->id != DW_TAG_array_type) {
		msg_stderr("p_element_hierarchy_member isn't array\n");
		return -(EINVAL);
	}

	if (p_element_hierarchy_member->p_element_type_hierarchy_member->p_actual_type->array_type_info.is_flexible == 0) {
		msg_stderr("p_element_hierarchy_member isn't flexible array\n");
		return -(EINVAL);
	}

	assert(p_element_hierarchy_member->p_element);
	assert(p_element_hierarchy_member->p_element->p_memory_block);
	assert(p_element_hierarchy_member->p_element->p_memory_block->p_memory_backend_interface);

	p_memory_backend_interface = p_element_hierarchy_member->p_element->p_memory_block->p_memory_backend_interface;

	if (p_memory_backend_interface->p_ops->element_hierarchy_member_get_array_subrange0 == NULL) {
		msg_stderr("element_hierarchy_member_get_array_subrange0 isn't defined\n");
		return -(ENOENT);
	}

	return p_memory_backend_interface->p_ops->element_hierarchy_member_get_array_subrange0(
			p_element_hierarchy_member);
}

int element_hierarchy_member_get_pointer_subrange0(
		struct element_hierarchy_member *			p_element_hierarchy_member) {
	struct memory_backend_interface * p_memory_backend_interface;

	if (p_element_hierarchy_member == NULL) {
		msg_stderr("p_element_hierarchy_member is NULL\n");
		return -(EINVAL);
	}

	assert(p_element_hierarchy_member->p_element_type_hierarchy_member);
	assert(p_element_hierarchy_member->p_element_type_hierarchy_member->p_actual_type);

	if (p_element_hierarchy_member->p_element_type_hierarchy_member->p_actual_type->id != DW_TAG_pointer_type) {
		msg_stderr("p_element_hierarchy_member isn't array\n");
		return -(EINVAL);
	}

	assert(p_element_hierarchy_member->p_element);
	assert(p_element_hierarchy_member->p_element->p_memory_block);
	assert(p_element_hierarchy_member->p_element->p_memory_block->p_memory_backend_interface);

	p_memory_backend_interface = p_element_hierarchy_member->p_element->p_memory_block->p_memory_backend_interface;

	if (p_memory_backend_interface->p_ops->element_hierarchy_member_get_pointer_subrange0 == NULL) {
		msg_stderr("element_hierarchy_member_get_pointer_subrange0 isn't defined\n");
		return -(ENOENT);
	}

	return p_memory_backend_interface->p_ops->element_hierarchy_member_get_pointer_subrange0(
			p_element_hierarchy_member);
}

int element_hierarchy_member_cast_pointer(
		struct element_hierarchy_member *			p_element_hierarchy_member) {
	struct memory_backend_interface * p_memory_backend_interface;

	if (p_element_hierarchy_member == NULL) {
		msg_stderr("p_element_hierarchy_member is NULL\n");
		return -(EINVAL);
	}

	assert(p_element_hierarchy_member->p_element_type_hierarchy_member);
	assert(p_element_hierarchy_member->p_element_type_hierarchy_member->p_actual_type);

	if (p_element_hierarchy_member->p_element_type_hierarchy_member->p_actual_type->id != DW_TAG_pointer_type) {
		msg_stderr("p_element_hierarchy_member isn't array\n");
		return -(EINVAL);
	}

	assert(p_element_hierarchy_member->p_element);
	assert(p_element_hierarchy_member->p_element->p_memory_block);
	assert(p_element_hierarchy_member->p_element->p_memory_block->p_memory_backend_interface);

	p_memory_backend_interface = p_element_hierarchy_member->p_element->p_memory_block->p_memory_backend_interface;

	if (p_memory_backend_interface->p_ops->element_hierarchy_member_cast_pointer == NULL) {
		msg_stderr("element_hierarchy_member_cast_pointer isn't defined\n");
		return -(ENOENT);
	}

	return p_memory_backend_interface->p_ops->element_hierarchy_member_cast_pointer(
			p_element_hierarchy_member);
}

