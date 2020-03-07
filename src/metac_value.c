/*
 * metac_value.c
 *
 *  Created on: Mar 5, 2020
 *      Author: mralex
 */

#include <assert.h>			/* assert */
#include <errno.h>			/* ENOMEM etc */
#include <stdlib.h>			/* calloc, free, NULL, qsort... */
#include <string.h>			/* strdup */
#include <urcu/list.h>		/* struct cds_list_head etc */

#include "metac/metac_value.h"

//#define METAC_DEBUG_ENABLE

#include "metac_debug.h"	/* msg_stderr, ...*/
#include "metac_oop.h"		/*_create, _delete, ...*/
#include "scheduler.h"



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

	p_metac_value_backend->p_ops = p_metac_value_backend_ops;

	return 0;
}

/*****************************************************************************/
int metac_value_calculate_hierarchy_top_discriminator_value(
		struct metac_value *						p_metac_value,
		int											id) {

	struct metac_value_backend * p_metac_value_backend;

	if (p_metac_value == NULL) {

		msg_stderr("invalid argument\n");
		return -(EINVAL);
	}

	assert(p_metac_value->p_scheme);
	assert(p_metac_value->p_scheme->p_actual_type);

	if (metac_scheme_is_hierarchy_top_scheme(p_metac_value->p_scheme) != 1) {

		msg_stderr("p_metac_value isn't hierarchy(structure or union)\n");
		return -(EINVAL);
	}

	assert(p_metac_value->p_value_backend);

	p_metac_value_backend = p_metac_value->p_value_backend;

	if (p_metac_value_backend->p_ops->value_calculate_hierarchy_top_discriminator_value == NULL) {
		msg_stderr("value_calculate_hierarchy_top_discriminator_value isn't defined\n");
		return -(ENOENT);
	}

	return p_metac_value_backend->p_ops->value_calculate_hierarchy_top_discriminator_value(
			p_metac_value,
			id);
}
/*****************************************************************************/
static int discriminator_value_delete(
		struct discriminator_value **				pp_discriminator_value) {
	_delete_(discriminator_value);
	return 0;
}

static struct discriminator_value * discriminator_value_create(
		metac_discriminator_value_t					value) {
	_create_(discriminator_value);
	p_discriminator_value->value = value;
	return p_discriminator_value;
}

static void metac_value_clean_as_hierarchy_top(
		struct metac_value *						p_metac_value) {
	metac_count_t i;
//	struct element_hierarchy_top * p_element_hierarchy_top;
//	struct element_type_hierarchy_top * p_element_type_hierarchy_top;
//
//	assert(p_element);
//	if (element_type_is_hierachy(p_element->p_element_type) == 0) {
//		msg_stderr("element isn't hierarchy\n");
//		return;
//	}
//
//	p_element_hierarchy_top = &p_element->hierarchy_top;
//	p_element_type_hierarchy_top = &p_element->p_element_type->hierarchy_top;
//
//
//	if (p_element_hierarchy_top->pp_members) {
//		for (i = 0; i < p_element_type_hierarchy_top->members_count; ++i) {
//			if (p_element_hierarchy_top->pp_members[i] != NULL) {
//				element_hierarchy_member_delete(
//						&p_element_hierarchy_top->pp_members[i]);
//			}
//		}
//		free(p_element_hierarchy_top->pp_members);
//		p_element_hierarchy_top->pp_members = NULL;
//	}
//	if (p_element_hierarchy_top->pp_discriminator_values) {
//		for (i = 0; i < p_element_type_hierarchy_top->discriminators_count; ++i) {
//			if (p_element_hierarchy_top->pp_discriminator_values[i] != NULL) {
//				discriminator_value_delete(&p_element_hierarchy_top->pp_discriminator_values[i]);
//			}
//		}
//		free(p_element_hierarchy_top->pp_discriminator_values);
//		p_element_hierarchy_top->pp_discriminator_values = NULL;
//	}
}

static int metac_value_init_as_hierarchy_top(
		struct metac_value *						p_metac_value,
		char * 										global_path,
		char *										value_path,
		struct metac_scheme *						p_metac_scheme) {

	if (p_metac_value == NULL) {

		msg_stderr("invalid argument for %s\n", value_path);
		return -(EINVAL);
	}

	if (metac_scheme_is_hierarchy_top_scheme(p_metac_scheme) != 1) {

		msg_stderr("invalid argument for %s\n", value_path);
		return -(EINVAL);
	}

	if (metac_hierarchy_top_scheme_get_discriminators_count(p_metac_value->p_scheme) > 0) {

		int i;

		p_metac_value->hierarchy_top.pp_discriminator_values =
				(struct discriminator_value **)calloc(
						metac_hierarchy_top_scheme_get_discriminators_count(p_metac_value->p_scheme),
						sizeof(*p_metac_value->hierarchy_top.pp_discriminator_values));
		if (p_metac_value->hierarchy_top.pp_discriminator_values != NULL) {

			msg_stderr("wasn't able to get memory for pp_discriminator_values\n");
			metac_value_clean_as_hierarchy_top(p_metac_value);
			return (-EFAULT);
		}

		for (i = 0; i < metac_hierarchy_top_scheme_get_discriminators_count(p_metac_value->p_scheme); ++i) {
			metac_flag_t allocate;
			struct discriminator * p_discriminator;

			p_discriminator = metac_hierarchy_top_scheme_get_discriminator(p_metac_value->p_scheme, i);

			assert(p_discriminator != NULL);
			/*assumption: if we initialize all discriminators in order of their appearance - we won't need to resolve dependencides*/
			assert(
					p_discriminator->precondition.p_discriminator == NULL ||
					p_discriminator->precondition.p_discriminator->id < i);
			/*Not needed since it's checked with the previous assert:*/
			/*assert(
				p_element->p_element_type->hierarchy_top.pp_discriminators[i]->precondition.p_discriminator == NULL ||
				p_element->p_element_type->hierarchy_top.pp_discriminators[i]->precondition.p_discriminator->id < p_element->p_element_type->hierarchy_top.discriminators_count);*/

			if (p_discriminator->cb == NULL) {

				msg_stderr("discriminator %d for path %s is NULL\n", (int)i, value_path);
				metac_value_clean_as_hierarchy_top(p_metac_value);
				return (-EFAULT);
			}

			allocate = 0;
			if (	allocate == 0 &&
					p_discriminator->precondition.p_discriminator == NULL) {

				allocate = 1;
			}

			if (	allocate == 0 &&
					p_discriminator->precondition.p_discriminator != NULL) {

				struct discriminator_value * p_value = p_metac_value->hierarchy_top.pp_discriminator_values[p_discriminator->precondition.p_discriminator->id];
				if (	p_value != NULL &&
						p_value->value == p_discriminator->precondition.expected_discriminator_value) {

					allocate = 1;
				}
			}

			if (allocate != 0) {
				p_metac_value->hierarchy_top.pp_discriminator_values[i] = discriminator_value_create(0);
				if (p_metac_value->hierarchy_top.pp_discriminator_values[i] == NULL) {

					msg_stderr("can't allocated pp_discriminator_values %d for path %s\n", (int)i, value_path);
					metac_value_clean_as_hierarchy_top(p_metac_value);
					return (-EFAULT);
				}
			}

			if (metac_value_calculate_hierarchy_top_discriminator_value(p_metac_value, i) != 0){

				msg_stderr("metac_value_calculate_hierarchy_top_discriminator_value failed for path %s\n", value_path);
				metac_value_clean_as_hierarchy_top(p_metac_value);
				return (-EFAULT);
			}

		}
	}

	if (metac_hierarchy_top_scheme_get_members_count(p_metac_value->p_scheme) > 0) {

		p_metac_value->hierarchy_top.pp_members =
				(struct metac_value **)calloc(
						metac_hierarchy_top_scheme_get_members_count(p_metac_value->p_scheme),
						sizeof(*p_metac_value->hierarchy_top.pp_members));
		if (p_metac_value->hierarchy_top.pp_members == NULL) {

			msg_stderr("wasn't able to get memory for pp_hierarchy_members\n");
			metac_value_clean_as_hierarchy_top(p_metac_value);
			return (-EFAULT);
		}
	}

	return 0;
}

static int metac_value_init_as_indexable(
		struct metac_value *						p_metac_value,
		char * 										global_path,
		char *										value_path,
		struct metac_scheme *						p_metac_scheme,
		struct metac_value_backend *				p_metac_value_backend) {	/*< -- p_metac_value_backend */

	if (p_metac_value == NULL) {

		msg_stderr("invalid argument for %s\n", value_path);
		return -(EINVAL);
	}

	p_metac_value->p_value_backend = metac_value_backend_get(p_metac_value_backend);
	p_metac_value->p_scheme = metac_scheme_get(p_metac_scheme);

	switch (p_metac_value->p_scheme->p_actual_type->id) {
	case DW_TAG_structure_type:
	case DW_TAG_union_type:
		return metac_value_init_as_hierarchy_top(
				p_metac_value,
				global_path,
				value_path,
				p_metac_scheme);
		break;
	case DW_TAG_pointer_type:
		break;
	case DW_TAG_array_type:
		break;

	}

	return 0;

}

/*****************************************************************************/
