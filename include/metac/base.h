#ifndef METAC_H_
#define METAC_H_

#include <stdio.h>  /* NULL */
#include <libdwarf/dwarf.h>

#include "metac/refcounter.h"
#include "metac/attrtypes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct metac_type metac_type_t;
typedef struct metac_type_at metac_type_at_t;
typedef struct metac_type_annotation metac_type_annotation_t;
typedef struct metac_type_annotation_value metac_type_annotation_value_t;

struct metac_type {
    metac_refcounter_object_t refcounter_object; /* needed to work with references if the object is dynamically allocated (see metac_type_create_pointer_for) */

    metac_type_kind_t kind; /* type kind (base type and etc) */
    metac_name_t name; /* name of type (can be NULL) */
    metac_flag_t declaration; /* =1 if type is incomplete */

    /* METAC specific data */
    union {
        /* .kind == DW_TAG_typedef */
        struct typedef_info {
            metac_type_t *type; /* universal field */
        } typedef_info;
        /* .kind == DW_TAG_const_type */
        struct const_type_info {
            metac_type_t *type; /* universal field */
        } const_type_info;
        /* .kind == DW_TAG_base_type */
        struct base_type_info {
            metac_byte_size_t byte_size; /* mandatory field */
            metac_encoding_t encoding; /* type encoding (DW_ATE_signed etc) */
        } base_type_info;
        /* .kind == DW_TAG_pointer_type */
        struct pointer_type_info {
            metac_byte_size_t byte_size; /* can be optional */
            metac_type_t *type; /* universal field */
        } pointer_type_info;
        /* .kind == DW_TAG_enumeration_type */
        struct enumeration_type_info {
            metac_type_t *type; /* universal field (can be NULL in some compliers) */
            metac_byte_size_t byte_size; /* mandatory field */
            metac_encoding_t encoding;
            metac_num_t enumerators_count; /* mandatory field */
            struct metac_type_enumerator_info {
                metac_name_t name; /* enumerator name */
                metac_const_value_t const_value; /* enumerator value */
            } *enumerators;
        } enumeration_type_info;
        /* .kind == DW_TAG_subprogram */
        struct subprogram_info {
            metac_type_t *type; /* function return type (NULL if void) */
            metac_num_t parameters_count; /* number of parameters */
            struct metac_type_subprogram_parameter {
                int unspecified_parameters; /* if 1 - after that it's possible to have a lot of arguments*/
                metac_type_t *type; /* parameter type */
                metac_name_t name; /* parameter name */
            } *parameters;
        } subprogram_info;
        /* .kind == DW_TAG_subroutine_type */
        struct subroutine_type_info {
            metac_type_t *type; /* function return type (NULL if void) */
            metac_num_t parameters_count; /* number of parameters */
            struct metac_type_subroutine_type_parameter {
                int unspecified_parameters; /* if 1 - after that it's possible to have a lot of arguments*/
                metac_type_t *type; /* parameter type */
                metac_name_t name; /* parameter name */
            } *parameters;
        } subroutine_type_info;
        /* .kind = DW_TAG_structure_type */
        struct structure_type_info {
            metac_byte_size_t byte_size; /* size of the structure in bytes */
            metac_num_t members_count; /* number of members */
            struct metac_type_member_info {
                metac_type_t *type; /* type of the member (mandatory) */
#define METAC_ANON_MEMBER_NAME ""
                metac_name_t name; /* name of the member (mandatory), but can be "" for anonymous elements */
                metac_data_member_location_t data_member_location; /* location - offset in bytes (mandatory only for structure members, but 0 is ok if not defined) */
                metac_bit_offset_t *p_bit_offset; /* bit offset - used only when bits were specified. Can be NULL */
                metac_bit_size_t *p_bit_size; /* bit size - used only when bits were specified. Can be NULL */
            } *members;
        } structure_type_info;
        /* .kind == DW_TAG_union_type */
        struct union_type_info {
            metac_byte_size_t byte_size; /* size of the union in bytes */
            metac_num_t members_count; /* number of members */
            struct metac_type_member_info *members;
        } union_type_info;
        /* .kind == DW_TAG_array_type */
        struct array_type_info {
            metac_type_t *type; /* type of elements */
            metac_flag_t is_flexible; /* 1 - if array is flexible ??? may be create a macro and use elements_count == -1*/
            metac_count_t elements_count; /* based on min and max or count - length of the array ---- TBD: to rework it for multi-dimensional case */
            metac_num_t subranges_count; /* number of subranges ( >1 if array is multi-dimensional) */
            struct metac_type_subrange_info {
                metac_type_t *type; /* type of index */
                /*pre-calculated data*/
                metac_flag_t is_flexible; /* if nothing was specified in DWARF*/
                metac_bound_t lower_bound; /* min index in subrange */
                metac_count_t count; /* number of elements in subrange */
                /*raw data*/
                struct {
                    metac_count_t *p_count; /* number of elements in subrange */
                    metac_bound_t *p_lower_bound; /* min index in subrange */
                    metac_bound_t *p_upper_bound; /* max index in subrange */
                } raw_data;
            } *subranges;
        } array_type_info;
    };

    /* METAC allows to set additional type annotations that help to make a decision e.g. during serialization */
    metac_type_annotation_t *annotations; /* pointer to explicit annotations array for this type*/
#if 0
    /**
     * RAW DWARF data
     * definition of C type attributes in C (based on DWARF)
     * defined here because these types are used by metac.awk during generation of type structure
     * possible to move to metac_impl.h
     **/
    struct {
        metac_num_t child_num; /* number of children */
        metac_type_t **child; /* pointer to array of children */
        metac_num_t at_num; /* number of attributes */
        struct metac_type_at {
            metac_type_at_id_t id;
            union {
                metac_name_t name; /* universal field */
                metac_type_t *type; /* universal field */
                metac_byte_size_t byte_size; /* type size */
                metac_encoding_t encoding; /* type encoding (DW_ATE_signed etc) */
                metac_data_member_location_t data_member_location; /* member offset in structs and unions */
                metac_bit_offset_t bit_offset; /* bit-field member bit offset in structs and unions */
                metac_bit_size_t bit_size; /* bit-field member bit size in structs and unions */
                metac_bound_t lower_bound; /* for array_ranges*/
                metac_bound_t upper_bound; /* for array_ranges*/
                metac_count_t count; /* for array_ranges*/
                metac_const_value_t const_value; /* for enums*/
                metac_flag_t declaration; /* true if type isn't complete */
            };
        } *at; /* pointer to array of attributes */
    } dwarf_info;
#endif
};

metac_name_t metac_type_name(struct metac_type *type);
metac_byte_size_t metac_type_byte_size(struct metac_type *type); /*< returns length in bytes of any type */
struct metac_type* metac_type_actual_type(struct metac_type *type); /*< skips DW_TAG_typedef & DW_TAG_const_type and return actual type */
int metac_type_enumeration_type_get_value(struct metac_type *type,
        metac_name_t name, metac_const_value_t *p_const_value);
metac_name_t metac_type_enumeration_type_get_name(struct metac_type *type,
        metac_const_value_t const_value);
int metac_type_array_subrange_count(struct metac_type *type,
        metac_num_t subrange_id, metac_count_t *p_count);
int metac_type_array_member_location(struct metac_type *type,
        metac_num_t subranges_count, metac_num_t *subranges,
        metac_data_member_location_t *p_data_member_location);
const metac_type_annotation_t*
metac_type_annotation(struct metac_type *type, const char *key,
        metac_type_annotation_t *override_annotations); /* return annotation value by key (NULL if not found)*/

/* dynamic functions - for future */
struct metac_type* metac_type_create_pointer_for(struct metac_type *p_type);
struct metac_type* metac_type_get_actual_type(struct metac_type *p_metac_type);
struct metac_type* metac_type_get(struct metac_type *p_metac_type);
int metac_type_put(struct metac_type **pp_metac_type);

#define _METAC(x, name) metac__ ## x ## _ ## name
#define METAC(x, name) _METAC(x, name)
/* macroses to export C type definitions and their params in code*/
#define METAC_TYPE_NAME(name) METAC(type, name)
#define METAC_TYPE_GENERATOR_NAME(name) METAC(typegenerator, name)
#define METAC_TYPE_GENERATE(name) size_t METAC_TYPE_GENERATOR_NAME(name)() { name * _pp_ = NULL; return sizeof(_pp_); }
#define METAC_TYPE_IMPORT(name) extern struct metac_type METAC_TYPE_NAME(name)
#define METAC_TYPE_GENERATE_AND_IMPORT(name) \
	METAC_TYPE_GENERATE(name); \
	METAC_TYPE_IMPORT(name)

#define METAC_TYPE_ANNOTATION_NAME(name) METAC(typeannotations, name)
#define METAC_TYPE_ANNOTATION_DECLARE(name) extern struct metac_type_annotation METAC_TYPE_ANNOTATION_NAME(name)[];
#define METAC_TYPE_ANNOTATION_BEGIN(name) struct metac_type_annotation METAC_TYPE_ANNOTATION_NAME(name)[] = {
#define METAC_TYPE_ANNOTATION_OVERRIDE_BEGIN(override_name) static struct metac_type_annotation override_name[] = {
#define METAC_TYPE_ANNOTATION(_key_, _value_...) {\
		.key = _key_,\
		.value = (metac_type_annotation_value_t[]){\
			{\
				_value_\
			},\
	}}
#define METAC_TYPE_ANNOTATION_END {NULL, NULL}};

typedef int (*metac_cb_discriminator_t)(char *annotation_key,
        int write_operation, /* 0 - if need to store date to p_discriminator_val, 1 - vice-versa*/
        void *ptr, metac_type_t *type, /*pointer to memory region and its type */
        metac_discriminator_value_t *p_discriminator_val, void *data);

typedef int (*metac_cb_array_elements_count_t)(char *annotation_key,
        int write_operation, /* 0 - if need to store date to p_discriminator_val, 1 - vice-versa*/
        void *region_ptr, metac_type_t *region_element_type, /*pointer to memory region element and its type */
        void *first_element_ptr, metac_type_t *first_element_type,
        metac_num_t *p_subrange0_count, void *data);

typedef int (*metac_cb_generic_cast_t)(char *annotation_key,
        int write_operation, /* 0 - if need to store date to p_discriminator_val, 1 - vice-versa*/
        metac_flag_t *p_use_cast, metac_count_t *p_generic_cast_type_id,
        void **ptr, void **ptr_after_generic_cast, void *data);

struct metac_type_annotation {
    char *key;
    metac_type_annotation_value_t *value; /* pointer to explicit specifications array for this type*/
};

struct metac_type_annotation_value {
    struct {
        metac_cb_discriminator_t cb;
        void *data;
    } discriminator;
    struct {
        metac_cb_array_elements_count_t cb;
        void *data;
    } array_elements_count;
    struct {
        metac_cb_generic_cast_t cb;
        void *data;
        struct metac_type **types;
    } generic_cast;
};

#define METAC_CALLBACK_DISCRIMINATOR(_cb_, _data_) \
				.discriminator = {.cb = _cb_, .data = _data_, }
#define METAC_CALLBACK_ARRAY_ELEMENTS_COUNT(_cb_, _data_) \
				.array_elements_count = {.cb = _cb_, .data = _data_, }
#define METAC_CALLBACK_GENERIC_CAST(_cb_, _data_, _types_...) \
				.generic_cast = {.cb = _cb_, .data = _data_, \
					.types = (struct metac_type *[]){\
						_types_, NULL\
				}}

/*some helpful generic functions */
int metac_array_elements_single( /*this array has only 1 elements/pointer points to 1 element*/
char *annotation_key, int write_operation, void *ptr, metac_type_t *type,
        void *first_element_ptr, metac_type_t *first_element_type,
        metac_num_t *p_subrange0_count, void *array_elements_count_cb_context);
int metac_array_elements_1d_with_null( /*1-dimension array with Null at the end*/
char *annotation_key, int write_operation, void *ptr, metac_type_t *type,
        void *first_element_ptr, metac_type_t *first_element_type,
        metac_num_t *p_subrange0_count, void *array_elements_count_cb_context);

/* pre-compile type to make serialization/deletion and de-serialization/creation faster */
typedef struct metac_precompiled_type metac_precompiled_type_t;
metac_precompiled_type_t* metac_precompile_type(struct metac_type *type,
        metac_type_annotation_t *override_annotations);
void metac_dump_precompiled_type(metac_precompiled_type_t *precompiled_type);
int metac_free_precompiled_type(metac_precompiled_type_t **precompiled_type);

struct metac_runtime_object;
struct metac_runtime_object* metac_runtime_object_create(void *ptr,
        struct metac_precompiled_type *p_metac_precompiled_type);
int metac_runtime_object_delete(
        struct metac_runtime_object **pp_metac_runtime_object);

/* C-type-> C-type (simplified operation, like delete, but using memcpy) */
int metac_copy(void *ptr, metac_byte_size_t byte_size,
        metac_precompiled_type_t *precompiled_type,
        metac_count_t elements_count, void **p_ptr);
/* returns 0 if non-equal, 1 if equal, < 0 if there was a error*/
int metac_equal(void *ptr0, void *ptr1, metac_byte_size_t byte_size,
        metac_precompiled_type_t *precompiled_type,
        metac_count_t elements_count);
/* destruction */
int metac_delete(void *ptr, metac_byte_size_t byte_size,
        metac_precompiled_type_t *precompiled_type,
        metac_count_t elements_count);

/*****************************************************************************/
struct metac_type_sorted_array {
    metac_num_t number;
    struct {
        metac_name_t name;
        struct metac_type *ptr;
    } item[];
};

#define METAC_TYPES_ARRAY METAC(types, array)
#define METAC_TYPES_ARRAY_SYMBOL "metac__types_array"
#define METAC_DECLARE_EXTERN_TYPES_ARRAY extern struct metac_type_sorted_array METAC_TYPES_ARRAY
struct metac_type* metac_type_by_name(struct metac_type_sorted_array *array,
        metac_name_t name);

struct metac_object {
    struct metac_type *type;
    void *ptr;
    int fixed_part_byte_size;
    int flexible_part_byte_size;
    /*todo: reference count (0 for objects that were initialized by METAC_OBJECT/METAC_FUNCTION)*/
    int ref_count;
};
#define METAC_OBJECT_NAME(name) METAC(object, name)
#define METAC_OBJECT(_type_, _name_) \
	size_t METAC_TYPE_GENERATOR_NAME(_type_)() { return sizeof(_type_); } \
	METAC_TYPE_IMPORT(_type_); \
	struct metac_object METAC_OBJECT_NAME(_name_) = {\
			.type = &METAC_TYPE_NAME(_type_),\
			.ptr = &_name_,\
			.fixed_part_byte_size = sizeof(_type_), \
			.flexible_part_byte_size = sizeof(_name_) - sizeof(_type_),\
	}
#define METAC_FUNCTION(_name_) METAC_OBJECT(_name_, _name_)

struct metac_object_sorted_array {
    metac_num_t number;
    struct {
        metac_name_t name;
        struct metac_object *ptr;
    } item[];
};

#define METAC_OBJECTS_ARRAY METAC(objects, array)
#define METAC_OBJECTS_ARRAY_SYMBOL "metac__objects_array"
#define METAC_DECLARE_EXTERN_OBJECTS_ARRAY extern struct metac_object_sorted_array METAC_OBJECTS_ARRAY
struct metac_object* metac_object_by_name(
        struct metac_object_sorted_array *array, metac_name_t name);
///*for dynamic objects*/
//struct metac_object * metac_object_get(struct metac_object * object);
//int metac_object_put(struct metac_object * object);

#ifdef __cplusplus
}
#endif
#endif /* METAC_H_ */
