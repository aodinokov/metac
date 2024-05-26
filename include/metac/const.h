/*
 * const.h
 *
 *  Created on: Feb 15, 2020
 *      Author: mralex
 */

#ifndef INCLUDE_METAC_CONST_H_
#define INCLUDE_METAC_CONST_H_

#ifdef __cplusplus
extern "C" {
#endif

/** @brief main kinds of types for metac_type_kind_t */
enum {
    METAC_KND_invalid = -1,
    METAC_KND_typedef = 0,    
    METAC_KND_base_type,
    METAC_KND_pointer_type,
    METAC_KND_enumeration_type,
    METAC_KND_subroutine_type,
    METAC_KND_array_type,
    /* qual types */
    METAC_KND_const_type,
    METAC_KND_volatile_type,
    METAC_KND_restrict_type,
    METAC_KND_mutable_type,
    METAC_KND_shared_type,
    METAC_KND_packed_type,
    /* C++ specific type kinds */
    METAC_KND_reference_type,
    METAC_KND_rvalue_reference_type,
    METAC_KND_unspecified_type,
    /* hierarchy types */
    /*   structs  */
    METAC_KND_member, /* separated into entry so it could be convenient for value type implementation */
    METAC_KND_struct_type,
    METAC_KND_union_type,
    METAC_KND_class_type, /* C++ specific type kind */
    METAC_KND_variable,
    METAC_KND_subprogram_parameter,
    METAC_KND_subprogram,
    METAC_KND_lex_block,
    METAC_KND_namespace,  /* C++ specific type kind */
    METAC_KND_compile_unit,
};

/** @brief supported encodings for metac_encoding_t */
enum {
    METAC_ENC_undefined = 0,
    METAC_ENC_void,
    METAC_ENC_address,
    METAC_ENC_boolean,
    METAC_ENC_complex_float,
    METAC_ENC_float,
    METAC_ENC_signed,
    METAC_ENC_signed_char,
    METAC_ENC_unsigned,
    METAC_ENC_unsigned_char,
    METAC_ENC_imaginary_float,
    METAC_ENC_packed_decimal,
    METAC_ENC_numeric_string,
    METAC_ENC_edited,
    METAC_ENC_signed_fixed,
    METAC_ENC_unsigned_fixed,
    METAC_ENC_decimal_float,
    METAC_ENC_utf,
    METAC_ENC_ucs,
    METAC_ENC_ascii,
};

/** @brief supported languages for metac_lang_t */
enum {
	METAC_LANG_C89           = 0x1,
	METAC_LANG_C             = 0x2,
	METAC_LANG_Ada83         = 0x3,
	METAC_LANG_C_plus_plus   = 0x4,
	METAC_LANG_Cobol74       = 0x5,
	METAC_LANG_Cobol85       = 0x6,
	METAC_LANG_Fortran77     = 0x7,
	METAC_LANG_Fortran90     = 0x8,
	METAC_LANG_Pascal83      = 0x9,
	METAC_LANG_Modula2       = 0xa,
	METAC_LANG_Java          = 0xb,
	METAC_LANG_C99           = 0xc,
	METAC_LANG_Ada95         = 0xd,
	METAC_LANG_Fortran95     = 0xe,
	METAC_LANG_PLI           = 0xf,
	METAC_LANG_ObjC          = 0x10,
	METAC_LANG_ObjC_plus_plus= 0x11,
	METAC_LANG_UPC           = 0x12,
	METAC_LANG_D             = 0x13,
	METAC_LANG_Python        = 0x14,
	METAC_LANG_OpenCL        = 0x15,
	METAC_LANG_Go            = 0x16,
	METAC_LANG_Modula3       = 0x17,
	METAC_LANG_Haskell       = 0x18,
	METAC_LANG_C_plus_plus_03= 0x19,
	METAC_LANG_C_plus_plus_11= 0x1a,
	METAC_LANG_OCaml         = 0x1b,
	METAC_LANG_Rust          = 0x1c,
	METAC_LANG_C11           = 0x1d,
	METAC_LANG_Swift         = 0x1e,
	METAC_LANG_Julia         = 0x1f,
	METAC_LANG_Dylan         = 0x20,
	METAC_LANG_C_plus_plus_14= 0x21,
	METAC_LANG_Fortran03     = 0x22,
	METAC_LANG_Fortran08     = 0x23,
	METAC_LANG_RenderScript  = 0x24,
	METAC_LANG_BLISS         = 0x25,
};

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_METAC_CONST_H_ */
