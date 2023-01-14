/*
 * scheduler.h
 *
 *  Created on: Feb 19, 2020
 *      Author: mralex
 */

#ifndef SRC_SCHEDULER_H_
#define SRC_SCHEDULER_H_

#include <urcu/list.h>																	/* struct cds_list_head */

/*****************************************************************************/
struct scheduler;
struct scheduler_task;

/*****************************************************************************/
typedef int (*scheduler_task_fn_t)(struct scheduler *p_scheduler,
        struct scheduler_task *p_task, int error_flag);

struct scheduler {
    struct cds_list_head queue;
};

struct scheduler_task {
    struct cds_list_head list;

    scheduler_task_fn_t fn;
};
/*****************************************************************************/
int scheduler_init(struct scheduler *p_scheduler);
int scheduler_add_task_to_tail(struct scheduler *p_scheduler,
        struct scheduler_task *p_task);
int scheduler_add_task_to_front(struct scheduler *p_scheduler,
        struct scheduler_task *p_task);
int scheduler_run(struct scheduler *p_scheduler);
void scheduler_clean(struct scheduler *p_scheduler);

#endif /* SRC_SCHEDULER_H_ */
