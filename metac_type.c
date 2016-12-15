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

int metac_type_id(struct metac_type *type) {
	if (type == NULL)
		return -1;
	return type->id;
}

unsigned int metac_type_child_num(struct metac_type *type) {
	if (type == NULL)
		return 0;
	return type->child_num;
}

struct metac_type* metac_type_child(struct metac_type *type, unsigned int i) {
	if (type == NULL)
		return NULL;
	if (i >= type->child_num)
		return NULL;

	return type->child[i];
}

unsigned int metac_type_at_num(struct metac_type *type) {
	if (type == NULL)
		return 0;
	return type->at_num;
}

struct metac_type_at* 	metac_type_at(struct metac_type *type, unsigned int i)  {
	if (type == NULL)
		return NULL;
	if (i >= type->at_num)
		return NULL;

	return &type->at[i];
}

struct metac_type_p_at*	metac_type_p_at(struct metac_type *type) {
	if (type == NULL)
		return NULL;
	return &type->p_at;
}

int metac_type_at_map(struct metac_type *type, metac_type_at_map_func_t map_func, void * data) {
	int id;

	if (type == NULL || map_func == NULL)
		return -1;

	for (id = 0; id < type->at_num; id++) {
		struct metac_type_at* at = &type->at[id];

		if (at != NULL && map_func(type, at, data) != 0)
			return 0;
	}
	return 0;
}

struct metac_type_at* metac_type_at_by_key(struct metac_type *type, metac_type_at_id_t id) {
	if (type == NULL)
		return NULL;

	switch (id) {
#define HANDLE_KEY(_key_) case DW_AT_ ##_key_ : return type->p_at.at_ ## _key_
	HANDLE_KEY(name);
	HANDLE_KEY(type);
	HANDLE_KEY(byte_size);
	HANDLE_KEY(encoding);
	HANDLE_KEY(data_member_location);
	HANDLE_KEY(bit_offset);
	HANDLE_KEY(bit_size);
	HANDLE_KEY(lower_bound);
	HANDLE_KEY(upper_bound);
	HANDLE_KEY(const_value);
	}
	return NULL;
}

char*	metac_type_name(struct metac_type *type) {
	if (type == NULL || type->p_at.at_name == NULL)
		return NULL;
	return type->p_at.at_name->name;
}

struct metac_type *metac_type_typedef_skip(struct metac_type *type) {
	if (type == NULL)
		return NULL;

	if (type->id == DW_TAG_typedef) {
		if (type->p_at.at_type == NULL || type->p_at.at_type->type == NULL) {
			msg_stderr("typedef has to contain type in attributes\n");
			return NULL;
		}
		return metac_type_typedef_skip(type->p_at.at_type->type);
	}
	return type;
}

/*--------- need further refactoring ---------*/
unsigned int metac_type_byte_size(struct metac_type *type) {
	assert(type);
	type = metac_type_typedef_skip(type);
	assert(type);

	switch(type->id) {
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
			unsigned int elements_num =
					metac_type_array_length(type);
			struct metac_type * element_type =
					metac_type_array_element_type(type);
			if (element_type == NULL) {
				msg_stderr("metac_type_array_element_type returned NULL\n");
				return 0;
			}
			return elements_num * metac_type_byte_size(element_type);


		}while(0);
		break;
	case DW_TAG_subprogram:
		return 1;	/*sizeof function == 1*/
	}
	return 0;
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
	switch(at->id){
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
	if (type->id != DW_TAG_member) {
		msg_stderr("expected type DW_TAG_member\n");
		return -1;
	}

	metac_type_at_map(type, metac_type_member_info_func, &data);
	if (	data.at_name == NULL ||
			data.at_type == NULL /*||
			data.at_data_member_location == NULL*/) {
		msg_stderr("mandatory fields are absent\n");
		return -1;
	}

	if (p_member_info != NULL) {
		p_member_info->name = data.at_name->name;
		p_member_info->type = data.at_type->type;
		p_member_info->p_data_member_location = data.at_data_member_location!=NULL?&data.at_data_member_location->data_member_location:NULL;
		p_member_info->p_bit_offset = data.at_bit_offset!=NULL?&data.at_bit_offset->bit_offset:NULL;
		p_member_info->p_bit_size = data.at_bit_size!=NULL?&data.at_bit_size->bit_size:NULL;
	}
	return 0;
}

unsigned int _metac_type_su_member_count(struct metac_type *type, int expected_type) {
#ifndef	NDEBUG
	unsigned int i;
#endif

	assert(type);
	type = metac_type_typedef_skip(type);
	assert(type);

	if ((expected_type >= 0 && metac_type_id(type) != expected_type) ||
		(expected_type < 0 && (metac_type_id(type) != DW_TAG_structure_type || metac_type_id(type) != DW_TAG_union_type))) {
		msg_stderr("incorrect type %d\n", (int)metac_type_id(type));
		return 0;
	}
#ifndef	NDEBUG
	for (i = 0; i < metac_type_child_num(type); i++) {
		struct metac_type *child = metac_type_child(type, i);
		assert(child);
		assert(child->id == DW_TAG_member);
	}
#endif
	return metac_type_child_num(type);
}

struct metac_type * _metac_type_su_member(struct metac_type *type, int expected_type, unsigned int id) {
#ifndef	NDEBUG
	unsigned int i;
#endif

	assert(type);
	type = metac_type_typedef_skip(type);
	assert(type);

	if ((expected_type >= 0 && metac_type_id(type) != expected_type) ||
		(expected_type < 0 && (metac_type_id(type) != DW_TAG_structure_type || metac_type_id(type) != DW_TAG_union_type))) {
		msg_stderr("incorrect type %d\n", (int)metac_type_id(type));
		return NULL;
	}
#ifndef	NDEBUG
	for (i = 0; i < metac_type_child_num(type); i++) {
		struct metac_type *child = metac_type_child(type, i);
		assert(child);
		assert(child->id == DW_TAG_member);
	}
#endif

	return metac_type_child(type, id);
}

struct metac_type *		_metac_type_su_member_by_name(struct metac_type *type, int expected_type, const char *parameter_name) {
	int i;
	assert(type);
	type = metac_type_typedef_skip(type);
	assert(type);

	if ((expected_type >= 0 && metac_type_id(type) != expected_type) ||
		(expected_type < 0 && (metac_type_id(type) != DW_TAG_structure_type || metac_type_id(type) != DW_TAG_union_type))) {
		msg_stderr("incorrect type %d\n", (int)metac_type_id(type));
		return NULL;
	}

	for (i = 0; i < _metac_type_su_member_count(type, expected_type); i++){
		struct metac_type_at * at_name;
		struct metac_type * member_type = _metac_type_su_member(type, expected_type, i);
		assert(member_type);
		at_name = metac_type_at_by_key(member_type, DW_AT_name);
		if (at_name != NULL &&
				strcmp(at_name->name, parameter_name) == 0) {
			return member_type;
		}
	}
	return NULL;
}

struct metac_type_subrange_info_func_data {
	struct metac_type_at * at_type;
	struct metac_type_at * at_lower_bound;
	struct metac_type_at * at_upper_bound;
};
static int metac_type_subrange_info_func(struct metac_type *type, struct metac_type_at *at, void * data) {
	struct metac_type_subrange_info_func_data * p =(struct metac_type_subrange_info_func_data*)data;
	switch(at->id){
	case DW_AT_type:
		p->at_type = at;
		break;
	case DW_AT_lower_bound:
		p->at_lower_bound = at;
		break;
	case DW_AT_upper_bound:
		p->at_upper_bound = at;
		break;
	}
	return ((p->at_type != NULL) &&
			(p->at_lower_bound != NULL) &&
			(p->at_upper_bound != NULL))?1:0;
}

int metac_type_subrange_info(struct metac_type *type, struct metac_type_subrange_info * p_subrange_info) {
	struct metac_type_subrange_info_func_data data = {
			.at_type = NULL,
			.at_lower_bound = NULL,
			.at_upper_bound = NULL,
	};
	assert(type);
	if (type->id != DW_TAG_subrange_type) {
		msg_stderr("expected type DW_TAG_subrange_type\n");
		return -1;
	}

	metac_type_at_map(type, metac_type_subrange_info_func, &data);
	if (	data.at_type == NULL) {
		msg_stderr("mandatory fields are absent\n");
		return -1;
	}

	if (p_subrange_info != NULL) {
		p_subrange_info->type = data.at_type->type;
		p_subrange_info->p_lower_bound = data.at_lower_bound!=NULL?&data.at_lower_bound->lower_bound:NULL;
		p_subrange_info->p_upper_bound = data.at_upper_bound!=NULL?&data.at_upper_bound->upper_bound:NULL;
	}
	return 0;
}


struct metac_type * metac_type_array_element_type(struct metac_type *type) {
	struct metac_type_at * at_type;

	assert(type);
	type = metac_type_typedef_skip(type);
	assert(type);
	if (metac_type_id(type) != DW_TAG_array_type) {
		msg_stderr("expected type DW_TAG_array_type\n");
		return NULL;
	}
	at_type = metac_type_at_by_key(type, DW_AT_type);
	if (at_type == NULL) {
		msg_stderr("array has to contain type at\n");
		return NULL;
	}
	return at_type->type;
}

int metac_type_array_element_info(struct metac_type *type, unsigned int N, struct metac_type_element_info *p_element_info){
	unsigned int i;
	unsigned int element_location = 0;

	assert(type);
	type = metac_type_typedef_skip(type);
	assert(type);
	if (metac_type_id(type) != DW_TAG_array_type) {
		msg_stderr("expected type DW_TAG_array_type\n");
		return -1;
	}

	for (i = 0; i < metac_type_array_subrange_count(type); i++) {
		struct metac_type_subrange_info subrange_info;
		if (metac_type_subrange_info(metac_type_array_subrange(type, i), &subrange_info ) != 0){
			msg_stderr("metac_type_subrange_info returned error\n");
			return -1;
		}
		if (	(i == 0 && subrange_info.p_upper_bound == NULL) || /* allow arrays without bounds to calc Nth element */
				(((subrange_info.p_lower_bound == NULL) ||
				(subrange_info.p_lower_bound != NULL  && *(subrange_info.p_lower_bound) <= N)) &&
				*(subrange_info.p_upper_bound) >= N)) { /* found range */
			element_location +=N;
			if (p_element_info) {
				p_element_info->type = metac_type_array_element_type(type);
				assert(p_element_info->type != NULL);
				p_element_info->element_location = element_location * metac_type_byte_size(p_element_info->type);
			}
			return 0;
		}

		element_location += *(subrange_info.p_upper_bound) + 1;
		if (subrange_info.p_lower_bound != NULL)
			element_location -= *(subrange_info.p_lower_bound);

	}
	/* msg_stderr("N is out of range\n"); */
	return -1;
}

unsigned int metac_type_array_subrange_count(struct metac_type *type){
	assert(type);
	type = metac_type_typedef_skip(type);
	assert(type);
	if (metac_type_id(type) != DW_TAG_array_type) {
		msg_stderr("expected type DW_TAG_array_type\n");
		return 0;
	}
	return metac_type_child_num(type);
}

struct metac_type * metac_type_array_subrange(struct metac_type *type, unsigned int id) {
	struct metac_type * subrange_type;
	assert(type);
	type = metac_type_typedef_skip(type);
	assert(type);
	if (metac_type_id(type) != DW_TAG_array_type) {
		msg_stderr("expected type DW_TAG_array_type\n");
		return NULL;
	}

	subrange_type = metac_type_child(type, id);
	assert(metac_type_id(subrange_type) == DW_TAG_subrange_type);
	return subrange_type;
}

unsigned int metac_type_array_length(struct metac_type *type) {
	unsigned int i;
	unsigned int length = 0;
	assert(type);
	type = metac_type_typedef_skip(type);
	assert(type);
	if (metac_type_id(type) != DW_TAG_array_type) {
		msg_stderr("expected type DW_TAG_array_type\n");
		return 0;
	}

	for (i = 0; i < metac_type_array_subrange_count(type); i++) {
		struct metac_type_subrange_info subrange_info;
		if (metac_type_subrange_info(metac_type_array_subrange(type, i), &subrange_info ) != 0){
			msg_stderr("metac_type_subrange_info returned error\n");
			return 0;
		}
		if (subrange_info.p_upper_bound == NULL) {
			msg_stderr("subrange upper_bound isn't set\n");
			return 0;
		}
		length += *(subrange_info.p_upper_bound) + 1;
		if (subrange_info.p_lower_bound != NULL)
			length -= *(subrange_info.p_lower_bound);

	}
	return length;
}

struct metac_type_subprogram_info_func_data {
	struct metac_type_at * at_type;
	struct metac_type_at * at_name;
};
static int metac_type_subprogram_info_func(struct metac_type *type, struct metac_type_at *at, void * data) {
	struct metac_type_subprogram_info_func_data * p = (struct metac_type_subprogram_info_func_data*)data;
	switch(at->id){
	case DW_AT_name:
		p->at_name = at;
		break;
	case DW_AT_type:
		p->at_type = at;
		break;
	}
	return ((p->at_name != NULL) &&
			(p->at_type != NULL))?1:0;
}

int metac_type_subprogram_info(struct metac_type *type,
		struct metac_type_subprogram_info *p_info) {
	struct metac_type_subprogram_info_func_data data = {
			.at_name = NULL,
			.at_type = NULL,
	};
	assert(type);
	type = metac_type_typedef_skip(type);
	assert(type);
	if (type->id != DW_TAG_subprogram) {
		msg_stderr("expected type DW_TAG_subprogram\n");
		return -1;
	}

	metac_type_at_map(type, metac_type_subprogram_info_func, &data);
	if (	data.at_name == NULL) {
		msg_stderr("mandatory fields are absent\n");
		return -1;
	}
	if (p_info != NULL) {
		p_info->name = data.at_name != NULL? data.at_name->name:NULL;
		p_info->return_type = data.at_type != NULL? data.at_type->type:NULL;
		p_info->parameters_count = metac_type_child_num(type);
	}
	return 0;
}

struct metac_type_parameter_info_func_data {
	struct metac_type_at * at_name;
	struct metac_type_at * at_type;
};
static int metac_type_parameter_info_func(struct metac_type *type, struct metac_type_at *at, void * data) {
	struct metac_type_parameter_info_func_data * p = (struct metac_type_parameter_info_func_data*)data;
	switch(at->id){
	case DW_AT_name:
		p->at_name = at;
		break;
	case DW_AT_type:
		p->at_type = at;
		break;
	}
	return ((p->at_name != NULL) &&
			(p->at_type != NULL))?1:0;
}

int metac_type_parameter_info(struct metac_type *type,
		struct metac_type_parameter_info *p_info) {
	struct metac_type_parameter_info_func_data data = {
			.at_name = NULL,
			.at_type = NULL,
	};
	assert(type);
	type = metac_type_typedef_skip(type);
	assert(type);

	/*handle DW_TAG_unspecified_parameters*/
	if (type->id == DW_TAG_unspecified_parameters){
		if (p_info != NULL) {
			p_info->unspecified_parameters = 1;
		}
		return 0;
	}

	if (type->id != DW_TAG_formal_parameter) {
		msg_stderr("expected type DW_TAG_formal_parameter or DW_TAG_unspecified_parameters\n");
		return -1;
	}

	metac_type_at_map(type, metac_type_parameter_info_func, &data);
	if (	data.at_name == NULL ||
			data.at_type == NULL) {
		msg_stderr("mandatory fields are absent\n");
		return -1;
	}
	if (p_info != NULL) {
		p_info->unspecified_parameters = 0;
		p_info->name = data.at_name->name;
		p_info->type = data.at_type->type;
	}
	return 0;
}


int metac_type_subprogram_parameter_info(struct metac_type *type, unsigned int i,
		struct metac_type_parameter_info *p_info) {
	struct metac_type* 	metac_type_parameter;

	assert(type);
	type = metac_type_typedef_skip(type);
	assert(type);
	if (type->id != DW_TAG_subprogram) {
		msg_stderr("expected type DW_TAG_subprogram\n");
		return -1;
	}
	metac_type_parameter = metac_type_child(type, i);
	if (metac_type_parameter == NULL) {
		msg_stderr("i is incorrect\n");
		return -1;
	}
	return metac_type_parameter_info(metac_type_parameter, p_info);
}

struct metac_type_enumeration_type_info_func_data {
	struct metac_type_at * at_name;
	struct metac_type_at * at_byte_size;
};
static int metac_type_enumeration_type_info_func(struct metac_type *type, struct metac_type_at *at, void * data) {
	struct metac_type_enumeration_type_info_func_data * p = (struct metac_type_enumeration_type_info_func_data*)data;
	switch(at->id){
	case DW_AT_name:
		p->at_name = at;
		break;
	case DW_AT_byte_size:
		p->at_byte_size = at;
		break;
	}
	return ((p->at_name != NULL) &&
			(p->at_byte_size != NULL))?1:0;
}

int metac_type_enumeration_type_info(struct metac_type *type,
		struct metac_type_enumeration_type_info *p_info) {
	struct metac_type_enumeration_type_info_func_data data = {
			.at_name = NULL,
			.at_byte_size = NULL,
	};
	assert(type);
	type = metac_type_typedef_skip(type);
	assert(type);
	if (type->id != DW_TAG_enumeration_type) {
		msg_stderr("expected type DW_TAG_enumeration_type\n");
		return -1;
	}

	metac_type_at_map(type, metac_type_enumeration_type_info_func, &data);
	if (	/*data.at_name == NULL || not true for anonymous enums */
			data.at_byte_size == NULL) {
		msg_stderr("mandatory fields are absent\n");
		return -1;
	}
	if (p_info != NULL) {
		p_info->name = data.at_name != NULL?data.at_name->name:NULL;
		p_info->byte_size = data.at_byte_size->byte_size;
		p_info->enumerators_count = metac_type_child_num(type);
	}
	return 0;
}

struct metac_type_enumerator_info_func_data {
	struct metac_type_at * at_name;
	struct metac_type_at * at_const_value;
};
static int metac_type_enumerator_info_func(struct metac_type *type, struct metac_type_at *at, void * data) {
	struct metac_type_enumerator_info_func_data * p = (struct metac_type_enumerator_info_func_data*)data;
	switch(at->id){
	case DW_AT_name:
		p->at_name = at;
		break;
	case DW_AT_const_value:
		p->at_const_value = at;
		break;
	}
	return ((p->at_name != NULL) &&
			(p->at_const_value != NULL))?1:0;
}

int metac_type_enumerator_info(struct metac_type *type,
		struct metac_type_enumerator_info *p_info) {
	struct metac_type_enumerator_info_func_data data = {
			.at_name = NULL,
			.at_const_value = NULL,
	};
	assert(type);
	type = metac_type_typedef_skip(type);
	assert(type);
	if (type->id != DW_TAG_enumerator) {
		msg_stderr("expected type DW_TAG_enumerator\n");
		return -1;
	}

	metac_type_at_map(type, metac_type_enumerator_info_func, &data);
	if (	data.at_name == NULL ||
			data.at_const_value == NULL) {
		msg_stderr("mandatory fields are absent\n");
		return -1;
	}
	if (p_info != NULL) {
		p_info->name = data.at_name->name;
		p_info->const_value = data.at_const_value->const_value;
	}
	return 0;
}


int metac_type_enumeration_type_enumerator_info(struct metac_type *type, unsigned int N,
		struct metac_type_enumerator_info *p_info) {
	struct metac_type* 	metac_type_enumerator;

	assert(type);
	type = metac_type_typedef_skip(type);
	assert(type);
	if (type->id != DW_TAG_enumeration_type) {
		msg_stderr("expected type DW_TAG_enumeration_type\n");
		return -1;
	}
	metac_type_enumerator = metac_type_child(type, N);
	if (metac_type_enumerator == NULL) {
		msg_stderr("N is incorrect\n");
		return -1;
	}
	return metac_type_enumerator_info(metac_type_enumerator, p_info);
}

