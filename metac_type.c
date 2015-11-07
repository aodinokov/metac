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

int metac_type_at_map(struct metac_type *type, metac_type_at_map_func_t map_func, void * data) {
	int i;

	if (type == NULL)
		return -1;

	for (i = 0; i < metac_type_at_num(type); i++) {
		struct metac_type_at* at = metac_type_at(type, i);
		if (at != NULL && map_func(type,  metac_type_at(type, i), data) != 0)
			return 0;
	}
	return 0;
}

struct metac_type_at_by_key_func_data {
	int key;
	struct metac_type_at* result;
};
static int metac_type_at_by_key_func(struct metac_type *type, struct metac_type_at *at, void * data) {
	if (at->key == ((struct metac_type_at_by_key_func_data*)data)->key) {
		((struct metac_type_at_by_key_func_data*)data)->result = at;
		return 1;
	}
	return 0;
}

struct metac_type_at* metac_type_at_by_key(struct metac_type *type, int key) {
	struct metac_type_at_by_key_func_data data = {
			.key = key,
			.result = NULL,
	};
	metac_type_at_map(type, metac_type_at_by_key_func, &data);
	return data.result;
// easy implementation based on nested functions (GCC)
//	struct metac_type_at* res = NULL;
//	int _func_(struct metac_type *type, struct metac_type_at *at, void * data) {
//		if (at->key == key) {
//			res = at;
//			return 1;
//		}
//		return 0;
//	}
//	metac_type_at_map(type, _func_, NULL);
//	return res;
}


char *					metac_type_name(struct metac_type *type) {
	struct metac_type_at* at_name;
	assert(type);

	at_name = metac_type_at_by_key(type, DW_AT_name);
	if (at_name)
		return at_name->name;
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


struct metac_type_member_info_func_data {
	struct metac_type_at * at_name;
	struct metac_type_at * at_type;
	struct metac_type_at * at_data_member_location;
	struct metac_type_at * at_bit_offset;
	struct metac_type_at * at_bit_size;
};
static int metac_type_member_info_func(struct metac_type *type, struct metac_type_at *at, void * data) {
	struct metac_type_member_info_func_data * p =(struct metac_type_member_info_func_data*)data;
	switch(at->key){
	case DW_AT_name:
		p->at_name = at;
		break;
	case DW_AT_type:
		p->at_type = at;
		break;
	case DW_AT_data_member_location:
		p->at_data_member_location = at;
		break;
	case DW_AT_bit_offset:
		p->at_bit_offset = at;
		break;
	case DW_AT_bit_size:
		p->at_bit_size = at;
		break;
	}
	return ((p->at_name != NULL) &&
			(p->at_type != NULL) &&
			(p->at_data_member_location != NULL) &&
			(p->at_bit_offset != NULL) &&
			(p->at_bit_size != NULL))?1:0;
}

int metac_type_member_info(struct metac_type *type, struct metac_type_member_info * p_member_info) {
	struct metac_type_member_info_func_data data = {
			.at_name = NULL,
			.at_type = NULL,
			.at_data_member_location = NULL,
			.at_bit_offset = NULL,
			.at_bit_size = NULL,
	};
	assert(type);
	if (type->type != DW_TAG_member) {
		msg_stderr("expected type DW_TAG_member\n");
		return -1;
	}

	metac_type_at_map(type, metac_type_member_info_func, &data);
	if (	data.at_name == NULL ||
			data.at_type == NULL ||
			data.at_data_member_location == NULL) {
		msg_stderr("mandatory fields are absent\n");
		return -1;
	}

	if (p_member_info != NULL) {
		p_member_info->name = data.at_name->name;
		p_member_info->type = data.at_type->type;
		p_member_info->p_data_member_location = &data.at_data_member_location->data_member_location;
		p_member_info->p_bit_offset = &data.at_bit_offset->bit_offset;
		p_member_info->p_bit_size = &data.at_bit_offset->bit_size;
	}
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

struct metac_type *		metac_type_structure_member_by_name(struct metac_type *type, const char *parameter_name) {
	int i;
	assert(type);
	type = metac_type_typedef_skip(type);
	assert(type);

	if (type->type != DW_TAG_structure_type) {
		msg_stderr("expected type DW_TAG_structure_type\n");
		return NULL;
	}

	for (i = 0; i < metac_type_structure_member_count(type); i++){
		struct metac_type_at * at_name;
		struct metac_type * member_type = metac_type_structure_member(type, i);
		assert(member_type);
		at_name = metac_type_at_by_key(member_type, DW_AT_name);
		if (at_name != NULL &&
				strcmp(at_name->name, parameter_name) == 0) {
			return member_type;
		}
	}
	return NULL;
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
