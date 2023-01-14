/*
 * traversing_engine.h
 *
 *  Created on: Mar 25, 2018
 *      Author: mralex
 */

#ifndef TRAVERSING_ENGINE_H_
#define TRAVERSING_ENGINE_H_

#include <urcu/list.h> /* struct cds_list_head */

struct traversing_engine_task;

struct traversing_engine {
    struct cds_list_head queue;
};

typedef int (*traversing_engine_task_fn_t)(struct traversing_engine *p_engine,
        struct traversing_engine_task *p_task, int error_flag);

struct traversing_engine_task {
    struct cds_list_head list;
    traversing_engine_task_fn_t fn;
};

int traversing_engine_init(struct traversing_engine *p_engine);
int add_traversing_task_to_tail(struct traversing_engine *p_engine,
        struct traversing_engine_task *p_task);
int add_traversing_task_to_front(struct traversing_engine *p_engine,
        struct traversing_engine_task *p_task);
int traversing_engine_run(struct traversing_engine *p_engine);
int traversing_engine_clean(struct traversing_engine *p_engine);

#endif /* TRAVERSING_ENGINE_H_ */
