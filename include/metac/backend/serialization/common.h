#ifndef INCLUDE_METAC_BACKEND_SERIALIZATION_COMMON_H_
#define INCLUDE_METAC_BACKEND_SERIALIZATION_COMMON_H_

#include "metac/reflect.h"
#include "metac/backend/iterator.h"

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

    // in total memory allocated must be allocated_el_number * (allocated_prefix_size + allocated_el_sz + allocated_flexible_el_number * allocated_flexible_el_sz)
    // though for if flexible part isn't 0, allocated_el_number must be 1
    void* p_allocated;
    metac_size_t allocated_prefix_size; // for container_of scenario when pointer points to 1 object
    metac_size_t allocated_el_number; // if non zero - this task allocated memory for p_val;
    metac_size_t allocated_el_sz;
    metac_size_t allocated_flexible_el_number; // extra part
    metac_size_t allocated_flexible_el_sz; // extra part

    void * p_external;
}metac_deserialization_task_t;

metac_deserialization_task_t * metac_new_deserialization_task(metac_value_t * p_val, void * p_external);
void metac_deserialization_task_delete(metac_deserialization_task_t * p_pair);

// common helpers
metac_value_t * _metac_deserialization_task_value_extractor(void * p_in);
metac_deserialization_task_t * _metac_deserialization_task_find_task_with_allocation(metac_recursive_iterator_t * p_iterator);

int _metac_deserialization_task_dequeue_check_or_fail(metac_recursive_iterator_t * p_iterator, int cleanup_and_fail_state_id);
int _metac_deserialization_task_cleanup_and_fail(metac_recursive_iterator_t * p_iterator);

metac_name_t metac_value_name_per_protocol(metac_value_t* p_memb_val, char * protocol, metac_tag_map_t* p_tag_map);

#endif