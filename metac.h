#ifndef METAC_H_
#define METAC_H_

#include "dwarf.h"

/*todo: consider renaming of lib to cmeta, mc or metaC instead of ffi_meta*/
/*TODO: unittests ideas:
 * 1. define type and check via this api that all fields are present and there are not extra fields.
 * 2. init object dinamically via API and check that all fields have right values
 * */

/* declaration of C type in C */
struct metac_type;

/**
 * definition of C type in C (based on DWARF)
 * defined here because these types are used by metac.awk during generation of type structure
 **/
struct metac_type_at{
    int key;
    union {
	char *name;
	unsigned int byte_size;
	unsigned int data_member_location;
	int encoding;
	struct metac_type *type;
	unsigned int lower_bound;
	unsigned int upper_bound;
	unsigned long const_value;
    };
};

struct metac_type {
    int type;
    unsigned int child_num;
    struct metac_type **child;
    unsigned int at_num;
    struct metac_type_at *at;
};

/* some fuctions to navigate in metac_type */
struct metac_type_at* metac_type_get_at(struct metac_type *type, int key);

/* special functions when metac_object_type(object) == DW_TAG_subprogram */
struct metac_type *	metac_type_subprogram_return_type(struct metac_type *type);
int 					metac_type_subprogram_parameter_count(struct metac_type *type);
struct metac_type *	metac_type_subprogram_parameter_by_id(struct metac_type *type, unsigned int parameter_id);
struct metac_type *	metac_type_subprogram_parameter_by_name(struct metac_type *type, const char *parameter_name);

/* special functions when metac_object_type(object) == DW_TAG_structure_type */
unsigned int 			metac_type_structure_member_count(struct metac_type *type);
const char *			metac_type_structure_member_name(struct metac_type *type);
struct metac_type *	metac_type_structure_member_type(struct metac_type *type);

/* special function to work with typedef */
struct metac_type *	metac_type_typedef_skip(struct metac_type *type);	/*< returns real type*/

/* special function to work with arrays */
unsigned int metac_type_byte_size(struct metac_type *type);	/*< returns length in bytes of any type */
unsigned int metac_type_array_length(struct metac_type *type);	/*< returns length in elements of array */

/* TODO: to be extended by other types */

/* macroses to export C type definitions in code*/
#define _METAC_TYPE(name) metac__type_   ## name
#define METAC_TYPE(name) _METAC_TYPE(name)
#define METAC_EXPORT_TYPE(name)            extern struct metac_type *METAC_TYPE(name)

/* definition of instance of the specific type in memory*/
struct metac_object;

/*TODO: rename to metac_array_create and make metac_object_create based on it */
struct metac_object * 	metac_object_create(struct metac_type *type);
struct metac_object * 	metac_object_array_create(struct metac_type *type, unsigned int count);
struct metac_type * 		metac_object_type(struct metac_object *object);
unsigned int 				metac_object_count(struct metac_object *object);
/*TODO: rename to metac_array_ptr */
void *						metac_object_ptr(struct metac_object *object, unsigned int *p_data_length);	/*<< Returns pointer to the C data*/
void 						metac_object_destroy(struct metac_object *object);

/* object structure functions */
struct metac_object *	metac_object_structure_member_by_name(struct metac_object *object, const char *member_name);

#endif /* METAC_H_ */
