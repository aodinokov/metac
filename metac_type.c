#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "metac_type.h"

/*TODO: to use some logging lib */
#define msg_stddbg(...) fprintf(stderr, ##__VA_ARGS__)
#define msg_stderr(...) fprintf(stderr, ##__VA_ARGS__)


unsigned int 			metac_type_child_num(struct metac_type *type) {
	if (type == NULL)
		return 0;
	return type->child_num;
}

struct metac_type* 		metac_type_child(struct metac_type *type, unsigned int id) {
	if (type == NULL)
		return NULL;
	if (id >= type->child_num)
		return NULL;

	return type->child[id];
}

unsigned int 			metac_type_at_num(struct metac_type *type) {
	if (type == NULL)
		return 0;
	return type->at_num;
}

struct metac_type_at* 	metac_type_at(struct metac_type *type, unsigned int id)  {
	if (type == NULL)
		return NULL;
	if (id >= type->at_num)
		return NULL;

	return &type->at[id];
}

struct metac_type_at* metac_type_at_by_key(struct metac_type *type, int key) {
	int i;

	if (type == NULL)
		return NULL;

	for (i = 0; i < metac_type_at_num(type); i++) {
		struct metac_type_at* at = metac_type_at(type, i);
		if (at != NULL && at->key == key)
			return at;
	}
	return NULL;
}

unsigned int metac_type_byte_size(struct metac_type *type) {
	assert(type);
	type = metac_type_typedef_skip(type);
	assert(type);

	switch(type->type) {
	case DW_TAG_base_type:
	case DW_TAG_enumeration_type:
	case DW_TAG_structure_type:
	case DW_TAG_union_type:
		{
			struct metac_type_at * at_byte_size = metac_type_at_by_key(type, DW_AT_byte_size);
			assert(at_byte_size != NULL);
			return at_byte_size->byte_size;
		}while(0);
		break;
	case DW_TAG_pointer_type:
		{
			struct metac_type_at * at_byte_size = metac_type_at_by_key(type, DW_AT_byte_size);
			if (at_byte_size != NULL)
				return at_byte_size->byte_size;
			return sizeof(void*);
		}while(0);
		break;
	case DW_TAG_array_type:
		{
			struct metac_type_at * at_type;
			unsigned int elements_num = metac_type_array_length(type);

			at_type = metac_type_at_by_key(type, DW_AT_type);
			if (at_type == NULL) {
				msg_stderr("array has to contain type at\n");
				return 0;
			}
			return elements_num * metac_type_byte_size(at_type->type);


		}while(0);
		break;
	}
	return 0;
}

struct metac_type *metac_type_typedef_skip(struct metac_type *type) {
	assert(type);
	if (type->type == DW_TAG_typedef){
		struct metac_type_at * at_type;
		at_type = metac_type_at_by_key(type, DW_AT_type);
		if (at_type == NULL) {
			msg_stderr("typedef has to contain type at\n");
			return NULL;
		}
		return metac_type_typedef_skip(at_type->type);
	}
	return type;
}

struct metac_type * metac_type_member_type(struct metac_type *type) {
	struct metac_type_at * at_type;
	assert(type);
	if (type->type != DW_TAG_member) {
		msg_stderr("expected type DW_TAG_member\n");
		return NULL;
	}
	at_type = metac_type_at_by_key(type, DW_AT_type);
	if (at_type == NULL)
		return NULL;

	return at_type->type;
}

char * 		metac_type_member_name(struct metac_type *type)  {
	struct metac_type_at * at_name;
	assert(type);
	if (type->type != DW_TAG_member) {
		msg_stderr("expected type DW_TAG_member\n");
		return NULL;
	}
	at_name = metac_type_at_by_key(type, DW_AT_name);
	if (at_name == NULL)
		return NULL;

	return at_name->name;
}

int 		metac_type_member_offset(struct metac_type *type, unsigned int * p_offset) {
	struct metac_type_at * at_data_member_location;
	assert(type);
	if (type->type != DW_TAG_member) {
		msg_stderr("expected type DW_TAG_member\n");
		return -1;
	}
	at_data_member_location = metac_type_at_by_key(type, DW_AT_data_member_location);
	if (at_data_member_location == NULL)
		return -1;

	if (p_offset)
		*p_offset = at_data_member_location->data_member_location;
	return 0;
}

int 		metac_type_member_bit_attr(struct metac_type *type, unsigned int * p_bit_offset, unsigned int * p_bit_size) {
	struct metac_type_at * at_bit_offset,
		* at_bit_size;
	assert(type);
	if (type->type != DW_TAG_member) {
		msg_stderr("expected type DW_TAG_member\n");
		return -1;
	}
	at_bit_offset = metac_type_at_by_key(type, DW_AT_bit_offset);
	at_bit_size = metac_type_at_by_key(type, DW_AT_bit_size);
	if (at_bit_offset == NULL || at_bit_size == NULL)
		return -1;

	if (p_bit_offset)
		*p_bit_offset = at_bit_offset->bit_offset;
	if (p_bit_size)
		*p_bit_size = at_bit_size->bit_size;
	return 0;
}


unsigned int metac_type_structure_member_count(struct metac_type *type) {
#ifndef	NDEBUG
	unsigned int i;
#endif

	assert(type);
	type = metac_type_typedef_skip(type);
	assert(type);

	if (type->type != DW_TAG_structure_type) {
		msg_stderr("expected type DW_TAG_structure_type\n");
		return 0;
	}
#ifndef	NDEBUG
	for (i = 0; i < metac_type_child_num(type); i++) {
		struct metac_type *child = metac_type_child(type, i);
		assert(child);
		assert(child->type == DW_TAG_member);
	}
#endif
	return metac_type_child_num(type);
}

struct metac_type * metac_type_structure_member(struct metac_type *type, unsigned int id) {
#ifndef	NDEBUG
	unsigned int i;
#endif

	assert(type);
	type = metac_type_typedef_skip(type);
	assert(type);

	if (type->type != DW_TAG_structure_type) {
		msg_stderr("expected type DW_TAG_structure_type\n");
		return NULL;
	}
#ifndef	NDEBUG
	for (i = 0; i < metac_type_child_num(type); i++) {
		struct metac_type *child = metac_type_child(type, i);
		assert(child);
		assert(child->type == DW_TAG_member);
	}
#endif

	return metac_type_child(type, id);
}

struct metac_type *		metac_type_structure_member_type(struct metac_type *type, unsigned int id) {
	struct metac_type * member = metac_type_structure_member(type, id);
	if (member == NULL)
		return NULL;
	return metac_type_member_type(member);
}

char *					metac_type_structure_member_name(struct metac_type *type, unsigned int id) {
	struct metac_type * member = metac_type_structure_member(type, id);
	if (member == NULL)
		return NULL;
	return metac_type_member_name(member);
}

int						metac_type_structure_member_offset(struct metac_type *type, unsigned int id, unsigned int * p_offset) {
	struct metac_type * member = metac_type_structure_member(type, id);
	if (member == NULL)
		return -1;
	return metac_type_member_offset(member, p_offset);
}

int						bit_attr(struct metac_type *type, unsigned int id, unsigned int * p_bit_offset, unsigned int * p_bit_size) {
	struct metac_type * member = metac_type_structure_member(type, id);
	if (member == NULL)
		return -1;
	return metac_type_member_bit_attr(member, p_bit_offset, p_bit_size);
}


unsigned int metac_type_array_length(struct metac_type *type) {
	unsigned int i;
	unsigned int length = 0;
	assert(type);
	type = metac_type_typedef_skip(type);
	assert(type);
	if (type->type != DW_TAG_array_type) {
		msg_stderr("expected type DW_TAG_array_type\n");
		return 0;
	}

	for (i = 0; i < metac_type_child_num(type); i++) {
		struct metac_type_at
			* at_lower_bound,
			* at_upper_bound;
		struct metac_type *child = metac_type_child(type, i);
		assert(child->type == DW_TAG_subrange_type);

		/* get range parameter */
		at_lower_bound = metac_type_at_by_key(child, DW_AT_lower_bound); /*optional*/
		at_upper_bound = metac_type_at_by_key(child, DW_AT_upper_bound); /*mandatory?*/
		if (at_upper_bound == NULL) {
			msg_stderr("upper_bound is mandatory\n");
			return 0;
		}
		length += at_upper_bound->upper_bound + 1;
		if (at_lower_bound)
			length -= at_lower_bound->lower_bound;

	}
	return length;
}

struct metac_type *	metac_type_subprogram_return_type(struct metac_type *type) {
	struct metac_type_at * res;

	if (type->type != DW_TAG_subprogram) {
		msg_stderr("_DW_TAG_subprogram_ expected type == DW_TAG_subprogram\n");
		return NULL;
	}

	res = metac_type_at_by_key(type, DW_AT_type);
	assert(res);

	return res->type;
}

int 					metac_type_subprogram_parameter_count(struct metac_type *type) {
	int i;
	int count = 0;

	if (type->type != DW_TAG_subprogram) {
		msg_stderr("_DW_TAG_subprogram_ expected type == DW_TAG_subprogram\n");
		return -1;
	}

	for (i = 0; i < metac_type_child_num(type); i++) {
		struct metac_type *child = metac_type_child(type, i);
		if (child->type == DW_TAG_formal_parameter)
			count++;
	}
	return count;
}

struct metac_type *	metac_type_subprogram_parameter(struct metac_type *type, unsigned int id) {
	int i, j = 0;
	struct metac_type * res = NULL;

	if (type->type != DW_TAG_subprogram) {
		msg_stderr("_DW_TAG_subprogram_ expected type == DW_TAG_subprogram\n");
		return NULL;
	}

	for (i = 0; i < metac_type_child_num(type); i++) {
		struct metac_type *child = metac_type_child(type, i);
		if (child->type == DW_TAG_formal_parameter) {
			if (j == id) {
				res = child;
				break;
			}
			/* TODO: in fact we can do it without loop if this confistion is always true*/
			assert(child->type == DW_TAG_formal_parameter);

			j++;
		}
	}

	/*TODO: common code with the next function? - to a separete function */
	if (res) { /* formal parameter found */
		struct metac_type_at * at_type;
		at_type = metac_type_at_by_key(res, DW_AT_type);

		res = NULL;
		if (at_type)
			res = at_type->type;

	}

	return res;
}

struct metac_type *	metac_type_subprogram_parameter_by_name(struct metac_type *type, const char *parameter_name) {
	int i, j = 0;
	struct metac_type * res = NULL;

	if (type->type != DW_TAG_subprogram) {
		msg_stderr("_DW_TAG_subprogram_ expected type == DW_TAG_subprogram\n");
		return NULL;
	}

	for (i = 0; i < metac_type_child_num(type); i++) {
		struct metac_type *child = metac_type_child(type, i);
		if (child->type == DW_TAG_formal_parameter) {
			struct metac_type_at * at;

			at = metac_type_at_by_key(child, DW_AT_name);
			assert(at);
			if (at && strcmp(parameter_name, at->name) == 0) {
				res = child;
				break;
			}
			j++;
		}
	}

	/*TODO: common code with the previous function? - to a separate function */
	if (res) { /* formal parameter found */
		struct metac_type_at * at;
		at = metac_type_at_by_key(res, DW_AT_type);

		res = NULL;
		if (at)
			res = at->type;

	}

	return res;
}

/*TODO: put the code below to the separate file metac_object.c*/
struct metac_object {
	    struct metac_type *_type;		/*< type that was used to call object_create function of this object (instance)*/
	    struct metac_type *type;	/*< metac_type_typedef_skip(type) - it contains all necessary info to really create object */

	    unsigned int count;				/* 1 in normal state, array len >0 if we need array*/

	    void *data;					/*pointer to memory*/
	    unsigned int data_length; 	/*keep length for bounds checking*/
	    unsigned char parent_data_memory; /*flag that p_mem wasn't allocated by this object*/

	    struct metac_object *p_agg; /*when type doesn't use children you use DW_AT_type*/
	    struct metac_object **child;/*when we use children*/
};

struct metac_type * 		metac_object_type(struct metac_object *object) {
	return object->_type;
}

unsigned int 				metac_object_count(struct metac_object *object) {
	return object->count;
}

void *						metac_object_ptr(struct metac_object *object, unsigned int *p_data_length) {
	if (p_data_length != NULL)
		*p_data_length = object->data_length;
	return object->data;
}

void 						metac_object_destroy(struct metac_object *object) {
	unsigned int i;

	assert(object);
	assert(object->type);
	assert(object->count > 0);

	if (object->p_agg)
		free(object->p_agg);
	object->p_agg = NULL;

	if (object->child) {
		for (i = 0; i < object->count * object->type->child_num; i++) {
			if (object->child[i])
				metac_object_destroy(object->child[i]);
			object->child[i] = NULL;
		}
		free(object->child);
	}
	object->child = NULL;

	if (!object->parent_data_memory && object->data)
		free(object->data);
	object->data = NULL;
	object->data_length = 0;

	free(object);
}
static struct metac_object * 	_metac_object_create(struct metac_type *_type,
		unsigned int count,
		void *data,
		unsigned int max_data_lenth,
		unsigned int * p_data_lenth) {
	struct metac_type *type;
	struct metac_type_at * at_byte_size;
	struct metac_object * object;

	static struct metac_type_at ptr_at_byte_size = {.key = DW_AT_byte_size, .byte_size = sizeof(void*)};
	struct metac_type_at 		arr_at_byte_size = {.key = DW_AT_byte_size, .byte_size = 0};

	assert(_type);
	assert(count > 0);

	type = metac_type_typedef_skip(_type);
	assert(type);

	at_byte_size = metac_type_at_by_key(type, DW_AT_byte_size);
	if (at_byte_size == NULL) {
		if (type->type == DW_TAG_pointer_type) {
			/*workaround for clang - it doesn't provide length of ptrs */
			at_byte_size = &ptr_at_byte_size;
		} else if (data == NULL && type->type == DW_TAG_array_type) {
			/*workaroung with arrays - we need to know byte_size */
			arr_at_byte_size.byte_size = metac_type_byte_size(type);
			at_byte_size = &arr_at_byte_size;
		}
	}
	assert(data || at_byte_size);

	/* create object itself */
	object = (struct metac_object *)calloc(1,  sizeof(struct metac_object));
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
			metac_object_destroy(object);
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
	case DW_TAG_enumeration_type:
	case DW_TAG_pointer_type:
		assert(at_byte_size);
		object->data_length = at_byte_size->byte_size * object->count;

		if (object->data_length > max_data_lenth) {
			msg_stderr("max_data_lenth is too small: %d > %d\n", object->data_length, max_data_lenth);
			metac_object_destroy(object);
			return NULL;
		}

		if (p_data_lenth)
			*p_data_lenth = object->data_length;
		break;
	case DW_TAG_structure_type:
	case DW_TAG_union_type:
		{
			unsigned int global_offset = 0;
			unsigned int global_id;
			unsigned int id_in_array;

			assert(at_byte_size);

			/*create count structures with child_num members*/
			object->child = (struct metac_object **)calloc(object->count * object->type->child_num, sizeof(struct metac_object *));
			if (object->child == NULL) {
				msg_stderr("can't allocate memory for sub-objects\n");
				metac_object_destroy(object);
				return NULL;
			}

			for (id_in_array = 0; id_in_array < object->count; id_in_array++) {
				unsigned int child;

				for (child = 0; child < object->type->child_num; child++) {
					unsigned int child_data_lenth = 0;
					unsigned int child_max_data_lenth = 0;

					struct metac_type_at default_at_data_member_location = {
							.key = DW_AT_data_member_location,
							.data_member_location = 0,
					};

					struct metac_type_at * at_data_member_location =
							metac_type_at_by_key(type->child[child], DW_AT_data_member_location); /*TODO: invert cycles for better performance*/

					if (object->type->type == DW_TAG_union_type &&
						at_data_member_location == NULL) {
						at_data_member_location = &default_at_data_member_location;
					}

					assert(at_data_member_location);

					/*calc max_data_size*/
					child_max_data_lenth = at_byte_size->byte_size - at_data_member_location->data_member_location;

					global_id = id_in_array * object->type->child_num + child;


					object->child[global_id] = _metac_object_create(type->child[child], 1,
							((char*)object->data) + global_offset + at_data_member_location->data_member_location,
							child_max_data_lenth, &child_data_lenth);

				}
				global_offset += at_byte_size->byte_size;
			}
			assert(global_offset <= max_data_lenth);
			object->data_length = global_offset;
			if (p_data_lenth)
				*p_data_lenth = object->data_length;
		}while(0);
		break;
	case DW_TAG_member:
		{
			struct metac_type_at * at_type =
					metac_type_at_by_key(object->type, DW_AT_type);
			assert(at_type);
			assert(count == 1);

			object->p_agg = _metac_object_create(at_type->type, count, data, max_data_lenth, p_data_lenth);
			if (object->p_agg == NULL) {
				msg_stderr("failed to create p_agg\n");
				metac_object_destroy(object);
				return NULL;
			}
			if (p_data_lenth)
				object->data_length = *p_data_lenth;
		}while(0);
		break;
	case DW_TAG_array_type:
		{
			unsigned int global_offset = 0;
			unsigned int global_id = 0;
			unsigned int id;
			struct metac_type_at * at_type =
					metac_type_at_by_key(type, DW_AT_type); /*type of array elements*/
			assert(at_type);

			object->child = (struct metac_object **)calloc(object->count * object->type->child_num,
					sizeof(struct metac_object *));
			if (object->child == NULL) {
				msg_stderr("can't allocate memory for sub-objects\n");
				metac_object_destroy(object);
				return NULL;
			}

			for (id = 0; id < object->count; id++) {
				unsigned int child;
				for (child = 0; child < object->type->child_num; child++) {
					unsigned int child_data_lenth = 0;
					unsigned int elements_count = 0;
					struct metac_type_at * at_lower_bound,
											* at_upper_bound;

					global_id = id * object->type->child_num + child;
					assert(type->child[child]->type == DW_TAG_subrange_type);

					/* get range parameter */
					at_lower_bound = metac_type_at_by_key(type->child[child], DW_AT_lower_bound); /*optional*/
					at_upper_bound = metac_type_at_by_key(type->child[child], DW_AT_upper_bound); /*mandatory?*/
					if (at_upper_bound == NULL) {
						msg_stderr("upper_bound is mandatory\n");
						metac_object_destroy(object);
						return NULL;
					}
					elements_count = at_upper_bound->upper_bound + 1;
					if (at_lower_bound)
						elements_count -= at_lower_bound->lower_bound;

					/*try to create array of objects*/
					object->child[global_id] = _metac_object_create(at_type->type, elements_count,
							((char*)object->data) + global_offset, max_data_lenth - global_offset, &child_data_lenth);
					if (object->child[global_id] == NULL) {
						msg_stderr("failed to create subrange_type\n");
						metac_object_destroy(object);
						return NULL;
					}
					assert(max_data_lenth >= child_data_lenth);
					global_offset += child_data_lenth;
					max_data_lenth -= child_data_lenth;
				}
			}
			object->data_length = global_offset;
			if (p_data_lenth)
				*p_data_lenth = object->data_length;
		}while(0);
		break;
	default:
		msg_stderr("type %x is unsupported\n", object->type->type);
		assert(0/*TBD: not implemented yet*/);
	}

	return object;
}
struct metac_object * 	metac_object_create(struct metac_type *type) {
	return metac_object_array_create(type, 1);
}
struct metac_object * 	metac_object_array_create(struct metac_type *type, unsigned int count) {
	return _metac_object_create(type, count, NULL, 0, NULL);
}
struct metac_object *	metac_object_structure_member_by_name(struct metac_object *object, const char *member_name) {
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
		struct metac_type_at * at_name =
				metac_type_at_by_key(object->child[global_id]->type, DW_AT_name);

		assert(at_name);
		if (strcmp(at_name->name, member_name) == 0) {
			return object->child[global_id];
		}
	}

	return NULL;
}

