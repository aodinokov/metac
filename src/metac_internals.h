/*

* metac_internals.h
 *
 *  Created on: Feb 19, 2018
 *      Author: mralex
 */

#ifndef METAC_INTERNALS_H_
#define METAC_INTERNALS_H_

#include "metac/metac_type.h"
#include "metac/metac_array_info.h"
#include <stdint.h>	/*uint32_t*/

/*****************************************************************************/
struct element_type_hierarchy;
struct element_type_hierarchy_member;
struct element_hierarchy_member;
/*****************************************************************************/
struct condition {
	struct discriminator *							p_discriminator;
	metac_discriminator_value_t						expected_discriminator_value;
};
struct discriminator {
	metac_count_t									id;									/*index in the region_element_type. needed to correlate with discriminator_values */
	struct condition								precondition;						/*pre-condition for this discriminator*/

	struct element_type_hierarchy *					p_hierarchy;						/* weak pointer to hierarchy (union) that needs this discriminator */

	char *											annotation_key;						/*keep annotation_key to use it for cb call*/

	metac_cb_discriminator_t						cb;
	void *											p_data;
};
struct element_type_base_type_hierarchy_member {
	metac_bit_offset_t *							p_bit_offset;						/* bit offset - used only when bits were specified. Can be NULL */
	metac_bit_size_t *								p_bit_size;							/* bit size - used only when bits were specified. Can be NULL */
};
struct element_type_pointer {
	struct element_type *							p_element_type;
	/*keep annotation_key to use it for cb call*/
	char *											annotation_key;
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
	/*keep annotation_key to use it for cb call*/
	char *											annotation_key;
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
	struct metac_type *								p_actual_type;						/* metac_actual_type(p_type) */

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

	metac_flag_t										is_potentially_flexible;

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
/* abstract substitution for pointers */
struct memory_backend_interface;
/*****************************************************************************/
struct memory_pointer {
	struct element *								p_element;
	struct element_hierarchy_member *				p_element_hierarchy_member;			/* only if element is hierarchy(struct/union) */
};
struct memory_block_reference {	/*TODO: rename to memory_block_top_reference*/
	struct memory_block_top	*						p_memory_block_top;
	/*reference info about itself, so it's possible to go in the reverse direction*/
	struct memory_pointer							reference_location;
	/*reference destination and offset are initialized only if pointer was pointing to the place other than beginning of the p_memory_block*/
	struct memory_pointer							reference_destination;
	metac_data_member_location_t					offset;
};
struct memory_block {
	metac_count_t									id;

	/* calculate based on p_parent_memory_block*/
	struct memory_block_top * 						p_memory_block_top;
	metac_data_member_location_t					memory_block_top_offset;

	/* calculate based on local_parent*/
	struct memory_block *							p_parent_memory_block;
	metac_data_member_location_t					parent_memory_block_offset;

	struct memory_pointer							local_parent;

	struct memory_backend_interface *				p_memory_backend_interface;			/* abstract memory access */

	metac_count_t									byte_size;							/* 0 - unknown?*/

	metac_flag_t										is_flexible;						/* was created from flexible array (don't confuse with pointers) */

	metac_array_info_t *							p_array_info;

	metac_count_t									elements_count;						/* calculated from p_array_info */
	struct element *								p_elements;
};
struct element_pointer {
	struct memory_backend_interface *				p_original_memory_backend_interface;/* the interface what was read */

	metac_flag_t										use_cast;
	metac_count_t									generic_cast_type_id;				/* we use callback to get this*/
	struct element_type *							p_casted_element_type;				/* after generic_cast */
	struct memory_backend_interface *				p_casted_memory_backend_interface;	/* the interface what was read */
	metac_data_member_location_t					casted_based_original_offset;		/* the delta between original and casted offsets*/

	metac_num_t										subrange0_count;					/*we use callback to get this*/

	struct memory_block_reference					memory_block_reference;
};
struct element_array {
	struct memory_backend_interface *				p_memory_backend_interface;			/* the interface what was read */

	metac_flag_t										is_flexible;
	metac_num_t										subrange0_count;					/*we use callback to get this*/

	struct memory_block	*							p_memory_block;
};
struct element_hierarchy_member {
	metac_count_t									id;

	struct element_type_hierarchy_member *			p_element_type_hierarchy_member;	/* type description */
	struct element *								p_element;							/* element to which this member belongs to */

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

	struct element_type *							p_element_type;
	struct memory_block *							p_memory_block;						/* memory which this element is part of */

	char *											path_within_memory_block_top;		/* path relevant to the memory_block_top */

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
	struct memory_block								memory_block;						/* block without parents */

	metac_count_t									memory_blocks_count;				/* others */
	struct memory_block	**							pp_memory_blocks;

	metac_count_t									memory_pointers_count;
	struct memory_pointer **						pp_memory_pointers;

	/* answers who is pointing to this memory_block*/
	metac_count_t									memory_block_references_count;
	struct memory_block_reference ** 				pp_memory_block_references;

	metac_flag_t										is_flexible;						/* if contained flexible arrays */
	metac_byte_size_t								flexible_byte_size;					/* flexible part: should be added to the memory_block_top*/
};
struct object_root {
	struct memory_backend_interface * 				p_memory_backend_interface;
	struct memory_block								memory_block_for_pointer;

	metac_count_t									memory_block_tops_count;			/*only memory_blocks without parents*/
	struct memory_block_top	**						pp_memory_block_tops;
};
struct metac_runtime_object {
	struct object_root								object_root;
};
/*****************************************************************************/
struct memory_backend_interface_ops {
	/* abstract destructor */
	int												(*memory_backend_interface_delete)(
			struct memory_backend_interface **			pp_memory_backend_interface);

	/* compare pointers/objects. returns < 0 in case of error, 1 if equals and 0 if not */
	int												(*memory_backend_interface_equals)(
			struct memory_backend_interface *			p_memory_backend_interface0,
			struct memory_backend_interface *			p_memory_backend_interface1);

	/* work with p_element */
	int												(*element_get_memory_backend_interface)(
			struct element *							p_element,
			struct memory_backend_interface **			pp_memory_backend_interface);

	int												(*element_read_memory_backend_interface)(
			struct element *							p_element,
			struct memory_backend_interface **			pp_memory_backend_interface);

	int												(*element_get_array_subrange0)(
			struct element *							p_element);

	int												(*element_get_pointer_subrange0)(
			struct element *							p_element);

	int												(*element_cast_pointer)(
			struct element *							p_element);

	int												(*element_calculate_hierarchy_top_discriminator_values)(
			struct element *							p_element);
	/* work with p_element_hierarchy_member */
	int												(*element_hierarchy_member_get_memory_backend_interface)(
			struct element_hierarchy_member *			p_element_hierarchy_member,
			struct memory_backend_interface **			pp_memory_backend_interface);

	int												(*element_hierarchy_member_read_memory_backend_interface)(
			struct element_hierarchy_member *			p_element_hierarchy_member,
			struct memory_backend_interface **			pp_memory_backend_interface);

	int												(*element_hierarchy_member_get_array_subrange0)(
			struct element_hierarchy_member *			p_element_hierarchy_member);

	int												(*element_hierarchy_member_get_pointer_subrange0)(
			struct element_hierarchy_member *			p_element_hierarchy_member);

	int												(*element_hierarchy_member_cast_pointer)(
			struct element_hierarchy_member *			p_element_hierarchy_member);
	/* work with p_memory_block_top*/
//	int												(*memory_block_top_alloc_memory)(
//			struct memory_block_top *					p_memory_block_top);
	int												(*memory_block_top_free_memory)(
			struct memory_block_top *					p_memory_block_top);
	/* work with p_object_root */
	int												(*object_root_validate)(
			struct object_root *						p_object_root);
};
struct memory_backend_interface {
	uint32_t										_ref_count;
	struct memory_backend_interface_ops *			p_ops;
};
/*****************************************************************************/
int memory_backend_interface(
		struct memory_backend_interface *			p_memory_backend_interface,
		struct memory_backend_interface_ops *		p_memory_backend_interface_ops);
int memory_backend_interface_get(
		struct memory_backend_interface *			p_memory_backend_interface,
		struct memory_backend_interface **			pp_memory_backend_interface);
int memory_backend_interface_put(
		struct memory_backend_interface **			pp_memory_backend_interface);
int memory_backend_interface_equals(
		struct memory_backend_interface *			p_memory_backend_interface0,
		struct memory_backend_interface *			p_memory_backend_interface1);
int element_get_memory_backend_interface(
		struct element *							p_element,
		struct memory_backend_interface **			pp_memory_backend_interface);
int element_read_memory_backend_interface(
		struct element *							p_element,
		struct memory_backend_interface **			pp_memory_backend_interface);
int	element_get_array_subrange0(
		struct element *							p_element);
int element_get_pointer_subrange0(
		struct element *							p_element);
int element_cast_pointer(
		struct element *							p_element);
int element_calculate_hierarchy_top_discriminator_values(
		struct element *							p_element);
int element_hierarchy_member_get_memory_backend_interface(
		struct element_hierarchy_member *			p_element_hierarchy_member,
		struct memory_backend_interface **			pp_memory_backend_interface);
int element_hierarchy_member_read_memory_backend_interface(
		struct element_hierarchy_member *			p_element_hierarchy_member,
		struct memory_backend_interface **			pp_memory_backend_interface);
int element_hierarchy_member_get_array_subrange0(
		struct element_hierarchy_member *			p_element_hierarchy_member);
int element_hierarchy_member_get_pointer_subrange0(
		struct element_hierarchy_member *			p_element_hierarchy_member);
int element_hierarchy_member_cast_pointer(
		struct element_hierarchy_member *			p_element_hierarchy_member);
int object_root_validate(
		struct object_root *						p_object_root);
/*****************************************************************************/
struct memory_backend_interface * memory_backend_interface_create_from_pointer(
		void *										ptr);
/*TODO: change function format to something similar to json ^ (because we don't distinguish error from NULL ptr */
//int memory_backend_interface_create_from_json(
//		json_object *								p_json,
//		struct memory_backend_interface **			pp_memory_backend_interface);
/*****************************************************************************/
struct discriminator * discriminator_create(
		struct metac_type *							p_root_type,
		char * 										global_path,
		metac_type_annotation_t *					p_override_annotations,
		struct discriminator *						p_previous_discriminator,
		metac_discriminator_value_t					previous_expected_discriminator_value,
		struct element_type_hierarchy *				p_hierarchy);
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
struct element_type_hierarchy_member * element_type_hierarchy_get_element_hierarchy_member(
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
struct element_type_hierarchy_member * element_type_hierarchy_member_get_parent_element_hierarchy_member(
		struct element_type_hierarchy_member *		p_element_type_hierarchy_member);
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
/*****************************************************************************/
struct discriminator_value * discriminator_value_create(
		metac_discriminator_value_t					value);
int discriminator_value_delete(
		struct discriminator_value **				pp_discriminator_value);

int memory_block_init(
		struct memory_block *						p_memory_block,
		struct element *							p_local_parent_element,
		struct element_hierarchy_member *			p_local_parent_element_hierarchy_member,
		struct memory_backend_interface *			p_memory_backend_interface,
		struct element_type *						p_element_type,
		metac_array_info_t *						p_array_info,
		metac_flag_t									is_flexible);
struct memory_block * memory_block_create(
		struct element *							p_local_parent_element,
		struct element_hierarchy_member *			p_local_parent_element_hierarchy_member,
		struct memory_backend_interface *			p_memory_backend_interface,
		struct element_type *						p_element_type,
		metac_array_info_t *						p_array_info,
		metac_flag_t									is_flexible);
void memory_block_clean(
		struct memory_block *						p_memory_block);
int memory_block_delete(
		struct memory_block **						pp_memory_block);

int memory_block_top_init(
		struct memory_block_top *					p_memory_block_top,
		char *										global_path,
		struct memory_backend_interface *			p_memory_backend_interface,
		struct element_type *						p_element_type,
		metac_array_info_t *						p_array_info);
void memory_block_top_clean(
		struct memory_block_top *					p_memory_block_top);
struct memory_block_top * memory_block_top_create(
		char *										global_path,
		struct memory_backend_interface *			p_memory_backend_interface,
		struct element_type *						p_element_type,
		metac_array_info_t *						p_array_info);
int memory_block_top_delete(
		struct memory_block_top **					pp_memory_block_top);
int memory_block_top_free_memory(
		struct memory_block_top *					p_memory_block_top);

int object_root_init(
		struct object_root *						p_object_root,
		struct memory_backend_interface *			p_memory_backend_interface,
		struct element_type_top *					p_element_type_top);
void object_root_clean(
		struct object_root *						p_object_root);
int object_root_equals(
		struct object_root *						p_object_root0,
		struct object_root *						p_object_root1);
int object_root_copy(
		struct object_root *						p_object_root,
		struct object_root **						pp_object_root);
int object_root_copy_ex(
		struct object_root *						p_object_root,
		struct memory_backend_interface_ops *		p_memory_backend_interface_ops,				/*it may be we want to have a fabric here */
		struct object_root **						pp_object_root);
int object_root_free_memory(
		struct object_root *						p_object_root);


#endif /* METAC_INTERNALS_H_ */
