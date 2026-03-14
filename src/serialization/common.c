#include "metac/backend/serialization/common.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>

metac_deserialization_task_t * metac_new_deserialization_task(metac_value_t * p_val, void * p_external) {
    metac_deserialization_task_t * p_task = calloc(1, sizeof(*p_task));
    if (p_task == NULL) {
        return NULL;
    }
    p_task->p_val = p_val;
    p_task->p_external = p_external;
    return p_task;
}

void metac_deserialization_task_delete(metac_deserialization_task_t * p_task) {
    if (p_task == NULL) {
        return;
    }
    free(p_task);
}

metac_value_t * _metac_deserialization_task_value_extractor(void * p_in) {
    if (p_in == NULL) {
        return NULL;
    }
    metac_deserialization_task_t * p_task = (metac_deserialization_task_t *)p_in;
    return p_task->p_val;
}

metac_deserialization_task_t * _metac_deserialization_task_find_task_with_allocation(metac_recursive_iterator_t * p_iterator) {
    int level = metac_recursive_iterator_level(p_iterator);
    if (level < 1) {
        return NULL;
    }
    for (int l = 0; l < level + 1; ++l) { /* if level = 1 there are 2 levels: 0 and 1 */
        metac_deserialization_task_t * p_task = metac_recursive_iterator_get_in(p_iterator, l);
        if (p_task->p_allocated != 0) {
            return p_task;
        }
    }
    return NULL;
}

int _metac_deserialization_task_cleanup_and_fail(metac_recursive_iterator_t * p_iterator) {
    while (metac_recursive_iterator_dep_queue_is_empty(p_iterator) == 0) {
        metac_deserialization_task_t * p_task = NULL;
        metac_recursive_iterator_dequeue_and_delete_dep(p_iterator, (void**)&p_task, NULL);
        if (p_task != NULL) {
            assert(p_task->p_allocated == NULL); // this memory has to be freed up already
            if (p_task->p_val != NULL ) {
                metac_value_delete(p_task->p_val);
            }
            metac_deserialization_task_delete(p_task);
        }
    }
    return -(EFAULT);
}    

int _metac_deserialization_task_dequeue_check_or_fail(metac_recursive_iterator_t * p_iterator, int cleanup_and_fail_state_id) {
    while (metac_recursive_iterator_dep_queue_is_empty(p_iterator) == 0) {
        metac_deserialization_task_t * p_task = NULL;
        metac_value_t * p_val_out = (metac_value_t *)metac_recursive_iterator_dequeue_and_delete_dep(p_iterator, (void**)&p_task, NULL);
        if (p_task != NULL) {
            if (p_task->p_val != NULL) {
                metac_value_delete(p_task->p_val);
            }
            metac_deserialization_task_delete(p_task);
        }
        if (p_val_out == NULL) {
            return cleanup_and_fail_state_id; // failure
        }
    }
    return 0; // success
}

