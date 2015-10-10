#include <stdio.h>
#include <assert.h>

#include <ffi.h>
#include <jansson.h>


#include "ffi_meta.h"

/*TODO: to use some logging lib */
#define msg_stddbg(...) fprintf(stderr, ##__VA_ARGS__)
#define msg_stderr(...) fprintf(stderr, ##__VA_ARGS__)


struct ffi_meta_type_at* ffi_meta_type_get_at(struct ffi_meta_type *type, int key) {
	int i;

	if (type == NULL)
		return NULL;

	for (i = 0; i < type->at_num; i++) {
		if (type->at[i].key == key)
			return &type->at[i];
	}
	return NULL;
}

struct ffi_meta_type *	ffi_meta_type_subprogram_return_type(struct ffi_meta_type *type) {
	struct ffi_meta_type_at * res;

	if (type->type != DW_TAG_subprogram) {
		msg_stderr("_DW_TAG_subprogram_ expected type == DW_TAG_subprogram\n");
		return NULL;
	}

	res = ffi_meta_type_get_at(type, DW_AT_type);
	assert(res);

	return res->type;
}
int 					ffi_meta_type_subprogram_parameter_count(struct ffi_meta_type *type) {
	int i;
	int res = 0;

	if (type->type != DW_TAG_subprogram) {
		msg_stderr("_DW_TAG_subprogram_ expected type == DW_TAG_subprogram\n");
		return -1;
	}

	for (i = 0; i < type->child_num; i++) {
		if (type->child[i]->type == DW_TAG_formal_parameter)
			res++;
	}
	return res;
}
struct ffi_meta_type *	ffi_meta_type_subprogram_parameter_by_id(struct ffi_meta_type *type, unsigned int parameter_id) {
	int i, j = 0;
	struct ffi_meta_type * res = NULL;

	if (type->type != DW_TAG_subprogram) {
		msg_stderr("_DW_TAG_subprogram_ expected type == DW_TAG_subprogram\n");
		return NULL;
	}

	for (i = 0; i < type->child_num; i++) {
		if (type->child[i]->type == DW_TAG_formal_parameter) {
			if (j == parameter_id) {
				res = type->child[i];
				break;
			}
			/* TODO: in fact we can do it without loop if this confistion is always true*/
			assert(type->child[i]->type == DW_TAG_formal_parameter);

			j++;
		}
	}

	/*TODO: common code with the next function? - to a separete function */
	if (res) { /* formal parameter found */
		struct ffi_meta_type_at * at_type;
		at_type = ffi_meta_type_get_at(res, DW_AT_type);

		res = NULL;
		if (at_type)
			res = at_type->type;

	}

	return res;
}
struct ffi_meta_type *	ffi_meta_type_subprogram_parameter_by_name(struct ffi_meta_type *type, const char *parameter_name) {
	int i, j = 0;
	struct ffi_meta_type * res = NULL;

	if (type->type != DW_TAG_subprogram) {
		msg_stderr("_DW_TAG_subprogram_ expected type == DW_TAG_subprogram\n");
		return NULL;
	}

	for (i = 0; i < type->child_num; i++) {
		if (type->child[i]->type == DW_TAG_formal_parameter) {
			struct ffi_meta_type_at * at;

			at = ffi_meta_type_get_at(type->child[i], DW_AT_name);
			assert(at);
			if (at && strcmp(parameter_name, at->name) == 0) {
				res = type->child[i];
				break;
			}
			j++;
		}
	}

	/*TODO: common code with the previous function? - to a separate function */
	if (res) { /* formal parameter found */
		struct ffi_meta_type_at * at;
		at = ffi_meta_type_get_at(res, DW_AT_type);

		res = NULL;
		if (at)
			res = at->type;

	}

	return res;
}

struct ffi_meta_type *ffi_meta_type_typedef_skip(struct ffi_meta_type *type) {
	assert(type);
	if (type->type == DW_TAG_typedef){
		struct ffi_meta_type_at * at_type;
		at_type = ffi_meta_type_get_at(type, DW_AT_type);
		if (at_type == NULL) {
			msg_stderr("typedef has to contain type at\n");
			return NULL;
		}
		return ffi_meta_type_typedef_skip(at_type->type);
	}
	return type;
}

unsigned int ffi_meta_structure_member_count(struct ffi_meta_type *type) {
	unsigned int i;
	unsigned int count = 0;

	assert(type);
	type = ffi_meta_type_typedef_skip(type);
	assert(type);

	if (type->type != DW_TAG_structure_type) {
		msg_stderr("expected type DW_TAG_structure_type\n");
		return 0;
	}
	for (i = 0; i < type->child_num; i++) {
		assert(type->child[i]->type == DW_TAG_member);
		if (type->child[i]->type == DW_TAG_member)
			count++;
	}
	return count;
}


struct ffi_meta_object {
	    struct ffi_meta_type *_type;		/*< type that was used to call object_create function of this object (instance)*/
	    struct ffi_meta_type *type;	/*< ffi_meta_type_typedef_skip(type) - it contains all necessary info to really create object */

	    unsigned int count;				/* 1 in normal state, array len >0 if we need array*/

	    void *data;					/*pointer to memory*/
	    unsigned int data_length; 	/*keep length for bounds checking*/
	    unsigned char parent_data_memory; /*flag that p_mem wasn't allocated by this object*/

	    struct ffi_meta_object *p_agg; /*when type doesn't use children you use DW_AT_type*/
	    struct ffi_meta_object **child;/*when we use children*/
};

void 						ffi_meta_object_destroy(struct ffi_meta_object *object) {
	unsigned int i;

	assert(object);
	assert(object->type);
	assert(object->count > 0);

	if (object->p_agg)
		free(object->p_agg);
	object->p_agg = NULL;

	for (i = 0; i < object->count * object->type->child_num; i++) {
		if (object->child[i])
			ffi_meta_object_destroy(object->child[i]);
		object->child[i] = NULL;
	}
	if (object->child)
		free(object->child);
	object->child = NULL;

	if (!object->parent_data_memory && object->data)
		free(object->data);
	object->data = NULL;
	object->data_length = 0;

	free(object);
}
static struct ffi_meta_object * 	_ffi_meta_object_create(struct ffi_meta_type *_type,
		unsigned int count,
		void *data,
		unsigned int max_data_lenth,
		unsigned int * p_data_lenth) {
	struct ffi_meta_type *type;
	struct ffi_meta_type_at * at_byte_size;
	struct ffi_meta_object * object;
	assert(_type);
	assert(count > 0);

	type = ffi_meta_type_typedef_skip(_type);
	assert(type);

	at_byte_size = ffi_meta_type_get_at(type, DW_AT_byte_size);
	assert(data || at_byte_size);

	/* create object itself */
	object = (struct ffi_meta_object *)calloc(1,  sizeof(struct ffi_meta_object));
	if (object == NULL) {
		msg_stderr("can't allocate memory for object\n");
		return NULL;
	}

	/*init fields*/
	object->type = type;
	object->_type = _type;
	object->count = count;

	if (data == NULL) { /* take care of memory unless parent doesn't do that*/
		object->data = calloc(count, at_byte_size->byte_size);
		if (object->data == NULL) {
			msg_stderr("can't allocate memory for p_mem\n");
			ffi_meta_object_destroy(object);
			return NULL;
		}
		object->parent_data_memory = 0;
		max_data_lenth = at_byte_size->byte_size * object->count;
	} else {
		object->data = data;
		object->parent_data_memory = 1;
	}

	/* work with agg and children */
	switch(object->type->type) {
	case DW_TAG_base_type:
	case DW_TAG_pointer_type:
		assert(at_byte_size);

		object->data_length = at_byte_size->byte_size * object->count;

		if (object->data_length > max_data_lenth) {
			msg_stderr("max_data_lenth is too small: %d > %d\n", object->data_length, max_data_lenth);
			ffi_meta_object_destroy(object);
			return NULL;
		}

		if (p_data_lenth)
			*p_data_lenth = object->data_length;
		break;
	case DW_TAG_structure_type:
		{
			unsigned int global_offset = 0;
			unsigned int global_id;
			unsigned int id_in_array;

			assert(at_byte_size);

			/*create count structures with child_num members*/
			object->child = (struct ffi_meta_object **)calloc(object->count * object->type->child_num, sizeof(struct ffi_meta_object *));
			if (object->child == NULL) {
				msg_stderr("can't allocate memory for sub-objects\n");
				ffi_meta_object_destroy(object);
				return NULL;
			}

			for (id_in_array = 0; id_in_array < object->count; id_in_array++) {
				unsigned int child;

				for (child = 0; child < object->type->child_num; child++) {
					unsigned int child_data_lenth = 0;
					unsigned int child_max_data_lenth = 0;

					struct ffi_meta_type_at * at_data_member_location =
							ffi_meta_type_get_at(type->child[child], DW_AT_data_member_location); /*TODO: invert cycles for better performance*/
					assert(at_data_member_location);

					/*calc max_data_size*/
					if (child < object->type->child_num - 1) {
						struct ffi_meta_type_at * at_data_member_location_next =
								ffi_meta_type_get_at(type->child[child+1], DW_AT_data_member_location); /*TODO: invert cycles for better performance*/
						assert(at_data_member_location_next);
						assert(at_data_member_location_next->data_member_location > at_data_member_location->data_member_location);
						child_max_data_lenth = at_data_member_location_next->data_member_location - at_data_member_location->data_member_location;
					} else {
						child_max_data_lenth = at_byte_size->byte_size - at_data_member_location->data_member_location;
					}

					global_id = id_in_array * object->type->child_num + child;


					object->child[global_id] = _ffi_meta_object_create(type->child[child], 1,
							((char*)object->data) + global_offset + at_data_member_location->data_member_location,
							child_max_data_lenth, &child_data_lenth);

				}
				global_offset += at_byte_size->byte_size;
			}
			assert(global_offset <= max_data_lenth);
			if (p_data_lenth)
				*p_data_lenth = global_offset;
		}while(0);
		break;
	case DW_TAG_array_type:
		{
			unsigned int global_offset = 0;
			unsigned int global_id = 0;
			unsigned int id;
			struct ffi_meta_type_at * at_type =
					ffi_meta_type_get_at(type, DW_AT_type); /*type of array elements*/
			assert(at_type);

			object->child = (struct ffi_meta_object **)calloc(object->count * object->type->child_num,
					sizeof(struct ffi_meta_object *));
			if (object->child == NULL) {
				msg_stderr("can't allocate memory for sub-objects\n");
				ffi_meta_object_destroy(object);
				return NULL;
			}

			for (id = 0; id < object->count; id++) {
				unsigned int child;
				for (child = 0; child < object->type->child_num; child++) {
					unsigned int child_data_lenth = 0;
					unsigned int elements_count = 0;
					struct ffi_meta_type_at * at_lower_bound,
											* at_upper_bound;

					global_id = id * object->type->child_num + child;
					assert(type->child[child]->type == DW_TAG_subrange_type);

					/* get range parameter */
					at_lower_bound = ffi_meta_type_get_at(type->child[child], DW_AT_lower_bound); /*optional*/
					at_upper_bound = ffi_meta_type_get_at(type->child[child], DW_AT_upper_bound); /*mandatory?*/
					if (at_upper_bound == NULL) {
						msg_stderr("upper_bound is mandatory\n");
						ffi_meta_object_destroy(object);
						return NULL;
					}
					elements_count = at_upper_bound->upper_bound + 1;
					if (at_lower_bound)
						elements_count -= at_lower_bound->lower_bound;

					/*try to create array of objects*/
					object->child[global_id] = _ffi_meta_object_create(at_type->type, elements_count,
							((char*)object->data) + global_offset, max_data_lenth - global_offset, &child_data_lenth);
					if (object->child[global_id] == NULL) {
						msg_stderr("failed to create subrange_type\n");
						ffi_meta_object_destroy(object);
						return NULL;
					}
					assert(max_data_lenth >= child_data_lenth);
					global_offset += child_data_lenth;
					max_data_lenth -= child_data_lenth;
				}
			}
			if (p_data_lenth)
				*p_data_lenth = global_offset;
		}while(0);
		break;
	case DW_TAG_member:
		{
			struct ffi_meta_type_at * at_type =
					ffi_meta_type_get_at(object->type, DW_AT_type);
			assert(at_type);
			assert(count == 1);

			object->p_agg = _ffi_meta_object_create(at_type->type, count, data, max_data_lenth, p_data_lenth);
			if (object->p_agg == NULL) {
				msg_stderr("failed to create p_agg\n");
				ffi_meta_object_destroy(object);
				return NULL;
			}
		}while(0);
		break;
	default:
		msg_stderr("type %x is unsupported\n", object->type->type);
		assert(0/*TBD: not implemented yet*/);
	}

	return object;
}
struct ffi_meta_object * 	ffi_meta_object_create(struct ffi_meta_type *type) {
	return ffi_meta_object_array_create(type, 1);
}
struct ffi_meta_object * 	ffi_meta_object_array_create(struct ffi_meta_type *type, unsigned int count) {
	return _ffi_meta_object_create(type, count, NULL, 0, NULL);
}
struct ffi_meta_type * 		ffi_meta_object_type(struct ffi_meta_object *object) {
	return object->_type;
}
unsigned int 				ffi_meta_object_count(struct ffi_meta_object *object) {
	return object->count;
}
void *						ffi_meta_object_ptr(struct ffi_meta_object *object) {
	return object->data;
}

struct ffi_meta_object *	ffi_meta_object_structure_member_by_name(struct ffi_meta_object *object, const char *member_name) {
	unsigned int child;
	unsigned int id = 0;	/*TODO: object id in array if count > 0*/
	assert(object);
	assert(object->type);

	if (object->type->type != DW_TAG_structure_type) {
		msg_stderr("function expects DW_TAG_structure_type\n");
		return NULL;
	}

	for (child = 0; child < object->type->child_num; child++) {
		unsigned int global_id =
				id * object->type->child_num + child;
		struct ffi_meta_type_at * at_name =
				ffi_meta_type_get_at(object->child[global_id]->type, DW_AT_name);

		assert(at_name);
		if (strcmp(at_name->name, member_name) == 0) {
			return object->child[global_id];
		}
	}

	msg_stderr("failed to find member %s\n", member_name);
	return NULL;
}


void *ffi_meta_data(struct ffi_meta_object *p_object){

}

/* create real c types instances (with dependencies) */
void * ffi_meta_object_create_from_json(struct ffi_meta_type *type, json_t *json) {

}
int ffi_meta_destroy_mem(struct ffi_meta_type *type, void * p_mem) {

}


static int _init_data(void * p_mem, struct ffi_meta_type *type, json_t *json) {
	struct ffi_meta_type_at * at_type;
	if (type->type != DW_TAG_formal_parameter) {
		msg_stderr("_init_data can accept only DW_TAG_formal_parameter\n");
		return -1;
	}
	at_type = ffi_meta_type_get_at(type, DW_AT_type);
	if (!at_type) {
		msg_stderr("type doesn't have DW_AT_type\n");
		return -1;
	}
	switch (at_type->type->type) {
	case DW_TAG_base_type: {
		struct ffi_meta_type_at * at_name;
		at_name = ffi_meta_type_get_at(at_type->type, DW_AT_name);
		if (!at_name) {
			msg_stderr("type doesn't have DW_AT_name\n");
			return -1;
		}
		if (strcmp(at_name->name, "int") == 0) {
			if (!json_is_number(json)) {
				msg_stderr("expected number\n");
				return -1;
			}
			(*(int*)p_mem) = (int)json_integer_value(json);
			return 0;
		}
		if (strcmp(at_name->name, "unsigned int") == 0) {
			if (!json_is_number(json)) {
				msg_stderr("expected number\n");
				return -1;
			}
			(*(unsigned int*)p_mem) = (unsigned int)json_integer_value(json);
			return 0;
		}
		msg_stderr("returned NULL for base type: %s\n", at_name->name);
		return -1;
	}
	case DW_TAG_pointer_type:
		return -1;
	}
	msg_stderr("unsupported at_type->type->type\n");
	return -1;

}

static void _destroy_data(void * p_mem, struct ffi_meta_type *type) {

}

static ffi_type * _get_param_ffi_type(struct ffi_meta_type *type) {
	struct ffi_meta_type_at * at_type;
	if (type->type != DW_TAG_formal_parameter) {
		msg_stderr("_get_ffi_type can accept only DW_TAG_formal_parameter\n");
		return 0;
	}
	at_type = ffi_meta_type_get_at(type, DW_AT_type);
	if (!at_type) {
		msg_stderr("type doesn't have DW_AT_type\n");
		return 0;
	}
	switch (at_type->type->type) {
	case DW_TAG_base_type: {
		struct ffi_meta_type_at * at_name;
		at_name = ffi_meta_type_get_at(at_type->type, DW_AT_name);
		if (!at_name) {
			msg_stderr("type doesn't have DW_AT_name\n");
			return 0;
		}
		if (strcmp(at_name->name, "int") == 0)
			return &ffi_type_sint;
		if (strcmp(at_name->name, "unsigned int") == 0)
			return &ffi_type_uint;

		msg_stderr("returned NULL for base type: %s\n", at_name->name);
		return 0;
	}
	case DW_TAG_pointer_type:
		return &ffi_type_pointer;
	}
	msg_stderr("unsupported at_type->type->type\n");
	return NULL;
}

static size_t _get_type_size(struct ffi_meta_type *type) {
	struct ffi_meta_type_at * at_type;
	struct ffi_meta_type_at * at_byte_size;
	if (type->type != DW_TAG_formal_parameter) {
		msg_stderr("_get_type_size can accept only DW_TAG_formal_parameter\n");
		return 0;
	}
	at_type = ffi_meta_type_get_at(type, DW_AT_type);
	if (!at_type) {
		msg_stderr("type doesn't have DW_AT_type\n");
		return 0;
	}
	at_byte_size = ffi_meta_type_get_at(at_type->type, DW_AT_byte_size);
	if (!at_byte_size) {
		msg_stderr("type doesn't have DW_AT_byte_size\n");
		return 0;
	}
	msg_stddbg("_get_type_size: return %d\n", (int)at_byte_size->byte_size);
	return (size_t)at_byte_size->byte_size;
}

/* metadata: is metadata of DW_TAG_subprogram, json - TBD? only args or obj with name: "(cast)fn_name", args: array or obj*/
static int _DW_TAG_subprogram_(void *fn, struct ffi_meta_type *type, json_t *json) {

	if (type->type != DW_TAG_subprogram) {
		msg_stderr("_DW_TAG_subprogram_ expected type == DW_TAG_subprogram\n");
		return -1;
	}

	if (json_is_array(json)) {
		size_t i;

		ffi_cif cif;
		ffi_status status;

		size_t args_count;
		ffi_type **arg_types = NULL;
		void **arg_values = NULL;
		ffi_type *result_type = NULL;
		ffi_arg result;

		size_t formal_parameters_count = 0;
		struct ffi_meta_type **meta_types = NULL;

		args_count = json_array_size(json);
		msg_stddbg("args_count %d\n", (int)args_count);

		/* Check that we have the same number of formal parameters */
		for (i = 0; i < type->child_num; i++) {
			if (type->child[i]->type == DW_TAG_formal_parameter)
				formal_parameters_count++;
		}
		msg_stddbg("formal_parameters_count %d\n", (int)formal_parameters_count);

		if (formal_parameters_count != args_count) {
			msg_stderr("json array has incorrect number of elements\n");
			return -1;
		}

		if (args_count > 0) {
			/* allocate memory for all arguments */
			arg_types = (ffi_type **)calloc(sizeof(ffi_type *), args_count);
			if (arg_types == NULL) {
				msg_stderr("Can't allocate memory for arg_types\n");
				return -1;
			}

			arg_values = (void **)calloc(sizeof(void *), args_count);
			if (arg_values == NULL) {
				msg_stderr("Can't allocate memory for arg_values\n");
				free(arg_types);
				return -1;
			}

			meta_types = (struct ffi_meta_type **)calloc(sizeof(struct ffi_meta_type *), args_count);
			if (meta_types == NULL) {
				msg_stderr("Can't allocate memory for meta_types\n");
				free(arg_values);
				free(arg_types);
				return -1;
			}

			formal_parameters_count = 0;
			for (i = 0; i < type->child_num; i++) {
				if (type->child[i]->type == DW_TAG_formal_parameter) {
					meta_types[formal_parameters_count] = type->child[i];
					formal_parameters_count++;
				}
			}

			for (i = 0; i < args_count; i++) {
				json_t * array_element;
				/* convert arg_type to ffi_type, allocate memory for it */
				arg_types[i] = _get_param_ffi_type(meta_types[i]);
				arg_values[i] = calloc(_get_type_size(meta_types[i]), 1);
				/*TODO check if it wasn't allocated*/

				/* init data by Json string */
				array_element = json_array_get(json, i);
				_init_data(arg_values[i], meta_types[i], array_element);
				json_decref(array_element);
			}
		}

		/* TODO: convert return type to ffi_type*/
		result_type = &ffi_type_sint; //_get_ffi_type(meta_types[i])

		/* Prepare the ffi_cif structure */
		if ((status = ffi_prep_cif(&cif,
				FFI_DEFAULT_ABI,
				args_count,
				result_type,
				arg_types)) == FFI_OK)
		{
			ffi_call(&cif, FFI_FN(fn), &result, arg_values);
		} else {
			msg_stderr("ffi_prep_cif returned error\n");
		}

		if (args_count > 0) {
			for (i = 0; i < args_count; i++) {
				_destroy_data(arg_values[i], meta_types[i]);
			}
			free(meta_types);
			free(arg_values);
			free(arg_types);
		}
	} else {
		msg_stderr("Subprogram doesn't expect this type of json as argument\n");
		return -1;
	}

	return 0;
}


int ffi_meta_fn_call_args_json(void *fn, struct ffi_meta_type *type, char *json_string) {
	json_t *json;
	json_error_t jerr;

	json = json_loads(json_string, JSON_REJECT_DUPLICATES, &jerr);
	if (!json) {
		msg_stderr("Error loading JSON object; %s\n", jerr.text);
		return -1;
	}
	_DW_TAG_subprogram_(fn, type, json);
	json_decref(json);
	return 0;
}
