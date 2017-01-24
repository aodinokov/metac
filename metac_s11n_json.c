/*
 * metac_s11n_json.c
 *
 *  Created on: Dec 15, 2016
 *      Author: mralex
 */

#include <json/json.h>

#include <stdint.h>	/*int8_t - int64_t*/
#include <stdlib.h>	/*calloc/free*/
#include <string.h>	/*strcmp*/
#include <assert.h>	/*assert*/
#include <errno.h>	/*EINVAL &etc*/

#include "metac_type.h"
#include "metac_debug.h"

static int _metac_alloc_and_fill_recursevly(struct metac_type * type, json_object * jobj, void **ptr);
static int _metac_fill_recursevly(struct metac_type * type, json_object * jobj, void *ptr, metac_byte_size_t byte_size);

static int _metac_fill_basic_type(struct metac_type * type, json_object * jobj, void *ptr, metac_byte_size_t byte_size) {
	if (type->p_at.p_at_name == NULL)
		return -EINVAL;
	enum json_type jtype = json_object_get_type(jobj);

	/*TODO: don't like this strcmp. need to search once to find type*/
	if (strcmp(type->p_at.p_at_name->name, "int") == 0){
		assert(byte_size == sizeof(int));
		assert(jtype == json_type_int);
		*((int*)ptr) = (int)json_object_get_int(jobj); /*FIXME: json_object_get_int64???*/
	}else if (strcmp(type->p_at.p_at_name->name, "unsigned int") == 0){
		assert(byte_size == sizeof(unsigned int));
		assert(jtype == json_type_int);
		*((int*)ptr) = (int)json_object_get_int(jobj); /*FIXME: json_object_get_int64???*/
	}else if (strcmp(type->p_at.p_at_name->name, "long long int") == 0){
		assert(byte_size == sizeof(long long int));
		assert(jtype == json_type_int);
		*((long long int*)ptr) = (long long int)json_object_get_int64(jobj);
	}else if (strcmp(type->p_at.p_at_name->name, "char") == 0){
		assert(byte_size == sizeof(char));
		/*json_type_string or TODO: json_type_int in range 0 - 255*/
		assert(jtype == json_type_string);
		/*TODO: make some ifdef to make it work with old versions of json lib*/
		//assert(json_object_get_string_len(jobj) == 1);
		*((char*)ptr) = json_object_get_string(jobj)[0];
	}/*TODO: else ...*/

	return 0;
}

static int _metac_fill_enumeration_type(struct metac_type * type, json_object * jobj, void *ptr, metac_byte_size_t byte_size) {
	int res = -EINVAL;
	metac_num_t i;
	metac_const_value_t	const_value;
	struct metac_type_enumeration_type_info info;
	struct metac_type_enumerator_info einfo;

	if (metac_type_enumeration_type_info(type, &info) != 0) {
		msg_stderr("metac_type_enumeration_type_info returned error\n");
		return -EINVAL;
	}

	enum json_type jtype = json_object_get_type(jobj);
	assert(jtype == json_type_string);
	assert(byte_size == info.byte_size);

	for (i = 0; i < info.enumerators_count; i++) {
		if (metac_type_enumeration_type_enumerator_info(type, i, &einfo) != 0) {
			msg_stderr("metac_type_enumeration_type_enumerator_info error\n");
			return -EINVAL;
		}
		if (strcmp(json_object_get_string(jobj), einfo.name) == 0) {
			res = 0;
			const_value = einfo.const_value;
			break;
		}
	}
	if (res == 0) {
		/*store const_value to ptr*/
		/*TODO: Can we find easier way than this vvv?*/
		switch(byte_size){
		case 1:
			*((int8_t*)ptr) = (int8_t)const_value;
			break;
		case 2:
			*((int16_t*)ptr) = (int16_t)const_value;
			break;
		case 4:
			*((int32_t*)ptr) = (int32_t)const_value;
			break;
		case 8: /*TODO: probably need to make appropriate ifdef */
			*((int64_t*)ptr) = (int64_t)const_value;
			break;
		default:
			msg_stderr("byte_size %d isn't supported\n", (int)byte_size);
			res = -EINVAL;
			break;
		}
	}
	return res;
}

static int _metac_fill_pointer_type(struct metac_type * type, json_object * jobj, void *ptr, metac_byte_size_t byte_size) {
	int res = -EINVAL;
	assert(byte_size == sizeof(void*));

	if (type->p_at.p_at_type == NULL) {
		/*TBD: deserialize like int???*/
		msg_stderr("Can't de-serialize to void*\n");
		return -EINVAL;
	}

	/*use jobj without changes, but allocate memory for new object and store pointer by ptr address*/
	return _metac_alloc_and_fill_recursevly(type->p_at.p_at_type->type, jobj, (void**)ptr);
}

static int _metac_fill_structure_type(struct metac_type * type, json_object * jobj, void *ptr, metac_byte_size_t byte_size) {
	int res = 0;
	metac_num_t i;
	struct metac_type_structure_info info;
	struct metac_type_member_info minfo;
	json_object * mjobj;

	enum json_type jtype = json_object_get_type(jobj);
	assert(jtype == json_type_object);

	if (metac_type_structure_info(type, &info) != 0) {
		msg_stderr("metac_type_structure_info returned error\n");
		return -EINVAL;
	}

	for (i = 0; i < info.members_count; i++) {
		metac_byte_size_t byte_size;
		if (metac_type_structure_member_info(type, i, &minfo) != 0) {
			msg_stderr("metac_type_structure_member_info returned error\n");
			return -EINVAL;
		}

		/* this is a bit slow, but it's fine for now */
		byte_size = metac_type_byte_size(minfo.type);
		if (byte_size == 0) {
			msg_stderr("metac_type_byte_size returned 0 for member %s\n", minfo.name);
			return -EINVAL;
		}

#if JSON_C_MAJOR_VERSION == 0 && JSON_C_MINOR_VERSION < 10
		mjobj = json_object_object_get(jobj, minfo.name);
		if (mjobj == NULL) {
#else
		if (json_object_object_get_ex(jobj, minfo.name, &mjobj) == 0) {
#endif
			/*TODO: may be we must init this objects by default*/
			msg_stderr("Can't find member %s in json\n", minfo.name);
			return -EINVAL;
		}

		if (minfo.p_bit_offset != NULL || minfo.p_bit_size != NULL) {
			/* bit-fields only possible for integral types (that means that minfo type will be base_type of its typedef)
			 * long long heightValidated2 : 64 will not have bit offset and size, but
			 * long long heightValidated2 : 63 will have
			 * the idea is to make array (8 byte max) and use it for _metac_fill_recursevly (instead of pointers)
			 * */
			unsigned char buffer[8] = {0,};
			assert(byte_size <= 8);

			res = _metac_fill_recursevly(minfo.type, mjobj, buffer, byte_size);
			if (res != 0) {
				msg_stderr("_metac_fill_recursevly returned error for member %s\n", minfo.name);
				return res;
			}
			/*
			 * now need to make mask value, shift it and to put to addr:
			 * ptr + (minfo.p_data_member_location?(*minfo.p_data_member_location):0)
			 * the main concern will be with sign (do we need to care about encoding?);
			 */
#define _HANDLE_BITFIELDS(_type_, _mask_) do{ \
	_type_ _i, mask = _mask_; \
	_i = *(_type_*)buffer; \
	_i = (_i & (~(mask << (minfo.p_bit_size?(*minfo.p_bit_size):0)))) << \
			(byte_size*8 - (minfo.p_bit_size?(*minfo.p_bit_size):0) - (minfo.p_bit_offset?(*minfo.p_bit_offset):0)); \
	*((_type_*)(ptr + (minfo.p_data_member_location?(*minfo.p_data_member_location):0))) |= _i; \
}while(0)
			switch(byte_size){
			case 1:
				_HANDLE_BITFIELDS(uint8_t, 0xff);
				break;
			case 2:
				_HANDLE_BITFIELDS(uint16_t, 0xffff);
				break;
			case 4:
				_HANDLE_BITFIELDS(uint32_t, 0xffffffff);
				break;
			case 8:
				_HANDLE_BITFIELDS(uint64_t, 0xffffffffffffffff);
				break;
			default:
				msg_stderr("byte_size %d isn't supported\n", (int)byte_size);
				res = -EINVAL;
				return res;
			}
		}else {
			/*not bit-fields*/
			res = _metac_fill_recursevly(minfo.type, mjobj, ptr + (minfo.p_data_member_location?(*minfo.p_data_member_location):0), byte_size);
			if (res != 0) {
				msg_stderr("_metac_fill_recursevly returned error for member %s\n", minfo.name);
				return res;
			}
		}
	}

	return 0;
}

static int _metac_fill_array_type(struct metac_type * type, json_object * jobj, void *ptr, metac_byte_size_t byte_size) {
	int res = 0;
	metac_num_t i;
	struct metac_type_array_info info;
	struct metac_type_element_info einfo;
	json_object * ejobj;
	metac_byte_size_t ebyte_size;

	enum json_type jtype = json_object_get_type(jobj);
	assert(jtype == json_type_array);

	if (metac_type_array_info(type, &info) != 0) {
		msg_stderr("metac_type_array_info returned error\n");
		return -EINVAL;
	}

	ebyte_size = metac_type_byte_size(info.type);
	if (byte_size == 0) {
		msg_stderr("metac_type_byte_size returned 0 array element type\n");
		return -EINVAL;
	}

	for (i = 0; i < info.elements_count; i++) {
		if (metac_type_array_element_info(type, i, &einfo) != 0) {
			msg_stderr("metac_type_array_element_info returned error\n");
			return -EINVAL;
		}

		ejobj = json_object_array_get_idx(jobj, (int)i);
		if (ejobj == NULL) { /*TODO: may be we must init this objects by default*/
			msg_stderr("Can't find indx %d in json\n", (int)i);
			return -EINVAL;
		}

		res = _metac_fill_recursevly(info.type, ejobj, ptr + einfo.data_location, ebyte_size);
		if (res != 0) {
			msg_stderr("_metac_fill_recursevly returned error for array element %d\n", (int)i);
			return res;
		}

	}
	return 0;
}

static int _metac_fill_recursevly(struct metac_type * type, json_object * jobj, void *ptr, metac_byte_size_t byte_size) {
	metac_type_id_t id;

	//msg_stddbg("type %s jtype %d\n", type->p_at.p_at_name?type->p_at.p_at_name->name:"", (int)json_object_get_type(jobj));
	type = metac_type_typedef_skip(type);
	assert(type);

	id = metac_type_id(type);
	switch (id) {
	case DW_TAG_base_type:
		return _metac_fill_basic_type(type, jobj, ptr, byte_size);
	case DW_TAG_enumeration_type:
		return _metac_fill_enumeration_type(type, jobj, ptr, byte_size);
	case DW_TAG_pointer_type:
		return _metac_fill_pointer_type(type, jobj, ptr, byte_size);
	case DW_TAG_array_type:
		return _metac_fill_array_type(type, jobj, ptr, byte_size);
	case DW_TAG_structure_type:
		return _metac_fill_structure_type(type, jobj, ptr, byte_size);
	case DW_TAG_union_type:
		/*TODO: don't know what to do here */
		assert(0);
		return -1;
	default:
		return -1;
	}
	return 0;
}

static int _metac_alloc_and_fill_recursevly(struct metac_type * type, json_object * jobj, void **ptr) {
	int res = 0;
	metac_byte_size_t byte_size;
	if (type == NULL || ptr == NULL)
		return -EINVAL;

	byte_size = metac_type_byte_size(type);
	if (byte_size == 0) {
		msg_stderr("Can't determine byte_size for type");
		return -EINVAL;
	}

	/* TODO: 27. Flexible Array: _metac_alloc_and_fill_recursevly - modify implementation to check if type has array with zero length and has
	 * siblings with <array_name>_len that sets dynamically length of array*/

	*ptr = (void *)calloc(1, byte_size);
	if ((*ptr) == NULL) {
		msg_stderr("Can't allocate memory");
		return -ENOMEM;
	}

	res = _metac_fill_recursevly(type, jobj, *ptr, byte_size);
	if (res != 0){
		msg_stderr("_metac_fill_recursevly returned error");
		free(*ptr);
		*ptr = NULL;
	}
	return res;
}


static struct metac_object * _metac_json_object_2_metac_object(struct metac_type * mtype, json_object * jobj) {
	struct metac_object * mobj;
	metac_type_id_t id = metac_type_id(mtype);
	if (id != DW_TAG_typedef &&
		id != DW_TAG_base_type &&
		id != DW_TAG_pointer_type &&
		id != DW_TAG_array_type &&
		id != DW_TAG_union_type &&
		id != DW_TAG_structure_type &&
		id != DW_TAG_enumeration_type)
		return NULL;

	mobj = (struct metac_object *)calloc(1, sizeof(struct metac_object));
	if (mobj == NULL)
		return NULL;

	mobj->type = mtype;
	mobj->ref_count = 1;

	if (_metac_alloc_and_fill_recursevly(mtype, jobj, &mobj->ptr) != 0){
		metac_object_put(mobj);	/* free partly allocated data */
		mobj = NULL;
	}

	return mobj;
}

struct metac_object * metac_json2object(struct metac_type * mtype, char *string) {

	json_object * jobj = json_tokener_parse(string);
	if (jobj){
		struct metac_object * mobj = _metac_json_object_2_metac_object(mtype, jobj);
		json_object_put(jobj);
		return mobj;
	}
	return NULL;
}


