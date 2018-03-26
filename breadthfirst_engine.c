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

struct breadthfirst_engine* create_breadthfirst_engine(void) {
	struct breadthfirst_engine* p_breadthfirst_engine;

	p_breadthfirst_engine = calloc(1, sizeof(*(p_breadthfirst_engine)));
	if (p_breadthfirst_engine == NULL) {
		msg_stderr("Can't create p_breadthfirst_engine: no memory\n");
		return NULL;
	}

	CDS_INIT_LIST_HEAD(&p_breadthfirst_engine->queue);

	return p_breadthfirst_engine;
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

int run_breadthfirst_engine(struct breadthfirst_engine* p_breadthfirst_engine, breadthfirst_engine_task_fn_t override_fn) {
	struct cds_list_head *pos;

	if (p_breadthfirst_engine == NULL) {
		msg_stderr("Invalid argument: p_breadthfirst_engine\n");
		return -EINVAL;
	}

	cds_list_for_each(pos, &p_breadthfirst_engine->queue) {
		int res = 0;
		struct breadthfirst_engine_task * task = cds_list_entry(pos, struct breadthfirst_engine_task, list);
		if (override_fn == NULL) {
			if (task->fn != NULL) {
				res = task->fn(p_breadthfirst_engine, task);
			}
		}else {
			res = override_fn(p_breadthfirst_engine, task);
		}
		if (res != 0) {
			msg_stddbg("task returned error - aborting\n");
			return res;
		}

	}
	return 0;
}

int delete_breadthfirst_engine(struct breadthfirst_engine ** pp_breadthfirst_engine) {
	struct cds_list_head *pos, *_pos;
	struct breadthfirst_engine *p_breadthfirst_engine;

	if (pp_breadthfirst_engine == NULL) {
		msg_stderr("Can't delete breadthfirst_engine: invalid parameter\n");
		return -EINVAL;
	}

	p_breadthfirst_engine = *pp_breadthfirst_engine;
	if (p_breadthfirst_engine == NULL) {
		msg_stderr("Can't delete breadthfirst_engine: already deleted\n");
		return -EALREADY;
	}

	cds_list_for_each_safe(pos, _pos, &p_breadthfirst_engine->queue) {
		struct breadthfirst_engine_task * task = cds_list_entry(pos, struct breadthfirst_engine_task, list);
		cds_list_del(pos);
		if (task->destroy != NULL) {
			task->destroy(p_breadthfirst_engine, task);
		}
	}

	free(p_breadthfirst_engine);
	*pp_breadthfirst_engine = NULL;

	return 0;
}

