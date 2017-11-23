/*
 * metac_s11n_json.h
 *
 *  Created on: Nov 21, 2017
 *      Author: mralex
 */

#ifndef METAC_S11N_JSON_H_
#define METAC_S11N_JSON_H_

#include "metac_type.h"

struct metac_object * metac_json2object(struct metac_type * mtype, char *string);
char * metac_type_and_ptr2json_string(struct metac_type * type, void * ptr, metac_byte_size_t byte_size);

#endif /* METAC_S11N_JSON_H_ */
