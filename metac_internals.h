/*
 * metac_internals.h
 *
 *  Created on: Feb 19, 2018
 *      Author: mralex
 */

#ifndef METAC_INTERNALS_H_
#define METAC_INTERNALS_H_

#include "metac_type.h"

/*****************************************************************************/
struct element_type_hierarchy_member;
struct element_hierarchy_member;
struct array;
/*****************************************************************************/
struct condition {
	struct discriminator *							p_discriminator;
	metac_discriminator_value_t						expected_discriminator_value;
};
struct discriminator {
	metac_count_t									id;									/*index in the region_element_type. needed to correlate with discriminator_values */
	struct condition								precondition;						/*pre-condition for this discriminator*/
	metac_cb_discriminator_t						cb;
	void *											p_data;
};
struct element_type_base_type_hierarchy_member {
	metac_bit_offset_t *							p_bit_offset;						/* bit offset - used only when bits were specified. Can be NULL */
	metac_bit_size_t *								p_bit_size;							/* bit size - used only when bits were specified. Can be NULL */
};
struct element_type_pointer {
	struct element_type *							p_element_type;
	/* from annotations:*/
	struct {
		metac_cb_generic_cast_t						cb;
		void *										p_data;
		metac_count_t								types_count;
		struct generic_cast_type{
			struct metac_type *						p_type;
			struct element_type *					p_element_type;
		}*											p_types;
	}generic_cast;
	struct {
		metac_cb_array_elements_count_t				cb;
		void *										p_data;
	}array_elements_count;
};
struct element_type_array {
	struct element_type *							p_element_type;
	/* from annotations:*/
	struct {
		metac_cb_array_elements_count_t				cb;
		void *										p_data;
	}array_elements_count;
};
struct element_type_hierarchy {															/* struct or union*/
	struct element_type_hierarchy *					parent;
	metac_count_t 									members_count;
	struct element_type_hierarchy_member **
													members;
};
struct element_type_hierarchy_member {
	metac_count_t									id;									/*index in the element_type_hierarhy_top. needed to find parents quickier */
	struct condition 								precondition;						/*precondition for this region element*/

	struct element_type_hierarchy *					p_hierarchy;
	metac_count_t									member_id;							/* member id within parent hierarchy*/
	char *											path_within_hierarchy;
	struct metac_type_member_info *					p_member_info;

	metac_data_member_location_t					offset;

	struct metac_type *								p_type;
	struct metac_type *								p_actual_type;

	metac_byte_size_t 								byte_size;							/*bite size*/

	union {
		struct element_type_base_type_hierarchy_member
													base_type;
		struct element_type_pointer					pointer;
		struct element_type_array					array;
		struct element_type_hierarchy				hierarchy;
	};
};
struct element_type {																	/*array element type*/
	struct metac_type *								p_type;
	struct metac_type *								p_actual_type;						/*cached: actual type without typedefs and etc */

	metac_byte_size_t 								byte_size;							/*cached: element bite size*/

	metac_flag										is_potentially_flexible;

	union {	/*depending on actual_type->id*/
		struct element_type_pointer					pointer;
		struct element_type_array					array;
		struct element_type_hierarchy_top {
			metac_count_t 							discriminators_count;
			struct discriminator **					pp_discriminators;

			metac_count_t 							members_count;
			struct element_type_hierarchy_member **	pp_members;							/* full list of members*/

			struct element_type_hierarchy			hierarchy;
		}hierarchy_top;
	};
};
struct element_type_top {
	struct metac_type *								p_pointer_type;
	struct element_type *							p_element_type_for_pointer;

	metac_count_t									element_types_count;
	struct element_type **							pp_element_types;
};
struct metac_precompiled_type {
	struct element_type_top							element_type_top;
};

/*****************************************************************************/
struct memory_pointer {
	struct element *								p_element;
	struct element_hierarchy_member *				p_element_hierarchy_member;			/* only if element is hierarchy(struct/union) */
};
struct memory_block_reference {
	struct memory_pointer							reference_location;

	struct memory_block	*							p_memory_block;						/* original memory_block */

	/*????depends on how we recalculate it */
	/*reference destination and offset are initialized only if pointer was pointing to the place other than beginning of the p_memory_block*/
	struct memory_pointer							reference_destination;
	metac_data_member_location_t					offset;
};
struct memory_block {
	metac_count_t									id;

	struct memory_block_top * 						p_memory_block_top;
	metac_data_member_location_t					memory_block_top_offset;

	struct memory_block *							p_parent_memory_block;
	metac_data_member_location_t					parent_memory_block_offset;

	void * 											ptr;								/*real mem - not always initialized*/
	metac_count_t									byte_size;							/* 0 - unknown?*/

	metac_count_t									elements_count;
	struct element *								p_elements;

	/* answers who is pointing to this memory_block*/
	metac_count_t									memory_block_references_count;
	struct memory_block_reference ** 				pp_memory_block_references;
};
struct element_pointer {
	metac_flag										use_cast;
	metac_count_t									generic_cast_type_id;				/*we use callback to get this*/

	void *											ptr;								// TODO: maybe offset is enough? another option - additional range
	void *											actual_ptr;							/* after generic_cast if it's a case */

	metac_num_t										subrange0_count;					/*we use callback to get this*/
	metac_array_info_t *							p_array_info;

	struct memory_block_reference					memory_block_reference;
};
struct element_array {
	metac_num_t										subrange0_count;					/*we use callback to get this*/
	metac_array_info_t *							p_array_info;
	void *											actual_ptr;							/* after generic_cast if it's a case */

	struct memory_block	*							p_memory_block;
};
struct element_hierarchy_member {
	metac_count_t									id;

	struct element_type_hierarchy_member *			p_element_type_hierarchy_member;
	struct element *								p_element;

	union {
		struct element_pointer						pointer;
		struct element_array						array;
	};
};
struct discriminator_value {
	metac_discriminator_value_t						value;
};
struct element {
	metac_count_t									id;
	//struct memory_pointer							local_parent;						/* TODO: reconsider moving this to this to memory_block. parent within one memory_block_top */

	struct element_type *							p_element_type;
	struct memory_block *							p_memory_block;						/* memory which this element is part of */

	union {
		struct element_pointer						pointer;
		struct element_array						array;
		struct element_hierarchy_top {
			struct discriminator_value **			pp_discriminator_values;			/*we use callback to get this*/
			struct element_hierarchy_member **		pp_members;
		}											hierarchy_top;
	};
};
struct memory_block_top {
	metac_count_t									id;
	struct memory_block	*							p_memory_block;						/* block without parents */

	metac_count_t									memory_blocks_count;				/* others */
	struct memory_block	**							pp_memory_blocks;

	metac_count_t									pointers_count;
	struct memory_pointer **						pp_memory_pointers;
};
struct object_root {
	void * 											ptr;
	struct memory_block								memory_block_for_pointer;

	metac_count_t									memory_block_tops_count;			/*only memory_blocks without parents*/
	struct memory_block_top	**						pp_memory_block_tops;

	/*pointers to ranges can be calculated via mem_blocks if we count all originating info??? anyway we'll get the list of all pointers*/
};
struct metac_runtime_object {
	struct object_root								object_root;
};
/*****************************************************************************/
struct discriminator * discriminator_create(
		struct metac_type *							p_root_type,
		char * 										global_path,
		metac_type_annotation_t *					p_override_annotations,
		struct discriminator *						p_previous_discriminator,
		metac_discriminator_value_t					previous_expected_discriminator_value);
int discriminator_delete(
		struct discriminator **						pp_discriminator);
int element_type_array_init(
		struct metac_type *							p_root_type,
		char * 										global_path,
		metac_type_annotation_t *					p_override_annotations,
		struct metac_type *							p_actual_type,
		struct element_type_array *					p_element_type_array);
void element_type_array_clean(
		struct element_type_array *					p_element_type_array);
int element_type_pointer_init(
		struct metac_type *							p_root_type,
		char * 										global_path,
		metac_type_annotation_t *					p_override_annotations,
		struct metac_type *							p_actual_type,
		struct element_type_pointer *				p_element_type_pointer);
void element_type_pointer_clean(
		struct element_type_pointer *				p_element_type_pointer);
int element_type_hierarchy_init(
		struct metac_type *							p_root_type,
		char * 										global_path,
		metac_type_annotation_t *					p_override_annotations,
		struct metac_type *							p_actual_type,
		struct element_type_hierarchy *				p_element_type_hierarchy,
		struct element_type_hierarchy *				p_parent_hierarchy);
void element_type_hierarchy_clean(
		struct element_type_hierarchy *				p_element_type_hierarchy);
struct element_type_hierarchy_member * element_type_hierarchy_member_create(
		struct metac_type *							p_root_type,
		char *										global_path,
		char *										hirarchy_path,
		metac_type_annotation_t *					p_override_annotations,
		metac_count_t								member_id,
		struct discriminator *						p_discriminator,
		metac_discriminator_value_t					expected_discriminator_value,
		struct element_type_hierarchy *				p_hierarchy,
		struct metac_type_member_info *				p_member_info);
int element_type_hierarchy_member_delete(
		struct element_type_hierarchy_member **		pp_element_type_hierarchy_member);
struct element_type * element_type_create(
		struct metac_type *							p_root_type,
		char *										global_path,
		metac_type_annotation_t *					p_override_annotations,
		struct metac_type *							p_type);
int element_type_delete(
		struct element_type **						pp_element_type);
int element_type_top_init(
		struct metac_type *							p_root_type,
		char *										global_path,
		metac_type_annotation_t *					p_override_annotations,
		struct element_type_top *					p_element_type_top);
void element_type_top_clean(
		struct element_type_top *					p_element_type_top);

#if 0
struct region_element_type_member {
	int 							id;							/*index in the region_element_type. needed to find parents quickier */
	struct condition 				precondition;				/*precondition for this region element*/

	struct metac_type *				type;						/*type without intermediate types (typdefs, consts &etc)*/
	struct region_element_type_member *
									parent;						/*pointer to the parent element (structure/union)*/

	metac_data_member_location_t	offset;						/*offset in local region element*/
	metac_bit_offset_t *			p_bit_offset;				/* bit offset - used only when bits were specified. Can be NULL */
	metac_bit_size_t *				p_bit_size;					/* bit size - used only when bits were specified. Can be NULL */
	metac_byte_size_t 				byte_size;					/*bite size*/

	char *							name_local;					/* member name if exists */
	char *							path_within_region_element;	/* path in local region element*/
	char *							path_global;				/* path starting from the initial type via pointers etc*/

	metac_num_t						member_id;					/* member*/
	int								is_flexible;				/* flexible member flag */

	/*** used only if element is pointer or array - we create separate region element for that and length will be identified in runtime ***/
	metac_array_elements_count_cb_ptr_t
									array_elements_count_funtion_ptr;	/*  if pointers or arrays - overrides byte_size - in case of non-flexible arrays -
													will use a special func by default*/
	void *							array_elements_count_cb_context;
	struct region_element_type *
									array_elements_region_element_type;	/* region_element_type of elements or pointer */
};

/*region type can be used by arrays and pointers. depending on that it may or may not represent really allocated space*/
struct region_element_type {
	struct metac_type *				type;

	int 							discriminators_count;
	struct discriminator **			discriminator;
	int 							members_count;
	struct region_element_type_member **
									members;
	/*to navigate easily store begin/count for every region element type */
	int								hierarchy_members_count;
	struct region_element_type_member **
									hierarchy_members;	/* structs/unions */
	int								base_type_members_count;
	struct region_element_type_member **
									base_type_members;
	int								enum_type_members_count;
	struct region_element_type_member **
									enum_type_members;
	int								pointer_type_members_count;
	struct region_element_type_member **
									pointer_type_members;
	int 							array_type_elements_count;
	struct region_element_type_member **
									array_type_members;
};

struct metac_precompiled_type {
	struct metac_type *				type;

	struct _region_element_types_array{
		int								region_element_types_count;
		struct region_element_type **	region_element_type;
	}	pointers,
		arrays;
};
/*****************************************************************************/
struct region2;

struct discriminator_value {
	int								is_initialized;
	metac_discriminator_value_t		value;
};

struct pointer {
	union {
		struct region *					p_region;
		struct region2 *				p_region2;
	};
};

struct array {
	union {
		struct region *					p_region;
		struct region2 *				p_region2;
	};
};

struct region_element {
	void *							ptr;
	metac_byte_size_t 				byte_size;
	struct region_element_type *	region_element_type;

	/* initialized based on region_element_type numbers of each element */
	struct discriminator_value *	p_discriminator_value;
	struct pointer *				p_pointer;
	struct array *					p_array;
};

struct _location {
	metac_count_t					region_idx;
	metac_data_member_location_t	offset; /*< offset within region*/
};

struct region { /*can contain several elements of region_element_type*/
	metac_count_t					id;
	void *							ptr;
	metac_byte_size_t				byte_size;

	metac_array_info_t *			p_array_info;
	metac_count_t 					elements_count;			/*elements_count - cached value of metac_array_info_get_element_count(p_array_info)*/
	struct region_element *			elements;

	struct region *					part_of_region; 		/*	sometimes pointers in one structs point not to the beginning of the region -
																there is a tricky algorithm to find this. Also this is a common situation for arrays in structs*/

	int								unique_region_id; 		/*will be -1 for non-unique*/
	struct _location				location;
};

struct metac_runtime_object {
	struct metac_precompiled_type *	precompiled_type;

	int use_region2;

	metac_count_t					regions_count;
	union {
		struct region **				region;
		struct region2 **				region2;
	};

	/* really allocated memory regions (subset of all regions) */
	metac_count_t					unique_regions_count;
	union {
		struct region **				unique_region;
		struct region2 **				unique_region2;
	};
};

/*****************************************************************************/

struct metac_precompiled_type * create_precompiled_type(struct metac_type * type);
int delete_precompiled_type(struct metac_precompiled_type ** pp_precompiled_type);

struct region_element_type * create_region_element_type(struct metac_type * type);
int delete_region_element_type(struct region_element_type ** pp_region_element_type);

struct region_element_type_member * create_region_element_type_member(
		struct metac_type * type,
		struct discriminator * p_discriminator,
		metac_discriminator_value_t expected_discriminator_value,
		metac_num_t member_id,
		metac_data_member_location_t offset,
		metac_bit_offset_t * p_bit_offset,
		metac_bit_size_t * p_bit_size,
		metac_byte_size_t byte_size,
		struct region_element_type_member * parent,
		char *	name_local,
		char *	path_within_region,
		char *	path_global,
		metac_array_elements_count_cb_ptr_t array_elements_count_funtion_ptr,
		void *	array_elements_count_cb_context,
		struct region_element_type * array_elements_region_element_type);
int delete_region_element_type_member(
		struct region_element_type_member **pp_region_element_type_element);
void update_region_element_type_member_array_params(
		struct region_element_type_member *p_region_element_type_element,
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
		metac_array_info_t * p_array_info,
		struct region * part_of_region);
int delete_region(struct region **pp_region);
int update_region_ptr(
		struct region *p_region,
		void *ptr);

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


struct region2_link {
	struct region2 *						p_region;
	struct region_element *					p_region_element;
	struct region_element_type_member *		p_member;
};

/* new API */
struct region2 {
	metac_count_t							region_id;

	struct region_element_type *			p_region_element_type;	/* region is array of elements of the same type. p_region_element_type is a type of one element*/
	metac_array_info_t *					p_array_info;			/* region is array of elements. p_array_info represents n-dimensions array */

	metac_byte_size_t						byte_size;				/* total byte_size of the region*/
	metac_byte_size_t						byte_size_flexible;		/* flexible part (if region is flexible:
																	 * this is possible only if element_number == 1),
																	 * and p_region_element_type has an array as a member marked as 'is_flexible'
															 */
	metac_count_t 							elements_count;			/* elements_count - cached value of metac_array_info_get_element_count(p_array_info)*/
	struct region_element *					elements;

	metac_count_t 							links_count;
	struct region2_link *					links;					/* who linked this region? (p_member->type can be pointer or array)
																	 * in case of array there is always only 1 parent - upper structure
																	 * in case of pointers - there can be several pointers
																	 */
	int 									is_allocated_region;	/* the region isn't a part of other region. it owns the pointer (unique region)*/
	union {
		metac_count_t						allocated_region_id;	/* valid only if is_allocated_region is true */
		struct {
			struct region2_link			member;					/* member of the parent structure that has the closest address
																	 * (ideally the same) to the beginning of this region.
																	 * */
			metac_data_member_location_t	offset;					/* if there is no member in parent structure with this address - we use offset
																	 * (keep in mind, this is not cross-platform compatible)
																	 */
		}									parent_info;			/* valid only if is_allocated_region is false */
	};
	void *									ptr;					/* ptr where the region is located (can be NULL initially), e.g. when we build
																	 * memory object from json and etc.
																	 */
};

/* create region_v2
 * result - 0 - ok, 1- skip(??? in what cases? - in cases element count is 0), -1-error
 */
int
create_region2(
		struct region_element_type * p_region_element_type,
		metac_array_info_t * p_array_info,
		void * ptr,
		struct region2 ** pp_region);
/* delete region_v2
 */
int
delete_region2(
		struct region2 **pp_region);
/*
 * update region_v2 ptr
 */
int
update_region2_ptr(
		struct region2 * p_region,
		void * ptr);
/*
 * update region_v2 params if it's not allocated_region
 */
int
update_region2_non_allocated(
		struct region2 * p_region,
		struct region2 * p_region_parent,
		struct region_element * p_region_element_parent,
		struct region_element_type_member * p_member_parent,
		metac_data_member_location_t offset_parent
		);
/*
 * update region_v2 with new byte_size_flexible and re-calculate byte_size
 */
int
update_region2_flexible_bytesize(
		struct region2 * p_region,
		metac_byte_size_t byte_size_flexible);
/*
 * get region_v2 non-flexible part byte_size
 */
metac_byte_size_t
get_region2_static_bytesize(
		struct region2 * p_region);
#endif

#endif /* METAC_INTERNALS_H_ */
