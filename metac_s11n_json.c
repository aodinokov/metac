/*
 * metac_s11n_json.c
 *
 *  Created on: Dec 15, 2016
 *      Author: mralex
 */

#include <json/json.h>
#include <assert.h>

#include "metac_type.h"
#include "metac_debug.h"

static int _metac_alloc_and_fill_recursevly(struct metac_type * type, json_object * jobj, void **ptr) {
	metac_type_id_t id;
	if (type == NULL || ptr == NULL)
		return -1;

	type = metac_type_typedef_skip(type);
	assert(type);

	id = metac_type_id(type);
	switch (id) {
	case DW_TAG_base_type:
		/*allocate */
	case DW_TAG_enumeration_type:
		/*allocate */
		return 0;
	case DW_TAG_pointer_type: {
			int res = 0;
			/*get info about what type of the pointer*/
			if (type->p_at.p_at_type) {
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
				assert(metac_type_id(minfo.type) == DW_TAG_pointer_type && (minfo.p_bit_offset || minfo.p_bit_size));
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

	/*fill*/
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
		//msg_stddbg("mobj %p\n", mobj);
		json_object_put(jobj);
		return mobj;
	}
	return NULL;
}


