/*
 * metac_json.c
 *
 *  Created on: Jan 15, 2019
 *      Author: mralex
 */
//#define METAC_DEBUG_ENABLE

#include "metac_type.h"
#include "metac_debug.h"	/* msg_stderr, ...*/

#include <json/json.h>		/* json_... */
#include <urcu/list.h>		/* cds_list_entry, ... */
#include <errno.h>			/* ENOMEM etc */
#include <assert.h>			/* assert */
#include <stdint.h>			/* int8_t - int64_t*/
#include <complex.h>		/* complext */
#include <string.h>			/* strcmp*/
#include <endian.h>			/* __BYTE_ORDER at compile time */

/*****************************************************************************/
/*Caution: this has been tested only with little-endian machines*/
#define _COPY_FROM_BITFIELDS_(_ptr_, _type_, _signed_, _mask_, _p_bit_offset, _p_bit_size) do { \
	metac_bit_offset_t shift = \
		(byte_size << 3) - ((_p_bit_size?(*_p_bit_size):0) + (_p_bit_offset?(*_p_bit_offset):0)); \
	_type_ mask = ~(((_type_)_mask_) << (_p_bit_size?(*_p_bit_size):0)); \
	_type_ _i = *((_type_*)(ptr)); \
	_i = (_i >> shift) & mask; \
	if ((_signed_) && (_i & (1 << (_p_bit_size?(*_p_bit_size):0)))) { \
		_i = (mask << ((_p_bit_size?(*_p_bit_size):0))) ^ _i;\
	} \
	*(_type_*)_ptr_ = _i; \
}while(0)
/*****************************************************************************/
static int _metac_base_type_to_json(
		struct metac_type * type,
		void *ptr,
		metac_bit_offset_t * p_bit_offset,
		metac_bit_size_t * p_bit_size,
		metac_byte_size_t byte_size,
		json_object ** pp_json
		) {
	int len = 0;
	char buf[64] = {0,};

	assert(type->id == DW_TAG_base_type);
	if (p_bit_offset != NULL || p_bit_size != NULL) {
		int is_signed = type->base_type_info.encoding == DW_ATE_signed_char || type->base_type_info.encoding == DW_ATE_signed;
		switch(type->base_type_info.byte_size){
		case sizeof(uint8_t):	_COPY_FROM_BITFIELDS_(buf, uint8_t,		is_signed , 0xff, p_bit_offset, p_bit_size); break;
		case sizeof(uint16_t):	_COPY_FROM_BITFIELDS_(buf, uint16_t,	is_signed , 0xffff, p_bit_offset, p_bit_size); break;
		case sizeof(uint32_t):	_COPY_FROM_BITFIELDS_(buf, uint32_t,	is_signed , 0xffffffff, p_bit_offset, p_bit_size); break;
		case sizeof(uint64_t):	_COPY_FROM_BITFIELDS_(buf, uint64_t,	is_signed , 0xffffffffffffffff, p_bit_offset, p_bit_size); break;
		default: msg_stderr("BITFIELDS: byte_size %d isn't supported\n", (int)type->base_type_info.byte_size); return -EINVAL;
		}
		return _metac_base_type_to_json(type, buf, NULL, NULL, byte_size, pp_json);
	}

	switch(type->base_type_info.encoding) {
	case DW_ATE_unsigned_char:
	case DW_ATE_signed_char:
		msg_stddbg("DW_ATE_(un)signed_char (byte)\n");
		if (type->base_type_info.byte_size == sizeof(int8_t)) {
			char v = *((char*)ptr);
			buf[0] = v;
			len = 1;
			break;
		}
	/* break removed - fallback to std approach*/
	case DW_ATE_signed:
	case DW_ATE_unsigned:
		switch (type->base_type_info.encoding) {
		case DW_ATE_signed:
		case DW_ATE_signed_char:
			msg_stddbg("DW_ATE_signed\n");
			switch (type->base_type_info.byte_size) {
			case sizeof(int8_t):  len = snprintf(buf, sizeof(buf), "%"SCNi8"" , *((int8_t* )ptr)); break;
			case sizeof(int16_t): len = snprintf(buf, sizeof(buf), "%"SCNi16"", *((int16_t*)ptr)); break;
			case sizeof(int32_t): len = snprintf(buf, sizeof(buf), "%"SCNi32"", *((int32_t*)ptr)); break;
			case sizeof(int64_t): len = snprintf(buf, sizeof(buf), "%"SCNi64"", *((int64_t*)ptr)); break;
			default: msg_stderr("DW_ATE_signed: Unsupported byte_size %d\n",(int)type->base_type_info.byte_size); return -EINVAL;
			}
			break;
		case DW_ATE_unsigned:
		case DW_ATE_unsigned_char:
			msg_stddbg("DW_ATE_unsigned\n");
			switch (type->base_type_info.byte_size) {
			case sizeof(uint8_t):  len = snprintf(buf, sizeof(buf), "%"SCNu8"" , *((uint8_t* )ptr)); break;
			case sizeof(uint16_t): len = snprintf(buf, sizeof(buf), "%"SCNu16"", *((uint16_t*)ptr)); break;
			case sizeof(uint32_t): len = snprintf(buf, sizeof(buf), "%"SCNu32"", *((uint32_t*)ptr)); break;
			case sizeof(uint64_t): len = snprintf(buf, sizeof(buf), "%"SCNu64"", *((uint64_t*)ptr)); break;
			default: msg_stderr("DW_ATE_unsigned: Unsupported byte_size %d\n",(int)type->base_type_info.byte_size); return -EINVAL;
			}
			break;
		}
		break;
	case DW_ATE_float:
		switch(type->base_type_info.byte_size) {
		case sizeof(float):			len = snprintf(buf, sizeof(buf), "%f",	*((float*		)ptr)); break;
		case sizeof(double):		len = snprintf(buf, sizeof(buf), "%f",	*((double*		)ptr)); break;
		case sizeof(long double):	len = snprintf(buf, sizeof(buf), "%Lf",	*((long double*	)ptr)); break;
		default: msg_stderr("DW_ATE_float: Unsupported byte_size %d\n",(int)type->base_type_info.byte_size); return -EINVAL;
		}
		break;
	case DW_ATE_complex_float:
		switch(type->base_type_info.byte_size) {
		case sizeof(float complex): {
				float complex v = *((float complex*)ptr);
				len = snprintf(buf, sizeof(buf), "%f + i%f", creal(v), cimag(v));
			}
			break;
		case sizeof(double complex): {
				double complex v = *((double complex*)ptr);
				len = snprintf(buf, sizeof(buf), "%f + i%f", creal(v), cimag(v));
			}
			break;
		case sizeof(long double complex): {
				long double complex v = *((long double complex*)ptr);
				len = snprintf(buf, sizeof(buf), "%f + i%f", creal(v), cimag(v));
			}
			break;
		default: msg_stderr("DW_ATE_complex_float: Unsupported byte_size %d\n",(int)type->base_type_info.byte_size); return -EINVAL;
		}
		break;
	default: msg_stderr("Unsupported encoding %d\n",(int)type->base_type_info.encoding); return -EINVAL;
	}

	if (pp_json) {
		*pp_json = json_object_new_string_len(buf, len);
	}
	return 0;
}
/*****************************************************************************/
static int _metac_enumeration_type_to_json(
		struct metac_type * type,
		void *ptr,
		metac_byte_size_t byte_size,
		json_object ** pp_json
		) {
	metac_num_t i;
	metac_const_value_t const_value;

	assert(type->id == DW_TAG_enumeration_type);
	switch(type->enumeration_type_info.byte_size) {
	case sizeof(int8_t):	const_value = *((int8_t*	)ptr);	break;
	case sizeof(int16_t):	const_value = *((int16_t*	)ptr);	break;
	case sizeof(int32_t):	const_value = *((int32_t*	)ptr);	break;
	case sizeof(int64_t):	const_value = *((int64_t*	)ptr);	break;
	default:
#ifdef __BYTE_ORDER
		/*read maximum bits we can*/
#if __BYTE_ORDER == __LITTLE_ENDIAN
		const_value = *((int64_t*	)ptr);	break;
#else
		const_value = *((int64_t*	)(ptr + (type->enumeration_type_info.byte_size-sizeof(int64_t))));	break;
#endif
#else
		/*fallback if we don't know endian*/
		msg_stderr("byte_size %d isn't supported\n", (int)type->enumeration_type_info.byte_size);
		return -EINVAL;
#endif
	}

	for (i = 0; i < type->enumeration_type_info.enumerators_count; ++i) {
		if (const_value == type->enumeration_type_info.enumerators[i].const_value) {
			if (pp_json) {
				*pp_json = json_object_new_string(type->enumeration_type_info.enumerators[i].name);
			}
			return 0;
		}
	}
	msg_stderr("wasn't able to find enum name for %d\n", (int)const_value);
	return -EINVAL;
}
/*****************************************************************************/
struct region_element_element {
	metac_type_t * type;

	void *ptr;
	metac_bit_offset_t * p_bit_offset;
	metac_bit_size_t * p_bit_size;
	metac_byte_size_t byte_size;

	char * name_local;
	char * path_within_region_element;

	struct region_element_element * parent;

	metac_region_ee_subtype_t subtype;

	/*for arrays or pointers only*/
	struct region * p_linked_region;

	json_object * p_json_object;
};


struct region_element {
	metac_type_t * type;

	void *ptr;
	metac_byte_size_t byte_size;

	int real_region_element_element_count;
	struct region_element_element * real_region_element_element;

	int arrays_count;
	struct region_element_element ** arrays;
	int pointers_count;
	struct region_element_element ** pointers;
};

struct region {
	void *ptr;
	metac_byte_size_t byte_size;
	metac_count_t elements_count;
	struct region_element * elements;

	/*for unique only*/
	json_object * p_json_object;

	/*for non_unique only*/
	metac_count_t u_idx;
	metac_data_member_location_t offset;
};

struct json_visitor {
	struct metac_visitor visitor; /**< parent type */

	struct region * regions;
	metac_count_t regions_count;

	struct region ** unique_regions;
	metac_count_t unique_regions_count;
};
/*****************************************************************************/
static void json_visitor_cleanup(struct json_visitor * json_visitor) {

	if (json_visitor->unique_regions != NULL) {
		free(json_visitor->unique_regions);
		json_visitor->unique_regions = NULL;
	}

	if (json_visitor->regions != NULL) {
		int i;
		for (i = 0; i < json_visitor->regions_count; ++i) {
			if (json_visitor->regions[i].p_json_object != NULL) {
				json_object_put(json_visitor->regions[i].p_json_object);
				json_visitor->regions[i].p_json_object = NULL;
			}
			if (json_visitor->regions[i].elements != NULL) {
				int j;
				for (j = 0 ;j < json_visitor->regions[i].elements_count; ++j) {
					if (json_visitor->regions[i].elements[j].arrays != NULL) {
						free(json_visitor->regions[i].elements[j].arrays);
						json_visitor->regions[i].elements[j].arrays = NULL;
					}

					if (json_visitor->regions[i].elements[j].pointers != NULL) {
						free(json_visitor->regions[i].elements[j].pointers);
						json_visitor->regions[i].elements[j].pointers = NULL;
					}

					if (json_visitor->regions[i].elements[j].real_region_element_element != NULL) {
						int k;
						for (k = 0; k < json_visitor->regions[i].elements[j].real_region_element_element_count; ++k) {
							if (json_visitor->regions[i].elements[j].real_region_element_element[k].p_json_object != NULL) {
								json_object_put(json_visitor->regions[i].elements[j].real_region_element_element[k].p_json_object);
								json_visitor->regions[i].elements[j].real_region_element_element[k].p_json_object = NULL;
							}
						}
						free(json_visitor->regions[i].elements[j].real_region_element_element);
						json_visitor->regions[i].elements[j].real_region_element_element = NULL;
					}
				}
				free(json_visitor->regions[i].elements);
				json_visitor->regions[i].elements = NULL;
			}
		}
		free(json_visitor->regions);
		json_visitor->regions = NULL;
	}
}
/*****************************************************************************/
static int json_visitor_start(
		struct metac_visitor *p_visitor,
		metac_count_t regions_count,
		metac_count_t unique_regions_count
		) {
	struct json_visitor * json_visitor = cds_list_entry(p_visitor, struct json_visitor, visitor);
	int i;

	json_visitor->regions_count = regions_count;
	json_visitor->regions = calloc(json_visitor->regions_count, sizeof(*json_visitor->regions));
	if (json_visitor->regions == NULL) {
		msg_stderr("Can't allocate memory\n");
		json_visitor_cleanup(json_visitor);
		return -ENOMEM;
	}

	json_visitor->unique_regions_count = unique_regions_count;
	json_visitor->unique_regions = calloc(json_visitor->unique_regions_count, sizeof(*json_visitor->unique_regions));
	if (json_visitor->regions == NULL) {
		msg_stderr("Can't allocate memory\n");
		json_visitor_cleanup(json_visitor);
		return -ENOMEM;
	}

	return 0;
}
static int json_visitor_region(
		struct metac_visitor *p_visitor,
		metac_count_t r_id,
		void *ptr,
		metac_byte_size_t byte_size,
		metac_count_t elements_count
		) {
	struct json_visitor * json_visitor = cds_list_entry(p_visitor, struct json_visitor, visitor);
	assert(r_id < json_visitor->regions_count);

	json_visitor->regions[r_id].ptr = ptr;
	json_visitor->regions[r_id].byte_size = byte_size;
	json_visitor->regions[r_id].elements_count = elements_count;
	json_visitor->regions[r_id].elements = calloc(elements_count, sizeof(*json_visitor->regions[r_id].elements));
	if (json_visitor->regions[r_id].elements == NULL) {
		msg_stderr("Can't allocate memory\n");
		return -ENOMEM;
	}
	return 0;
}
static int json_visitor_unique_region(
		struct metac_visitor *p_visitor,
		metac_count_t r_id,
		metac_count_t u_idx
		) {
	struct json_visitor * json_visitor = cds_list_entry(p_visitor, struct json_visitor, visitor);
	assert(r_id < json_visitor->regions_count);
	assert(u_idx < json_visitor->unique_regions_count);

	json_visitor->regions[r_id].u_idx = u_idx;
	json_visitor->unique_regions[u_idx] = &json_visitor->regions[r_id];

	/*unique region - always need array of elements */
	json_visitor->regions[r_id].p_json_object = json_object_new_array();
	if (json_visitor->regions[r_id].p_json_object == NULL) {
		msg_stderr("Can't allocate memory\n");
		json_visitor_cleanup(json_visitor);
		return -ENOMEM;
	}

	return 0;
}
static int json_visitor_non_unique_region(
		struct metac_visitor *p_visitor,
		metac_count_t r_id,
		metac_count_t u_idx,
		metac_data_member_location_t offset
		) {
	struct json_visitor * json_visitor = cds_list_entry(p_visitor, struct json_visitor, visitor);
	assert(r_id < json_visitor->regions_count);
	assert(u_idx < json_visitor->unique_regions_count);
	assert(offset < json_visitor->unique_regions[u_idx]->byte_size);

	json_visitor->regions[r_id].u_idx = u_idx;
	json_visitor->regions[r_id].offset = offset;
	return 0;
}
static int json_visitor_region_element(
		struct metac_visitor *p_visitor,
		metac_count_t r_id,
		metac_count_t e_id,
		metac_type_t * type,
		void *ptr,
		metac_byte_size_t byte_size,
		int real_region_element_element_count,
		metac_region_ee_subtype_t *subtypes_sequence,
		int * real_count_array_per_type, /*array with real number of elements_elements for each item in subtypes_sequence*/
		int subtypes_sequence_lenth
		) {
	struct json_visitor * json_visitor = cds_list_entry(p_visitor, struct json_visitor, visitor);
	assert(r_id < json_visitor->regions_count);
	assert(e_id < json_visitor->regions[r_id].elements_count);


	json_visitor->regions[r_id].elements[e_id].type = type;
	json_visitor->regions[r_id].elements[e_id].ptr = ptr;
	json_visitor->regions[r_id].elements[e_id].byte_size = byte_size;
	json_visitor->regions[r_id].elements[e_id].real_region_element_element_count = real_region_element_element_count;
	json_visitor->regions[r_id].elements[e_id].real_region_element_element =
			calloc(real_region_element_element_count, sizeof(*json_visitor->regions[r_id].elements[e_id].real_region_element_element));
	if (json_visitor->regions[r_id].elements[e_id].real_region_element_element == NULL) {
		msg_stderr("Can't allocate memory\n");
		return -ENOMEM;
	}

	assert(subtypes_sequence[3] == reesPointer);
	json_visitor->regions[r_id].elements[e_id].pointers_count = real_count_array_per_type[3];
	if (json_visitor->regions[r_id].elements[e_id].pointers_count){
		json_visitor->regions[r_id].elements[e_id].pointers = calloc(
				json_visitor->regions[r_id].elements[e_id].pointers_count,
				sizeof(*json_visitor->regions[r_id].elements[e_id].pointers));
		if (json_visitor->regions[r_id].elements[e_id].pointers == NULL) {
			msg_stderr("Can't allocate memory\n");
			return -ENOMEM;
		}
	}

	assert(subtypes_sequence[4] == reesArray);
	json_visitor->regions[r_id].elements[e_id].arrays_count = real_count_array_per_type[4];
	json_visitor->regions[r_id].elements[e_id].arrays = calloc(
			json_visitor->regions[r_id].elements[e_id].arrays_count,
			sizeof(*json_visitor->regions[r_id].elements[e_id].arrays));
	if (json_visitor->regions[r_id].elements[e_id].arrays == NULL) {
		msg_stderr("Can't allocate memory\n");
		return -ENOMEM;
	}

	return 0;
}
static int json_visitor_region_element_element(
		struct metac_visitor *p_visitor,
		metac_count_t r_id,
		metac_count_t e_id,
		metac_count_t ee_id,
		metac_count_t parent_ee_id,
		metac_type_t * type,
		void *ptr,
		metac_bit_offset_t * p_bit_offset,
		metac_bit_size_t * p_bit_size,
		metac_byte_size_t byte_size,
		char * name_local,
		char * path_within_region_element
		) {
	struct json_visitor * json_visitor = cds_list_entry(p_visitor, struct json_visitor, visitor);
	assert(r_id < json_visitor->regions_count);
	assert(e_id < json_visitor->regions[r_id].elements_count);
	assert(ee_id < json_visitor->regions[r_id].elements[e_id].real_region_element_element_count);

	if (ee_id != 0) {
		json_visitor->regions[r_id].elements[e_id].real_region_element_element[ee_id].parent =
				&json_visitor->regions[r_id].elements[e_id].real_region_element_element[parent_ee_id];
	}
	json_visitor->regions[r_id].elements[e_id].real_region_element_element[ee_id].type = type;
	json_visitor->regions[r_id].elements[e_id].real_region_element_element[ee_id].ptr = ptr;
	json_visitor->regions[r_id].elements[e_id].real_region_element_element[ee_id].p_bit_offset = p_bit_offset;
	json_visitor->regions[r_id].elements[e_id].real_region_element_element[ee_id].p_bit_size = p_bit_size;
	json_visitor->regions[r_id].elements[e_id].real_region_element_element[ee_id].byte_size = byte_size;
	json_visitor->regions[r_id].elements[e_id].real_region_element_element[ee_id].name_local = name_local;
	json_visitor->regions[r_id].elements[e_id].real_region_element_element[ee_id].path_within_region_element = path_within_region_element;

	return 0;
}
int json_region_element_element_per_subtype(
		struct metac_visitor *p_visitor,
		metac_count_t r_id,
		metac_count_t e_id,
		metac_count_t ee_id,
		metac_region_ee_subtype_t subtype,
		metac_count_t see_id,
		int n,
		metac_count_t * p_elements_count,
		metac_count_t * p_linked_r_id
		){
	struct json_visitor * json_visitor = cds_list_entry(p_visitor, struct json_visitor, visitor);
	struct region_element_element * ee;
	assert(r_id < json_visitor->regions_count);
	assert(e_id < json_visitor->regions[r_id].elements_count);
	assert(ee_id < json_visitor->regions[r_id].elements[e_id].real_region_element_element_count);
	ee = &json_visitor->regions[r_id].elements[e_id].real_region_element_element[ee_id];

	ee->subtype = subtype;
	switch(subtype) {
	case reesHierarchy:
		if (ee->parent == NULL || strlen(ee->name_local) > 0) {
			ee->p_json_object = json_object_new_object();
			if (ee->parent != NULL) {
				json_object_object_add(ee->parent->p_json_object, ee->name_local, json_object_get(ee->p_json_object));
			}
		}else{
			ee->p_json_object = json_object_get(ee->parent->p_json_object);
		}
		break;
	case reesEnum:
		if (_metac_enumeration_type_to_json(ee->type, ee->ptr, ee->byte_size, &ee->p_json_object) != 0) {
			msg_stderr("_metac_enumeration_type_to_json error\n");
			return -ENOMEM;
		}
		if (ee->parent != NULL) {
			assert(strlen(ee->name_local) > 0);
			json_object_object_add(ee->parent->p_json_object, ee->name_local, json_object_get(ee->p_json_object));
		}
		break;
	case reesBase:
		if (_metac_base_type_to_json(ee->type, ee->ptr, ee->p_bit_offset, ee->p_bit_size, ee->byte_size, &ee->p_json_object) != 0) {
			msg_stderr("_metac_base_type_to_json error\n");
			return -ENOMEM;
		}
		if (ee->parent != NULL) {
			assert(strlen(ee->name_local) > 0);
			json_object_object_add(ee->parent->p_json_object, ee->name_local, json_object_get(ee->p_json_object));
		}
		break;
	case reesPointer:
		assert(p_linked_r_id ==NULL || *p_linked_r_id < json_visitor->regions_count);
		ee->p_linked_region = (p_linked_r_id != NULL)?(&json_visitor->regions[*p_linked_r_id]):NULL;
		assert(see_id < json_visitor->regions[r_id].elements[e_id].pointers_count);
		json_visitor->regions[r_id].elements[e_id].pointers[see_id] = ee;
		ee->p_json_object = json_object_new_object();
		if (ee->p_json_object == NULL) {
			msg_stderr("json_object_new_object failed\n");
			json_visitor_cleanup(json_visitor);
			return -ENOMEM;
		}
		if (ee->parent != NULL) {
			assert(strlen(ee->name_local) > 0);
			json_object_object_add(ee->parent->p_json_object, ee->name_local, json_object_get(ee->p_json_object));
		}
		break;
	case reesArray:
		assert(p_linked_r_id ==NULL || *p_linked_r_id < json_visitor->regions_count);
		ee->p_linked_region = (p_linked_r_id != NULL)?(&json_visitor->regions[*p_linked_r_id]):NULL;
		assert(see_id < json_visitor->regions[r_id].elements[e_id].arrays_count);
		json_visitor->regions[r_id].elements[e_id].arrays[see_id] = ee;
		ee->p_json_object = json_object_new_array();
		if (ee->p_json_object == NULL) {
			msg_stderr("json_object_new_array failed\n");
			json_visitor_cleanup(json_visitor);
			return -ENOMEM;
		}
		if (ee->parent != NULL) {
			assert(strlen(ee->name_local) > 0);
			json_object_object_add(ee->parent->p_json_object, ee->name_local, json_object_get(ee->p_json_object));
		}
		break;
	default:
		break;
	}

	return 0;
}
/*****************************************************************************/
int metac_unpack_to_json(
		void *ptr,
		metac_byte_size_t size,
		metac_precompiled_type_t * precompiled_type,
		metac_count_t elements_count,
		json_object ** pp_json) {
	int i;
	int res = 0;
	json_object * res_json = NULL;

	static metac_region_ee_subtype_t subtypes_sequence[] = {
		reesHierarchy,
		reesEnum,
		reesBase,
		reesPointer,
		reesArray,
	};

	struct json_visitor json_visitor = {
		.visitor = {
			.start = json_visitor_start,
			.region = json_visitor_region,
			.non_unique_region = json_visitor_non_unique_region,
			.unique_region = json_visitor_unique_region,
			.region_element = json_visitor_region_element,
			.region_element_element = json_visitor_region_element_element,
			.region_element_element_per_subtype = json_region_element_element_per_subtype,
		},
	};

	res = metac_visit(ptr, size, precompiled_type, elements_count, subtypes_sequence,
			sizeof(subtypes_sequence)/sizeof(subtypes_sequence[0]),
			&json_visitor.visitor);
	if (res != 0) {
		msg_stderr("metac_visit failed\n");
		json_visitor_cleanup(&json_visitor);
		return -EFAULT;
	}

	res_json = json_object_new_array();
	if (res_json == NULL) {
		msg_stderr("json_object_new_array failed\n");
		json_visitor_cleanup(&json_visitor);
		return -ENOMEM;
	}

	/*prepare pointers and arrays first*/
	for (i = 0; i < json_visitor.regions_count; ++i) {
		int j;
		struct region * region= &json_visitor.regions[i];
		for (j = 0 ;j < region->elements_count; ++j) {
			int k;
			for (k = 0; k < region->elements[j].arrays_count; ++k) {
				if (region->elements[j].arrays[k]->p_linked_region != NULL) {
					int l;
					/*must be non-unique region*/
					assert(region->elements[j].arrays[k]->p_linked_region->p_json_object == NULL);

					for (l = 0; l < region->elements[j].arrays[k]->p_linked_region->elements_count; ++l) {
						/*TBD - make it n-dimentions compatible!*/
						assert(region->elements[j].arrays[k]->p_linked_region->elements[l].real_region_element_element[0].p_json_object);
						json_object_array_add(region->elements[j].arrays[k]->p_json_object,
								json_object_get(region->elements[j].arrays[k]->p_linked_region->elements[l].real_region_element_element[0].p_json_object));
					}
				}
			}
			for (k = 0; k < region->elements[j].pointers_count; ++k) {
				if (region->elements[j].pointers[k]->p_linked_region != NULL) {
					/*TBD: try to make it as arrays if it's tree*/
					assert(region->elements[j].pointers[k]->p_json_object != NULL);
					json_object_object_add(region->elements[j].pointers[k]->p_json_object,
							"region_id", json_object_new_int(region->elements[j].pointers[k]->p_linked_region->u_idx));
					if (region->elements[j].pointers[k]->p_linked_region->offset != 0) {
						assert(region->elements[j].pointers[k]->p_linked_region->p_json_object == NULL);/* only unique regions have json_object created*/
						json_object_object_add(region->elements[j].pointers[k]->p_json_object,
								"offset", json_object_new_int(region->elements[j].pointers[k]->p_linked_region->offset));
					}
				}
			}
		}
	}

	for (i = 0; i < json_visitor.unique_regions_count; ++i) {
		int j;
		assert(json_visitor.unique_regions[i]->p_json_object != NULL);
		assert(json_visitor.unique_regions[i]->elements != NULL);

		for (j = 0 ;j < json_visitor.unique_regions[i]->elements_count; ++j) {
			assert(json_visitor.unique_regions[i]->elements[j].real_region_element_element != NULL);
			assert(json_visitor.unique_regions[i]->elements[j].real_region_element_element[0].p_json_object != NULL);
			json_object_array_add(json_visitor.unique_regions[i]->p_json_object,
					json_object_get(json_visitor.unique_regions[i]->elements[j].real_region_element_element[0].p_json_object));
		}
		json_object_array_add(res_json, json_object_get(json_visitor.unique_regions[i]->p_json_object));
	}

	if (pp_json) {
		*pp_json = res_json;
	}

	json_visitor_cleanup(&json_visitor);
	return 0;
}

