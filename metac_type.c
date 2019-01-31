/*
 * metac_type.c
 *
 *  Created on: Aug 31, 2015
 *      Author: mralex
 */

//#define METAC_DEBUG_ENABLE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>		/*uint64_t etc*/
#include <assert.h>
#include <errno.h>

#include "metac_type.h"
#include "metac_debug.h"

const metac_type_specification_value_t *
	metac_type_specification(struct metac_type *type, const char *key) {
	int i = 0;
	if (type == NULL){
		msg_stderr("invalid argument value: return NULL\n");
		return NULL;
	}
	if (type->specifications == NULL) {
		msg_stddbg("type %s doesn't have specifications\n", type->name != NULL?type->name:"(nil)");
		return NULL;
	}
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
	if (type == NULL) {
		msg_stderr("invalid argument value: type\n");
		return 0;
	}

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
		return sizeof(metac_type_byte_size);	/*it's always a constant*/
	}
	return 0;
}

metac_name_t metac_type_name(struct metac_type *type) {
	if (type == NULL) {
		msg_stderr("invalid argument value: type\n");
		return NULL;
	}
	return type->name;
}

int	metac_type_enumeration_type_get_value(struct metac_type *type, metac_name_t name, metac_const_value_t *p_const_value) {
	metac_num_t i;
	if (type == NULL) {
		msg_stderr("invalid argument value: return NULL\n");
		return -(EINVAL);
	}
	if (type->id != DW_TAG_enumeration_type) {
		msg_stderr("invalid argument type: return NULL\n");
		return -(EINVAL);
	}

	for (i = 0; i < type->enumeration_type_info.enumerators_count; ++i) {
		if (strcmp(name, type->enumeration_type_info.enumerators[i].name) == 0) {
			if (p_const_value != NULL)
				*p_const_value = type->enumeration_type_info.enumerators[i].const_value;
			return 0;
		}
	}
	msg_stddbg("wan't able to find const_value for %s\n", name);
	return -(EFAULT);
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

int metac_type_array_subrange_count(struct metac_type *type, metac_num_t subrange_id, metac_count_t *p_count) {
	if (type == NULL) {
		msg_stderr("invalid argument value: type\n");
		return -(EINVAL);
	}
	if (type->id != DW_TAG_array_type) {
		msg_stderr("type id isn't ARRAY\n");
		return -(EINVAL);
	}
	if (subrange_id >= type->array_type_info.subranges_count) {
		msg_stderr("subrange_id is too big\n");
		return -(EINVAL);
	}

	if (p_count != NULL)
		*p_count = type->array_type_info.subranges[subrange_id].count;
	return type->array_type_info.subranges[subrange_id].is_flexible;
}

int metac_type_array_member_location(struct metac_type *type, metac_num_t subranges_count, metac_num_t * subranges, metac_data_member_location_t *p_data_member_location) {
	int res = 0;
	metac_num_t i;
	metac_data_member_location_t offset = 0;
	metac_data_member_location_t ratio = 1;

	if (type == NULL) {
		msg_stderr("invalid argument value: type\n");
		return -(EINVAL);
	}
	if (type->id != DW_TAG_array_type) {
		msg_stderr("type id isn't ARRAY\n");
		return -(EINVAL);
	}
	if (subranges_count > type->array_type_info.subranges_count) {
		msg_stderr("subranges_count is too big\n");
		return -(EINVAL);
	}

	for (i = type->array_type_info.subranges_count; i > 0; i--){
		metac_num_t index = i - 1;
		metac_num_t current_index_value = (index < subranges_count)?subranges[index]:0;
		if (current_index_value < type->array_type_info.subranges[index].lower_bound) {
			msg_stderr("ERROR: %x index must be in range [%x, %x), but it's %x\n", index,
					type->array_type_info.subranges[index].lower_bound,
					type->array_type_info.subranges[index].lower_bound + type->array_type_info.subranges[index].count,
					current_index_value);
			return -(EINVAL);
		}
		if (current_index_value >= type->array_type_info.subranges[index].lower_bound + type->array_type_info.subranges[index].count) {
			msg_stddbg("WARNING: %x index should be in range [%x, %x), but it's %x\n", index,
					type->array_type_info.subranges[index].lower_bound,
					type->array_type_info.subranges[index].lower_bound + type->array_type_info.subranges[index].count,
					current_index_value);
			++res;
		}

		current_index_value -= type->array_type_info.subranges[index].lower_bound;
		offset +=  ratio*current_index_value;
		//msg_stddbg("DBG: index %d, current index %d, ratio %d\n", index, current_index_value, ratio);
		ratio*= type->array_type_info.subranges[index].count;

	}
	//msg_stddbg("DBG: offset %d\n", offset);
	if (p_data_member_location)
		*p_data_member_location = offset*metac_type_byte_size(type->array_type_info.type);
	return res;
}

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

struct metac_object * metac_object_by_name(struct metac_object_sorted_array * array, metac_name_t name) {
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
/*****************************************************************************/
metac_array_info_t * metac_array_info_create_from_type(struct metac_type *type) {
	metac_num_t i;
	metac_num_t subranges_count;
	metac_array_info_t * p_array_info;
	metac_type_id_t id;

	if (type == NULL) {
		msg_stderr("invalid argument value: type\n");
		return NULL;
	}

	id = metac_type_typedef_skip(type)->id;
	switch (id) {
	case DW_TAG_pointer_type:
		subranges_count = 1;
		break;
	case DW_TAG_array_type:
		subranges_count = type->array_type_info.subranges_count;
		break;
	default:
		msg_stderr("metac_array_info_t can't be created(%d)\n", (int)id);
		return NULL;
	}

	p_array_info = calloc(1, sizeof(*p_array_info) + subranges_count * sizeof(struct _metac_array_subrange_info));
	if (p_array_info == NULL) {
		msg_stderr("no memory\n");
		return NULL;
	}
	p_array_info->subranges_count = subranges_count;
	p_array_info->subranges[0].count = 1;

	if (id == DW_TAG_array_type) {
		for (i = 0; i < subranges_count; ++i) {
			metac_type_array_subrange_count(type, i, &p_array_info->subranges[i].count);
		}
	}

	return p_array_info;
}
metac_array_info_t * metac_array_info_create_from_elements_count(metac_count_t elements_count) {
	metac_num_t subranges_count = 1;
	metac_array_info_t * p_array_info;

	p_array_info = calloc(1, sizeof(*p_array_info) + subranges_count * sizeof(struct _metac_array_subrange_info));
	if (p_array_info == NULL) {
		msg_stderr("no memory\n");
		return NULL;
	}
	p_array_info->subranges_count = subranges_count;
	p_array_info->subranges[0].count = elements_count;

	return p_array_info;
}
metac_array_info_t * metac_array_info_copy(metac_array_info_t *p_array_info_orig) {
	metac_num_t i;
	metac_num_t subranges_count;
	metac_array_info_t * p_array_info;
	metac_type_id_t id;

	if (p_array_info_orig == NULL) {
		msg_stderr("invalid argument value: p_array_info\n");
		return NULL;
	}

	subranges_count = p_array_info_orig->subranges_count;

	p_array_info = calloc(1, sizeof(*p_array_info) + subranges_count * sizeof(struct _metac_array_subrange_info));
	if (p_array_info == NULL) {
		msg_stderr("no memory\n");
		return NULL;
	}
	p_array_info->subranges_count = subranges_count;

	for (i = 0; i < p_array_info->subranges_count; ++i) {
		p_array_info->subranges[i].count = p_array_info_orig->subranges[i].count;
	}

	return p_array_info;
}
int metac_array_info_delete(metac_array_info_t ** pp_array_info) {
	if (pp_array_info != NULL) {
		metac_array_info_t * p_array_info = *pp_array_info;
		if (p_array_info != NULL) {
			free(p_array_info);
		}
		*pp_array_info = NULL;
	}
	return 0;
}
metac_count_t metac_array_info_get_element_count(metac_array_info_t * p_array_info) {
	metac_num_t i;
	metac_count_t element_count;

	if (p_array_info == NULL || p_array_info->subranges_count == 0)
		return 1;

	element_count = p_array_info->subranges[0].count;
	for (i = 1; i < p_array_info->subranges_count; ++i) {
		element_count *= p_array_info->subranges[i].count;
	}
	return element_count;
}
int metac_array_info_equal(metac_array_info_t * p_array_info0, metac_array_info_t * p_array_info1) {
	metac_num_t i;

	if (p_array_info0 == NULL ||
		p_array_info1 == NULL) {
		return -EINVAL;
	}

	if (p_array_info0->subranges_count != p_array_info1->subranges_count)
		return 0;

	for (i = 0; i < p_array_info0->subranges_count; ++i) {
		if (p_array_info0->subranges[i].count != p_array_info1->subranges[i].count)
			return 0;
	}

	return 1;
}
metac_array_info_t * metac_array_info_counter_init(metac_array_info_t *p_array_info_orig) {
	metac_num_t i;
	metac_num_t subranges_count;
	metac_array_info_t * p_array_info;
	metac_type_id_t id;

	if (p_array_info_orig == NULL) {
		msg_stderr("invalid argument value: p_array_info\n");
		return NULL;
	}

	subranges_count = p_array_info_orig->subranges_count;

	p_array_info = calloc(1, sizeof(*p_array_info) + subranges_count * sizeof(struct _metac_array_subrange_info));
	if (p_array_info == NULL) {
		msg_stderr("no memory\n");
		return NULL;
	}
	p_array_info->subranges_count = subranges_count;

	for (i = 0; i < p_array_info->subranges_count; ++i) {
		p_array_info->subranges[i].count = 0;
	}

	return p_array_info;
}

int metac_array_info_counter_increment(metac_array_info_t *p_array_info_orig, metac_array_info_t *p_array_info_current) {
	metac_num_t i;
	if (p_array_info_orig == NULL ||
		p_array_info_current == NULL ||
		p_array_info_orig->subranges_count != p_array_info_current->subranges_count) {
		msg_stderr("invalid argument value\n");
		return -EINVAL;
	}

	i = p_array_info_current->subranges_count - 1;
	while(1) {
		++p_array_info_current->subranges[i].count;
		if (p_array_info_current->subranges[i].count == p_array_info_orig->subranges[i].count) {
			p_array_info_current->subranges[i].count = 0;
			if (i == 0)return 1;	/*overflow*/
			--i;
		} else break;
	};
	return 0;
}

/*****************************************************************************/
int metac_array_elements_single(
	int write_operation,
	void * ptr,
	metac_type_t * type,
	void * first_element_ptr,
	metac_type_t * first_element_type,
	metac_array_info_t * p_array_info,
	void * array_elements_count_cb_context) {
	metac_count_t i;
	if (write_operation == 0) {
		for (i = 0; i < p_array_info->subranges_count; i++)
			p_array_info->subranges[i].count = 1;
		return 0;
	}
	return 0;
}

int metac_array_elements_1d_with_null(
	int write_operation,
	void * ptr,
	metac_type_t * type,
	void * first_element_ptr,
	metac_type_t * first_element_type,
	metac_array_info_t * p_array_info,
	void * array_elements_count_cb_context) {

	if (p_array_info->subranges_count != 1) {
		msg_stderr("metac_array_elements_1d_with_null can work only with 1 dimension arrays\n");
		return -EFAULT;
	}

	if (write_operation == 0) {
		metac_byte_size_t j;
		metac_byte_size_t element_size = metac_type_byte_size(first_element_type);
		metac_count_t i = 0;
		msg_stddbg("elements_size %d\n", (int)element_size);
		do {
			unsigned char * _ptr;
			for (j=0; j<element_size; j++) { /*non optimal - can use different sized to char,short,int &etc, see memcmp for reference*/
				_ptr = ((unsigned char *)first_element_ptr) + i*element_size + j;
				if ((*_ptr) != 0){
					++i;
					break;
				}
			}
			if ((*_ptr) == 0) break;
		}while(1);
		/*found all zeroes*/
		++i;
		p_array_info->subranges[0].count = i;
		msg_stddbg("p_elements_count %d\n", (int)i);
		return 0;
	}
	return 0;
}

