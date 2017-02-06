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
#include <inttypes.h>	/* sscanf support for int8_t - int64_t*/

#include "metac_type.h"
#include "metac_debug.h"

static int _metac_alloc_and_fill_recursevly(struct metac_type * type, json_object * jobj, void **ptr);
static int _metac_fill_recursevly(struct metac_type * type, json_object * jobj, void *ptr, metac_byte_size_t byte_size);

static int _metac_fill_basic_type_from_int(struct metac_type * type, json_object * jobj, void *ptr, metac_byte_size_t byte_size) {
	int64_t val;
#if JSON_C_MAJOR_VERSION == 0 && JSON_C_MINOR_VERSION < 10
	val = json_object_get_int(jobj);
#else
	val = json_object_get_int64(jobj);
#endif
	/* Notes:
	 * json_tokener_parse("-99999999999999999999999999999999999999") will not return error.
	 * with the newest libjson the result can be in range [INT64_MIN; INT64_MAX]
	 * with the version < 0.10 the result can be in range [INT32_MIN; INT32_MAX]
	 * definitely both cases will truncate uint64_t (+uint32_t, int64_t in the second case).
	 * For that purpose we have _metac_fill_basic_type_from_string that allows to use longer values.
	 */
	switch(type->p_at.p_at_encoding->encoding) {
	case DW_ATE_unsigned_char:
	case DW_ATE_signed_char:
		switch(type->p_at.p_at_byte_size->byte_size) {
		case sizeof(int8_t):
			*((int8_t*)ptr) = val;
			break;
		default:
			msg_stderr("Unsupported byte_size %d\n",(int)type->p_at.p_at_byte_size->byte_size);
			return -EFAULT;
		}
		break;
	case DW_ATE_unsigned:
	case DW_ATE_signed:
		if (val < 0 &&
			type->p_at.p_at_encoding->encoding == DW_ATE_unsigned) {
			msg_stderr("Negative value %ld for unsigned type\n", val);
			return -EFAULT;
		}
		switch(type->p_at.p_at_byte_size->byte_size) {
		case sizeof(int8_t):
			*((int8_t*)ptr) = val;
			break;
		case sizeof(int16_t):
			*((int16_t*)ptr) = val;
			break;
		case sizeof(int32_t):
			*((int32_t*)ptr) = val;
			break;
		case sizeof(int64_t):
			*((int64_t*)ptr) = val;
			break;
		default:
			msg_stderr("Unsupported byte_size %d\n",(int)type->p_at.p_at_byte_size->byte_size);
			return -EFAULT;
		}
		break;
	case DW_ATE_float:
		switch(type->p_at.p_at_byte_size->byte_size) {
		case sizeof(float):
			*((float*)ptr) = val;
			break;
		case sizeof(double):
			*((double*)ptr) = val;
			break;
		case sizeof(long double):
			*((long double*)ptr) = val;
			break;
		default:
			msg_stderr("Unsupported byte_size %d\n",(int)type->p_at.p_at_byte_size->byte_size);
			return -EFAULT;
		}
		break;
	default:
		msg_stderr("Unsupported encoding %d\n",(int)type->p_at.p_at_encoding->encoding);
		return -EFAULT;
	}
	return 0;
}

static int _metac_fill_basic_type_from_double(struct metac_type * type, json_object * jobj, void *ptr, metac_byte_size_t byte_size) {
	double val = json_object_get_double(jobj);

	switch(type->p_at.p_at_encoding->encoding) {
	case DW_ATE_float:
		switch(type->p_at.p_at_byte_size->byte_size) {
		case sizeof(float):
			*((float*)ptr) = val;
			break;
		case sizeof(double):
			*((double*)ptr) = val;
			break;
		case sizeof(long double):
			*((long double*)ptr) = val;
			break;
		default:
			msg_stderr("Unsupported byte_size %d\n",(int)type->p_at.p_at_byte_size->byte_size);
			return -EFAULT;
		}
		break;
	default:
		msg_stderr("Unsupported encoding %d\n",(int)type->p_at.p_at_encoding->encoding);
		return -EFAULT;
	}
	return 0;
}

static int _metac_fill_basic_type_from_string(struct metac_type * type, json_object * jobj, void *ptr, metac_byte_size_t byte_size) {
	const char * string = json_object_get_string(jobj);
	int string_len;
#if JSON_C_MAJOR_VERSION == 0 && JSON_C_MINOR_VERSION < 10
	string_len = (string!=NULL)?strlen(string):0;
#else
	string_len = json_object_get_string_len(jobj);
#endif

	switch(type->p_at.p_at_encoding->encoding) {
	case DW_ATE_unsigned_char:
	case DW_ATE_signed_char:
		switch(type->p_at.p_at_byte_size->byte_size) {
		case sizeof(int8_t):
			if (string_len != 1) {
				msg_stderr("expected string_len == 1 instead of %d in json\n", (int)string_len);
				return -EFAULT;
			}
			*((char*)ptr) = string[0];
			return 0;
		default:
			msg_stderr("Unsupported byte_size %d\n",(int)type->p_at.p_at_byte_size->byte_size);
			return -EFAULT;
		}
		break;
	case DW_ATE_unsigned:
	case DW_ATE_signed: {
			/* possible to extend this and if we meet 0x or 0 switch to hex and octal.
			 * also don't forget that we can meet signs [-/+].
			 * in that case we can always read as unsigned and multiply if necessary -1 (only if encoding is unsigned)
			 */
			char suffix;
			const char * _string = string;
			int _string_len = string_len;
			char * format = "%"SCNu64"%c";
			uint64_t uval;
			int64_t val = 1;

			if (_string_len > 0 &&
				(_string[0] == '+' ||
				_string[0] == '-')) {
				val = (_string[0] == '-')?-1:1;
				++_string;
				--_string_len;
			}

			if (val < 0 &&
				type->p_at.p_at_encoding->encoding == DW_ATE_unsigned) {
				msg_stderr("Invalid string %s for unsigned type\n", string);
				return -EFAULT;
			}

			if (_string_len > 0 &&
				_string[0] == '0') { /*octal*/
				format = "%"SCNo64"%c";
				++_string;
				--_string_len;
			}

			if (_string_len > 0 &&
				_string[0] == 'x' &&
				strcmp(format, "%"SCNo64"%c") == 0) { /*hex*/
				format = "%"SCNx64"%c";
				++_string;
				--_string_len;
			}

			if (_string_len == 0) { /*if we read the prefix, but we don't have anything after - reset*/
				val = 1;
				format = "%"SCNu64"%c";
				_string = string;
				_string_len = string_len;
			}

			/* we don't know if we read the whole string or only part that matched. For that purpose we have suffix var.
			 * if sscanf returns 2 - we have something in remaining */
			if (sscanf(_string, format, &uval, &suffix) != 1) {
				msg_stderr("Wasn't able to read string \"%s\"to data\n", string);
				return -EFAULT;
			}

			if (type->p_at.p_at_encoding->encoding == DW_ATE_signed) {
				val*= uval;
			}

			switch(type->p_at.p_at_byte_size->byte_size) {
			case sizeof(int8_t):
				if (string_len != 1) {
					msg_stderr("expected string_len == 1 instead of %d in json\n", (int)string_len);
					return -EFAULT;
				}
				*((char*)ptr) = string[0];
				return 0;
			case sizeof(int16_t):
				if (type->p_at.p_at_encoding->encoding == DW_ATE_signed) {
					*((int16_t*)ptr) = val;
				}else{
					*((uint16_t*)ptr) = uval;
				}
				break;
			case sizeof(int32_t):
				if (type->p_at.p_at_encoding->encoding == DW_ATE_signed) {
					*((int32_t*)ptr) = val;
				}else{
					*((uint32_t*)ptr) = uval;
				}
				break;
			case sizeof(int64_t):
				if (type->p_at.p_at_encoding->encoding == DW_ATE_signed) {
					*((int64_t*)ptr) = val;
				}else{
					*((uint64_t*)ptr) = uval;
				}
				break;
			default:
				msg_stderr("Unsupported byte_size %d\n",(int)type->p_at.p_at_byte_size->byte_size);
				return -EFAULT;
			}
		}
		break;
	case DW_ATE_float: {
			long double val;

			if (sscanf(string, "%Lf", &val) != 1) {
				msg_stderr("Wasn't able to read string \"%s\"to data\n", string);
				return -EFAULT;
			}

			switch(type->p_at.p_at_byte_size->byte_size) {
			case sizeof(float):
				*((float*)ptr) = val;
				break;
			case sizeof(double):
				*((double*)ptr) = val;
				break;
			case sizeof(long double):
				*((long double*)ptr) = val;
				break;
			default:
				msg_stderr("Unsupported byte_size %d\n",(int)type->p_at.p_at_byte_size->byte_size);
				return -EFAULT;
			}
		}
		break;
	default:
		msg_stderr("Unsupported encoding %d\n",(int)type->p_at.p_at_encoding->encoding);
		return -EFAULT;
	}

	return 0;
}

static int _metac_fill_basic_type(struct metac_type * type, json_object * jobj, void *ptr, metac_byte_size_t byte_size) {
	if (type->p_at.p_at_name == NULL ||
		type->p_at.p_at_encoding == NULL ||
		type->p_at.p_at_byte_size == NULL)
		return -EINVAL;

	if ((metac_byte_size_t)type->p_at.p_at_byte_size->byte_size != byte_size) {
		msg_stderr("expected byte_size %d instead of %d to store %s\n",
				(int)type->p_at.p_at_byte_size->byte_size, (int)byte_size, type->p_at.p_at_name->name);
		return -EFAULT;
	}

	switch (json_object_get_type(jobj)) {
		case json_type_int:
			return _metac_fill_basic_type_from_int(type, jobj, ptr, byte_size);
		case json_type_double:
			return _metac_fill_basic_type_from_double(type, jobj, ptr, byte_size);
		case json_type_string:
			return _metac_fill_basic_type_from_string(type, jobj, ptr, byte_size);
		default:
			msg_stderr("Can't convert from json_type_%d to basic type\n",(int)json_object_get_type(jobj));
			return -EFAULT;
	}
	return 0;
}

static int _metac_fill_enumeration_type(struct metac_type * type, json_object * jobj, void *ptr, metac_byte_size_t byte_size) {
	int res = -EINVAL;
	metac_num_t i;
	metac_const_value_t	const_value;
	enum json_type jtype;
	struct metac_type_enumeration_type_info info;
	struct metac_type_enumerator_info einfo;

	if (metac_type_enumeration_type_info(type, &info) != 0) {
		msg_stderr("metac_type_enumeration_type_info returned error\n");
		return -EINVAL;
	}

	if (byte_size != info.byte_size) {
		msg_stderr("expected byte_size %d instead of %d to store enum\n", (int)info.byte_size, (int)byte_size);
		return -EFAULT;
	}

	jtype = json_object_get_type(jobj);
	if (jtype != json_type_string) {
		msg_stderr("expected string in json\n");
		return -EFAULT;
	}

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
			 * FIXME: not sure if this will work for BIG ENDIAN
			 */
#define _HANDLE_BITFIELDS(_type_, _mask_) do{ \
	metac_bit_offset_t lshift = (byte_size << 3) - \
			((minfo.p_bit_size?(*minfo.p_bit_size):0) + (minfo.p_bit_offset?(*minfo.p_bit_offset):0)); \
	_type_ mask = ~(((_type_)_mask_) << (minfo.p_bit_size?(*minfo.p_bit_size):0)); \
	/*msg_stddbg("lshift = %x, mask = %x\n", (int)lshift, (int)mask);*/ \
	_type_ _i = *(_type_*)buffer; \
	_i = (_i & mask) << lshift;\
	*((_type_*)(ptr + (minfo.p_data_member_location?(*minfo.p_data_member_location):0))) ^= _i; \
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
		msg_stderr("Can't determine byte_size for type\n");
		return -EINVAL;
	}

	/* TODO: 27. Flexible Array: _metac_alloc_and_fill_recursevly - modify implementation to check if type has array with zero length and has
	 * siblings with <array_name>_len that sets dynamically length of array*/

	*ptr = (void *)calloc(1, byte_size);
	if ((*ptr) == NULL) {
		msg_stderr("Can't allocate memory\n");
		return -ENOMEM;
	}

	res = _metac_fill_recursevly(type, jobj, *ptr, byte_size);
	if (res != 0){
		msg_stderr("_metac_fill_recursevly returned error\n");
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


