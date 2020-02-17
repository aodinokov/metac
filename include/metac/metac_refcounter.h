/*
 * metac_refcounter.h
 *
 *  Created on: Feb 16, 2020
 *      Author: mralex
 */

#ifndef INCLUDE_METAC_METAC_REFCOUNTER_H_
#define INCLUDE_METAC_METAC_REFCOUNTER_H_

#include <stdint.h>									/*uint32_t*/

typedef struct metac_unknown_object_ops				metac_unknown_object_ops_t;
typedef struct metac_unknown_object					metac_unknown_object_t;
typedef struct metac_refcounter_object_ops			metac_refcounter_object_ops_t;
typedef struct metac_refcounter_object				metac_refcounter_object_t;

struct metac_unknown_object_ops {
	int												(*metac_unknown_object_delete)(
			struct metac_unknown_object *				p_metac_unknown_object);
};

struct metac_unknown_object {
	struct metac_unknown_object_ops *				p_unknown_object_ops;
	void *											p_private_data;
};

int metac_unknown_object_init(
		struct metac_unknown_object *				p_unknown_object,
		struct metac_unknown_object_ops *			p_unknown_object_ops,
		void *										p_private_data);
int metac_unknown_object_delete(
		struct metac_unknown_object *				p_unknown_object);

struct metac_refcounter_object_ops {
	struct metac_unknown_object_ops					unknown_object_ops;
};

struct metac_refcounter_object {
	struct metac_unknown_object						unknown_object;
	uint32_t										ref_count;
};

int metac_refcounter_object_init(
		struct metac_refcounter_object *			p_refcounter_object,
		struct metac_refcounter_object_ops *		p_refcounter_object_ops,
		void *										p_private_data);
struct metac_refcounter_object *
	metac_refcounter_object_get(
		struct metac_refcounter_object *			p_refcounter_object);
int metac_refcounter_object_put(
		struct metac_refcounter_object *			p_refcounter_object);

#endif /* INCLUDE_METAC_METAC_REFCOUNTER_H_ */
