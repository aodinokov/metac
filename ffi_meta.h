#ifndef FFI_META_H_
#define FFI_META_H_

#include "dwarf.h"

/*todo: consider renaming of lib to cmeta, mc or metaC instead of ffi_meta*/
/*TODO: unittests ideas:
 * 1. define type and check via this api that all fields are present and there are not extra fields.
 * 2. init object dinamically via API and check that all fields have right values
 * */

/* declaration of C type in C */
struct ffi_meta_type;

/**
 * definition of C type in C (based on DWARF)
 * defined here because these types are used by ffi_meta.awk during generation of type structure
 **/
struct ffi_meta_type_at{
    int key;
    union {
	char *name;
	unsigned int byte_size;
	unsigned int data_member_location;
	int encoding;
	struct ffi_meta_type *type;
	unsigned int lower_bound;
	unsigned int upper_bound;
    };
};

struct ffi_meta_type {
    int type;
    unsigned int child_num;
    struct ffi_meta_type **child;
    unsigned int at_num;
    struct ffi_meta_type_at *at;
};

/* some fuctions to navigate in ffi_meta_type */
struct ffi_meta_type_at* ffi_meta_type_get_at(struct ffi_meta_type *type, int key);

/* special functions when ffi_meta_object_type(object) == DW_TAG_subprogram */
struct ffi_meta_type *	ffi_meta_type_subprogram_return_type(struct ffi_meta_type *type);
int 					ffi_meta_type_subprogram_parameter_count(struct ffi_meta_type *type);
struct ffi_meta_type *	ffi_meta_type_subprogram_parameter_by_id(struct ffi_meta_type *type, unsigned int parameter_id);
struct ffi_meta_type *	ffi_meta_type_subprogram_parameter_by_name(struct ffi_meta_type *type, const char *parameter_name);

/* special functions when ffi_meta_object_type(object) == DW_TAG_structure_type */
unsigned int 			ffi_meta_type_structure_member_count(struct ffi_meta_type *type);
const char *			ffi_meta_type_structure_member_name(struct ffi_meta_type *type);
struct ffi_meta_type *	ffi_meta_type_structure_member_type(struct ffi_meta_type *type);

/* special function to work with typedef*/
struct ffi_meta_type *	ffi_meta_type_typedef_skip(struct ffi_meta_type *type);	/*< returns real type*/

/* TODO: to be extended by other types */

/* macroses to export C type definitions in code*/
#define FFI_META_TYPE(name) ffi_meta__type_   ## name
#define FFI_META_EXPORT_TYPE(name)            extern struct ffi_meta_type *FFI_META_TYPE(name)

/* definition of instance of the specific type in memory*/
struct ffi_meta_object;

/*TODO: rename to ffi_meta_array_create and make ffi_meta_object_create based on it */
struct ffi_meta_object * 	ffi_meta_object_create(struct ffi_meta_type *type);
struct ffi_meta_object * 	ffi_meta_object_array_create(struct ffi_meta_type *type, unsigned int count);
struct ffi_meta_type * 		ffi_meta_object_type(struct ffi_meta_object *object);
unsigned int 				ffi_meta_object_count(struct ffi_meta_object *object);
/*TODO: rename to ffi_meta_array_ptr */
void *						ffi_meta_object_ptr(struct ffi_meta_object *object);	/*<< Returns pointer to the C data*/
void 						ffi_meta_object_destroy(struct ffi_meta_object *object);

/* object structure functions */
struct ffi_meta_object *	ffi_meta_object_structure_member_by_name(struct ffi_meta_object *object, const char *member_name);

/* (de)serialization example to be moved to ffi_meta_json lib */
#include <jansson.h>
struct ffi_meta_object * 	ffi_meta_object_from_json(struct ffi_meta_type *type, json_t *json);
json_t * 					ffi_meta_object_to_json(struct ffi_meta_object *object);

/*for now it's just example - this should be moved to ffi_meta_json lib */
int ffi_meta_fn_call_args_json(void *fn, struct ffi_meta_type *fn_meta_type, char *args);

#endif /* FFI_META_H_ */
