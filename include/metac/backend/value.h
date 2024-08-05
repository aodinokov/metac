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

typedef struct metac_parameter_storage metac_parameter_storage_t;

metac_parameter_storage_t * metac_new_parameter_storage();
int metac_parameter_storage_copy(metac_parameter_storage_t * p_src_param_storage, metac_parameter_storage_t * p_dst_param_storage);
void metac_parameter_storage_delete(metac_parameter_storage_t * p_param_load);

metac_num_t metac_parameter_storage_size(metac_parameter_storage_t * p_param_load);

metac_value_t * metac_parameter_storage_new_param_value(metac_parameter_storage_t * p_param_storage, metac_num_t id);

int metac_parameter_storage_append_by_parameter_storage(metac_parameter_storage_t * p_param_storage,
    metac_entry_t *p_entry);
int metac_parameter_storage_append_by_buffer(metac_parameter_storage_t * p_param_storage,
    metac_entry_t *p_entry,
    metac_num_t size);

/*
some thougths
metac_parameter_storage_t * metac_new_parameter_storage();
void metac_parameter_storage_delete(metac_parameter_storage_t * p_param_load);

metac_parameter_storage_cleanup(metac_parameter_storage_t * p_param_load) // reset internal list
metac_parameter_storage_t * metac_parameter_storage_copy(metac_parameter_storage_t * p_param_load); // ??? why do we need this? - deep copy will use this to copy structure

metac_num_t metac_parameter_storage_size(metac_parameter_storage_t * p_param_load);
metac_value_t * metac_parameter_storage_item(metac_parameter_storage_t * p_param_load, metac_num_t id); -> new_item

metac_value_t * metac_parameter_storage_append_by_buffer(metac_parameter_storage_t * p_param_load,
    metac_entry_t *p_entry,
    metac_num_t size); -> this will create a value which we need to delete? probably we jsut need to split this
    put here flag and if ok - call metac_value_t * metac_parameter_storage_item(

metac_value_t * metac_parameter_storage_append_by_parameter_storage(metac_parameter_storage_t * p_param_load,
    metac_entry_t *p_entry);

Then
metac_value_t * metac_new_value_with_parameters(metac_tag_map_t * p_tag_map, metac_entry_t * p_entry, ...) ->
metac_value_t * metac_new_value(metac_entry_t * p_entry, (void*)p_param_load); +

metac_value_parameters_process(p_val, metac_tag_map_t * p_tag_map, ...);


and
metac_value_t * metac_new_value_printf(const char * format, ...);
metac_value_t * metac_new_value_vprintf(const char * format, va_list parameters);
metac_value_t * metac_new_value_vprintf_ex(const char * format, metac_entry_t * p_va_list_entry, va_list parameters);
>
metac_value_t * metac_new_value(metac_entry_t * p_entry, (void*)p_param_load); + (p_entry - va_list or ...)

metac_value_printf_parameters_process(p_val, const char * format, ...)


copy will look like

metac_value_t * src = metac_new_value(metac_entry_t * p_entry, (void*)p_param_load); +
metac_value_t * dst = metac_new_value(metac_entry_t * p_entry, (void*)p_param_load);

metac_value_parameters_process(src, metac_tag_map_t * p_tag_map, ...);

metac_copy_ex(dst, src);

dst will have everythong copied and arg modification won't affect this values...
metac_delete_ex(dst) will clean all allocated things

metac_parameter_storage_delete or clean (if we want to reuse) to cleanup params


we can remove then treating for addr ptr from value.c:
    if (metac_value_has_parameter_load(p_val)){
        // cleanup load
        metac_parameter_storage_t * p_in_para_load = metac_value_addr(p_val);
        if (p_in_para_load != NULL) {
            metac_parameter_storage_delete(p_in_para_load);
        }
    }

*/

#endif