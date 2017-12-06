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

