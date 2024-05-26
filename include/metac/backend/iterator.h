#ifndef INCLUDE_METAC_BACKEND_ITERATOR_H_
#define INCLUDE_METAC_BACKEND_ITERATOR_H_
/** \page avoiding_stack_limitation Avoiding Stack Limitation
C/C++ runtimes have very limited stack size. It's important to solve this problem for metac, because
it may work with very complex and deep objects. we need iterator to process big recursive data without actually using stack.
instead we're going to use heap to keep intermittent data.

Each task may have subtasks that the parent task typically defines on start state.
Once all subtasks marked as done or failed the parent task again gets control.
The call sequence is similar to the walker, but we can control it in very granular manner.
The parent task may add more dependent tasks.

Each task including the main parent has:
1. p_in - pointer to the input data
2. state - when the task gets called for the first time it gets state METAC_R_ITER_start. once some part of work
is done, e.g. dependent tasks are created - we can update current state to the next one and call next in iterator.
it will start calling dependent tasks until all of them are not in done or failed state.
Dependent tasks may have their dependencies as well. When dependent task moved to done state it also can provide
it's result as p_out pointer.
3. p_out - result of the task if it's in the done-state. Parent task can take this info and use it.
parent task is responsible for de-allocation of memory of p_out. depending on the particular use-case the responsibility on
de-allocation of p_in may also be part of parent responsibility.
4. p_context - in order to store more than just state each task may allocate some memblock and store some data there.
the pointer to that data can be stored to p_context to be able to keep that data between switching of states.
the task iteself is responsible for deletion of this data.

Here is an example:
```c
int iteratory_test(){
    metac_entry_t *p_entry = NULL; // some var of type of pointer to struct x with members;
    // simple case
    // this is very like twisted in Python
    // continue->yield
    metac_recursive_iterator_t * p_iter = metac_new_recursive_iterator(p_entry);
    for (metac_entry_t * p = (metac_entry_t *)metac_recursive_iterator_next(p_iter);
            p != NULL; p = (metac_entry_t *)metac_recursive_iterator_next(p_iter)) {
        int state = metac_entry_iterator_get_state(p_iter);
        switch (p->kind) {
        case METAC_KND_variable:
            switch (state) { // state is int 0 - started, -2 - failed -1 finished
            case METAC_R_ITER_start: {
                //metac_recursive_iterator_set_context(p_iter, void*)
                metac_recursive_iterator_create_and_append_dep(p_iter, p->type);// ask to process type on the next level
                metac_recursive_iterator_create_and_append_dep(p_iter, somethig_else);// and proces something else
                metac_entry_iterator_set_state(p_iter, state + 1);
                continue;
            }
            case 1: {
                char buf[5];
                //char * type_name = metac_recursive_iterator_current_dep_list_out(p_iter, 0); // this has to be, but
                char * type_name = metac_recursive_iterator_delete_and_dequeue_dep(p_iter); // from beginning
                char * something_else = metac_recursive_iterator_delete_and_dequeue_dep(p_iter); // from beginning
                int len = snprintf(buf, sizeof(buf), "%s %s", type_name, p->name);
                char * res = calloc(len+1, sizeof(*res));
                if (res == NULL) {
                    metac_entry_iterator_fail(p_iter); // fail
                    continue;
                }
                snprintf(res, len+1, "%s %s", type_name, p->name);
                free(type_name);
                free(something_else);
                metac_entry_iterator_done(p_iter, res);
                continue;
            }
        }
        case METAC_KND_base_type: {
            char * res = strdup(p->name);
            if (res == NULL) {
                metac_recursive_iterator_fail(p_iter);
            }
            metac_recursive_iterator_done(p_iter, res);
            continue;
        }
        break;
    }
}
```
*/

#ifdef __cplusplus
extern "C" {
#endif

/** @brief special states */
enum {
    /* all positive numbers are to users */
    METAC_R_ITER_start = 0, /**< iterator alsways start from this */
    METAC_R_ITER_error = -1, /**< to report that the function that returns state is failed */
    METAC_R_ITER_done = -2, /**< successful finish */
    METAC_R_ITER_failed = -3, /**< not-successful finish */
};
typedef int metac_r_iter_state_t;

static inline int metac_r_iter_state_is_end(metac_r_iter_state_t state) {
    return state == METAC_R_ITER_done || state == METAC_R_ITER_failed;
}

typedef struct metac_recursive_iterator metac_recursive_iterator_t;

/** @brief create new iterator and provide task */
metac_recursive_iterator_t * metac_new_recursive_iterator(void * p_in);
/** @brief cleanup iterator */
void metac_recursive_iterator_free(metac_recursive_iterator_t * p_iterator);
/** @brief the main part of iterator - it's like a scheduler */
void * metac_recursive_iterator_next(metac_recursive_iterator_t * p_iterator);

/** @brief each entry can store and get access to its private data between states */
void * metac_recursive_iterator_get_context(metac_recursive_iterator_t * p_iterator);
/** @brief each entry can store some private data between states */
void * metac_recursive_iterator_set_context(metac_recursive_iterator_t * p_iterator, void * p_context);

/** @brief the deeper we - the bigger the number. level 0 is iterator itself */
int metac_recursive_iterator_level(metac_recursive_iterator_t * p_iterator);
/** @brief the higher the parent - the bigger the number. level 0 is current. max can be taken by metac_recursive_iterator_level. */
void * metac_recursive_iterator_get_in(metac_recursive_iterator_t * p_iterator, int req_level);
/** @brief get the current out - typically needed to take out from 0th level of iterator */
void * metac_recursive_iterator_get_out(metac_recursive_iterator_t * p_iterator, void ** pp_in, int * p_fail);

/** @brief get our state */
metac_r_iter_state_t metac_recursive_iterator_get_state(metac_recursive_iterator_t * p_iterator);
/** @brief change our state */
metac_recursive_iterator_t *metac_recursive_iterator_set_state(metac_recursive_iterator_t * p_iterator, metac_r_iter_state_t state);
/** @brief special case - mark our level as failed */
metac_recursive_iterator_t *metac_recursive_iterator_fail(metac_recursive_iterator_t * p_iterator);
/** @brief special case - mark our level as complete and set out */
metac_recursive_iterator_t *metac_recursive_iterator_done(metac_recursive_iterator_t * p_iterator, void * p_out);

/** @brief ensure that the queue is empty */
int metac_recursive_iterator_dep_queue_is_empty(metac_recursive_iterator_t * p_iterator);
/** @brief get queue size (size != 0 works longer than empty for long lists than is_empty)*/
int metac_recursive_iterator_dep_queue_size(metac_recursive_iterator_t * p_iterator);
/** @brief enqueue dependency and provide its in (push to tail) */
int metac_recursive_iterator_create_and_append_dep(metac_recursive_iterator_t * p_iterator, void * p_in);
/** @brief dequeue dependency and get its out, failstate and in (pop from front) */
void * metac_recursive_iterator_dequeue_and_delete_dep(metac_recursive_iterator_t * p_iterator, void ** pp_in, int * p_fail);
/** @brief get dependency out, failstate and in without dequeue */
void * metac_recursive_iterator_get_dep_out(metac_recursive_iterator_t * p_iterator, int i, void ** pp_in, int * p_fail);

#ifdef __cplusplus
}
#endif

#endif