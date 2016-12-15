#ifndef METAC_H_
#define METAC_H_

#include "dwarf.h"

/* declaration of C type in C */
struct metac_type;
struct metac_type_at;
struct metac_type_p_at;

/* definition of types used for attributes */
typedef char *					metac_name_t;
typedef struct metac_type		metac_type_t;
typedef struct metac_type_at	metac_type_at_t;
typedef unsigned int			metac_byte_size_t;
typedef unsigned int			metac_encoding_t;
typedef unsigned int			metac_data_member_location_t;
typedef unsigned int			metac_bit_offset_t;
typedef unsigned int			metac_bit_size_t;
typedef unsigned int			metac_bound_t;
typedef long					metac_const_value_t;

typedef int						metac_type_id_t;
typedef int						metac_type_at_id_t;
typedef unsigned int			metac_num_t;


/**
 * definition of C type attributes in C (based on DWARF)
 * defined here because these types are used by metac.awk during generation of type structure
 * possible to move to metac_impl.h
 **/
struct metac_type_at {
	metac_type_at_id_t					id;
	union {
		metac_name_t					name;					/* universal field */
		metac_type_t *					type;					/* universal field */
		metac_byte_size_t				byte_size;				/* type size */
		metac_encoding_t				encoding;				/* type encoding (DW_ATE_signed etc) */
		metac_data_member_location_t	data_member_location;	/* member offset in structs and unions */
		metac_bit_offset_t				bit_offset;				/* bit-field member bit offset in structs and unions */
		metac_bit_size_t				bit_size;				/* bit-field member bit size in structs and unions */
		metac_bound_t					lower_bound;			/* for array_ranges*/
		metac_bound_t					upper_bound;			/* for array_ranges*/
		metac_const_value_t				const_value;			/* for enums*/
	};
};

struct metac_type_p_at {
	struct metac_type_at *		p_at_name;					/* universal field */
	struct metac_type_at *		p_at_type;					/* universal field */
	struct metac_type_at *		p_at_byte_size;				/* type size */
	struct metac_type_at *		p_at_encoding;				/* type encoding (DW_ATE_signed etc) */
	struct metac_type_at *		p_at_data_member_location;	/* member offset in structs and unions */
	struct metac_type_at *		p_at_bit_offset;			/* bit-field member bit offset in structs and unions */
	struct metac_type_at *		p_at_bit_size;				/* bit-field member bit size in structs and unions */
	struct metac_type_at *		p_at_lower_bound;			/* for array_ranges*/
	struct metac_type_at *		p_at_upper_bound;			/* for array_ranges*/
	struct metac_type_at *		p_at_const_value;			/* for enums*/
};


struct metac_type {
	metac_type_id_t		id;				/* type id */
	metac_num_t			child_num;		/* number of children */
	metac_type_t **		child;			/* pointer to array of children */
	metac_num_t			at_num;			/* number of attributes */
	metac_type_at_t *	at;				/* pointer to array of attributes */

	struct metac_type_p_at p_at;		/* pre-calculated attributes by name*/
};

/* some basic functions to navigate in structure metac_type */
metac_type_id_t			metac_type_id(struct metac_type *type);
metac_num_t				metac_type_child_num(struct metac_type *type);
struct metac_type*		metac_type_child(struct metac_type *type, unsigned int i);
metac_num_t				metac_type_at_num(struct metac_type *type);
struct metac_type_at* 	metac_type_at(struct metac_type *type, unsigned int i);
struct metac_type_p_at*	metac_type_p_at(struct metac_type *type);
struct metac_type_at*	metac_type_at_by_id(struct metac_type *type, metac_type_at_id_t id);


/* basic example that use metac_type_at_by_key */
metac_name_t			metac_type_name(struct metac_type *type);
int 					metac_type_child_id_by_name(struct metac_type *type, metac_name_t name);
/*???*/
metac_byte_size_t		metac_type_byte_size(struct metac_type *type);	/*< returns length in bytes of any type */

/* special function to work with typedef */
struct metac_type *		metac_type_typedef_skip(struct metac_type *type);	/*< returns real type*/

/*
 * special functions when metac_type(type) == DW_TAG_enumeration_type (enum)
 */
struct metac_type_enumeration_type_info {
	char *			name;					/* name of type (can be NULL for anonymous enums) */
	unsigned int	byte_size;				/* mandatory field */
	unsigned int	enumerators_count;		/* mandatory field */
};
struct metac_type_enumerator_info {
	char *			name;					/* enumerator name */
	long			const_value;			/* enumerator value */
};
int metac_type_enumeration_type_info(struct metac_type *type, struct metac_type_enumeration_type_info *p_info);		/*< returns enum type info*/
int metac_type_enumeration_type_enumerator_info(struct metac_type *type, unsigned int i,
		struct metac_type_enumerator_info *p_info);	/*< returns Nth element info */

/*
 * special functions when metac_type(type) == DW_TAG_subprogram
 */
struct metac_type_subprogram_info {
	metac_type_t *	type;			/* function return type (NULL if void) */
	metac_name_t	name;					/* function name */
	metac_num_t		parameters_count;		/* number of parameters */
};
struct metac_type_parameter_info {
	int				unspecified_parameters;	/*if 1 - after that it's possible to have a lot of arguments*/
	metac_type_t *	type;					/* parameter type */
	metac_name_t	name;					/* parameter name */
};
int metac_type_subprogram_info(struct metac_type *type, struct metac_type_subprogram_info *p_info);		/*< returns subprogram type info*/
int metac_type_subprogram_parameter_info(struct metac_type *type, unsigned int i,
		struct metac_type_parameter_info *p_info);		/*< returns subprogram parameter info*/

/*
 * special functions when metac_type(type) == DW_TAG_member (element of structure or union)
 */
struct metac_type_member_info {
	metac_type_t *					type;						/* type of the member (mandatory) */
	metac_name_t					name;						/* name of the member (mandatory) */
	metac_data_member_location_t *	p_data_member_location;		/* location - offset in bytes (mandatory only for structure members) */
	metac_bit_offset_t *			p_bit_offset;				/* bit offset - used only when bits were specified. Can be NULL */
	metac_bit_size_t *				p_bit_size;					/* bit size - used only when bits were specified. Can be NULL */
};

/*
 * special functions when metac_type(type) == DW_TAG_union_type
 */
struct metac_type_structure_info {
	metac_name_t					name;						/* name of the structure (may be NULL)*/
	metac_byte_size_t				byte_size;					/* size of the structure in bytes */
	metac_num_t						members_count;				/* number of members */
};
int metac_type_structure_info(struct metac_type *type, struct metac_type_structure_info *p_info);		/*< returns subprogram type info*/
int metac_type_structure_member_info(struct metac_type *type, unsigned int i,
		struct metac_type_member_info *p_info);		/*< returns subprogram parameter info*/

/*
 * special functions when metac_type(type) == DW_TAG_union_type
 */
struct metac_type_union_info {
	metac_name_t					name;						/* name of the union (may be NULL)*/
	metac_byte_size_t				byte_size;					/* size of the union in bytes */
	metac_num_t						members_count;				/* number of members */
};
int metac_type_union_info(struct metac_type *type, struct metac_type_union_info *p_info);		/*< returns subprogram type info*/
int metac_type_union_member_info(struct metac_type *type, unsigned int i,
		struct metac_type_member_info *p_info);		/*< returns union parameter info*/

/*
 * special functions when metac_type(type) == DW_TAG_subrange_type (array theoretically can contain several subranges)
 */
struct metac_type_subrange_info {
	metac_type_t *					type;					/* type of index */
	metac_bound_t *					p_lower_bound;			/* min index in subrange */
	metac_bound_t *					p_upper_bound;			/* max index in subrange */
};

/* special functions to work with arrays when metac_type(type) == DW_TAG_array_type (array) */
struct metac_type * metac_type_array_element_type(struct metac_type *type);		/*< returns array elements type */

unsigned int 		metac_type_array_subrange_count(struct metac_type *type);
struct metac_type * metac_type_array_subrange(struct metac_type *type, unsigned int id);

/* some additional helpful functions when metac_type(type) == DW_TAG_array_type (array) */
struct metac_type_element_info {
	struct metac_type *	type;
	unsigned int element_location;
};
int 				metac_type_array_element_info(struct metac_type *type, unsigned int N,
							struct metac_type_element_info *p_element_info);	/*< returns Nth element info */
unsigned int		metac_type_array_length(struct metac_type *type);	/*< returns length in elements of array */



/* TODO: to be extended by other types */

/* macroses to export C type definitions in code*/
#define _METAC_TYPE(name) metac__type_ ## name
#define METAC_TYPE(name) _METAC_TYPE(name)
#define METAC_EXPORT_TYPE(name) extern struct metac_type *METAC_TYPE(name)

#endif /* METAC_H_ */
