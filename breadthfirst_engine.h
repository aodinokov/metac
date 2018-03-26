/*
 * breadthfirst_engine.h
 *
 *  Created on: Mar 25, 2018
 *      Author: mralex
 */

#ifndef BREADTHFIRST_ENGINE_H_
#define BREADTHFIRST_ENGINE_H_

#include <urcu/list.h> /* struct cds_list_head */

struct breadthfirst_engine_task;

struct breadthfirst_engine {
	struct cds_list_head queue;

	void * private_data;
};

typedef int (*breadthfirst_engine_task_fn_t)(
	struct breadthfirst_engine * p_breadthfirst_engine,
	struct breadthfirst_engine_task * p_breadthfirst_engine_task);

typedef int (*breadthfirst_engine_task_destructor_t)(
	struct breadthfirst_engine * p_breadthfirst_engine,
	struct breadthfirst_engine_task * p_breadthfirst_engine_task);


struct breadthfirst_engine_task {
	struct cds_list_head list;

	breadthfirst_engine_task_fn_t fn;
	breadthfirst_engine_task_destructor_t destroy;

	void * private_data;
};

struct breadthfirst_engine* create_breadthfirst_engine(void);
int add_breadthfirst_task(struct breadthfirst_engine* p_breadthfirst_engine, struct breadthfirst_engine_task * task);
int run_breadthfirst_engine(struct breadthfirst_engine* p_breadthfirst_engine, breadthfirst_engine_task_fn_t override_fn);
int delete_breadthfirst_engine(struct breadthfirst_engine ** pp_breadthfirst_engine);

#endif /* BREADTHFIRST_ENGINE_H_ */
