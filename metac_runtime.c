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

#include <stdlib.h>			/* calloc, ... */
#include <string.h>			/* strlen, strcpy */
#include <assert.h>			/* assert */
#include <errno.h>			/* ENOMEM etc */
#include <urcu/list.h>		/* I like struct cds_list_head :) */

/* separated this part from metac_precompiled_type */
/*****************************************************************************/
struct runtime_context {
	struct metac_runtime_object * runtime_object;
};

struct runtime_task {
	struct breadthfirst_engine_task task;

	struct runtime_task* parent_task;

	struct region_type * region_type;
	void * ptr;
	metac_byte_size_t byte_size;
};
/*****************************************************************************/

static struct region * create_region(
/*		struct region_type * region_type;
		void *ptr;
		metac_byte_size_t byte_size;
		struct region * part_of_region;*/
){
	/*TO start from this next time*/
}

/* ... smaller types first ...*/

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

//	for (i = 0; i < p_precompiled_type->region_types_count; i++) {
//		delete_region_type(&p_precompiled_type->region_type[i]);
//	}
//
//	if (p_precompiled_type->region_type != NULL) {
//		free(p_precompiled_type->region_type);
//		p_precompiled_type->region_type = NULL;
//	}

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
	p_runtime_object->is_tree = 1;
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

//	if (p_ptask->name_local) {
//		free(p_ptask->name_local);
//		p_ptask->name_local = NULL;
//	}
//	if (p_ptask->given_name_local) {
//		free(p_ptask->given_name_local);
//		p_ptask->given_name_local = NULL;
//	}

	free(p_task);
	*pp_task = NULL;

	return 0;
}


static struct runtime_task * create_and_add_runtime_task_4_region(
		struct breadthfirst_engine * p_breadthfirst_engine,
		struct runtime_task * parent_task,
		breadthfirst_engine_task_fn_t fn,
		breadthfirst_engine_task_destructor_t destroy,
		struct region_type * p_region_type,
		/*struct region * part_of_region - not NULL for elements of arrays only (probably we can also re-use it for pointers???*/
		void * ptr,
		metac_byte_size_t byte_size
		/*element_byte_size, int number_of_elemetns - this is to handle pointers with n elements*/
		) {
	struct runtime_task* p_task;

	msg_stddbg("p_region_type = %p, ptr = %p, byte_size = %d\n",
			p_region_type,
			ptr,
			(int)byte_size);

	/* allocate object */
	p_task = calloc(1, sizeof(*p_task));
	if (p_task == NULL) {
		msg_stderr("no memory\n");
		return NULL;
	}

	p_task->task.fn = fn;
	p_task->task.destroy = destroy;

	p_task->parent_task = parent_task;

	p_task->region_type = p_region_type;
	p_task->ptr = ptr;
	p_task->byte_size = byte_size;

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
		struct breadthfirst_engine_task * p_breadthfirst_engine_task){
	struct runtime_task * p_task = cds_list_entry(p_breadthfirst_engine_task, struct runtime_task, task);
	struct runtime_context * p_context = (struct runtime_context *)p_breadthfirst_engine->private_data;

	/*create region*/
	//p_task->

	return 0;
}
/*****************************************************************************/
static void cleanup_runtime_context(struct runtime_context *p_runtime_context) {
	/*TBD*/
}

/*****************************************************************************/
struct metac_runtime_object * build_runtime_object(
		struct metac_precompiled_type * p_precompiled_type,
		void * ptr,
		metac_byte_size_t byte_size
		/*element_byte_size, int number_of_elemetns - this is to handle pointers with n elements*/
		) {
	struct breadthfirst_engine* p_breadthfirst_engine;
	struct runtime_context context;

	if (p_precompiled_type->region_type[0]->element[0]->byte_size > byte_size) {
		msg_stderr("byte_size parameter is too small for this precompiled type\n");
		return NULL;
	}

	context.runtime_object = create_runtime_object(p_precompiled_type);
	if (context.runtime_object == NULL) {
		msg_stderr("create_runtime_object failed\n");
		return NULL;
	}
	//CDS_INIT_LIST_HEAD(&context.region_type_list);

	/*use breadthfirst_engine*/
	p_breadthfirst_engine = create_breadthfirst_engine();
	if (p_breadthfirst_engine == NULL){
		msg_stderr("create_breadthfirst_engine failed\n");
		delete_runtime_object(&context.runtime_object);
		return NULL;
	}
	p_breadthfirst_engine->private_data = &context;

	if (create_and_add_runtime_task_4_region(p_breadthfirst_engine,
			NULL,
			_runtime_task_fn,
			_runtime_task_destroy_fn,
			p_precompiled_type->region_type[0],
			ptr,
			byte_size) == NULL) {
		msg_stderr("add_initial_precompile_task failed\n");
		cleanup_runtime_context(&context);
		delete_breadthfirst_engine(&p_breadthfirst_engine);
		delete_runtime_object(&context.runtime_object);
		return NULL;
	}
	if (run_breadthfirst_engine(p_breadthfirst_engine, NULL) != 0) {
		msg_stderr("run_breadthfirst_engine failed\n");
		cleanup_runtime_context(&context);
		delete_breadthfirst_engine(&p_breadthfirst_engine);
		delete_runtime_object(&context.runtime_object);
		return NULL;
	}

	cleanup_runtime_context(&context);
	delete_breadthfirst_engine(&p_breadthfirst_engine);
	return context.runtime_object;
}

