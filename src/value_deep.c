#include "metac/backend/entry.h"
#include "metac/backend/iterator.h"
#include "metac/backend/value.h"
#include "metac/reflect.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h> /* qsort */
#include <string.h> /* mempy... */

/** \page deep_function_implementation Deep Functions implementation

Here are the initial thoughts on 'deep' functions: compare, delete and copy:
The simplest looks compare, let's start from it.
The easiest approach would be to - to convert to string and to compare strings.
There are several caveats:
string returns error if meets loop (actually we could return / *loop number* /.
also string doesn't support containerof scenario, - it returns NULL on that as well, because it's not compatible with c-init format(need to experiment)
the last thing - void* pointers can be compared only by value - this is all we can do if typecast handler returns NULL.
This is compatible with the string approach, but this is not the best thing - we didn't want to compare pointers.
The idea is to compare data and structure.
We can probably add a mode to fail if we have a pointer which wasn't casted from void, or which is only declared, but we don't have data about that.
(in that case we treat it as void * also).
So, we still could use the 'string' approach if we add all those things as string 'modes'. Potentially in future string could benefit from that.
The last caveat is - string approach will require more memory. So we're going to create a function that would compare in place.

during comparison we won't analyze the structure of data: e.g.


       A
      / \ 
     /   \ 
    B     C
     \   /
      \ /
       D

     Pic 1.
is going to be equal to

       A
      / \ 
     /   \ 
    B     C
    |     |
    |     |
    D1    D2

     Pic 2.
in case D1 is equal to D2.


This may be a subject for later additional research: e.g. metac_value_free shouldn't delete D twice. That means
that in order to implement metac_value_free we'll need to track memory blocks allocated. One more question would be
if we can allow 'weak pointers' - pointers that point to the field within a block of memory. We must not try to delete those memory blocks either.
We may:
1. we can make a flag in the handler to allow walkers to ignore such pointers (this is ok, we typically know in advance if the pointer is weak).
2. use containerof scenario to detect the beginning of the allocated memory (this is not always possible, or at least there are some cases where it will difficult)
3. we can track memory blocks (anyway for metac_value_copy we will need to know all memory blocks sizes), so we may try to detect weak pointers.


The same is applicable for copy function, but it can tolerate weak pointers, because it's not doing anything with the original struct.
when copying it can create a strong pointer instead of weak and generate a copy of part of mem block.
This behavior of 'changing' the structure and making Pic 2 out of Pic 1 structure isn't good, but this may be a known behavior flaw.


in general we must decide on if we're going to allow:
1. only tree structures? (pic 2). In that case the simple cycle check implemented in value_string can be enough.
2. DAGs (pic 1.) - we'll need to have a hash table of pointers to identify if
3. DAGs with weak pointers pointing to the part of the allocated memory (the most difficult case) - some extra mechanism (maybe something in addition to hash table)


so, with weak pointers we can deal by collecting all of such values,
We can sort them by address and if we have size, we can find what pointers point to inner and what not (sweep line algorithm. O(n*log(n)))
the pointers which point to the inner blocks are weak.


it would be nice to re-use the same handler pointers we used for strings.
we must (or shouldn't) not ask the handler twice about the same object during the same operation.
e.g. to make a copy of an object - we need to know the size of the object in advance.
that's ok of all types except for flexible arrays. and we don't know on struct level if we have a flexible array.
The generic way is to go and build the values hierarchy, in parallel, we're tracking all pointers and their corresponding arrays they're pointing to.
. We also need to calculate the size of each value. Ideally we must get len on the base_type/enum level and calculate length on each level. That will
allow us to solve the issue with flexible arrays. we may also potentially detect overlap for cases when struct with flexible array gives us bigger
len and we have that struct in an array with count >1. though it may be intended behavior (??? for whom???).


so building that hierarchy with extra data (each child provides this info: value, actual len, ... pointer to list of children).
+ we need to track pointers and that will be additional data. part.
value_map {
   hashmap [address] struct {pointer_value, resulting_array_value and size of the block (if any), weak flag (calculated later)};
   value_data, size, children ->...
}
For delete we will just find outer blocks and delete all of them and clean up all the data.
For a copy we'll need to make a copy of all blocks with the same size, and using a map we need to replicate the same decision based on the result of handlers.
(new_value_from_existing_with_different_addr?? -> new_value(get_entry(old), new address))




we may build this struct as a separate internal function and use this data to other functions, e.g. copy, delete.


Summary:
1. there can be different traits of behavior of each function. we may want to support them:
1.1. compare_modes: only values, don't pay attention to structure, compare with structure awareness.
1.2. delete - we do really want to be aware of structure always
1.3. copy_modes: strict (aware of structure and create full copy), only data: treat all pointers as strong. this
    will create a Tree instead of DAG with weak pointers. In some cases this may be a desired behavior. E.g. we
    may want to copy DAG with weak pointers into the tree and delete after that. delete will always be struct-aware.
    compare op will return true or false based on the mode.


For non-structure aware traits we don't need to build a hashmap.


for building map we may also have 3 types of how we want to treat pointers:
1. fail on void* and declarations types (if handler didn't fix that)
2. fallback (we won't be able to copy those, but we can use the same values, call delete and compare values of the pointer itself)
3. ignore those pointers (don't do anything with them).


We want to deliver this original function that builds that data as a part of the API.
it may be used for further implementations like json serialization.

There more considerations if we now speak about values with parameters
(list of args and result of function).
we may want to support it , but in order to do so we need to make sure that 
value_memory_map supports that.
it seems like we always create struct metac_memory_entry * p_memory_entry = metac_new_memory_entry(p, sz, 0);
even for base_types.
so our memory entry will be with value of type with parameters.
we'll need to filter those things out. we also can provide sz as 0. or we need to create a flag to
pseudo entry
*/

/* get n'th parent (to work with loop_level) */
struct metac_memory_entry * metac_memory_entry_parent(struct metac_memory_entry * p_memory_entry, int n) {
    if (n < 0) {
        return NULL;
    }
    while (n > 0) {
        if (p_memory_entry->p_parent == NULL) {
            return NULL;
        }
        p_memory_entry = p_memory_entry->p_parent;
        --n;
    }
    return p_memory_entry;
}

struct metac_memory_entry * metac_new_memory_entry(metac_value_t * p_val, metac_size_t byte_size, metac_num_t children_count) {
    struct metac_memory_entry * p_memory_entry = calloc(1, sizeof(*p_memory_entry));
    if (p_memory_entry == NULL) {
        return NULL;
    }
    if (children_count > 0) {
        p_memory_entry->pp_children = calloc(children_count, sizeof(*p_memory_entry->pp_children));
        if (p_memory_entry->pp_children == NULL) {
            free(p_memory_entry);
            return NULL;
        }

        p_memory_entry->children_count = children_count;
    }

    p_memory_entry->p_val = p_val;
    p_memory_entry->byte_size = byte_size;

    return p_memory_entry;
}

void metac_memory_entry_delete(struct metac_memory_entry * p_memory_entry) {
    if (p_memory_entry == NULL) {
        return;
    }
    metac_recursive_iterator_t * p_iter = metac_new_recursive_iterator(p_memory_entry);
    for (struct metac_memory_entry * p = (struct metac_memory_entry *)metac_recursive_iterator_next(p_iter); p != NULL;
        p = (struct metac_memory_entry *)metac_recursive_iterator_next(p_iter)) {
        int state = metac_recursive_iterator_get_state(p_iter);
        switch (state) {
            case METAC_R_ITER_start: {
                if (p->p_val != NULL) {
                    metac_value_delete(p->p_val);
                }
                if (p->children_count == 0) {
                    assert(p->pp_children == NULL);
                    free(p);
                    metac_recursive_iterator_done(p_iter, NULL);
                    continue;
                }
                for (metac_num_t i = 0; i < p->children_count; ++i) {
                    if (p->pp_children[i] != NULL) {
                        metac_recursive_iterator_create_and_append_dep(p_iter, p->pp_children[i]);
                    }
                }
                metac_recursive_iterator_set_state(p_iter, 1);
                continue;
            }
            case 1: {
                while(metac_recursive_iterator_dep_queue_is_empty(p_iter) == 0) {
                    metac_recursive_iterator_dequeue_and_delete_dep(p_iter, NULL, NULL);
                }
                free(p->pp_children);
                free(p);
                metac_recursive_iterator_done(p_iter, NULL);
                continue;
            }
        }
    }
    metac_recursive_iterator_free(p_iter);
}

static int addr_ptr_key_hash(void* key) {
    return hashmapHash(&key, sizeof(void*));
}
static bool addr_ptr_key_equals(void* keyA, void* keyB) {
    return keyA == keyB;
}

void metac_value_memory_map_delete(struct metac_memory_map * p_map) {
    if (p_map->p_addr_ptr_val_to_memory_entry != NULL) {
        hashmapFree(p_map->p_addr_ptr_val_to_memory_entry);
    }
    if (p_map->p_top_entry != NULL) {
        metac_memory_entry_delete(p_map->p_top_entry);
    }
    free(p_map);
}

struct _store_hash_map_entry_context {
    metac_num_t len;
    metac_num_t current;
    void ** p_keys;
};

/* callback for hashmapForEach */
static bool _store_hash_map_entry(void* key, void* value, void* context) {
    assert(context != NULL);
    struct _store_hash_map_entry_context * p_context = (struct _store_hash_map_entry_context *)context;
    assert(p_context->current < p_context->len);
    if (!(p_context->current < p_context->len)) {
        return false;
    }

    p_context->p_keys[p_context->current] = key;
    ++p_context->current;
    return true;
}

/* callback for qsort */
static  int _sort_addr_(const void *a, const void *b) {
    void* key_a = *(void**)a;
    void* key_b = *(void**)b;
    if (key_a < key_b) return -1;
    if (key_a == key_b) return 0;
    return 1;
}

static int _identify_allocatable_entries(struct metac_memory_map * p_map) {
    if (p_map->p_addr_ptr_val_to_memory_entry == NULL) {
        return 0;
    }

    struct _store_hash_map_entry_context context = {.len = hashmapSize(p_map->p_addr_ptr_val_to_memory_entry), .current = 0, .p_keys = NULL};
    if (context.len == 0) {
        return 0;
    }

    context.p_keys = calloc(context.len, sizeof(*context.p_keys));
    if (context.p_keys == NULL) {
        return -(ENOMEM);
    }

    hashmapForEach(p_map->p_addr_ptr_val_to_memory_entry, _store_hash_map_entry, &context);
    assert(context.current == context.len);

    /* sort keys for sweep line algorithm */
    qsort(context.p_keys, context.len, sizeof(*context.p_keys), _sort_addr_);

    /* sweep line*/
    void *p_main = NULL;
    struct metac_memory_entry * p_main_entry = NULL;
    for (metac_size_t i = 0; i < context.len; ++i) {
        void *p_current = context.p_keys[i];
        struct metac_memory_entry * p_current_entry = hashmapGet(p_map->p_addr_ptr_val_to_memory_entry, p_current);
        assert(p_current_entry != NULL);

        if (p_current_entry->byte_size <= 0) {
            // artificial elements, like param/function loads
            continue;
        }

        if (p_main == NULL) {
            p_main = p_current;
            p_main_entry = p_current_entry;
            continue;
        }
        if (p_main + p_main_entry->byte_size <= p_current) {
            p_main = p_current;
            p_main_entry = p_current_entry;
            continue;
        }
        if (p_main + p_main_entry->byte_size < p_current + p_current_entry->byte_size) {
            metac_size_t delta = p_current + p_current_entry->byte_size - (p_main + p_main_entry->byte_size);
            printf("WARNING: partial overlap %p:%"METAC_SIZE_PRIx" with %p:%"METAC_SIZE_PRIx
                ". should we extend the first size by %"METAC_SIZE_PRIx"\n",
                p_main, p_main_entry->byte_size, p_current, p_current_entry->byte_size, 
                delta);
        }
        p_current_entry->offset_from_allocate_base = p_current - p_main;
    }
    free(context.p_keys);
    return 0;
}

/* default mode - failsafe */
static metac_value_memory_map_mode_t const _default_value_memory_map_mode = {
    .memory_block_mode = METAC_MMODE_dag,
    .flex_array_mode = METAC_FLXARR_fail,
    .union_mode = METAC_UNION_fail,
    .unknown_ptr_mode = METAC_UPTR_fail,
};

struct metac_memory_map * metac_new_value_memory_map_ex(
    metac_value_t * p_val, 
    metac_value_memory_map_mode_t * p_map_mode,
    metac_tag_map_t * p_tag_map) {
    if (p_map_mode == NULL) {
        p_map_mode = (metac_value_memory_map_mode_t *)&_default_value_memory_map_mode;
    }
    p_val = metac_new_value_from_value(p_val);
    if (p_val == NULL) {
        return NULL;
    }

    struct metac_memory_map *p_map = calloc(1, sizeof(*p_map));
    if (p_map == NULL) {
        return NULL;
    }
    if (p_map_mode->memory_block_mode == METAC_MMODE_dag) {
        p_map->p_addr_ptr_val_to_memory_entry = hashmapCreate(1024, addr_ptr_key_hash, addr_ptr_key_equals);
        if (p_map->p_addr_ptr_val_to_memory_entry == NULL) {
            free(p_map);
            return NULL;
        }
    }

    metac_recursive_iterator_t * p_iter = metac_new_recursive_iterator(p_val);
    for (metac_value_t * p = (metac_value_t *)metac_recursive_iterator_next(p_iter); p != NULL;
        p = (metac_value_t *)metac_recursive_iterator_next(p_iter)) {
        int state = metac_recursive_iterator_get_state(p_iter);

        metac_kind_t final_kind;
        if (metac_value_kind(p) != METAC_KND_func_parameter || metac_value_has_parameter_load(p) == 0) {
            final_kind = metac_value_final_kind(p, NULL);
        } else {
            final_kind = METAC_KND_func_parameter;    // unspecified and va_arg;
        }

        switch(final_kind) {
        case METAC_KND_enumeration_type:
        case METAC_KND_base_type: {
            metac_size_t sz = 0;
            if (metac_entry_byte_size(metac_value_entry(p), &sz) != 0) {
                metac_recursive_iterator_fail(p_iter);
                continue;       
            }
            struct metac_memory_entry * p_memory_entry = metac_new_memory_entry(p, sz, 0);
            if (p_memory_entry == NULL) {
                metac_recursive_iterator_fail(p_iter);
                continue;                 
            }
            metac_recursive_iterator_done(p_iter, p_memory_entry);
            continue;
        }
        //case METAC_KND_class_type: 
        case METAC_KND_union_type:
        case METAC_KND_struct_type:
        case METAC_KND_array_type: {
            switch(state) {
                case METAC_R_ITER_start: {
                    metac_size_t sz = 0; // this is initial size. it may be updated if we have flex array on the next state
                    metac_num_t children_count = 0;
                    metac_flag_t failure = 0;
                    if (final_kind != METAC_KND_array_type) {
                        assert(metac_value_has_members(p) != 0);

                        if (metac_entry_byte_size(metac_value_entry(p), &sz) != 0) {
                            metac_recursive_iterator_fail(p_iter);
                            continue;       
                        }

                        metac_num_t mcount = metac_value_member_count(p);
                        if (final_kind == METAC_KND_union_type) {
                            if (mcount > 0) {
                                if (p_tag_map != NULL) {
                                    metac_value_event_t ev = {.type = METAC_RQVST_union_member, .p_return_value = NULL};
                                    metac_entry_tag_t * p_tag = metac_tag_map_tag(p_tag_map, metac_value_entry(p));
                                    if (p_tag != NULL && p_tag->handler) {
                                        if (metac_value_event_handler_call(p_tag->handler, p_iter, &ev, p_tag->p_context) != 0) {
                                            metac_recursive_iterator_fail(p_iter);
                                            continue;
                                        }
                                        if (ev.p_return_value != NULL) {
                                            metac_recursive_iterator_create_and_append_dep(p_iter, ev.p_return_value);
                                            children_count = 1;
                                        }
                                    }
                                }
                                if (children_count == 0 && p_map_mode->union_mode == METAC_UNION_fail) {
                                    failure = 1;
                                    break;
                                }
                            }
                        } else { /* structs, classes must go over all members */
                            for (metac_num_t i = 0; i < mcount; ++i) {
                                metac_value_t * p_memb_val = metac_new_value_by_member_id(p, i);
                                if (p_memb_val == NULL) {
                                    failure = 1;
                                    break;
                                }
                                metac_recursive_iterator_create_and_append_dep(p_iter, p_memb_val);
                                ++children_count;
                            }
                        }
                        if (failure != 0) {
                            metac_recursive_iterator_set_state(p_iter, 2);  // failure cleanup
                            continue;  
                        }

                        struct metac_memory_entry * p_memory_entry = metac_new_memory_entry(p, sz, children_count);
                        if (p_memory_entry == NULL) {
                            metac_recursive_iterator_set_state(p_iter, 2);  // failure cleanup
                            continue;                 
                        }

                        metac_recursive_iterator_set_context(p_iter, p_memory_entry);
                        metac_recursive_iterator_set_state(p_iter, 1);
                        continue;
                    } else { // final_kind  == METAC_KND_array_type
                        assert(metac_value_has_elements(p) != 0);

                        metac_value_t * p_local = p;
                        metac_value_t * p_non_flexible = NULL;
                        if (metac_value_element_count_flexible(p) != 0) {
                            if (p_tag_map != NULL) {
                                metac_value_event_t ev = {.type = METAC_RQVST_flex_array_count, .p_return_value = NULL};
                                metac_entry_tag_t * p_tag = metac_tag_map_tag(p_tag_map, metac_value_entry(p));
                                if (p_tag != NULL && p_tag->handler) {
                                    if (metac_value_event_handler_call(p_tag->handler, p_iter, &ev, p_tag->p_context) != 0) {
                                        metac_recursive_iterator_set_state(p_iter, 2);  // failure cleanup
                                        continue;
                                    }
                                    p_non_flexible = ev.p_return_value;
                                }
                            }
                            if (p_non_flexible == NULL) { /* this is a way to skip that item */
                                if (p_map_mode->flex_array_mode == METAC_FLXARR_fail) {
                                    metac_recursive_iterator_fail(p_iter);
                                    continue; 
                                } 
                                p_non_flexible = metac_new_element_count_value(p, 0); /* generate {} - basically this is ignore - safe default */
                            }
                            if (p_non_flexible == NULL) {
                                metac_recursive_iterator_fail(p_iter);
                                continue; 
                            }
                            p_local = p_non_flexible;
                        }

                        metac_num_t mcount = metac_value_element_count(p_local);
                        for (metac_num_t i = 0; i < mcount; ++i) {
                            metac_value_t * p_el_val = metac_new_value_by_element_id(p_local, i);
                            if (p_el_val == NULL) {
                                failure = 1;
                                break;
                            }
                            metac_recursive_iterator_create_and_append_dep(p_iter, p_el_val);
                            ++children_count;
                        }

                        if (children_count > 0 && metac_entry_byte_size(metac_value_entry(p_local), &sz) != 0) {
                            failure = 1;
                        }

                        if (p_non_flexible) {
                            metac_value_delete(p_non_flexible);
                            p_local = p;
                        }
                        if (failure != 0) {
                            metac_recursive_iterator_set_state(p_iter, 2);  /* failure cleanup */
                            continue;  
                        }

                        struct metac_memory_entry * p_memory_entry = metac_new_memory_entry(p, sz, children_count);
                        if (p_memory_entry == NULL) {
                            metac_recursive_iterator_set_state(p_iter, 2);  // failure cleanup
                            continue;                 
                        }

                        metac_recursive_iterator_set_context(p_iter, p_memory_entry);
                        metac_recursive_iterator_set_state(p_iter, 1);
                        continue;
                    }
                }
                case 1: {
                    struct metac_memory_entry * p_memory_entry = (struct metac_memory_entry *)metac_recursive_iterator_get_context(p_iter);
                    assert(p_memory_entry != NULL);

                    metac_num_t children_count = 0;
                    metac_flag_t failure = 0;

                    while(metac_recursive_iterator_dep_queue_is_empty(p_iter) == 0) {
                        metac_value_t * p_memb_val = NULL;
                        struct metac_memory_entry * p_member_memory_entry = metac_recursive_iterator_dequeue_and_delete_dep(p_iter, (void**)&p_memb_val, NULL);
                        if (p_member_memory_entry == NULL) {
                            if (p_memb_val != NULL) {
                                metac_value_delete(p_memb_val);
                            }
                            failure = 1;
                            break;
                        }
                        // check if member offset+size goes beyond our size
                        assert(metac_value_addr(p_member_memory_entry->p_val) >= metac_value_addr(p_memory_entry->p_val));
                        size_t offset = metac_value_addr(p_member_memory_entry->p_val) - metac_value_addr(p_memory_entry->p_val);
                        if (p_memory_entry->byte_size < offset + p_member_memory_entry->byte_size) {
                            // made bigger because of flexible member
                            p_memory_entry->byte_size = offset + p_member_memory_entry->byte_size;
                        }

                        p_memory_entry->pp_children[children_count] = p_member_memory_entry;
                        p_member_memory_entry->p_parent = p_memory_entry;
                        ++children_count;
                    }
                    if (failure != 0) {
                        metac_recursive_iterator_set_state(p_iter, 2);  // failure cleanup
                        continue;
                    }
                    metac_recursive_iterator_done(p_iter, p_memory_entry);
                    continue;
                }
                case 2: 
                default: {
                    /* failure cleanup*/
                    while(metac_recursive_iterator_dep_queue_is_empty(p_iter) == 0) {
                        metac_value_t * p_memb_val = NULL;
                        struct metac_memory_entry * p_member_memory_entry = metac_recursive_iterator_dequeue_and_delete_dep(p_iter, (void**)&p_memb_val, NULL);
                        if (p_member_memory_entry == NULL) {
                            if (p_memb_val != NULL) {
                                metac_value_delete(p_memb_val);
                            }
                        }else {
                            metac_memory_entry_delete(p_member_memory_entry);
                        }
                    }

                    struct metac_memory_entry * p_memory_entry = (struct metac_memory_entry *)metac_recursive_iterator_get_context(p_iter);
                    if (p_memory_entry != NULL) {
                        // p_in stil must exist if we fail
                        if (p_memory_entry->p_val == p) {
                            p_memory_entry->p_val = NULL;
                        }
                        metac_memory_entry_delete(p_memory_entry);
                    }

                    metac_recursive_iterator_fail(p_iter);
                    continue;
                }
            }
        }
        case METAC_KND_pointer_type: {
            switch(state) {
                case METAC_R_ITER_start: {
                    int loop_level = 0;
                    metac_size_t sz = 0;
                    if (metac_entry_byte_size(metac_value_entry(p), &sz) != 0) {
                        metac_recursive_iterator_fail(p_iter);
                        continue;
                    }

                    /* simple case 1 - NULL pointer */
                    void *p_addr = NULL;
                    if (metac_value_pointer(p, &p_addr) != 0) {
                        metac_recursive_iterator_fail(p_iter);
                        continue;
                    }
                    if (p_addr == NULL) {
                        struct metac_memory_entry * p_memory_entry = metac_new_memory_entry(p, sz, 0);
                        if (p_memory_entry == NULL) {
                            metac_recursive_iterator_fail(p_iter);
                            continue;                 
                        }
                        metac_recursive_iterator_done(p_iter, p_memory_entry);
                        continue;
                    }

                    /* simple case 2 - if we are in the loop - children will be created on the parent level */
                    loop_level = metac_value_level_introduced_loop(p_iter);
                    if (loop_level > 0) { 
                        struct metac_memory_entry * p_memory_entry = metac_new_memory_entry(p, sz, 0);
                        if (p_memory_entry == NULL) {
                            metac_recursive_iterator_fail(p_iter);
                            continue;                 
                        }
                        p_memory_entry->loop_level = loop_level;
                        metac_recursive_iterator_done(p_iter, p_memory_entry);
                        continue;    
                    }
                    /* generic case - we need to convert pointer to array (can be flexible), try handler first */
                    metac_value_t * p_arr_val = NULL;
                    if (p_tag_map != NULL) {
                        metac_value_event_t ev = {.type = METAC_RQVST_pointer_array_count, .p_return_value = NULL};
                        metac_entry_tag_t * p_tag = metac_tag_map_tag(p_tag_map, metac_value_entry(p));
                        if (p_tag != NULL && p_tag->handler) {
                            if (metac_value_event_handler_call(p_tag->handler, p_iter, &ev, p_tag->p_context) != 0) {
                                metac_recursive_iterator_fail(p_iter);
                                continue;
                            }
                            p_arr_val = ev.p_return_value;
                        }
                    }
                    if (p_arr_val == NULL && (metac_value_is_void_pointer(p) != 0 || metac_value_is_declaration_pointer(p) != 0)) {
                        switch(p_map_mode->unknown_ptr_mode) {
                            case METAC_UPTR_fail: {
                                metac_recursive_iterator_fail(p_iter);
                                continue;
                            }
                            case METAC_UPTR_as_values:
                            case METAC_UPTR_ignore: { /* FALLBACK to the simple case 1 */
                                struct metac_memory_entry * p_memory_entry = metac_new_memory_entry(p, sz, 0);
                                if (p_memory_entry == NULL) {
                                    metac_recursive_iterator_fail(p_iter);
                                    continue;
                                }
                                metac_recursive_iterator_done(p_iter, p_memory_entry);
                                continue;
                            }
                            case METAC_UPTR_as_one_byte_ptr: {
                                static struct metac_entry unsigned_char = { /* this is static - it will be used outside as type of array elem*/
                                    .kind = METAC_KND_base_type,    
                                    .name = "unsigned char",    
                                    .parents_count = 0, /* warning: no parent */
                                    .base_type_info = {        
                                        .byte_size = 1,
                                        .encoding = METAC_ENC_unsigned_char,
                                    },
                                };
                                struct metac_entry ptr = { /* CAUTION: this is emulation of static, but we immediatly delete p_new_ptr_val */
                                    .kind = METAC_KND_pointer_type,            
                                    .parents_count = 0,
                                    .pointer_type_info = {
                                        .byte_size = sizeof(void*),        
                                        .type = &unsigned_char,
                                    },
                                };
                                metac_value_t *p_new_ptr_val = metac_new_value(&ptr, metac_value_addr(p));
                                if (p_new_ptr_val == NULL) {
                                    metac_recursive_iterator_fail(p_iter);
                                    continue;
                                }
                                p_arr_val = metac_new_element_count_value(p_new_ptr_val, 1);
                                metac_value_delete(p_new_ptr_val);
                                if (p_arr_val == NULL) {
                                    metac_recursive_iterator_fail(p_iter);
                                    continue;
                                }
                            }
                        }
                    }
                    if (p_arr_val == NULL) {
                        p_arr_val = metac_new_element_count_value(p, 1); /*create arr with len 1 - good default */
                    }
                    if (p_arr_val == NULL) {
                        metac_recursive_iterator_fail(p_iter);
                        continue;
                    }
                    // assert that we really have array
                    if (metac_value_has_elements(p_arr_val) == 0) {
                        metac_value_delete(p_arr_val);
                        metac_recursive_iterator_fail(p_iter);
                        continue;                        
                    }
                    /* finally we have p_arr_val which will be the child */
                    metac_recursive_iterator_create_and_append_dep(p_iter, p_arr_val);
                    struct metac_memory_entry * p_memory_entry = metac_new_memory_entry(p, sz, 1);
                    if (p_memory_entry == NULL) {
                        metac_recursive_iterator_fail(p_iter);
                        continue;                 
                    }

                    metac_recursive_iterator_set_context(p_iter, p_memory_entry);
                    metac_recursive_iterator_set_state(p_iter, 1);
                    continue;
                }
                case 1: {
                    struct metac_memory_entry * p_memory_entry = (struct metac_memory_entry *)metac_recursive_iterator_get_context(p_iter);
                    assert(p_memory_entry != NULL);

                    metac_value_t * p_memb_val = NULL;
                    struct metac_memory_entry * p_member_memory_entry = metac_recursive_iterator_dequeue_and_delete_dep(p_iter, (void**)&p_memb_val, NULL);
                    if (p_member_memory_entry == NULL) {
                        if (p_memb_val != NULL) {
                            metac_value_delete(p_memb_val);
                        }
                        if (p_memory_entry != NULL) {
                            // p_in stil must exist if we fail
                            if (p_memory_entry->p_val == p) {
                                p_memory_entry->p_val = NULL;
                            }
                            metac_memory_entry_delete(p_memory_entry);
                        }

                        metac_recursive_iterator_fail(p_iter);
                        continue;
                    }

                    p_memory_entry->pp_children[0] = p_member_memory_entry;
                    p_member_memory_entry->p_parent = p_memory_entry;
                    /* if in DAG mode - keep also info about new entries where pointers point in hashmap*/
                    if (p_map->p_addr_ptr_val_to_memory_entry != NULL) {
                        void * addr = metac_value_addr(p_member_memory_entry->p_val);

                        if (hashmapContainsKey(p_map->p_addr_ptr_val_to_memory_entry, addr) != 0) {
                            /* compare sizes and update if we have bigger */
                            struct metac_memory_entry * p_prev_member_memory_entry = 
                                (struct metac_memory_entry *)hashmapGet(p_map->p_addr_ptr_val_to_memory_entry, addr);
                            if (p_member_memory_entry->byte_size > p_prev_member_memory_entry->byte_size) {
                                if (hashmapPut(p_map->p_addr_ptr_val_to_memory_entry, addr, (void*)p_member_memory_entry, NULL)!=0) {
                                    // p_in stil must exist if we fail
                                    if (p_memory_entry->p_val == p) {
                                        p_memory_entry->p_val = NULL;
                                    }
                                    metac_memory_entry_delete(p_memory_entry);
                                    metac_recursive_iterator_fail(p_iter);
                                    continue;
                                }
                            }
                        } else {
                            if (hashmapPut(p_map->p_addr_ptr_val_to_memory_entry, addr, (void*)p_member_memory_entry, NULL)!=0) {
                                // p_in stil must exist if we fail
                                if (p_memory_entry->p_val == p) {
                                    p_memory_entry->p_val = NULL;
                                }
                                metac_memory_entry_delete(p_memory_entry);
                                metac_recursive_iterator_fail(p_iter);
                                continue;
                            }
                        }
                    }

                    metac_recursive_iterator_done(p_iter, p_memory_entry);
                    continue;
                }
            }
        }
        case METAC_KND_func_parameter:// this is only it's unspecified param // TODO: we need also va_arg here
        case METAC_KND_subroutine_type:
        case METAC_KND_subprogram: {
            switch(state) {
                case METAC_R_ITER_start: {
                    metac_flag_t failure = 0;
                    assert(metac_value_has_parameter_load(p) != 0);

                    metac_num_t children_count = 0;

                    metac_num_t mcount = metac_value_parameter_count(p);
                    children_count += mcount;
                    for (metac_num_t i = 0; i < mcount; ++i) {
                        // we're making copy because memmap deletes all values
                        metac_value_t * p_param_val = metac_value_parameter_new_item(p, i);
                        if (p_param_val == NULL) {
                            failure = 1;
                            break;
                        }
                        metac_recursive_iterator_create_and_append_dep(p_iter, p_param_val);
                    }
                    if (failure != 0) {
                        metac_recursive_iterator_set_state(p_iter, 2);  /* failure cleanup */
                        continue;  
                    }
                    // TODO: if there is a result, add it as a lask child

                    struct metac_memory_entry * p_memory_entry = metac_new_memory_entry(p, 0, children_count);
                    if (p_memory_entry == NULL) {
                        metac_recursive_iterator_set_state(p_iter, 2);  // failure cleanup
                        continue;                 
                    }

                    metac_recursive_iterator_set_context(p_iter, p_memory_entry);
                    metac_recursive_iterator_set_state(p_iter, 1);
                    continue;
                }
                case 1: {
                    struct metac_memory_entry * p_memory_entry = (struct metac_memory_entry *)metac_recursive_iterator_get_context(p_iter);
                    assert(p_memory_entry != NULL);

                    metac_num_t children_count = 0;
                    metac_flag_t failure = 0;

                    while(metac_recursive_iterator_dep_queue_is_empty(p_iter) == 0) {
                        metac_value_t * p_memb_val = NULL;
                        struct metac_memory_entry * p_member_memory_entry = metac_recursive_iterator_dequeue_and_delete_dep(p_iter, (void**)&p_memb_val, NULL);
                        if (p_member_memory_entry == NULL) {
                            if (p_memb_val != NULL) {
                                metac_value_delete(p_memb_val);
                            }
                            failure = 1;
                            break;
                        }

                        p_memory_entry->pp_children[children_count] = p_member_memory_entry;
                        p_member_memory_entry->p_parent = p_memory_entry;
                        ++children_count;
                    }
                    if (failure != 0) {
                        metac_recursive_iterator_set_state(p_iter, 2);  // failure cleanup
                        continue;
                    }
                    metac_recursive_iterator_done(p_iter, p_memory_entry);
                    continue;
                }
                case 2: 
                default: {
                    /* failure cleanup*/
                    while(metac_recursive_iterator_dep_queue_is_empty(p_iter) == 0) {
                        metac_value_t * p_memb_val = NULL;
                        struct metac_memory_entry * p_member_memory_entry = metac_recursive_iterator_dequeue_and_delete_dep(p_iter, (void**)&p_memb_val, NULL);
                        if (p_member_memory_entry == NULL) {
                            if (p_memb_val != NULL) {
                                metac_value_delete(p_memb_val);
                            }
                        }else {
                            metac_memory_entry_delete(p_member_memory_entry);
                        }
                    }

                    struct metac_memory_entry * p_memory_entry = (struct metac_memory_entry *)metac_recursive_iterator_get_context(p_iter);
                    if (p_memory_entry != NULL) {
                        // p_in stil must exist if we fail
                        if (p_memory_entry->p_val == p) {
                            p_memory_entry->p_val = NULL;
                        }
                        metac_memory_entry_delete(p_memory_entry);
                    }

                    metac_recursive_iterator_fail(p_iter);
                    continue;
                }
            }
        }
        default: {
                /*quickly fail if we don't know how to handle*/
                metac_recursive_iterator_fail(p_iter);
                continue;
            }
        }
    }
    p_map->p_top_entry = metac_recursive_iterator_get_out(p_iter, NULL, NULL);
    metac_recursive_iterator_free(p_iter);

    // if error
    if (p_map->p_top_entry == NULL) {
        if (p_map->p_addr_ptr_val_to_memory_entry != NULL) {
            hashmapFree(p_map->p_addr_ptr_val_to_memory_entry); // hasmap containes invalid references. if p_map->p_top_entry is NULL FSM will free data
        }
        free(p_map);
        p_map = NULL;

        if (p_val != NULL) {
            metac_value_delete(p_val);
            p_val = NULL;
        }
        return NULL;
    }

    /* p_map is fully built at this point, but in case of DAG no all entries are marked as non-allocatable.
    it's ok to delete p_map with metac_value_memory_map_delete if needed */
    if (_identify_allocatable_entries(p_map) != 0 ) {
        metac_value_memory_map_delete(p_map);
        p_map = NULL;
    }

    return p_map;
}

struct _metac_value_free_context {
    void (*free_fn)(void *ptr);
};

/* callback for hashmapForEach */
static bool _value_free(void* key, void* value, void* context) {
    assert(context != NULL);
    struct _metac_value_free_context * p_context = (struct _metac_value_free_context *)context;
    void* ptr = key;
    struct metac_memory_entry *p_ptr_entry = (struct metac_memory_entry *)value;
    if (p_ptr_entry->offset_from_allocate_base == 0) { /* delete non weak only */
        if (p_context->free_fn != NULL) {
            p_context->free_fn(ptr);
        }
    }
    return true;
}

metac_flag_t metac_value_free_ex(metac_value_t * p_val,
    metac_value_memory_map_non_handled_mode_t * p_mode,
    void (*free_fn)(void *ptr),
    metac_tag_map_t * p_tag_map) {
    if (free_fn == NULL) {
        free_fn = free;
    }
    
    struct metac_memory_map * p_map = metac_new_value_memory_map_ex(p_val,
        (metac_value_memory_map_mode_t[]){{
            .memory_block_mode = METAC_MMODE_dag /* delete won't work in tree mode */,
            .unknown_ptr_mode = (p_mode != NULL)?p_mode->unknown_ptr_mode:METAC_UPTR_fail,
            .union_mode = (p_mode != NULL)?p_mode->union_mode:METAC_UNION_fail,
            .flex_array_mode = (p_mode != NULL)?p_mode->flex_array_mode:METAC_FLXARR_fail,
        }},
        p_tag_map);
    if (p_map == NULL) {
        return 0;
    }

    struct _metac_value_free_context context = {.free_fn = free_fn,};
    hashmapForEach(p_map->p_addr_ptr_val_to_memory_entry, _value_free, &context);

    metac_value_memory_map_delete(p_map);
    return 1;
}

struct _metac_value_copy_context {
    void *(*calloc_fn)(size_t nmemb, size_t size);
    void (*free_fn)(void *ptr);
    struct metac_memory_map * p_map;
    Hashmap * src_to_dst;
    bool preallocation_failed;
};

static bool _value_copy_preallocate(void* key, void* value, void* context) {
    assert(context != NULL);
    struct _metac_value_copy_context * p_context = (struct _metac_value_copy_context *)context;

    if (p_context->calloc_fn == NULL || p_context->src_to_dst == NULL) {
        p_context->preallocation_failed = 1;
        return false;
    }

    struct metac_memory_entry *p_ptr_entry = (struct metac_memory_entry *)value;
    if (p_ptr_entry->offset_from_allocate_base == 0) { /* delete non weak only */
        void * dst = p_context->calloc_fn(1, p_ptr_entry->byte_size);
        if (dst == NULL) {
            p_context->preallocation_failed = 1;
            return false;
        }
        if (hashmapPut(p_context->src_to_dst, key, dst, NULL) != 0) {
            free(dst);
            p_context->preallocation_failed = 1;
            return false;
        }
    }
    return true;
}

static bool _value_copy_free(void* key, void* value, void* context) {
    assert(context != NULL);
    struct _metac_value_copy_context * p_context = (struct _metac_value_copy_context *)context;

    assert(p_context->free_fn != NULL);
    if (p_context->free_fn == NULL) {
        return false;
    }

    if (value != NULL) {
        p_context->free_fn(value);
    }
    return true;
}

/* copy src to dst (with rewriting data), be careful, dst must have enough space.
don't use flexibe array. instead make val as a ptr to that flexible array so 
the function will create a pointer and create a deep copy for everything it points to.
returns dst in case of success.*/
metac_value_t *metac_value_copy_ex(metac_value_t * p_src_val, metac_value_t * p_dst_val,
    metac_value_memory_map_mode_t * p_mode,
    void *(*calloc_fn)(size_t nmemb, size_t size),
    void (*free_fn)(void *ptr), /* free used in case of failure */
    metac_tag_map_t * p_tag_map) {

    /*set some defaults*/
    if (p_mode == NULL) {
        p_mode = (metac_value_memory_map_mode_t *)&_default_value_memory_map_mode;
    }
    if (calloc_fn == NULL) {
        calloc_fn = calloc;
    }
    if (free_fn == NULL) {
        free_fn = free;
    }

    if (p_src_val == NULL || p_dst_val == NULL){
        return NULL;
    }

    metac_entry_t
        * p_src_final_entry = metac_entry_final_entry(metac_value_entry(p_src_val), NULL),
        * p_dst_final_entry = metac_entry_final_entry(metac_value_entry(p_dst_val), NULL);
    if (p_src_final_entry == NULL || p_dst_final_entry == NULL) {
        return NULL;
    }

    /* compare if we really can copy src to dst using c description of final types. comparing strins is because entries can be from differnt dbs*/
    int types_cmp = -1;
    char * src_cdecl = metac_entry_cdecl(p_src_final_entry), * dst_cdecl = metac_entry_cdecl(p_dst_final_entry);
    if (src_cdecl != NULL && dst_cdecl != NULL) {
        types_cmp = strcmp(src_cdecl, dst_cdecl);
    }
    if (src_cdecl != NULL) {
        free (src_cdecl);
    }
    if (dst_cdecl != NULL) {
        free(dst_cdecl);
    }
    /* check that initial entries are compatible = ideally the same */
    if (types_cmp != 0) {
        return NULL;
    }
    
    struct _metac_value_copy_context context = {
        .calloc_fn = calloc_fn,
        .free_fn = free_fn,
        .preallocation_failed = 0,
    };

    context.p_map = metac_new_value_memory_map_ex(p_src_val, p_mode, p_tag_map);
    if (context.p_map == NULL) {
        return NULL;
    }

    if (context.p_map->p_top_entry == NULL) {
        metac_value_memory_map_delete(context.p_map);
        return NULL;
    }

    if (p_mode->memory_block_mode != METAC_MMODE_tree) {
        /* preparations are needed for that mode - we need to allocate memory for all allocatable blocks */
        context.src_to_dst = hashmapCreate(1024, addr_ptr_key_hash, addr_ptr_key_equals);
        if (context.src_to_dst == NULL) {
            metac_value_memory_map_delete(context.p_map);
            return NULL;   
        }
        hashmapForEach(context.p_map->p_addr_ptr_val_to_memory_entry, _value_copy_preallocate, &context);

        if (context.preallocation_failed != 0) {
            hashmapForEach(context.src_to_dst, _value_copy_free, &context);
            hashmapFree(context.src_to_dst);
            metac_value_memory_map_delete(context.p_map);
            return NULL;
        }
    }

    context.p_map->p_top_entry->private_data = p_dst_val;
    metac_recursive_iterator_t * p_iter = metac_new_recursive_iterator(context.p_map->p_top_entry);
    for (struct metac_memory_entry * p = (struct metac_memory_entry *)metac_recursive_iterator_next(p_iter); p != NULL;
        p = (struct metac_memory_entry *)metac_recursive_iterator_next(p_iter)) {
        metac_value_t * p_dst_val = (metac_value_t *)p->private_data;
        assert(p_dst_val != NULL);

        int state = metac_recursive_iterator_get_state(p_iter);
        switch (state) {
            case METAC_R_ITER_start: {
                void * p_dst_children_addr_override = NULL;
                if (metac_value_is_pointer(p->p_val)) {
                    assert(p->children_count <= 1);
                    if (p->children_count == 1) {
                        size_t nmemb;
                        void * src;
                        switch (p_mode->memory_block_mode) {
                            case METAC_MMODE_tree:
                                nmemb = 1;
                                if (metac_value_has_elements(p->pp_children[0]->p_val)) {
                                    size_t local_nmemb = metac_value_element_count(p->pp_children[0]->p_val);
                                    if (local_nmemb > 1 && 
                                        p->pp_children[0]->byte_size % local_nmemb == 0) {
                                        nmemb = local_nmemb;
                                    }
                                }
                                p_dst_children_addr_override = calloc_fn(nmemb, p->pp_children[0]->byte_size/nmemb);
                                if (p_dst_children_addr_override == NULL) {
                                    metac_recursive_iterator_fail(p_iter);
                                    continue;
                                }
                                break;
                            case METAC_MMODE_dag:
                            default:
                                /* p_dst_children_addr_override = size of childred sz if it's allocatable .. allocated in advance */
                                src = metac_value_addr(p->pp_children[0]->p_val);
                                assert(src != NULL); /* since children count >0 this must be true */
                                assert(context.src_to_dst != NULL);
                                if (p->pp_children[0]->offset_from_allocate_base != 0) {
                                    void * allocate_src = src - p->pp_children[0]->offset_from_allocate_base;
                                    void * allocate_dst = hashmapGet(context.src_to_dst, allocate_src);
                                    if (allocate_dst == NULL) {
                                        metac_recursive_iterator_fail(p_iter);
                                        continue;
                                    }
                                    p_dst_children_addr_override = allocate_dst + p->pp_children[0]->offset_from_allocate_base;
                                } else {
                                    p_dst_children_addr_override = hashmapGet(context.src_to_dst, src);
                                    if (p_dst_children_addr_override == NULL) {
                                        metac_recursive_iterator_fail(p_iter);
                                        continue;
                                    }
                                }
                                break;
                        }
                        void *v_dst = p_dst_children_addr_override;

                        /* containerof: ptr points to field, but container has smaller address by this value */
                        void *v_src = NULL;
                        metac_value_pointer(p->p_val, &v_src);
                        assert(v_src != NULL); /* we have child - it can't be NULL pointer */

                        if (metac_value_addr(p->pp_children[0]->p_val) != v_src) {
                            size_t field_offset = v_src - metac_value_addr(p->pp_children[0]->p_val);
                            v_dst = v_dst + field_offset;
                        }

                        /* set dst val ptr */
                        if (metac_value_set_pointer(p_dst_val, v_dst)!= 0) {
                            metac_recursive_iterator_fail(p_iter);
                            continue;
                        }
                    } else if (p->loop_level > 0) {
                        /* copy ptr value from the parent pointer with which the loop is detected */
                        struct metac_memory_entry * p_parent = 
                            metac_memory_entry_parent(p, p->loop_level); /* actually the same as (struct metac_memory_entry *)metac_recursive_iterator_get_in(p_iter, p->loop_level);*/
                        if (p_parent == NULL) {
                            metac_recursive_iterator_fail(p_iter);
                            continue;  
                        }
                        assert(p_parent->private_data != NULL);
                        assert(metac_value_is_pointer((metac_value_t*)p_parent->private_data));
                        if (metac_value_copy_pointer((metac_value_t*)p_parent->private_data, p_dst_val) == NULL) {
                            metac_recursive_iterator_fail(p_iter);
                            continue;
                        }
                    } else if (p_mode->unknown_ptr_mode == METAC_UPTR_ignore){  /* set as NULL. */
                        if (metac_value_set_pointer(p_dst_val, NULL)!=0) {
                            metac_recursive_iterator_fail(p_iter);
                            continue;
                        }
                    } else if (p_mode->unknown_ptr_mode == METAC_UPTR_as_values) { /* treat as value, just copy as memory sizeof(void*) */
                        if (metac_value_copy_pointer(p->p_val, p_dst_val)== NULL) {
                            metac_recursive_iterator_fail(p_iter);
                            continue;
                        }
                    }
                }else if (metac_value_is_enumeration(p->p_val)) {
                    if (metac_value_copy_enumeration(p->p_val, p_dst_val) == NULL) {
                        metac_recursive_iterator_fail(p_iter);
                        continue;
                    }
                }else if (metac_value_is_base_type(p->p_val)) {
                    if (metac_value_copy_base_type(p->p_val, p_dst_val) == NULL) {
                        metac_recursive_iterator_fail(p_iter);
                        continue;
                    }
                }else if (p->children_count == 0 && 
                        metac_value_final_kind(p->p_val, NULL) == METAC_KND_union_type && 
                        p->byte_size > 0) {
                    switch (p_mode->union_mode) {
                        case METAC_UNION_as_mem:
                            memcpy(metac_value_addr(p_dst_val), metac_value_addr(p->p_val), p->byte_size);
                            break;
                        case METAC_UNION_ignore:
                            memset(metac_value_addr(p_dst_val), 0, p->byte_size);
                            break;
                        default:
                            break;
                    }
                }
                /* complex types will be covered here: */
                for (metac_num_t i = 0; i < p->children_count; ++i) {
                    if (p->pp_children[i] != NULL) {
                        if (p->children_count == 1 && p_dst_children_addr_override != NULL) {
                            p->pp_children[i]->private_data = metac_new_value(metac_value_entry(p->pp_children[i]->p_val), p_dst_children_addr_override);
                        } else {
                            metac_size_t delta = metac_value_addr(p->pp_children[i]->p_val) - metac_value_addr(p->p_val);
                            p->pp_children[i]->private_data = metac_new_value(metac_value_entry(p->pp_children[i]->p_val), metac_value_addr(p_dst_val) + delta);
                        }
                        metac_recursive_iterator_create_and_append_dep(p_iter, p->pp_children[i]);
                    }
                }
                metac_recursive_iterator_set_state(p_iter, 1);
                continue;
            }
            case 1: {
                int gfail = 0;
                while(metac_recursive_iterator_dep_queue_is_empty(p_iter) == 0) {
                    int fail = 0;
                    struct metac_memory_entry *p_children = NULL;
                    metac_recursive_iterator_dequeue_and_delete_dep(p_iter, (void**)&p_children, &fail);
                    if (fail!= 0) {
                        gfail = 1;
                    }
                    if (p_children != NULL && p_children->private_data != NULL) {
                        if (fail != 0 && metac_value_is_pointer(p->p_val) && p_mode->memory_block_mode == METAC_MMODE_tree) {
                            /*need to free memory in that case */
                            assert(p->children_count == 1);
                            void * p_dst_children_addr_override = metac_value_addr((metac_value_t*)p_children->private_data);
                            assert(p_dst_children_addr_override != NULL);
                            free (p_dst_children_addr_override);
                        }
                        metac_value_delete((metac_value_t*)p_children->private_data);
                        p_children->private_data = NULL;
                    }
                }
                if (gfail != 0) {
                    metac_recursive_iterator_fail(p_iter);
                    continue; 
                }
                metac_recursive_iterator_done(p_iter, NULL);
                continue;
            }
        }
    }
    int fail = 0;
    metac_recursive_iterator_get_out(p_iter, NULL, &fail);
    metac_recursive_iterator_free(p_iter);

    if (context.src_to_dst != NULL) {
        if (fail != 0) { /* free memory - it wasn't used */
            hashmapForEach(context.src_to_dst, _value_copy_free, &context);
        }
        hashmapFree(context.src_to_dst);
        context.src_to_dst = NULL;
    }
    context.p_map->p_top_entry->private_data = NULL;

    metac_value_memory_map_delete(context.p_map);
    if (fail != 0) {
        return NULL;
    }
    return p_dst_val;
}

int metac_value_equal_ex(metac_value_t * p_val1, metac_value_t * p_val2, 
    metac_value_memory_map_mode_t * p_mode,
    metac_tag_map_t * p_tag_map) {
    if (p_val1 == NULL || p_val2 == NULL) {
        return -(EINVAL);
    }

    /*set some defaults*/
    if (p_mode == NULL) {
        p_mode = (metac_value_memory_map_mode_t *)&_default_value_memory_map_mode;
    }

    struct metac_memory_map *p_map1, *p_map2;
    p_map1 = metac_new_value_memory_map_ex(p_val1, p_mode, p_tag_map);
    if (p_map1 == NULL) {
        return -(EFAULT);
    }
    p_map2 = metac_new_value_memory_map_ex(p_val2, p_mode, p_tag_map);
    if (p_map2 == NULL) {
        metac_value_memory_map_delete(p_map1);
        return -(EFAULT);
    }

    int cmp_res = 1;
    struct metac_memory_entry * p1 = p_map1->p_top_entry;
    struct metac_memory_entry * p2 = p_map2->p_top_entry;
    p1->private_data = p2;
    metac_recursive_iterator_t * p_iter = metac_new_recursive_iterator(p1);
    for (struct metac_memory_entry * p1 = (struct metac_memory_entry *)metac_recursive_iterator_next(p_iter); p1 != NULL;
        p1 = (struct metac_memory_entry *)metac_recursive_iterator_next(p_iter)) {
        struct metac_memory_entry * p2 = (struct metac_memory_entry *)p1->private_data;
        assert(p2 != NULL);

        int state = metac_recursive_iterator_get_state(p_iter);
        switch (state) {
            case METAC_R_ITER_start: {
                if (p1->byte_size != p2->byte_size ||
                    p1->children_count != p2->children_count ||
                    p1->loop_level != p2->loop_level ||
                    p1->offset_from_allocate_base != p2->offset_from_allocate_base||
                    metac_value_final_kind(p1->p_val, NULL) != metac_value_final_kind(p2->p_val, NULL)) {
                    cmp_res = 0;
                    metac_recursive_iterator_done(p_iter, &cmp_res);
                    continue;
                }

                if (metac_value_is_pointer(p1->p_val) != 0 && metac_value_is_pointer(p2->p_val) != 0) {
                    if (p1->children_count == 0 && p2->children_count == 0) {
                        if (p_mode->unknown_ptr_mode == METAC_UPTR_as_values) {
                            /*treat pointer as value*/
                            int _cmp = metac_value_equal_pointer(p1->p_val, p2->p_val);
                            if (_cmp < 0) {
                                metac_recursive_iterator_fail(p_iter);
                                continue;
                            }
                            if (_cmp == 0) {
                                cmp_res = 0;
                            }
                        }
                        metac_recursive_iterator_done(p_iter, &cmp_res);
                        continue;
                    } 
                }else if (metac_value_is_enumeration(p1->p_val) != 0 && metac_value_is_enumeration(p2->p_val) != 0) {
                    int _cmp = metac_value_equal_enumeration(p1->p_val, p2->p_val);
                    if (_cmp < 0) {
                        metac_recursive_iterator_fail(p_iter);
                        continue;
                    }
                    if (_cmp == 0) {
                        cmp_res = 0;
                    }
                    metac_recursive_iterator_done(p_iter, &cmp_res);
                    continue;
                } else if (metac_value_is_base_type(p1->p_val) != 0 && metac_value_is_base_type(p2->p_val) != 0) {
                    int _cmp = metac_value_equal_base_type(p1->p_val, p2->p_val);
                    if (_cmp < 0) {
                        metac_recursive_iterator_fail(p_iter);
                        continue;
                    }
                    if (_cmp == 0) {
                        cmp_res = 0;
                    }
                    metac_recursive_iterator_done(p_iter, &cmp_res);
                    continue;    
                }else if (p_mode->union_mode == METAC_UNION_as_mem &&
                        p1->children_count == 0 &&
                        p2->children_count == 0 && 
                        metac_value_final_kind(p1->p_val, NULL) == METAC_KND_union_type && 
                        metac_value_final_kind(p2->p_val, NULL) == METAC_KND_union_type &&
                        p1->byte_size > 0) {
                    if (p1->byte_size != p2->byte_size ||
                        memcmp(metac_value_addr(p1->p_val), metac_value_addr(p2->p_val), p1->byte_size) != 0) {
                        cmp_res = 0;
                        metac_recursive_iterator_done(p_iter, &cmp_res);
                        continue; 
                    }
                }
                /* complex types will be covered here: */
                for (metac_num_t i = 0; i < p1->children_count; ++i) {
                    assert(p1->pp_children[i] != NULL && p2->pp_children[i] != NULL);
                    if (p1->pp_children[i] != NULL && p2->pp_children[i] != NULL) {
                        p1->pp_children[i]->private_data = p2->pp_children[i];
                        metac_recursive_iterator_create_and_append_dep(p_iter, p1->pp_children[i]);
                    }
                }
                metac_recursive_iterator_set_state(p_iter, 1);
                continue;

            }
            case 1:
            default: {
                int gfail = 0;
                while(metac_recursive_iterator_dep_queue_is_empty(p_iter) == 0) {
                    int fail = 0;
                    struct metac_memory_entry *p1_children = NULL;
                    metac_recursive_iterator_dequeue_and_delete_dep(p_iter, (void**)&p1_children, &fail);
                    assert(p1_children != NULL);
                    if (p1_children != NULL) {
                        p1_children->private_data = NULL;
                    }
                    if (fail!= 0) {
                        gfail = 1;
                    }
                }
                if (gfail != 0) {
                    metac_recursive_iterator_fail(p_iter);
                    continue;
                }
                metac_recursive_iterator_done(p_iter, &cmp_res);
                continue; 
            }

        }
    }
    int fail = 0;
    metac_recursive_iterator_get_out(p_iter, NULL, &fail);
    metac_recursive_iterator_free(p_iter);

    p1->private_data = NULL;

    metac_value_memory_map_delete(p_map2);
    metac_value_memory_map_delete(p_map1);
    
    return fail == 0? cmp_res: -(EFAULT);
}
