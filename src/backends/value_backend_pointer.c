/*
 * value_backend_pointer.c
 *
 *  Created on: Mar 8, 2020
 *      Author: mralex
 */

#define METAC_DEBUG_ENABLE

#include <assert.h>			/* assert */
#include <errno.h>			/* ENOMEM etc */
#include <stdlib.h>			/* calloc, free, NULL, qsort... */
#include <urcu/list.h>		/* struct cds_list_entry etc */

#include <metac/metac_value.h>

#include "metac_oop.h"		/*_create, _delete, ...*/
#include "metac_debug.h"	/* msg_stderr, ...*/

struct metac_value_backend_with_pointer {
    struct metac_value_backend value_backend;

    void *ptr;
};

/*****************************************************************************/

static int metac_value_backend_with_pointer_clean(
        struct metac_value_backend_with_pointer *p_metac_value_backend_with_pointer);

/*****************************************************************************/
static int metac_value_backend_with_pointer_delete(
        struct metac_value_backend_with_pointer **pp_metac_value_backend_with_pointer) {

    _delete_start_(metac_value_backend_with_pointer);
    metac_value_backend_with_pointer_clean(
            *pp_metac_value_backend_with_pointer);
    _delete_finish(metac_value_backend_with_pointer);
    return 0;
}

static inline struct metac_value_backend_with_pointer* metac_unknown_object_get_metac_value_backend_with_pointer(
        struct metac_unknown_object *p_metac_unknown_object) {
return cds_list_entry(
        p_metac_unknown_object,
        struct metac_value_backend_with_pointer,
        value_backend.refcounter_object.unknown_object);
}

static int metac_value_backend_with_pointer_unknown_object_delete(
    struct metac_unknown_object *p_metac_unknown_object) {

struct metac_value_backend_with_pointer *p_metac_value_backend_with_pointer;

if (p_metac_unknown_object == NULL) {

    msg_stderr("p_metac_unknown_object is NULL\n");
    return -(EINVAL);
}

p_metac_value_backend_with_pointer =
        metac_unknown_object_get_metac_value_backend_with_pointer(
                p_metac_unknown_object);
if (p_metac_value_backend_with_pointer == NULL) {

    msg_stderr("p_metac_value_backend_with_pointer is NULL\n");
    return -(EINVAL);
}

return metac_value_backend_with_pointer_delete(
        &p_metac_value_backend_with_pointer);
}
/*****************************************************************************/
static int metac_value_backend_with_pointer_value_get_value_backend_based_on_parent(
    struct metac_value *p_value) {

assert(p_value);
assert(p_value->p_current_container_value);

//	switch() {
//
//	}

return 0;
}

static int metac_value_backend_with_pointer_value_calculate_hierarchy_top_discriminator_value(
    struct metac_value *p_value, int id) {

return 0;
}

/*****************************************************************************/
static struct metac_value_backend_ops _metac_value_backend_with_pointer_ops =
    {
            .refcounter_object_ops =
                    {
                            .unknown_object_ops =
                                    {
                                            .metac_unknown_object_delete =
                                                    metac_value_backend_with_pointer_unknown_object_delete, }, },
            .value_get_value_backend_based_on_parent =
                    metac_value_backend_with_pointer_value_get_value_backend_based_on_parent,
            .value_calculate_hierarchy_top_discriminator_value =
                    metac_value_backend_with_pointer_value_calculate_hierarchy_top_discriminator_value, };
/*****************************************************************************/
static int metac_value_backend_with_pointer_clean(
    struct metac_value_backend_with_pointer *p_metac_value_backend_with_pointer) {
if (p_metac_value_backend_with_pointer == NULL) {

    msg_stderr("invalid argument\n");
    return -EINVAL;
}

p_metac_value_backend_with_pointer->ptr = NULL;

return 0;
}

static int metac_value_backend_with_pointer_init(
    struct metac_value_backend_with_pointer *p_metac_value_backend_with_pointer,
    void *ptr) {

if (p_metac_value_backend_with_pointer == NULL) {

    msg_stderr("invalid argument\n");
    return -EINVAL;
}

p_metac_value_backend_with_pointer->ptr = ptr;

return 0;
}

struct metac_value_backend_with_pointer* metac_value_backend_with_pointer_create(
    void *ptr) {

_create_(metac_value_backend_with_pointer);

if (metac_value_backend_init(&p_metac_value_backend_with_pointer->value_backend,
        &_metac_value_backend_with_pointer_ops, NULL) != 0) {

    msg_stderr("metac_value_backend_init failed\n");

    metac_value_backend_with_pointer_delete(
            &p_metac_value_backend_with_pointer);
    return NULL;
}

if (metac_value_backend_with_pointer_init(p_metac_value_backend_with_pointer,
        ptr) != 0) {

    msg_stderr("metac_value_backend_with_pointer_init failed\n");

    metac_value_backend_with_pointer_delete(
            &p_metac_value_backend_with_pointer);
    return NULL;
}

return p_metac_value_backend_with_pointer;
}

