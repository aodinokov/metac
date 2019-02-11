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

int init_traversing_engine(struct traversing_engine* p_engine) {
	if (p_engine == NULL) {
		msg_stderr("invalid argument\n");
		return -EINVAL;
	}
	CDS_INIT_LIST_HEAD(&p_engine->queue);
	return 0;
}

int add_traversing_task_to_tail(struct traversing_engine* p_engine, struct traversing_engine_task * p_task) {
	if (p_engine == NULL) {
		msg_stderr("Invalid argument: p_engine\n");
		return -EINVAL;
	}
	if (p_task == NULL) {
		msg_stderr("Invalid argument: task\n");
		return -EINVAL;
	}
	cds_list_add_tail(&p_task->list, &p_engine->queue);
	return 0;
}

int add_traversing_task_to_front(struct traversing_engine* p_engine, struct traversing_engine_task * p_task) {
	if (p_engine == NULL) {
		msg_stderr("Invalid argument: p_engine\n");
		return -EINVAL;
	}
	if (p_task == NULL) {
		msg_stderr("Invalid argument: task\n");
		return -EINVAL;
	}
	cds_list_add(&p_task->list, &p_engine->queue);
	return 0;
}

int run_traversing_engine(struct traversing_engine* p_engine) {
	int error_flag = 0;

	if (p_engine == NULL) {
		msg_stderr("Invalid argument: p_engine\n");
		return -EINVAL;
	}

	while (!cds_list_empty(&p_engine->queue)) {
		struct traversing_engine_task * task = cds_list_first_entry(&p_engine->queue, struct traversing_engine_task, list);
		cds_list_del(&task->list);
		if (task->fn != NULL) {
			int res = task->fn(p_engine, task, error_flag);
			if (error_flag == 0 && res != 0) {
				msg_stddbg("task returned error - raising error_flag\n");
				error_flag = 1;
			}
		}
	}
	return error_flag==0?0:-EFAULT;
}

int cleanup_traversing_engine(struct traversing_engine *p_engine) {
	if (p_engine == NULL) {
		msg_stderr("Invalid argument: p_engine\n");
		return -EINVAL;
	}

	while (!cds_list_empty(&p_engine->queue)) {
		struct traversing_engine_task * task = cds_list_first_entry(&p_engine->queue, struct traversing_engine_task, list);
		cds_list_del(&task->list);
		if (task->fn != NULL) {
			task->fn(p_engine, task, 1);
		}
	}
	return 0;
}

