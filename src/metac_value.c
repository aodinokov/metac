/*
 * metac_value.c
 *
 *  Created on: Mar 5, 2020
 *      Author: mralex
 */

#include <assert.h>			/* assert */
#include <errno.h>			/* ENOMEM etc */

#include "metac/metac_value.h"

/*****************************************************************************/

struct metac_value_backend * metac_value_backend_get(
		struct metac_value_backend *				p_metac_value_backend) {

	if (p_metac_value_backend != NULL &&
		metac_refcounter_object_get(&p_metac_value_backend->refcounter_object) != NULL) {
		return p_metac_value_backend;
	}

	return NULL;
}

int metac_value_backend_put(
		struct metac_value_backend **				pp_metac_value_backend) {

	if (pp_metac_value_backend != NULL &&
		(*pp_metac_value_backend) != NULL &&
		metac_refcounter_object_put(&(*pp_metac_value_backend)->refcounter_object) == 0) {

		*pp_metac_value_backend = NULL;
		return 0;
	}

	return -(EFAULT);
}

int metac_value_backend_init(
		struct metac_value_backend *				p_metac_value_backend,
		struct metac_value_backend_ops *			p_metac_value_backend_ops,
		void *										p_private_data) {

	if (p_metac_value_backend == NULL) {

		msg_stderr("Invalid argument\n");
		return -(EINVAL);
	}

	if (metac_refcounter_object_init(
			&p_metac_value_backend->refcounter_object,
			&p_metac_value_backend_ops->refcounter_object_ops,
			p_private_data) != 0) {
		msg_stderr("metac_refcounter_object_init failed\n");
		return -(EFAULT);
	}

	return 0;
}

/*****************************************************************************/
