#ifndef INCLUDE_METAC_BACKEND_SERIALIZATION_H_
#define INCLUDE_METAC_BACKEND_SERIALIZATION_H_

#include "metac/reflect.h"

/*
 * there is a difference between how we serialize and deserialize in metac.
 * in case of serialization we're creating external objects, passing them between iterator cyclkes
 * in case of deserialization we always have a value to which we deserialize and external object we're using as a source
 * the iterator passes only 1 parameter as task p_in
 * iterator context is a memory that manages by the task itself - we can't pass that from outside (better not to mix this)
 * so in order to deserialize, we'll need a pair of pointes as a p_in - one of them is value, another is p_in

 hmm
 src/entry_tag.c:391:        metac_value_t *p_val = metac_value_walker_hierarchy_value(p_hierarchy, 0);
src/entry_tag.c:392:        metac_value_t *p_parent_val = metac_value_walker_hierarchy_value(p_hierarchy, 1);
src/entry_tag.c:446:        metac_value_t *p_val = metac_value_walker_hierarchy_value(p_hierarchy, 0);
src/entry_tag.c:499:        metac_value_t *p_ptr_val = metac_value_walker_hierarchy_value(p_hierarchy, 0);
src/entry_tag.c:594:                metac_value_t *p_val = metac_value_walker_hierarchy_value(p_hierarchy, 0);
src/entry_tag.c:595:                metac_value_t *p_parent_val = metac_value_walker_hierarchy_value(p_hierarchy, 1);
src/entry_tag.c:643:                metac_value_t *p_val = metac_value_walker_hierarchy_value(p_hierarchy, 0);
src/entry_tag.c:741:        metac_value_t *p_val = metac_value_walker_hierarchy_value(p_hierarchy, 0);
src/entry_tag.c:742:        metac_value_t *p_parent_val = metac_value_walker_hierarchy_value(p_hierarchy, 1);
src/entry_tag.c:798:    metac_value_t *p_val = metac_value_walker_hierarchy_value(p_hierarchy, 0);


struct metac_value_walker_hierarchy {
    metac_recursive_iterator_t * p_iterator;
};

int metac_value_walker_hierarchy_level(metac_value_walker_hierarchy_t *p_hierarchy) {
    _check_(p_hierarchy == NULL, -(EINVAL));
    _check_(p_hierarchy->p_iterator == NULL, -(EINVAL));
    return metac_recursive_iterator_level(p_hierarchy->p_iterator);
}

metac_value_t * metac_value_walker_hierarchy_value(metac_value_walker_hierarchy_t *p_hierarchy, int req_level) {
    _check_(p_hierarchy == NULL, NULL);
    _check_(p_hierarchy->p_iterator == NULL, NULL);
    return (metac_value_t *)metac_recursive_iterator_get_in(p_hierarchy->p_iterator, req_level);
}

seems like we always assumed that p_in is metac_value_t *
which isn't true..
 we need to add function which would take void* and conver to metac_value_t *

not so many places
src/serialization/cjson/value_from_cjson.c:275:                    if (metac_value_event_handler_call(p_tag->handler, NULL, &ev, p_tag->p_context) == 0 && ev.p_return_value != NULL) {
src/serialization/cjson/value_to_cjson.c:233:                                if (metac_value_event_handler_call(p_tag->handler, p_iter, &ev, p_tag->p_context) != 0) {
src/serialization/cjson/value_to_cjson.c:324:                                    if (metac_value_event_handler_call(p_tag->handler, p_iter, &ev, p_tag->p_context) == 0 && ev.p_return_value != NULL) {
src/serialization/cjson/value_to_cjson.c:412:                                    if (metac_value_event_handler_call(p_tag->handler, p_iter, &ev, p_tag->p_context) != 0) {

 */

typedef struct metac_deserialization_pair {
    metac_value_t * p_val;
    void * p_external;
}metac_deserialization_pair_t;

metac_deserialization_pair_t * metac_new_deserialization_pair(metac_value_t * p_val, void * p_external);
void metac_deserialization_pair_delete(metac_deserialization_pair_t * p_pair);

#endif