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

/* main kinds of types for metac_type_kind_t */
enum {
    METAC_KND_typedef = 0,
    METAC_KND_const_type,
    METAC_KND_base_type,
    METAC_KND_pointer_type,
    METAC_KND_enumeration_type,
    METAC_KND_subprogram,
    METAC_KND_subroutine_type,
    METAC_KND_structure_type,
    METAC_KND_union_type,
    METAC_KND_array_type,
};
/* supported encodings for metac_encoding_t*/
enum
{
    METAC_ENC_void = 0x0,
    METAC_ENC_address = 0x1,
    METAC_ENC_boolean = 0x2,
    METAC_ENC_complex_float = 0x3,
    METAC_ENC_float = 0x4,
    METAC_ENC_signed = 0x5,
    METAC_ENC_signed_char = 0x6,
    METAC_ENC_unsigned = 0x7,
    METAC_ENC_unsigned_char = 0x8,
    METAC_ENC_imaginary_float = 0x9,
    METAC_ENC_packed_decimal = 0xa,
    METAC_ENC_numeric_string = 0xb,
    METAC_ENC_edited = 0xc,
    METAC_ENC_signed_fixed = 0xd,
    METAC_ENC_unsigned_fixed = 0xe,
    METAC_ENC_decimal_float = 0xf,
    METAC_ENC_UTF = 0x10,
    METAC_ENC_UCS = 0x11,
    METAC_ENC_ASCII = 0x12,
    METAC_ENC_lo_user = 0x80,
    METAC_ENC_hi_user = 0xff
};

/* metac_name_t value if name is anonymous (e.g. anonymous struct inside another struct) */
#define METAC_ANON_MEMBER_NAME ""

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_METAC_METAC_ATTRTYPES_H_ */
