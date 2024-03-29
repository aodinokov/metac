/*
 * metac_oop.h
 *
 *  Created on: Feb 15, 2020
 *      Author: mralex
 */

#ifndef SRC_METAC_OOP_H_
#define SRC_METAC_OOP_H_

#include <stddef.h> /* for offsetof */

#define _create_with_error_result_(_type_, _error_result_) \
	struct _type_ * p_##_type_; \
	p_##_type_ = calloc(1, sizeof(*(p_##_type_))); \
	if (p_##_type_ == NULL) { \
		msg_stderr("Can't create " #_type_ ": no memory\n"); \
		return (_error_result_); \
	}

#define _create_(_type_) \
	_create_with_error_result_(_type_, NULL)

#define _delete_start_(_type_) \
	if (pp_##_type_ == NULL) { \
		msg_stderr("Can't delete " #_type_ ": invalid parameter\n"); \
		return -EINVAL; \
	} \
	if (*pp_##_type_ == NULL) { \
		msg_stderr("Can't delete " #_type_ ": already deleted\n"); \
		return -EALREADY; \
	}
#define _delete_finish(_type_) \
	free(*pp_##_type_); \
	*pp_##_type_ = NULL;
#define _delete_(_type_) \
	_delete_start_(_type_) \
	_delete_finish(_type_)

/*
 * _container_of - Get the address of an object containing a field.
 *
 * @ptr: pointer to the field.
 * @type: type of the object.
 * @member: name of the field within the object.
 */
#define _container_of(ptr, type, member) \
    ({ \
        const __typeof__(((type *) NULL)->member) * __ptr = (ptr); \
        (type *)((char *)__ptr - offsetof(type, member));   \
    })

#endif /* SRC_METAC_OOP_H_ */
