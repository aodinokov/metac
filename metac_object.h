/*
 * metac_object.h
 *
 *  Created on: Oct 31, 2015
 *      Author: mralex
 */

#ifndef METAC_OBJECT_H_
#define METAC_OBJECT_H_

#include "metac_type.h"

/*TODO: put the code below to the separate file metac_object.h*/
/* definition of instance of the specific type in memory*/
struct metac_object;

/*TODO: rename to metac_array_create and make metac_object_create based on it */
struct metac_object * 	metac_object_create(struct metac_type *type);
struct metac_object * 	metac_object_array_create(struct metac_type *type, unsigned int count);
struct metac_type * 		metac_object_type(struct metac_object *object);
unsigned int 				metac_object_count(struct metac_object *object);
/*TODO: rename to metac_array_ptr */
void *						metac_object_ptr(struct metac_object *object, unsigned int *p_data_length);	/*<< Returns pointer to the C data*/
void 						metac_object_destroy(struct metac_object *object);

/* object structure functions */
struct metac_object *	metac_object_structure_member_by_name(struct metac_object *object, const char *member_name);



#endif /* METAC_OBJECT_H_ */
