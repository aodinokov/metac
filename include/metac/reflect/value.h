#ifndef INCLUDE_METAC_VALUE_H_
#define INCLUDE_METAC_VALUE_H_

#include "metac/reflect/entry_tag.h"

/** @brief get value for variable using declloc
 * @param _name_ declloc
 * @param _val_ variable (can't be expression or argument)
*/
#define METAC_VALUE_FROM_DECLLOC(_name_, _val_) ({ \
    metac_value_t * p_val = NULL; \
    metac_entry_t * p_entry = METAC_ENTRY_FROM_DECLLOC(_name_, _val_); \
    if (p_entry != NULL) { \
        p_val = metac_new_value(p_entry, &_val_); \
    } \
    p_val; })

/** @brief get value for variable using link
 * @param _name_ link name
*/
#define METAC_VALUE_FROM_LINK(_name_) ({ \
    metac_value_t * p_val = NULL; \
    if (METAC_GSYM_LINK_NAME(_name_).pp_entry != NULL) { \
        p_val = metac_new_value( \
        *(METAC_GSYM_LINK_NAME(_name_).pp_entry), \
        (void*)(METAC_GSYM_LINK_NAME(_name_).p_data)); \
    } \
    p_val; })

/** @brief create new value out of metac_entry_t and address */
metac_value_t * metac_new_value(metac_entry_t *p_entry, void * addr);
/** @brief delete metac_value_t from memory */
void metac_value_delete(metac_value_t * p_val);

/** @brief walker mode (used for metac_value_string)*/
typedef enum {
    METAC_WMODE_shallow = 1,    /**< shallow - means pointer will be just treated as value */
    METAC_WMODE_deep,           /**< deep - detect and fail on circles */
}metac_value_walk_mode_t;

/** @brief default behavior in case callback didn't return information for pointer */
typedef enum {
    METAC_UPTR_fail = 0,    /**< fail if met void* or ptr to declaration type (without knowing of internal structure) - safest and default*/
    METAC_UPTR_ignore,      /**< mark them as ignored - don't do anything with them. Set them as NULL during copying, don't compare during comparison */
    METAC_UPTR_as_values,   /**< treat them as long. copy them as unsigned long values on copy/comparison (ptr without children). delete will ignore it.*/
    METAC_UPTR_as_one_byte_ptr, /**< the size isn't known, but for sure it's at least 1 byte. This is done more for free memory. */
}metac_value_memory_map_uptr_mode_t;
/** @brief default behavior in case callback didn't return information for union */
typedef enum {
    METAC_UNION_fail = 0,    /**< fail if met union - safest and default*/
    METAC_UNION_ignore,      /**< mark as ignored - will set to 0, don't compare on equal. Warning, if union contains pointers - there can be leaks*/
    METAC_UNION_as_mem,      /**< use memcmp/memcpy to treat. Warning, if union contains pointers - there can be leaks*/
}metac_value_memory_map_union_mode_t;
/** @brief default behavior in case callback didn't return information for flexible array */
typedef enum {
    METAC_FLXARR_fail = 0,    /**< fail if met flexible array (array with not set length) - safest and default*/
    METAC_FLXARR_ignore,      /**< mark as ignored - won't do anything. Warning, if array elements contains pointers - there can be leaks*/
}metac_value_memory_map_flex_array_mode_t;
/** @brief mode of deep free function */
typedef struct {
    metac_value_memory_map_uptr_mode_t unknown_ptr_mode;
    metac_value_memory_map_union_mode_t union_mode;
    metac_value_memory_map_flex_array_mode_t flex_array_mode;
}metac_value_memory_map_non_handled_mode_t;
/** @brief mode of deep copy and equal functions */
typedef struct {
    enum {
        METAC_MMODE_dag = 0,    /**< analyse pointers to detect map structure (can be DAG with weak pointers) -safest and default */
        METAC_MMODE_tree,       /**< treat every pointer as a separate mem block */
    }memory_block_mode;
    metac_value_memory_map_uptr_mode_t unknown_ptr_mode;
    metac_value_memory_map_union_mode_t union_mode;
    metac_value_memory_map_flex_array_mode_t flex_array_mode;
}metac_value_memory_map_mode_t;

/** @brief get value's entry(basically type) */
metac_entry_t * metac_value_entry(metac_value_t * p_val);
/** @brief get value's address of memory it points  */
void * metac_value_addr(metac_value_t * p_val);
/** @brief make a copy ov value  */
static inline metac_value_t * metac_new_value_from_value(metac_value_t * p_val) {
    return metac_new_value(metac_value_entry(p_val), metac_value_addr(p_val));
}
/* deep functions and their shorter versions */

/** @brief print value data in c format
 *  @param p_val value
 *  @param wmode shallow or deep mode
 *  @param p_tag_map tagmap to resolve ambiguities
 *  @return string in dynamic memory (don't forget to free it)
 */
char * metac_value_string_ex(metac_value_t * p_val, metac_value_walk_mode_t wmode, metac_tag_map_t * p_tag_map);

/** @brief compare 2 values
 *  @param p_val1 value1
 *  @param p_val2 value2
 *  @param p_mode mode in which this function will work. if NULL - uses DAG with fail on all unknown situations
 *  @param p_tag_map tagmap to resolve ambiguities
 *  @return negative in case of error, 0 if not equal, 1 is equal
 */
int metac_value_equal_ex(metac_value_t * p_val1, metac_value_t * p_val2, 
    metac_value_memory_map_mode_t * p_mode,
    metac_tag_map_t * p_tag_map);

/** @brief compare 2 values
 *  @param p_src_val source value
 *  @param p_dst_val destination value (must have the same type as source)
 *  @param p_mode mode in which this function will work. if NULL - uses DAG with fail on all unknown situations
 *  @param calloc_fn address of function which can be used for allocating. if NULL - use calloc
 *  @param free_fn address of function which can be used for freeing memory (in case of failure during execution). if NULL - uses free
 *  @param p_tag_map tagmap to resolve ambiguities
 *  @return returns p_dst_val in case of success. NULL in case of failure 
 */
metac_value_t *metac_value_copy_ex(metac_value_t * p_src_val, metac_value_t * p_dst_val,
    metac_value_memory_map_mode_t * p_mode,
    void *(*calloc_fn)(size_t nmemb, size_t size),
    void (*free_fn)(void *ptr),
    metac_tag_map_t * p_tag_map);

/** @brief free memory of value object (metac_value_t itself isn't deleted)
 *  @param p_val value to free
 *  @param p_mode mode in which this function will work.
 *  @param free_fn address of function which can be used for freeing memory. if NULL uses free
 *  @param p_tag_map tagmap to resolve ambiguities
 *  @return 0 in case of failure (that means that memory isn't deleted). !=0 - success 
 */
metac_flag_t metac_value_free_ex(metac_value_t * p_val,
    metac_value_memory_map_non_handled_mode_t * p_mode,
    void (*free_fn)(void *ptr),
    metac_tag_map_t * p_tag_map);

/** @brief simplified verions of metac_value_string_ex */
static inline char * metac_value_string(metac_value_t * p_val) {
    return (p_val != NULL)?metac_value_string_ex(p_val, METAC_WMODE_shallow, NULL):NULL;
}

/** @brief return value's kind */
metac_kind_t metac_value_kind(metac_value_t * p_val);
/** @brief return different things depending on kind, e.g. var name if it's a variable,
memeber name if it's a member of struct, argument name if it's parameter,
in threory if it's struct, enum, etc, it should retrun their names, but we
skip many intermittent types, so it's in rare cases */
metac_name_t metac_value_name(metac_value_t * p_val); 
/** @brief return value's final entry kind */
metac_kind_t metac_value_final_kind(metac_value_t * p_val, metac_quals_t * p_quals); /* returns actual type kind, skips quals, typedefs, variable */

/* kind = METAC_KND_subroutine_type */
/* metac_flag_t metac_value_is_subroutine(metac_value_t *p_val);
TODO: we may have a call if pointer to subroutine, using libffi */

/* kind = METAC_KND_array */

/** @brief return non-zero if value's final kind is array */
metac_flag_t metac_value_has_elements(metac_value_t *p_val);
/** @brief return array's len. -1 for flexible array */
metac_num_t metac_value_element_count(metac_value_t *p_val);
/** @brief check if the array is flexible */
static inline metac_flag_t metac_value_element_count_flexible(metac_value_t *p_val) { return metac_value_element_count(p_val) == -1;}
/** @brief creates new value with overriden element count. useful for flexible arrays if we got actual len */
metac_value_t * metac_new_element_count_value(metac_value_t *p_val, metac_num_t count);
/** @brief creates and returns value of the element. element_id must be in the range [0; count - 1]*/
metac_value_t * metac_new_value_by_element_id(metac_value_t *p_val, metac_num_t element_id);

/* kind = METAC_KND_structure_type || kind == METAC_KND_union_type || C++: kind == METAC_KND_class_type */

/** @brief return non-zero if value's final kind is struct/union... */
metac_flag_t metac_value_has_members(metac_value_t *p_val);
/** @brief return number of fields of struct/union */
metac_num_t metac_value_member_count(metac_value_t *p_val);
/** @brief to get member name use metac_value_name(p_val) */
metac_value_t * metac_new_value_by_member_id(metac_value_t *p_val, metac_num_t member_id);

/* kind == METAC_KND_enumeration_type */

/** @brief return non-zero if value's final kind is enum */
metac_flag_t metac_value_is_enumeration(metac_value_t *p_val);
/** @brief return number of entries in enum */
metac_num_t metac_value_enumeration_info_size(metac_value_t * p_val);
/** @brief return information (pair: name/value) with id. id is in range [0; info_size - 1]*/
struct metac_type_enumerator_info const * metac_value_enumeration_info(metac_value_t * p_val, metac_num_t id);
/** @brief concert value to string representation of enum */
metac_name_t metac_value_enumeration_to_name(metac_value_t * p_val, metac_const_value_t var);
/** @brief read value of enumeration
 *  @param p_val value
 *  @param p_var the data will be put by this addr
 *  @return 0 in case of success
 */
int metac_value_enumeration(metac_value_t * p_val, metac_const_value_t *p_var);
/** @brief write value of enumeration
 *  @param p_val value
 *  @param var the data to be written
 *  @return 0 in case of success
 */
int metac_value_set_enumeration(metac_value_t * p_val, metac_const_value_t var);
/** @brief compare 2 enum values
 *  @param p_val1 value1
 *  @param p_val2 value2
 *  @return 1 in case of equal, 0 in case of non equal, negative in case of error
 */
int metac_value_equal_enumeration(metac_value_t *p_val1, metac_value_t *p_val2);
/** @brief copy enum to enum
 *  @param p_src_val from
 *  @param p_dst_val to
 *  @return p_dst_val in case of success, NULL if failed
 */
metac_value_t * metac_value_copy_enumeration(metac_value_t *p_src_val, metac_value_t *p_dst_val);
/** @brief represent enum value as string
 *  @param p_val
 *  @return if data is matching with one of enum iterm - string. otherwise will print digital representation. dynamically allocated
 */
char * metac_value_enumeration_string(metac_value_t * p_val);

/* kind == METAC_KND_pointer_type */

/** @brief return non-zero if value's final kind is ponter */
metac_flag_t metac_value_is_pointer(metac_value_t *p_val);
/** @brief return non-zero if value's void * ponter */
metac_flag_t metac_value_is_void_pointer(metac_value_t *p_val);
/**
 * @brief check if final entry of this value is a pointer of type we don't know internals
 * It's allowed in C to use pointers of types which were declared, but not defined.
 * This is a way how developers hide internal implementaion details.
 * Basically it's similar to void *, but with control of type nam.
*/
metac_flag_t metac_value_is_declaration_pointer(metac_value_t *p_val);
/** @brief read value of pointer
 *  @param p_val value
 *  @param pp_addr the data will be put by this addr
 *  @return 0 in case of success
 */
int metac_value_pointer(metac_value_t *p_val, void ** pp_addr);
/** @brief write value of pointer
 *  @param p_val value
 *  @param addr the data to be written
 *  @return 0 in case of success
 */
int metac_value_set_pointer(metac_value_t *p_val, void * addr);
/** @brief compare 2 pointer values
 *  @param p_val1 value1
 *  @param p_val2 value2
 *  @return 1 in case of equal, 0 in case of non equal, negative in case of error
 */
int metac_value_equal_pointer(metac_value_t *p_val1, metac_value_t *p_val2);
/** @brief copy enum to pointer
 *  @param p_src_val from
 *  @param p_dst_val to
 *  @return p_dst_val in case of success, NULL if failed
 */
metac_value_t * metac_value_copy_pointer(metac_value_t *p_src_val, metac_value_t *p_dst_val);
/** @brief represent pointer value as string
 *  @param p_val
 *  @return dynamically allocated string with pointer string representation
 */
char * metac_value_pointer_string(metac_value_t * p_val);

/* kind == METAC_KND_base_type || (kind = METAC_KND_member_info && type.kind == METAC_KND_base_type) || (kind = METAC_KND_variable && type.kind == METAC_KND_base_type) */

/** @brief return non-zero if value's final kind is base type (e.g. char, short, int &etc) */
metac_flag_t metac_value_is_base_type(metac_value_t *p_val);
/**
 * @brief checks that base_type_name, encoding and size are as expected. Returns negative errno if something isn't expected
 */
int metac_value_check_base_type(metac_value_t * p_val, metac_name_t type_name, metac_encoding_t encoding, metac_size_t var_size);
/** @brief read value of base type with given parameters
 *  @param p_val value
 *  @param type_name name of type must match
 *  @param encoding encoding must match
 *  @param p_var the data will be put by this addr
 *  @param var_size the data size must match with the entry
 *  @return 0 in case of success
 */
int metac_value_base_type(metac_value_t * p_val, metac_name_t type_name, metac_encoding_t encoding, void * p_var, metac_size_t var_size);
/** @brief write value of base type with given parameters
 *  @param p_val value
 *  @param type_name name of type must match
 *  @param encoding encoding must match
 *  @param p_var the data will be written
 *  @param var_size the data size must match with the entry
 *  @return 0 in case of success
 */
int metac_value_set_base_type(metac_value_t * p_val, metac_name_t type_name, metac_encoding_t encoding, void * p_var, metac_size_t var_size);
/** @brief compare 2 base type values. that means to compare type, size and data
 *  @param p_val1 value1
 *  @param p_val2 value2
 *  @return 1 in case of equal, 0 in case of non equal, negative in case of error
 */
int metac_value_equal_base_type(metac_value_t *p_val1, metac_value_t *p_val2);
/** @brief copy base type to another base type. that means to compare type, size and only after that copy
 *  @param p_src_val from
 *  @param p_dst_val to
 *  @return p_dst_val in case of success, NULL if failed
 */
metac_value_t * metac_value_copy_base_type(metac_value_t *p_src_val, metac_value_t *p_dst_val);
/** @brief represent base type value as string
 *  @param p_val
 *  @return dynamically allocated string with pointer string representation
 */
char * metac_value_base_type_string(metac_value_t * p_val);

/* per base type check using metac_value_check_base_type */

metac_flag_t metac_value_is_char(metac_value_t * p_val);
metac_flag_t metac_value_is_uchar(metac_value_t * p_val);
metac_flag_t metac_value_is_short(metac_value_t * p_val);
metac_flag_t metac_value_is_ushort(metac_value_t * p_val);
metac_flag_t metac_value_is_int(metac_value_t * p_val);
metac_flag_t metac_value_is_uint(metac_value_t * p_val);
metac_flag_t metac_value_is_long(metac_value_t * p_val);
metac_flag_t metac_value_is_ulong(metac_value_t * p_val);
metac_flag_t metac_value_is_llong(metac_value_t * p_val);
metac_flag_t metac_value_is_ullong(metac_value_t * p_val);
metac_flag_t metac_value_is_bool(metac_value_t * p_val);
metac_flag_t metac_value_is_float(metac_value_t * p_val);
metac_flag_t metac_value_is_double(metac_value_t * p_val);
metac_flag_t metac_value_is_ldouble(metac_value_t * p_val);
metac_flag_t metac_value_is_float_complex(metac_value_t * p_val);
metac_flag_t metac_value_is_double_complex(metac_value_t * p_val);
metac_flag_t metac_value_is_ldouble_complex(metac_value_t * p_val);

/* per base type read using metac_value_base_type */

int metac_value_char(metac_value_t * p_val, char *p_var);
int metac_value_uchar(metac_value_t * p_val, unsigned char *p_var);
int metac_value_short(metac_value_t * p_val, short *p_var);
int metac_value_ushort(metac_value_t * p_val, unsigned short *p_var);
int metac_value_int(metac_value_t * p_val, int *p_var);
int metac_value_uint(metac_value_t * p_val, unsigned int *p_var);
int metac_value_long(metac_value_t * p_val, long *p_var);
int metac_value_ulong(metac_value_t * p_val, unsigned long *p_var);
int metac_value_llong(metac_value_t * p_val, long long *p_var);
int metac_value_ullong(metac_value_t * p_val, unsigned long long *p_var);
int metac_value_bool(metac_value_t * p_val, bool *p_var);
int metac_value_float(metac_value_t * p_val, float *p_var);
int metac_value_double(metac_value_t * p_val, double *p_var);
int metac_value_ldouble(metac_value_t * p_val, long double *p_var);
#ifndef __cplusplus // TODO: C++ doesn't know that 
int metac_value_float_complex(metac_value_t * p_val, float complex *p_var);
int metac_value_double_complex(metac_value_t * p_val, double complex *p_var);
int metac_value_ldouble_complex(metac_value_t * p_val, long double complex *p_var);
#endif

/* per base type write metac_value_set_base_type */

int metac_value_set_char(metac_value_t * p_val, char var);
int metac_value_set_uchar(metac_value_t * p_val, unsigned char var);
int metac_value_set_short(metac_value_t * p_val, short var);
int metac_value_set_ushort(metac_value_t * p_val, unsigned short var);
int metac_value_set_int(metac_value_t * p_val, int var);
int metac_value_set_uint(metac_value_t * p_val, unsigned int var);
int metac_value_set_long(metac_value_t * p_val, long var);
int metac_value_set_ulong(metac_value_t * p_val, unsigned long var);
int metac_value_set_llong(metac_value_t * p_val, long long var);
int metac_value_set_ullong(metac_value_t * p_val, unsigned long long var);
int metac_value_set_bool(metac_value_t * p_val, bool var);
int metac_value_set_float(metac_value_t * p_val, float var);
int metac_value_set_double(metac_value_t * p_val, double var);
int metac_value_set_ldouble(metac_value_t * p_val, long double var);
#ifndef __cplusplus
int metac_value_set_float_complex(metac_value_t * p_val, float complex var);
int metac_value_set_double_complex(metac_value_t * p_val, double complex var);
int metac_value_set_ldouble_complex(metac_value_t * p_val, long double complex var);
#endif

/** @brief read any numberic from char to unsigned long long into num_t */
int metac_value_num(metac_value_t * p_val, metac_num_t * p_var);

#endif