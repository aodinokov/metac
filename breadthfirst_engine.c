/*
 * breadthfirst_engine.c
 *
 *  Created on: Mar 25, 2018
 *      Author: mralex
 */

#include "breadthfirst_engine.h"
#include "metac_debug.h"	/* msg_stderr, ...*/

#include <stdlib.h>			/* calloc, ... */
#include <errno.h>			/* ENOMEM etc */

#ifndef cds_list_for_each_safe
#define cds_list_for_each_safe(pos, p, head) \
	for (pos = (head)->next, p = pos->next; \
		pos != (head); \
		pos = p, p = pos->next)
#endif

int init_breadthfirst_engine(struct breadthfirst_engine* p_breadthfirst_engine) {
	if (p_breadthfirst_engine == NULL) {
		msg_stderr("invalid argument\n");
		return -EINVAL;
	}
	CDS_INIT_LIST_HEAD(&p_breadthfirst_engine->queue);
	return 0;
}

int add_breadthfirst_task(struct breadthfirst_engine* p_breadthfirst_engine, struct breadthfirst_engine_task * task) {
	if (p_breadthfirst_engine == NULL) {
		msg_stderr("Invalid argument: p_breadthfirst_engine\n");
		return -EINVAL;
	}
	if (task == NULL) {
		msg_stderr("Invalid argument: task\n");
		return -EINVAL;
	}
	cds_list_add_tail(&task->list, &p_breadthfirst_engine->queue);
	return 0;
}

int run_breadthfirst_engine(struct breadthfirst_engine* p_breadthfirst_engine) {
	int error_flag = 0;

	if (p_breadthfirst_engine == NULL) {
		msg_stderr("Invalid argument: p_breadthfirst_engine\n");
		return -EINVAL;
	}

	while (!cds_list_empty(&p_breadthfirst_engine->queue)) {
		struct breadthfirst_engine_task * task = cds_list_first_entry(&p_breadthfirst_engine->queue, struct breadthfirst_engine_task, list);
		cds_list_del(&task->list);
		if (task->fn != NULL) {
			int res = task->fn(p_breadthfirst_engine, task, error_flag);
			if (error_flag == 0 && res != 0) {
				msg_stddbg("task returned error - raising error_flag\n");
				error_flag = 1;
			}
		}
	}
	return error_flag==0?0:-EFAULT;
}

int cleanup_breadthfirst_engine(struct breadthfirst_engine *p_breadthfirst_engine) {
	if (p_breadthfirst_engine == NULL) {
		msg_stderr("Invalid argument: p_breadthfirst_engine\n");
		return -EINVAL;
	}

	while (!cds_list_empty(&p_breadthfirst_engine->queue)) {
		struct breadthfirst_engine_task * task = cds_list_first_entry(&p_breadthfirst_engine->queue, struct breadthfirst_engine_task, list);
		cds_list_del(&task->list);
		if (task->fn != NULL) {
			task->fn(p_breadthfirst_engine, task, 1);
		}
	}

	return 0;
}

