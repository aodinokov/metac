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

	struct metac_precompiled_type * p_precompiled_type;
	struct cds_list_head pointers_region_list;
	struct cds_list_head arrays_region_list;
	struct cds_list_head executed_tasks_queue;
};

struct _region {
	struct cds_list_head list;
	struct region2 * p_region;
};

struct runtime_task {
	struct traversing_engine_task task;
	struct runtime_task* parent_task;
	struct region2 * p_region;
};
/*****************************************************************************/
static int create__region(
		struct cds_list_head * p_region_list,
		void *ptr,
		struct region_element_type * region_element_type,
		metac_array_info_t * p_array_info,
		struct _region ** pp__region) {
	int res;
	struct _region * p__region;
	msg_stddbg("called\n");
	assert(pp__region);

	p__region = calloc(1, sizeof(*p__region));
	if (p__region == NULL) {
		msg_stderr("no memory\n");
		return -ENOMEM;
	}

	res = create_region2(region_element_type, p_array_info, ptr, &p__region->p_region);
	if (res != 0) {
		msg_stderr("create_region failed\n");
		free(p__region);
		return res;
	}

	*pp__region = p__region;

	cds_list_add_tail(&p__region->list, p_region_list);
	return res;
}
/*****************************************************************************/
static int create__region_for_array(
		struct cds_list_head * p_region_list,
		void *ptr,
		struct region_element_type * region_element_type,
		metac_array_info_t * p_array_info,
		struct region2 * p_region_parent,
		struct region_element * p_region_element_parent,
		struct region_element_type_member * p_member_parent,
		metac_data_member_location_t offset_parent,
		struct _region ** pp__region) {
	int res;

	res = create__region(
			p_region_list,
			ptr,
			region_element_type,
			p_array_info,
			pp__region);
	if (res != 0) {
		msg_stderr("create__region returned non success\n");
		return res;
	}

	res = update_region2_non_allocated(
			(*pp__region)->p_region,
			p_region_parent,
			p_region_element_parent,
			p_member_parent,
			offset_parent);
	if (res != 0) {
		msg_stderr("Warning: update_region2_non_allocated returned non success\n");
	}
	return 0;
}
/*****************************************************************************/
static int create__region_for_pointer(
		struct cds_list_head * p_region_list,
		void *ptr,
		struct region_element_type * region_element_type,
		metac_array_info_t * p_array_info,
		struct region2 * p_region_parent,
		struct region_element * p_region_element_parent,
		struct region_element_type_member * p_member_parent,
		metac_data_member_location_t offset_parent,
		struct _region ** pp__region) {
	int res;
	struct _region * _region = NULL;
	struct _region * _region_iter;
	metac_byte_size_t bytesize = metac_type_byte_size(region_element_type->type) * metac_array_info_get_element_count(p_array_info);

	cds_list_for_each_entry(_region_iter, p_region_list, list) {
		/*compare ptr and it's type. bytesize for a new region isn't known at this point in case of flexible field*/
		if (ptr == _region_iter->p_region->ptr &&
			/*_region_iter->p_region->elements_count > 0 &&*/
			/*region_element_type->type == _region_iter->p_region->p_region_element_type->type &&*/
			bytesize == get_region2_static_bytesize(_region_iter->p_region)) {
			/* case when ptr is inside will be covered later */
			_region = _region_iter;
			msg_stddbg("found region for %p\n", _region->p_region->ptr);
			if (pp__region)
				*pp__region = _region;
			return 2;
		}
	}

	res = create__region(
			p_region_list,
			ptr,
			region_element_type,
			p_array_info,
			pp__region);
	if (res != 0) {
		msg_stderr("create__region returned non success\n");
		return res;
	}

	return 0;
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
/*****************************************************************************/
static struct runtime_task * create_and_add_runtime_task(
		struct traversing_engine * p_breadthfirst_engine,
		struct runtime_task * parent_task,
		traversing_engine_task_fn_t fn,
		struct region2 * p_region) {
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
static int _runtime_task_fn(
		struct traversing_engine * p_traversing_engine,
		struct traversing_engine_task * p_traversing_engine_task,
		int error_flag) {
	int e;
	struct runtime_context * p_context = cds_list_entry(p_traversing_engine, struct runtime_context, traversing_engine);
	struct runtime_task * p_task = cds_list_entry(p_traversing_engine_task, struct runtime_task, task);
	struct region2 * p_region = p_task->p_region;

	cds_list_add_tail(&p_traversing_engine_task->list, &p_context->executed_tasks_queue);
	if (error_flag != 0) return 0;

	assert(p_region);
	assert(p_region->elements_count > 0);
	assert(p_region->p_region_element_type->type);
	msg_stddbg("started task %p (%s) for %d elements\n",
			p_region->ptr,
			p_region->p_region_element_type->type->name,
			p_region->elements_count);

	for (e = 0; e < p_region->elements_count; e++) {
		int res;
		struct _region * _region;
		metac_array_info_t * p_array_info;
		metac_byte_size_t elements_bytesize;
		metac_count_t elements_count;
		int m;

		struct region_element * p_region_element = &p_region->elements[e];
		assert(p_region->p_region_element_type == p_region_element->region_element_type);
		if (p_region_element->region_element_type->array_type_elements_count > 0) {
			msg_stddbg("element %d arrays: %d items\n", e, p_region_element->region_element_type->array_type_elements_count);
			for (m = 0; m < p_region_element->region_element_type->array_type_elements_count; m++) {
				void * new_ptr;

				assert(p_region_element->region_element_type->array_type_members[m]->type->id == DW_TAG_array_type);
				assert(p_region_element->region_element_type->array_type_members[m]->is_flexible ||
					p_region_element->region_element_type->array_type_members[m]->array_elements_region_element_type);
				msg_stddbg("array %s\n", p_region_element->region_element_type->array_type_members[m]->path_within_region_element);

				res = is_region_element_precondition_true(p_region_element, &p_region_element->region_element_type->array_type_members[m]->precondition);
				if (res < 0) {
					return -EFAULT;
				}
				if (res == 0) {
					msg_stddbg("skipping by precondition\n");
					continue;
				}
				/* check that flexible array can be used */
				if (p_region_element->region_element_type->array_type_members[m]->is_flexible && (
						p_region->elements_count > 1 ||
						p_region->is_allocated_region == 0)
					) {
					msg_stddbg("skipping flexible array in element of array\n");
					continue;
				}
				if (p_region_element->region_element_type->array_type_members[m]->is_flexible && (
						p_region_element->region_element_type->array_type_members[m]->array_elements_count_funtion_ptr == NULL ||
						p_region_element->region_element_type->array_type_members[m]->array_elements_region_element_type == NULL)
					) {
					msg_stddbg("skipping because don't have a cb to determine elements count\n");
					continue; /*we don't handle pointers if we can't get fn*/
				}
				/* set ptr to the first element */
				new_ptr = (void*)(p_region_element->ptr + p_region_element->region_element_type->array_type_members[m]->offset);
				/* create array info (array with sizes in all dimensions) */
				p_array_info = metac_array_info_create_from_type(p_region_element->region_element_type->array_type_members[m]->type);
				if (p_array_info == NULL) {
					msg_stderr("metac_array_info_create failed - exiting\n");
					return -EFAULT;
				}
				/* get updated p_array_info if array is flexible */
				if (p_region_element->region_element_type->array_type_members[m]->is_flexible) {
					if (p_region_element->region_element_type->array_type_members[m]->array_elements_count_funtion_ptr(
							0,
							p_region_element->ptr,
							p_region_element->region_element_type->type,
							new_ptr,
							p_region_element->region_element_type->array_type_members[m]->array_elements_region_element_type->type,
							p_array_info,
							p_region_element->region_element_type->array_type_members[m]->array_elements_count_cb_context) != 0) {
						msg_stderr("Error calling array_elements_count_funtion_ptr for pointer element %d in type %s\n",
								m, p_region_element->region_element_type->type->name);
						metac_array_info_delete(&p_array_info);
						return -EFAULT;
					}
				}
				/* calculate elements_size and elements_count */
				elements_bytesize = metac_type_byte_size(p_region_element->region_element_type->array_type_members[m]->array_elements_region_element_type->type);
				elements_count = metac_array_info_get_element_count(p_array_info);
				msg_stddbg("elements_byte_size: %d, elements_count: %d\n", (int)elements_bytesize, (int)elements_count);
				if (elements_count <= 0) {
					msg_stderr("Skipping array with 0 elements\n");
					metac_array_info_delete(&p_array_info);
					continue;
				}
				/* update the current region with additional size */
				if (p_region_element->region_element_type->array_type_members[m]->is_flexible) {
					assert(p_region->elements_count == 1);
					update_region2_flexible_bytesize(p_region, elements_bytesize * elements_count);
				}
				/*we have to create region and store it */
				res = create__region_for_array(
						&p_context->arrays_region_list,
						new_ptr,
						p_region_element->region_element_type->array_type_members[m]->array_elements_region_element_type,
						p_array_info,
						p_region,
						p_region_element,
						p_region_element->region_element_type->array_type_members[m],
						0,
						&_region);
				if (res == 1) {
					msg_stddbg("skipping\n");
					metac_array_info_delete(&p_array_info);
					continue;
				}
				if (res != 0) {
					msg_stderr("Error calling find_or_create_region\n");
					metac_array_info_delete(&p_array_info);
					return -EFAULT;
				}
				p_region_element->p_array[m].p_region2 = _region->p_region;

				/*create the new task for this region*/
				if (create_and_add_runtime_task(p_traversing_engine,
						p_task,
						_runtime_task_fn,
						_region->p_region) == NULL) {
					msg_stderr("Error calling create_and_add_runtime_task\n");
					return -EFAULT;
				}
			}
		}
		if (p_region_element->region_element_type->pointer_type_members_count > 0) {
			msg_stddbg("element %d pointers: %d items\n", e, p_region_element->region_element_type->pointer_type_members_count);
			for (m = 0; m < p_region_element->region_element_type->pointer_type_members_count; m++) {
				void * new_ptr;
				assert(p_region_element->region_element_type->pointer_type_members[m]->type->id == DW_TAG_pointer_type);
				msg_stddbg("pointer %s\n", p_region_element->region_element_type->pointer_type_members[m]->path_within_region_element);
				/* check preconditions */
				res = is_region_element_precondition_true(p_region_element, &p_region_element->region_element_type->pointer_type_members[m]->precondition);
				if (res < 0) {
					return -EFAULT;
				}
				if (res == 0) {
					msg_stddbg("skipping by precondition\n");
					continue;
				}
				if (p_region_element->region_element_type->pointer_type_members[m]->array_elements_region_element_type == NULL) {
					msg_stddbg("skipping because it's void* - don't know the size of the block it points\n");
					continue;
				}
				/* now read the pointer */
				new_ptr = *((void**)(p_region_element->ptr + p_region_element->region_element_type->pointer_type_members[m]->offset));
				if (new_ptr == NULL) {
					msg_stddbg("skipping because new ptr is null\n");
					continue;
				}
				/* create array info (array with sizes in all dimensions) */
				p_array_info = metac_array_info_create_from_type(p_region_element->region_element_type->pointer_type_members[m]->type);
				if (p_array_info == NULL) {
					msg_stderr("metac_array_info_create failed - exiting\n");
					return -EFAULT;
				}
				/* update array info if necessary */
				if (p_region_element->region_element_type->pointer_type_members[m]->array_elements_count_funtion_ptr != NULL &&
					p_region_element->region_element_type->pointer_type_members[m]->array_elements_count_funtion_ptr(
						0,
						p_region_element->ptr,
						p_region_element->region_element_type->type,
						new_ptr,
						p_region_element->region_element_type->pointer_type_members[m]->array_elements_region_element_type->type,
						p_array_info,
						p_region_element->region_element_type->pointer_type_members[m]->array_elements_count_cb_context) != 0) {
					msg_stderr("Error calling array_elements_count_funtion_ptr for pointer element %d in type %s\n",
							m, p_region_element->region_element_type->type->name);
					metac_array_info_delete(&p_array_info);
					return -EFAULT;
				}
				/* calculate byte_size using length */
				elements_bytesize = metac_type_byte_size(p_region_element->region_element_type->pointer_type_members[m]->array_elements_region_element_type->type);
				elements_count = metac_array_info_get_element_count(p_array_info);
				msg_stddbg("elements_byte_size: %d, elements_count: %d\n", (int)elements_bytesize, (int)elements_count);
				if (elements_count <= 0) {
					msg_stderr("Skipping new region with 0 elements\n");
					metac_array_info_delete(&p_array_info);
					continue;
				}
				/*we have to create region and store it (need create or find to support loops) */
				res = create__region_for_pointer(
						&p_context->pointers_region_list,
						new_ptr,
						p_region_element->region_element_type->pointer_type_members[m]->array_elements_region_element_type,
						p_array_info,
						p_region,
						p_region_element,
						p_region_element->region_element_type->pointer_type_members[m],
						0,
						&_region);
				if (res < 0) {
					msg_stderr("Error calling find_or_create_region\n");
					metac_array_info_delete(&p_array_info);
					return -EFAULT;
				}
				if (res == 1) {
					msg_stddbg("skipping\n");
					metac_array_info_delete(&p_array_info);
					continue;
				}
				p_region_element->p_pointer[m].p_region2 = _region->p_region;
				/* add task to handle this region fields properly */
				if (res == 0) {
					/*create the new task for this region*/
					if (create_and_add_runtime_task(p_traversing_engine,
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
		}
	}
	msg_stddbg("finished task\n");
	return 0;
}
/*****************************************************************************/
static int _compare_regions(const void *_a, const void *_b) {
	struct region2 *a = *((struct region2 **)_a);
	struct region2 *b = *((struct region2 **)_b);
	if (a->ptr < b->ptr)
		return -1;
	if (a->ptr == b->ptr) {
		if (a->byte_size > b->byte_size)return -1;
		if (a->byte_size == b->byte_size) {
			if (a->region_id < a->region_id)return -1;
			if (a->region_id > a->region_id)return 1;
			return 0;
		}
		return 1;
	}
	return 1;
}
/*****************************************************************************/
static struct metac_runtime_object * _context_to_runtime_object(
		struct runtime_context *p_runtime_context) {
	int i, j, k, l;
	struct metac_runtime_object * p_runtime_object;
	struct region2 * region2;
	struct _region * _region;
	metac_count_t pointers_regions_count = 0;
	metac_count_t arrays_regions_count = 0;

	cds_list_for_each_entry(_region, &p_runtime_context->pointers_region_list, list) {
		++pointers_regions_count;
	}
	cds_list_for_each_entry(_region, &p_runtime_context->arrays_region_list, list) {
		++arrays_regions_count;
	}

	p_runtime_object = create_runtime_object(p_runtime_context->p_precompiled_type);
	if (p_runtime_object == NULL) {
		msg_stderr("create_runtime_object failed\n");
		cds_list_for_each_entry(_region, &p_runtime_context->pointers_region_list, list) {
			delete_region2(&_region->p_region);
		}
		cds_list_for_each_entry(_region, &p_runtime_context->arrays_region_list, list) {
			delete_region2(&_region->p_region);
		}
		return NULL;
	}

	p_runtime_object->use_region2 = 1;
	p_runtime_object->regions_count = pointers_regions_count + arrays_regions_count;
	msg_stddbg("regions_count: %d\n", (int)p_runtime_object->regions_count);

	p_runtime_object->region = calloc(p_runtime_object->regions_count, sizeof(*(p_runtime_object->region)));
	if (p_runtime_object->region == NULL) {
		msg_stderr("Can't allocate memory for regions array in runtime_object\n");
		free_runtime_object(&p_runtime_object);
		cds_list_for_each_entry(_region, &p_runtime_context->pointers_region_list, list) {
			delete_region2(&_region->p_region);
		}
		cds_list_for_each_entry(_region, &p_runtime_context->arrays_region_list, list) {
			delete_region2(&_region->p_region);
		}
		return NULL;
	}

	i = 0;
	cds_list_for_each_entry(_region, &p_runtime_context->pointers_region_list, list) {
		_region->p_region->region_id = i;
		p_runtime_object->region2[i] = _region->p_region;
		++i;
	}
	cds_list_for_each_entry(_region, &p_runtime_context->arrays_region_list, list) {
		_region->p_region->region_id = i;
		p_runtime_object->region2[i] = _region->p_region;
		++i;
	}
	assert(p_runtime_object->regions_count == i);

	/*find unique regions*/
	qsort(p_runtime_object->region,
			/*pointers_regions_count*/p_runtime_object->regions_count,
			sizeof(*(p_runtime_object->region)),
			_compare_regions); /*TODO: change to hsort? - low prio so far*/

	i = 0;
	region2 = p_runtime_object->region2[i];
	assert(region2->is_allocated_region != 0);
	p_runtime_object->unique_regions_count = 1;
	msg_stddbg("i=%d, region_id=%d, ptr=%p, size=%x\n",
			i,
			p_runtime_object->region2[i]->region_id,
			p_runtime_object->region2[i]->ptr,
			p_runtime_object->region2[i]->byte_size);

	msg_stddbg("it's allocated. starting checking addresses of regions: %p\n", region2->ptr);
	for (i = 1; i < /*pointers_regions_count*/p_runtime_object->regions_count; i++) {
		msg_stddbg("i=%d, region_id=%d, ptr=%p, size=0x%x\n",
				i,
				p_runtime_object->region2[i]->region_id,
				p_runtime_object->region2[i]->ptr,
				p_runtime_object->region2[i]->byte_size);

		if (p_runtime_object->region2[i]->parent_info.member.p_region != NULL) {
			/*arrays are already pre-filled*/
			assert(metac_type_actual_type(p_runtime_object->region2[i]->parent_info.member.p_member->type)->id == DW_TAG_array_type);
			region2 = p_runtime_object->region2[i];
			continue;
		}

		while(1) {
			if (	p_runtime_object->region2[i]->ptr >= region2->ptr &&
					p_runtime_object->region2[i]->ptr < region2->ptr + region2->byte_size) { /*ptr within the previous*/

				if (p_runtime_object->region2[i] != region2 &&
					p_runtime_object->region2[i]->ptr == region2->ptr &&
					p_runtime_object->region2[i]->byte_size == region2->byte_size) {
					msg_stderr("Warning: ambiguity between 2 regions: %d and %d\n",
							region2->region_id,
							p_runtime_object->region2[i]->region_id);
					assert(0);
					msg_stddbg("repeating\n");
					if (region2->parent_info.member.p_region != NULL) {
						update_region2_non_allocated(p_runtime_object->region2[i],
							region2->parent_info.member.p_region,
							NULL,
							NULL,
							0);
					} else {
						++p_runtime_object->unique_regions_count;
					}
					region2 = p_runtime_object->region2[i];
					break;
				}

				if (p_runtime_object->region2[i]->ptr + p_runtime_object->region2[i]->byte_size > region2->ptr + region2->byte_size) {
					msg_stderr("Error: region %d %p %d is partially within previous %p %d. exiting\n",
							i,
							p_runtime_object->region2[i]->ptr,
							p_runtime_object->region2[i]->byte_size,
							region2->ptr,
							region2->byte_size);
					free_runtime_object(&p_runtime_object);
					return NULL;
				}
				update_region2_non_allocated(p_runtime_object->region2[i],
						region2,
						NULL,
						NULL,
						0);
				msg_stddbg("adjusting down\n");
				region2 = p_runtime_object->region2[i];
				break;
			} else {
				if (region2->parent_info.member.p_region != NULL) {
					msg_stddbg("adjusting up\n");
					region2 = region2->parent_info.member.p_region;
				} else {
					msg_stddbg("reached top level - it's allocated\n");
					region2 = p_runtime_object->region2[i];
					++p_runtime_object->unique_regions_count;
					break;
				}
			}
		}
	}

	msg_stddbg("unique_regions_count: %d\n", (int)p_runtime_object->unique_regions_count);
	p_runtime_object->unique_region2 = calloc(p_runtime_object->unique_regions_count, sizeof(*(p_runtime_object->unique_region2)));
	if (p_runtime_object->unique_region2 == NULL) {
		msg_stderr("Can't allocate memory for unique_region array in runtime_object\n");
		free_runtime_object(&p_runtime_object);
		return NULL;
	}

	/* get pointer regions back to the original sequence*/
//	i = 0;
//	cds_list_for_each_entry(_region, &p_runtime_context->pointers_region_list, list) {
//		p_runtime_object->region2[i] = _region->p_region;
//		++i;
//	}
//	assert(pointers_regions_count == i);
	i = 0;
	cds_list_for_each_entry(_region, &p_runtime_context->pointers_region_list, list) {
		p_runtime_object->region2[i] = _region->p_region;
		++i;
	}
	cds_list_for_each_entry(_region, &p_runtime_context->arrays_region_list, list) {
		p_runtime_object->region2[i] = _region->p_region;
		++i;
	}
	assert(p_runtime_object->regions_count == i);


	/* initialize not-unique's location and add unique regions*/
	j = 0; k = 0;
	for (i = 0; i < p_runtime_object->regions_count; ++i) {
		if (p_runtime_object->region2[i]->is_allocated_region == 0 ) { /*init not-unique region location */
			/*if we go in direction of region creation will appear in certain sequence so we won't need to do while in this algorithm*/
			assert(p_runtime_object->region2[i]->parent_info.member.p_region != NULL);
			assert(p_runtime_object->region2[i]->ptr >= p_runtime_object->region2[i]->parent_info.member.p_region->ptr);
			assert(p_runtime_object->region2[i]->ptr + p_runtime_object->region2[i]->byte_size  <=
					p_runtime_object->region2[i]->parent_info.member.p_region->ptr + p_runtime_object->region2[i]->parent_info.member.p_region->byte_size);

			/*TBD: calculate member exact member in case of the pointer! */
//			p_runtime_object->region2[i]->location.offset =
//					p_runtime_object->region2[i]->ptr - p_runtime_object->region2[i]->part_of_region->ptr;
//			if (p_runtime_object->region2[i]->part_of_region->part_of_region == NULL) {
//				p_runtime_object->region2[i]->location.region_idx = p_runtime_object->region2[i]->part_of_region->unique_region_id;
//			}else {
//				p_runtime_object->region2[i]->location.region_idx = p_runtime_object->region2[i]->part_of_region->location.region_idx;
//				p_runtime_object->region2[i]->location.offset += p_runtime_object->region2[i]->part_of_region->location.offset;
//			}
		} else {
			assert(j < p_runtime_object->unique_regions_count);
			p_runtime_object->unique_region2[j] = p_runtime_object->region2[i];
			/*init unique_region_id*/
			p_runtime_object->region2[i]->allocated_region_id = j;
			++j;
		}
	}

	return p_runtime_object;
}
/*****************************************************************************/
static void cleanup_runtime_context(struct runtime_context *p_runtime_context, int clean_regions) {
	struct traversing_engine_task * task, *_task;
	struct _region * _region, * __region;

	cleanup_traversing_engine(&p_runtime_context->traversing_engine);

	cds_list_for_each_entry_safe(task, _task, &p_runtime_context->executed_tasks_queue, list) {
		struct runtime_task  * p_task = cds_list_entry(task, struct runtime_task, task);
		cds_list_del(&task->list);
		delete_runtime_task(&p_task);
	}

	cds_list_for_each_entry_safe(_region, __region, &p_runtime_context->pointers_region_list, list) {
		if (clean_regions != 0) {
			delete_region2(&_region->p_region);
		}
		cds_list_del(&_region->list);
		free(_region);
	}

	cds_list_for_each_entry_safe(_region, __region, &p_runtime_context->arrays_region_list, list) {
		if (clean_regions != 0) {
			delete_region2(&_region->p_region);
		}
		cds_list_del(&_region->list);
		free(_region);
	}

}
/*****************************************************************************/
static struct metac_runtime_object * create_runtime_object_from_ptr(
		void * ptr,
		struct metac_precompiled_type * p_precompiled_type,
		metac_count_t elements_count) {
	int res;
	struct metac_runtime_object * p_runtime_object;
	struct runtime_context context;
	metac_array_info_t * p_array_info;
	struct _region * _region;

	context.p_precompiled_type = p_precompiled_type;
	CDS_INIT_LIST_HEAD(&context.pointers_region_list);
	CDS_INIT_LIST_HEAD(&context.arrays_region_list);
	CDS_INIT_LIST_HEAD(&context.executed_tasks_queue);

	/*use breadthfirst_engine*/
	if (init_traversing_engine(&context.traversing_engine)!=0){
		msg_stderr("create_breadthfirst_engine failed\n");
		return NULL;
	}

	p_array_info = metac_array_info_create_from_elements_count(elements_count);
	if (p_array_info == NULL){
		msg_stderr("metac_array_info_create_from_elements_count failed\n");
		cleanup_runtime_context(&context, 1);
		return NULL;
	}

	res = create__region_for_pointer(
			&context.pointers_region_list,
			ptr,
			p_precompiled_type->pointers.region_element_type[0],
			p_array_info,
			NULL,
			NULL,
			NULL,
			0,
			&_region);
	if (_region == NULL) {
		msg_stderr("find_or_create_region failed\n");
		cleanup_runtime_context(&context, 1);
		return NULL;
	}

	if (create_and_add_runtime_task(&context.traversing_engine,
			NULL,
			_runtime_task_fn,
			_region->p_region) == NULL) {
		msg_stderr("add_initial_precompile_task failed\n");
		cleanup_runtime_context(&context, 1);
		return NULL;
	}
	if (run_traversing_engine(&context.traversing_engine) != 0) {
		msg_stderr("run_breadthfirst_engine failed\n");
		cleanup_runtime_context(&context, 1);
		return NULL;
	}

	p_runtime_object = _context_to_runtime_object(&context);
	if (p_runtime_object == NULL) {
		cleanup_runtime_context(&context, 0);
		msg_stderr("_context_to_precompiled_type failed\n");
		return NULL;
	}

	cleanup_runtime_context(&context, 0);
	return p_runtime_object;
}
/*****************************************************************************/
int metac_visit2(
	void *ptr,
	metac_precompiled_type_t * precompiled_type,
	metac_count_t elements_count,
	struct metac_visitor2 * p_visitor) {
	int cb_res;
	int i, j, k;
	struct metac_runtime_object * p_runtime_object;

	p_runtime_object = create_runtime_object_from_ptr(ptr, precompiled_type, elements_count);
	if (p_runtime_object == NULL) {
		msg_stderr("Error while building runtime object\n");
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
				free_runtime_object(&p_runtime_object);
				return -EFAULT;
			}
		}
	}
	/* --- */
	free_runtime_object(&p_runtime_object);
	return 0;
}


#if 0
int metac_copy(
		metac_precompiled_type_t * p_ptype,
		void *ptr,
		metac_count_t elements_count,
		void **p_ptr);
int metac_is_equal(
		metac_precompiled_type_t * p_ptype,
		void *ptr0,
		void *ptr1,
		metac_count_t elements_count
		);
int metac_delete(
		metac_precompiled_type_t * p_ptype,
		void *ptr,
		metac_count_t elements_count
		);
#endif
