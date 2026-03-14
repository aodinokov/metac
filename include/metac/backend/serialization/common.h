#ifndef INCLUDE_METAC_BACKEND_SERIALIZATION_COMMON_H_
#define INCLUDE_METAC_BACKEND_SERIALIZATION_COMMON_H_

#include "metac/reflect.h"

/*
 * there is a difference between how we serialize and deserialize in metac.
 * in case of serialization we're creating external objects, passing them between iterator cyckles
 * in case of deserialization we always have a value to which we deserialize and external object we're using as a source
 * the iterator passes only 1 parameter as task p_in
 * iterator context is a memory that manages by the task itself - we can't pass that from outside (better not to mix this)
 * so in order to deserialize, we'll need an object (task) of pointes as a p_in - one of them is value, another is p_in

 */

typedef struct metac_deserialization_task {
    metac_value_t * p_val;

    // in total memory allocated must be allocating_el_number * allocating_el_sz + allocating_flexible_sz
    metac_size_t allocating_el_number; // if non zero - this task allocated memory for p_val;
    metac_size_t allocating_el_sz;
    metac_size_t allocating_flexible_sz; // extra part 

    void * p_external;
}metac_deserialization_task_t;

metac_deserialization_task_t * metac_new_deserialization_task(metac_value_t * p_val, void * p_external);
void metac_deserialization_task_delete(metac_deserialization_task_t * p_pair);

#endif