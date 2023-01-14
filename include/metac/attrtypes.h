/*
 * metac_attrtypes.h
 *
 *  Created on: Feb 15, 2020
 *      Author: mralex
 */

#ifndef INCLUDE_METAC_METAC_ATTRTYPES_H_
#define INCLUDE_METAC_METAC_ATTRTYPES_H_

#ifdef __cplusplus
extern "C" {
#endif

/* definition of types used for attributes */
typedef char *metac_name_t;
typedef unsigned int metac_byte_size_t;
typedef unsigned int metac_encoding_t;
typedef metac_byte_size_t metac_data_member_location_t; /*make it as metac_byte_size_t - we're multiplying byte_size with index in array*/
typedef unsigned int metac_bit_offset_t;
typedef unsigned int metac_bit_size_t;
typedef unsigned int metac_bound_t; /*make it long? for easier calculations*/
typedef unsigned int metac_count_t;
typedef unsigned long metac_const_value_t;
typedef int metac_flag_t;

typedef int metac_type_kind_t;
typedef int metac_type_at_id_t;
typedef unsigned int metac_num_t;

typedef int metac_discriminator_value_t;

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_METAC_METAC_ATTRTYPES_H_ */
