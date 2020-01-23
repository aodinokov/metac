/*
 * memory_backend_interface.c
 *
 *  Created on: Jan 22, 2020
 *      Author: mralex
 */

#define METAC_DEBUG_ENABLE

#include <assert.h>			/* assert */
#include <errno.h>			/* ENOMEM etc */

#include "metac_internals.h"
#include "metac_debug.h"	/* msg_stderr, ...*/

int memory_backend_interface_delete(
		struct memory_backend_interface **			pp_memory_backend_interface) {

	if (pp_memory_backend_interface == NULL) {
		msg_stderr("pp_memory_backend_interface is NULL\n");
		return -(EINVAL);
	}

	if ((*pp_memory_backend_interface) == NULL) {
		msg_stddbg("p_memory_backend_interface is NULL\n");
		return -EALREADY;
	}

	if ((*pp_memory_backend_interface)->p_ops->memory_backend_interface_delete == NULL) {
		msg_stderr("memory_backend_interface_delete isn't defined\n");
		return -(ENOENT);
	}

	return (*pp_memory_backend_interface)->p_ops->memory_backend_interface_delete(pp_memory_backend_interface);
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

