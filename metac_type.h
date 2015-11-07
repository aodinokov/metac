#ifndef METAC_H_
#define METAC_H_

#include "dwarf.h"

/* declaration of C type in C */
struct metac_type;

/**
 * definition of C type in C (based on DWARF)
 * defined here because these types are used by metac.awk during generation of type structure
 * possible to move to metac_impl.h
 **/
struct metac_type_at{
    int key;
    union {
		char *name;							/* universal field */
		struct metac_type *type;			/* universal field */
		unsigned int byte_size;				/* type size */
		unsigned int encoding;				/* type encoding (DW_ATE_signed etc) */
		unsigned int data_member_location;	/* member offset in structs and unions */
		unsigned int bit_offset;			/* bit-field member bit offset in structs and unions */
		unsigned int bit_size;				/* bit-field member bit size in structs and unions */
		unsigned int lower_bound;			/* for array_ranges*/
		unsigned int upper_bound;			/* for array_ranges*/
		unsigned long const_value;			/* for enums*/
    };
};

struct metac_type {
    int type;
    unsigned int child_num;
    struct metac_type **child;
    unsigned int at_num;
    struct metac_type_at *at;
};

/* some basic fuctions to navigate in metac_type */
static inline int 		metac_type(struct metac_type *type){return type->type;}
unsigned int 			metac_type_child_num(struct metac_type *type);
struct metac_type* 		metac_type_child(struct metac_type *type, unsigned int id);
unsigned int 			metac_type_at_num(struct metac_type *type);
struct metac_type_at* 	metac_type_at(struct metac_type *type, unsigned int id);
/* some service fuctions to navigate in metac_type */
/* easy function to find at by at.key */
struct metac_type_at* 	metac_type_at_by_key(struct metac_type *type, int key);
/* basic examples that use metac_type_at_by_key */
char *					metac_type_name(struct metac_type *type);
unsigned int 			metac_type_byte_size(struct metac_type *type);	/*< returns length in bytes of any type */
/* generalized approach: pointer + function to iterate by at array */
typedef int (*metac_type_at_map_func_t)(struct metac_type *type, struct metac_type_at *at, void * data);
int metac_type_at_map(struct metac_type *type, metac_type_at_map_func_t map_func, void * data);

/* special functions when metac_type(type) == DW_TAG_subprogram */
struct metac_type *	metac_type_subprogram_return_type(struct metac_type *type);
int 					metac_type_subprogram_parameter_count(struct metac_type *type);
struct metac_type *	metac_type_subprogram_parameter(struct metac_type *type, unsigned int id);
struct metac_type *	metac_type_subprogram_parameter_by_name(struct metac_type *type, const char *parameter_name);

/* special functions when metac_type(type) == DW_TAG_member */
struct metac_type_member_info {
	struct metac_type *	type;	/* mandatory field */
	char * name;				/* mandatory field */
	unsigned int * p_data_member_location;	/* mandatory field */
	unsigned int * p_bit_offset;/* optional field (may be NULL) */
	unsigned int * p_bit_size;	/* optional field (may be NULL) */
};
int metac_type_member_info(struct metac_type *type, struct metac_type_member_info * p_member_info);

/* special functions when metac_type(type) == DW_TAG_structure_type */
unsigned int 			metac_type_structure_member_count(struct metac_type *type);
struct metac_type *		metac_type_structure_member(struct metac_type *type, unsigned int id);
struct metac_type *		metac_type_structure_member_by_name(struct metac_type *type, const char *parameter_name);

//struct metac_type *		metac_type_structure_member_type(struct metac_type *type, unsigned int id);
//char *					metac_type_structure_member_name(struct metac_type *type, unsigned int id);
//int						metac_type_structure_member_offset(struct metac_type *type, unsigned int id, unsigned int * p_offset);
//int 					metac_type_structure_member_bit_attr(struct metac_type *type, unsigned int id, unsigned int * p_bit_offset, unsigned int * p_bit_size);

/* special function to work with typedef */
struct metac_type *	metac_type_typedef_skip(struct metac_type *type);	/*< returns real type*/

/* special function to work with arrays */
unsigned int metac_type_array_length(struct metac_type *type);	/*< returns length in elements of array */

/* TODO: to be extended by other types */

/* macroses to export C type definitions in code*/
#define _METAC_TYPE(name) metac__type_   ## name
#define METAC_TYPE(name) _METAC_TYPE(name)
#define METAC_EXPORT_TYPE(name)            extern struct metac_type *METAC_TYPE(name)

#endif /* METAC_H_ */
