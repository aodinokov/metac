#ifndef INCLUDE_METAC_BACKEND_VALUE_H_
#define INCLUDE_METAC_BACKEND_VALUE_H_

#include "metac/reflect/entry_tag.h"
#include "metac/backend/iterator.h"
#include "metac/backend/hashmap.h"

// implemented in value.c, but we don't want to expose that in value.h
int metac_value_level_introduced_loop(metac_recursive_iterator_t * p_iterator);


/* the deeper we - the bigger the number. level 0 is iterator itself */
int metac_value_walker_hierarchy_level(metac_value_walker_hierarchy_t *p_hierarchy);
/* the higher the parent - the bigger the number. level 0 is current. max can be taken by metac_recursive_iterator_level. */
metac_value_t * metac_value_walker_hierarchy_value(metac_value_walker_hierarchy_t *p_hierarchy, int req_level);
int metac_value_event_handler_call(metac_value_event_handler_t handler, metac_recursive_iterator_t * p_iterator, metac_value_event_t * p_ev, void *p_context);

struct metac_memory_entry {
    metac_value_t * p_val;
    metac_size_t byte_size; /* actual size in bytes, taking into account flex arrays and etc */
    
    struct metac_memory_entry *p_parent; /* set by parent */

    metac_num_t children_count;
    struct metac_memory_entry ** pp_children;

    void * private_data; // use this in order to pass some extra to the child when walk, e.g. copy

    /* the next fields are used only with pointers parents */
    int loop_level; /*if > 0 - references the number of parent to pass to find children */
    metac_size_t offset_from_allocate_base;   /* 0 if pointer looks at the beginning of allocated area. otherwise if will point to the region whihch belongs to someone else */
};

struct metac_memory_entry * metac_new_memory_entry(metac_value_t * p_val, metac_size_t byte_size, metac_num_t children_count);
void metac_memory_entry_delete(struct metac_memory_entry * p_memory_entry);

/* get n'th parent (to work with loop_level) */
struct metac_memory_entry * metac_memory_entry_parent(struct metac_memory_entry * p_memory_entry, int n);

struct metac_memory_map {
    Hashmap* p_addr_ptr_val_to_memory_entry;   // only pointers write to this hash
    struct metac_memory_entry *p_top_entry;
};

struct metac_memory_map * metac_new_value_memory_map_ex(
    metac_value_t * p_val,
    metac_value_memory_map_mode_t * p_map_mode,
    metac_tag_map_t * p_tag_map);
void metac_value_memory_map_delete(struct metac_memory_map * p_map);

typedef struct metac_value_parameter_load metac_value_parameter_load_t;
typedef struct metac_value_func_load metac_value_func_load_t;

metac_value_parameter_load_t * metac_new_parameter_load(metac_num_t values_count);
metac_flag_t metac_parameter_load_delete(metac_value_parameter_load_t * p_param_load);
metac_value_t * metac_parameter_load_value(metac_value_parameter_load_t * p_param_load, metac_num_t id);
metac_value_t * metac_parameter_load_new_value(metac_value_parameter_load_t * p_param_load,
    metac_num_t id,
    metac_entry_t *p_entry,
    metac_num_t size);
metac_num_t metac_parameter_load_value_count(metac_value_parameter_load_t * p_param_load);
metac_value_t * metac_parameter_load_value(metac_value_parameter_load_t * p_param_load, metac_num_t id);
metac_value_t * metac_parameter_load_set_value(metac_value_parameter_load_t * p_param_load, metac_num_t id, metac_value_t * p_value);

metac_value_func_load_t * metac_new_func_load(metac_num_t values_count);
metac_flag_t metac_func_load_delete(metac_value_func_load_t * p_subprog_load);
metac_value_t * metac_func_load_param_new_value(metac_value_func_load_t * p_subprog_load,
    metac_num_t id,
    metac_entry_t *p_entry,
    metac_num_t size);
metac_value_t * metac_func_load_param_value(metac_value_func_load_t * p_subprog_load, metac_num_t id);
metac_value_t * metac_func_load_param_set_value(metac_value_func_load_t * p_subprog_load, metac_num_t id, metac_value_t * p_value);
metac_num_t metac_func_load_param_value_count(metac_value_func_load_t * p_subprog_load);
metac_value_t * metac_func_load_result_value(metac_value_func_load_t * p_subprog_load);
metac_flag_t metac_func_load_set_result_value(metac_value_func_load_t * p_subprog_load, metac_value_t * p_value);

#endif