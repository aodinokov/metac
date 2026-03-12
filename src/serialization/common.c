#include "metac/backend/serialization/common.h"

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
