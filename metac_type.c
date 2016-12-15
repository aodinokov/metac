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

struct metac_type_at* metac_type_at_by_key(struct metac_type *type, metac_type_at_id_t id) {
	if (type == NULL)
		return NULL;

	switch (id) {
#define HANDLE_KEY(_key_) case DW_AT_ ##_key_ : return type->p_at.p_at_ ## _key_
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

struct metac_type *metac_type_typedef_skip(struct metac_type *type) {
	if (type == NULL)
		return NULL;

	if (type->id == DW_TAG_typedef) {
		if (type->p_at.p_at_type == NULL || type->p_at.p_at_type->type == NULL) {
			msg_stderr("typedef has to contain type in attributes\n");
			return NULL;
		}
		return metac_type_typedef_skip(type->p_at.p_at_type->type);
	}
	return type;
}

metac_name_t metac_type_name(struct metac_type *type) {
	if (type == NULL || type->p_at.p_at_name == NULL)
		return NULL;
	return type->p_at.p_at_name->name;
}

int metac_type_child_id_by_name(struct metac_type *type, metac_name_t name) {
	unsigned int i;

	if (type == NULL || name == NULL)
		return -1;

	type = metac_type_typedef_skip(type);
	assert(type);

	for (i = 0; i < type->child_num; i++) {
		metac_name_t child_name = metac_type_name(type->child[i]);
		if (	child_name != NULL &&
				strcmp(child_name, name) == 0)
			return (int)i;
	}
	return -1;
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
		assert(type->p_at.p_at_byte_size);
		return type->p_at.p_at_byte_size->byte_size;
	case DW_TAG_pointer_type:
		if (type->p_at.p_at_byte_size != NULL)
			return type->p_at.p_at_byte_size->byte_size;
		return sizeof(void*);
	case DW_TAG_array_type:
		if (type->p_at.p_at_type->type == NULL) {
			msg_stderr("metac_type_array_element_type returned NULL\n");
			return 0;
		}
		return metac_type_array_length(type) * metac_type_byte_size(type->p_at.p_at_type->type);
	case DW_TAG_subprogram:
		return 1;	/*sizeof function == 1*/
	}
	return 0;
}

/*-----------V refactored V----------------*/
int metac_type_subprogram_info(struct metac_type *type,
		struct metac_type_subprogram_info *p_info) {
	if (type == NULL)
		return -1;

	type = metac_type_typedef_skip(type);
	assert(type);

	if (type->id != DW_TAG_subprogram) {
		msg_stderr("expected type DW_TAG_subprogram\n");
		return -1;
	}

	if (	type->p_at.p_at_name == NULL) {
		msg_stderr("mandatory fields are absent\n");
		return -1;
	}
	if (p_info != NULL) {
		p_info->name = type->p_at.p_at_name != NULL?
				type->p_at.p_at_name->name:NULL;
		p_info->type = type->p_at.p_at_type != NULL?
				type->p_at.p_at_type->type:NULL;
		p_info->parameters_count = metac_type_child_num(type);
	}
	return 0;
}

int metac_type_parameter_info(struct metac_type *type,
		struct metac_type_parameter_info *p_info) {
	if (type == NULL)
		return -1;
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

	if (	type->p_at.p_at_name == NULL ||
			type->p_at.p_at_type == NULL) {
		msg_stderr("mandatory fields are absent\n");
		return -1;
	}
	if (p_info != NULL) {
		p_info->unspecified_parameters = 0;
		p_info->name = type->p_at.p_at_name->name;
		p_info->type = type->p_at.p_at_type->type;
	}
	return 0;
}

int metac_type_subprogram_parameter_info(struct metac_type *type, unsigned int i,
		struct metac_type_parameter_info *p_info) {
	struct metac_type* 	metac_type_parameter;

	if (type == NULL)
		return -1;

	type = metac_type_typedef_skip(type);
	assert(type);

	if (type->id != DW_TAG_subprogram) {
		msg_stderr("expected type DW_TAG_subprogram\n");
		return -1;
	}
	metac_type_parameter = metac_type_child(type, i);
	assert(metac_type_id(metac_type_parameter) == DW_TAG_formal_parameter ||
			metac_type_id(metac_type_parameter) == DW_TAG_unspecified_parameters);
	if (metac_type_parameter == NULL) {
		msg_stderr("i is incorrect\n");
		return -1;
	}
	return metac_type_parameter_info(metac_type_parameter, p_info);
}

int metac_type_enumeration_type_info(struct metac_type *type,
		struct metac_type_enumeration_type_info *p_info) {
	if (type == NULL)
		return -1;

	type = metac_type_typedef_skip(type);
	assert(type);
	if (type->id != DW_TAG_enumeration_type) {
		msg_stderr("expected type DW_TAG_enumeration_type\n");
		return -1;
	}

	if (type->p_at.p_at_byte_size == NULL) {
		msg_stderr("mandatory fields are absent\n");
		return -1;
	}
	if (p_info != NULL) {
		p_info->name = type->p_at.p_at_name != NULL?type->p_at.p_at_name->name:NULL;
		p_info->byte_size = type->p_at.p_at_byte_size->byte_size;
		p_info->enumerators_count = metac_type_child_num(type);
	}
	return 0;
}

int metac_type_enumerator_info(struct metac_type *type,
		struct metac_type_enumerator_info *p_info) {
	if (type == NULL)
		return -1;

	type = metac_type_typedef_skip(type);
	assert(type);
	if (type->id != DW_TAG_enumerator) {
		msg_stderr("expected type DW_TAG_enumerator\n");
		return -1;
	}

	if (	type->p_at.p_at_name == NULL ||
			type->p_at.p_at_const_value == NULL) {
		msg_stderr("mandatory fields are absent\n");
		return -1;
	}
	if (p_info != NULL) {
		p_info->name = type->p_at.p_at_name->name;
		p_info->const_value = type->p_at.p_at_const_value->const_value;
	}
	return 0;
}

int metac_type_enumeration_type_enumerator_info(struct metac_type *type, unsigned int i,
		struct metac_type_enumerator_info *p_info) {
	struct metac_type* 	metac_type_enumerator;

	if (type == NULL)
		return -1;

	type = metac_type_typedef_skip(type);
	assert(type);
	if (type->id != DW_TAG_enumeration_type) {
		msg_stderr("expected type DW_TAG_enumeration_type\n");
		return -1;
	}
	metac_type_enumerator = metac_type_child(type, i);
	assert(metac_type_id(metac_type_enumerator) == DW_TAG_enumerator);
	if (metac_type_enumerator == NULL) {
		msg_stderr("i is incorrect\n");
		return -1;
	}
	return metac_type_enumerator_info(metac_type_enumerator, p_info);
}

int metac_type_member_info(struct metac_type *type, struct metac_type_member_info * p_member_info) {
	if (type == NULL)
		return -1;

	if (type->id != DW_TAG_member) {
		msg_stderr("expected type DW_TAG_member\n");
		return -1;
	}

	if (	type->p_at.p_at_name == NULL ||
			type->p_at.p_at_type == NULL /*||
			type->p_at.p_at_data_member_location == NULL*/) {
		msg_stderr("mandatory fields are absent\n");
		return -1;
	}

	if (p_member_info != NULL) {
		p_member_info->name = type->p_at.p_at_name->name;
		p_member_info->type = type->p_at.p_at_type->type;
		p_member_info->p_data_member_location = type->p_at.p_at_data_member_location!=NULL?
				&type->p_at.p_at_data_member_location->data_member_location:NULL;
		p_member_info->p_bit_offset = type->p_at.p_at_bit_offset!=NULL?
				&type->p_at.p_at_bit_offset->bit_offset:NULL;
		p_member_info->p_bit_size = type->p_at.p_at_bit_size!=NULL?
				&type->p_at.p_at_bit_size->bit_size:NULL;
	}
	return 0;
}

int metac_type_structure_info(struct metac_type *type, struct metac_type_structure_info *p_info) {
	if (type == NULL)
		return -1;

	type = metac_type_typedef_skip(type);
	assert(type);
	if (type->id != DW_TAG_structure_type) {
		msg_stderr("expected type DW_TAG_structure_type\n");
		return -1;
	}

	if (type->p_at.p_at_byte_size == NULL) {
		msg_stderr("mandatory fields are absent\n");
		return -1;
	}
	if (p_info != NULL) {
		p_info->name = type->p_at.p_at_name != NULL?type->p_at.p_at_name->name:NULL;
		p_info->byte_size = type->p_at.p_at_byte_size->byte_size;
		p_info->members_count = metac_type_child_num(type);
	}
	return 0;
}

int metac_type_structure_member_info(struct metac_type *type, unsigned int i,
		struct metac_type_member_info *p_info) {
	struct metac_type* 	metac_type_member;

	if (type == NULL)
		return -1;

	type = metac_type_typedef_skip(type);
	assert(type);
	if (type->id != DW_TAG_structure_type) {
		msg_stderr("expected type DW_TAG_structure_type\n");
		return -1;
	}
	metac_type_member = metac_type_child(type, i);
	assert(metac_type_id(metac_type_member) == DW_TAG_member);
	if (metac_type_member == NULL) {
		msg_stderr("i is incorrect\n");
		return -1;
	}
	return metac_type_member_info(metac_type_member, p_info);
}

int metac_type_union_info(struct metac_type *type, struct metac_type_union_info *p_info) {
	if (type == NULL)
		return -1;

	type = metac_type_typedef_skip(type);
	assert(type);
	if (type->id != DW_TAG_union_type) {
		msg_stderr("expected type DW_TAG_union_type\n");
		return -1;
	}

	if (type->p_at.p_at_byte_size == NULL) {
		msg_stderr("mandatory fields are absent\n");
		return -1;
	}
	if (p_info != NULL) {
		p_info->name = type->p_at.p_at_name != NULL?type->p_at.p_at_name->name:NULL;
		p_info->byte_size = type->p_at.p_at_byte_size->byte_size;
		p_info->members_count = metac_type_child_num(type);
	}
	return 0;
}

int metac_type_union_member_info(struct metac_type *type, unsigned int i,
		struct metac_type_member_info *p_info) {
	struct metac_type* 	metac_type_member;

	if (type == NULL)
		return -1;

	type = metac_type_typedef_skip(type);
	assert(type);
	if (type->id != DW_TAG_union_type) {
		msg_stderr("expected type DW_TAG_union_type\n");
		return -1;
	}
	metac_type_member = metac_type_child(type, i);
	assert(metac_type_id(metac_type_member) == DW_TAG_member);
	if (metac_type_member == NULL) {
		msg_stderr("i is incorrect\n");
		return -1;
	}
	return metac_type_member_info(metac_type_member, p_info);
}

int metac_type_subrange_info(struct metac_type *type, struct metac_type_subrange_info * p_subrange_info) {
	if (type == NULL)
		return -1;

	if (type->id != DW_TAG_subrange_type) {
		msg_stderr("expected type DW_TAG_subrange_type\n");
		return -1;
	}

	if (	type->p_at.p_at_type == NULL) {
		msg_stderr("mandatory fields are absent\n");
		return -1;
	}

	if (p_subrange_info != NULL) {
		p_subrange_info->type = type->p_at.p_at_type->type;
		p_subrange_info->p_lower_bound = type->p_at.p_at_lower_bound!=NULL?
				&type->p_at.p_at_lower_bound->lower_bound:NULL;
		p_subrange_info->p_upper_bound = type->p_at.p_at_upper_bound!=NULL?
				&type->p_at.p_at_upper_bound->upper_bound:NULL;
	}
	return 0;
}

int metac_type_array_info(struct metac_type *type, struct metac_type_array_info *p_info) {
	if (type == NULL)
		return -1;

	type = metac_type_typedef_skip(type);
	assert(type);
	if (type->id != DW_TAG_array_type) {
		msg_stderr("expected type DW_TAG_array_type\n");
		return -1;
	}

	if (type->p_at.p_at_type == NULL) {
		msg_stderr("mandatory fields are absent\n");
		return -1;
	}
	if (p_info != NULL) {
		p_info->type = type->p_at.p_at_type->type;
		p_info->subranges_count = metac_type_child_num(type);
	}
	return 0;
}

int metac_type_array_subrange_info(struct metac_type *type, unsigned int i,
		struct metac_type_subrange_info *p_info) {
	struct metac_type* 	metac_type_subrange;

	if (type == NULL)
		return -1;

	type = metac_type_typedef_skip(type);
	assert(type);
	if (type->id != DW_TAG_array_type) {
		msg_stderr("expected type DW_TAG_array_type\n");
		return -1;
	}
	metac_type_subrange = metac_type_child(type, i);
	assert(metac_type_id(metac_type_subrange) == DW_TAG_subrange_type);
	if (metac_type_subrange == NULL) {
		msg_stderr("i is incorrect\n");
		return -1;
	}
	return metac_type_subrange_info(metac_type_subrange, p_info);
}

int metac_type_array_element_info(struct metac_type *type, unsigned int i, struct metac_type_element_info *p_element_info) {
	metac_num_t id;
	unsigned int data_location = 0;

	if (type == NULL)
		return -1;

	type = metac_type_typedef_skip(type);
	assert(type);
	if (metac_type_id(type) != DW_TAG_array_type) {
		msg_stderr("expected type DW_TAG_array_type\n");
		return -1;
	}

	for (id = 0; id < metac_type_child_num(type); id++) {
		struct metac_type_subrange_info subrange_info;
		if (metac_type_array_subrange_info(type, id, &subrange_info) != 0) {
			msg_stderr("metac_type_subrange_info returned error\n");
			return -1;
		}
		if (	(id == 0 && subrange_info.p_upper_bound == NULL) || /* allow arrays without bounds to calc Nth element */
				(((subrange_info.p_lower_bound == NULL) ||
				(subrange_info.p_lower_bound != NULL  && *(subrange_info.p_lower_bound) <= i)) &&
				*(subrange_info.p_upper_bound) >= i)) { /* found range */
			data_location +=i;
			if (p_element_info) {
				p_element_info->type = type->p_at.p_at_type->type;
				assert(p_element_info->type != NULL);
				p_element_info->data_location = data_location * metac_type_byte_size(p_element_info->type);
			}
			return 0;
		}

		data_location += *(subrange_info.p_upper_bound) + 1;
		if (subrange_info.p_lower_bound != NULL)
			data_location -= *(subrange_info.p_lower_bound);

	}
	return -1;
}

metac_bound_t metac_type_array_length(struct metac_type *type) {
	metac_num_t id;
	metac_bound_t count = 0;
	assert(type);
	type = metac_type_typedef_skip(type);
	assert(type);
	if (metac_type_id(type) != DW_TAG_array_type) {
		msg_stderr("expected type DW_TAG_array_type\n");
		return 0;
	}

	/*TODO: completely not sure that it will work! may be we just can take upper bound of the last subrange???? - in that case
	 * it's easy just to put it into array_info! */
	for (id = 0; id < metac_type_child_num(type); id++) {
		struct metac_type_subrange_info subrange_info;
		if (metac_type_array_subrange_info(type, id, &subrange_info) != 0){
			msg_stderr("metac_type_subrange_info returned error\n");
			return 0;
		}
		if (subrange_info.p_upper_bound == NULL) {
			msg_stderr("subrange upper_bound isn't set\n");
			return 0;
		}
		count += *(subrange_info.p_upper_bound) + 1;
		if (subrange_info.p_lower_bound != NULL)
			count -= *(subrange_info.p_lower_bound);

	}
	return count;
}

