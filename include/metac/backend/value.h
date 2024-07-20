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

typedef struct metac_value_load_of_parameter {
    metac_num_t values_count;
    metac_value_t ** values;
} metac_value_load_of_parameter_t;

typedef struct metac_value_load_of_subprogram {
    metac_value_load_of_parameter_t * parameters;
    metac_value_t * result;
} metac_value_load_of_subprogram_t;

metac_value_load_of_parameter_t * metac_new_load_of_parameter(metac_num_t values_count);
metac_flag_t metac_load_of_parameter_delete(metac_value_load_of_parameter_t * p_param_load);
metac_value_t * metac_load_of_parameter_value(metac_value_load_of_parameter_t * p_param_load, metac_num_t id);
metac_value_t * metac_load_of_parameter_new_value(metac_value_load_of_parameter_t * p_param_load,
    metac_num_t id,
    metac_entry_t *p_entry,
    metac_num_t size);
metac_num_t metac_load_of_parameter_value_count(metac_value_load_of_parameter_t * p_param_load);
metac_value_t * metac_load_of_parameter_value(metac_value_load_of_parameter_t * p_param_load, metac_num_t id);

metac_value_load_of_subprogram_t * metac_new_load_of_subprogram(metac_num_t values_count);
metac_flag_t metac_load_of_subprogram_delete(metac_value_load_of_subprogram_t * p_subprog_load);

// to remove
/** @brief metac_value internal buffer to store unspecified params or va_list */
typedef struct metac_parameter_load {
    metac_num_t val_count;  // typically it must be 1, but it can be more for va_list or unspecified param
    metac_value_t *p_val[];
} metac_parameter_load_t;

/** @brief metac_value internal buffer to store arguments and result
 * we can't used va_list, because it's getting destroyed as soon as va_arg is called and some fn which uses va_list
 */
typedef struct metac_value_with_parameters_load {
    metac_flag_t with_res;      /**< if not 0 - we have only args */
    void * p_res_buf;           /**< buffer of result */

    metac_num_t parameters_count;
    metac_value_t *parameters[];
}metac_value_with_parameters_load_t;

// struct metac_value_with_parameters_load * metac_new_value_with_parameters_load(metac_num_t parameters_count);
// void metac_value_with_parameters_load_delete(struct metac_value_with_parameters_load * p_load);

metac_parameter_load_t * metac_new_parameter_load(metac_num_t val_count);
void metac_parameter_load_delete(metac_parameter_load_t * p_pload);


#endif