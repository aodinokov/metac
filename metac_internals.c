/*
 * metac_internals.c
 *
 *  Created on: Jan 23, 2019
 *      Author: mralex
 */

//#define METAC_DEBUG_ENABLE

#include <stdlib.h>			/* calloc, free, NULL... */
#include <string.h>			/* strdup */
#include <errno.h>			/* ENOMEM etc */
#include <assert.h>			/* assert */

#include "metac_internals.h"
#include "metac_debug.h"	/* msg_stderr, ...*/

/*****************************************************************************/
static void dump_discriminator(FILE * file, struct discriminator * p_discriminator) {
	fprintf(file, "\t\tdiscriminator %d {\n", p_discriminator->id);
	if (p_discriminator->precondition.p_discriminator) {
		fprintf(file, 	"\t\t\tprecondition discriminator: %d, \n"
						"\t\t\texpected value: %d\n",
				p_discriminator->precondition.p_discriminator->id,
				p_discriminator->precondition.expected_discriminator_value);
	}
	fprintf(file, 	"\t\t\tfn: %p,\n"
					"\t\t\tcontext %p\n"
					"\t\t}\n",
			p_discriminator->discriminator_cb,
			p_discriminator->discriminator_cb_context);
}
static void dump_region_element_type_element(FILE * file, struct region_element_type_element * p_region_element_type_element) {

	fprintf(file, "\t\tregion_element_type_element {\n");
	fprintf(file, "\t\t\tid %d\n", (int)p_region_element_type_element->id);
	fprintf(file, "\t\t\tparent %p\n", p_region_element_type_element->parent);
	if (p_region_element_type_element->precondition.p_discriminator) {
		fprintf(file, "\t\t\tprecondition discriminator: %d, \n"
				"\t\t\texpected value: %d\n",
				p_region_element_type_element->precondition.p_discriminator->id,
				p_region_element_type_element->precondition.expected_discriminator_value);
	}
	if (p_region_element_type_element->type->name) {
		fprintf(file, "\t\t\ttype.name %s\n", p_region_element_type_element->type->name);
	}
	{
		fprintf(file, "\t\t\tname_local %s\n", p_region_element_type_element->name_local);
		fprintf(file, "\t\t\tpath_within_region_element %s\n", p_region_element_type_element->path_within_region_element);
		fprintf(file, "\t\t\tpath_global %s\n", p_region_element_type_element->path_global);
		fprintf(file, "\t\t\toffset %d\n", (int)p_region_element_type_element->offset);
		fprintf(file, "\t\t\tbyte_size %d\n", (int)p_region_element_type_element->byte_size);
	}
	if (p_region_element_type_element->array_elements_count_funtion_ptr != NULL) {
		fprintf(file, "\t\t\tarray_elements_count_funtion_ptr %p\n", p_region_element_type_element->array_elements_count_funtion_ptr);
	}
	if (p_region_element_type_element->array_elements_count_cb_context != NULL) {
		fprintf(file, "\t\t\tarray_elements_count_cb_context %p\n", p_region_element_type_element->array_elements_count_cb_context);
	}
	if (p_region_element_type_element->array_elements_region_element_type != NULL) {
		fprintf(file, "\t\t\tarray_elements_region_element_type %p\n", p_region_element_type_element->array_elements_region_element_type);
	}

	fprintf(file, "\t\t}\n");
}
static void dump_region_element_type(FILE * file, struct region_element_type * p_region_element_type) {
	int i;
	fprintf(file, "region_element_type %p {\n", p_region_element_type);
	if (p_region_element_type->discriminators_count > 0) {
		fprintf(file, "\tdiscriminators: [\n");
		for (i = 0 ; i < p_region_element_type->discriminators_count; i++) {
			dump_discriminator(file, p_region_element_type->discriminator[i]);
		}
		fprintf(file, "\t]\n");
	}
	fprintf(file, "\telements_count: %d\n", p_region_element_type->elements_count);
	fprintf(file, "\tpointer_type_elements_count: %d\n", p_region_element_type->pointer_type_elements_count);
	fprintf(file, "\tarray_type_elements_count: %d\n", p_region_element_type->array_type_elements_count);
	if (p_region_element_type->elements_count > 0) {
		fprintf(file, "\telements: [\n");
		for (i = 0 ; i < p_region_element_type->elements_count; i++) {
			dump_region_element_type_element(file, p_region_element_type->element[i]);
		}
		fprintf(file, "\t]\n");
	}
	fprintf(file, "}\n");
}

static void dump_precompiled_type(FILE * file, metac_precompiled_type_t * p_precompiled_type) {
	int i;
	for (i = 0; i<p_precompiled_type->region_element_types_count; i++) {
		dump_region_element_type(file, p_precompiled_type->region_element_type[i]);
	}
}
void metac_dump_precompiled_type(metac_precompiled_type_t * precompiled_type) {
	dump_precompiled_type(stdout, precompiled_type);
}
/*****************************************************************************/
int delete_discriminator(struct discriminator ** pp_discriminator) {
	if (pp_discriminator == NULL) {
		msg_stderr("Can't delete discriminator: invalid parameter\n");
		return -EINVAL;
	}

	if (*pp_discriminator == NULL) {
		msg_stderr("Can't delete discriminator: already deleted\n");
		return -EALREADY;
	}

	free(*pp_discriminator);
	*pp_discriminator = NULL;

	return 0;
}

struct discriminator * create_discriminator(
		struct discriminator * p_previous_discriminator,
		metac_discriminator_value_t previous_expected_discriminator_value,
		metac_discriminator_cb_ptr_t discriminator_cb,
		void * discriminator_cb_context) {
	struct discriminator * p_discriminator;
	p_discriminator = calloc(1, sizeof(*(p_discriminator)));
	if (p_discriminator == NULL) {
		msg_stderr("Can't create discriminator: no memory\n");
		return NULL;
	}

	if (p_previous_discriminator != NULL) {	/*copy precondition*/
		p_discriminator->precondition.p_discriminator = p_previous_discriminator;
		p_discriminator->precondition.expected_discriminator_value = previous_expected_discriminator_value;
	}

	p_discriminator->discriminator_cb = discriminator_cb;
	p_discriminator->discriminator_cb_context = discriminator_cb_context;

	return p_discriminator;
}
/*****************************************************************************/
int delete_region_element_type_element(struct region_element_type_element **pp_region_element_type_element) {
	struct region_element_type_element *p_region_element_type_element;

	if (pp_region_element_type_element == NULL) {
		msg_stderr("Can't delete region_element_type_element: invalid parameter\n");
		return -EINVAL;
	}

	p_region_element_type_element = *pp_region_element_type_element;
	if (p_region_element_type_element == NULL) {
		msg_stderr("Can't delete region_element_type_element: already deleted\n");
		return -EALREADY;
	}

	if (p_region_element_type_element->name_local != NULL) {
		free(p_region_element_type_element->name_local);
		p_region_element_type_element->name_local = NULL;
	}
	if (p_region_element_type_element->path_within_region_element != NULL) {
		free(p_region_element_type_element->path_within_region_element);
		p_region_element_type_element->path_within_region_element = NULL;
	}
	if (p_region_element_type_element->path_global != NULL) {
		free(p_region_element_type_element->path_global);
		p_region_element_type_element->path_global = NULL;
	}
	free(p_region_element_type_element);
	*pp_region_element_type_element = NULL;

	return 0;
}

void update_region_element_type_element_array_params(
		struct region_element_type_element *p_region_element_type_element,
		metac_array_elements_count_cb_ptr_t array_elements_count_funtion_ptr,
		void *	array_elements_count_cb_context,
		struct region_element_type * array_elements_region_element_type) {
	p_region_element_type_element->array_elements_count_funtion_ptr = array_elements_count_funtion_ptr;
	p_region_element_type_element->array_elements_count_cb_context = array_elements_count_cb_context;
	p_region_element_type_element->array_elements_region_element_type = array_elements_region_element_type;
}

struct region_element_type_element * create_region_element_type_element(
		struct metac_type * type,
		struct discriminator * p_discriminator,
		metac_discriminator_value_t expected_discriminator_value,
		metac_data_member_location_t offset,
		metac_byte_size_t byte_size,
		struct region_element_type_element * parent,
		char *	name_local,
		char *	path_within_region,
		char *	path_global,
		metac_array_elements_count_cb_ptr_t array_elements_count_funtion_ptr,
		void *	array_elements_count_cb_context,
		struct region_element_type * array_elements_region_element_type) {

	struct region_element_type_element *p_region_element_type_element;

	if (type == NULL) {
		msg_stderr("invalid argument\n");
		return NULL;
	}

	assert(type->id != DW_TAG_typedef && type->id != DW_TAG_const_type);

	msg_stddbg("(name_local = %s, path_within_region = %s, path_global = %s, offset = %d, byte_size = %d)\n",
			name_local, path_within_region, path_global,
			(int)offset, (int)byte_size);

	p_region_element_type_element = calloc(1, sizeof(*(p_region_element_type_element)));
	if (p_region_element_type_element == NULL) {
		msg_stderr("Can't create region_element_type_element: no memory\n");
		return NULL;
	}

	p_region_element_type_element->type = type;

	if (p_discriminator != NULL) {	/*copy precondition*/
		p_region_element_type_element->precondition.p_discriminator = p_discriminator;
		p_region_element_type_element->precondition.expected_discriminator_value = expected_discriminator_value;
	}

	p_region_element_type_element->offset = offset;
	p_region_element_type_element->byte_size = byte_size;

	p_region_element_type_element->parent = parent;

	p_region_element_type_element->name_local = (name_local != NULL)?strdup(name_local):NULL;
	p_region_element_type_element->path_within_region_element = (path_within_region != NULL)?strdup(path_within_region):NULL;
	p_region_element_type_element->path_global = (path_global != NULL)?strdup(path_global):NULL;;
	if (	(name_local != NULL && p_region_element_type_element->name_local == NULL) ||
			(path_within_region != NULL && p_region_element_type_element->path_within_region_element == NULL) ||
			(path_global != NULL && p_region_element_type_element->path_global == NULL) ) {
		delete_region_element_type_element(&p_region_element_type_element);
		return NULL;
	}

	update_region_element_type_element_array_params(p_region_element_type_element,
			array_elements_count_funtion_ptr, array_elements_count_cb_context, array_elements_region_element_type);

	return p_region_element_type_element;
}
/*****************************************************************************/
int delete_region_element_type(struct region_element_type ** pp_region_element_type) {
	int i;
	struct region_element_type *p_region_element_type;

	if (pp_region_element_type == NULL) {
		msg_stderr("Can't delete region_element_type: invalid parameter\n");
		return -EINVAL;
	}

	p_region_element_type = *pp_region_element_type;
	if (p_region_element_type == NULL) {
		msg_stderr("Can't delete region_element_type: already deleted\n");
		return -EALREADY;
	}

	for (i = 0; i < p_region_element_type->elements_count; i++) {
		delete_region_element_type_element(&p_region_element_type->element[i]);
	}
	free(p_region_element_type->element);
	p_region_element_type->element = NULL;
	free(p_region_element_type->base_type_element);
	p_region_element_type->base_type_element = NULL;
	free(p_region_element_type->enum_type_element);
	p_region_element_type->enum_type_element = NULL;
	free(p_region_element_type->pointer_type_element);
	p_region_element_type->pointer_type_element = NULL;
	free(p_region_element_type->array_type_element);
	p_region_element_type->array_type_element = NULL;
	free(p_region_element_type->hierarchy_element);
	p_region_element_type->hierarchy_element = NULL;

	for (i = 0; i < p_region_element_type->discriminators_count; i++) {
		delete_discriminator(&p_region_element_type->discriminator[i]);
	}
	free(p_region_element_type->discriminator);
	p_region_element_type->discriminator = NULL;


	free(p_region_element_type);
	*pp_region_element_type = NULL;

	return 0;
}

struct region_element_type * create_region_element_type(
		struct metac_type * type) {

	struct region_element_type *p_region_element_type;

	if (type == NULL) {
		msg_stderr("invalid argument\n");
		return NULL;
	}

	msg_stddbg("create_region_element_type: %s\n", type->name);

	p_region_element_type = calloc(1, sizeof(*(p_region_element_type)));
	if (p_region_element_type == NULL) {
		msg_stderr("Can't create region_element_type: no memory\n");
		return NULL;
	}

	p_region_element_type->type = type;

	p_region_element_type->discriminators_count = 0;
	p_region_element_type->discriminator = NULL;

	p_region_element_type->elements_count = 0;
	p_region_element_type->element = NULL;

	p_region_element_type->hierarchy_element = NULL;
	p_region_element_type->hierarchy_elements_count = 0;
	p_region_element_type->base_type_element = NULL;
	p_region_element_type->base_type_elements_count = 0;
	p_region_element_type->enum_type_element = NULL;
	p_region_element_type->enum_type_elements_count = 0;
	p_region_element_type->pointer_type_element = NULL;
	p_region_element_type->pointer_type_elements_count = 0;
	p_region_element_type->array_type_element = NULL;
	p_region_element_type->array_type_elements_count = 0;

	return p_region_element_type;
}
/*****************************************************************************/
int delete_precompiled_type(struct metac_precompiled_type ** pp_precompiled_type) {
	int i;
	struct metac_precompiled_type *p_precompiled_type;

	if (pp_precompiled_type == NULL) {
		msg_stderr("Can't delete precompiled_type: invalid parameter\n");
		return -EINVAL;
	}

	p_precompiled_type = *pp_precompiled_type;
	if (p_precompiled_type == NULL) {
		msg_stderr("Can't delete precompiled_type: already deleted\n");
		return -EALREADY;
	}

	for (i = 0; i < p_precompiled_type->region_element_types_count; i++) {
		delete_region_element_type(&p_precompiled_type->region_element_type[i]);
	}

	if (p_precompiled_type->region_element_type != NULL) {
		free(p_precompiled_type->region_element_type);
		p_precompiled_type->region_element_type = NULL;
	}

	free(p_precompiled_type);
	*pp_precompiled_type = NULL;

	return 0;
}

struct metac_precompiled_type * create_precompiled_type(
		struct metac_type * type) {

	struct metac_precompiled_type *p_precompiled_type;

	if (type == NULL) {
		msg_stderr("invalid argument\n");
		return NULL;
	}

	p_precompiled_type = calloc(1, sizeof(*(p_precompiled_type)));
	if (p_precompiled_type == NULL) {
		msg_stderr("Can't create precompiled_type: no memory\n");
		return NULL;
	}

	p_precompiled_type->type = type;

	p_precompiled_type->region_element_types_count = 0;
	p_precompiled_type->region_element_type = NULL;

	return p_precompiled_type;
}
void metac_free_precompiled_type(metac_precompiled_type_t ** pp_precompiled_type) {
	delete_precompiled_type(pp_precompiled_type);
}

/* separated this part from metac_precompiled_type */
/*****************************************************************************/
int is_region_element_precondition_true(
		struct region_element * p_region_element,
		struct condition * p_precondition) {
	int id;

	assert(p_region_element);
	assert(p_precondition);
	assert(p_region_element->region_element_type);

	if (p_precondition->p_discriminator == NULL)
		return 1;

	id = p_precondition->p_discriminator->id;
	assert(id < p_region_element->region_element_type->discriminators_count);

	if (p_region_element->p_discriminator_value[id].is_initialized == 0) {
		if (is_region_element_precondition_true(p_region_element, &p_precondition->p_discriminator->precondition) == 0)
			return 0;
		assert(p_precondition->p_discriminator->discriminator_cb);

		if (p_precondition->p_discriminator->discriminator_cb(
				0,
				p_region_element->ptr,
				p_region_element->region_element_type->type,
				&p_region_element->p_discriminator_value[id].value,
				p_precondition->p_discriminator->discriminator_cb_context) != 0) {
			msg_stderr("Error calling discriminatior_cb %d for type %s\n", id, p_region_element->region_element_type->type->name);
			return -EFAULT;
		}
		msg_stddbg("discriminator returned %d\n", (int)p_region_element->p_discriminator_value[id].value);
		p_region_element->p_discriminator_value[id].is_initialized = 1;
	}
	return p_region_element->p_discriminator_value[id].value == p_precondition->expected_discriminator_value;
}
/*****************************************************************************/
int set_region_element_precondition(
		struct region_element * p_region_element,
		struct condition * p_precondition) {
	int id;

	assert(p_region_element);
	assert(p_precondition);
	assert(p_region_element->region_element_type);

	if (p_precondition->p_discriminator == NULL)
		return 0;	/*nothing to do*/

	id = p_precondition->p_discriminator->id;
	assert(id < p_region_element->region_element_type->discriminators_count);

	if (p_region_element->p_discriminator_value[id].is_initialized == 0) {
		if (set_region_element_precondition(p_region_element, &p_precondition->p_discriminator->precondition) != 0)
			return -EEXIST;
		p_region_element->p_discriminator_value[id].value = p_precondition->expected_discriminator_value;
		p_region_element->p_discriminator_value[id].is_initialized = 1;
	}
	if (p_region_element->p_discriminator_value[id].value != p_precondition->expected_discriminator_value)
		return -EEXIST;
	return 0;
}
/*****************************************************************************/
int write_region_element_discriminators(
		struct region_element * p_region_element) {
	int res = 0;
	int id;
	assert(p_region_element);
	assert(p_region_element->region_element_type);

	for (id = 0; id < p_region_element->region_element_type->discriminators_count; ++id) {
		if (p_region_element->p_discriminator_value[id].is_initialized != 0 &&
			p_region_element->region_element_type->discriminator[id]->discriminator_cb != NULL) {
			if (p_region_element->region_element_type->discriminator[id]->discriminator_cb(
					1,
					p_region_element->ptr,
					p_region_element->region_element_type->type,
					&p_region_element->p_discriminator_value[id].value,
					p_region_element->region_element_type->discriminator[id]->discriminator_cb_context)!=0) {
				msg_stderr("Cb failed for discriminator: %d\n", id);
				res = -EFAULT;
			}
		}
	}
	return res;
}
/*****************************************************************************/
int cleanup_region_element(struct region_element *p_region_element) {
	int i;

	if (p_region_element == NULL) {
		msg_stderr("invalid parameter\n");
		return -EINVAL;
	}

	if (p_region_element->p_array != NULL) {
		free(p_region_element->p_array);
		p_region_element->p_array = NULL;
	}

	if (p_region_element->p_pointer != NULL) {
		free(p_region_element->p_pointer);
		p_region_element->p_pointer = NULL;
	}

	if (p_region_element->p_discriminator_value != NULL) {
		free(p_region_element->p_discriminator_value);
		p_region_element->p_discriminator_value = NULL;
	}

	return 0;
}
int init_region_element(
		struct region_element *p_region_element,
		void *ptr,
		metac_byte_size_t byte_size,
		struct region_element_type * region_element_type) {
	if (p_region_element == NULL || region_element_type == NULL) {
		msg_stderr("invalid argument\n");
		return -EINVAL;
	}

	p_region_element->region_element_type = region_element_type;
	p_region_element->ptr = ptr;
	p_region_element->byte_size = byte_size;

	if (region_element_type->discriminators_count > 0 &&
		p_region_element->p_discriminator_value == NULL) {
		p_region_element->p_discriminator_value =
				calloc(region_element_type->discriminators_count, sizeof(*(p_region_element->p_discriminator_value)));
		if (p_region_element->p_discriminator_value == NULL) {
			msg_stderr("Can't create region_element's discriminator_value array: no memory\n");
			cleanup_region_element(p_region_element);
			return -ENOMEM;
		}
	}

	if (region_element_type->pointer_type_elements_count > 0 &&
		p_region_element->p_pointer == NULL) {
		p_region_element->p_pointer =
				calloc(region_element_type->pointer_type_elements_count, sizeof(*(p_region_element->p_pointer)));
		if (p_region_element->p_pointer == NULL) {
			msg_stderr("Can't create region_element's pointers array: no memory\n");
			cleanup_region_element(p_region_element);
			return -ENOMEM;
		}
	}

	if (region_element_type->array_type_elements_count > 0 &&
		p_region_element->p_array == NULL) {
		p_region_element->p_array =
				calloc(region_element_type->array_type_elements_count, sizeof(*(p_region_element->p_array)));
		if (p_region_element->p_array == NULL) {
			msg_stderr("Can't create region's arrays array: no memory\n");
			cleanup_region_element(p_region_element);
			return -ENOMEM;
		}
	}

	return 0;
}
/*****************************************************************************/
int delete_region(struct region **pp_region) {
	struct region *p_region;
	int i;

	if (pp_region == NULL) {
		msg_stderr("Can't delete region: invalid parameter\n");
		return -EINVAL;
	}

	p_region = *pp_region;
	if (p_region == NULL) {
		msg_stderr("Can't delete region: already deleted\n");
		return -EALREADY;
	}

	if (p_region->elements != NULL) {
		msg_stddbg("deleting elements\n");
		for (i = 0; i < p_region->elements_count; i++){
			cleanup_region_element(&p_region->elements[i]);
		}
		free(p_region->elements);
		p_region->elements = NULL;
	}

	if (p_region->p_array_info != NULL) {
		metac_array_info_delete(&p_region->p_array_info);
	}

	msg_stddbg("deleting the object itself\n");
	free(p_region);
	*pp_region = NULL;

	return 0;
}
struct region * create_region(
		void *ptr,
		metac_byte_size_t byte_size,
		struct region_element_type * region_element_type,
		metac_array_info_t * p_array_info,
		struct region * part_of_region) {
	struct region *p_region;
	metac_byte_size_t region_element_byte_size;

	if (region_element_type == NULL || p_array_info == NULL) {
		msg_stderr("invalid argument\n");
		return NULL;
	}

	assert(region_element_type->type);
	region_element_byte_size = metac_type_byte_size(region_element_type->type);
	msg_stddbg("p_region(_element)_type = %p (%s,%d), ptr = %p, byte_size = %d\n",
			region_element_type,
			region_element_type->type->name,
			(int)region_element_byte_size,
			ptr,
			(int)byte_size);

	p_region = calloc(1, sizeof(*(p_region)));
	if (p_region == NULL) {
		msg_stderr("Can't create region: no memory\n");
		return NULL;
	}

	p_region->ptr = ptr;
	p_region->byte_size = byte_size;
	p_region->p_array_info = p_array_info;
	p_region->elements_count = metac_array_info_get_element_count(p_array_info);
	p_region->part_of_region = part_of_region;
	p_region->unique_region_id = -1;

	if (p_region->elements_count > 0) {
		int i;

		p_region->elements =
				calloc(p_region->elements_count, sizeof(*(p_region->elements)));
		if (p_region->elements == NULL) {
			msg_stderr("Can't create region's elements array: no memory\n");
			delete_region(&p_region);
			return NULL;
		}

		for (i = 0; i < p_region->elements_count; i++) {
			if (init_region_element(
					&p_region->elements[i],
					ptr + i*region_element_byte_size,
					region_element_byte_size,
					region_element_type)!=0) {
				msg_stderr("init_region_element for element %d\n", i);
				delete_region(&p_region);
				return NULL;
			}
		}
	}

	return p_region;
}

int update_region_ptr(
		struct region *p_region,
		void *ptr) {
	int i;
	metac_byte_size_t region_element_byte_size;
	struct region_element_type * region_element_type;

	if (p_region == NULL) {
		msg_stderr("invalid argument\n");
		return -EINVAL;
	}

	p_region->ptr = ptr;

	region_element_byte_size = p_region->elements[0].byte_size;
	region_element_type = p_region->elements[0].region_element_type;

	for (i = 0; i < p_region->elements_count; i++) {
		if (init_region_element(
				&p_region->elements[i],
				ptr + i*region_element_byte_size,
				region_element_byte_size,
				region_element_type)!=0) {
			msg_stderr("init_region_element for element %d\n", i);
			return -EFAULT;
		}
	}

	return 0;
}

/*****************************************************************************/
int free_runtime_object(struct metac_runtime_object ** pp_runtime_object) {
	int i;
	struct metac_runtime_object *p_runtime_object;

	if (pp_runtime_object == NULL) {
		msg_stderr("Can't delete runtime_object: invalid parameter\n");
		return -EINVAL;
	}

	p_runtime_object = *pp_runtime_object;
	if (p_runtime_object == NULL) {
		msg_stderr("Can't delete runtime_object: already deleted\n");
		return -EALREADY;
	}

	if (p_runtime_object->unique_region != NULL) {
		free(p_runtime_object->unique_region);
		p_runtime_object->unique_region = NULL;
	}

	if (p_runtime_object->region != NULL){
		for (i = 0; i < p_runtime_object->regions_count; i++) {
			delete_region(&p_runtime_object->region[i]);
		}
		free(p_runtime_object->region);
		p_runtime_object->region = NULL;
	}

	free(p_runtime_object);
	*pp_runtime_object = NULL;

	return 0;
}

struct metac_runtime_object * create_runtime_object(struct metac_precompiled_type * p_precompiled_type) {

	struct metac_runtime_object * p_runtime_object;

	if (p_precompiled_type == NULL) {
		msg_stderr("invalid argument\n");
		return NULL;
	}

	p_runtime_object = calloc(1, sizeof(*(p_runtime_object)));
	if (p_runtime_object == NULL) {
		msg_stderr("Can't create create_runtime_object: no memory\n");
		return NULL;
	}

	p_runtime_object->precompiled_type = p_precompiled_type;
	p_runtime_object->region = NULL;
	p_runtime_object->regions_count = 0;

	return p_runtime_object;
}
/*****************************************************************************/

