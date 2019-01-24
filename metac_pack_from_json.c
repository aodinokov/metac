/*
 * metac_pack_from_json.c
 *
 *  Created on: Jan 24, 2019
 *      Author: mralex
 */

//#define METAC_DEBUG_ENABLE
#include <errno.h>			/* ENOMEM etc */
#include <assert.h>			/* assert */
#include <stdint.h>			/* int8_t - int64_t*/
#include <complex.h>		/* complext */
#include <string.h>			/* strcmp*/
#include <endian.h>			/* __BYTE_ORDER at compile time */
#include <json/json.h>		/* json_... */
#include <urcu/list.h>		/* cds_list_entry, ... */

#include "metac_type.h"
#include "metac_debug.h"	/* msg_stderr, ...*/

/*****************************************************************************/
int metac_pack_from_json(
		metac_precompiled_type_t * precompiled_type,
		json_object * p_json,
		void **p_ptr,
		metac_byte_size_t * p_size,
		metac_count_t * p_elements_count
		) {
	metac_count_t unique_regions_count;

	/*check consistency first*/
	if (json_object_get_type(p_json) != json_type_array) {
		msg_stderr("p_json isn't array\n");
		return -EINVAL;
	}
	unique_regions_count = json_object_array_length(p_json);
	if (unique_regions_count == 0) {
		msg_stderr("p_json has zero length\n");
		return -EINVAL;
	}

	return 0;
}
