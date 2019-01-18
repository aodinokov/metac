/*
 * metac_json.c
 *
 *  Created on: Jan 15, 2019
 *      Author: mralex
 */
#define METAC_DEBUG_ENABLE

#include "metac_type.h"
#include "metac_debug.h"	/* msg_stderr, ...*/

#include <json/json.h>		/* json_... */
#include <urcu/list.h>		/* cds_list_entry, ... */
#include <errno.h>			/* ENOMEM etc */
#include <assert.h>			/* assert */

struct region_element {
	metac_type_t * type;

	void *ptr;
	metac_byte_size_t byte_size;

	int real_region_element_element_count;
};

struct region {
	void *ptr;
	metac_byte_size_t byte_size;
	metac_count_t elements_count;

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
		}
		free(json_visitor->regions);
		json_visitor->regions = NULL;
	}
}
/*****************************************************************************/
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
	char buf[32] = {0,};

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
			buf[0] = *((char*)ptr);
			if (isprint(buf[0]))break;
		}
	/* break removed - fallback to std approach*/
	case DW_ATE_signed:
	case DW_ATE_unsigned:
		switch (type->base_type_info.encoding) {
		case DW_ATE_signed:
		case DW_ATE_signed_char:
			msg_stddbg("DW_ATE_signed\n");
			switch (type->base_type_info.byte_size) {
			case sizeof(int8_t):  sprintf(buf, "%"SCNi8"" , *((int8_t* )ptr)); break;
			case sizeof(int16_t): sprintf(buf, "%"SCNi16"", *((int16_t*)ptr)); break;
			case sizeof(int32_t): sprintf(buf, "%"SCNi32"", *((int32_t*)ptr)); break;
			case sizeof(int64_t): sprintf(buf, "%"SCNi64"", *((int64_t*)ptr)); break;
			default: msg_stderr("DW_ATE_signed: Unsupported byte_size %d\n",(int)type->base_type_info.byte_size); return -EINVAL;
			}
			break;
		case DW_ATE_unsigned:
		case DW_ATE_unsigned_char:
			msg_stddbg("DW_ATE_unsigned\n");
			switch (type->base_type_info.byte_size) {
			case sizeof(uint8_t):  sprintf(buf, "%"SCNu8"" , *((uint8_t* )ptr)); break;
			case sizeof(uint16_t): sprintf(buf, "%"SCNu16"", *((uint16_t*)ptr)); break;
			case sizeof(uint32_t): sprintf(buf, "%"SCNu32"", *((uint32_t*)ptr)); break;
			case sizeof(uint64_t): sprintf(buf, "%"SCNu64"", *((uint64_t*)ptr)); break;
			default: msg_stderr("DW_ATE_unsigned: Unsupported byte_size %d\n",(int)type->base_type_info.byte_size); return -EINVAL;
			}
			break;
		}
		break;
	case DW_ATE_float:
		switch(type->base_type_info.byte_size) {
		case sizeof(float):			sprintf(buf, "%f",	*((float*		)ptr)); break;
		case sizeof(double):		sprintf(buf, "%f",	*((double*		)ptr)); break;
		case sizeof(long double):	sprintf(buf, "%Lf",	*((long double*	)ptr)); break;
		default: msg_stderr("DW_ATE_float: Unsupported byte_size %d\n",(int)type->base_type_info.byte_size); return -EINVAL;
		}
		break;
	default: msg_stderr("Unsupported encoding %d\n",(int)type->base_type_info.encoding); return -EINVAL;
	}

	if (pp_json) {
		*pp_json = json_object_new_string(buf);
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
	default: msg_stderr("byte_size %d isn't supported\n", (int)type->enumeration_type_info.byte_size); return -EINVAL;
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
		int * real_count_array_per_type, /*array with real number of elements_elements for each item in subtypes_sequence*/
		int subtypes_sequence_lenth
		) {
	struct json_visitor * json_visitor = cds_list_entry(p_visitor, struct json_visitor, visitor);
	/*TBD: to*/
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
		metac_byte_size_t byte_size,
		char * name_local,
		char * path_within_region_element
		) {
	struct json_visitor * json_visitor = cds_list_entry(p_visitor, struct json_visitor, visitor);
	return 0;
}
/*****************************************************************************/
int metac_unpack_to_json(
		void *ptr,
		metac_byte_size_t size,
		metac_precompiled_type_t * precompiled_type,
		metac_count_t elements_count,
		json_object ** pp_json) {
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

	if (pp_json) {
		*pp_json = res_json;
	}

	json_visitor_cleanup(&json_visitor);
	return 0;
}


