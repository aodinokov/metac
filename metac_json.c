/*
 * metac_json.c
 *
 *  Created on: Jan 15, 2019
 *      Author: mralex
 */

#include "metac_type.h"
#include "metac_debug.h"	/* msg_stderr, ...*/

#include <json/json.h>

#include <errno.h>			/* ENOMEM etc */

struct json_visitor {
	/* parent type */
	struct metac_visitor visitor;
	/* temp data */
};

static void json_visitor_start(
		struct metac_visitor *p_visitor,
		metac_count_t regions_count,
		metac_count_t unique_regions_count
		) {
}
static void json_visitor_region(
		struct metac_visitor *p_visitor,
		metac_count_t r_id,
		void *ptr,
		metac_byte_size_t byte_size,
		metac_count_t elements_count
		) {
}
static void json_visitor_unique_region(
		struct metac_visitor *p_visitor,
		metac_count_t r_id,
		metac_count_t u_idx
		) {
}
static void json_visitor_non_unique_region(
		struct metac_visitor *p_visitor,
		metac_count_t r_id,
		metac_count_t u_idx,
		metac_data_member_location_t offset
		) {
}
static void json_visitor_region_element(
		struct metac_visitor *p_visitor,
		metac_count_t r_id,
		metac_count_t e_id,
		metac_type_t * type,
		void *ptr,
		metac_byte_size_t byte_size,
		int * real_count_array,
		int subtypes_sequence_lenth
		) {
}
static void json_visitor_region_element_element(
		struct metac_visitor *p_visitor,
		metac_count_t r_id,
		metac_count_t e_id,
		metac_region_ee_subtype_t subtype,
		metac_count_t ee_id,
		metac_type_t * type,
		void *ptr,
		metac_byte_size_t byte_size,
		char * name_local,
		char * path_within_region_element
		) {
}

int metac_unpack_to_json(
		void *ptr,
		metac_byte_size_t size,
		metac_precompiled_type_t * precompiled_type,
		metac_count_t elements_count,
		json_object ** pp_json) {
	int res = 0;
	json_object * res_json = NULL;

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

	res = metac_visit(ptr, size, precompiled_type, elements_count, NULL, 0, &json_visitor);
	if (res != 0) {
		msg_stderr("metac_visit failed\n");
		/*TBD: cleanup json_visitor*/
		return -EFAULT;
	}

	res_json = json_object_new_array();
	if (res_json == NULL) {
		msg_stderr("json_object_new_array failed\n");
		/*TBD: cleanup json_visitor*/
		return -ENOMEM;
	}

	if (pp_json) {
		*pp_json = res_json;
	}

	return 0;
}


