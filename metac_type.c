/*
 * metac_type.c
 *
 *  Created on: Aug 31, 2015
 *      Author: mralex
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>		/*uint64_t etc*/
#include <assert.h>
#include <errno.h>

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
	return type->dwarf_info.child_num;
}

struct metac_type* metac_type_child(struct metac_type *type, unsigned int i) {
	if (type == NULL)
		return NULL;
	if (i >= type->dwarf_info.child_num)
		return NULL;

	return type->dwarf_info.child[i];
}

unsigned int metac_type_at_num(struct metac_type *type) {
	if (type == NULL)
		return 0;
	return type->dwarf_info.at_num;
}

struct metac_type_at* 	metac_type_at(struct metac_type *type, unsigned int i)  {
	if (type == NULL)
		return NULL;
	if (i >= type->dwarf_info.at_num)
		return NULL;

	return &type->dwarf_info.at[i];
}

const char*	metac_type_specification(struct metac_type *type, const char *key) {
	int i = 0;
	if (type == NULL)
		return NULL;
	while (type->specifications[i].key) {
		if (strcmp(type->specifications[i].key, key) == 0)
			return type->specifications[i].value;
		++i;
	}
	return NULL;
}

struct metac_type *metac_type_typedef_skip(struct metac_type *type) {
	if (type == NULL){
		msg_stderr("invalid argument value: return NULL\n");
		return NULL;
	}
	if (type->id == DW_TAG_typedef) {
		if (type->typedef_info.type == NULL) {
			msg_stderr("typedef has to contain type in attributes: return NULL\n");
			return NULL;
		}
		return metac_type_typedef_skip(type->typedef_info.type);
	}
	return type;
}

metac_byte_size_t metac_type_byte_size(struct metac_type *type) {
	if (type == NULL)
		return 0;

	type = metac_type_typedef_skip(type);
	assert(type);

	switch(type->id) {
	case DW_TAG_base_type:
		return type->base_type_info.byte_size;
	case DW_TAG_enumeration_type:
		return type->enumeration_type_info.byte_size;
	case DW_TAG_structure_type:
		return type->structure_type_info.byte_size;
	case DW_TAG_union_type:
		return type->union_type_info.byte_size;
	case DW_TAG_pointer_type:
		if (type->pointer_type_info.byte_size > 0)
			return type->pointer_type_info.byte_size;
		return sizeof(void*);
	case DW_TAG_array_type:
		return type->array_type_info.elements_count * metac_type_byte_size(type->array_type_info.type);
	case DW_TAG_subprogram:
		return 1;	/*sizeof function == 1*/
	}
	return 0;
}
metac_name_t metac_type_name(struct metac_type *type) {
	if (type == NULL)
		return NULL;

	switch(type->id) {
	case DW_TAG_base_type:
		return type->base_type_info.name;
	case DW_TAG_typedef:
		return type->typedef_info.name;
	case DW_TAG_enumeration_type:
		return type->enumeration_type_info.name;
	case DW_TAG_structure_type:
		return type->structure_type_info.name;
	case DW_TAG_union_type:
		return type->union_type_info.name;
	case DW_TAG_pointer_type:
		return type->pointer_type_info.name;
	case DW_TAG_array_type:
		return NULL;
	case DW_TAG_subprogram:
		return type->subprogram_info.name;
	}
	return NULL;
}

int	metac_type_enumeration_type_get_value(struct metac_type *type, metac_name_t name, metac_const_value_t *p_const_value) {
	metac_num_t i;
	if (type == NULL) {
		msg_stderr("invalid argument value: return NULL\n");
		return -1;
	}
	if (type->id != DW_TAG_enumeration_type) {
		msg_stderr("invalid argument type: return NULL\n");
		return -1;
	}

	for (i = 0; i < type->enumeration_type_info.enumerators_count; ++i) {
		if (strcmp(name, type->enumeration_type_info.enumerators[i].name) == 0) {
			if (p_const_value != NULL)
				*p_const_value = type->enumeration_type_info.enumerators[i].const_value;
			return 0;
		}
	}

	msg_stddbg("wan't able to find const_value for %s\n", name);
	return -1;
}
metac_name_t metac_type_enumeration_type_get_name(struct metac_type *type, metac_const_value_t const_value) {
	metac_num_t i;
	if (type == NULL) {
		msg_stderr("invalid argument value: return NULL\n");
		return NULL;
	}
	if (type->id != DW_TAG_enumeration_type) {
		msg_stderr("invalid argument type: return NULL\n");
		return NULL;
	}

	for (i = 0; i < type->enumeration_type_info.enumerators_count; ++i) {
		if (type->enumeration_type_info.enumerators[i].const_value == const_value) {
			return type->enumeration_type_info.enumerators[i].name;
		}
	}

	msg_stddbg("Wan't able to find const_value for 0x%Lx\n", (unsigned long long)const_value);
	return NULL;
}

//
//int metac_type_subprogram_info(struct metac_type *type,
//		struct metac_type_subprogram_info *p_info) {
//	if (type == NULL)
//		return -1;
//
//	type = metac_type_typedef_skip(type);
//	assert(type);
//
//	if (type->id != DW_TAG_subprogram) {
//		msg_stderr("expected type DW_TAG_subprogram\n");
//		return -1;
//	}
//
//	if (type->p_at.p_at_name == NULL) {
//		msg_stderr("mandatory fields are absent\n");
//		return -1;
//	}
//	if (p_info != NULL) {
//		p_info->name = type->p_at.p_at_name != NULL?
//				type->p_at.p_at_name->name:NULL;
//		p_info->type = type->p_at.p_at_type != NULL?
//				type->p_at.p_at_type->type:NULL;
//		p_info->parameters_count = metac_type_child_num(type);
//	}
//	return 0;
//}
//
//int metac_type_parameter_info(struct metac_type *type,
//		struct metac_type_parameter_info *p_info) {
//	if (type == NULL)
//		return -1;
//	type = metac_type_typedef_skip(type);
//	assert(type);
//
//	/*handle DW_TAG_unspecified_parameters*/
//	if (type->id == DW_TAG_unspecified_parameters){
//		if (p_info != NULL) {
//			p_info->unspecified_parameters = 1;
//		}
//		return 0;
//	}
//
//	if (type->id != DW_TAG_formal_parameter) {
//		msg_stderr("expected type DW_TAG_formal_parameter or DW_TAG_unspecified_parameters\n");
//		return -1;
//	}
//
//	if (type->p_at.p_at_name == NULL ||
//		type->p_at.p_at_type == NULL) {
//		msg_stderr("mandatory fields are absent\n");
//		return -1;
//	}
//	if (p_info != NULL) {
//		p_info->unspecified_parameters = 0;
//		p_info->name = type->p_at.p_at_name->name;
//		p_info->type = type->p_at.p_at_type->type;
//	}
//	return 0;
//}
//
//int metac_type_subprogram_parameter_info(struct metac_type *type, unsigned int i,
//		struct metac_type_parameter_info *p_info) {
//	struct metac_type* 	metac_type_parameter;
//
//	if (type == NULL)
//		return -1;
//
//	type = metac_type_typedef_skip(type);
//	assert(type);
//
//	if (type->id != DW_TAG_subprogram) {
//		msg_stderr("expected type DW_TAG_subprogram\n");
//		return -1;
//	}
//	metac_type_parameter = metac_type_child(type, i);
//	assert(metac_type_id(metac_type_parameter) == DW_TAG_formal_parameter ||
//			metac_type_id(metac_type_parameter) == DW_TAG_unspecified_parameters);
//	if (metac_type_parameter == NULL) {
//		msg_stderr("i is incorrect\n");
//		return -1;
//	}
//	return metac_type_parameter_info(metac_type_parameter, p_info);
//}
//
//int metac_type_enumeration_type_info(struct metac_type *type,
//		struct metac_type_enumeration_type_info *p_info) {
//	if (type == NULL)
//		return -1;
//
//	type = metac_type_typedef_skip(type);
//	assert(type);
//	if (type->id != DW_TAG_enumeration_type) {
//		msg_stderr("expected type DW_TAG_enumeration_type\n");
//		return -1;
//	}
//
//	if (type->p_at.p_at_byte_size == NULL) {
//		msg_stderr("mandatory fields are absent\n");
//		return -1;
//	}
//	if (p_info != NULL) {
//		p_info->name = type->p_at.p_at_name != NULL?type->p_at.p_at_name->name:NULL;
//		p_info->byte_size = type->p_at.p_at_byte_size->byte_size;
//		p_info->enumerators_count = metac_type_child_num(type);
//	}
//	return 0;
//}
//
//int metac_type_enumerator_info(struct metac_type *type,
//		struct metac_type_enumerator_info *p_info) {
//	if (type == NULL)
//		return -1;
//
//	type = metac_type_typedef_skip(type);
//	assert(type);
//	if (type->id != DW_TAG_enumerator) {
//		msg_stderr("expected type DW_TAG_enumerator\n");
//		return -1;
//	}
//
//	if (type->p_at.p_at_name == NULL ||
//		type->p_at.p_at_const_value == NULL) {
//		msg_stderr("mandatory fields are absent\n");
//		return -1;
//	}
//	if (p_info != NULL) {
//		p_info->name = type->p_at.p_at_name->name;
//		p_info->const_value = type->p_at.p_at_const_value->const_value;
//	}
//	return 0;
//}
//
//int metac_type_enumeration_type_enumerator_info(struct metac_type *type, unsigned int i,
//		struct metac_type_enumerator_info *p_info) {
//	struct metac_type* 	metac_type_enumerator;
//
//	if (type == NULL)
//		return -1;
//
//	type = metac_type_typedef_skip(type);
//	assert(type);
//	if (type->id != DW_TAG_enumeration_type) {
//		msg_stderr("expected type DW_TAG_enumeration_type\n");
//		return -1;
//	}
//	metac_type_enumerator = metac_type_child(type, i);
//	assert(metac_type_id(metac_type_enumerator) == DW_TAG_enumerator);
//	if (metac_type_enumerator == NULL) {
//		msg_stderr("i is incorrect\n");
//		return -1;
//	}
//	return metac_type_enumerator_info(metac_type_enumerator, p_info);
//}
//
//int metac_type_member_info(struct metac_type *type, struct metac_type_member_info * p_member_info) {
//	if (type == NULL)
//		return -1;
//
//	if (type->id != DW_TAG_member) {
//		msg_stderr("expected type DW_TAG_member\n");
//		return -1;
//	}
//
//	if (/*type->p_at.p_at_name == NULL ||*/
//		type->p_at.p_at_type == NULL /*||
//		type->p_at.p_at_data_member_location == NULL*/) {
//		msg_stderr("mandatory fields are absent\n");
//		return -1;
//	}
//
//	if (p_member_info != NULL) {
//		p_member_info->name = type->p_at.p_at_name!=NULL?type->p_at.p_at_name->name:NULL;
//		p_member_info->type = type->p_at.p_at_type->type;
//		p_member_info->p_data_member_location = type->p_at.p_at_data_member_location!=NULL?
//				&type->p_at.p_at_data_member_location->data_member_location:NULL;
//		p_member_info->p_bit_offset = type->p_at.p_at_bit_offset!=NULL?
//				&type->p_at.p_at_bit_offset->bit_offset:NULL;
//		p_member_info->p_bit_size = type->p_at.p_at_bit_size!=NULL?
//				&type->p_at.p_at_bit_size->bit_size:NULL;
//	}
//	return 0;
//}
//
//int metac_type_structure_info(struct metac_type *type, struct metac_type_structure_info *p_info) {
//	if (type == NULL)
//		return -1;
//
//	type = metac_type_typedef_skip(type);
//	assert(type);
//	if (type->id != DW_TAG_structure_type) {
//		msg_stderr("expected type DW_TAG_structure_type\n");
//		return -1;
//	}
//
//	if (type->p_at.p_at_byte_size == NULL) {
//		msg_stderr("mandatory fields are absent\n");
//		return -1;
//	}
//	if (p_info != NULL) {
//		p_info->name = type->p_at.p_at_name != NULL?type->p_at.p_at_name->name:NULL;
//		p_info->byte_size = type->p_at.p_at_byte_size->byte_size;
//		/*fixme: need to members_count can be less that child_num if other types defined in the union or struct*/
//		p_info->members_count = metac_type_child_num(type);
//	}
//	return 0;
//}
//
//int metac_type_structure_member_info(struct metac_type *type, unsigned int i,
//		struct metac_type_member_info *p_info) {
//	struct metac_type* 	metac_type_member;
//
//	if (type == NULL)
//		return -1;
//
//	type = metac_type_typedef_skip(type);
//	assert(type);
//	if (type->id != DW_TAG_structure_type) {
//		msg_stderr("expected type DW_TAG_structure_type\n");
//		return -1;
//	}
//	metac_type_member = metac_type_child(type, i);
//	//FIXME: assert(metac_type_id(metac_type_member) == DW_TAG_member);
//	if (metac_type_id(metac_type_member) != DW_TAG_member){
//		msg_stderr("Warning: member %d isn't TAG_member. something was declared in struct\n", (int)i);
//		return -1;
//	}
//
//	if (metac_type_member == NULL) {
//		msg_stderr("i is incorrect\n");
//		return -1;
//	}
//	return metac_type_member_info(metac_type_member, p_info);
//}
//
//int metac_type_union_info(struct metac_type *type, struct metac_type_union_info *p_info) {
//	if (type == NULL)
//		return -1;
//
//	type = metac_type_typedef_skip(type);
//	assert(type);
//	if (type->id != DW_TAG_union_type) {
//		msg_stderr("expected type DW_TAG_union_type\n");
//		return -1;
//	}
//
//	if (type->p_at.p_at_byte_size == NULL) {
//		msg_stderr("mandatory fields are absent\n");
//		return -1;
//	}
//	if (p_info != NULL) {
//		p_info->name = type->p_at.p_at_name != NULL?type->p_at.p_at_name->name:NULL;
//		p_info->byte_size = type->p_at.p_at_byte_size->byte_size;
//		/*fixme: need to members_count can be less that child_num if other types defined in the union or struct*/
//		p_info->members_count = metac_type_child_num(type);
//	}
//	return 0;
//}
//
//int metac_type_union_member_info(struct metac_type *type, unsigned int i,
//		struct metac_type_member_info *p_info) {
//	struct metac_type* 	metac_type_member;
//
//	if (type == NULL)
//		return -1;
//
//	type = metac_type_typedef_skip(type);
//	assert(type);
//	if (type->id != DW_TAG_union_type) {
//		msg_stderr("expected type DW_TAG_union_type\n");
//		return -1;
//	}
//	metac_type_member = metac_type_child(type, i);
//	//FIXME: assert(metac_type_id(metac_type_member) == DW_TAG_member);
//	if (metac_type_member == NULL) {
//		msg_stderr("i is incorrect\n");
//		return -1;
//	}
//	return metac_type_member_info(metac_type_member, p_info);
//}
//
//int metac_type_subrange_info(struct metac_type *type, struct metac_type_subrange_info * p_subrange_info) {
//	if (type == NULL)
//		return -1;
//
//	if (type->id != DW_TAG_subrange_type) {
//		msg_stderr("expected type DW_TAG_subrange_type\n");
//		return -1;
//	}
//
//	if (type->p_at.p_at_type == NULL) {
//		msg_stderr("mandatory fields are absent\n");
//		return -1;
//	}
//
//	if (p_subrange_info != NULL) {
//		p_subrange_info->type = type->p_at.p_at_type->type;
//		p_subrange_info->p_lower_bound = type->p_at.p_at_lower_bound!=NULL?
//				&type->p_at.p_at_lower_bound->lower_bound:NULL;
//		p_subrange_info->p_upper_bound = type->p_at.p_at_upper_bound!=NULL?
//				&type->p_at.p_at_upper_bound->upper_bound:NULL;
//		p_subrange_info->p_count = type->p_at.p_at_count!=NULL?
//				&type->p_at.p_at_count->count:NULL;
//	}
//	return 0;
//}
//
//int metac_type_array_info(struct metac_type *type, struct metac_type_array_info *p_info) {
//	if (type == NULL)
//		return -1;
//
//	type = metac_type_typedef_skip(type);
//	assert(type);
//	if (type->id != DW_TAG_array_type) {
//		msg_stderr("expected type DW_TAG_array_type\n");
//		return -1;
//	}
//
//	if (type->p_at.p_at_type == NULL) {
//		msg_stderr("mandatory fields are absent\n");
//		return -1;
//	}
//	if (p_info != NULL) {
//		p_info->type = type->p_at.p_at_type->type;
//		p_info->subranges_count = metac_type_child_num(type);
//
//		/**/
//		assert(p_info->subranges_count < 2);	/* assumption: C shouldn't have several subranges */
//
//		p_info->lower_bound = 0;
//		p_info->p_upper_bound = NULL;
//		p_info->elements_count = 0;
//
//		if (p_info->subranges_count > 0) {
//			struct metac_type_subrange_info subrange_info;
//			/*check if we need to use upper_bound or count param of subrange*/
//			if (metac_type_array_subrange_info(type, p_info->subranges_count - 1, &subrange_info) == 0) { /*we have subranges*/
//				if (subrange_info.p_count != NULL) {
//					/*in general we need to calc all subranges. with with our assumption about number of subranges we'll take count*/
//					p_info->elements_count = *subrange_info.p_count;
//				} else { /*we can get upper and substitute lower*/
//					if (subrange_info.p_upper_bound != NULL)
//						p_info->p_upper_bound = subrange_info.p_upper_bound;
//
//					if (metac_type_array_subrange_info(type, 0, &subrange_info) == 0){
//						if (subrange_info.p_lower_bound != NULL)
//							p_info->lower_bound = *subrange_info.p_lower_bound;
//					}
//					if (p_info->p_upper_bound != NULL) {
//						p_info->elements_count = *(p_info->p_upper_bound) + 1 - p_info->lower_bound;
//					}
//				}
//			}
//		}
//	}
//	return 0;
//}
//
//int metac_type_array_subrange_info(struct metac_type *type, unsigned int i,
//		struct metac_type_subrange_info *p_info) {
//	struct metac_type* 	metac_type_subrange;
//
//	if (type == NULL)
//		return -1;
//
//	type = metac_type_typedef_skip(type);
//	assert(type);
//	if (type->id != DW_TAG_array_type) {
//		msg_stderr("expected type DW_TAG_array_type\n");
//		return -1;
//	}
//	metac_type_subrange = metac_type_child(type, i);
//	if (metac_type_subrange == NULL) {
//		msg_stderr("i is incorrect\n");
//		return -1;
//	}
//
//	assert(metac_type_id(metac_type_subrange) == DW_TAG_subrange_type);
//	return metac_type_subrange_info(metac_type_subrange, p_info);
//}
//
//int metac_type_array_element_info(struct metac_type *type, unsigned int i, struct metac_type_element_info *p_element_info) {
//	struct metac_type_array_info info;
//	if (metac_type_array_info(type, &info) == 0) {
//		if (info.subranges_count == 0)
//			return -1;
//		if (i >= info.elements_count && info.elements_count > 0)
//			return -1;
//		if (p_element_info) {
//			p_element_info->type = info.type;
//			p_element_info->data_location = i * metac_type_byte_size(p_element_info->type);
//		}
//		return 0;
//	}
//	return -1;
//}
//
struct metac_type * metac_type_by_name(struct metac_type_sorted_array * array, metac_name_t name) {
	if (array == NULL || name == NULL)
		return NULL;
	/*binary search*/
	metac_num_t min = 0, max = array->number-1;
	do {
		metac_num_t i = min + (max - min)/2;
		int cmp_res = strcmp(array->item[i].name, name);
		switch((cmp_res>0)?(1):((cmp_res<0)?(-1):(0))) {
		case 0: return array->item[i].ptr;
		case 1: max = i-1; break;
		case -1: min = i+1; break;
		}
	}while(min<=max);
	return NULL;
}
//
//struct metac_object * metac_object_by_name(struct metac_object_sorted_array * array, metac_name_t name) {
//	if (array == NULL || name == NULL)
//		return NULL;
//	/*binary search*/
//	metac_num_t min = 0, max = array->number-1;
//	do {
//		metac_num_t i = min + (max - min)/2;
//		int cmp_res = strcmp(array->item[i].name, name);
//		switch((cmp_res>0)?(1):((cmp_res<0)?(-1):(0))) {
//		case 0: return array->item[i].ptr;
//		case 1: max = i-1; break;
//		case -1: min = i+1; break;
//		}
//	}while(min<=max);
//	return NULL;
//}
//
//static int _metac_delete(struct metac_type *type, void *ptr){
//	metac_type_id_t id;
//	if (type == NULL || ptr == NULL)
//		return -1;
//
//	type = metac_type_typedef_skip(type);
//	assert(type);
//
//	id = metac_type_id(type);
//	switch (id) {
//	case DW_TAG_base_type:
//	case DW_TAG_enumeration_type:
//		/*nothing to do*/
//		return 0;
//	case DW_TAG_pointer_type: {
//			int res = 0;
//			/*get info about what type of the pointer*/
//			if (type->p_at.p_at_type) {
//				void *_ptr = *((void**)ptr);
//				res = _metac_delete(type->p_at.p_at_type->type, _ptr);
//				if (_ptr)
//					free(_ptr);
//			}
//			return res;
//		}
//	case DW_TAG_array_type: {
//			int res = 0;
//			metac_num_t i;
//			struct metac_type_array_info info;
//
//			if (metac_type_array_info(type, &info) != 0)
//				return -1;
//
//			for (i = 0; i < info.elements_count; i++) {
//				struct metac_type_element_info einfo;
//				if (metac_type_array_element_info(type, i, &einfo) != 0) {
//					++res;
//					continue;
//				}
//				res += _metac_delete(einfo.type, ptr + einfo.data_location);
//			}
//
//			return res;
//		}
//	case DW_TAG_structure_type: {
//			int res = 0;
//			metac_num_t i;
//			struct metac_type_structure_info info;
//
//			if (metac_type_structure_info(type, &info) != 0)
//				return -1;
//
//			for (i = 0; i < info.members_count; i++) {
//				struct metac_type_member_info minfo;
//
//				if (metac_type_structure_member_info(type, i, &minfo) != 0) {
//					++res;
//					continue;
//				}
//
//				assert( (metac_type_id(minfo.type) != DW_TAG_pointer_type) ||
//						(metac_type_id(minfo.type) == DW_TAG_pointer_type && (minfo.p_bit_offset == NULL || minfo.p_bit_size == NULL)));
//				if (minfo.p_bit_offset != NULL || minfo.p_bit_size != NULL) {
//					/*
//					 * we can continue, because bit_fields can't contain pointers.
//					 * Both gcc and clang notifies about error on ptr with bit fields
//					 */
//					continue;
//				}
//
//				res += _metac_delete(minfo.type, ptr + (minfo.p_data_member_location?(*minfo.p_data_member_location):0));
//			}
//			return res;
//		}
//	case DW_TAG_union_type:
//		/*TODO: don't know what to do here */
//		//assert(0);
//		return 0;
//	}
//
//	return 0;
//}
//
//static int _metac_object_delete(struct metac_object * object) {
//	if (object) {
//		int res = _metac_delete(object->type, object->ptr);
//		if (object->ptr)
//			free(object->ptr);
//		free(object);
//		return res;
//	}
//	return -1;
//}
//
//struct metac_object * metac_object_get(struct metac_object * object) {
//	if (object) {
//		if (object->ref_count != 0) {
//			++object->ref_count;
//		}
//	}
//	return object;
//}
//
//int metac_object_put(struct metac_object * object) {
//	if (object) {
//		if (object->ref_count != 0) {
//			--object->ref_count;
//			if (object->ref_count == 0) {
//				_metac_object_delete(object);
//				return 1;
//			}
//		}
//	}
//	return 0;
//}

