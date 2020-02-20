/*
 * scheduler.c
 *
 *  Created on: Feb 19, 2020
 *      Author: mralex
 */

#include "scheduler.h"
#include "metac_debug.h"	/* msg_stderr, ...*/

#include <stdlib.h>			/* calloc, ... */
#include <errno.h>			/* ENOMEM etc */

int scheduler_init(
		struct scheduler *							p_scheduler) {

	if (p_scheduler == NULL) {
		msg_stderr("invalid argument\n");
		return -EINVAL;
	}

	CDS_INIT_LIST_HEAD(&p_scheduler->queue);

	return 0;
}

int scheduler_add_task_to_tail(
		struct scheduler *							p_scheduler,
		struct scheduler_task *						p_task) {

	if (p_scheduler == NULL) {
		msg_stderr("Invalid argument: p_scheduler\n");
		return -EINVAL;
	}

	if (p_task == NULL) {
		msg_stderr("Invalid argument: p_task\n");
		return -EINVAL;
	}

	cds_list_add_tail(&p_task->list, &p_scheduler->queue);

	return 0;
}

int scheduler_add_task_to_front(
		struct scheduler *							p_scheduler,
		struct scheduler_task *						p_task) {

	if (p_scheduler == NULL) {
		msg_stderr("Invalid argument: p_scheduler\n");
		return -EINVAL;
	}

	if (p_task == NULL) {
		msg_stderr("Invalid argument: p_task\n");
		return -EINVAL;
	}

	cds_list_add(&p_task->list, &p_scheduler->queue);

	return 0;
}

int scheduler_run(
		struct scheduler *							p_scheduler) {
	int error_flag = 0;

	if (p_scheduler == NULL) {
		msg_stderr("Invalid argument: p_scheduler\n");
		return -EINVAL;
	}

	while (!cds_list_empty(&p_scheduler->queue)) {

		struct scheduler_task * task = cds_list_first_entry(&p_scheduler->queue, struct scheduler_task, list);

		cds_list_del(&task->list);

		if (task->fn != NULL) {

			int res = task->fn(
					p_scheduler,
					task,
					error_flag);

			if (error_flag == 0 &&
				res != 0) {

				msg_stddbg("task returned error - raising error_flag\n");

				error_flag = 1;
			}
		}
	}
	return error_flag==0?0:-EFAULT;
}

void scheduler_clean(
		struct scheduler *							p_scheduler) {

	if (p_scheduler == NULL) {

		msg_stderr("Invalid argument: p_scheduler\n");
		return;
	}

	while (!cds_list_empty(&p_scheduler->queue)) {

		struct scheduler_task * task = cds_list_first_entry(&p_scheduler->queue, struct scheduler_task, list);

		cds_list_del(&task->list);

		if (task->fn != NULL) {

			task->fn(p_scheduler, task, 1);
		}
	}
}

