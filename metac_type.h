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
		long const_value;					/* for enums*/
    };
};

struct metac_type {
    int type;
    unsigned int child_num;
    struct metac_type **child;
    unsigned int at_num;
    struct metac_type_at *at;
};

/* some basic functions to navigate in structure metac_type */
static inline int 		metac_type(struct metac_type *type){return type->type;}
unsigned int 			metac_type_child_num(struct metac_type *type);
struct metac_type* 		metac_type_child(struct metac_type *type, unsigned int id);
unsigned int 			metac_type_at_num(struct metac_type *type);
struct metac_type_at* 	metac_type_at(struct metac_type *type, unsigned int id);
/* some service functions to navigate in metac_type */
/* easy function to find at by at.key */
struct metac_type_at* 	metac_type_at_by_key(struct metac_type *type, int key);
/* basic examples that use metac_type_at_by_key */
char *					metac_type_name(struct metac_type *type);
unsigned int 			metac_type_byte_size(struct metac_type *type);	/*< returns length in bytes of any type */
/* generalized map approach: pointer + function to iterate by at array */
typedef int (*metac_type_at_map_func_t)(struct metac_type *type, struct metac_type_at *at, void * data);
int metac_type_at_map(struct metac_type *type, metac_type_at_map_func_t map_func, void * data);

/* special functions when metac_type(type) == DW_TAG_subprogram */
struct metac_type_subprogram_info {
	struct metac_type * return_type;
	char * name;
	unsigned int parameters_count;
};
struct metac_type_parameter_info {
	int unspecified_parameters;	/*if 1 - after that it's possible to have a lot of arguments*/
	struct metac_type * type;
	char * name;
};
int metac_type_subprogram_info(struct metac_type *type, struct metac_type_subprogram_info *p_info);		/*< returns subprogram type info*/
int metac_type_subprogram_parameter_info(struct metac_type *type, unsigned int N,
		struct metac_type_parameter_info *p_info);		/*< returns subprogram parameter info*/

/* special functions when metac_type(type) == DW_TAG_member (element of structure or union) */
struct metac_type_member_info {
	struct metac_type *	type;	/* mandatory field */
	char * name;				/* mandatory field */
	unsigned int * p_data_member_location;	/* mandatory field for structures, but optional for unions */
	unsigned int * p_bit_offset;/* optional field (may be NULL) */
	unsigned int * p_bit_size;	/* optional field (may be NULL) */
};
int metac_type_member_info(struct metac_type *type, struct metac_type_member_info * p_member_info);

/* special functions when metac_type(type) == DW_TAG_structure_type || metac_type(type) == DW_TAG_union_type */
unsigned int 			_metac_type_su_member_count(struct metac_type *type, int expected_type);
struct metac_type *		_metac_type_su_member(struct metac_type *type, int expected_type, unsigned int id);
struct metac_type *		_metac_type_su_member_by_name(struct metac_type *type, int expected_type, const char *parameter_name);

/* special functions when metac_type(type) == DW_TAG_structure_type */
static inline unsigned int 			metac_type_structure_member_count(struct metac_type *type) {return _metac_type_su_member_count(type, DW_TAG_structure_type);}
static inline struct metac_type *	metac_type_structure_member(struct metac_type *type, unsigned int id) {return _metac_type_su_member(type, DW_TAG_structure_type, id);}
static inline struct metac_type *	metac_type_structure_member_by_name(struct metac_type *type, const char *parameter_name) {return _metac_type_su_member_by_name(type, DW_TAG_structure_type, parameter_name);}

/* special functions when metac_type(type) == DW_TAG_union_type */
static inline unsigned int 			metac_type_union_member_count(struct metac_type *type) {return _metac_type_su_member_count(type, DW_TAG_union_type);}
static inline struct metac_type *	metac_type_union_member(struct metac_type *type, unsigned int id) {return _metac_type_su_member(type, DW_TAG_union_type, id);}
static inline struct metac_type *	metac_type_union_member_by_name(struct metac_type *type, const char *parameter_name) {return _metac_type_su_member_by_name(type, DW_TAG_union_type, parameter_name);}


/* special function to work with typedef */
struct metac_type *	metac_type_typedef_skip(struct metac_type *type);	/*< returns real type*/


/* special functions when metac_type(type) == DW_TAG_subrange_type */
struct metac_type_subrange_info {
	struct metac_type *	type;				/* index type */
	unsigned int * p_lower_bound;			/* min index in subrange */
	unsigned int * p_upper_bound;			/* max index in subrange */
};
int metac_type_subrange_info(struct metac_type *type, struct metac_type_subrange_info * p_subrange_info);

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

/* special functions to work with enumeration type when metac_type(type) == DW_TAG_enumeration_type (enum) */
struct metac_type_enumeration_type_info {
	char * name;				/* mandatory field */
	unsigned int byte_size;		/* mandatory field */
	unsigned int enumerators_count;	/* mandatory field */
};
struct metac_type_enumerator_info {
	char * name;				/* mandatory field */
	long const_value;		/* mandatory field */
};

int metac_type_enumeration_type_info(struct metac_type *type, struct metac_type_enumeration_type_info *p_info);		/*< returns enum type info*/
int metac_type_enumeration_type_enumerator_info(struct metac_type *type, unsigned int N,
		struct metac_type_enumerator_info *p_info);	/*< returns Nth element info */



/* TODO: to be extended by other types */

/* macroses to export C type definitions in code*/
#define _METAC_TYPE(name) metac__type_   ## name
#define METAC_TYPE(name) _METAC_TYPE(name)
#define METAC_EXPORT_TYPE(name)            extern struct metac_type *METAC_TYPE(name)

#endif /* METAC_H_ */
