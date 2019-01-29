/*
 * metac_internals.h
 *
 *  Created on: Feb 19, 2018
 *      Author: mralex
 */

#ifndef METAC_INTERNALS_H_
#define METAC_INTERNALS_H_

#include "metac_type.h"

struct discriminator;
struct region_element_type;

struct condition {
	struct discriminator *		p_discriminator;
	metac_discriminator_value_t	expected_discriminator_value;
};

struct discriminator {
	struct condition precondition;				/*pre-condition for this discriminator*/

	int id;											/* index in the region_element_type. needed to correlate with discriminator_values */
	metac_discriminator_cb_ptr_t discriminator_cb;
	void * discriminator_cb_context;
};

struct region_element_type_element {
	struct condition precondition;			/*precondition for this region element*/

	int id;										/* index in the region_element_type. needed to find parents quickier */
	struct metac_type * type;					/*type without intermediate types (typdefs, consts &etc)*/
	struct region_element_type_element * parent;		/*pointer to the parent element (structure/union)*/

	metac_data_member_location_t offset;		/*offset in local region element*/
	metac_bit_offset_t *	p_bit_offset;		/* bit offset - used only when bits were specified. Can be NULL */
	metac_bit_size_t *		p_bit_size;			/* bit size - used only when bits were specified. Can be NULL */
	metac_byte_size_t byte_size;				/*bite size*/

	char *	name_local;							/* member name if exists */
	char *	path_within_region_element;					/* path in local region element*/
	char *	path_global;						/* path starting from the initial type via pointers etc*/

	/*** used only if element is pointer or array - we create separate region element for that and length will be identified in runtime ***/
	metac_array_elements_count_cb_ptr_t
			array_elements_count_funtion_ptr;	/*  if pointers or arrays - overrides byte_size - in case of non-flexible arrays -
													will use a special func by default*/
	void *	array_elements_count_cb_context;
	struct region_element_type *
			array_elements_region_element_type;			/* region_element_type of elements or pointer */
};

/*region type can be used by arrays and pointers. depending on that it may or may not represent really allocated space*/
struct region_element_type {
	struct metac_type *type;

	int discriminators_count;
	struct discriminator ** discriminator;

	int elements_count;
	struct region_element_type_element ** element;

	/*to navigate easily store begin/count for every region element type */
	struct region_element_type_element ** hierarchy_element;	/* structs/unions */
	int hierarchy_elements_count;
	struct region_element_type_element ** base_type_element;
	int base_type_elements_count;
	struct region_element_type_element ** enum_type_element;
	int enum_type_elements_count;
	struct region_element_type_element ** pointer_type_element;
	int pointer_type_elements_count;
	struct region_element_type_element ** array_type_element;
	int array_type_elements_count;
};

struct metac_precompiled_type {
	struct metac_type *type;

	int	region_element_types_count;
	struct region_element_type ** region_element_type;
};
/*****************************************************************************/
struct discriminator_value {
	int is_initialized;
	metac_discriminator_value_t value;
};

struct pointer {
	metac_array_info_t * p_array_info;
	struct region* p_region;
};

struct array {
	metac_array_info_t * p_array_info;
	struct region* p_region;
};

struct region_element {
	void *ptr;
	metac_byte_size_t byte_size;
	struct region_element_type * region_element_type;

	/* initialized based on region_element_type numbers of each element */
	struct discriminator_value * p_discriminator_value;
	struct pointer * p_pointer;
	struct array * p_array;
};

struct _location {
	metac_count_t region_idx;
	metac_data_member_location_t offset; /*< offset within region*/
};

struct region { /*can contain several elements of region_element_type*/
	metac_count_t id;
	void *ptr;
	metac_byte_size_t byte_size;

	metac_count_t 			elements_count;
	struct region_element * elements;

	struct region * part_of_region; /*	sometimes pointers in one structs point not to the beginning of the region -
									there is a tricky algorithm to find this. Also this is a common situation for arrays in structs*/

	int 			 unique_region_id; /*will be -1 for non-unique*/
	struct _location location;
};

struct metac_runtime_object {
	struct metac_precompiled_type *precompiled_type;

	metac_count_t	regions_count;
	struct region ** region;

	/* really allocated memory regions (subset of all regions) */
	metac_count_t	unique_regions_count;
	struct region ** unique_region;
};

/*****************************************************************************/

struct metac_precompiled_type * create_precompiled_type(struct metac_type * type);
int delete_precompiled_type(struct metac_precompiled_type ** pp_precompiled_type);

struct region_element_type * create_region_element_type(struct metac_type * type);
int delete_region_element_type(struct region_element_type ** pp_region_element_type);

struct region_element_type_element * create_region_element_type_element(
		struct metac_type * type,
		struct discriminator * p_discriminator,
		metac_discriminator_value_t expected_discriminator_value,
		metac_data_member_location_t offset,
		metac_byte_size_t byte_size,
		struct region_element_type_element * parent,
		char *	name_local,
		char *	path_within_region,
		char *	path_global,
		metac_array_elements_count_cb_ptr_t array_elements_count_funtion_ptr,
		void *	array_elements_count_cb_context,
		struct region_element_type * array_elements_region_element_type);
int delete_region_element_type_element(
		struct region_element_type_element **pp_region_element_type_element);
void update_region_element_type_element_array_params(
		struct region_element_type_element *p_region_element_type_element,
		metac_array_elements_count_cb_ptr_t array_elements_count_funtion_ptr,
		void *	array_elements_count_cb_context,
		struct region_element_type * array_elements_region_element_type);

struct discriminator * create_discriminator(
		struct discriminator * p_previous_discriminator,
		metac_discriminator_value_t previous_expected_discriminator_value,
		metac_discriminator_cb_ptr_t discriminator_cb,
		void * discriminator_cb_context);
int delete_discriminator(struct discriminator ** pp_discriminator);

/*****************************************************************************/
struct metac_runtime_object * create_runtime_object(
		struct metac_precompiled_type * p_precompiled_type);
int free_runtime_object(
		struct metac_runtime_object ** pp_runtime_object);

struct region * create_region(
		void *ptr,
		metac_byte_size_t byte_size,
		struct region_element_type * region_element_type,
		metac_count_t elements_count,
		struct region * part_of_region);
int delete_region(struct region **pp_region);
int update_region_ptr_and_size(
		struct region *p_region,
		void *ptr,
		metac_byte_size_t byte_size);

static inline metac_count_t get_region_unique_region_id(
		struct region * p_region) {
	if (p_region->part_of_region)
		return p_region->location.region_idx;
	return p_region->unique_region_id;
}

int init_region_element(
		struct region_element *p_region_element,
		void *ptr,
		metac_byte_size_t byte_size,
		struct region_element_type * region_element_type);
int cleanup_region_element(struct region_element *p_region_element);
int is_region_element_precondition_true(
		struct region_element * p_region_element,
		struct condition * p_precondition);
int set_region_element_precondition(
		struct region_element * p_region_element,
		struct condition * p_precondition);
int write_region_element_discriminators(
		struct region_element * p_region_element);

#endif /* METAC_INTERNALS_H_ */
