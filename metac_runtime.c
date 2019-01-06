/*
 * metac_runtime.c
 *
 *  Created on: Apr 1, 2018
 *      Author: mralex
 */
#include "metac_type.h"
#include "metac_precompiled_type_int.h"	/*definitions of internal objects*/
#include "metac_debug.h"	/* msg_stderr, ...*/
#include "breadthfirst_engine.h" /*breadthfirst_engine module*/

#include <stdlib.h>			/* calloc, qsort... */
#include <string.h>			/* strlen, strcpy */
#include <assert.h>			/* assert */
#include <errno.h>			/* ENOMEM etc */
#include <urcu/list.h>		/* I like struct cds_list_head :) */

/* separated this part from metac_precompiled_type */
/*****************************************************************************/
struct runtime_context {
	struct metac_runtime_object * runtime_object;
	struct cds_list_head region_list;
};

struct _region {
	struct cds_list_head list;
	struct region * p_region;
};

struct runtime_task {
	struct breadthfirst_engine_task task;
	struct runtime_task* parent_task;
	struct region * p_region;
};
/*****************************************************************************/
static int destroy_region_element(struct region_element *p_region_element) {
	int i;

	if (p_region_element == NULL) {
		msg_stderr("invalid parameter\n");
		return -EINVAL;
	}

	if (p_region_element->p_array != NULL) {
		for (i = 0; i < p_region_element->region_element_type->array_type_elements_count; i++) {
			if (p_region_element->p_array[i].p_elements_count != NULL) {
				free (p_region_element->p_array[i].p_elements_count);
				p_region_element->p_array[i].p_elements_count = NULL;
			}
		}
		free(p_region_element->p_array);
		p_region_element->p_array = NULL;
	}

	if (p_region_element->p_pointer != NULL) {
		for (i = 0; i < p_region_element->region_element_type->pointer_type_elements_count; i++) {
			if (p_region_element->p_pointer[i].p_elements_count != NULL) {
				free (p_region_element->p_pointer[i].p_elements_count);
				p_region_element->p_pointer[i].p_elements_count = NULL;
			}
		}
		free(p_region_element->p_pointer);
		p_region_element->p_pointer = NULL;
	}

	if (p_region_element->p_discriminator_value != NULL) {
		free(p_region_element->p_discriminator_value);
		p_region_element->p_discriminator_value = NULL;
	}

	return 0;
}

static int init_region_element(struct region_element *p_region_element,
		void *ptr,
		metac_byte_size_t byte_size,
		struct region_element_type * region_element_type
){
	if (p_region_element == NULL || region_element_type == NULL) {
		msg_stderr("invalid argument\n");
		return -EINVAL;
	}

	p_region_element->region_element_type = region_element_type;
	p_region_element->ptr = ptr;
	p_region_element->byte_size = byte_size;

	if (region_element_type->discriminators_count > 0) {
		p_region_element->p_discriminator_value =
				calloc(region_element_type->discriminators_count, sizeof(*(p_region_element->p_discriminator_value)));
		if (p_region_element->p_discriminator_value == NULL) {
			msg_stderr("Can't create region_element's discriminator_value array: no memory\n");
			destroy_region_element(p_region_element);
			return -ENOMEM;
		}
	}

	if (region_element_type->pointer_type_elements_count > 0) {
		p_region_element->p_pointer =
				calloc(region_element_type->pointer_type_elements_count, sizeof(*(p_region_element->p_pointer)));
		if (p_region_element->p_pointer == NULL) {
			msg_stderr("Can't create region_element's pointers array: no memory\n");
			destroy_region_element(p_region_element);
			return -ENOMEM;
		}
	}

	if (region_element_type->array_type_elements_count > 0) {
		p_region_element->p_array =
				calloc(region_element_type->array_type_elements_count, sizeof(*(p_region_element->p_array)));
		if (p_region_element->p_array == NULL) {
			msg_stderr("Can't create region's arrays array: no memory\n");
			destroy_region_element(p_region_element);
			return -ENOMEM;
		}
	}

	return 0;
}

/*****************************************************************************/
static int delete_region(struct region **pp_region) {
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
			destroy_region_element(&p_region->elements[i]);
		}
		free(p_region->elements);
		p_region->elements = NULL;
	}

	msg_stddbg("deleting the object itself\n");
	free(p_region);
	*pp_region = NULL;

	return 0;
}

static struct region * create_region(
		void *ptr,
		metac_byte_size_t byte_size,
		struct region_element_type * region_element_type,
		metac_count_t elements_count,
		struct region * part_of_region
){
	struct region *p_region;
	metac_byte_size_t region_element_byte_size;

	if (region_element_type == NULL || elements_count <= 0) {
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
	p_region->elements_count = elements_count;
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

static int region_element_precondition_is_true(struct region_element * p_region_element, struct condition * p_precondition) {
	int id;

	assert(p_region_element);
	assert(p_precondition);
	assert(p_region_element->region_element_type);

	if (p_precondition->p_discriminator == NULL)
		return 1;

	id = p_precondition->p_discriminator->id;
	assert(id < p_region_element->region_element_type->discriminators_count);

	if (p_region_element->p_discriminator_value[id].is_initialized == 0) {
		if (region_element_precondition_is_true(p_region_element, &p_precondition->p_discriminator->precondition) == 0)
			return 0;
		assert(p_precondition->p_discriminator->discriminator_cb);

		if (p_precondition->p_discriminator->discriminator_cb(0,
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
static struct _region * create__region(
		void *ptr,
		metac_byte_size_t byte_size,
		struct region_element_type * region_element_type,
		metac_count_t elements_count,
		struct region * part_of_region
		) {
	struct _region * _region;

	_region = calloc(1, sizeof(*_region));
	if (_region == NULL) {
		msg_stderr("no memory\n");
		return NULL;
	}

	_region->p_region = create_region(ptr, byte_size, region_element_type, elements_count, part_of_region);
	if (_region->p_region == NULL) {
		msg_stderr("create_region failed\n");
		free(_region);
		return NULL;
	}

	return _region;
}

static struct _region * find_or_create_region(
		struct runtime_context * p_runtime_context,
		void *ptr,
		metac_byte_size_t byte_size,
		struct region_element_type * region_element_type,
		metac_count_t elements_count,
		struct region * part_of_region,
		int * p_created_flag) {
	/*check if region_element_type for the same type already exists*/
	struct _region * _region = NULL;
	struct _region * _region_iter;

	if (p_created_flag != NULL) *p_created_flag = 0;

	cds_list_for_each_entry(_region_iter, &p_runtime_context->region_list, list) {
		if (ptr == _region_iter->p_region->ptr) { /* case when ptr is inside will be covered later */
			_region = _region_iter;
			msg_stddbg("found region %p\n", _region);
			break;
		}
	}

	if (_region == NULL) {
		/*create otherwise*/
		msg_stddbg("create region_element_type for : ptr %p byte_size %d\n", ptr, (int)byte_size);
		_region = create__region(ptr, byte_size, region_element_type, elements_count, part_of_region);
		msg_stddbg("create region_element_type result %p\n", _region);
		if (_region == NULL) {
			msg_stddbg("create__region failed\n");
			return NULL;
		}
		cds_list_add_tail(&_region->list, &p_runtime_context->region_list);
		++p_runtime_context->runtime_object->regions_count;

		if (p_created_flag != NULL) *p_created_flag = 1;
	}

	return _region;
}
/*****************************************************************************/
static int delete_runtime_object(struct metac_runtime_object ** pp_runtime_object) {
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

	if (p_runtime_object->pointer_table_items) {
		free(p_runtime_object->pointer_table_items);
		p_runtime_object->pointer_table_items = NULL;
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

static struct metac_runtime_object * create_runtime_object(struct metac_precompiled_type * p_precompiled_type) {

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
static int delete_runtime_task(struct runtime_task ** pp_task) {
	struct runtime_task * p_task;

	if (pp_task == NULL) {
		msg_stderr("Can't delete p_task: invalid parameter\n");
		return -EINVAL;
	}

	p_task = *pp_task;
	if (p_task == NULL) {
		msg_stderr("Can't delete p_task: already deleted\n");
		return -EALREADY;
	}

	free(p_task);
	*pp_task = NULL;

	return 0;
}

static struct runtime_task * create_and_add_runtime_task_4_region(
		struct breadthfirst_engine * p_breadthfirst_engine,
		struct runtime_task * parent_task,
		breadthfirst_engine_task_fn_t fn,
		breadthfirst_engine_task_destructor_t destroy,
		struct region * p_region
		/*element_byte_size, int number_of_elemetns - this is to handle pointers with n elements*/
		) {
	struct runtime_task* p_task;

	if (p_region == NULL){
		msg_stderr("region is mandatory to create runtime_task\n");
		return NULL;
	}

	assert(p_region->elements_count > 0);
	assert(p_region->elements[0].region_element_type->type);
	msg_stddbg("p_region_element_type = %p (%s), ptr = %p, byte_size = %d\n",
			p_region->elements[0].region_element_type,
			p_region->elements[0].region_element_type->type->name,
			p_region->ptr,
			(int)p_region->byte_size);

	/* allocate object */
	p_task = calloc(1, sizeof(*p_task));
	if (p_task == NULL) {
		msg_stderr("no memory\n");
		return NULL;
	}

	p_task->task.fn = fn;
	p_task->task.destroy = destroy;

	p_task->parent_task = parent_task;

	p_task->p_region = p_region;

	if (add_breadthfirst_task(p_breadthfirst_engine, &p_task->task) != 0) {
		msg_stderr("add_breadthfirst_task failed\n");
		free(p_task);
		return NULL;
	}
	return p_task;
}
/*****************************************************************************/
static int _runtime_task_destroy_fn(
	struct breadthfirst_engine * p_breadthfirst_engine,
	struct breadthfirst_engine_task * p_breadthfirst_engine_task) {
	struct runtime_task * p_task = cds_list_entry(p_breadthfirst_engine_task, struct runtime_task, task);
	delete_runtime_task(&p_task);
	return 0;
}
/*****************************************************************************/
static int _runtime_task_fn(
		struct breadthfirst_engine * p_breadthfirst_engine,
		struct breadthfirst_engine_task * p_breadthfirst_engine_task) {
	struct runtime_task * p_task = cds_list_entry(p_breadthfirst_engine_task, struct runtime_task, task);
	struct runtime_context * p_context = (struct runtime_context *)p_breadthfirst_engine->private_data;

	int e;
	int i;
	struct region * p_region;

	p_region = p_task->p_region;
	if (p_region == NULL) {
		msg_stderr("create_region failed - exiting\n");
		return -EFAULT;
	}

	assert(p_region->elements_count > 0);
	assert(p_region->elements[0].region_element_type->type);
	msg_stddbg("started task %p (%s) for %d elements\n",
			p_region->ptr,
			p_region->elements[0].region_element_type->type->name,
			p_region->elements_count);

	for (e = 0; e < p_region->elements_count; e++) {
		struct region_element * p_region_element = &p_region->elements[e];

		if (p_region_element->region_element_type->array_type_elements_count > 0)
			msg_stddbg("element %d pointers: %d items\n", e, p_region_element->region_element_type->pointer_type_elements_count);
		for (i = 0; i < p_region_element->region_element_type->pointer_type_elements_count; i++) {
			int j;
			int res;
			metac_count_t elements_count;
			metac_byte_size_t elements_byte_size;
			struct _region * _region;
			void * new_ptr;
			int new_region = 0;

			msg_stddbg("pointer %s\n", p_region_element->region_element_type->pointer_type_element[i]->path_within_region_element);

			res = region_element_precondition_is_true(p_region_element, &p_region_element->region_element_type->pointer_type_element[i]->precondition);
			if (res < 0) {
				msg_stderr("Something wrong with conditions\n");
				return -EFAULT;
			}else if (res == 0) {
				msg_stddbg("skipping by precondition\n");
				continue;
			}

			p_region_element->p_pointer[i].n = 1; /*pointers are always 1d */

			p_region_element->p_pointer[i].p_elements_count =
					calloc(p_region_element->p_pointer[i].n, sizeof(*(p_region_element->p_pointer[i].p_elements_count)));
			if (p_region_element->p_pointer[i].p_elements_count == NULL) {
				msg_stderr("Pointer p_elements_count allocation failed - exiting\n");
				return -EFAULT;
			}

			if (p_region_element->region_element_type->pointer_type_element[i]->array_elements_count_funtion_ptr == NULL) {
				msg_stddbg("skipping because don't have a cb to determine elements count\n");
				continue; /*we don't handle pointers if we can't get fn*/
			}

			/* now read the pointer */
			new_ptr = *((void**)(p_region_element->ptr + p_region_element->region_element_type->pointer_type_element[i]->offset));
			if (new_ptr == NULL) {
				msg_stddbg("skipping because new ptr is null\n");
				continue;
			}

			if (p_region_element->region_element_type->pointer_type_element[i]->array_elements_count_funtion_ptr(
					0,
					p_region_element->ptr,
					p_region_element->region_element_type->type,
					new_ptr,
					p_region_element->region_element_type->pointer_type_element[i]->array_elements_region_element_type?
							p_region_element->region_element_type->pointer_type_element[i]->array_elements_region_element_type->type:NULL,
					p_region_element->p_pointer[i].n,
					p_region_element->p_pointer[i].p_elements_count,
					p_region_element->region_element_type->pointer_type_element[i]->array_elements_count_cb_context) != 0) {
				msg_stderr("Error calling array_elements_count_funtion_ptr for pointer element %d in type %s\n",
						i, p_region_element->region_element_type->type->name);
				return -EFAULT;
			}

			/* calculate byte_size using length */
			elements_count = p_region_element->p_pointer[i].p_elements_count[0];
			for (j = 1; j < p_region_element->p_pointer[i].n; j++)
				elements_count *= p_region_element->p_pointer[i].p_elements_count[j];
			msg_stddbg("elements_count: %d\n", (int)elements_count);
			elements_byte_size = p_region_element->region_element_type->pointer_type_element[i]->array_elements_region_element_type?
					metac_type_byte_size(p_region_element->region_element_type->pointer_type_element[i]->array_elements_region_element_type->type):0;

			if (elements_byte_size == 0 || elements_count == 0) {
				msg_stddbg("skipping because size is 0\n");
				continue;
			}

			/*we have to create region and store it (need create or find to support loops) */
			_region = find_or_create_region(
					p_context,
					new_ptr,
					elements_byte_size * elements_count,
					p_region_element->region_element_type->pointer_type_element[i]->array_elements_region_element_type,
					elements_count,
					NULL,
					&new_region);
			if (_region == NULL) {
				msg_stderr("Error calling find_or_create_region\n");
				return -EFAULT;
			}
			p_region_element->p_pointer[i].p_region = _region->p_region;
			++p_context->runtime_object->pointer_table_items_count; /* count pointers with initialized region */

			/* add task to handle this region fields properly */
			if (new_region == 1) {
				/*create the new task for this region*/
				if (create_and_add_runtime_task_4_region(p_breadthfirst_engine,
						p_task,
						_runtime_task_fn,
						_runtime_task_destroy_fn, _region->p_region) == NULL) {
					msg_stderr("Error calling create_and_add_runtime_task_4_region\n");
					return -EFAULT;
				}
			}
		}

		if (p_region_element->region_element_type->array_type_elements_count > 0)
			msg_stddbg("element %d arrays: %d items\n", e, p_region_element->region_element_type->array_type_elements_count);
		for (i = 0; i < p_region_element->region_element_type->array_type_elements_count; i++) {
			int j;
			int res;
			void * new_ptr;
			metac_count_t elements_count;
			metac_byte_size_t elements_byte_size;
			struct _region * _region;
			int new_region = 0;

			msg_stddbg("array %s\n", p_region_element->region_element_type->array_type_element[i]->path_within_region_element);

			res = region_element_precondition_is_true(p_region_element, &p_region_element->region_element_type->array_type_element[i]->precondition);
			if (res < 0) {
				msg_stderr("Something wrong with conditions\n");
				return -EFAULT;
			}else if (res == 0) {
				msg_stddbg("skipping by precondition\n");
				continue;
			}

			assert(p_region_element->region_element_type->array_type_element[i]->type->id == DW_TAG_array_type);

			p_region_element->p_array[i].n = p_region_element->region_element_type->array_type_element[i]->type->array_type_info.subranges_count;

			p_region_element->p_array[i].p_elements_count = calloc(p_region_element->p_array[i].n, sizeof(*(p_region_element->p_array[i].p_elements_count)));
			if (p_region_element->p_array[i].p_elements_count == NULL) {
				msg_stderr("Pointer p_elements_count allocation failed - exiting\n");
				return -EFAULT;
			}

			/* set ptr to the first element */
			new_ptr = (void*)(p_region_element->ptr + p_region_element->region_element_type->array_type_element[i]->offset);

			/*use different approaches to calculate lengths*/
			if (!p_region_element->region_element_type->array_type_element[i]->type->array_type_info.is_flexible){
				for (j = 0; j < p_region_element->p_array[i].n; j++){
					metac_type_array_subrange_count(
							p_region_element->region_element_type->array_type_element[i]->type,
							j,
							&p_region_element->p_array[i].p_elements_count[j]);
				}
			}else{
				if (p_region_element->region_element_type->array_type_element[i]->array_elements_count_funtion_ptr == NULL) {
					msg_stddbg("skipping because don't have a cb to determine elements count\n");
					continue; /*we don't handle pointers if we can't get fn*/
				}

				if (p_region_element->region_element_type->array_type_element[i]->array_elements_count_funtion_ptr(
						0,
						p_region_element->ptr,
						p_region_element->region_element_type->type,
						new_ptr,
						p_region_element->region_element_type->array_type_element[i]->array_elements_region_element_type?
								p_region_element->region_element_type->array_type_element[i]->array_elements_region_element_type->type:NULL,
						p_region_element->p_array[i].n,
						p_region_element->p_array[i].p_elements_count,
						p_region_element->region_element_type->array_type_element[i]->array_elements_count_cb_context) != 0) {
					msg_stderr("Error calling array_elements_count_funtion_ptr for pointer element %d in type %s\n",
							i, p_region_element->region_element_type->type->name);
					return -EFAULT;
				}
			}

			/* calculate overall elements_count */
			elements_count = p_region_element->p_array[i].p_elements_count[0];
			for (j = 1; j < p_region_element->p_array[i].n; j++)
				elements_count *= p_region_element->p_array[i].p_elements_count[j];
			msg_stddbg("elements_count: %d\n", (int)elements_count);
			elements_byte_size = p_region_element->region_element_type->pointer_type_element[i]->array_elements_region_element_type?
					metac_type_byte_size(p_region_element->region_element_type->pointer_type_element[i]->array_elements_region_element_type->type):0;

			if (elements_byte_size == 0 || elements_count == 0) {
				msg_stddbg("skipping because size is 0\n");
				continue;
			}

			/*we have to create region and store it (need create or find to support loops) */
			_region = find_or_create_region(
					p_context,
					new_ptr,
					elements_byte_size * elements_count,
					p_region_element->region_element_type->array_type_element[i]->array_elements_region_element_type,
					elements_count,
					p_region, &new_region);
			if (_region == NULL) {
				msg_stderr("Error calling find_or_create_region\n");
				return -EFAULT;
			}
			p_region_element->p_array[i].p_region = _region->p_region;
			/* add task to handle this region fields properly */
			if (new_region == 1) {
				/*create the new task for this region*/
				if (create_and_add_runtime_task_4_region(p_breadthfirst_engine,
						p_task,
						_runtime_task_fn,
						_runtime_task_destroy_fn, _region->p_region) == NULL) {
					msg_stderr("Error calling create_and_add_runtime_task_4_region\n");
					return -EFAULT;
				}
			}
		}
	}

	msg_stddbg("finished task\n");
	return 0;
}
/*****************************************************************************/
static void cleanup_runtime_context(struct runtime_context *p_runtime_context) {
	struct _region * _region, * __region;

	cds_list_for_each_entry_safe(_region, __region, &p_runtime_context->region_list, list) {
		cds_list_del(&_region->list);
		free(_region);
	}
}

/*****************************************************************************/
int _compare_regions(const void *_a, const void *_b) {
	struct region *a = *((struct region **)_a);
	struct region *b = *((struct region **)_b);
	if (a->ptr < b->ptr)
		return -1;
	if (a->ptr == b->ptr) {
		if (a->byte_size > b->byte_size)return -1;
		if (a->byte_size == b->byte_size)return 0;
		return 1;
	}
	return 1;
}

static struct metac_runtime_object * build_runtime_object(
		void * ptr,
		metac_byte_size_t byte_size,
		struct metac_precompiled_type * p_precompiled_type,
		metac_count_t elements_count
		) {
	int i, j, k, l;
	struct breadthfirst_engine* p_breadthfirst_engine;
	struct runtime_context context;
	struct region * region;
	struct _region * _region;

	if (p_precompiled_type->region_element_type[0]->element[0]->byte_size > byte_size) {
		msg_stderr("byte_size parameter is too small for this precompiled type\n");
		return NULL;
	}

	context.runtime_object = create_runtime_object(p_precompiled_type);
	if (context.runtime_object == NULL) {
		msg_stderr("create_runtime_object failed\n");
		return NULL;
	}
	CDS_INIT_LIST_HEAD(&context.region_list);

	/*use breadthfirst_engine*/
	p_breadthfirst_engine = create_breadthfirst_engine();
	if (p_breadthfirst_engine == NULL){
		msg_stderr("create_breadthfirst_engine failed\n");
		delete_runtime_object(&context.runtime_object);
		return NULL;
	}
	p_breadthfirst_engine->private_data = &context;

	_region = find_or_create_region(&context,
			ptr,
			byte_size,
			p_precompiled_type->region_element_type[0],
			elements_count,
			NULL,
			NULL);
	if (_region == NULL) {
		msg_stderr("find_or_create_region failed\n");
		cleanup_runtime_context(&context);
		delete_breadthfirst_engine(&p_breadthfirst_engine);
		delete_runtime_object(&context.runtime_object);
		return NULL;
	}

	if (create_and_add_runtime_task_4_region(p_breadthfirst_engine,
			NULL,
			_runtime_task_fn,
			_runtime_task_destroy_fn, _region->p_region) == NULL) {
		msg_stderr("add_initial_precompile_task failed\n");

		cds_list_for_each_entry(_region, &context.region_list, list) {
			delete_region(&_region->p_region);
		}

		cleanup_runtime_context(&context);
		delete_breadthfirst_engine(&p_breadthfirst_engine);
		delete_runtime_object(&context.runtime_object);
		return NULL;
	}
	if (run_breadthfirst_engine(p_breadthfirst_engine, NULL) != 0) {
		msg_stderr("run_breadthfirst_engine failed\n");

		cds_list_for_each_entry(_region, &context.region_list, list) {
			delete_region(&_region->p_region);
		}

		cleanup_runtime_context(&context);
		delete_breadthfirst_engine(&p_breadthfirst_engine);
		delete_runtime_object(&context.runtime_object);
		return NULL;
	}

	/*TBD move to separate func???*/
	context.runtime_object->region = calloc(context.runtime_object->regions_count, sizeof(*(context.runtime_object->region)));
	if (context.runtime_object->region == NULL) {
		msg_stderr("Can't allocate memory for regions array in runtime_object\n");

		cds_list_for_each_entry(_region, &context.region_list, list) {
			delete_region(&_region->p_region);
		}

		cleanup_runtime_context(&context);
		delete_breadthfirst_engine(&p_breadthfirst_engine);
		delete_runtime_object(&context.runtime_object);
		return NULL;
	}

	i = 0;
	cds_list_for_each_entry(_region, &context.region_list, list) {
		context.runtime_object->region[i] = _region->p_region;
		++i;
	}
	assert(context.runtime_object->regions_count == i);

	/*find unique regions*/
	qsort(context.runtime_object->region,
			context.runtime_object->regions_count,
			sizeof(*(context.runtime_object->region)),
			_compare_regions); /*TODO: change to hsort?*/

	region = context.runtime_object->region[0];

	assert(region->part_of_region == NULL);
	context.runtime_object->unique_regions_count = 1;

	msg_stddbg("starting checking addresses of regions: %p\n", region->ptr);
	for (i = 1; i < context.runtime_object->regions_count; i++) {
		msg_stddbg("%p\n", context.runtime_object->region[i]->ptr);

		if (context.runtime_object->region[i]->part_of_region != NULL) {
			msg_stddbg("skipping\n");
			continue;
		}

		if (	context.runtime_object->region[i]->ptr >= region->ptr &&
				context.runtime_object->region[i]->ptr < region->ptr + region->byte_size) { /*within the previous*/
			if (context.runtime_object->region[i]->ptr + context.runtime_object->region[i]->byte_size > region->ptr + region->byte_size) {
				msg_stderr("Warning: region(%d) %p %d is partially within previous %p %d\n",
						i,
						context.runtime_object->region[i]->ptr,
						context.runtime_object->region[i]->byte_size,
						region->ptr,
						region->byte_size);
			}
			context.runtime_object->region[i]->part_of_region = region;
		}else {
			region = context.runtime_object->region[0];
			++context.runtime_object->unique_regions_count;
		}
	}

	context.runtime_object->unique_region = calloc(context.runtime_object->unique_regions_count, sizeof(*(context.runtime_object->region)));
	if (context.runtime_object->region == NULL) {
		msg_stderr("Can't allocate memory for unique_region array in runtime_object\n");

		cds_list_for_each_entry(_region, &context.region_list, list) {
			delete_region(&_region->p_region);
		}

		cleanup_runtime_context(&context);
		delete_breadthfirst_engine(&p_breadthfirst_engine);
		delete_runtime_object(&context.runtime_object);
		return NULL;
	}

	/* get regions back, add unique regions or initialize not-unique's location*/
	i = 0; j = 0; k = 0;
	cds_list_for_each_entry(_region, &context.region_list, list) {
		context.runtime_object->region[i] = _region->p_region;
		if (context.runtime_object->region[i]->part_of_region == NULL) {
			assert(j < context.runtime_object->unique_regions_count);
			context.runtime_object->unique_region[j] = context.runtime_object->region[i];
			/*init unique_region_id*/
			context.runtime_object->region[i]->unique_region_id = j;
			++j;
		} else { /*init not-unique region location */
			/*if we go in direction of region creation will appear in certain sequence so we won't need to do while in this algorithm*/
			assert( context.runtime_object->region[i]->part_of_region->part_of_region == NULL ||
					context.runtime_object->region[i]->part_of_region->unique_region_id != -1);

			context.runtime_object->region[i]->location.offset =
					context.runtime_object->region[i]->ptr - context.runtime_object->region[i]->part_of_region->ptr;
			if (context.runtime_object->region[i]->part_of_region->part_of_region == NULL) {
				context.runtime_object->region[i]->location.region_idx = context.runtime_object->region[i]->part_of_region->unique_region_id;
			}else {
				context.runtime_object->region[i]->location.region_idx = context.runtime_object->region[i]->part_of_region->location.region_idx;
				context.runtime_object->region[i]->location.offset += context.runtime_object->region[i]->part_of_region->location.offset;
			}
		}
		++i;
	}
	assert(context.runtime_object->regions_count == i);

	/* initialize pointers table */
	if (context.runtime_object->pointer_table_items_count > 0) {
		context.runtime_object->pointer_table_items = calloc(context.runtime_object->pointer_table_items_count, sizeof(*context.runtime_object->pointer_table_items));
		if (context.runtime_object->pointer_table_items == NULL) {
			msg_stderr("Can't allocate memory for pointer_table_items array in runtime_object\n");

			cds_list_for_each_entry(_region, &context.region_list, list) {
				delete_region(&_region->p_region);
			}

			cleanup_runtime_context(&context);
			delete_breadthfirst_engine(&p_breadthfirst_engine);
			delete_runtime_object(&context.runtime_object);
			return NULL;
		}
		l = 0;
		for (i = 0; i < context.runtime_object->regions_count; i++) {
			for (j = 0; j < context.runtime_object->region[i]->elements_count; j++) {
				for (k = 0; k < context.runtime_object->region[i]->elements[j].region_element_type->pointer_type_elements_count; k++) {
					if (context.runtime_object->region[i]->elements[j].p_pointer[k].p_region != NULL) {
						context.runtime_object->pointer_table_items[l].p_pointer = &context.runtime_object->region[i]->elements[j].p_pointer[k];

						/*set location*/
						context.runtime_object->pointer_table_items[l].location.offset =
								((metac_data_member_location_t)(context.runtime_object->region[i]->elements[j].ptr - context.runtime_object->region[i]->ptr)) +
								context.runtime_object->region[i]->elements[j].region_element_type->pointer_type_element[k]->offset;

						if (context.runtime_object->region[i]->part_of_region == NULL) {
							assert(context.runtime_object->region[i]->unique_region_id != -1);
							context.runtime_object->pointer_table_items[l].location.region_idx = context.runtime_object->region[i]->unique_region_id;
						}else {
							context.runtime_object->pointer_table_items[l].location.region_idx = context.runtime_object->region[i]->location.region_idx;
							context.runtime_object->pointer_table_items[l].location.offset += context.runtime_object->region[i]->location.offset;
						}

						/*set value*/
						context.runtime_object->pointer_table_items[l].value.offset =
								0;//TBD:((metac_data_member_location_t)(new_ptr-context.runtime_object->region[i]->elements[j].p_pointer[k].p_region->ptr));

						if (context.runtime_object->region[i]->elements[j].p_pointer[k].p_region->part_of_region == NULL) {
							assert(context.runtime_object->region[i]->elements[j].p_pointer[k].p_region->unique_region_id != -1);
							context.runtime_object->pointer_table_items[l].value.region_idx = context.runtime_object->region[i]->elements[j].p_pointer[k].p_region->unique_region_id;
						}else {
							context.runtime_object->pointer_table_items[l].value.region_idx = context.runtime_object->region[i]->elements[j].p_pointer[k].p_region->location.region_idx;
							context.runtime_object->pointer_table_items[l].value.offset += context.runtime_object->region[i]->elements[j].p_pointer[k].p_region->location.offset;
						}
						msg_stddbg("ptr %d location %d %d value %d %d\n",
								l,
								context.runtime_object->pointer_table_items[l].location.region_idx,
								context.runtime_object->pointer_table_items[l].location.offset,
								context.runtime_object->pointer_table_items[l].value.region_idx,
								context.runtime_object->pointer_table_items[l].value.offset);
						++l;
					}
				}
			}
		}
		assert(context.runtime_object->pointer_table_items_count == l);
	}


	cleanup_runtime_context(&context);
	delete_breadthfirst_engine(&p_breadthfirst_engine);
	return context.runtime_object;
}
/*****************************************************************************/
static int runtime_object_check_pointer_table(
		struct metac_runtime_object * p_runtime_object) {
	int i;
	for (i = 0; i < p_runtime_object->pointer_table_items_count; i++ ){
		if (
				p_runtime_object->pointer_table_items[i].location.region_idx < 0 ||
				p_runtime_object->pointer_table_items[i].location.region_idx >= p_runtime_object->unique_regions_count ||
				p_runtime_object->pointer_table_items[i].value.region_idx < 0 ||
				p_runtime_object->pointer_table_items[i].value.region_idx >= p_runtime_object->unique_regions_count
			) {
			msg_stderr("idx is incorrect for item %d\n", i);
			return -EFAULT;
		}
		if (
				p_runtime_object->pointer_table_items[i].location.offset + sizeof(void*) >
					p_runtime_object->unique_region[p_runtime_object->pointer_table_items[i].location.region_idx]->byte_size ||
				p_runtime_object->pointer_table_items[i].value.offset + p_runtime_object->pointer_table_items[i].p_pointer->p_region->byte_size >
					p_runtime_object->unique_region[p_runtime_object->pointer_table_items[i].value.region_idx]->byte_size
			) {
			msg_stderr("offset is incorrect for item %d\n", i);
			return -EFAULT;
		}
	}
	return 0;
}
/*****************************************************************************/
static int free_runtime_object(struct metac_runtime_object ** pp_runtime_object) {
	return delete_runtime_object(pp_runtime_object);
}
/*****************************************************************************/
int metac_delete(
		void *ptr,
		metac_byte_size_t size,
		metac_precompiled_type_t * precompiled_type,
		metac_count_t elements_count) {
	int i;
	struct metac_runtime_object * p_runtime_object;

	p_runtime_object = build_runtime_object(ptr, size, precompiled_type, elements_count);
	if (p_runtime_object == NULL) {
		msg_stderr("Error while building runtime object\n");
		return -EFAULT;
	}

	for (i = 0; i < p_runtime_object->unique_regions_count; i++) {
		msg_stddbg("freeing region with ptr %p\n", p_runtime_object->unique_region[i]->ptr);
		free(p_runtime_object->unique_region[i]->ptr);
	}
	free_runtime_object(&p_runtime_object);
	return 0;
}
/*****************************************************************************/
int metac_copy(
		void *ptr,
		metac_byte_size_t size,
		metac_precompiled_type_t * precompiled_type,
		metac_count_t elements_count,
		void **p_ptr) {
	int i;
	struct metac_runtime_object * p_runtime_object;
	void ** ptrs;

	p_runtime_object = build_runtime_object(ptr, size, precompiled_type, elements_count);
	if (p_runtime_object == NULL) {
		msg_stderr("Error while building runtime object\n");
		return -EFAULT;
	}

	/* self check - it's better to do this for safety */
	if (runtime_object_check_pointer_table(p_runtime_object) != 0) {
		msg_stderr("runtime_object_check_pointer_table hasn't passed\n");
		free_runtime_object(&p_runtime_object);
		return -EFAULT;
	}

	ptrs = calloc(p_runtime_object->unique_regions_count, sizeof(*ptrs));
	if (ptrs == NULL){
		msg_stderr("Can't allocate memory for temporary array of regions\n");
		free_runtime_object(&p_runtime_object);
		return -ENOMEM;
	}

	for (i = 0; i < p_runtime_object->unique_regions_count; i++) {
		ptrs[i] = calloc(p_runtime_object->unique_region[i]->elements_count, p_runtime_object->unique_region[i]->byte_size);
		if (ptrs[i] == NULL) {
			msg_stderr("Can't allocate memory new object regions\n");
			for (i = 0; i < p_runtime_object->unique_regions_count; i++) {
				if (ptrs[i]) {
					free(ptrs[i]);
					ptrs[i] = NULL;
				}
			}
			free(ptrs);
			free_runtime_object(&p_runtime_object);
			return -ENOMEM;
		}
		memcpy(ptrs[i], p_runtime_object->unique_region[i]->ptr,
				p_runtime_object->unique_region[i]->elements_count * p_runtime_object->unique_region[i]->byte_size);
	}

	for (i = 0; i < p_runtime_object->pointer_table_items_count; i++ ){
		msg_stddbg("Changing %p to %p\n",
				*((void**)(ptrs[p_runtime_object->pointer_table_items[i].location.region_idx] + p_runtime_object->pointer_table_items[i].location.offset)),
				(void*)(ptrs[p_runtime_object->pointer_table_items[i].value.region_idx] + p_runtime_object->pointer_table_items[i].value.offset));

		*((void**)(ptrs[p_runtime_object->pointer_table_items[i].location.region_idx] + p_runtime_object->pointer_table_items[i].location.offset)) =
				(void*)(ptrs[p_runtime_object->pointer_table_items[i].value.region_idx] + p_runtime_object->pointer_table_items[i].value.offset);
	}

	if (p_ptr) { /*return value*/
		*p_ptr = ptrs[0];
	}

	free(ptrs);
	free_runtime_object(&p_runtime_object);

	return 0;
}

