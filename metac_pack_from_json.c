/*
 * metac_pack_from_json.c
 *
 *  Created on: Jan 24, 2019
 *      Author: mralex
 */

#define METAC_DEBUG_ENABLE
#include <errno.h>			/* ENOMEM etc */
#include <assert.h>			/* assert */
#include <stdint.h>			/* int8_t - int64_t*/
#include <complex.h>		/* complext */
#include <string.h>			/* strcmp*/
#include <endian.h>			/* __BYTE_ORDER at compile time */
#include <json/json.h>		/* json_... */
#include <urcu/list.h>		/* cds_list_entry, ... */

#include "metac_type.h"
#include "metac_internals.h"		/*definitions of internal objects*/
#include "metac_debug.h"			/* msg_stderr, ...*/
#include "breadthfirst_engine.h"	/*breadthfirst_engine module*/
/*****************************************************************************/
struct runtime_context {
	struct metac_runtime_object * runtime_object;
	json_object * p_json;

	struct cds_list_head region_list;
};

struct _region {
	struct cds_list_head list;
	struct region * p_region;

	json_object * p_json;
};

struct runtime_task {
	struct breadthfirst_engine_task task;
	struct runtime_task* parent_task;

	struct _region * p__region;
};
/*****************************************************************************/
static struct _region * create__region(
		void *ptr,
		metac_byte_size_t byte_size,
		struct region_element_type * region_element_type,
		metac_count_t elements_count,
		struct region * part_of_region) {
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
static struct _region * simply_create__region(
		struct runtime_context * p_runtime_context,
		void *ptr,
		metac_byte_size_t byte_size,
		struct region_element_type * region_element_type,
		metac_count_t elements_count,
		struct region * part_of_region) {
	/*check if region with the same addr already exists*/
	struct _region * _region = NULL;
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
	return _region;
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
		struct breadthfirst_engine * p_breadthfirst_engine,
		struct runtime_task * parent_task,
		breadthfirst_engine_task_fn_t fn,
		breadthfirst_engine_task_destructor_t destroy,
		struct _region * p__region) {
	struct runtime_task* p_task;
	struct region * p_region = p__region->p_region;

	if (p_region == NULL){
		msg_stderr("region is mandatory to create runtime_task\n");
		return NULL;
	}

	assert(p_region->elements_count > 0);

	/* allocate object */
	p_task = calloc(1, sizeof(*p_task));
	if (p_task == NULL) {
		msg_stderr("no memory\n");
		return NULL;
	}

	p_task->task.fn = fn;
	p_task->task.destroy = destroy;

	p_task->parent_task = parent_task;

	p_task->p__region = p__region;

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
struct _region_element_element_data_ {
	json_bool found;
	json_object * p_json;
};
/*****************************************************************************/
static int _runtime_task_fn(
		struct breadthfirst_engine * p_breadthfirst_engine,
		struct breadthfirst_engine_task * p_breadthfirst_engine_task) {
	int i;
	int e;
	struct runtime_task * p_task = cds_list_entry(p_breadthfirst_engine_task, struct runtime_task, task);
	struct runtime_context * p_context = (struct runtime_context *)p_breadthfirst_engine->private_data;
	struct _region_element_element_data_ * p_data;

	assert(p_task->p__region);
	assert(p_task->p__region->p_json);
	assert(p_task->p__region->p_region);
	assert(p_task->p__region->p_region->elements_count > 0);

	msg_stddbg("started task: region_element_type: %s, json %s\n",
			p_task->p__region->p_region->elements[0].region_element_type->type->name,
			json_object_to_json_string(p_task->p__region->p_json));

	p_data = calloc(p_task->p__region->p_region->elements[0].region_element_type->elements_count, sizeof(*p_data));
	if (p_data == NULL) {
		msg_stderr("can't allocate data for p_data\n");
		return -ENOMEM;
	}

	assert(p_task->p__region->p_region->elements_count == json_object_array_length(p_task->p__region->p_json));
	for (e = 0; e < p_task->p__region->p_region->elements_count; e++) {
		struct region_element * p_region_element = &p_task->p__region->p_region->elements[e];

		assert(p_task->p__region->p_region->elements[e].region_element_type == p_task->p__region->p_region->elements[0].region_element_type);

		/*reset p_data for new element*/
		memset(p_data, 0, p_region_element->region_element_type->elements_count * sizeof(*p_data));
		p_data[0].p_json = json_object_array_get_idx(p_task->p__region->p_json, e);

		/*TODO: found out how to understand what fields were not used */
		for (i = 0; i < p_region_element->region_element_type->elements_count; ++i) {
			if (p_region_element->region_element_type->element[i]->parent != NULL) {
				assert(p_data[p_region_element->region_element_type->element[i]->parent->id].p_json != NULL);
				if (json_object_get_type(p_data[p_region_element->region_element_type->element[i]->parent->id].p_json) != json_type_object) {
					msg_stderr("json isn't object: %s\n", json_object_to_json_string(p_data[p_region_element->region_element_type->element[i]->parent->id].p_json));
					free(p_data);
					return -ENOMEM;
				}
				if (strlen(p_region_element->region_element_type->element[i]->name_local) > 0) {
					p_data[p_region_element->region_element_type->element[i]->id].found = json_object_object_get_ex(
							p_data[p_region_element->region_element_type->element[i]->parent->id].p_json,
							p_region_element->region_element_type->element[i]->name_local,
							&p_data[p_region_element->region_element_type->element[i]->id].p_json);
				} else {
					p_data[p_region_element->region_element_type->element[i]->id].p_json =
							p_data[p_region_element->region_element_type->element[i]->parent->id].p_json;
				}
			}
		}

		/* initialize conditions */
		if (p_region_element->region_element_type->discriminators_count > 0) {
			for (i = 0; i < p_region_element->region_element_type->elements_count; ++i) {
				if (p_data[p_region_element->region_element_type->element[i]->id].p_json != NULL) {
					if (set_region_element_precondition(p_region_element, &p_region_element->region_element_type->element[i]->precondition) != 0){
						msg_stderr("Element %s can't be used with others: precondition validation failed\n",
								p_region_element->region_element_type->element[i]->path_within_region_element);
					}
				}
			}
		}
	}


	free(p_data);
	msg_stddbg("finished task\n");
	return 0;
}
/*****************************************************************************/
static struct metac_runtime_object * create_runtime_object_from_json(
		struct metac_precompiled_type * p_precompiled_type,
		json_object * p_json) {
	struct breadthfirst_engine* p_breadthfirst_engine;
	struct runtime_context context;
	struct region * region;
	struct _region * _region;
	json_object * p_json_unique_region;

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
		free_runtime_object(&context.runtime_object);
		return NULL;
	}
	p_breadthfirst_engine->private_data = &context;

	context.runtime_object->unique_regions_count = json_object_array_length(p_json);
	assert(context.runtime_object->unique_regions_count > 0);

	context.runtime_object->unique_region = calloc(
			context.runtime_object->unique_regions_count, sizeof(*context.runtime_object->unique_region));
	if (context.runtime_object->unique_region == NULL) {
		msg_stderr("create_runtime_object failed\n");
		cleanup_runtime_context(&context);
		delete_breadthfirst_engine(&p_breadthfirst_engine);
		free_runtime_object(&context.runtime_object);
		return NULL;
	}

	p_json_unique_region = json_object_array_get_idx(p_json, 0);
	if (json_object_get_type(p_json_unique_region) != json_type_array) {
		msg_stderr("p_json_unique_region isn't array\n");
		cleanup_runtime_context(&context);
		delete_breadthfirst_engine(&p_breadthfirst_engine);
		free_runtime_object(&context.runtime_object);
		return NULL;
	}

	_region = simply_create__region(&context,
			NULL, 0, /*right now we don't know ptr, size*/
			p_precompiled_type->region_element_type[0],
			json_object_array_length(p_json_unique_region),
			NULL);
	if (_region == NULL) {
		msg_stderr("create_region failed\n");
		cleanup_runtime_context(&context);
		delete_breadthfirst_engine(&p_breadthfirst_engine);
		free_runtime_object(&context.runtime_object);
		return NULL;
	}

	_region->p_json = p_json_unique_region;
	context.runtime_object->unique_region[0] = _region->p_region;

	/*add task for the first region*/
	if (create_and_add_runtime_task(p_breadthfirst_engine,
			NULL,
			_runtime_task_fn,
			_runtime_task_destroy_fn, _region) == NULL) {
		msg_stderr("add_initial_precompile_task failed\n");

		cds_list_for_each_entry(_region, &context.region_list, list) {
			delete_region(&_region->p_region);	/*region_array isn't initialized yet, runtime_object won't take care of regions*/
		}

		cleanup_runtime_context(&context);
		delete_breadthfirst_engine(&p_breadthfirst_engine);
		free_runtime_object(&context.runtime_object);
		return NULL;
	}
	if (run_breadthfirst_engine(p_breadthfirst_engine, NULL) != 0) {
		msg_stderr("run_breadthfirst_engine failed\n");

		cds_list_for_each_entry(_region, &context.region_list, list) {
			delete_region(&_region->p_region);
		}

		cleanup_runtime_context(&context);
		delete_breadthfirst_engine(&p_breadthfirst_engine);
		free_runtime_object(&context.runtime_object);
		return NULL;
	}

	cleanup_runtime_context(&context);
	delete_breadthfirst_engine(&p_breadthfirst_engine);

	free_runtime_object(&context.runtime_object);
	return context.runtime_object;
}
/*****************************************************************************/
int metac_pack_from_json(
		metac_precompiled_type_t * precompiled_type,
		json_object * p_json,
		void **p_ptr,
		metac_byte_size_t * p_size,
		metac_count_t * p_elements_count
		) {
	metac_count_t unique_regions_count;

	/*check consistency first*/
	if (json_object_get_type(p_json) != json_type_array) {
		msg_stderr("p_json isn't array\n");
		return -EINVAL;
	}
	unique_regions_count = json_object_array_length(p_json);
	if (unique_regions_count == 0) {
		msg_stderr("p_json has zero length\n");
		return -EINVAL;
	}
	create_runtime_object_from_json(precompiled_type, p_json);
	return 0;
}
