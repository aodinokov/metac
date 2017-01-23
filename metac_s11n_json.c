/*
 * metac_s11n_json.c
 *
 *  Created on: Dec 15, 2016
 *      Author: mralex
 */

#include <json/json.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "metac_type.h"
#include "metac_debug.h"

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
	}else if (strcmp(type->p_at.p_at_name->name, "char") == 0){
		assert(byte_size == sizeof(char));
		/*json_type_string or TODO: json_type_int in range 0 - 255*/
		assert(jtype == json_type_string);
		assert(json_object_get_string_len(jobj) == 1);
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

static int _metac_alloc_and_fill_recursevly(struct metac_type * type, json_object * jobj, void **ptr);
static int _metac_fill_recursevly(struct metac_type * type, json_object * jobj, void *ptr, metac_byte_size_t byte_size) {
	metac_type_id_t id;

	msg_stddbg("type %s jtype %d\n", type->p_at.p_at_name?type->p_at.p_at_name->name:"", (int)json_object_get_type(jobj));

	id = metac_type_id(type);
	switch (id) {
	case DW_TAG_base_type:
		return _metac_fill_basic_type(type, jobj, ptr, byte_size);
	case DW_TAG_enumeration_type:
		return _metac_fill_enumeration_type(type, jobj, ptr, byte_size);
	case DW_TAG_pointer_type: {
			int res = 0;
			/*get info about what type of the pointer*/
			if (type->p_at.p_at_type == NULL) {
				/*we don't know the type, so we just can fill this if we have any data in jobj */
			}else { /*we know the type of data, so we need to allocate space*/
				/*check that jobj has appropriate type*/

			}
			return res;
		}
	case DW_TAG_array_type: {
			int res = 0;
			metac_num_t i;
			struct metac_type_array_info info;

			if (metac_type_array_info(type, &info) != 0)
				return -1;

			for (i = 0; i < info.elements_count; i++) {
				void *_ptr;
				struct metac_type_element_info einfo;
				if (metac_type_array_element_info(type, i, &einfo) != 0) {
					++res;
					continue;
				}
			}
			return res;
		}
	case DW_TAG_structure_type: {
			int res = 0;
			metac_num_t i;
			struct metac_type_structure_info info;

			if (metac_type_structure_info(type, &info) != 0)
				return -1;

			for (i = 0; i < info.members_count; i++) {
				void *_ptr;
				struct metac_type_member_info minfo;
				if (metac_type_structure_member_info(type, i, &minfo) != 0) {
					++res;
					continue;
				}
				if (minfo.p_bit_offset || minfo.p_bit_size)
					continue; /*TODO: support this case if needed*/

			}
			return res;
		}
	case DW_TAG_union_type:
		/*TODO: don't know what to do here */
		assert(0);
		return -1;
	}
	return 0;
}

static int _metac_alloc_and_fill_recursevly(struct metac_type * type, json_object * jobj, void **ptr) {
	int res = 0;
	metac_byte_size_t byte_size;
	if (type == NULL || ptr == NULL)
		return -EINVAL;

	type = metac_type_typedef_skip(type);
	assert(type);

	byte_size = metac_type_byte_size(type);
	if (byte_size == 0) {
		msg_stderr("Can't determine byte_size for type");
		return -EINVAL;
	}

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
		free(mobj);
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


