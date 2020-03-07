/*
 * metac_value.h
 *
 *  Created on: Mar 5, 2020
 *      Author: mralex
 */

#ifndef INCLUDE_METAC_METAC_VALUE_H_
#define INCLUDE_METAC_METAC_VALUE_H_

#include "metac/metac_scheme.h"
#include "metac/metac_refcounter.h"

#ifdef __cplusplus
extern "C" {
#endif

/*****************************************************************************/
struct value_with_pointer;
struct value_with_array;
struct metac_value_backend_ops;
struct metac_value_backend;																/* abstract substitution for pointers */
struct metac_value;
/*****************************************************************************/

struct metac_value_backend {
	metac_refcounter_object_t						refcounter_object;					/* we want to make this object refcountable */

	struct metac_value_backend_ops *				p_ops;
};

struct metac_value_backend_ops {
	struct metac_refcounter_object_ops				refcounter_object_ops;

	int												(*value_get_value_backend)(
			struct metac_value *						p_value,
			struct metac_value_backend **				pp_memory_value_backend);

	int												(*value_read_value_backend)(
			struct metac_value *						p_value,
			struct metac_value_backend **				pp_memory_value_backend);

	int												(*value_calculate_hierarchy_top_discriminator_value)(
			struct metac_value *						p_value,
			int											id);


};
int metac_value_backend_init(
		struct metac_value_backend *				p_metac_value_backend,
		struct metac_value_backend_ops *			p_metac_value_backend_ops,
		void *										p_private_data);
struct metac_value_backend * metac_value_backend_get(
		struct metac_value_backend *				p_metac_value_backend);
int metac_value_backend_put(
		struct metac_value_backend **				pp_metac_value_backend);

/*****************************************************************************/

struct value_with_pointer {
	struct metac_value_backend *					p_original_value_backend;			/* the interface what was read */

	metac_flag_t									use_cast;
	metac_count_t									generic_cast_type_id;				/* we use callback to get this */
	//struct memory_block_scheme *					p_casted_memory_block_scheme;		/* after generic_cast */
	struct metac_value_backend *					p_casted_value_backend;				/* the interface what was read */
	metac_data_member_location_t					casted_based_original_offset;		/* the delta between original and casted offsets */

	metac_num_t										subrange0_count;					/* we use callback to get this */

	//struct memory_block_top_reference				memory_block_top_reference;
};
struct value_with_array {
	struct metac_value_backend *					p_value_backend;					/* the interface what was read */

	metac_flag_t									is_flexible;
	metac_num_t										subrange0_count;					/*we use callback to get this*/

	struct metac_value	*							p_memory_block;
};
struct value_with_hierarchy {															/* structure or union */
	metac_count_t 									members_count;
	struct metac_value **							members;
};

struct metac_value {
	metac_refcounter_object_t						refcounter_object;					/* we want to make this object refcountable */

//	/* calculate based on p_parent_memory_block */
//	struct top_memory_block * 						p_top_memory_block;
//	metac_data_member_location_t					top_memory_block_offset;

	struct value_with_hierarchy *					p_current_hierarchy;
	union {
		struct _hierarchy_member {														/* if p_current_hierarchy != NULL*/
			metac_count_t							id;
			/*TODO:*/
		}											hierarchy_member;
		struct _hierarchy_top {															/* if p_current_hierarchy == NULL */
			struct discriminator_value {
				metac_discriminator_value_t			value;
			} **									pp_discriminator_values;			/* we use callback to get this */

			struct metac_value **					pp_members;
		}											hierarchy_top;
	};

	struct metac_value_backend *					p_value_backend;					/* abstract memory access (e.g. pointer) */
	struct metac_scheme *							p_scheme;

	/* content */
	union {																				/* based on p_memory_block_scheme->p_actual_type->id */
		struct value_with_pointer					pointer;
		struct value_with_array						array;
		struct value_with_hierarchy					hierarchy;
	};
};

//struct top_memory_block_reference {
//	/*referenced memory_block_top*/
//	struct top_memory_block	*						p_top_memory_block;
//
//	/* reference info about itself, so it's possible to go in the reverse direction */
//	struct memory_block *							p_reference_location;
//
//	/* reference destination and offset are initialized only if pointer was pointing to the place other than beginning of the p_memory_block */
//	struct memory_block *							p_reference_destination;
//	metac_data_member_location_t					offset;
//
//};
//
//struct top_memory_block {
//	metac_count_t									id;
//	struct memory_block								memory_block;						/* block without parents */
//
//	metac_count_t									memory_blocks_with_arrays_count;
//	struct memory_block	**							pp_memory_blocks_with_arrays;
//
//	metac_count_t									memory_blocks_with_pointers_count;
//	struct memory_block **							pp_memory_blocks_with_pointers;
//
//	/* answers who is pointing to this memory_block*/
//	metac_count_t									top_memory_block_references_count;
//	struct top_memory_block_reference ** 			pp_top_memory_block_references;
//
//	metac_flag_t										is_flexible;						/* if contained flexible arrays */
//	metac_byte_size_t								flexible_byte_size;					/* flexible part: should be added to the memory_block_top*/
//};
//struct object {
//	struct memory_backend_interface * 				p_memory_backend_interface;
//	struct memory_block								memory_block_for_pointer;
//
//	metac_count_t									top_memory_blocks_count;			/*only memory_blocks without parents*/
//	struct top_memory_block	**						pp_top_memory_blocks;
//};

#ifdef __cplusplus
}
#endif
#endif /* INCLUDE_METAC_METAC_VALUE_H_ */
