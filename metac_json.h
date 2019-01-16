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
		void *ptr,
		metac_byte_size_t size,
		metac_precompiled_type_t * precompiled_type,
		metac_count_t elements_count,
		json_object ** pp_json);

#endif /* METAC_JSON_H_ */
