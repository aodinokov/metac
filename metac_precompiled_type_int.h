/*
 * metac_precompiled_type_int.h
 *
 *  Created on: Feb 19, 2018
 *      Author: mralex
 */

#ifndef METAC_PRECOMPILED_TYPE_INT_H_
#define METAC_PRECOMPILED_TYPE_INT_H_

struct region_element_type;
struct discriminator;
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

	struct metac_type * type;					/*type without intermediate types (typdefs, consts &etc)*/
	struct region_element_type_element * parent;		/*pointer to the parent element (structure/union)*/

	metac_data_member_location_t offset;		/*offset in local region element*/
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

struct discriminator_value {
	int is_initialized;
	metac_discriminator_value_t value;
};

struct pointer {
	int n;		/*1-dimention*/
	metac_count_t * p_elements_count;

	struct region* p_region;
};

struct array {
	int n;
	metac_count_t * p_elements_count;

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

struct region { /*can contain several elements of region_element_type*/
	void *ptr;
	metac_byte_size_t byte_size;

	struct region * part_of_region; /*	sometimes pointers in one structs point not to the beginning of the region -
									there is a tricky algorithm to find this. Also this is a common situation for arrays in structs*/

//	struct region_element_type * 	region_element_type;
	metac_count_t 			elements_count;
	struct region_element * elements;
};


struct metac_runtime_object {
	struct metac_precompiled_type *precompiled_type;

	int	regions_count;
	struct region ** region;


	int is_tree;	/* during evaluation this value will be updated to reflect if it's possible to serialize this object as tree*/
};


#endif /* METAC_PRECOMPILED_TYPE_INT_H_ */
