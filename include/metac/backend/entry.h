/*
 * NOTE: this file is considred as part of internal implementation
 * and may be changed more often than API of the lib.
 * Instead of using those structures, please rely on the API functions.
*/
#ifndef INCLUDE_METAC_BACKEND_ENTRY_H_
#define INCLUDE_METAC_BACKEND_ENTRY_H_

#include "metac/reflect.h"

typedef metac_flag_t(*va_arg_t)(struct va_list_container *p_va_list_container, void * buf);

// NOTE: C++ doesn't treat structs the same way as C if we define struct inside anonymous union

struct typedef_info {
    metac_entry_t *type; /* universal field */
};
struct qual_type_info {
    metac_entry_t *type; /* universal field */
};
struct base_type_info {
    metac_size_t byte_size; /* mandatory field */
    metac_encoding_t encoding; /* type encoding (METAC_ENC_signed etc) */
};
struct pointer_type_info {
    metac_size_t byte_size; /* can be optional */
    metac_entry_t *type; /* universal field */
};
struct reference_type_info {
    metac_size_t byte_size; /* can be optional */
    metac_entry_t *type; /* universal field */
};
struct enumeration_type_info {
    metac_entry_t *type; /* universal field (can be NULL in some compliers) */
    metac_size_t byte_size; /* mandatory field */
    metac_encoding_t encoding;
    metac_num_t enumerators_count; /* mandatory field */
    struct metac_type_enumerator_info * enumerators;
};
struct array_type_info {
    metac_entry_t *type; /* type of elements */
    metac_count_t count; /* length of array in elements. -1 if flexible */
    metac_num_t level; /* just to track multi-dimentional arrays, type will point to the next array */
    metac_bound_t lower_bound; /* min index in subrange (typically 0) */
    metac_size_t *p_stride_bit_size; /* not sure what it is, but it's said "number of bits to hold each element" */
};
struct hierarchy_item {
    metac_entry_t *link; /* link to the actual object (variable, subroutine, )*/
    // where is it declared in cu (if present)
    metac_size_t *p_decl_file; /* number of declfile in compile_unit*/
    metac_size_t *p_decl_line; 
    metac_size_t *p_decl_column;
};
struct metac_member_info { /* define separatelly to make C and C++ behave the same way*/
    metac_entry_t *type; /* type of the member (mandatory) */

    metac_size_t *p_byte_size; /* sometimes it's provided, can be taken from type if not. Can be NULL */
    metac_size_t *p_alignment; /* alignment of the field. Can be NULL */
    metac_offset_t byte_offset; /* location - offset in bytes (mandatory only for structure members, but 0 is ok if not defined) */
    metac_offset_t *p_data_bit_offset; /* data bit offset - new field in DWARF4 instead of p_bit_offset */
    metac_offset_t *p_bit_offset; /* bit offset - used only when bits were specified. Can be NULL */
    metac_size_t *p_bit_size; /* bit size - used only when bits were specified. Can be NULL */
};
struct structure_type_info { // fits to all Classes, Structs and Unions
    metac_size_t byte_size; /* size of the structure in bytes */
    metac_size_t *p_alignment; /* alignment of the struct. Can be NULL */

    va_arg_t p_va_arg_fn;   /* function which is needed to get extract this structure from va_list */

    metac_num_t members_count; /* number of members */
    metac_entry_t *members;

    metac_num_t hierarchy_items_count;
    struct hierarchy_item *hierarchy_items;
};

struct linkage_info {
    metac_flag_t external;
    metac_name_t linkage_name;  // if different from the entry name. typically used for C++
};
struct variable_info {
    metac_entry_t *type;
    struct linkage_info linkage_info;
};
struct lex_block_info {
    metac_num_t hierarchy_items_count;
    struct hierarchy_item *hierarchy_items;
};
struct metac_type_parameter_info {
    metac_flag_t unspecified_parameters; /* if 1 - after that it's possible to have a lot of arguments... */
    metac_entry_t *type; /* parameter type */
};
struct subprogram_info {
    metac_entry_t *type; /* function return type (NULL if void) */

    metac_num_t parameters_count; /* number of parameters */
    metac_entry_t *parameters;

    struct linkage_info linkage_info;
    
    metac_num_t hierarchy_items_count;
    struct hierarchy_item *hierarchy_items;
};
struct subroutine_type_info {
    metac_entry_t *type; /* function return type (NULL if void) */
    metac_num_t parameters_count; /* number of parameters */

    metac_entry_t *parameters;  // will use metac_type_parameter_info
};

struct namespace_info {
    metac_num_t imported_modules_count;
    metac_entry_t **imported_modules;

    metac_num_t imported_declarations_count;
    metac_entry_t **imported_declarations;

    metac_num_t hierarchy_items_count;
    struct hierarchy_item *hierarchy_items;
};
struct compile_unit_info {
    metac_lang_t lang;

    metac_num_t decl_files_count;
    metac_name_t *decl_files;

    metac_num_t imported_modules_count;
    metac_entry_t **imported_modules;

    metac_num_t imported_declarations_count;
    metac_entry_t **imported_declarations;

    metac_num_t hierarchy_items_count;
    struct hierarchy_item *hierarchy_items;
};

struct metac_entry {
    metac_flag_t dynamically_allocated; /* this flag tells if it's necessary to free the entry */

    metac_kind_t kind; /* type kind (base type and etc) */

    metac_name_t name; /* name of type (can be NULL) */
    metac_flag_t declaration; /* =1 if type is incomplete */

    metac_num_t parents_count;
    metac_entry_t **parents; /* parents (for types there can be several compileUnits). needed to C++ to calculate full name of type */

    /* METAC specific data */
    union {
        /* .kind == METAC_KND_typedef */
        struct typedef_info typedef_info;
        /* .kind == METAC_KND_const_type || .kind == METAC_KND_const_type || ... */
        struct qual_type_info qual_type_info;
        /* .kind == METAC_KND_base_type */
        struct base_type_info base_type_info;
        /* .kind == METAC_KND_pointer_type */
        struct pointer_type_info pointer_type_info;
        /* .kind == METAC_KND_enumeration_type */
        struct enumeration_type_info enumeration_type_info;
        /* .kind == METAC_KND_subroutine_type */
        struct subroutine_type_info subroutine_type_info;
        /* .kind == METAC_KND_array_type */
        struct array_type_info array_type_info;
        /* C++: .kind == METAC_KND_reference_type || .kind == METAC_KND_rvalue_reference_type */
        struct reference_type_info reference_type_info;
        // hierarchy kinds
        /* .kind = METAC_KND_member */
        struct metac_member_info member_info;
        /* .kind = METAC_KND_structure_type || .kind == METAC_KND_union_type || C++: .kind == METAC_KND_class_type */
        struct structure_type_info structure_type_info;
         /* .kind == METAC_KND_variable */
        struct variable_info variable_info;
        /* .kind = METAC_KND_func_parameter */
        struct metac_type_parameter_info func_parameter_info;
        /* .kind == METAC_KND_subprogram */
        struct subprogram_info subprogram_info;
        /* .kind == METAC_KND_lex_block */
        struct lex_block_info lex_block_info;
        /* .kind == METAC_KND_namespace */
        struct namespace_info namespace_info;
        /* .kind == METAC_KND_compile_unit */
        struct compile_unit_info compile_unit_info;
    };
};

#endif