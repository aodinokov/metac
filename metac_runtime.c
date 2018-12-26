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

	struct region_type_element * region_type_element;
	void * ptr;
	metac_byte_size_t byte_size;
};

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


static struct runtime_task* create_and_add_runtime_task(
		struct breadthfirst_engine* p_breadthfirst_engine
		) {
	struct runtime_task* p_task;

//	msg_stddbg("(name_local = %s, given_name_local = %s, offset = %d, byte_size = %d)\n",
//			name_local, given_name_local,
//			(int)offset, (int)byte_size);

	/* allocate object */
	p_task = calloc(1, sizeof(*p_task));
	if (p_task == NULL) {
		msg_stderr("no memory\n");
		return NULL;
	}

	/*TBD*/
//	p_task->task.fn = fn;
//	p_task->task.destroy = destroy;


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

	return 0;
}
/*****************************************************************************/

struct metac_runtime_object * build_runtime_object(struct metac_precompiled_type * p_precompiled_type,
		void * ptr, metac_byte_size_t byte_size) {

	if (p_precompiled_type->region_type[0]->element[0]->byte_size > byte_size) {
		msg_stderr("byte_size parameter is too small for this precompiled type\n");
		return NULL;
	}



	return NULL;
}

