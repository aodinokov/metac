#ifndef INCLUDE_METAC_ENTRY_H_
#define INCLUDE_METAC_ENTRY_H_

#include "metac/base.h"

/** @brief entry - contains info about objects like vars, subprograms(functions). defined in the implementation */
typedef struct metac_entry metac_entry_t;

/**
 * @brief Returns entry declaration in C-like format.
 * Note: For non Variables, Member fields or function parameters the string will have %s as a placeholder for the object name.
 * @return Dynamically allocated string with declaration. Has to be deallocated with free after usage.
 */
char * metac_entry_cdecl(metac_entry_t * p_entry);

/**
 * @brief Returns kind of entry. see metac/const.h all METAC_KND_ values.
 */
metac_kind_t metac_entry_kind(metac_entry_t * p_entry);
/**
 * @brief Returns name of entry. depending on Kind it may mean different things.
 */
metac_name_t metac_entry_name(metac_entry_t * p_entry);

/**
 * @brief Calculates entry size and returns to p_sz
 * @param p_entry - entry
 * @param p_sz - address of the memory where the calculated value is stored, can't be NULL.
 * @returns negative - errno in case of error. 0 - otherwise.
 */
int metac_entry_byte_size(metac_entry_t *p_entry, metac_size_t *p_sz);

/**
 * @brief Number of parents of the entry
 * Entries typically stored in hierarchy. top level is compile unit (CU). 
 * If the same type is met in several CU - it will have several parents - each hiearchy for each CU.
 */
metac_num_t metac_entry_parent_count(metac_entry_t * p_entry);
/**
 * @brief get parent entry
 * Entries typically stored in hierarchy. top level is compile unit (CU). 
 * If the same type is met in several CU - it will have several parents - each hiearchy for each CU.
 * @param p_entry - entry
 * @param id in range [0; metac_entry_parent_count(p_entry)-1]
 */
metac_entry_t * metac_entry_parent_entry(metac_entry_t * p_entry, metac_num_t id);

/** @brief structure to keep intermittent qualifiers kinds on the way to final entry (see metac_entry_final_entry) */
typedef union { 
    struct {
        unsigned short flag_const:1;
        unsigned short flag_volatile:1;
        unsigned short flag_restrict:1;
        unsigned short flag_mutable:1;
        unsigned short flag_shared:1;
        unsigned short flag_packed:1;
    };
    unsigned short flags; /**< representation as a single short value */
} metac_quals_t;
/**
 * @brief get final entry for some entry
 * Final entries are: base type, enum, pointer, array, structure/union. All others are not final
 * @param p_entry - entry
 * @param p_quals - optional, can be NULL. function put flag to non 0 if it meats the corresponding qualifier on the way to the final entry
 */
metac_entry_t * metac_entry_final_entry(metac_entry_t *p_entry, metac_quals_t * p_quals);

/**
 * @brief create dynamically allocated entry and copy data from original
 */
metac_entry_t * metac_new_entry(metac_entry_t * p_entry_from);
/**
 * @brief returns non-zero if entry was created with metac_new_entry
 */
metac_flag_t metac_entry_is_dynamic(metac_entry_t * p_entry);
/**
 * @brief free entry (checks metac_entry_is_dynamic). Deletes only entry itself - non recursiv
 */
void metac_entry_delete(metac_entry_t * p_entry);

/**
 * @brief return name of base type (char, int, double, etc). NULL in case of error
*/
metac_name_t metac_entry_base_type_name(metac_entry_t *p_entry);

/**
 * @brief check if final entry is base type (char, int, double, etc)
*/
metac_flag_t metac_entry_is_base_type(metac_entry_t *p_entry);
/**
 * @brief Returns size in bytes to p_sz if final entry is base type
 * @param p_entry - entry
 * @param p_sz - address of the memory where the calculated value is stored, can't be NULL.
 * @returns negative - errno in case of error. 0 - otherwise.
*/
int metac_entry_base_type_byte_size(metac_entry_t *p_entry, metac_size_t *p_sz);
/**
 * @brief Returns entry encoding (see metac/const.h all METAC_ENC_) to p_enc if final entry is base type
 * @param p_entry - entry
 * @param p_enc - address of the memory where the calculated value is stored, can't be NULL.
 * @returns negative - errno in case of error. 0 - otherwise.
*/
int metac_entry_base_type_encoding(metac_entry_t *p_entry, metac_encoding_t *p_enc);
/**
 * @brief checks that base_type_name, encoding and size are as expected. Returns negative errno if something isn't expected
 */
int metac_entry_check_base_type(metac_entry_t *p_entry, metac_name_t expected_name, metac_encoding_t expected_encoding, metac_size_t expected_size);

/**
 * @brief check if final entry is a pointer of some type
*/
metac_flag_t metac_entry_is_pointer(metac_entry_t *p_entry);
/**
 * @brief check if final entry is void*
*/
metac_flag_t metac_entry_is_void_pointer(metac_entry_t *p_entry);
/**
 * @brief check if final entry is a pointer of type we don't know internals
 * It's allowed in C to use pointers of types which were declared, but not defined.
 * This is a way how developers hide internal implementaion details.
 * Basically it's similar to void *, but with control of type nam.
*/
metac_flag_t metac_entry_is_declaration_pointer(metac_entry_t *p_entry);
/**
 * @brief get entry of the type to which pointer points
 * e.g. if it's char*, this will entry of char 
*/
metac_entry_t * metac_entry_pointer_entry(metac_entry_t *p_entry);

/**
 * @brief check if final entry is an array(has members) of some type
*/
metac_flag_t metac_entry_has_elements(metac_entry_t * p_entry);
/**
 * @brief Returns length of array. -1 if array is flexible.
*/
metac_num_t metac_entry_element_count(metac_entry_t * p_entry);
/**
 * @brief get entry of the type to which array consists
 * e.g. if it's char[], this will entry of char 
*/
metac_entry_t * metac_entry_element_entry(metac_entry_t *p_entry);

/**
 * @brief check if final entry is enumeration
*/
metac_flag_t metac_entry_is_enumeration(metac_entry_t *p_entry);
/**
 * @brief return number of enumerated members
*/
metac_num_t metac_entry_enumeration_info_size(metac_entry_t * p_entry);
/** @brief structure to keep relation between name and value in enum */
struct metac_type_enumerator_info {
    metac_name_t name; /**< enumerator name */
    metac_const_value_t const_value; /**< enumerator value */
};
/**
 * @brief get one pair of name/value for enum by id which must be in rage [0; metac_entry_enumeration_info_size - 1]
*/
struct metac_type_enumerator_info const * metac_entry_enumeration_info(metac_entry_t * p_entry, metac_num_t id);
/**
 * @brief converts enum value to name. NULL matching item wasn't found
*/
metac_name_t metac_entry_enumeration_to_name(metac_entry_t * p_entry, metac_const_value_t var);

/**
 * @brief check if final entry is structure/union (potentially class)
*/
metac_flag_t metac_entry_has_members(metac_entry_t *p_entry);

/**
 * @brief performs analog of va_arg(*p_va_list, this struct)
*/
void * metac_entry_struct_va_arg(metac_entry_t *p_entry, struct va_list_container *p_va_list_container);

/**
 * @brief returns number of members of the structure/union. Negative value is errno
*/
metac_num_t metac_entry_member_count(metac_entry_t *p_entry);
/**
 * @brief returns pointer to the member entry (kind==METAC_KND_member) by member_id
*/
metac_entry_t * metac_entry_by_member_id(metac_entry_t *p_entry, metac_num_t member_id);
/**
 * @brief returns id of them member with the given name. Negative valis is errno
*/
metac_num_t metac_entry_member_name_to_id(metac_entry_t *p_entry, metac_name_t name);

/**
 * @brief check if entry member (kind==METAC_KND_member)
*/
metac_flag_t metac_entry_is_member(metac_entry_t *p_entry);
/**
 * @brief returns type of the member
*/
metac_entry_t * metac_entry_member_entry(metac_entry_t *p_entry);
/** @brief raw representation of the member of structure/union */
struct metac_member_raw_location_info {
    metac_size_t *p_byte_size; /**< sometimes it's provided, can be taken from type if not. Can be NULL */
    metac_offset_t byte_offset; /**< location - offset in bytes (mandatory only for structure members, but 0 is ok in case of dwarf4 and bitfield. data_bit_offset/8 should be used) */
    metac_offset_t *p_data_bit_offset; /**< data bit offset - new field in DWARF4 instead of p_bit_offset */
    metac_offset_t *p_bit_offset; /**< bit offset - used only when bits were specified. Can be NULL */
    metac_size_t *p_bit_size; /**< bit size - used only when bits were specified. Can be NULL */
};
/**
 * @brief make a copy for raw member information to p_bitfields_raw_info
*/
int metac_entry_member_raw_location_info(metac_entry_t *p_entry, struct metac_member_raw_location_info * p_bitfields_raw_info);
/**
 * @brief highlevel function that gets information about member offset from the beginning of the structure memory, offset in bits and number of bits.
 * This information must be used in combination with type of the member which can be byte, word, dword,....
*/
int metac_entry_member_bitfield_offsets(metac_entry_t *p_memb_entry, metac_offset_t *p_byte_offset, metac_offset_t *p_bit_offset, metac_offset_t *p_bit_size);

/** @brief struct which is used by metac_entry_by_member_ids to locate member on arbitrary level of structure/union */
typedef struct {
    metac_num_t i;  /**< member id (e.g. useful when member doesn't have a name). is is used only if n in NULL */
    metac_name_t n; /**< member name */
}metac_entry_id_t;

/** wrapper for metac_entry_by_member_ids to hide _expression_ length */
#define METAC_ENTRY_BY_MEMBER_IDS(_entry_, _final_, _expression_...) ({ \
        metac_entry_id_t id[] = {_expression_}; \
        metac_entry_by_member_ids(_entry_, _final_, id, sizeof(id)/sizeof(id[0])); \
    })
/**
 * @brief locate member entry on arbitrary level of structure/union
 * better to use wrapper METAC_ENTRY_BY_MEMBER_IDS that hides ids_count parameter
*/
metac_entry_t * metac_entry_by_member_ids(metac_entry_t * p_entry, metac_flag_t final, metac_entry_id_t* p_ids, metac_num_t ids_count);

/**
 * @brief check if final entry is function (METAC_KND_subroutine)
*/
metac_flag_t metac_entry_has_paremeters(metac_entry_t * p_entry);
/**
 * @brief return number of parameters. Negative value is errno
*/
metac_num_t metac_entry_paremeters_count(metac_entry_t *p_entry);
/**
 * @brief returns pointer to the parameter entry (kind==METAC_KND_member) by param_id which is in range [0; metac_entry_paremeters_count -1]
*/
metac_entry_t * metac_entry_by_paremeter_id(metac_entry_t *p_entry, metac_num_t param_id);
/**
 * @brief returns id of them parameters with the given name. Negative valis is errno
*/
metac_num_t metac_entry_paremeter_name_to_id(metac_entry_t *p_entry, metac_name_t name);

/** wrapper for metac_entry_by_parameter_ids */
#define METAC_ENTRY_BY_PARAMETER_IDS(_entry_, _final_, _expression_...)\
        metac_entry_by_parameter_ids(_entry_, _final_, (metac_entry_id_t[]){_expression_})
/**
 * @brief locate param entry by name or id
 * better to use wrapper METAC_ENTRY_BY_PARAMETER_IDS
*/
metac_entry_t * metac_entry_by_parameter_ids(metac_entry_t * p_entry, metac_flag_t final, metac_entry_id_t* p_ids);

/**
 * @brief check if entry is subprogram parameter (kind==METAC_KND_subprogram_parameter)
*/
metac_flag_t metac_entry_is_parameter(metac_entry_t * p_entry);
/**
 * @brief check if parameter is ... (va_arg)
*/
metac_flag_t metac_entry_is_unspecified_parameter(metac_entry_t * p_entry);
/**
 * @brief returns type of argument
*/
metac_entry_t * metac_entry_parameter_entry(metac_entry_t *p_entry);

/**
 * @brief check if final entry is function (METAC_KND_subroutine) which isn't void
*/
metac_flag_t metac_entry_has_result(metac_entry_t * p_entry);

/**
 * @brief returns type which the function returns
*/
metac_entry_t * metac_entry_result_type(metac_entry_t * p_entry);

#endif