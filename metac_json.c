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

struct region {
	json_object * p_json_object;

	void *ptr;
	metac_byte_size_t byte_size;
	metac_count_t elements_count;
	metac_count_t u_idx;	/*for non_unique only*/
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

	res = metac_visit(ptr, size, precompiled_type, elements_count, NULL, 0, &json_visitor.visitor);
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


