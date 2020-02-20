/*
 * metac_value_scheme.h
 *
 *  Created on: Feb 18, 2020
 *      Author: mralex
 */

#ifndef INCLUDE_METAC_METAC_VALUE_SCHEME_H_
#define INCLUDE_METAC_METAC_VALUE_SCHEME_H_

#include "metac/metac_type.h"
#include "metac/metac_refcounter.h"

/*****************************************************************************/
struct condition;
struct discriminator;
struct value_scheme_with_pointer;
struct value_scheme_with_hierarchy;
struct value_scheme_with_array;
/*****************************************************************************/
struct metac_value_scheme;
struct metac_top_value_scheme;
struct metac_object_scheme;
/*****************************************************************************/

struct condition {
	struct discriminator *							p_discriminator;
	metac_discriminator_value_t						expected_discriminator_value;
};

struct value_scheme_with_pointer {
	struct metac_value_scheme *						p_default_child_value_scheme;

	char *											annotation_key;						/* keep annotation_key to use it for cb call */

	/* from annotations:*/
	struct {
		metac_cb_generic_cast_t						cb;
		void *										p_data;
		metac_count_t								types_count;
		struct generic_cast_type{
			struct metac_type *						p_type;
			struct metac_value_scheme *				p_child_value_scheme;
		}*											p_types;
	}generic_cast;

	struct {
		metac_cb_array_elements_count_t				cb;
		void *										p_data;
	}array_elements_count;
};

struct value_scheme_with_array {
	struct metac_value_scheme *						p_child_value_scheme;

	char *											annotation_key;						/* keep annotation_key to use it for cb call */

	/* from annotations:*/
	struct {
		metac_cb_array_elements_count_t				cb;
		void *										p_data;
	}array_elements_count;
};

struct value_scheme_with_hierarchy {													/* struct or union */
	metac_count_t 									members_count;
	struct metac_value_scheme **					members;

	struct discriminator {																/* needed only if it's a union */
		metac_count_t								id;									/* index in the region_element_type. needed to correlate with discriminator_values */
		struct condition							precondition;						/* pre-condition for this discriminator: (copy of precondition from hierarchy_memeber) */

		char *										annotation_key;						/* keep annotation_key to use it for cb call */

		metac_cb_discriminator_t					cb;
		void *										p_data;
	}												union_discriminator;
};

struct metac_value_scheme {
	metac_refcounter_object_t						refcounter_object;					/* we want to make this object refcountable */

	/* info about link to the container */
	struct value_scheme_with_hierarchy *			p_current_hierarchy;

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
			struct metac_value_scheme **			pp_members;							/* full list of members */
		}											hierarchy_top;
	};

	/* content info */
	struct metac_type *								p_type;
	struct metac_type *								p_actual_type;						/* cache: metac_actual_type(p_type) */
	metac_byte_size_t 								byte_size;							/* cache: bite size */

	union {																				/* based on p_actual_type->id */
		struct value_scheme_with_pointer			pointer;
		struct value_scheme_with_array				array;
		struct value_scheme_with_hierarchy			hierarchy;
	};
};

metac_flag_t metac_value_scheme_is_hierachy(
		struct metac_value_scheme *					p_metac_value_scheme);
metac_flag_t metac_value_scheme_is_array(
		struct metac_value_scheme *					p_metac_value_scheme);
metac_flag_t metac_value_scheme_is_pointer(
		struct metac_value_scheme *					p_metac_value_scheme);

metac_flag_t metac_value_scheme_is_hierarchy_member(
		struct metac_value_scheme *					p_metac_value_scheme);
struct metac_value_scheme * metac_value_scheme_get_parent_value_scheme(
		struct metac_value_scheme *					p_metac_value_scheme);

/*****************************************************************************/
struct metac_top_value_scheme {
	struct metac_value_scheme						value_scheme;

	metac_count_t									value_schemes_count;
	struct metac_value_scheme **					pp_value_schemes;
};
/*****************************************************************************/
struct metac_object_scheme {
	struct metac_value_scheme *						p_value_scheme_for_pointer;			/* TODO: can it be element_top_scheme ?*/

	struct metac_top_value_scheme					top_value_scheme;

	metac_count_t									top_value_schemes_count;
	struct metac_top_value_scheme **				pp_top_value_schemes;
};

#endif /* INCLUDE_METAC_METAC_VALUE_SCHEME_H_ */
