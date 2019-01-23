/*
 * metac_json.h
 *
 *  Created on: Jan 15, 2019
 *      Author: mralex
 */

#ifndef METAC_JSON_H_
#define METAC_JSON_H_

#include "metac_type.h"
#include <json/json.h>

int metac_unpack_to_json(
		metac_precompiled_type_t * precompiled_type,
		void *ptr,
		metac_byte_size_t size,
		metac_count_t elements_count,
		json_object ** pp_json);
int metac_pack_from_json(
		metac_precompiled_type_t * precompiled_type,
		json_object * p_json,
		void **p_ptr,
		metac_byte_size_t * p_size,
		metac_count_t * p_elements_count
		);

#endif /* METAC_JSON_H_ */
