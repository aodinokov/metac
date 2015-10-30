/*
 * metac_type.c
 *
 *  Created on: Aug 31, 2015
 *      Author: mralex
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "metac_type.h"
#include "metac_debug.h"

unsigned int 			metac_type_child_num(struct metac_type *type) {
	if (type == NULL)
		return 0;
	return type->child_num;
}

struct metac_type* 		metac_type_child(struct metac_type *type, unsigned int id) {
	if (type == NULL)
		return NULL;
	if (id >= type->child_num)
		return NULL;

	return type->child[id];
}

unsigned int 			metac_type_at_num(struct metac_type *type) {
	if (type == NULL)
		return 0;
	return type->at_num;
}

struct metac_type_at* 	metac_type_at(struct metac_type *type, unsigned int id)  {
	if (type == NULL)
		return NULL;
	if (id >= type->at_num)
		return NULL;

	return &type->at[id];
}

struct metac_type_at* metac_type_at_by_key(struct metac_type *type, int key) {
	int i;

	if (type == NULL)
		return NULL;

	for (i = 0; i < metac_type_at_num(type); i++) {
		struct metac_type_at* at = metac_type_at(type, i);
		if (at != NULL && at->key == key)
			return at;
	}
	return NULL;
}

unsigned int metac_type_byte_size(struct metac_type *type) {
	assert(type);
	type = metac_type_typedef_skip(type);
	assert(type);

	switch(type->type) {
	case DW_TAG_base_type:
	case DW_TAG_enumeration_type:
	case DW_TAG_structure_type:
	case DW_TAG_union_type:
		{
			struct metac_type_at * at_byte_size = metac_type_at_by_key(type, DW_AT_byte_size);
			assert(at_byte_size != NULL);
			return at_byte_size->byte_size;
		}while(0);
		break;
	case DW_TAG_pointer_type:
		{
			struct metac_type_at * at_byte_size = metac_type_at_by_key(type, DW_AT_byte_size);
			if (at_byte_size != NULL)
				return at_byte_size->byte_size;
			return sizeof(void*);
		}while(0);
		break;
	case DW_TAG_array_type:
		{
			struct metac_type_at * at_type;
			unsigned int elements_num = metac_type_array_length(type);

			at_type = metac_type_at_by_key(type, DW_AT_type);
			if (at_type == NULL) {
				msg_stderr("array has to contain type at\n");
				return 0;
			}
			return elements_num * metac_type_byte_size(at_type->type);


		}while(0);
		break;
	case DW_TAG_subprogram:
		return 1;	/*sizeof function == 1*/
	}
	return 0;
}

struct metac_type *metac_type_typedef_skip(struct metac_type *type) {
	assert(type);
	if (type->type == DW_TAG_typedef){
		struct metac_type_at * at_type;
		at_type = metac_type_at_by_key(type, DW_AT_type);
		if (at_type == NULL) {
			msg_stderr("typedef has to contain type at\n");
			return NULL;
		}
		return metac_type_typedef_skip(at_type->type);
	}
	return type;
}

struct metac_type * metac_type_member_type(struct metac_type *type) {
	struct metac_type_at * at_type;
	assert(type);
	if (type->type != DW_TAG_member) {
		msg_stderr("expected type DW_TAG_member\n");
		return NULL;
	}
	at_type = metac_type_at_by_key(type, DW_AT_type);
	if (at_type == NULL)
		return NULL;

	return at_type->type;
}

char * 		metac_type_member_name(struct metac_type *type)  {
	struct metac_type_at * at_name;
	assert(type);
	if (type->type != DW_TAG_member) {
		msg_stderr("expected type DW_TAG_member\n");
		return NULL;
	}
	at_name = metac_type_at_by_key(type, DW_AT_name);
	if (at_name == NULL)
		return NULL;

	return at_name->name;
}

int 		metac_type_member_offset(struct metac_type *type, unsigned int * p_offset) {
	struct metac_type_at * at_data_member_location;
	assert(type);
	if (type->type != DW_TAG_member) {
		msg_stderr("expected type DW_TAG_member\n");
		return -1;
	}
	at_data_member_location = metac_type_at_by_key(type, DW_AT_data_member_location);
	if (at_data_member_location == NULL)
		return -1;

	if (p_offset)
		*p_offset = at_data_member_location->data_member_location;
	return 0;
}

int 		metac_type_member_bit_attr(struct metac_type *type, unsigned int * p_bit_offset, unsigned int * p_bit_size) {
	struct metac_type_at * at_bit_offset,
		* at_bit_size;
	assert(type);
	if (type->type != DW_TAG_member) {
		msg_stderr("expected type DW_TAG_member\n");
		return -1;
	}
	at_bit_offset = metac_type_at_by_key(type, DW_AT_bit_offset);
	at_bit_size = metac_type_at_by_key(type, DW_AT_bit_size);
	if (at_bit_offset == NULL || at_bit_size == NULL)
		return -1;

	if (p_bit_offset)
		*p_bit_offset = at_bit_offset->bit_offset;
	if (p_bit_size)
		*p_bit_size = at_bit_size->bit_size;
	return 0;
}


unsigned int metac_type_structure_member_count(struct metac_type *type) {
#ifndef	NDEBUG
	unsigned int i;
#endif

	assert(type);
	type = metac_type_typedef_skip(type);
	assert(type);

	if (type->type != DW_TAG_structure_type) {
		msg_stderr("expected type DW_TAG_structure_type\n");
		return 0;
	}
#ifndef	NDEBUG
	for (i = 0; i < metac_type_child_num(type); i++) {
		struct metac_type *child = metac_type_child(type, i);
		assert(child);
		assert(child->type == DW_TAG_member);
	}
#endif
	return metac_type_child_num(type);
}

struct metac_type * metac_type_structure_member(struct metac_type *type, unsigned int id) {
#ifndef	NDEBUG
	unsigned int i;
#endif

	assert(type);
	type = metac_type_typedef_skip(type);
	assert(type);

	if (type->type != DW_TAG_structure_type) {
		msg_stderr("expected type DW_TAG_structure_type\n");
		return NULL;
	}
#ifndef	NDEBUG
	for (i = 0; i < metac_type_child_num(type); i++) {
		struct metac_type *child = metac_type_child(type, i);
		assert(child);
		assert(child->type == DW_TAG_member);
	}
#endif

	return metac_type_child(type, id);
}

struct metac_type *		metac_type_structure_member_type(struct metac_type *type, unsigned int id) {
	struct metac_type * member = metac_type_structure_member(type, id);
	if (member == NULL)
		return NULL;
	return metac_type_member_type(member);
}

char *					metac_type_structure_member_name(struct metac_type *type, unsigned int id) {
	struct metac_type * member = metac_type_structure_member(type, id);
	if (member == NULL)
		return NULL;
	return metac_type_member_name(member);
}

int						metac_type_structure_member_offset(struct metac_type *type, unsigned int id, unsigned int * p_offset) {
	struct metac_type * member = metac_type_structure_member(type, id);
	if (member == NULL)
		return -1;
	return metac_type_member_offset(member, p_offset);
}

int						bit_attr(struct metac_type *type, unsigned int id, unsigned int * p_bit_offset, unsigned int * p_bit_size) {
	struct metac_type * member = metac_type_structure_member(type, id);
	if (member == NULL)
		return -1;
	return metac_type_member_bit_attr(member, p_bit_offset, p_bit_size);
}


unsigned int metac_type_array_length(struct metac_type *type) {
	unsigned int i;
	unsigned int length = 0;
	assert(type);
	type = metac_type_typedef_skip(type);
	assert(type);
	if (type->type != DW_TAG_array_type) {
		msg_stderr("expected type DW_TAG_array_type\n");
		return 0;
	}

	for (i = 0; i < metac_type_child_num(type); i++) {
		struct metac_type_at
			* at_lower_bound,
			* at_upper_bound;
		struct metac_type *child = metac_type_child(type, i);
		assert(child->type == DW_TAG_subrange_type);

		/* get range parameter */
		at_lower_bound = metac_type_at_by_key(child, DW_AT_lower_bound); /*optional*/
		at_upper_bound = metac_type_at_by_key(child, DW_AT_upper_bound); /*mandatory?*/
		if (at_upper_bound == NULL) {
			msg_stderr("upper_bound is mandatory\n");
			return 0;
		}
		length += at_upper_bound->upper_bound + 1;
		if (at_lower_bound)
			length -= at_lower_bound->lower_bound;

	}
	return length;
}

struct metac_type *	metac_type_subprogram_return_type(struct metac_type *type) {
	struct metac_type_at * res;

	if (type->type != DW_TAG_subprogram) {
		msg_stderr("_DW_TAG_subprogram_ expected type == DW_TAG_subprogram\n");
		return NULL;
	}

	res = metac_type_at_by_key(type, DW_AT_type);
	assert(res);

	return res->type;
}

int 					metac_type_subprogram_parameter_count(struct metac_type *type) {
	int i;
	int count = 0;

	if (type->type != DW_TAG_subprogram) {
		msg_stderr("_DW_TAG_subprogram_ expected type == DW_TAG_subprogram\n");
		return -1;
	}

	for (i = 0; i < metac_type_child_num(type); i++) {
		struct metac_type *child = metac_type_child(type, i);
		if (child->type == DW_TAG_formal_parameter)
			count++;
	}
	return count;
}

struct metac_type *	metac_type_subprogram_parameter(struct metac_type *type, unsigned int id) {
	int i, j = 0;
	struct metac_type * res = NULL;

	if (type->type != DW_TAG_subprogram) {
		msg_stderr("_DW_TAG_subprogram_ expected type == DW_TAG_subprogram\n");
		return NULL;
	}

	for (i = 0; i < metac_type_child_num(type); i++) {
		struct metac_type *child = metac_type_child(type, i);
		if (child->type == DW_TAG_formal_parameter) {
			if (j == id) {
				res = child;
				break;
			}
			/* TODO: in fact we can do it without loop if this confistion is always true*/
			assert(child->type == DW_TAG_formal_parameter);

			j++;
		}
	}

	/*TODO: common code with the next function? - to a separete function */
	if (res) { /* formal parameter found */
		struct metac_type_at * at_type;
		at_type = metac_type_at_by_key(res, DW_AT_type);

		res = NULL;
		if (at_type)
			res = at_type->type;

	}

	return res;
}

struct metac_type *	metac_type_subprogram_parameter_by_name(struct metac_type *type, const char *parameter_name) {
	int i, j = 0;
	struct metac_type * res = NULL;

	if (type->type != DW_TAG_subprogram) {
		msg_stderr("_DW_TAG_subprogram_ expected type == DW_TAG_subprogram\n");
		return NULL;
	}

	for (i = 0; i < metac_type_child_num(type); i++) {
		struct metac_type *child = metac_type_child(type, i);
		if (child->type == DW_TAG_formal_parameter) {
			struct metac_type_at * at;

			at = metac_type_at_by_key(child, DW_AT_name);
			assert(at);
			if (at && strcmp(parameter_name, at->name) == 0) {
				res = child;
				break;
			}
			j++;
		}
	}

	/*TODO: common code with the previous function? - to a separate function */
	if (res) { /* formal parameter found */
		struct metac_type_at * at;
		at = metac_type_at_by_key(res, DW_AT_type);

		res = NULL;
		if (at)
			res = at->type;

	}

	return res;
}
