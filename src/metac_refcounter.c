/*
 * metac_refcounter.c
 *
 *  Created on: Feb 16, 2020
 *      Author: mralex
 */


#include <errno.h>			/* ENOMEM etc */
#include <stdlib.h>			/* calloc, free, NULL, qsort... */

#include "metac_debug.h"	/* msg_stderr, ...*/
#include "metac/metac_refcounter.h"

int metac_unknown_object_init(
		struct metac_unknown_object *				p_unknown_object,
		struct metac_unknown_object_ops	*			p_unknown_object_ops,
		void *										p_private_data) {

	if (p_unknown_object == NULL) {
		msg_stderr("Invalid argument\n");
		return -(EINVAL);
	}

	p_unknown_object->p_unknown_object_ops = p_unknown_object_ops;
	p_unknown_object->p_private_data = p_private_data;

	return 0;
}

int metac_unknown_object_supports_snprintf(
		struct metac_unknown_object *				p_unknown_object) {

	if (p_unknown_object == NULL) {
		msg_stderr("Invalid argument\n");
		return -(EINVAL);
	}

	if (p_unknown_object->p_unknown_object_ops == NULL ||
		p_unknown_object->p_unknown_object_ops->metac_unknown_object_snprintf == NULL) {
		msg_stddbg("Unsupported operation in ops: %p\n",
				p_unknown_object->p_unknown_object_ops);
		return 0;
	}

	return 1;
}

int metac_unknown_object_snprintf(
		struct metac_unknown_object *				p_unknown_object,
		char *										str,
		size_t										size) {

	int res = metac_unknown_object_supports_snprintf(p_unknown_object);

	if (res < 0) {
		msg_stderr("metac_unknown_object_supports_snprintf failed\n");
		return res;
	}

	if (res == 0) {
		msg_stderr("snprintf operation isn't supported\n");
		return -(EPROTONOSUPPORT);
	}

	return p_unknown_object->p_unknown_object_ops->metac_unknown_object_snprintf(
			p_unknown_object,
			str,
			size);
}

int metac_unknown_object_supports_delete(
		struct metac_unknown_object *				p_unknown_object) {

	if (p_unknown_object == NULL) {
		msg_stderr("Invalid argument\n");
		return -(EINVAL);
	}

	if (p_unknown_object->p_unknown_object_ops == NULL ||
		p_unknown_object->p_unknown_object_ops->metac_unknown_object_delete == NULL) {
		msg_stddbg("Unsupported operation in ops: %p\n",
				p_unknown_object->p_unknown_object_ops);
		return 0;
	}

	return 1;
}

int metac_unknown_object_delete(
		struct metac_unknown_object *				p_unknown_object) {

	int res = metac_unknown_object_supports_delete(p_unknown_object);

	if (res < 0) {
		msg_stderr("metac_unknown_object_supports_delete failed\n");
		return res;
	}

	if (res == 0) {
		msg_stderr("delete operation isn't supported\n");
		return -(EPROTONOSUPPORT);
	}

	return p_unknown_object->p_unknown_object_ops->metac_unknown_object_delete(
			p_unknown_object);
}

int metac_refcounter_object_init(
		struct metac_refcounter_object *			p_refcounter_object,
		struct metac_refcounter_object_ops *		p_refcounter_object_ops,
		void *										p_private_data) {

	if (p_refcounter_object == NULL) {
		msg_stderr("Invalid argument\n");
		return -(EINVAL);
	}

	if (metac_unknown_object_init(
			&p_refcounter_object->unknown_object,
			&p_refcounter_object_ops->unknown_object_ops,
			p_private_data) != 0) {
		msg_stderr("metac_unknown_object_init failed\n");
		return -(EFAULT);
	}
	p_refcounter_object->ref_count = 1;

	return 0;
}

struct metac_refcounter_object *
	metac_refcounter_object_get(
		struct metac_refcounter_object *			p_refcounter_object) {

	if (p_refcounter_object == NULL) {
		msg_stderr("p_refcounter_object is NULL\n");
		return NULL;
	}

	/*
	 * ref_count == 0 may happen only in case of non-dynamically allocated object, or objects that can't be deleted,
	 * we can stop tracking that references
	 */
	if (metac_unknown_object_supports_delete(
			&p_refcounter_object->unknown_object) == 0) {
		return p_refcounter_object;
	}

	if (p_refcounter_object->ref_count == 0) {
		msg_stderr("Requested for reference increment object %p had to be deleted before but wasn't deleted. This process has memory leaks\n",
				p_refcounter_object);
		return NULL;
	}

	if (p_refcounter_object->ref_count == UINT32_MAX) {
		msg_stderr("too many references\n");
		return NULL;
	}

	++p_refcounter_object->ref_count;

	return p_refcounter_object;
}

int metac_refcounter_object_put(
		struct metac_refcounter_object *			p_refcounter_object) {

	if (p_refcounter_object == NULL) {
		msg_stddbg("p_refcounter_object is NULL\n");
		return -EALREADY;
	}

	/*
	 * ref_count == 0 may happen only in case of non-dynamically allocated object, or objects that can't be deleted,
	 * we can stop tracking that references
	 */
	if (metac_unknown_object_supports_delete(
			&p_refcounter_object->unknown_object) == 0) {
		return 0;
	}

	if (p_refcounter_object->ref_count == 0) {
		msg_stderr("Requested for deletion object %p had to be deleted before but wasn't deleted. This process has memory leaks\n",
				p_refcounter_object);
		return (-EFAULT);
	}

	if ((--p_refcounter_object->ref_count) > 0) {
		return 0;
	}

	if (metac_unknown_object_delete(
			&p_refcounter_object->unknown_object) != 0) {
		msg_stderr("Can't delete object %p, still removing reference. We are stopping keep track of it's references. This process has memory leaks\n",
				p_refcounter_object);
		return -(EFAULT);
	}

	return 0;
}
