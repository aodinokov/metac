#ifndef METAC_H_
#define METAC_H_

#include <dwarf.h>

#ifdef __cplusplus
extern "C" {
#endif

/* declaration of C type in C */
struct metac_type;
struct metac_type_at;
struct metac_type_specification;
struct metac_type_specification_value;

/* definition of types used for attributes */
typedef char *							metac_name_t;
typedef struct metac_type				metac_type_t;
typedef struct metac_type_at			metac_type_at_t;
typedef unsigned int					metac_byte_size_t;
typedef unsigned int					metac_encoding_t;
typedef metac_byte_size_t				metac_data_member_location_t;	/*make it as metac_byte_size_t - we're multiplying byte_size with index in array*/
typedef unsigned int					metac_bit_offset_t;
typedef unsigned int					metac_bit_size_t;
typedef unsigned int					metac_bound_t;					/*make it long? for easier calculations*/
typedef unsigned int					metac_count_t;
typedef unsigned long					metac_const_value_t;
typedef int								metac_flag;

typedef int								metac_type_id_t;
typedef int								metac_type_at_id_t;
typedef unsigned int					metac_num_t;

typedef struct metac_type_specification_value
										metac_type_specification_value_t;

struct metac_type_specification {
	/*const*/ char *key;
	/*const*/ metac_type_specification_value_t	*value;
};												/* pointer to explicit specifications array for this type*/


typedef struct metac_type_specification metac_type_specification_t;

struct metac_type {
	metac_type_id_t						id;							/* type id */
	metac_name_t						name;						/* name of type (can be NULL) */
	int									declaration;				/* =1 if type is incomplete */

	/* METAC specific data */
	union {
		/* .id == DW_TAG_base_type */
		struct {
			metac_byte_size_t			byte_size;					/* mandatory field */
			metac_encoding_t			encoding;					/* type encoding (DW_ATE_signed etc) */
		}base_type_info;
		/* .id == DW_TAG_pointer_type */
		struct {
			metac_byte_size_t			byte_size;					/* can be optional */
			metac_type_t *				type;						/* universal field */
		}pointer_type_info;
		/* .id == DW_TAG_typedef */
		struct {
			metac_type_t *				type;						/* universal field */
		}typedef_info;
		/* .id == DW_TAG_const_type */
		struct {
			metac_type_t *				type;						/* universal field */
		}const_type_info;
		/* .id == DW_TAG_enumeration_type */
		struct {
			metac_byte_size_t			byte_size;					/* mandatory field */
			metac_num_t					enumerators_count;			/* mandatory field */
			struct metac_type_enumerator_info {
				metac_name_t			name;						/* enumerator name */
				metac_const_value_t		const_value;				/* enumerator value */
			}*							enumerators;
		}enumeration_type_info;
		/* .id == DW_TAG_subprogram */
		struct {
			metac_type_t *				type;						/* function return type (NULL if void) */
			metac_num_t					parameters_count;			/* number of parameters */
			struct metac_type_subprogram_parameter {
				int						unspecified_parameters;		/* if 1 - after that it's possible to have a lot of arguments*/
				metac_type_t *			type;						/* parameter type */
				metac_name_t			name;						/* parameter name */
			}*							parameters;
		}subprogram_info;
		/* .id = DW_TAG_structure_type */
		struct {
			metac_byte_size_t			byte_size;					/* size of the structure in bytes */
			metac_num_t					members_count;				/* number of members */
			struct metac_type_member_info {
				metac_type_t *			type;						/* type of the member (mandatory) */
				#define METAC_ANON_MEMBER_NAME ""
				metac_name_t			name;						/* name of the member (mandatory), but can be "" for anonymous elements */
				metac_data_member_location_t data_member_location;		/* location - offset in bytes (mandatory only for structure members, but 0 is ok if not defined) */
				metac_bit_offset_t *	p_bit_offset;				/* bit offset - used only when bits were specified. Can be NULL */
				metac_bit_size_t *		p_bit_size;					/* bit size - used only when bits were specified. Can be NULL */
			}*							members;
		}structure_type_info;
		/* .id == DW_TAG_union_type */
		struct {
			metac_byte_size_t			byte_size;					/* size of the union in bytes */
			metac_num_t					members_count;				/* number of members */
			struct metac_type_member_info *
										members;
		}union_type_info;
		/* .id == DW_TAG_array_type */
		struct {
			metac_type_t *				type;						/* type of elements */
			int 						is_flexible;				/* 1 - if array is flexible ??? may be create a macro and use elements_count == -1*/
			metac_count_t				elements_count;				/* based on min and max or count - length of the array ---- TBD: to rework it for multi-dimensional case */
			metac_num_t					subranges_count;			/* number of subranges ( >1 if array is multi-dimensional) */
			struct metac_type_subrange_info {
				metac_type_t *			type;						/* type of index */
				/*pre-calculated data*/
				int						is_flexible;				/* if nothing was specified in DWARF*/
				metac_bound_t 			lower_bound;				/* min index in subrange */
				metac_count_t 			count;						/* number of elements in subrange */
				/*raw data*/
				struct {
					metac_count_t *			p_count;				/* number of elements in subrange */
					metac_bound_t *			p_lower_bound;			/* min index in subrange */
					metac_bound_t *			p_upper_bound;			/* max index in subrange */
				}raw_data;
			}*subranges;
		}array_type_info;
	};

	/* METAC allows to set additional type specifications that help to make a decision e.g. during serialization */
	metac_type_specification_t *specifications;						/* pointer to explicit specifications array for this type*/

	/**
	 * RAW DWARF data
	 * definition of C type attributes in C (based on DWARF)
	 * defined here because these types are used by metac.awk during generation of type structure
	 * possible to move to metac_impl.h
	 **/
	struct {
		metac_num_t							child_num;				/* number of children */
		metac_type_t **						child;					/* pointer to array of children */
		metac_num_t							at_num;					/* number of attributes */
		struct metac_type_at {
			metac_type_at_id_t					id;
			union {
				metac_name_t					name;				/* universal field */
				metac_type_t *					type;				/* universal field */
				metac_byte_size_t				byte_size;			/* type size */
				metac_encoding_t				encoding;			/* type encoding (DW_ATE_signed etc) */
				metac_data_member_location_t	data_member_location;		/* member offset in structs and unions */
				metac_bit_offset_t				bit_offset;			/* bit-field member bit offset in structs and unions */
				metac_bit_size_t				bit_size;			/* bit-field member bit size in structs and unions */
				metac_bound_t					lower_bound;		/* for array_ranges*/
				metac_bound_t					upper_bound;		/* for array_ranges*/
				metac_count_t					count;				/* for array_ranges*/
				metac_const_value_t				const_value;		/* for enums*/
				metac_flag						declaration;		/* true if type isn't complete */
			};
		}*at;														/* pointer to array of attributes */
	}dwarf_info;
};

metac_name_t			metac_type_name(struct metac_type *type);
metac_byte_size_t		metac_type_byte_size(struct metac_type *type);	/*< returns length in bytes of any type */
struct metac_type *		metac_type_typedef_skip(struct metac_type *type);	/*< returns real type*/
int						metac_type_enumeration_type_get_value(struct metac_type *type, metac_name_t name, metac_const_value_t *p_const_value);
metac_name_t			metac_type_enumeration_type_get_name(struct metac_type *type, metac_const_value_t const_value);
int 					metac_type_array_subrange_count(struct metac_type *type, metac_num_t subrange_id, metac_count_t *p_count);
int 					metac_type_array_member_location(struct metac_type *type, metac_num_t subranges_count, metac_num_t * subranges, metac_data_member_location_t *p_data_member_location);
const metac_type_specification_value_t *
						metac_type_specification(struct metac_type *type, const char *key);		/* return spec value by key (NULL if not found)*/

#define _METAC(x, name) metac__ ## x ## _ ## name
#define METAC(x, name) _METAC(x, name)
/* macroses to export C type definitions and their params in code*/
#define METAC_TYPE_NAME(name) METAC(type, name)
#define METAC_TYPE_GENERATE(name) extern struct metac_type METAC_TYPE_NAME(name)

#define METAC_TYPE_SPECIFICATION_DECLARE(name) extern struct metac_type_specification METAC(typespec, name)[];
#define METAC_TYPE_SPECIFICATION_BEGIN(name) struct metac_type_specification METAC(typespec, name)[] = {
#define _METAC_TYPE_SPECIFICATION(_key_, _value_...) {.key = _key_, .value = _value_},
#define METAC_TYPE_SPECIFICATION_END {NULL, NULL}};


/* precompiled type specification values: */
typedef int metac_discriminator_value_t;
typedef int (*metac_discriminator_cb_ptr_t)(
	int write_operation,	/* 0 - if need to store date to p_discriminator_val, 1 - vice-versa*/
	void * ptr, metac_type_t * type, /*pointer to memory region and its type */
	metac_discriminator_value_t  * p_discriminator_val,
	void * discriminator_cb_context);

typedef int (*metac_array_elements_count_cb_ptr_t)(
	int write_operation,
	void * region_ptr,
	metac_type_t * region_element_type, /*pointer to memory region element and its type */
	void * first_element_ptr,
	metac_type_t * first_element_type,
	int n, metac_count_t * p_elements_count,/* supports n-dimensional arrays (see array subranges) - TBD:incorrect. Flex arrays can change only 1 dimension*/
	void * array_elements_count_cb_context);

/*some helpful generic functions */
#define metac_array_elements_stop NULL /* ignore this array/pointer */
int metac_array_elements_single( /*this array has only 1 elements/pointer points to 1 element*/
	int write_operation,
	void * ptr,
	metac_type_t * type,
	void * first_element_ptr,
	metac_type_t * first_element_type,
	int n, metac_count_t * p_elements_count,
	void * array_elements_count_cb_context);
int metac_array_elements_1d_with_null( /*1-dimension array with Null at the end*/
	int write_operation,
	void * ptr,
	metac_type_t * type,
	void * first_element_ptr,
	metac_type_t * first_element_type,
	int n, metac_count_t * p_elements_count,
	void * array_elements_count_cb_context);

struct metac_type_specification_value {
	void * specification_context;

	struct {
		metac_discriminator_cb_ptr_t discriminator_funtion_ptr;
	};

	struct {
		metac_array_elements_count_cb_ptr_t array_elements_count_funtion_ptr;
	};
};

#define METAC_DISCRIMINATOR_FUNCTION(_key_, _fn_) \
	_METAC_TYPE_SPECIFICATION(_key_, (metac_type_specification_value_t[]) \
			{{.discriminator_funtion_ptr = _fn_, .specification_context=_key_},})
#define METAC_ARRAY_ELEMENTS_COUNT_FUNCTION(_key_, _fn_) \
	_METAC_TYPE_SPECIFICATION(_key_, (metac_type_specification_value_t[]) \
			{{.array_elements_count_funtion_ptr = _fn_, .specification_context=_key_},})

/* pre-compile type to make serialization/deletion and de-serialization/creation faster */
typedef struct metac_precompiled_type metac_precompiled_type_t;
metac_precompiled_type_t * metac_precompile_type(struct metac_type *type);
void metac_dump_precompiled_type(metac_precompiled_type_t * precompiled_type);
void metac_free_precompiled_type(metac_precompiled_type_t ** precompiled_type);

/* C-type-> C-type (simplified operation, like delete, but using memcpy) */
int metac_copy(void *ptr, metac_byte_size_t size, metac_precompiled_type_t * precompiled_type, metac_count_t elements_count, void **p_ptr);
/* returns 0 if non-equal, 1 if equal, < 0 if there was a error*/
int metac_equal(void *ptr0, void *ptr1,
		metac_byte_size_t size, metac_precompiled_type_t * precompiled_type, metac_count_t elements_count);
/* destruction */
int metac_delete(void *ptr, metac_byte_size_t size, metac_precompiled_type_t * precompiled_type, metac_count_t elements_count);

/* C-type->some format - generic serialization*/
typedef enum _metac_region_element_element_subtype {
	reesHierarchy = 0,
	reesEnum,
	reesBase,
	reesPointer,
	reesArray,
	reesAll,
}metac_region_ee_subtype_t;
struct metac_visitor {
	int (*start)(
			struct metac_visitor *p_visitor,
			metac_count_t regions_count,
			metac_count_t unique_regions_count
			);
	int (*region)(
			struct metac_visitor *p_visitor,
			metac_count_t r_id,
			void *ptr,
			metac_byte_size_t byte_size,
			metac_count_t elements_count
			);
	int (*unique_region)(
			struct metac_visitor *p_visitor,
			metac_count_t r_id,
			metac_count_t u_idx
			);
	int (*non_unique_region)(
			struct metac_visitor *p_visitor,
			metac_count_t r_id,
			metac_count_t u_idx,
			metac_data_member_location_t offset
			);
	int (*region_element)(
			struct metac_visitor *p_visitor,
			metac_count_t r_id,
			metac_count_t e_id,
			metac_type_t * type,
			void *ptr,
			metac_byte_size_t byte_size,
			int real_region_element_element_count,
			metac_region_ee_subtype_t *subtypes_sequence,
			int * real_count_array_per_type, /*array with real number of elements_elements for each item in subtypes_sequence*/
			int subtypes_sequence_lenth
			);
	int (*region_element_element)(
			struct metac_visitor *p_visitor,
			metac_count_t r_id,
			metac_count_t e_id,
			metac_count_t ee_id,
			metac_count_t parent_ee_id,
			metac_type_t * type,
			void *ptr,
			metac_bit_offset_t * p_bit_offset,
			metac_bit_size_t * p_bit_size,
			metac_byte_size_t byte_size,
			char * name_local,
			char * path_within_region_element
			);
	int (*region_element_element_per_subtype)(
			struct metac_visitor *p_visitor,
			metac_count_t r_id,
			metac_count_t e_id,
			metac_count_t ee_id,
			metac_region_ee_subtype_t subtype,
			metac_count_t see_id,
			/* for pointers and arrays only */
			int n,
			metac_count_t * p_elements_count,
			metac_count_t * p_linked_r_id /*can be NULL*/
			);
};
int metac_visit(
	void *ptr,
	metac_byte_size_t size,
	metac_precompiled_type_t * precompiled_type,
	metac_count_t elements_count,
	metac_region_ee_subtype_t *subtypes_sequence,
	int subtypes_sequence_lenth,
	struct metac_visitor * p_visitor);
/*****************************************************************************/
struct metac_type_sorted_array {
	metac_num_t number;
	struct {
		metac_name_t 			name;
		struct metac_type * 	ptr;
	}item[];
};

#define METAC_TYPES_ARRAY METAC(types, array)
#define METAC_TYPES_ARRAY_SYMBOL "metac__types_array"
#define METAC_DECLARE_EXTERN_TYPES_ARRAY extern struct metac_type_sorted_array METAC_TYPES_ARRAY
struct metac_type * metac_type_by_name(struct metac_type_sorted_array * array, metac_name_t name);

struct metac_object {
	struct metac_type *		type;
	void *					ptr;
	int 					fixed_part_byte_size;
	int 					flexible_part_byte_size;
	/*todo: reference count (0 for objects that were initialized by METAC_OBJECT/METAC_FUNCTION)*/
	int						ref_count;
};
#define METAC_OBJECT_NAME(name) METAC(object, name)
#define METAC_OBJECT(_type_, _name_) \
	METAC_TYPE_GENERATE(_type_); \
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
		metac_name_t 			name;
		struct metac_object * 	ptr;
	}item[];
};

#define METAC_OBJECTS_ARRAY METAC(objects, array)
#define METAC_OBJECTS_ARRAY_SYMBOL "metac__objects_array"
#define METAC_DECLARE_EXTERN_OBJECTS_ARRAY extern struct metac_object_sorted_array METAC_OBJECTS_ARRAY
struct metac_object * metac_object_by_name(struct metac_object_sorted_array * array, metac_name_t name);
///*for dynamic objects*/
//struct metac_object * metac_object_get(struct metac_object * object);
//int metac_object_put(struct metac_object * object);

#ifdef __cplusplus
}
#endif
#endif /* METAC_H_ */
