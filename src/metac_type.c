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

#include "metac/metac_type.h"
#include "metac_debug.h"

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

static const metac_type_annotation_t *
	_annotation_by_key(metac_type_annotation_t *annotations, const char *key) {
	int i = 0;
	if (annotations == NULL) {
		msg_stddbg("type %s doesn't have specifications\n", type->name != NULL?type->name:"(nil)");
		return NULL;
	}
	while (annotations[i].key != NULL) {
		if (strcmp(annotations[i].key, key) == 0)
			return &annotations[i];
		++i;
	}
	return NULL;
}

const metac_type_annotation_t *
	metac_type_annotation(struct metac_type *type, const char *key, metac_type_annotation_t *override_annotations) {
	const metac_type_annotation_t * result;

	if (type == NULL){
		msg_stderr("invalid argument value: return NULL\n");
		return NULL;
	}

	if (override_annotations != NULL) {
		result = _annotation_by_key(override_annotations, key);
		if (result != NULL)
			return result;
	}
	return _annotation_by_key(type->annotations, key);
}

struct metac_type *metac_type_actual_type(struct metac_type *type) {
	if (type == NULL){
		msg_stderr("invalid argument value: return NULL\n");
		return NULL;
	}
	while (
			type->id == DW_TAG_typedef ||
			type->id == DW_TAG_const_type) {
		switch(type->id) {
		case DW_TAG_typedef:
			type = type->typedef_info.type;
			break;
		case DW_TAG_const_type:
			type = type->const_type_info.type;
			break;
		}
		if (type == NULL) {
			msg_stderr("typedef has to contain type in attributes: return NULL\n");
			return NULL;
		}
	}
	return type;
}

metac_byte_size_t metac_type_byte_size(struct metac_type *type) {
	if (type == NULL) {
		msg_stderr("invalid argument value: type\n");
		return 0;
	}

	type = metac_type_actual_type(type);
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
		msg_stddbg("DBG: index %d, current index %d, ratio %d\n", index, current_index_value, ratio);
		ratio*= type->array_type_info.subranges[index].count;

	}
	msg_stddbg("DBG: offset %d\n", offset);
	if (p_data_member_location)
		*p_data_member_location = offset*metac_type_byte_size(type->array_type_info.type);
	return res;
}

struct metac_type * metac_type_create_pointer_for(
		struct metac_type *							p_type) {
	_create_(metac_type);
	p_metac_type->id = DW_TAG_pointer_type;
	p_metac_type->pointer_type_info.type = p_type;
	return p_metac_type;
}
int metac_type_free(
		struct metac_type **						pp_metac_type) {
	_delete_(metac_type);
	return 0;
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
metac_array_info_t * metac_array_info_create_from_type(struct metac_type *type, metac_num_t default_subrange0_count) {
	metac_num_t i;
	metac_num_t subranges_count;
	metac_array_info_t * p_array_info;
	metac_type_id_t id;

	if (type == NULL) {
		msg_stderr("invalid argument value: type\n");
		return NULL;
	}

	id = metac_type_actual_type(type)->id;
	switch (id) {
	case DW_TAG_pointer_type:
		subranges_count = 1;
		break;
	case DW_TAG_array_type:
		subranges_count = type->array_type_info.subranges_count;
		break;
	default:
		msg_stderr("metac_array_info_t can't be created from type(0x%x)\n", (int)id);
		return NULL;
	}

	p_array_info = calloc(1, sizeof(*p_array_info) + subranges_count * sizeof(struct _metac_array_subrange_info));
	if (p_array_info == NULL) {
		msg_stderr("no memory\n");
		return NULL;
	}
	p_array_info->subranges_count = subranges_count;
	p_array_info->subranges[0].count = default_subrange0_count;

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
int metac_array_info_get_subrange_count(metac_array_info_t *p_array_info, metac_num_t subrange, metac_num_t *p_count) {
	if (p_array_info == NULL) {
		msg_stderr("invalid argument value: p_array_info\n");
		return (-EINVAL);
	}
	if (subrange >= p_array_info->subranges_count) {
		msg_stderr("invalid argument value: subrange\n");
		return (-EINVAL);
	}
	if (p_count != NULL) {
		*p_count = p_array_info->subranges[subrange].count;
	}
	return 0;
}
int metac_array_info_set_subrange_count(metac_array_info_t *p_array_info, metac_num_t subrange, metac_num_t count) {
	if (p_array_info == NULL) {
		msg_stderr("invalid argument value: p_array_info\n");
		return (-EINVAL);
	}
	if (subrange >= p_array_info->subranges_count) {
		msg_stderr("invalid argument value: subrange\n");
		return (-EINVAL);
	}
	p_array_info->subranges[subrange].count = count;
	return 0;
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
int metac_array_info_is_equal(metac_array_info_t * p_array_info0, metac_array_info_t * p_array_info1) {
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
metac_array_info_t * metac_array_info_create_counter(metac_array_info_t *p_array_info) {
	metac_num_t i;
	metac_num_t subranges_count;
	metac_array_info_t * p_array_info_counter;
	metac_type_id_t id;

	if (p_array_info == NULL) {
		msg_stderr("invalid argument value: p_array_info\n");
		return NULL;
	}

	subranges_count = p_array_info->subranges_count;

	p_array_info_counter = calloc(1, sizeof(*p_array_info_counter) + subranges_count * sizeof(struct _metac_array_subrange_info));
	if (p_array_info_counter == NULL) {
		msg_stderr("no memory\n");
		return NULL;
	}
	p_array_info_counter->subranges_count = subranges_count;

	for (i = 0; i < p_array_info_counter->subranges_count; ++i) {
		p_array_info_counter->subranges[i].count = 0;
	}

	return p_array_info_counter;
}

int metac_array_info_increment_counter(metac_array_info_t *p_array_info, metac_array_info_t *p_array_info_counter) {
	metac_num_t i;
	if (p_array_info == NULL ||
			p_array_info_counter == NULL ||
			p_array_info->subranges_count != p_array_info_counter->subranges_count) {
		msg_stderr("invalid argument value\n");
		return -EINVAL;
	}

	i = p_array_info_counter->subranges_count - 1;
	while(1) {
		++p_array_info_counter->subranges[i].count;
		if (p_array_info_counter->subranges[i].count == p_array_info->subranges[i].count) {
			p_array_info_counter->subranges[i].count = 0;
			if (i == 0)return 1;	/*overflow*/
			--i;
		} else break;
	};
	return 0;
}

metac_array_info_t * metac_array_info_convert_linear_id_2_subranges(
		metac_array_info_t *p_array_info,
		metac_num_t linear_id) {
	int i;

	metac_array_info_t * p_subranges_id;

	if (linear_id >= metac_array_info_get_element_count(p_array_info)) {
		msg_stderr("Can't convert linear_id %d - too big for this array_info\n", (int)linear_id);
		return NULL;
	}

	p_subranges_id = metac_array_info_copy(p_array_info);
	if (p_subranges_id == NULL) {
		return NULL;
	}

	for (i = 1; i <= p_array_info->subranges_count; ++i){
		p_subranges_id->subranges[p_array_info->subranges_count - i].count = linear_id % p_array_info->subranges[p_array_info->subranges_count - i].count;
		linear_id = linear_id / p_array_info->subranges[p_array_info->subranges_count - i].count;
	}

	assert(linear_id == 0);

	return p_subranges_id;
}

/*****************************************************************************/
int metac_array_elements_single(
	char * annotation_key,
	int write_operation,
	void * ptr,
	metac_type_t * type,
	void * first_element_ptr,
	metac_type_t * first_element_type,
	metac_num_t * p_subrange0_count,
	void * array_elements_count_cb_context) {
	metac_count_t i;
	if (write_operation == 0) {
		if (p_subrange0_count)
			*p_subrange0_count = 1;
		return 0;
	}
	return 0;
}

int metac_array_elements_1d_with_null(
	char * annotation_key,
	int write_operation,
	void * ptr,
	metac_type_t * type,
	void * first_element_ptr,
	metac_type_t * first_element_type,
	metac_num_t * p_subrange0_count,
	void * array_elements_count_cb_context) {

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
		if (p_subrange0_count)
			*p_subrange0_count = i;
		msg_stddbg("p_elements_count %d\n", (int)i);
		return 0;
	}
	return 0;
}


