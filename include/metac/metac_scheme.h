/*
 * metac_scheme.h
 *
 *  Created on: Feb 18, 2020
 *      Author: mralex
 */

#ifndef INCLUDE_METAC_METAC_SCHEME_H_
#define INCLUDE_METAC_METAC_SCHEME_H_

#include "metac/metac_type.h"
#include "metac/metac_refcounter.h"

/*****************************************************************************/
struct condition;
struct discriminator;
struct scheme_with_pointer;
struct scheme_with_hierarchy;
struct scheme_with_array;
struct metac_scheme;
/*****************************************************************************/

struct condition {
	struct discriminator *							p_discriminator;
	metac_discriminator_value_t						expected_discriminator_value;
};

struct scheme_with_pointer {
	struct metac_scheme *							p_default_child_value_scheme;

	char *											annotation_key;						/* keep annotation_key to use it for cb call */

	/* from annotations:*/
	struct {
		metac_cb_generic_cast_t						cb;
		void *										p_data;
		metac_count_t								types_count;
		struct generic_cast_type {
			struct metac_type *						p_type;
			struct metac_scheme *					p_child_value_scheme;
		}*											p_types;
	}generic_cast;

	struct {
		metac_cb_array_elements_count_t				cb;
		void *										p_data;
	}array_elements_count;
};

struct scheme_with_array {
	struct metac_scheme *							p_child_value_scheme;

	char *											annotation_key;						/* keep annotation_key to use it for cb call */

	/* from annotations:*/
	struct {
		metac_cb_array_elements_count_t				cb;
		void *										p_data;
	}array_elements_count;
};

struct scheme_with_hierarchy {															/* struct or union */
	metac_count_t 									members_count;
	struct metac_scheme **							members;

	struct discriminator {																/* needed only if it's a union */
		metac_count_t								id;									/* index in the region_element_type. needed to correlate with discriminator_values */
		struct condition							precondition;						/* pre-condition for this discriminator: (copy of precondition from hierarchy_memeber) */

		char *										annotation_key;						/* keep annotation_key to use it for cb call */

		metac_cb_discriminator_t					cb;
		void *										p_data;
	}												union_discriminator;
};

struct metac_scheme {
	metac_refcounter_object_t						refcounter_object;					/* we want to make this object refcountable */

	struct value_scheme {
		metac_count_t								arrays_value_schemes_count;
		struct metac_scheme **						pp_arrays_value_schemes;

		metac_count_t								pointers_value_schemes_count;
		struct metac_scheme **						pp_pointers_value_schemes;

		struct deep_value_scheme {

			metac_count_t							objects_count;

			struct deep_value_scheme_object {		/* I don't like the name*/
				struct metac_scheme *				p_object;

				metac_count_t						pointers_count;
				struct metac_scheme **				pp_pointers;

			} *										p_objects;

		} * 										p_deep_value_scheme;

	} *												p_value_scheme;

	struct scheme_with_hierarchy *					p_current_hierarchy;
	union {
		struct hierarchy_member {														/* if p_current_value_scheme_hierarchy isn't NULL */
			metac_count_t							id;									/* index in the element_type_hierarhy_top. needed to find parents quickier */
			struct condition 						precondition;						/* precondition for this region element */

			metac_data_member_location_t			offset;								/* offset within element */

			metac_count_t							member_id;							/* member id within parent hierarchy */
			struct metac_type_member_info *			p_member_info;

			char *									path_within_hierarchy;

		}											hierarchy_member;

		struct hierarchy_top {															/* if p_current_value_scheme_hierarchy is NULL */
			metac_count_t 							discriminators_count;
			struct discriminator **					pp_discriminators;

			metac_count_t 							members_count;
			struct metac_scheme **					pp_members;							/* full list of members */
		}											hierarchy_top;
	};

	/* content info */
	struct metac_type *								p_type;
	struct metac_type *								p_actual_type;						/* cache: metac_actual_type(p_type) */
	metac_byte_size_t 								byte_size;							/* cache: bite size */

	char *											path_within_value;

	union {																				/* based on p_actual_type->id */
		struct scheme_with_pointer					pointer;
		struct scheme_with_array					array;
		struct scheme_with_hierarchy				hierarchy;
	};
};

struct metac_scheme * metac_scheme_create(
		struct metac_type *							p_type,
		metac_type_annotation_t *					p_override_annotations,
		metac_flag_t								go_deep);
struct metac_scheme * metac_scheme_get(
		struct metac_scheme *						p_metac_scheme);
int metac_scheme_put(
		struct metac_scheme **						pp_metac_scheme);

metac_flag_t metac_scheme_is_hierachy_scheme(
		struct metac_scheme *						p_metac_scheme);
metac_flag_t metac_scheme_is_array_scheme(
		struct metac_scheme *						p_metac_scheme);
metac_flag_t metac_scheme_is_pointer_scheme(
		struct metac_scheme *						p_metac_scheme);

metac_flag_t metac_scheme_is_deep_value_scheme(
		struct metac_scheme *						p_metac_scheme);					/* contains in addition to object data it contains info on how to parse all objects referenced by pointers*/
metac_flag_t metac_scheme_is_value_scheme(
		struct metac_scheme *						p_metac_scheme);					/* can be any type or consist of any combination of types. Can be applied to any block */
metac_flag_t metac_scheme_is_indexable(
		struct metac_scheme *						p_metac_scheme);					/* can be used as element of array */
metac_flag_t metac_scheme_is_hierarchy_top_scheme(
		struct metac_scheme *						p_metac_scheme);
metac_flag_t metac_scheme_is_hierarchy_member_scheme(
		struct metac_scheme *						p_metac_scheme);
struct metac_scheme * metac_hierarchy_member_scheme_get_parent_scheme(
		struct metac_scheme *						p_metac_scheme);

#endif /* INCLUDE_METAC_METAC_SCHEME_H_ */
