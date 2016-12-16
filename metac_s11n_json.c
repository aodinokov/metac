/*
 * metac_s11n_json.c
 *
 *  Created on: Dec 15, 2016
 *      Author: mralex
 */

#include <json/json.h>

#include "metac_type.h"

static struct metac_object * _metac_json_object_2_metac_object(struct metac_type * mtype, json_object * jobj) {
	struct metac_object * mobj;

	/*todo: check if mtype has appropriate id*/

	mobj = (struct metac_object *)calloc(1, sizeof(struct metac_object));

	if (mobj == NULL)
		return NULL;

	mobj->type = mtype;

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


