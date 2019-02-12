/*
 * metac_runtime.c
 *
 *  Created on: Apr 1, 2018
 *      Author: mralex
 */

//#define METAC_DEBUG_ENABLE

#include <stdlib.h>			/* calloc, qsort... */
#include <string.h>			/* strlen, strcpy */
#include <assert.h>			/* assert */
#include <errno.h>			/* ENOMEM etc */
#include <urcu/list.h>		/* I like struct cds_list_head :) */

#include "metac_type.h"
#include "metac_internals.h"	/*definitions of internal objects*/
#include "metac_debug.h"	/* msg_stderr, ...*/
#include "breadthfirst_engine.h" /*breadthfirst_engine module*/
/*****************************************************************************/
struct runtime_context {
	struct traversing_engine traversing_engine;
	struct metac_runtime_object * runtime_object;
	struct cds_list_head region_list;
	struct cds_list_head executed_tasks_queue;
};

struct _region {
	struct cds_list_head list;
	struct region * p_region;
};

struct runtime_task {
	struct traversing_engine_task task;
	struct runtime_task* parent_task;
	struct region * p_region;
};
/*****************************************************************************/
static struct _region * create__region(
		void *ptr,
		metac_byte_size_t byte_size,
		struct region_element_type * region_element_type,
		metac_array_info_t * p_array_info,
		struct region * part_of_region) {
	struct _region * _region;

	_region = calloc(1, sizeof(*_region));
	if (_region == NULL) {
		msg_stderr("no memory\n");
		return NULL;
	}

	_region->p_region = create_region(ptr, byte_size, region_element_type, p_array_info, part_of_region);
	if (_region->p_region == NULL) {
		msg_stderr("create_region failed\n");
		free(_region);
		return NULL;
	}

	return _region;
}
/*****************************************************************************/
static struct _region * simply_create__region(
		struct runtime_context * p_runtime_context,
		void *ptr,
		metac_byte_size_t byte_size,
		struct region_element_type * region_element_type,
		metac_array_info_t * p_array_info,
		struct region * part_of_region) {
	/*check if region with the same addr already exists*/
	struct _region * _region = NULL;
	/*create otherwise*/
	msg_stddbg("create region_element_type for : ptr %p byte_size %d\n", ptr, (int)byte_size);
	_region = create__region(ptr, byte_size, region_element_type, p_array_info, part_of_region);
	msg_stddbg("create region_element_type result %p\n", _region);
	if (_region == NULL) {
		msg_stddbg("create__region failed\n");
		return NULL;
	}
	cds_list_add_tail(&_region->list, &p_runtime_context->region_list);
	++p_runtime_context->runtime_object->regions_count;
	return _region;
}
/*****************************************************************************/
static struct _region * find_or_create__region(
		struct runtime_context * p_runtime_context,
		void *ptr,
		metac_byte_size_t byte_size,
		struct region_element_type * region_element_type,
		metac_array_info_t * p_array_info,
		struct region * part_of_region,
		int * p_created_flag) {
	/*check if region with the same addr already exists*/
	struct _region * _region = NULL;
	struct _region * _region_iter;

	if (p_created_flag != NULL) *p_created_flag = 0;

	cds_list_for_each_entry(_region_iter, &p_runtime_context->region_list, list) {
		if (ptr == _region_iter->p_region->ptr &&
				byte_size == _region_iter->p_region->byte_size) { /* case when ptr is inside will be covered later */
			_region = _region_iter;
			msg_stddbg("found region %p\n", _region);
			break;
		}
	}

	if (_region == NULL) {
		_region = simply_create__region(p_runtime_context, ptr, byte_size, region_element_type, p_array_info, part_of_region);
		if (_region != NULL) {
			if (p_created_flag != NULL) *p_created_flag = 1;
		}
	}

	return _region;
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

static struct runtime_task * create_and_add_runtime_task(
		struct traversing_engine * p_breadthfirst_engine,
		struct runtime_task * parent_task,
		traversing_engine_task_fn_t fn,
		struct region * p_region) {
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
	p_task->parent_task = parent_task;

	p_task->p_region = p_region;

	if (add_traversing_task_to_tail(p_breadthfirst_engine, &p_task->task) != 0) {
		msg_stderr("add_breadthfirst_task failed\n");
		free(p_task);
		return NULL;
	}
	return p_task;
}
/*****************************************************************************/
static void cleanup_runtime_context(struct runtime_context *p_runtime_context) {
	struct traversing_engine_task * task, *_task;
	struct _region * _region, * __region;

	cleanup_traversing_engine(&p_runtime_context->traversing_engine);

	cds_list_for_each_entry_safe(task, _task, &p_runtime_context->executed_tasks_queue, list) {
		struct runtime_task  * p_task = cds_list_entry(task, struct runtime_task, task);
		cds_list_del(&task->list);
		delete_runtime_task(&p_task);
	}

	cds_list_for_each_entry_safe(_region, __region, &p_runtime_context->region_list, list) {
		cds_list_del(&_region->list);
		free(_region);
	}
}
/*****************************************************************************/
static int _runtime_task_fn(
		struct traversing_engine * p_breadthfirst_engine,
		struct traversing_engine_task * p_breadthfirst_engine_task,
		int error_flag) {
	struct runtime_context * p_context = cds_list_entry(p_breadthfirst_engine, struct runtime_context, traversing_engine);
	struct runtime_task * p_task = cds_list_entry(p_breadthfirst_engine_task, struct runtime_task, task);

	int e;
	int i;
	struct region * p_region;

	cds_list_add_tail(&p_breadthfirst_engine_task->list, &p_context->executed_tasks_queue);
	if (error_flag != 0) return 0;


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
			msg_stddbg("element %d pointers: %d items\n", e, p_region_element->region_element_type->pointer_type_members_count);
		for (i = 0; i < p_region_element->region_element_type->pointer_type_members_count; i++) {
			int j;
			int res;
			metac_array_info_t * p_array_info;
			metac_count_t elements_count;
			metac_byte_size_t elements_byte_size;
			struct _region * _region;
			void * new_ptr;
			int new_region = 0;

			msg_stddbg("pointer %s\n", p_region_element->region_element_type->pointer_type_members[i]->path_within_region_element);

			res = is_region_element_precondition_true(p_region_element, &p_region_element->region_element_type->pointer_type_members[i]->precondition);
			if (res < 0) {
				msg_stderr("Something wrong with conditions\n");
				return -EFAULT;
			}else if (res == 0) {
				msg_stddbg("skipping by precondition\n");
				continue;
			}

			/* now read the pointer */
			new_ptr = *((void**)(p_region_element->ptr + p_region_element->region_element_type->pointer_type_members[i]->offset));
			if (new_ptr == NULL) {
				msg_stddbg("skipping because new ptr is null\n");
				continue;
			}

			p_array_info = metac_array_info_create_from_type(p_region_element->region_element_type->pointer_type_members[i]->type);
			if (p_array_info == NULL) {
				msg_stderr("metac_array_info_create failed - exiting\n");
				return -EFAULT;
			}

			if (p_region_element->region_element_type->pointer_type_members[i]->array_elements_count_funtion_ptr != NULL &&
				p_region_element->region_element_type->pointer_type_members[i]->array_elements_count_funtion_ptr(
					0,
					p_region_element->ptr,
					p_region_element->region_element_type->type,
					new_ptr,
					p_region_element->region_element_type->pointer_type_members[i]->array_elements_region_element_type?
							p_region_element->region_element_type->pointer_type_members[i]->array_elements_region_element_type->type:NULL,
							p_array_info,
					p_region_element->region_element_type->pointer_type_members[i]->array_elements_count_cb_context) != 0) {
				msg_stderr("Error calling array_elements_count_funtion_ptr for pointer element %d in type %s\n",
						i, p_region_element->region_element_type->type->name);
				metac_array_info_delete(&p_array_info);
				return -EFAULT;
			}

			/* calculate byte_size using length */
			elements_count = metac_array_info_get_element_count(p_array_info);
			msg_stddbg("elements_count: %d\n", (int)elements_count);
			elements_byte_size = p_region_element->region_element_type->pointer_type_members[i]->array_elements_region_element_type?
					metac_type_byte_size(p_region_element->region_element_type->pointer_type_members[i]->array_elements_region_element_type->type):0;

			if (elements_byte_size == 0 || elements_count == 0) {
				msg_stddbg("skipping because size is 0\n");
				metac_array_info_delete(&p_array_info);
				continue;
			}

			/*we have to create region and store it (need create or find to support loops) */
			_region = find_or_create__region(
					p_context,
					new_ptr,
					elements_byte_size * elements_count,
					p_region_element->region_element_type->pointer_type_members[i]->array_elements_region_element_type,
					p_array_info,
					NULL,
					&new_region);
			if (_region == NULL) {
				msg_stderr("Error calling find_or_create_region\n");
				metac_array_info_delete(&p_array_info);
				return -EFAULT;
			}
			p_region_element->p_pointer[i].p_region = _region->p_region;

			/* add task to handle this region fields properly */
			if (new_region == 1) {
				/*create the new task for this region*/
				if (create_and_add_runtime_task(p_breadthfirst_engine,
						p_task,
						_runtime_task_fn,
						_region->p_region) == NULL) {
					msg_stderr("Error calling create_and_add_runtime_task_4_region\n");
					return -EFAULT;
				}
			}else{
				metac_array_info_delete(&p_array_info);
			}
		}

		if (p_region_element->region_element_type->array_type_elements_count > 0)
			msg_stddbg("element %d arrays: %d items\n", e, p_region_element->region_element_type->array_type_elements_count);
		for (i = 0; i < p_region_element->region_element_type->array_type_elements_count; i++) {
			int j;
			int res;
			void * new_ptr;
			metac_array_info_t * p_array_info;
			metac_count_t elements_count;
			metac_byte_size_t elements_byte_size;
			struct _region * _region;

			msg_stddbg("array %s\n", p_region_element->region_element_type->array_type_members[i]->path_within_region_element);

			res = is_region_element_precondition_true(p_region_element, &p_region_element->region_element_type->array_type_members[i]->precondition);
			if (res < 0) {
				msg_stderr("Something wrong with conditions\n");
				return -EFAULT;
			}else if (res == 0) {
				msg_stddbg("skipping by precondition\n");
				continue;
			}

			assert(p_region_element->region_element_type->array_type_members[i]->type->id == DW_TAG_array_type);

			p_array_info = metac_array_info_create_from_type(p_region_element->region_element_type->array_type_members[i]->type);
			if (p_array_info == NULL) {
				msg_stderr("metac_array_info_create failed - exiting\n");
				return -EFAULT;
			}

			/* set ptr to the first element */
			new_ptr = (void*)(p_region_element->ptr + p_region_element->region_element_type->array_type_members[i]->offset);

			if (p_region_element->region_element_type->array_type_members[i]->type->array_type_info.is_flexible) {
				if (p_region_element->region_element_type->array_type_members[i]->array_elements_count_funtion_ptr == NULL) {
					msg_stddbg("skipping because don't have a cb to determine elements count\n");
					metac_array_info_delete(&p_array_info);
					continue; /*we don't handle pointers if we can't get fn*/
				}

				if (p_region_element->region_element_type->array_type_members[i]->array_elements_count_funtion_ptr(
						0,
						p_region_element->ptr,
						p_region_element->region_element_type->type,
						new_ptr,
						p_region_element->region_element_type->array_type_members[i]->array_elements_region_element_type?
								p_region_element->region_element_type->array_type_members[i]->array_elements_region_element_type->type:NULL,
						p_array_info,
						p_region_element->region_element_type->array_type_members[i]->array_elements_count_cb_context) != 0) {
					msg_stderr("Error calling array_elements_count_funtion_ptr for pointer element %d in type %s\n",
							i, p_region_element->region_element_type->type->name);
					return -EFAULT;
				}
			}

			/* calculate overall elements_count */
			elements_count = metac_array_info_get_element_count(p_array_info);
			msg_stddbg("elements_count: %d\n", (int)elements_count);
			elements_byte_size = p_region_element->region_element_type->array_type_members[i]->array_elements_region_element_type?
					metac_type_byte_size(p_region_element->region_element_type->array_type_members[i]->array_elements_region_element_type->type):0;

			if (elements_byte_size == 0 || elements_count == 0) {
				msg_stddbg("skipping because size is 0\n");
				metac_array_info_delete(&p_array_info);
				continue;
			}

			/*we have to create region and store it */
			_region = simply_create__region(
					p_context,
					new_ptr,
					elements_byte_size * elements_count,
					p_region_element->region_element_type->array_type_members[i]->array_elements_region_element_type,
					p_array_info,
					p_region);
			if (_region == NULL) {
				msg_stderr("Error calling find_or_create_region\n");
				metac_array_info_delete(&p_array_info);
				return -EFAULT;
			}
			p_region_element->p_array[i].p_region = _region->p_region;
			/* add task to handle this region fields properly */
			/*create the new task for this region*/
			if (create_and_add_runtime_task(p_breadthfirst_engine,
					p_task,
					_runtime_task_fn,
					_region->p_region) == NULL) {
				msg_stderr("Error calling create_and_add_runtime_task_4_region\n");
				return -EFAULT;
			}
		}
	}

	msg_stddbg("finished task\n");
	return 0;
}
/*****************************************************************************/
static int _compare_regions(const void *_a, const void *_b) {
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

static struct metac_runtime_object * create_runtime_object_from_ptr(
		void * ptr,
		metac_byte_size_t byte_size,
		struct metac_precompiled_type * p_precompiled_type,
		metac_count_t elements_count) {
	int i, j, k, l;
	struct runtime_context context;
	metac_array_info_t * p_array_info;
	struct region * region;
	struct _region * _region;

	if (p_precompiled_type->region_element_type[0]->members[0]->byte_size > byte_size) {
		msg_stderr("byte_size parameter is too small for this precompiled type\n");
		return NULL;
	}

	context.runtime_object = create_runtime_object(p_precompiled_type);
	if (context.runtime_object == NULL) {
		msg_stderr("create_runtime_object failed\n");
		return NULL;
	}
	CDS_INIT_LIST_HEAD(&context.region_list);
	CDS_INIT_LIST_HEAD(&context.executed_tasks_queue);

	/*use breadthfirst_engine*/
	if (init_traversing_engine(&context.traversing_engine)!=0){
		msg_stderr("create_breadthfirst_engine failed\n");
		free_runtime_object(&context.runtime_object);
		return NULL;
	}

	p_array_info = metac_array_info_create_from_elements_count(elements_count);
	if (p_array_info == NULL){
		msg_stderr("metac_array_info_create_from_elements_count failed\n");
		cleanup_runtime_context(&context);
		free_runtime_object(&context.runtime_object);
		return NULL;
	}

	_region = simply_create__region(&context,
			ptr,
			byte_size,
			p_precompiled_type->region_element_type[0],
			p_array_info,
			NULL);
	if (_region == NULL) {
		msg_stderr("find_or_create_region failed\n");
		cleanup_runtime_context(&context);
		free_runtime_object(&context.runtime_object);
		return NULL;
	}

	if (create_and_add_runtime_task(&context.traversing_engine,
			NULL,
			_runtime_task_fn,
			_region->p_region) == NULL) {
		msg_stderr("add_initial_precompile_task failed\n");

		cds_list_for_each_entry(_region, &context.region_list, list) {
			delete_region(&_region->p_region);
		}

		cleanup_runtime_context(&context);
		free_runtime_object(&context.runtime_object);
		return NULL;
	}
	if (run_traversing_engine(&context.traversing_engine) != 0) {
		msg_stderr("run_breadthfirst_engine failed\n");

		cds_list_for_each_entry(_region, &context.region_list, list) {
			delete_region(&_region->p_region);
		}

		cleanup_runtime_context(&context);
		free_runtime_object(&context.runtime_object);
		return NULL;
	}

	msg_stddbg("regions_count: %d\n", (int)context.runtime_object->regions_count);
	context.runtime_object->region = calloc(context.runtime_object->regions_count, sizeof(*(context.runtime_object->region)));
	if (context.runtime_object->region == NULL) {
		msg_stderr("Can't allocate memory for regions array in runtime_object\n");

		cds_list_for_each_entry(_region, &context.region_list, list) {
			delete_region(&_region->p_region);
		}

		cleanup_runtime_context(&context);
		free_runtime_object(&context.runtime_object);
		return NULL;
	}

	i = 0;
	cds_list_for_each_entry(_region, &context.region_list, list) {
		_region->p_region->id = i;
		context.runtime_object->region[i] = _region->p_region;
		++i;
	}
	assert(context.runtime_object->regions_count == i);

	/*find unique regions*/
	qsort(context.runtime_object->region,
			context.runtime_object->regions_count,
			sizeof(*(context.runtime_object->region)),
			_compare_regions); /*TODO: change to hsort? - low prio so far*/

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

			if (context.runtime_object->region[i]->ptr == region->ptr &&
				context.runtime_object->region[i]->byte_size == region->byte_size) {
				/*TBD: this has to be handled*/
				msg_stderr("Ambiguity between 2 regions\n");
			}

			if (context.runtime_object->region[i]->ptr + context.runtime_object->region[i]->byte_size > region->ptr + region->byte_size) {
				msg_stderr("Warning: region(%d) %p %d is partially within previous %p %d\n",
						i,
						context.runtime_object->region[i]->ptr,
						context.runtime_object->region[i]->byte_size,
						region->ptr,
						region->byte_size);
			}
			context.runtime_object->region[i]->part_of_region = region;
		} else {
			region = context.runtime_object->region[0];
			++context.runtime_object->unique_regions_count;
		}
	}

	msg_stddbg("unique_regions_count: %d\n", (int)context.runtime_object->unique_regions_count);
	context.runtime_object->unique_region = calloc(context.runtime_object->unique_regions_count, sizeof(*(context.runtime_object->region)));
	if (context.runtime_object->region == NULL) {
		msg_stderr("Can't allocate memory for unique_region array in runtime_object\n");

		cds_list_for_each_entry(_region, &context.region_list, list) {
			delete_region(&_region->p_region);
		}

		cleanup_runtime_context(&context);
		free_runtime_object(&context.runtime_object);
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

	cleanup_runtime_context(&context);
	return context.runtime_object;
}
/*****************************************************************************/
struct pointer_table_item {
	struct pointer * p_pointer;
	struct _location location;
	struct _location value;
};
struct pointer_table {
	metac_count_t i;
	metac_count_t items_count;
	struct pointer_table_item * items;
};
struct runtime_object_ponter_tables {
	metac_count_t tables_count;
	struct pointer_table * tables;

	struct metac_runtime_object * p_runtime_object;
};
/*****************************************************************************/
static int _compare_pointer_table_item_per_location(const void *_a, const void *_b) {
	struct pointer_table_item *a = ((struct pointer_table_item *)_a);
	struct pointer_table_item *b = ((struct pointer_table_item *)_b);
	if (a->location.offset == b->location.offset)
		return 0;
	if (a->location.offset < b->location.offset)
		return -1;
	return 1;
}
/*****************************************************************************/
static int delete_runtime_object_ponter_tables(
		struct runtime_object_ponter_tables ** pp_ponter_tables) {
	struct runtime_object_ponter_tables *p_ponter_tables;

	if (pp_ponter_tables != NULL) {
		p_ponter_tables = *pp_ponter_tables;
		if (p_ponter_tables != NULL) {
			if (p_ponter_tables->tables != NULL) {
				int i;
				for (i = 0; i < p_ponter_tables->tables_count; ++i) {
					if (p_ponter_tables->tables[i].items != NULL) {
						free(p_ponter_tables->tables[i].items);
						p_ponter_tables->tables[i].items = NULL;
					}
				}
				free(p_ponter_tables->tables);
				p_ponter_tables->tables = NULL;
			}

			p_ponter_tables->p_runtime_object = NULL;

			free(p_ponter_tables);
			*pp_ponter_tables = NULL;
		}
	}
	return 0;
}

static int create_runtime_object_pointer_table(
		struct metac_runtime_object * p_runtime_object,
		struct runtime_object_ponter_tables ** pp_pointer_tables) {
	int i, j, k;
	struct runtime_object_ponter_tables * p_pointer_tables;

	p_pointer_tables = calloc(1, sizeof(*p_pointer_tables));
	if (p_pointer_tables == NULL) {
		msg_stderr("Can't allocate memory for pointer_table_items array in runtime_object\n");
		delete_runtime_object_ponter_tables(&p_pointer_tables);
		return -ENOMEM;
	}

	p_pointer_tables->p_runtime_object = p_runtime_object;
	p_pointer_tables->tables_count = p_runtime_object->unique_regions_count;
	p_pointer_tables->tables = calloc(p_pointer_tables->tables_count, sizeof(*p_pointer_tables->tables));
	if (p_pointer_tables->tables == NULL) {
		msg_stderr("Can't allocate memory tables\n");
		delete_runtime_object_ponter_tables(&p_pointer_tables);
		return -ENOMEM;
	}

	/* calculate number of pointers per unique regions, but without recursion */
	for (i = 0; i < p_runtime_object->regions_count; i++) {
		metac_count_t unique_region_idx =
				get_region_unique_region_id(p_runtime_object->region[i]);
		assert(unique_region_idx < p_runtime_object->unique_regions_count);

		for (j = 0; j < p_runtime_object->region[i]->elements_count; j++) {
			for (k = 0; k < p_runtime_object->region[i]->elements[j].region_element_type->pointer_type_members_count; k++) {
				if (p_runtime_object->region[i]->elements[j].p_pointer[k].p_region) { /*non-null pointer*/
					++p_pointer_tables->tables[unique_region_idx].items_count;
				}
			}
		}
	}
	/* allocate required number of items in tables */
	for (i = 0; i < p_runtime_object->unique_regions_count; ++i) {
		if (p_pointer_tables->tables[i].items_count > 0) {
			p_pointer_tables->tables[i].items = calloc(p_pointer_tables->tables[i].items_count, sizeof(*p_pointer_tables->tables[i].items));
			if (p_pointer_tables->tables[i].items == NULL) {
				msg_stderr("Can't allocate memory items\n");
				delete_runtime_object_ponter_tables(&p_pointer_tables);
				return -ENOMEM;
			}
		}
	}

	for (i = 0; i < p_runtime_object->regions_count; i++) {
		metac_count_t unique_region_idx =
				get_region_unique_region_id(p_runtime_object->region[i]);
		assert(unique_region_idx < p_runtime_object->unique_regions_count);

		for (j = 0; j < p_runtime_object->region[i]->elements_count; j++) {
			for (k = 0; k < p_runtime_object->region[i]->elements[j].region_element_type->pointer_type_members_count; k++) {
				if (p_runtime_object->region[i]->elements[j].p_pointer[k].p_region != NULL) {
					metac_count_t _i = p_pointer_tables->tables[unique_region_idx].i;
					++p_pointer_tables->tables[unique_region_idx].i;
					assert(p_pointer_tables->tables[unique_region_idx].i <= p_pointer_tables->tables[unique_region_idx].items_count);

					p_pointer_tables->tables[unique_region_idx].items[_i].p_pointer = &p_runtime_object->region[i]->elements[j].p_pointer[k];

					/*set location*/
					p_pointer_tables->tables[unique_region_idx].items[_i].location.offset =
							((metac_data_member_location_t)(p_runtime_object->region[i]->elements[j].ptr - p_runtime_object->region[i]->ptr)) +
							p_runtime_object->region[i]->elements[j].region_element_type->pointer_type_members[k]->offset;

					if (p_runtime_object->region[i]->part_of_region == NULL) {
						assert(p_runtime_object->region[i]->unique_region_id != -1);
						p_pointer_tables->tables[unique_region_idx].items[_i].location.region_idx =
								p_runtime_object->region[i]->unique_region_id;
					}else {
						p_pointer_tables->tables[unique_region_idx].items[_i].location.region_idx =
								p_runtime_object->region[i]->location.region_idx;
						p_pointer_tables->tables[unique_region_idx].items[_i].location.offset +=
								p_runtime_object->region[i]->location.offset;
					}

					/*set value*/
					p_pointer_tables->tables[unique_region_idx].items[_i].value.offset = 0;

					if (p_runtime_object->region[i]->elements[j].p_pointer[k].p_region->part_of_region == NULL) {
						assert(p_runtime_object->region[i]->elements[j].p_pointer[k].p_region->unique_region_id != -1);
						p_pointer_tables->tables[unique_region_idx].items[_i].value.region_idx =
								p_runtime_object->region[i]->elements[j].p_pointer[k].p_region->unique_region_id;
					}else {
						p_pointer_tables->tables[unique_region_idx].items[_i].value.region_idx =
								p_runtime_object->region[i]->elements[j].p_pointer[k].p_region->location.region_idx;
						p_pointer_tables->tables[unique_region_idx].items[_i].value.offset +=
								p_runtime_object->region[i]->elements[j].p_pointer[k].p_region->location.offset;
					}
					msg_stddbg("ptr %d:%d location %d %d value %d %d\n",
							unique_region_idx,
							_i,
							p_pointer_tables->tables[unique_region_idx].items[_i].location.region_idx,
							p_pointer_tables->tables[unique_region_idx].items[_i].location.offset,
							p_pointer_tables->tables[unique_region_idx].items[_i].value.region_idx,
							p_pointer_tables->tables[unique_region_idx].items[_i].value.offset);
				}
			}
		}
	}

	if (pp_pointer_tables != NULL){
		*pp_pointer_tables = p_pointer_tables;
	}

	return 0;
}
/*****************************************************************************/
static int check_runtime_object_pointer_table(
		struct runtime_object_ponter_tables *p_pointer_tables) {
	int i, j;

	if (p_pointer_tables->tables_count != p_pointer_tables->p_runtime_object->unique_regions_count) {
		msg_stderr("incorrect number of tables\n");
		return -EFAULT;
	}

	for (i = 0; i < p_pointer_tables->tables_count; i++ ) {
		for (j = 0; j < p_pointer_tables->tables[i].items_count; j++) {
			if (
					p_pointer_tables->tables[i].items[j].location.region_idx >= p_pointer_tables->tables_count ||
					p_pointer_tables->tables[i].items[j].value.region_idx >= p_pointer_tables->tables_count
				) {
				msg_stderr("idx is incorrect for item %d\n", i);
				return -EFAULT;
			}
			if (
					p_pointer_tables->tables[i].items[j].location.offset + sizeof(void*) >
						p_pointer_tables->p_runtime_object->unique_region[p_pointer_tables->tables[i].items[j].location.region_idx]->byte_size ||
					p_pointer_tables->tables[i].items[j].value.offset + p_pointer_tables->tables[i].items[j].p_pointer->p_region->byte_size >
						p_pointer_tables->p_runtime_object->unique_region[p_pointer_tables->tables[i].items[j].value.region_idx]->byte_size
				) {
				msg_stderr("offset is incorrect for item %d\n", i);
				return -EFAULT;
			}
		}
	}
	return 0;
}
/*****************************************************************************/
int metac_delete(
		void *ptr,
		metac_byte_size_t size,
		metac_precompiled_type_t * precompiled_type,
		metac_count_t elements_count) {
	int i;
	struct metac_runtime_object * p_runtime_object;

	p_runtime_object = create_runtime_object_from_ptr(ptr, size, precompiled_type, elements_count);
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
	int i, j;
	struct metac_runtime_object * p_runtime_object;
	struct runtime_object_ponter_tables * p_ponter_tables;
	void ** ptrs;

	p_runtime_object = create_runtime_object_from_ptr(ptr, size, precompiled_type, elements_count);
	if (p_runtime_object == NULL) {
		msg_stderr("Error while building runtime object\n");
		return -EFAULT;
	}

	if (create_runtime_object_pointer_table(p_runtime_object, &p_ponter_tables) != 0) {
		msg_stderr("create_runtime_object_pointer_table failed\n");
		free_runtime_object(&p_runtime_object);
		return -EFAULT;
	}

	/* self check - it's better to do this for safety */
	if (check_runtime_object_pointer_table(p_ponter_tables) != 0) {
		msg_stderr("check_runtime_object_pointer_table hasn't passed\n");
		delete_runtime_object_ponter_tables(&p_ponter_tables);
		free_runtime_object(&p_runtime_object);
		return -EFAULT;
	}

	ptrs = calloc(p_runtime_object->unique_regions_count, sizeof(*ptrs));
	if (ptrs == NULL){
		msg_stderr("Can't allocate memory for temporary array of regions\n");
		delete_runtime_object_ponter_tables(&p_ponter_tables);
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
			delete_runtime_object_ponter_tables(&p_ponter_tables);
			free_runtime_object(&p_runtime_object);
			return -ENOMEM;
		}
		memcpy(ptrs[i], p_runtime_object->unique_region[i]->ptr,
				p_runtime_object->unique_region[i]->elements_count * p_runtime_object->unique_region[i]->byte_size);
	}

	for (i = 0; i < p_ponter_tables->tables_count; i++ ) {
		for (j = 0; j < p_ponter_tables->tables[i].items_count; j++) {
			msg_stddbg("Changing %p to %p\n",
					*((void**)(ptrs[p_ponter_tables->tables[i].items[j].location.region_idx] + p_ponter_tables->tables[i].items[j].location.offset)),
					(void*)(ptrs[p_ponter_tables->tables[i].items[j].value.region_idx] + p_ponter_tables->tables[i].items[j].value.offset));

			*((void**)(ptrs[p_ponter_tables->tables[i].items[j].location.region_idx] + p_ponter_tables->tables[i].items[j].location.offset)) =
					(void*)(ptrs[p_ponter_tables->tables[i].items[j].value.region_idx] + p_ponter_tables->tables[i].items[j].value.offset);
		}
	}

	if (p_ptr) { /*return value*/
		*p_ptr = ptrs[0];
	}

	free(ptrs);
	delete_runtime_object_ponter_tables(&p_ponter_tables);
	free_runtime_object(&p_runtime_object);

	return 0;
}
/*****************************************************************************/
static int _metac_equal(
		struct metac_runtime_object * p_runtime_object0,
		struct metac_runtime_object * p_runtime_object1) {
	int res = 1;
	int i, j, k, l;
	struct runtime_object_ponter_tables * p_pointer_tables;

	if (p_runtime_object0->regions_count != p_runtime_object1->regions_count ||
		p_runtime_object0->unique_regions_count != p_runtime_object1->unique_regions_count ) {
		return 0;
	}
	/*runtime-objects and their components are locate in mem in order of mem-parsing - we're relying on that */
	for (i = 0; i < p_runtime_object0->regions_count; ++i) {
		if (p_runtime_object0->region[i]->elements_count != p_runtime_object1->region[i]->elements_count ||
			p_runtime_object0->region[i]->byte_size != p_runtime_object1->region[i]->byte_size) {
			return 0;
		}
		for (j = 0; j < p_runtime_object0->region[i]->elements_count; ++j) {
			if (p_runtime_object0->region[i]->elements[j].region_element_type != p_runtime_object1->region[i]->elements[j].region_element_type ||
				p_runtime_object0->region[i]->elements[j].byte_size != p_runtime_object1->region[i]->elements[j].byte_size) {
				return 0;
			}
			/* compare discriminator values */
			for (k = 0; k < p_runtime_object0->region[i]->elements[j].region_element_type->discriminators_count; ++k) {
				if (p_runtime_object0->region[i]->elements[j].p_discriminator_value[k].is_initialized != p_runtime_object1->region[i]->elements[j].p_discriminator_value[k].is_initialized) {
					return 0;
				}
				if (p_runtime_object0->region[i]->elements[j].p_discriminator_value[k].is_initialized != 0 &&
					p_runtime_object0->region[i]->elements[j].p_discriminator_value[k].value != p_runtime_object1->region[i]->elements[j].p_discriminator_value[k].value) {
					return 0;
				}
			}
			/*compare pointers*/
			for (k = 0; k < p_runtime_object0->region[i]->elements[j].region_element_type->pointer_type_members_count; ++k) {
				if ((p_runtime_object0->region[i]->elements[j].p_pointer[k].p_region != NULL &&
					p_runtime_object1->region[i]->elements[j].p_pointer[k].p_region != NULL &&
					metac_array_info_equal(p_runtime_object0->region[i]->elements[j].p_pointer[k].p_region->p_array_info, p_runtime_object1->region[i]->elements[j].p_pointer[k].p_region->p_array_info) == 0) ||
					(p_runtime_object0->region[i]->elements[j].p_pointer[k].p_region == NULL) != (p_runtime_object1->region[i]->elements[j].p_pointer[k].p_region == NULL) ||
					(p_runtime_object0->region[i]->elements[j].p_pointer[k].p_region != NULL &&
						(p_runtime_object0->region[i]->elements[j].p_pointer[k].p_region->id != p_runtime_object1->region[i]->elements[j].p_pointer[k].p_region->id))
					) {
					return 0;
				}
			}
			/*compare arrays*/
			for (k = 0; k < p_runtime_object0->region[i]->elements[j].region_element_type->array_type_elements_count; ++k) {
				if ((p_runtime_object0->region[i]->elements[j].p_array[k].p_region != NULL &&
					p_runtime_object1->region[i]->elements[j].p_array[k].p_region != NULL &&
					metac_array_info_equal(p_runtime_object0->region[i]->elements[j].p_array[k].p_region->p_array_info, p_runtime_object1->region[i]->elements[j].p_array[k].p_region->p_array_info) == 0) ||
					(p_runtime_object0->region[i]->elements[j].p_array[k].p_region == NULL) != (p_runtime_object1->region[i]->elements[j].p_array[k].p_region == NULL) ||
					(p_runtime_object0->region[i]->elements[j].p_array[k].p_region != NULL &&
						(p_runtime_object0->region[i]->elements[j].p_array[k].p_region->id != p_runtime_object1->region[i]->elements[j].p_array[k].p_region->id))
					) {
					return 0;
				}
			}
		}
	}

	if (create_runtime_object_pointer_table(p_runtime_object0, &p_pointer_tables) != 0) {
		msg_stderr("create_runtime_object_pointer_table failed\n");
		return -ENOMEM;
	}

	for (i = 0; i < p_pointer_tables->tables_count; i++ ) {
		metac_data_member_location_t prev_offset = 0;

		/*sort by offset!*/
		qsort(p_pointer_tables->tables[i].items,
				p_pointer_tables->tables[i].items_count,
				sizeof(*(p_pointer_tables->tables[i].items)),
				_compare_pointer_table_item_per_location); /*TODO: change to hsort? - low prio so far*/
		/*now go though*/
		for (j = 0; j < p_pointer_tables->tables[i].items_count; j++) {
			msg_stddbg("reg %d, off %d, sz %d\n", i, prev_offset, p_pointer_tables->tables[i].items[j].location.offset - prev_offset);
			res = 	(
					memcmp(
							p_runtime_object0->unique_region[i]->ptr + prev_offset,
							p_runtime_object1->unique_region[i]->ptr + prev_offset,
							p_pointer_tables->tables[i].items[j].location.offset - prev_offset) == 0
					)?1:0;
			msg_stddbg("res1 %d\n", res);
			if (res == 0)
				break;
			prev_offset = p_pointer_tables->tables[i].items[j].location.offset + sizeof(void*);
		}
		if (res == 0)
			break;
		/*compare tail*/
		if (p_runtime_object0->unique_region[i]->byte_size > prev_offset) {
			msg_stddbg("reg %d, off %d, sz %d\n", i, prev_offset, p_runtime_object0->unique_region[i]->byte_size - prev_offset);
			res = 	(
					memcmp(
							p_runtime_object0->unique_region[i]->ptr + prev_offset,
							p_runtime_object1->unique_region[i]->ptr + prev_offset,
							p_runtime_object0->unique_region[i]->byte_size - prev_offset) == 0
					)?1:0;
			msg_stddbg("res2 %d\n", res);
			if (res == 0)
				break;
		}
	}

	delete_runtime_object_ponter_tables(&p_pointer_tables);

	msg_stddbg("res %d\n", res);
	return res;
}

int metac_equal(
		void *ptr0,
		void *ptr1,
		metac_byte_size_t size,
		metac_precompiled_type_t * precompiled_type,
		metac_count_t elements_count) {
	int res;
	struct metac_runtime_object * p_runtime_object0;
	struct metac_runtime_object * p_runtime_object1;

	p_runtime_object0 = create_runtime_object_from_ptr(ptr0, size, precompiled_type, elements_count);
	if (p_runtime_object0 == NULL) {
		msg_stderr("Error while building runtime object0\n");
		return -EFAULT;
	}
	p_runtime_object1 = create_runtime_object_from_ptr(ptr1, size, precompiled_type, elements_count);
	if (p_runtime_object1 == NULL) {
		msg_stderr("Error while building runtime object1\n");
		free_runtime_object(&p_runtime_object0);
		return -EFAULT;
	}

	res = _metac_equal(p_runtime_object0, p_runtime_object1);

	free_runtime_object(&p_runtime_object0);
	free_runtime_object(&p_runtime_object1);
	return res;
}
/*****************************************************************************/
#define _FOR_EACH_REGION_ELEMENT_TYPE_ELEMENT(i, _p_region_element, _elements, _elements_count) \
	do { \
		int i; \
		for (i = 0; i < _elements_count; ++i) { \
			if (is_region_element_precondition_true(p_region_element, &(_elements[i]->precondition)))
#define _FOR_EACH_REGION_ELEMENT_TYPE_ELEMENT_DONE \
		} \
	}while(0)
static int _get_region_element_type_element_count(
		struct region_element * p_region_element,
		struct region_element_type_member ** elements,
		int elements_count) {
	int res = 0;
	_FOR_EACH_REGION_ELEMENT_TYPE_ELEMENT(i, p_region_element, elements, elements_count) {
		/*TBD: skip zero-length arrays and pointers */
		++res;
	}_FOR_EACH_REGION_ELEMENT_TYPE_ELEMENT_DONE;
	return res;
}
static int _get_region_element_type_element_info(
		struct region_element_type * p_region_element_type,
		metac_region_ee_subtype_t subtype,
		struct region_element_type_member *** ppp_elements,
		int * p_elements_count) {
	assert(ppp_elements && p_elements_count);
	switch(subtype){
	case reesHierarchy:
		*ppp_elements = p_region_element_type->hierarchy_members;
		*p_elements_count = p_region_element_type->hierarchy_members_count;
		break;
	case reesEnum:
		*ppp_elements = p_region_element_type->enum_type_members;
		*p_elements_count = p_region_element_type->enum_type_members_count;
		break;
	case reesBase:
		*ppp_elements = p_region_element_type->base_type_members;
		*p_elements_count = p_region_element_type->base_type_members_count;
		break;
	case reesPointer:
		*ppp_elements = p_region_element_type->pointer_type_members;
		*p_elements_count = p_region_element_type->pointer_type_members_count;
		break;
	case reesArray:
		*ppp_elements = p_region_element_type->array_type_members;
		*p_elements_count = p_region_element_type->array_type_elements_count;
		break;
	case reesAll:
	default:
		*ppp_elements = p_region_element_type->members;
		*p_elements_count = p_region_element_type->members_count;
		break;
	}
	return 0;
}

/*****************************************************************************/
int metac_visit(
	void *ptr,
	metac_byte_size_t size,
	metac_precompiled_type_t * precompiled_type,
	metac_count_t elements_count,
	metac_region_ee_subtype_t *subtypes_sequence,
	int subtypes_sequence_lenth,
	struct metac_visitor * p_visitor) {
	int cb_res;
	int i, j, k;
	static metac_region_ee_subtype_t default_subtypes_sequence[] = {
		reesHierarchy,
		reesEnum,
		reesBase,
		reesPointer,
		reesArray,
	};
	struct metac_runtime_object * p_runtime_object;
	int real_region_element_element_count;
	int * real_count_array;

	if (subtypes_sequence == NULL) {
		subtypes_sequence = default_subtypes_sequence;
		subtypes_sequence_lenth =
				sizeof(default_subtypes_sequence)/sizeof(default_subtypes_sequence[0]);
	}
	real_count_array = calloc(subtypes_sequence_lenth, sizeof(*real_count_array));
	if (subtypes_sequence_lenth > 0 && real_count_array == NULL) {
		msg_stderr("Error while building runtime object\n");
		return -ENOMEM;
	}

	p_runtime_object = create_runtime_object_from_ptr(ptr, size, precompiled_type, elements_count);
	if (p_runtime_object == NULL) {
		msg_stderr("Error while building runtime object\n");
		if (real_count_array != NULL)
			free(real_count_array);
		return -EFAULT;
	}
	/* --- */
	if (p_visitor != NULL) {
		if (p_visitor->start != NULL) {
			cb_res = p_visitor->start(
					p_visitor,
					p_runtime_object->regions_count,
					p_runtime_object->unique_regions_count);
			if (cb_res != 0) {
				msg_stddbg("start cb returned failure - exiting\n");
				if (real_count_array != NULL)
					free(real_count_array);
				free_runtime_object(&p_runtime_object);
				return -EFAULT;
			}
		}
		/* go through all regions */
		for (i = 0; i < p_runtime_object->regions_count; ++i) {
			if (p_visitor->region != NULL) {
				cb_res = p_visitor->region(
						p_visitor,
						i,
						p_runtime_object->region[i]->ptr,
						p_runtime_object->region[i]->byte_size,
						p_runtime_object->region[i]->elements_count);
				if (cb_res != 0) {
					msg_stddbg("region cb returned failure - exiting\n");
					if (real_count_array != NULL)
						free(real_count_array);
					free_runtime_object(&p_runtime_object);
					return -EFAULT;
				}
			}

			cb_res = 0;
			if (p_runtime_object->region[i]->part_of_region == NULL) {
				if (p_visitor->unique_region) {
					cb_res = p_visitor->unique_region(
							p_visitor,
							i,
							p_runtime_object->region[i]->unique_region_id);
				}
			}else{
				if (p_visitor->non_unique_region) {
					cb_res = p_visitor->non_unique_region(
							p_visitor,
							i,
							p_runtime_object->region[i]->location.region_idx,
							p_runtime_object->region[i]->location.offset);
				}
			}
			if (cb_res != 0) {
				msg_stddbg("(non_)unique_regio cb returned failure - exiting\n");
				if (real_count_array != NULL)
					free(real_count_array);
				free_runtime_object(&p_runtime_object);
				return -EFAULT;
			}
		}
		/* go through all region elements */
		if (p_visitor->region_element) {
			for (i = 0; i < p_runtime_object->regions_count; ++i) {
				for (j = 0; j < p_runtime_object->region[i]->elements_count; ++j) {
					struct region_element * p_region_element = &p_runtime_object->region[i]->elements[j];
					struct region_element_type_member ** elements;
					int elements_count;

					_get_region_element_type_element_info(
							p_region_element->region_element_type,
							reesAll,
							&elements,
							&elements_count);

					real_region_element_element_count = _get_region_element_type_element_count(
							p_region_element,
							elements,
							elements_count);

					for (k = 0; k < subtypes_sequence_lenth; ++k) {
						_get_region_element_type_element_info(
								p_region_element->region_element_type,
								subtypes_sequence[k],
								&elements,
								&elements_count);

						real_count_array[k] = _get_region_element_type_element_count(
								p_region_element,
								elements,
								elements_count);
					}

					cb_res = p_visitor->region_element(
							p_visitor,
							i,
							j,
							p_runtime_object->region[i]->elements[j].region_element_type->type,
							p_runtime_object->region[i]->elements[j].ptr,
							p_runtime_object->region[i]->elements[j].byte_size,
							real_region_element_element_count,
							subtypes_sequence,
							real_count_array,
							subtypes_sequence_lenth);
					if (cb_res != 0) {
						msg_stddbg("region_element cb returned failure - exiting\n");
						if (real_count_array != NULL)
							free(real_count_array);
						free_runtime_object(&p_runtime_object);
						return -EFAULT;
					}
				}
			}
		}
		/* go through all region element elements */
		for (i = 0; i < p_runtime_object->regions_count; ++i) {
			int _n;
			struct region_element * p_region_element = &p_runtime_object->region[i]->elements[0];
			struct region_element_type_member ** elements;
			int elements_count;
			/*real_ids array will be the same size because all elements in array has that same type*/
			msg_stddbg("allocating %d elements\n", p_region_element->region_element_type->members_count);
			metac_count_t * elements_elements_real_ids = calloc(p_region_element->region_element_type->members_count, sizeof(metac_count_t));
			if (elements_elements_real_ids == NULL) {
				msg_stderr("Error while allocation array for elements_elements_real_ids %d size\n",
						(int)p_region_element->region_element_type->members_count);
				if (real_count_array != NULL)
					free(real_count_array);
				free_runtime_object(&p_runtime_object);
				return -ENOMEM;
			}

			for (j = 0; j < p_runtime_object->region[i]->elements_count; ++j) {
				p_region_element = &p_runtime_object->region[i]->elements[j];

				memset(elements_elements_real_ids, 0, p_region_element->region_element_type->members_count * sizeof(metac_count_t));

				_get_region_element_type_element_info(
						p_region_element->region_element_type,
						reesAll,
						&elements,
						&elements_count);

				_n = 0;
				_FOR_EACH_REGION_ELEMENT_TYPE_ELEMENT(
						ee,
						p_region_element,
						elements,
						elements_count) {
					elements_elements_real_ids[ee] = _n;
					++_n;
				}_FOR_EACH_REGION_ELEMENT_TYPE_ELEMENT_DONE;

				if (p_visitor->region_element_element) {
					_n = 0;
					_FOR_EACH_REGION_ELEMENT_TYPE_ELEMENT(
							ee,
							p_region_element,
							elements,
							elements_count) {
						assert(ee == elements[ee]->id);
						cb_res = p_visitor->region_element_element(
								p_visitor,
								i,
								j,
								_n,
								elements[ee]->parent!=NULL?elements_elements_real_ids[elements[ee]->parent->id]:-1,
								elements[ee]->type,
								p_region_element->ptr + elements[ee]->offset,
								elements[ee]->p_bit_offset,
								elements[ee]->p_bit_size,
								elements[ee]->byte_size,
								elements[ee]->name_local,
								elements[ee]->path_within_region_element
								);
						if (cb_res != 0) {
							msg_stddbg("region_element_element cb returned failure - exiting\n");
							if (elements_elements_real_ids != NULL)
								free(elements_elements_real_ids);
							if (real_count_array != NULL)
								free(real_count_array);
							free_runtime_object(&p_runtime_object);
							return -EFAULT;
						}
						++_n;
					}_FOR_EACH_REGION_ELEMENT_TYPE_ELEMENT_DONE;
				}

				if (p_visitor->region_element_element_per_subtype != NULL) {
					for (k = 0; k < subtypes_sequence_lenth; ++k) {
						_get_region_element_type_element_info(
								p_region_element->region_element_type,
								subtypes_sequence[k],
								&elements,
								&elements_count);
						_n = 0;
						_FOR_EACH_REGION_ELEMENT_TYPE_ELEMENT(
								ee,
								p_region_element,
								elements,
								elements_count) {
							/* Careful: here ee means element number in array of the specific subtype */
							cb_res = 0;
							switch(subtypes_sequence[k]) {
							case reesHierarchy:
							case reesEnum:
							case reesBase:
								cb_res = p_visitor->region_element_element_per_subtype(
										p_visitor,
										i,
										j,
										elements_elements_real_ids[elements[ee]->id],
										subtypes_sequence[k],
										_n,
										NULL, 0
										);
								break;
							case reesPointer:
								cb_res = p_visitor->region_element_element_per_subtype(
										p_visitor,
										i,
										j,
										elements_elements_real_ids[elements[ee]->id],
										subtypes_sequence[k],
										_n,
										p_region_element->p_pointer[ee].p_region?p_region_element->p_pointer[ee].p_region->p_array_info:NULL,
										p_region_element->p_pointer[ee].p_region?&p_region_element->p_pointer[ee].p_region->id:NULL
										);
								break;
							case reesArray:
								cb_res = p_visitor->region_element_element_per_subtype(
										p_visitor,
										i,
										j,
										elements_elements_real_ids[elements[ee]->id],
										subtypes_sequence[k],
										_n,
										p_region_element->p_array[ee].p_region?p_region_element->p_array[ee].p_region->p_array_info:NULL,
										p_region_element->p_array[ee].p_region?&p_region_element->p_array[ee].p_region->id:NULL
										);
								break;
							default:
								break;
							}
							if (cb_res != 0) {
								msg_stddbg("region_element_element_per_subtype cb returned failure - exiting\n");
								if (elements_elements_real_ids != NULL)
									free(elements_elements_real_ids);
								if (real_count_array != NULL)
									free(real_count_array);
								free_runtime_object(&p_runtime_object);
								return -EFAULT;
							}
							++_n;
						}_FOR_EACH_REGION_ELEMENT_TYPE_ELEMENT_DONE;
					}
				}
			}
			free(elements_elements_real_ids);
		}
	}
	/* --- */
	if (real_count_array != NULL)
		free(real_count_array);
	free_runtime_object(&p_runtime_object);
	return 0;
}
