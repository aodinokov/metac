#ifndef METAC_H_
#define METAC_H_

#include <dwarf.h>

#ifdef __cplusplus
extern "C" {
#endif

/* declaration of C type in C */
struct metac_type;
struct metac_type_at;
struct metac_type_p_at;

/* definition of types used for attributes */
typedef char *							metac_name_t;
typedef struct metac_type				metac_type_t;
typedef struct metac_type_at			metac_type_at_t;
typedef unsigned int					metac_byte_size_t;
typedef unsigned int					metac_encoding_t;
typedef unsigned int					metac_data_location_t;
typedef metac_data_location_t			metac_data_member_location_t;
typedef unsigned int					metac_bit_offset_t;
typedef unsigned int					metac_bit_size_t;
typedef unsigned int					metac_bound_t;
typedef unsigned int					metac_count_t;
typedef unsigned long					metac_const_value_t;

typedef int								metac_type_id_t;
typedef int								metac_type_at_id_t;
typedef unsigned int					metac_num_t;


/**
 * definition of C type attributes in C (based on DWARF)
 * defined here because these types are used by metac.awk during generation of type structure
 * possible to move to metac_impl.h
 **/
struct metac_type_at {
	metac_type_at_id_t					id;
	union {
		metac_name_t					name;						/* universal field */
		metac_type_t *					type;						/* universal field */
		metac_byte_size_t				byte_size;					/* type size */
		metac_encoding_t				encoding;					/* type encoding (DW_ATE_signed etc) */
		metac_data_member_location_t	data_member_location;		/* member offset in structs and unions */
		metac_bit_offset_t				bit_offset;					/* bit-field member bit offset in structs and unions */
		metac_bit_size_t				bit_size;					/* bit-field member bit size in structs and unions */
		metac_bound_t					lower_bound;				/* for array_ranges*/
		metac_bound_t					upper_bound;				/* for array_ranges*/
		metac_count_t					count;						/* for array_ranges*/
		metac_const_value_t				const_value;				/* for enums*/
	};
};

///*
// * for metac_type_id(type) == DW_TAG_base_type
// */
//struct metac_type_base_type_info {
//	metac_name_t		name;					/* name of type (can be NULL for anonymous enums) */
//	metac_byte_size_t	byte_size;				/* mandatory field */
//	metac_encoding_t	encoding;				/* type encoding (DW_ATE_signed etc) */
//};
///*
// * for metac_type_id(type) == DW_TAG_pointer_type
// */
//struct metac_type_pointer_type_info {
//	metac_name_t		name;					/* name of type (can be NULL for anonymous enums) */
//	metac_byte_size_t	byte_size;				/* ?mandatory field */
//	metac_type_t *		type;					/* universal field */
//};
///*
// * for metac_type_id(type) == DW_TAG_typedef
// */
//struct metac_type_typedef_info {
//	metac_name_t		name;					/* name of type (can be NULL for anonymous enums) */
//	metac_type_t *		type;					/* universal field */
//};
///*
// * for metac_type_id(type) == DW_TAG_enumeration_type
// */
//struct metac_type_enumeration_type_info {
//	metac_name_t		name;					/* name of type (can be NULL for anonymous enums) */
//	metac_byte_size_t	byte_size;				/* mandatory field */
//	metac_num_t			enumerators_count;		/* mandatory field */
//	struct {
//		metac_name_t		name;					/* enumerator name */
//		metac_const_value_t	const_value;			/* enumerator value */
//	}enumerators[];
//};
///*
// * for metac_type_id(type) == DW_TAG_subprogram
// */
//struct metac_type_subprogram_info {
//	metac_type_t *	type;			/* function return type (NULL if void) */
//	metac_name_t	name;					/* function name */
//	metac_num_t		parameters_count;		/* number of parameters */
//	struct {
//		int				unspecified_parameters;	/*if 1 - after that it's possible to have a lot of arguments*/
//		metac_type_t *	type;					/* parameter type */
//		metac_name_t	name;					/* parameter name */
//	}parameters[];
//};
///*
// * for metac_type_id(type) == DW_TAG_structure_type
// */
//struct metac_type_structure_info {
//	metac_name_t					name;						/* name of the structure (may be NULL)*/
//	metac_byte_size_t				byte_size;					/* size of the structure in bytes */
//	metac_num_t						members_count;				/* number of members */
//	struct {
//		metac_type_t *					type;						/* type of the member (mandatory) */
//		metac_name_t					name;						/* name of the member (mandatory) */
//		metac_data_member_location_t *	p_data_member_location;		/* location - offset in bytes (mandatory only for structure members) */
//		metac_bit_offset_t *			p_bit_offset;				/* bit offset - used only when bits were specified. Can be NULL */
//		metac_bit_size_t *				p_bit_size;					/* bit size - used only when bits were specified. Can be NULL */
//	}members[];
//};
///*
// * for metac_type_id(type) == DW_TAG_union_type
// */
//struct metac_type_union_info {
//	metac_name_t					name;						/* name of the union (may be NULL)*/
//	metac_byte_size_t				byte_size;					/* size of the union in bytes */
//	metac_num_t						members_count;				/* number of members */
//	struct {
//		metac_type_t *					type;						/* type of the member (mandatory) */
//		metac_name_t					name;						/* name of the member (mandatory) */
//		metac_data_member_location_t *	p_data_member_location;		/* location - offset in bytes (mandatory only for structure members) */
//		metac_bit_offset_t *			p_bit_offset;				/* bit offset - used only when bits were specified. Can be NULL */
//		metac_bit_size_t *				p_bit_size;					/* bit size - used only when bits were specified. Can be NULL */
//	}members[];
//};
///*
// * special functions to work with arrays when metac_type_id(type) == DW_TAG_array_type (array)
// */
//struct metac_type_array_info {
//	metac_type_t *					type;					/* type of elements */
//	int 							is_flexible;			/* 1 - if array is flexible ??? may be create a macro and use elements_count == -1*/
//	metac_count_t					elements_count;			/* based on min and max or count - length of the array ---- TBD: to rework it for multi-dimensional case */
//	metac_num_t						subranges_count;		/* number of subranges ( >1 if array is multi-dimensional) */
//	struct {
//		metac_type_t *					type;					/* type of index */
//		metac_count_t *					p_count;				/* number of elements in subrange */
//		metac_bound_t *					p_lower_bound;			/* min index in subrange */
//		metac_bound_t *					p_upper_bound;			/* max index in subrange */
//	}subranges[];
//};



/*type additional specifications (alignment and etc) */
struct metac_type_specification {
	const char *key,*value;
};

struct metac_type {
	metac_type_id_t						id;							/* type id */

	/* RAW DWARF data */
	metac_num_t							child_num;					/* number of children */
	metac_type_t **						child;						/* pointer to array of children */
	metac_num_t							at_num;						/* number of attributes */
	metac_type_at_t *					at;							/* pointer to array of attributes */

	/* METAC specific data */
	struct metac_type_specification	*	specifications;				/* pointer to explicit specifications array for this type*/
	union {
		/* .id == DW_TAG_base_type */
		struct {
			metac_name_t		name;					/* name of type (can be NULL for anonymous enums) */
			metac_byte_size_t	byte_size;				/* mandatory field */
			metac_encoding_t	encoding;				/* type encoding (DW_ATE_signed etc) */
		}base_type_info;
		/* .id == DW_TAG_pointer_type */
		struct {
			metac_name_t		name;					/* name of type (can be NULL for anonymous enums) */
			metac_byte_size_t	byte_size;				/* ?mandatory field */
			metac_type_t *		type;					/* universal field */
		}pointer_type_info;
		/* .id == DW_TAG_typedef */
		struct {
			metac_name_t		name;					/* name of type (can be NULL for anonymous enums) */
			metac_type_t *		type;					/* universal field */
		}typedef_info;
		/* .id == DW_TAG_enumeration_type */
		struct {
			metac_name_t		name;					/* name of type (can be NULL for anonymous enums) */
			metac_byte_size_t	byte_size;				/* mandatory field */
			metac_num_t			enumerators_count;		/* mandatory field */
			struct enumerator_info {
				metac_name_t		name;					/* enumerator name */
				metac_const_value_t	const_value;			/* enumerator value */
			}*enumerators;
		}enumeration_type_info;
		/* .id == DW_TAG_subprogram */
		struct {
			metac_type_t *	type;					/* function return type (NULL if void) */
			metac_name_t	name;					/* function name */
			metac_num_t		parameters_count;		/* number of parameters */
			struct subprogram_parameter {
				int				unspecified_parameters;	/*if 1 - after that it's possible to have a lot of arguments*/
				metac_type_t *	type;					/* parameter type */
				metac_name_t	name;					/* parameter name */
			}*parameters;
		}subprogram_info;
		/* .id = DW_TAG_structure_type */
		struct {
			metac_name_t					name;						/* name of the structure (may be NULL)*/
			metac_byte_size_t				byte_size;					/* size of the structure in bytes */
			metac_num_t						members_count;				/* number of members */
			struct member_info {
				metac_type_t *					type;						/* type of the member (mandatory) */
				metac_name_t					name;						/* name of the member (mandatory) */
				metac_data_member_location_t *	p_data_member_location;		/* location - offset in bytes (mandatory only for structure members) */
				metac_bit_offset_t *			p_bit_offset;				/* bit offset - used only when bits were specified. Can be NULL */
				metac_bit_size_t *				p_bit_size;					/* bit size - used only when bits were specified. Can be NULL */
			}*members;
		}structure_type_info;
		/* .id == DW_TAG_union_type */
		struct {
			metac_name_t					name;						/* name of the union (may be NULL)*/
			metac_byte_size_t				byte_size;					/* size of the union in bytes */
			metac_num_t						members_count;				/* number of members */
			struct member_info *members;
		}union_type_info;
		/* .id == DW_TAG_array_type */
		struct {
			metac_type_t *					type;					/* type of elements */
			int 							is_flexible;			/* 1 - if array is flexible ??? may be create a macro and use elements_count == -1*/
			metac_count_t					elements_count;			/* based on min and max or count - length of the array ---- TBD: to rework it for multi-dimensional case */
			metac_num_t						subranges_count;		/* number of subranges ( >1 if array is multi-dimensional) */
			struct subrange_info {
				metac_type_t *					type;					/* type of index */
				metac_count_t *					p_count;				/* number of elements in subrange */
				metac_bound_t *					p_lower_bound;			/* min index in subrange */
				metac_bound_t *					p_upper_bound;			/* max index in subrange */
			}*subranges;
		}array_type_info;
	};
};

/* some basic functions to navigate in structure metac_type */
metac_type_id_t			metac_type_id(struct metac_type *type);
metac_num_t				metac_type_child_num(struct metac_type *type);
struct metac_type*		metac_type_child(struct metac_type *type, unsigned int i);
metac_num_t				metac_type_at_num(struct metac_type *type);
struct metac_type_at* 	metac_type_at(struct metac_type *type, unsigned int i);
struct metac_type_p_at*	metac_type_p_at(struct metac_type *type);
struct metac_type_at*	metac_type_at_by_id(struct metac_type *type, metac_type_at_id_t id);	/* find id for the first child with the given name */
const char*				metac_type_specification(struct metac_type *type, const char *key);		/* return spec value by key (NULL if not found)*/


/* basic example that use metac_type_at_by_key */
metac_name_t			metac_type_name(struct metac_type *type);
int 					metac_type_child_id_by_name(struct metac_type *type, metac_name_t name);
metac_byte_size_t		metac_type_byte_size(struct metac_type *type);	/*< returns length in bytes of any type */

///* special function to work with typedef */
//struct metac_type *		metac_type_typedef_skip(struct metac_type *type);	/*< returns real type*/
//
//int metac_type_enumeration_type_info(struct metac_type *type, struct metac_type_enumeration_type_info *p_info);		/*< returns enum type info*/
//int metac_type_enumeration_type_enumerator_info(struct metac_type *type, unsigned int i,
//		struct metac_type_enumerator_info *p_info);	/*< returns Nth element info */

//int metac_type_subprogram_info(struct metac_type *type, struct metac_type_subprogram_info *p_info);		/*< returns subprogram type info*/
//int metac_type_subprogram_parameter_info(struct metac_type *type, unsigned int i,
//		struct metac_type_parameter_info *p_info);		/*< returns subprogram parameter info*/

/*
 * special functions when metac_type_id(type) == DW_TAG_member (element of structure or union)
 */
//int metac_type_structure_info(struct metac_type *type, struct metac_type_structure_info *p_info);		/*< returns subprogram type info*/
//int metac_type_structure_member_info(struct metac_type *type, metac_num_t i,
//		struct metac_type_member_info *p_info);		/*< returns subprogram parameter info*/

//int metac_type_union_info(struct metac_type *type, struct metac_type_union_info *p_info);		/*< returns subprogram type info*/
//int metac_type_union_member_info(struct metac_type *type, unsigned int i,
//		struct metac_type_member_info *p_info);		/*< returns union parameter info*/

//int metac_type_array_info(struct metac_type *type, struct metac_type_array_info *p_info);		/*< returns subprogram type info*/
//int metac_type_array_subrange_info(struct metac_type *type, unsigned int i,
//		struct metac_type_subrange_info *p_info);			/*< returns i-th array subrange info*/
//int metac_type_array_element_info(struct metac_type *type, unsigned int i,
//		struct metac_type_element_info *p_element_info);	/*< returns i-th element info */

#define _METAC(x, name) metac__ ## x ## _ ## name
#define METAC(x, name) _METAC(x, name)
/* macroses to export C type definitions and their params in code*/
#define METAC_TYPE_NAME(name) METAC(type, name)
#define METAC_TYPE_GENERATE(name) extern struct metac_type METAC_TYPE_NAME(name)

#define METAC_TYPE_SPECIFICATION_DECLARE(name) extern struct metac_type_specification METAC(typespec, name)[];
#define METAC_TYPE_SPECIFICATION_BEGIN(name) struct metac_type_specification METAC(typespec, name)[] = {
#define _METAC_TYPE_SPECIFICATION(_key_, _value_) {.key = _key_, .value = _value_},
#define METAC_TYPE_SPECIFICATION_END {NULL, NULL}};


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
/*for dynamic objects*/
struct metac_object * metac_object_get(struct metac_object * object);
int metac_object_put(struct metac_object * object);

#ifdef __cplusplus
}
#endif
#endif /* METAC_H_ */
