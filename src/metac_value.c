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


#define alloc_sptrinf(...) ({\
	int len = snprintf(NULL, 0, ##__VA_ARGS__); \
	char *str = calloc(len + 1, sizeof(*str)); \
	if (str != NULL)snprintf(str, len + 1, ##__VA_ARGS__); \
	str; \
})
#define build_member_path(path, member_name) ({ \
	char * member_path = NULL; \
	if ((path) != NULL && strcmp((path), "") != 0 ) { \
		member_path = alloc_sptrinf("%s.%s", (path), (member_name)); \
	} else { \
		member_path = alloc_sptrinf("%s", (member_name)); \
	} \
	member_path; \
})

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
int metac_value_get_value_backend_based_on_parent(
		struct metac_value *						p_metac_value) {

	struct metac_value_backend * p_metac_value_backend;

	if (p_metac_value == NULL) {

		msg_stderr("invalid argument\n");
		return -(EINVAL);
	}

//	assert(p_metac_value->p_scheme);
//	assert(p_metac_value->p_scheme->p_actual_type);
//
//	if (metac_scheme_is_hierarchy_top_scheme(p_metac_value->p_scheme) != 1) {
//
//		msg_stderr("p_metac_value isn't hierarchy(structure or union)\n");
//		return -(EINVAL);
//	}
//
//	assert(p_metac_value->p_value_backend);
//
//	p_metac_value_backend = p_metac_value->p_value_backend;

	return 0;
}

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
/*****************************************************************************/

static int metac_value_clean(
		struct metac_value *						p_metac_value);

/*****************************************************************************/
static int metac_value_delete(
		struct metac_value **						pp_metac_value) {

	_delete_start_(metac_value);
	metac_value_clean(*pp_metac_value);
	_delete_finish(metac_value);
	return 0;
}

static inline struct metac_value * metac_unknown_object_get_metac_value(
		struct metac_unknown_object *				p_metac_unknown_object) {
	return cds_list_entry(
			p_metac_unknown_object,
			struct metac_value,
			refcounter_object.unknown_object);
}

static int _metac_value_unknown_object_delete(
		struct metac_unknown_object *				p_metac_unknown_object) {

	struct metac_value * p_metac_value;

	if (p_metac_unknown_object == NULL) {
		msg_stderr("p_metac_unknown_object is NULL\n");
		return -(EINVAL);
	}

	p_metac_value = metac_unknown_object_get_metac_value(p_metac_unknown_object);
	if (p_metac_value == NULL) {
		msg_stderr("p_metac_value is NULL\n");
		return -(EINVAL);
	}

	return metac_value_delete(&p_metac_value);
}

static metac_refcounter_object_ops_t _metac_value_refcounter_object_ops = {
		.unknown_object_ops = {
				.metac_unknown_object_delete = 		_metac_value_unknown_object_delete,
		},
};

/*****************************************************************************/
struct metac_value * metac_value_get(
		struct metac_value *						p_metac_value) {

	if (p_metac_value != NULL &&
		metac_refcounter_object_get(&p_metac_value->refcounter_object) != NULL) {

		return p_metac_value;
	}

	return NULL;
}

int metac_value_put(
		struct metac_value **						pp_metac_value) {

	if (pp_metac_value != NULL &&
		(*pp_metac_value) != NULL &&
		metac_refcounter_object_put(&(*pp_metac_value)->refcounter_object) == 0) {

		*pp_metac_value = NULL;
		return 0;
	}

	return -(EFAULT);
}
/*****************************************************************************/
static int metac_value_init_as_hierarchy_member_value(
		struct metac_value *						p_metac_value,
		char * 										global_path,
		char *										value_path,
		struct value_with_hierarchy *				p_current_hierarchy,
		struct metac_scheme *						p_metac_scheme) {

	p_metac_value->p_scheme = metac_scheme_get(p_metac_scheme);
	p_metac_value->path_within_value = strdup(value_path);
	p_metac_value->p_current_hierarchy = p_current_hierarchy;

	p_metac_value->hierarchy_member.id = p_metac_scheme->hierarchy_member.id;

	//p_metac_value->p_value_backend = metac_value_backend_get(p_metac_value_backend);
	//element_get_memory_backend_interface
	metac_value_get_value_backend_based_on_parent(p_metac_value);

	return 0;
}

static struct metac_value * metac_value_create_as_hierarchy_member_value(
		char * 										global_path,
		char *										value_path,
		struct value_with_hierarchy *				p_current_hierarchy,
		struct metac_scheme *						p_metac_scheme) {

	_create_(metac_value);

	if (metac_refcounter_object_init(
			&p_metac_value->refcounter_object,
			&_metac_value_refcounter_object_ops,
			NULL) != 0) {

		msg_stderr("metac_refcounter_object_init failed\n");

		metac_value_delete(&p_metac_value);
		return NULL;
	}

	if (metac_value_init_as_hierarchy_member_value(
			p_metac_value,
			global_path,
			value_path,
			p_current_hierarchy,
			p_metac_scheme) != 0) {

		msg_stderr("metac_value_init_as_hierarchy_member_value failed\n");

		metac_value_delete(&p_metac_value);
		return NULL;
	}

	return p_metac_value;
}

static void metac_value_clean_as_hierarchy_top_value(
		struct metac_value *						p_metac_value) {

	metac_count_t i;

	if (p_metac_value == NULL) {

		msg_stderr("invalid argument\n");
		return;
	}

	if (metac_value_is_hierarchy_top_value(p_metac_value) != 1) {

		msg_stderr("invalid argument\n");
		return;
	}

	if (p_metac_value->hierarchy_top.pp_members) {

		for (i = 0; i < metac_hierarchy_top_scheme_get_members_count(p_metac_value->p_scheme); ++i) {

			if (p_metac_value->hierarchy_top.pp_members[i] != NULL) {

				metac_value_put(&p_metac_value->hierarchy_top.pp_members[i]);
			}
		}
		free(p_metac_value->hierarchy_top.pp_members);
		p_metac_value->hierarchy_top.pp_members = NULL;
	}

	if (p_metac_value->hierarchy_top.pp_discriminator_values) {

		for (i = 0; i < metac_hierarchy_top_scheme_get_discriminators_count(p_metac_value->p_scheme); ++i) {

			if (p_metac_value->hierarchy_top.pp_discriminator_values[i] != NULL) {

				discriminator_value_delete(&p_metac_value->hierarchy_top.pp_discriminator_values[i]);
			}
		}
		free(p_metac_value->hierarchy_top.pp_discriminator_values);
		p_metac_value->hierarchy_top.pp_discriminator_values = NULL;
	}

	/**/
}
static int metac_value_init_as_hierarchy_top_value(
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
			metac_value_clean_as_hierarchy_top_value(p_metac_value);
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
				metac_value_clean_as_hierarchy_top_value(p_metac_value);
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
					metac_value_clean_as_hierarchy_top_value(p_metac_value);
					return (-EFAULT);
				}

				if (metac_value_calculate_hierarchy_top_discriminator_value(p_metac_value, i) != 0){

					msg_stderr("metac_value_calculate_hierarchy_top_discriminator_value failed for path %s\n", value_path);
					metac_value_clean_as_hierarchy_top_value(p_metac_value);
					return (-EFAULT);
				}

			}

		}
	}

	if (metac_hierarchy_top_scheme_get_members_count(p_metac_value->p_scheme) > 0) {

		int i;

		p_metac_value->hierarchy_top.pp_members =
				(struct metac_value **)calloc(
						metac_hierarchy_top_scheme_get_members_count(p_metac_value->p_scheme),
						sizeof(*p_metac_value->hierarchy_top.pp_members));
		if (p_metac_value->hierarchy_top.pp_members == NULL) {

			msg_stderr("wasn't able to get memory for pp_hierarchy_members\n");
			metac_value_clean_as_hierarchy_top_value(p_metac_value);
			return (-EFAULT);
		}

		/* create members based on discriminators */
		for (i = 0; i < metac_hierarchy_top_scheme_get_members_count(p_metac_value->p_scheme); ++i) {
			metac_flag_t allocate;
			struct metac_scheme * p_member_scheme;

			p_member_scheme = metac_hierarchy_top_scheme_get_hierarchy_member_scheme(
					p_metac_value->p_scheme,
					i);

			assert(p_member_scheme != NULL);

			allocate = 0;
			if (	allocate == 0 &&
					p_member_scheme->hierarchy_member.precondition.p_discriminator == NULL) {

				allocate = 1;
			}

			if (	allocate == 0 &&
					p_member_scheme->hierarchy_member.precondition.p_discriminator != NULL) {

				struct discriminator_value * p_value = p_metac_value->hierarchy_top.pp_discriminator_values[p_member_scheme->hierarchy_member.precondition.p_discriminator->id];
				if (	p_value != NULL &&
						p_value->value == p_member_scheme->hierarchy_member.precondition.expected_discriminator_value) {

					allocate = 1;
				}
			}

			if (allocate != 0) {

				struct metac_scheme * p_member_parent_scheme;
				struct value_with_hierarchy * p_current_hierarchy;
				char * target_global_path;
				char * target_value_path;

				target_global_path = build_member_path(
						global_path,
						p_member_scheme->hierarchy_member.path_within_hierarchy);
				if (target_global_path == NULL) {

					msg_stderr("Can't build target_global_path for %s member %d\n", global_path, i);
					return (-EFAULT);
				}

				target_value_path = build_member_path(
						value_path,
						p_member_scheme->hierarchy_member.path_within_hierarchy);
				if (target_value_path == NULL) {

					msg_stderr("Can't build target_value_path for %s member %d\n", global_path, i);

					free(target_global_path);
					return (-EFAULT);
				}

				p_member_parent_scheme = metac_hierarchy_member_scheme_get_parent_scheme(p_member_scheme);
				if (p_member_parent_scheme == p_metac_value->p_scheme) {

					p_current_hierarchy = &p_metac_value->hierarchy;
					++p_current_hierarchy->members;
				} else {

					assert(metac_scheme_is_hierarchy_member_scheme(p_member_parent_scheme) == 1);
					assert(i > p_member_parent_scheme->hierarchy_member.id);

					p_current_hierarchy = &p_metac_value->hierarchy_top.pp_members[p_member_parent_scheme->hierarchy_member.id]->hierarchy;
					++p_current_hierarchy->members;
				}
				metac_scheme_put(&p_member_parent_scheme);

				p_metac_value->hierarchy_top.pp_members[i] = metac_value_create_as_hierarchy_member_value(
						target_global_path,
						target_value_path,
						p_current_hierarchy,
						p_member_scheme);

				free(target_value_path);
				free(target_global_path);

				if (p_metac_value->hierarchy_top.pp_members[i] == NULL) {

					msg_stderr("metac_value_create_as_hierarchy_member failed for member %d for path %s\n", (int)i, value_path);
					metac_scheme_put(&p_member_scheme);
					metac_value_clean_as_hierarchy_top_value(p_metac_value);
					return (-EFAULT);
				}
			}
			metac_scheme_put(&p_member_scheme);
		}
	}

	if (p_metac_value->hierarchy.members_count > 0) {

		int i, j, k;

		p_metac_value->hierarchy.members = (struct metac_value **)calloc(
				p_metac_value->hierarchy.members_count,
				sizeof(*p_metac_value->hierarchy.members));
		if (p_metac_value->hierarchy.members == NULL) {

			msg_stderr("can't allocate memory for members for path %s\n", value_path);
			return (-ENOMEM);
		}

		j = 0;
		for (i = 0; i < metac_hierarchy_top_scheme_get_members_count(p_metac_value->p_scheme); ++i) {

			if (	p_metac_value->hierarchy_top.pp_members[i] != NULL &&
					p_metac_value->hierarchy_top.pp_members[i]->p_current_hierarchy == &p_metac_value->hierarchy) {

				assert( j < p_metac_value->hierarchy.members_count);

				p_metac_value->hierarchy.members[j] = metac_value_get(p_metac_value->hierarchy_top.pp_members[i]);
				++j;
			}
		}


		for (k = 0; k < metac_hierarchy_top_scheme_get_members_count(p_metac_value->p_scheme); ++k) {

			if (p_metac_value->hierarchy_top.pp_members[k] == NULL &&
				metac_scheme_is_hierachy_scheme(p_metac_value->hierarchy_top.pp_members[k]->p_scheme) != 1) {
				continue;
			}

			if (p_metac_value->hierarchy_top.pp_members[k]->hierarchy.members_count > 0) {

				p_metac_value->hierarchy_top.pp_members[k]->hierarchy.members = (struct metac_value **)calloc(
						p_metac_value->hierarchy_top.pp_members[k]->hierarchy.members_count,
						sizeof(*p_metac_value->hierarchy_top.pp_members[k]->hierarchy.members));
				if (p_metac_value->hierarchy_top.pp_members[k]->hierarchy.members == NULL) {

					msg_stderr("can't allocate memory for members for path %s\n", value_path);
					return (-ENOMEM);
				}

				j = 0;
				for (i = 0; i < metac_hierarchy_top_scheme_get_members_count(p_metac_value->p_scheme); ++i) {

					if (	p_metac_value->hierarchy_top.pp_members[i] != NULL &&
							p_metac_value->hierarchy_top.pp_members[i]->p_current_hierarchy == &p_metac_value->hierarchy_top.pp_members[k]->hierarchy) {

						assert(j < p_metac_value->hierarchy_top.pp_members[k]->hierarchy.members_count);

						p_metac_value->hierarchy_top.pp_members[k]->hierarchy.members[j] = metac_value_get(p_metac_value->hierarchy_top.pp_members[i]);
						++j;
					}
				}
			}
		}

	}

	return 0;
}

metac_flag_t metac_value_is_hierarchy_top_value(
		struct metac_value *						p_metac_value) {

	if (p_metac_value == NULL) {

		msg_stderr("invalid argument\n");
		return -(EINVAL);
	}

	if (p_metac_value->p_scheme == NULL) {

		msg_stderr("p_scheme is NULL\n");
		return -(EFAULT);
	}

	/* TODO: rework in future.. actually may be we should not put scheme inside value.. may be just copy scheme to value??? */
	return metac_scheme_is_hierarchy_top_scheme(p_metac_value->p_scheme);
}



static int metac_value_init_as_indexable_value(
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
		return metac_value_init_as_hierarchy_top_value(
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
static int metac_value_clean(
		struct metac_value *						p_metac_value) {
	/*TODO: clean metac_value*/
	return 0;
}

