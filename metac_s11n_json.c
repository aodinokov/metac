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

typedef struct _parent_struct_context {
	struct metac_type * type;
	json_object * jobj;
	void *ptr;
	metac_byte_size_t byte_size;
	char *field_name;
	json_object *field_jobj;
}parent_struct_context_t;

typedef struct _flex_array_context {
	void *flexarr_ptr;
	metac_byte_size_t flexarr_byte_size;
}flex_array_context_t;

static int _metac_alloc_and_fill_recursevly(struct metac_type * type, json_object * jobj, void **ptr);
static int _metac_alloc_and_fill_recursevly_array(struct metac_type * type, json_object * jobj, void **ptr);
static int _metac_fill_recursevly(struct metac_type * type, json_object * jobj, void *ptr, metac_byte_size_t byte_size,
		parent_struct_context_t *parentstr_cnxt,
		flex_array_context_t *flexarr_cnxt);

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
	double val = (long double)json_object_get_double(jobj);

	switch(type->p_at.p_at_encoding->encoding) {
	case DW_ATE_float:
		switch(type->p_at.p_at_byte_size->byte_size) {
		case sizeof(float):
			*((float*)ptr) = (float)val;
			break;
		case sizeof(double):
			*((double*)ptr) = (double)val;
			break;
		case sizeof(long double):
			*((long double*)ptr) = (long double)val;
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
			char suffix;
			long double val;

			if (sscanf(string, "%Lf%c", &val, &suffix) != 1) {
				msg_stderr("Wasn't able to read string \"%s\"to data\n", string);
				return -EFAULT;
			}

			switch(type->p_at.p_at_byte_size->byte_size) {
			case sizeof(float):
				*((float*)ptr) = (float)val;
				break;
			case sizeof(double):
				*((double*)ptr) = (double)val;
				break;
			case sizeof(long double):
				*((long double*)ptr) = (long double)val;
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
	metac_const_value_t const_value;
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
	switch(jtype) {
	case json_type_int: {
			int64_t val;
#if JSON_C_MAJOR_VERSION == 0 && JSON_C_MINOR_VERSION < 10
			val = json_object_get_int(jobj);
#else
			val = json_object_get_int64(jobj);
#endif
			for (i = 0; i < info.enumerators_count; i++) {
				if (metac_type_enumeration_type_enumerator_info(type, i, &einfo) != 0) {
					msg_stderr("metac_type_enumeration_type_enumerator_info error\n");
					return -EFAULT;
				}
				if (val == einfo.const_value) {
					res = 0;
					const_value = einfo.const_value;
					break;
				}
			}
			if (res != 0) {
				msg_stderr("wasn't able to find enumerator for %ld\n", val);
				return -EFAULT;
			}
		}
		break;
	case json_type_string: {
			const char * string = json_object_get_string(jobj);
			for (i = 0; i < info.enumerators_count; i++) {
				if (metac_type_enumeration_type_enumerator_info(type, i, &einfo) != 0) {
					msg_stderr("metac_type_enumeration_type_enumerator_info error\n");
					return -EFAULT;
				}
				if (strcmp(string, einfo.name) == 0) {
					res = 0;
					const_value = einfo.const_value;
					break;
				}
			}
			if (res != 0) {
				msg_stderr("wasn't able to find value for %s\n", string);
				return -EFAULT;
			}
		}
		break;
	default:
		msg_stderr("Can't convert from json_type_%d to basic type\n",(int)jtype);
		return -EFAULT;
	}

	switch(byte_size){
	case sizeof(int8_t):
		*((int8_t*)ptr) = (int8_t)const_value;
		break;
	case sizeof(int16_t):
		*((int16_t*)ptr) = (int16_t)const_value;
		break;
	case sizeof(int32_t):
		*((int32_t*)ptr) = (int32_t)const_value;
		break;
	case sizeof(int64_t):
		*((int64_t*)ptr) = (int64_t)const_value;
		break;
	default:
		msg_stderr("byte_size %d isn't supported\n", (int)byte_size);
		return -EFAULT;
	}

	return 0;
}

static int _metac_fill_pointer_type(struct metac_type * type, json_object * jobj, void *ptr, metac_byte_size_t byte_size) {
	int res = -EINVAL;
	struct metac_type * mtype;

	if (byte_size != sizeof(void*)) {
		msg_stderr("expected byte_size %d instead of %d to store ptr\n", (int)sizeof(void*), (int)byte_size);
		return -EFAULT;
	}

	if (type->p_at.p_at_type == NULL) {
		msg_stderr("Can't de-serialize to void*\n");
		return -EINVAL;
	}

	mtype = type->p_at.p_at_type->type;
	if (	json_object_get_type(jobj) == json_type_string &&
			metac_type_id(metac_type_typedef_skip(mtype)) == DW_TAG_base_type &&
			mtype->p_at.p_at_byte_size->bit_size == sizeof(uint8_t) &&
			(mtype->p_at.p_at_encoding->encoding == DW_ATE_unsigned_char ||
			mtype->p_at.p_at_encoding->encoding == DW_ATE_signed_char)) {
		/* handle json_type_string if type is ptr to char or uchar*/
		char * string = strdup(json_object_get_string(jobj));
		if (string == NULL) {
			msg_stderr("Can't allocate mem for string\n");
			return -ENOMEM;
		}
		*((char**)ptr) = string;
		return 0;
	}
	if (	json_object_get_type(jobj) == json_type_array) {
		return _metac_alloc_and_fill_recursevly_array(mtype, jobj, (void**)ptr);
	}

	/*use jobj without changes, but allocate memory for new object and store pointer by ptr address*/
	/*TBD: if we want to limit the recursion - instead of calling this - put here adding to queue*/
	return _metac_alloc_and_fill_recursevly(mtype, jobj, (void**)ptr);
}

/*todo: move it to metac_type.c?*/
static int _metac_find_structure_member_recurcevly(struct metac_type *type, metac_name_t name,
		struct metac_type_member_info *p_info,
		metac_data_member_location_t *	p_data_member_location) {
	assert(type && name && p_info && p_data_member_location);

	int i = metac_type_child_id_by_name(type, name);
	if (i >= 0) {
		switch(metac_type_id(type)){
		case DW_TAG_structure_type:
			if (metac_type_structure_member_info(type, i, p_info) != 0) {
				msg_stderr("metac_type_structure_member_info returned error\n");
				return -EINVAL;
			}
			*p_data_member_location = (p_info->p_data_member_location?(*p_info->p_data_member_location):0);
			return 0;
		case DW_TAG_union_type:
			if (metac_type_union_member_info(type, i, p_info) != 0) {
				msg_stderr("metac_type_structure_member_info returned error\n");
				return -EINVAL;
			}
			*p_data_member_location = (p_info->p_data_member_location?(*p_info->p_data_member_location):0);
			return 0;
		default:
			msg_stderr("found some strange type\n");
			return -EINVAL;
		}
	}

	type = metac_type_typedef_skip(type);
	for (i = 0; i < type->child_num; i++) {
		struct metac_type_member_info minfo;
		if (metac_type_id(type) == DW_TAG_structure_type?
				metac_type_structure_member_info(type, i, &minfo) != 0:
				metac_type_union_member_info(type, i, &minfo) != 0) {
			continue;
		}
		if (	minfo.name == NULL && (
				metac_type_id(metac_type_typedef_skip(minfo.type)) == DW_TAG_union_type ||
				metac_type_id(metac_type_typedef_skip(minfo.type)) == DW_TAG_structure_type)) {
			if (_metac_find_structure_member_recurcevly(minfo.type, name, p_info, p_data_member_location) == 0) {
				*p_data_member_location += (minfo.p_data_member_location?(*minfo.p_data_member_location):0);
				return 0;
			}
		}
	}
	msg_stderr("can't find field %s\n", name);
	return -EINVAL;
}

#define _STRUCTURE_HANDLE_BITFIELDS(_type_, _mask_) do{ \
	metac_bit_offset_t lshift = (byte_size << 3) - \
			((minfo.p_bit_size?(*minfo.p_bit_size):0) + (minfo.p_bit_offset?(*minfo.p_bit_offset):0)); \
	_type_ mask = ~(((_type_)_mask_) << (minfo.p_bit_size?(*minfo.p_bit_size):0)); \
	_type_ _i = *(_type_*)buffer; \
	_i = (_i & mask) << lshift;\
	*((_type_*)(ptr + data_member_location)) ^= _i; \
}while(0)

static int _metac_fill_structure_type(struct metac_type * type, json_object * jobj, void *ptr, metac_byte_size_t byte_size,
		flex_array_context_t *flexarr_cnxt) {
	int res;
	parent_struct_context_t cnxt;
	struct lh_table* table;
	struct lh_entry* entry;
	struct metac_type_member_info minfo;
	if (json_object_get_type(jobj) != json_type_object) {
		msg_stderr("expected json object\n");
		return -EINVAL;
	}

	table = json_object_get_object(jobj);
	if (table == NULL) {
		msg_stderr("json_object_get_object returned NULL\n");
		return -EFAULT;
	}

	/*fill in parent struct call context - to get info about siblings -required only for flex arrays and for unions */
	cnxt.byte_size = byte_size;
	cnxt.ptr = ptr;
	cnxt.type = type;
	cnxt.jobj = jobj;

	lh_foreach(table, entry) {
		metac_data_member_location_t data_member_location = 0;
		metac_byte_size_t byte_size;
		char *key = (char *)entry->k;	/*field name*/
		json_object *mjobj = (json_object *)entry->v;	/*field value*/

		cnxt.field_name = key;
		cnxt.field_jobj = mjobj;

		/*try to find field with name in structure*/
		if (_metac_find_structure_member_recurcevly(type, key, &minfo, &data_member_location) != 0) {
			msg_stderr("_metac_find_structure_member_recurcevly returned error\n");
			return -EINVAL;
		}

		/* this is a bit slow, but it's fine for now */
		byte_size = metac_type_byte_size(minfo.type);
		if (	byte_size == 0 ) {
			if (metac_type_id(metac_type_typedef_skip(minfo.type)) == DW_TAG_array_type /*&&
				child_id == metac_type_child_num(type) - 1*/ /*TBD: better to check member offset - it must be the last:
				cnxt.byte_size == *minfo.p_data_member_location???*/) {
				msg_stddbg("looks like %s is flexible array (byte_size == 0 is acceptable for that case)\n", minfo.name);
				/*TBD: possible to continue and fill all elements first and fill this element as the last element.
				 * but since for unions it will not work, it's better to make a simplified function for parent_struct_context_t
				 * to get int value of any field by name. -1 will be returned in case of any error*/
			}else {
				msg_stderr("metac_type_byte_size returned 0 for member %s\n", minfo.name);
				return -EFAULT;
			}
		}

		if (minfo.p_bit_offset != NULL || minfo.p_bit_size != NULL) {
			/* bit-fields only possible for integral types (that means that minfo type will be base_type of its typedef)
			 * long long heightValidated2 : 64 will not have bit offset and size, but
			 * long long heightValidated2 : 63 will have
			 * the idea is to make array (8 byte max) and use it for _metac_fill_recursevly (instead of pointers)
			 * */
			unsigned char buffer[8] = {0,};
			assert(byte_size <= 8);
			assert(metac_type_id(metac_type_typedef_skip(minfo.type)) == DW_TAG_base_type);

			res = _metac_fill_recursevly(minfo.type, mjobj, buffer, byte_size, &cnxt, flexarr_cnxt);
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
			switch(byte_size){
			case 1:
				_STRUCTURE_HANDLE_BITFIELDS(uint8_t, 0xff);
				break;
			case 2:
				_STRUCTURE_HANDLE_BITFIELDS(uint16_t, 0xffff);
				break;
			case 4:
				_STRUCTURE_HANDLE_BITFIELDS(uint32_t, 0xffffffff);
				break;
			case 8:
				_STRUCTURE_HANDLE_BITFIELDS(uint64_t, 0xffffffffffffffff);
				break;
			default:
				msg_stderr("byte_size %d isn't supported\n", (int)byte_size);
				res = -EINVAL;
				return res;
			}
		}else {
			/*not bit-fields*/
			res = _metac_fill_recursevly(minfo.type, mjobj, ptr + data_member_location, byte_size,
					&cnxt, flexarr_cnxt);
			if (res != 0) {
				msg_stderr("_metac_fill_recursevly returned error for member %s\n", minfo.name);
				return res;
			}
		}
	}
	return 0;
}
/*
 * get's or initializes (if not specified in json) number of array elements if it's stored in a sibling variable
 * field_postfix - what must be added to the array variable name to get sibling name with len
 * returns number of elements for flexible array
 */
static long _handle_array_type_len_sibling(parent_struct_context_t * cnxt, char * field_postfix) {
	int res;
	int child_id;
	metac_byte_size_t	byte_size;
	struct metac_type_member_info minfo;
	char * field_name;
	size_t cnxt_field_name_len;
	int init_len_by_json = 0;
	long buf = 0;

	if (cnxt == NULL || field_postfix == NULL) {
		msg_stderr("invalid argument\n");
		return -EINVAL;
	}

	cnxt_field_name_len = strlen(cnxt->field_name);
	field_name = calloc(1, cnxt_field_name_len + strlen(field_postfix)+1);
	if (field_name == NULL){
		msg_stderr("can't allocate mem for field_name\n");
		return -EINVAL;
	}
	strcpy(field_name, cnxt->field_name);
	strcpy(field_name + cnxt_field_name_len, field_postfix);

	/*try to find field with name in structure*/
	child_id = metac_type_child_id_by_name(cnxt->type, field_name);
	if (child_id < 0) {
		msg_stderr("Can't find member %s in structure\n", field_name);
		free(field_name);
		return -EFAULT;
	}
#if JSON_C_MAJOR_VERSION == 0 && JSON_C_MINOR_VERSION < 10
	if (!json_object_object_get(cnxt->jobj, field_name)) {
#else
	if (!json_object_object_get_ex(cnxt->jobj, field_name, NULL)) {
#endif
		msg_stddbg("Field %s isn't initialized in json\n", field_name);
		init_len_by_json = 1;
	}
	free(field_name);

	if (metac_type_structure_member_info(cnxt->type, child_id, &minfo) != 0) {
		msg_stderr("metac_type_structure_member_info returned error\n");
		return -EINVAL;
	}
	byte_size = metac_type_byte_size(minfo.type);

	assert(byte_size <= sizeof(long));
	assert(metac_type_id(metac_type_typedef_skip(minfo.type)) == DW_TAG_base_type);

	if (init_len_by_json == 0) {
		res = _metac_fill_recursevly(minfo.type, cnxt->jobj, &buf, byte_size, NULL, NULL);
		if (res != 0) {
			msg_stderr("_metac_fill_recursevly returned error for member %s\n", minfo.name);
			return res;
		}

		return buf;
	} else {
		int val = json_object_array_length(cnxt->field_jobj);
		void *ptr = cnxt->ptr + (minfo.p_data_member_location?(*minfo.p_data_member_location):0);
		assert(minfo.type->p_at.p_at_encoding->encoding == DW_ATE_unsigned ||
			minfo.type->p_at.p_at_encoding->encoding ==DW_ATE_signed);
		/*this should be put into macro of function*/
		switch(byte_size){
		case sizeof(int8_t):
			*((int8_t*)ptr) = (int8_t)val;
			break;
		case sizeof(int16_t):
			*((int16_t*)ptr) = (int16_t)val;
			break;
		case sizeof(int32_t):
			*((int32_t*)ptr) = (int32_t)val;
			break;
		case sizeof(int64_t):
			*((int64_t*)ptr) = (int64_t)val;
			break;
		default:
			msg_stderr("byte_size %d isn't supported\n", (int)byte_size);
			return -EFAULT;
		}
		return val;
	}
}

static int _metac_fill_array_type(struct metac_type * type, json_object * jobj, void *ptr, metac_byte_size_t byte_size,
		parent_struct_context_t *parentstr_cnxt,
		flex_array_context_t *flexarr_cnxt) {
	void *_ptr = ptr;
	int res = 0;
	int skip_len_check = 0;
	int i;
	struct metac_type_array_info info;
	struct metac_type_element_info einfo;
	json_object * ejobj;
	metac_byte_size_t ebyte_size;

	if (json_object_get_type(jobj) != json_type_array) {
		msg_stderr("expeted json array\n");
		return -EINVAL;
	}

	if (metac_type_array_info(type, &info) != 0) {
		msg_stderr("metac_type_array_info returned error\n");
		return -EINVAL;
	}

	ebyte_size = metac_type_byte_size(info.type);
	if (ebyte_size == 0) {
		msg_stderr("metac_type_byte_size returned 0 as array element type\n");
		return -EINVAL;
	}

	/* check if the array is flexible. also check if they are last in the structs */
	msg_stderr("subrange count %d elements_count %d\n", (int)info.subranges_count, (int)info.elements_count);
	if (info.subranges_count == 1) {
		struct metac_type_subrange_info sinfo;
		if (metac_type_array_subrange_info(type, 0, &sinfo) != 0) {
			msg_stderr("metac_type_array_subrange_info returned error\n");
			return -EINVAL;
		}

		if (sinfo.p_lower_bound == NULL &&
			sinfo.p_upper_bound == NULL) {
			long val;
			void * flex_ptr;
			int flex_len;
			msg_stddbg("flexible array is detected\n");
			assert(byte_size == 0);
			skip_len_check = 1;

			if (flexarr_cnxt == NULL) {
				msg_stderr("found flexible array, but flexarr_cnxt is NULL\n");
				return -EINVAL;
			}

			/*check sibling with name == <parentstr_cnxt->field_name>_len and get it's value;*/
			val = _handle_array_type_len_sibling(parentstr_cnxt,"_len"); /*TODO: _len must be configurable */
			if (val >= 0) {
				flex_len = val;
			}else {	/*pattern with zeroed element at the end */
				flex_len = json_object_array_length(jobj) + 1; /*last element must be initialized with zeros*/
			}

			if (json_object_array_length(jobj) > flex_len) {
				msg_stderr("array is too big\n");
				return -EINVAL;
			}

			flex_ptr = calloc(flex_len, ebyte_size);
			if (flex_ptr == NULL) {
				msg_stderr("can't allocate mem for flexible array\n");
				return -ENOMEM;
			}
			flexarr_cnxt->flexarr_byte_size = flex_len * ebyte_size;
			flexarr_cnxt->flexarr_ptr = flex_ptr;

			_ptr = flex_ptr;
		}
	}

	if (skip_len_check == 0 &&
		json_object_array_length(jobj) > info.elements_count) {
		msg_stderr("array is too big\n");
		return -EINVAL;
	}

	for (i = 0; i < json_object_array_length(jobj); i++) {
		if (metac_type_array_element_info(type, i, &einfo) != 0) {
			msg_stderr("metac_type_array_element_info returned error\n");
			return -EINVAL;
		}

		ejobj = json_object_array_get_idx(jobj, i);
		if (ejobj == NULL) {
			msg_stderr("Can't find indx %d in json\n", (int)i);
			return -EINVAL;
		}

		res = _metac_fill_recursevly(info.type, ejobj, _ptr + einfo.data_location, ebyte_size, NULL, NULL);
		if (res != 0) {
			msg_stderr("_metac_fill_recursevly returned error for array element %d\n", (int)i);
			return res;
		}
	}
	return 0;
}

static int _metac_fill_union_type(struct metac_type * type, json_object * jobj, void *ptr, metac_byte_size_t byte_size,
		parent_struct_context_t *parentstr_cnxt,
		flex_array_context_t *flexarr_cnxt) {
	int res;
	struct lh_table* table;
	struct lh_entry* entry;
	struct metac_type_member_info minfo;

	assert(metac_type_id(type) == DW_TAG_union_type);

	if (json_object_get_type(jobj) != json_type_object) {
		msg_stderr("expected json object\n");
		return -EINVAL;
	}

	table = json_object_get_object(jobj);
	if (table == NULL) {
		msg_stderr("json_object_get_object returned NULL\n");
		return -EFAULT;
	}

#if JSON_C_MAJOR_VERSION == 0 && JSON_C_MINOR_VERSION < 10
	if (table->head != NULL && table->head->next != NULL) {
#else
	if (json_object_object_length(jobj) > 1) {
#endif
		/*TODO: in fact gcc and clang allows this with disabled -Werr. in future this should be configured as setting */
		msg_stderr("Warning: only 1 field can be used in union at once\n");
		return -EINVAL;
	}

	//val = _handle_union_type_descriminator_sibling(parentstr_cnxt, "_descriminator"); /*TODO: _descriminator must be configurable */


	lh_foreach(table, entry) {
		int child_id;
		metac_byte_size_t byte_size;
		char *key = (char *)entry->k;	/*field name*/
		json_object *mjobj = (json_object *)entry->v;	/*field value*/

		/*try to find field with name in union*/
		child_id = metac_type_child_id_by_name(type, key);
		if (child_id < 0) {
			msg_stderr("Can't find member %s in union\n", key);
			return -EFAULT;
		}

		if (metac_type_union_member_info(type, child_id, &minfo) != 0) {
			msg_stderr("metac_type_union_member_info returned error\n");
			return -EINVAL;
		}

		/* this is a bit slow, but it's fine for now */
		byte_size = metac_type_byte_size(minfo.type);
		if (	byte_size == 0 ) {
			if (metac_type_id(metac_type_typedef_skip(minfo.type)) == DW_TAG_array_type &&
				child_id == metac_type_child_num(type) - 1 /*TBD: better to check member offset - it must be the last:
				cnxt.byte_size == *minfo.p_data_member_location???*/) {
				msg_stddbg("looks like %s is flexible array (byte_size == 0 is acceptable for that case)\n", minfo.name);
				/*TBD: possible to continue and fill all elements first and fill this element as the last element.
				 * but since for unions it will not work, it's better to make a simplified function for parent_struct_context_t
				 * to get int value of any field by name. -1 will be returned in case of any error*/
			}else {
				msg_stderr("metac_type_byte_size returned 0 for member %s\n", minfo.name);
				return -EFAULT;
			}
		}

		/*not bit-fields*/
		res = _metac_fill_recursevly(minfo.type, mjobj, ptr + (minfo.p_data_member_location?(*minfo.p_data_member_location):0), byte_size,
				NULL, flexarr_cnxt);
		if (res != 0) {
			msg_stderr("_metac_fill_recursevly returned error for member %s\n", minfo.name);
			return res;
		}
	}
	return 0;
}

static int _metac_fill_recursevly(struct metac_type * type, json_object * jobj, void *ptr, metac_byte_size_t byte_size,
		parent_struct_context_t *parentstr_cnxt,
		flex_array_context_t *flexarr_cnxt) {
	metac_type_id_t id;

	msg_stddbg("type %s jtype %d\n", type->p_at.p_at_name?type->p_at.p_at_name->name:"", (int)json_object_get_type(jobj));
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
		return _metac_fill_array_type(type, jobj, ptr, byte_size, parentstr_cnxt, flexarr_cnxt);
	case DW_TAG_structure_type:
		return _metac_fill_structure_type(type, jobj, ptr, byte_size, flexarr_cnxt);
	case DW_TAG_union_type:
		return _metac_fill_union_type(type, jobj, ptr, byte_size, parentstr_cnxt, flexarr_cnxt);
	default:
		return -1;
	}
	return 0;
}

static int _metac_alloc_and_fill_recursevly_array(struct metac_type * type, json_object * jobj, void **ptr) {
	int i;
	int res = 0;
	metac_byte_size_t byte_size;

	if (type == NULL || ptr == NULL)
		return -EINVAL;

	if (json_object_get_type(jobj) != json_type_array) {
		msg_stderr("expeted json array\n");
		return -EINVAL;
	}

	byte_size = metac_type_byte_size(type);
	if (byte_size == 0) {
		msg_stderr("Can't determine byte_size for type\n");
		return -EINVAL;
	}

	*ptr = (void *)calloc(json_object_array_length(jobj), byte_size);
	if ((*ptr) == NULL) {
		msg_stderr("Can't allocate memory\n");
		return -ENOMEM;
	}

	for (i = 0; i < json_object_array_length(jobj); i++) {
		res = _metac_fill_recursevly(type, json_object_array_get_idx(jobj, i), (*ptr) + i*byte_size, byte_size, NULL, NULL);
		if (res != 0) {
			msg_stderr("_metac_fill_recursevly returned error in _metac_alloc_and_fill_recursevly_array\n");
			/*fixme: need to de-init [0; i-1]*/
			free(*ptr);
			*ptr = NULL;
			break;
		}
	}

	return res;
}

static int _metac_alloc_and_fill_recursevly(struct metac_type * type, json_object * jobj, void **ptr) {
	int res = 0;
	metac_byte_size_t byte_size;
	flex_array_context_t cnxt = {.flexarr_ptr = NULL, .flexarr_byte_size = 0};

	if (type == NULL || ptr == NULL)
		return -EINVAL;

	byte_size = metac_type_byte_size(type);
	if (byte_size == 0) {
		msg_stderr("Can't determine byte_size for type\n");
		return -EINVAL;
	}

	*ptr = (void *)calloc(1, byte_size);
	if ((*ptr) == NULL) {
		msg_stderr("Can't allocate memory\n");
		return -ENOMEM;
	}

	res = _metac_fill_recursevly(type, jobj, *ptr, byte_size, NULL, &cnxt);
	if (res != 0){
		msg_stderr("_metac_fill_recursevly returned error\n");
		if (cnxt.flexarr_ptr != NULL) {
			free(cnxt.flexarr_ptr);
			cnxt.flexarr_ptr = NULL;
			cnxt.flexarr_byte_size = 0;
		}
		free(*ptr);
		*ptr = NULL;
	}

	if (cnxt.flexarr_ptr != NULL && cnxt.flexarr_byte_size > 0){
		void *_ptr;
		_ptr = realloc(*ptr, byte_size + cnxt.flexarr_byte_size);
		if (_ptr == NULL) {
			/*FIXME: need to free all data recursevly - current implementation has mem leaks if realloc is failed*/
			msg_stderr("_metac_fill_recursevly returned error\n");
			if (cnxt.flexarr_ptr != NULL) { /**/
				free(cnxt.flexarr_ptr);
				cnxt.flexarr_ptr = NULL;
				cnxt.flexarr_byte_size = 0;
			}
			free(*ptr);
			*ptr = NULL;
			return -ENOMEM;
		}
		memcpy(_ptr + byte_size, cnxt.flexarr_ptr, cnxt.flexarr_byte_size);
		free(cnxt.flexarr_ptr);
		*ptr = _ptr;
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


