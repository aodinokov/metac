/*
 * metac_array_info.h
 *
 *  Created on: Feb 15, 2020
 *      Author: mralex
 */

#ifndef INCLUDE_METAC_METAC_ARRAY_INFO_H_
#define INCLUDE_METAC_METAC_ARRAY_INFO_H_

typedef struct metac_array_info {
	metac_num_t subranges_count;
	struct _metac_array_subrange_info {
		metac_count_t count;
	}subranges[];
}metac_array_info_t;

metac_array_info_t * 	metac_array_info_create_from_type(struct metac_type *type, metac_num_t default_subrange0_count);
metac_array_info_t * 	metac_array_info_create_from_elements_count(metac_count_t elements_count);
metac_array_info_t * 	metac_array_info_copy(metac_array_info_t *p_array_info_orig);
metac_count_t 			metac_array_info_get_element_count(metac_array_info_t * p_array_info);
int						metac_array_info_get_subrange_count(metac_array_info_t *p_array_info, metac_num_t subrange, metac_num_t *p_count);
int						metac_array_info_set_subrange_count(metac_array_info_t *p_array_info, metac_num_t subrange, metac_num_t count);
int 					metac_array_info_is_equal(metac_array_info_t * p_array_info0, metac_array_info_t * p_array_info1);
int 					metac_array_info_delete(metac_array_info_t ** pp_array_info);

metac_array_info_t * 	metac_array_info_create_counter(metac_array_info_t *p_array_info);
int 					metac_array_info_increment_counter(metac_array_info_t *p_array_info, metac_array_info_t *p_array_info_counter);

metac_array_info_t *	metac_array_info_convert_linear_id_2_subranges(metac_array_info_t *p_array_info, metac_num_t linear_id);


#endif /* INCLUDE_METAC_METAC_ARRAY_INFO_H_ */
