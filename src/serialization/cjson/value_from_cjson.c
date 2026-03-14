#include "metac/serialization/cjson.h"

#include "metac/backend/serialization/common.h"
#include "metac/backend/iterator.h"
#include "metac/backend/value.h" // metac_value_event_handler_call???

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <cjson/cJSON.h>

// potentially make common
#define handle_state(_iterator_, _done_output, _fn_call) do { \
        int res = _fn_call; \
        if (res == 0) { \
            metac_recursive_iterator_done(_iterator_, _done_output); \
        } else if (res > 0) { \
            metac_recursive_iterator_set_state(_iterator_, res); \
        } else { \
            metac_recursive_iterator_fail(_iterator_); \
        } \
    }while (0)

// non-recursive kind - potentially make external
static int metac_value_base_type_from_cjson(metac_value_t* p_val, struct cJSON* json) {
    if (metac_value_is_bool(p_val)) {
        return metac_value_set_bool(p_val, cJSON_IsTrue(json));
    }
    if (metac_value_is_float_complex(p_val) || 
        metac_value_is_double_complex(p_val) ||
        metac_value_is_ldouble_complex(p_val)) {
        if (cJSON_IsObject(json)) {
            struct cJSON* json_real = cJSON_GetObjectItem(json, "real");
            struct cJSON* json_img = cJSON_GetObjectItem(json, "img");
            if (json_real == NULL || !cJSON_IsNumber(json_real) ||
                json_img == NULL || !cJSON_IsNumber(json_img)) {
                return -(EINVAL);
            }
            double  real = cJSON_GetNumberValue(json_real),
                    img = cJSON_GetNumberValue(json_img);
            if (metac_value_is_float_complex(p_val)) {
                return metac_value_set_float_complex(p_val, ((float)real) + I * ((float)img));
            }
            if (metac_value_is_double_complex(p_val)) {
                return metac_value_set_double_complex(p_val, ((double)real) + I * ((double)img));
            }
            if (metac_value_is_ldouble_complex(p_val)) {
                // TODO: we're losing precision in case of long double and cjson
                return metac_value_set_ldouble_complex(p_val, ((long double)real) + I * ((long double)img));
            }
            return -(EFAULT);
        }
        if (cJSON_IsString(json)) {
            if (metac_value_base_type_from_string(p_val, cJSON_GetStringValue(json)) == NULL) {
                return -(EINVAL);
            }
            return -(EFAULT);
        }
        return -(EINVAL);
    }
    if (cJSON_IsNumber(json)) {
        double num = cJSON_GetNumberValue(json);
        if (metac_value_is_char(p_val)) return metac_value_set_char(p_val, (char)num);
        if (metac_value_is_uchar(p_val)) return metac_value_set_uchar(p_val, (unsigned char)num);
        if (metac_value_is_short(p_val)) return metac_value_set_short(p_val, (short)num);
        if (metac_value_is_ushort(p_val)) return metac_value_set_ushort(p_val, (unsigned short)num);
        if (metac_value_is_int(p_val)) return metac_value_set_int(p_val, (int)num);
        if (metac_value_is_uint(p_val)) return metac_value_set_uint(p_val, (unsigned int)num);
        if (metac_value_is_long(p_val)) return metac_value_set_long(p_val, (long)num);
        if (metac_value_is_llong(p_val)) return metac_value_set_llong(p_val, (long long)num);
        if (metac_value_is_float(p_val)) return metac_value_set_float(p_val, (float)num);
        if (metac_value_is_double(p_val)) return metac_value_set_double(p_val, num);
    }
    return -(EINVAL); // Type mismatch
}

// non-recursive kind - potentially make external
static int metac_value_enumeration_type_from_cjson(metac_value_t* p_val, struct cJSON* json) {
    if (!cJSON_IsString(json)) {
        return -(EINVAL);
    }
    char * value = cJSON_GetStringValue(json);
    if (value == NULL) {
        return -(EFAULT);
    }
    metac_num_t icount = metac_value_enumeration_info_size(p_val);
    for (metac_num_t i = 0; i < icount; ++i) {
        struct metac_type_enumerator_info const * p_enum_entry = metac_value_enumeration_info(p_val, i);
        assert(p_enum_entry);
        assert(p_enum_entry->name != NULL);
        if (strcmp(p_enum_entry->name, value) == 0) {
            metac_value_set_enumeration(p_val, p_enum_entry->const_value);
            return 0;
        }
    }
    return -(ENODATA); // Enum value not found
}

// here in cjson deser as you can see below we use tasks as iterator task, it contains p_val
static metac_value_t *_metac_value_from_cjson_value_extractor(void*p_in) {
    if (p_in == NULL) {
        return NULL;
    }
    metac_deserialization_task_t * p_task = (metac_deserialization_task_t *)p_in;
    return p_task->p_val;
}

static int _metac_value_from_cjson_cleanup_failure(metac_recursive_iterator_t * p_iterator) {
    while (metac_recursive_iterator_dep_queue_is_empty(p_iterator) == 0) {
        metac_deserialization_task_t * p_task = NULL;
        metac_recursive_iterator_dequeue_and_delete_dep(p_iterator, (void**)&p_task, NULL);
        if (p_task != NULL) {
            if (p_task->allocating_el_number != 0) {
                //TODO: we allocated memory!
            }
            if (p_task->p_val != NULL ) {
                metac_value_delete(p_task->p_val);
            }
            metac_deserialization_task_delete(p_task);
        }
    }
    return -(EFAULT);
}    

static metac_deserialization_task_t * _find_task_with_allocation(metac_recursive_iterator_t * p_iterator) {
    int level = metac_recursive_iterator_level(p_iterator);
    if (level < 1) {
        return NULL;
    }
    for (int l = 0; l < level + 1; ++l) { /* if level = 1 there are 2 levels: 0 and 1 */
        metac_deserialization_task_t * p_task = metac_recursive_iterator_get_in(p_iterator, l);
        if (p_task->allocating_el_number != 0) {
            return p_task;
        }
    }
    return NULL;
}

static int _metac_value_pointer_from_cjson(
    // iter param
    metac_recursive_iterator_t * p_iter,
    metac_deserialization_task_t * p,
    metac_kind_t final_kind,
    int state,
    // in_param
    metac_value_deserialization_mode_t * p_mode,
    void *(*calloc_fn)(size_t nmemb, size_t size),
    void (*free_fn)(void *ptr), /* free used in case of failure */
    metac_tag_map_t* p_tag_map) {
    struct cJSON * json = (struct cJSON *)p->p_external;
    switch (state) {
        case METAC_R_ITER_start: {
            if (cJSON_IsNull(json)) {
                if (metac_value_set_pointer(p->p_val, NULL) != 0) {
                    return -(EFAULT);
                }
                return 0;
            }
            // we need to detect what mode we used when serialized. it it's a string - that was shallow or void*
            if (cJSON_IsString(json)) {
                // TODO: maybe we should use some measures/warnings, pointer may be non valid - handle p_mode
                if (metac_value_pointer_from_string(p->p_val, cJSON_GetStringValue(json)) == NULL) {
                    return -(EFAULT);
                }
                return 0;                    
            }
            if (cJSON_IsObject(json)) { // slightly different from array
                // need to allocate memory, it can be reallocated later if needed by flex array
                metac_size_t allocating_sz = 0;
                metac_entry_t * p_allocating_entry = metac_entry_pointer_entry(metac_value_entry(p->p_val));
                // TODO check if it's not void *, probably we'll need to work with tags?
                if (p_allocating_entry == NULL) {
                    // we're void *, all we need is - to get type, we know everything else (but can verify on this end)
                    if (p_tag_map != NULL) {
                        metac_value_event_t ev = {.type = METAC_RQVST_pointer_array_count, .p_return_value = NULL};
                        metac_entry_tag_t * p_tag = metac_tag_map_tag(p_tag_map, metac_value_entry(p->p_val));
                        if (p_tag != NULL && p_tag->handler) {
                            int dummy = 0;
                            // we need this because otherwise we won't pass checks of handler
                            metac_value_set_pointer(p->p_val, &dummy);

                            if (metac_value_event_handler_call(p_tag->handler, p_iter, &_metac_value_from_cjson_value_extractor, &ev, p_tag->p_context) != 0) {
                                return -(EFAULT);
                            }
                            // return back
                            metac_value_set_pointer(p->p_val, NULL);

                            if (ev.p_return_value == NULL) {
                                return -(EFAULT);
                            }
                            if (!metac_value_element_count_flexible(ev.p_return_value)) {
                                // that must be array with size 1 or flexible.. we anyway have object - nothing to check
                                if (metac_value_final_kind(ev.p_return_value, NULL) != METAC_KND_array_type ||
                                    metac_value_element_count(ev.p_return_value) != 1) {
                                    metac_value_delete(ev.p_return_value);
                                    return -(EFAULT);
                                }
                            }
                            p_allocating_entry = metac_entry_element_entry(metac_value_entry(ev.p_return_value));
                            metac_value_delete(ev.p_return_value);
                        }
                    }
                }
                if (p_allocating_entry == NULL ||
                    metac_entry_byte_size(p_allocating_entry, &allocating_sz) != 0) {
                    return -(EFAULT);
                }

                void * addr = calloc_fn(1, allocating_sz);
                if (addr == NULL) {
                    return -(EFAULT);
                }
                metac_value_t * p_allocating_value = metac_new_value(p_allocating_entry, addr);
                if (p_allocating_value == NULL ) {
                    free_fn(addr);
                    return -(EFAULT);
                }
                metac_deserialization_task_t * p_task = metac_new_deserialization_task(p_allocating_value, json);
                if (p_task == NULL) {
                    metac_value_delete(p_allocating_value);
                    free_fn(addr);
                    return -(EFAULT);
                }
                if (metac_recursive_iterator_create_and_append_dep(p_iter, p_task) != 0) {
                    metac_deserialization_task_delete(p_task);
                    metac_value_delete(p_allocating_value);
                    free_fn(addr);
                    return -(EFAULT);
                }
                // schedule children deserialization
                return 1;
            }
            if (cJSON_IsArray(json)) {
                // need to allocate memory, it can be reallocated later if needed by flex array
                metac_size_t allocating_len = cJSON_GetArraySize(json);
                metac_size_t allocating_el_sz = 0; // element size
                metac_entry_t * p_allocating_entry = metac_entry_pointer_entry(metac_value_entry(p->p_val));
                // TODO check if it's not void *, probably we'll need to work with tags?
                metac_value_t * p_arr_val = NULL;
                if (p_allocating_entry == NULL) {
                    // we're void *, all we need is - to get type, we know everything else (but can verify on this end)
                    if (p_tag_map != NULL) {
                        metac_value_event_t ev = {.type = METAC_RQVST_pointer_array_count, .p_return_value = NULL};
                        metac_entry_tag_t * p_tag = metac_tag_map_tag(p_tag_map, metac_value_entry(p->p_val));
                        if (p_tag != NULL && p_tag->handler) {
                            int dummy = 0;
                            // we need this because otherwise we won't pass checks of handler
                            metac_value_set_pointer(p->p_val, &dummy);

                            if (metac_value_event_handler_call(p_tag->handler, p_iter, &_metac_value_from_cjson_value_extractor, &ev, p_tag->p_context) != 0) {
                                return -(EFAULT);
                            }
                            // return back
                            metac_value_set_pointer(p->p_val, NULL);

                            if (ev.p_return_value == NULL) {
                                return -(EFAULT);
                            }
                            // that must be array with size at least cJSON_GetArraySize TODO: it can be flexible 
                            // (len -1, in that case we want to allocate len 1 and flexible array will reallocate, though flexible array impl doesn't check len now)
                            if (metac_value_element_count_flexible(ev.p_return_value)) {
                                allocating_len = 2; //TODO: 1 when we fix realloc on flex array side // make default as 1 - realloc will change it
                            } else {
                                allocating_len = metac_value_element_count(ev.p_return_value);
                                if (metac_value_final_kind(ev.p_return_value, NULL) != METAC_KND_array_type ||
                                    allocating_len < cJSON_GetArraySize(json)) {
                                    metac_value_delete(ev.p_return_value);
                                    return -(EFAULT);
                                }
                            }
                                
                            p_allocating_entry = metac_entry_element_entry(metac_value_entry(ev.p_return_value));
                            p_arr_val = ev.p_return_value;
                            // NOTE: p_arr_val contains address of our dummy integer, we can't use that
                        }
                    }
                }else{
                    p_arr_val = metac_new_element_count_value(p->p_val, allocating_len);
                }
                if (p_allocating_entry == NULL ||
                    metac_entry_byte_size(p_allocating_entry, &allocating_el_sz) != 0) {
                    metac_value_delete(p_arr_val);
                    return -(EFAULT);
                }

                void * addr = calloc_fn(allocating_len, allocating_el_sz);
                if (addr == NULL) {
                    metac_value_delete(p_arr_val);
                    return -(EFAULT);
                }
                metac_value_t * p_arr_val_new = metac_new_value(metac_value_entry(p_arr_val), addr);
                metac_value_delete(p_arr_val);
                p_arr_val = p_arr_val_new;
                p_arr_val_new = NULL;
                if (p_arr_val == NULL) {
                    return -(EFAULT);
                }
                
                metac_deserialization_task_t * p_task = metac_new_deserialization_task(p_arr_val, json);
                if (p_task == NULL) {
                    metac_value_delete(p_arr_val);
                    free_fn(addr);
                    return -(EFAULT);
                }
                if (metac_recursive_iterator_create_and_append_dep(p_iter, p_task) != 0) {
                    metac_deserialization_task_delete(p_task);
                    metac_value_delete(p_arr_val);
                    free_fn(addr);
                    return -(EFAULT);
                }
                // schedule children deserialization
                return 1;
            }
        }
        case 1: { // we returned after children finished
            void * addr = NULL; // address can be reallocated, so it's better to set it here - after children finished
            int counter = 0; // we expect only 1 child
            while (metac_recursive_iterator_dep_queue_is_empty(p_iter) == 0) {
                metac_deserialization_task_t * p_task = NULL;

                if (counter != 0) return 2; // if more than 1 child - cleanup and failure
                ++counter;

                metac_value_t * p_val_out = (metac_value_t *)metac_recursive_iterator_dequeue_and_delete_dep(p_iter, (void**)&p_task, NULL);
                if (p_task != NULL) {
                    if (p_task->p_val != NULL ) {
                        addr = metac_value_addr(p_task->p_val);
                        metac_value_delete(p_task->p_val);
                    }
                    metac_deserialization_task_delete(p_task);
                }
            }
            metac_value_set_pointer(p->p_val, addr);
            return 0;
        }
        case 2: break;
    }
    return _metac_value_from_cjson_cleanup_failure(p_iter);
}


static int _metac_value_with_members_from_cjson(
    // iter param
    metac_recursive_iterator_t * p_iter,
    metac_deserialization_task_t * p,
    metac_kind_t final_kind,
    int state) {
    struct cJSON * json = (struct cJSON *)p->p_external;
    switch (state) {
        case METAC_R_ITER_start: {
            if (!cJSON_IsObject(json)) {
                return -(EFAULT);
            }

            if (final_kind == METAC_KND_union_type) {
                // TODO: make sure first that there only 1 field in JSON which will be handled
                // fail if there are many which match - otherwise it may be a vulnarability 
            }

            // Struct deserialization succeeds as long as we could process all members
            // Individual field failures are silently ignored (partial deserialization)
            metac_num_t mcount = metac_value_member_count(p->p_val);
            for (metac_num_t i = 0; i < mcount; ++i) {

                metac_value_t* p_memb_val = metac_new_value_by_member_id(p->p_val, i);
                if (p_memb_val == NULL) {
                    return 2; // cleanup and failure
                }

                metac_name_t memb_name = metac_value_name(p_memb_val);
                struct cJSON* memb_json = NULL;

                if (memb_name && memb_name[0] != '\0') {
                    // Named member - find by name in JSON object
                    memb_json = cJSON_GetObjectItemCaseSensitive(json, memb_name);
                } else {
                    // Anonymous member - handle nested struct specially
                    // Anonymous members inherit the parent JSON object
                    memb_json = json;
                }

                if (memb_json) {
                    // Attempt to deserialize this member
                    // Note: We don't fail the whole struct if a member fails
                    // This allows partial deserialization for structs with optional fields
                    metac_deserialization_task_t * p_task = metac_new_deserialization_task(p_memb_val, memb_json);
                    if (p_task == NULL) {
                        metac_value_delete(p_memb_val);
                        return 2; // cleanup and failure
                    }
                    if (metac_recursive_iterator_create_and_append_dep(p_iter, p_task) != 0) {
                        metac_deserialization_task_delete(p_task);
                        metac_value_delete(p_memb_val);
                        return 2; // cleanup and failure
                    }
                } else {
                    // we just skip the field
                    metac_value_delete(p_memb_val);
                }
            }
            return 1; // next state
        }
        case 1: {
            while (metac_recursive_iterator_dep_queue_is_empty(p_iter) == 0) {
                metac_deserialization_task_t * p_task = NULL;
                metac_value_t * p_val_out = (metac_value_t *)metac_recursive_iterator_dequeue_and_delete_dep(p_iter, (void**)&p_task, NULL);
                if (p_task != NULL) {
                    if (p_task->p_val != NULL ) {
                        metac_value_delete(p_task->p_val);
                    }
                    metac_deserialization_task_delete(p_task);
                }
                if (p_val_out == NULL) {
                    return 2; // cleanup and failure
                }
            }
            return 0; // done!
        }
        case 2: break;
    }
    return _metac_value_from_cjson_cleanup_failure(p_iter);
}

static int _metac_value_with_elements_from_cjson(
    // iter param
    metac_recursive_iterator_t * p_iter,
    metac_deserialization_task_t * p,
    metac_kind_t final_kind,
    int state) {
    struct cJSON * json = (struct cJSON *)p->p_external;
    switch (state) {
        case METAC_R_ITER_start: {
            if (!cJSON_IsArray(json)) {
                return -(EFAULT);
            }

            metac_num_t count = metac_value_element_count(p->p_val);
            int json_count = cJSON_GetArraySize(json);

            metac_value_t * p_local = p->p_val;
            metac_value_t * p_non_flexible = NULL;

            if (metac_value_element_count_flexible(p->p_val) == 0) {
                if (json_count > count) {
                    // TODO: options - ignore extra, fail - make configurable
                    return 2; // cleanup and failure
                }
                // json may contain less - those elements will be init with zeros
                count = json_count;
            } else {
                // support flexible arrays (needs realloc)
                count = json_count;
                p_non_flexible = metac_new_element_count_value(p->p_val, count);
                // TODO: find in hierarchy if we need/can reallocate parent to fit this flexible array
                // need to work on pointers to understand how to do this
                metac_deserialization_task_t * p_tast_with_allocation = _find_task_with_allocation(p_iter);
                if (/*1 couldn't reallocate*/p_tast_with_allocation == NULL) {
                    metac_value_delete(p_non_flexible);
                    return 2; // cleanup and failure
                }
                // reallocate
                //p_tast_with_allocation
            }

            for (metac_num_t i = 0; i < count; ++i) {
                metac_value_t* p_el_val = metac_new_value_by_element_id(p_local, i);
                struct cJSON* el_json = cJSON_GetArrayItem(json, i);
                if (p_el_val == NULL) {
                    return 2; // cleanup and failure
                }
                if (el_json) {
                    // Attempt to deserialize this member
                    // Note: We don't fail the whole struct if a member fails
                    // This allows partial deserialization for structs with optional fields
                    metac_deserialization_task_t * p_task = metac_new_deserialization_task(p_el_val, el_json);
                    if (p_task == NULL) {
                        metac_value_delete(p_el_val);
                        return 2; // cleanup and failure
                    }
                    if (metac_recursive_iterator_create_and_append_dep(p_iter, p_task) != 0) {
                        metac_deserialization_task_delete(p_task);
                        metac_value_delete(p_el_val);
                        return 2; // cleanup and failure
                    }
                }
            }
            // return back
            if (p_non_flexible) {
                metac_value_delete(p_non_flexible);
                p_local = p->p_val;
            }
            return 1; // next state
        }
        case 1: {
            while (metac_recursive_iterator_dep_queue_is_empty(p_iter) == 0) {
                metac_deserialization_task_t * p_task = NULL;
                metac_value_t * p_val_out = (metac_value_t *)metac_recursive_iterator_dequeue_and_delete_dep(p_iter, (void**)&p_task, NULL);
                if (p_task != NULL) {
                    if (p_task->p_val != NULL ) {
                        metac_value_delete(p_task->p_val);
                    }
                    metac_deserialization_task_delete(p_task);
                }
                if (p_val_out == NULL) {
                    return 2; // cleanup and failure
                }
            }
            return 0; // done!
        }
        case 2: break;
    }
    return _metac_value_from_cjson_cleanup_failure(p_iter);
}

// all non-recursive and recursive kinds
int metac_value_from_cjson(metac_value_t* p_val, struct cJSON* in_json, 
    metac_value_deserialization_mode_t * p_mode,
    void *(*calloc_fn)(size_t nmemb, size_t size),
    void (*free_fn)(void *ptr), /* free used in case of failure */
    metac_tag_map_t* p_tag_map) {
    if (p_val == NULL || in_json == NULL) {
        return -(EINVAL);
    }

    // set defaults
    if (calloc_fn == NULL) {
        calloc_fn = calloc;
    }
    if (free_fn == NULL) {
        free_fn = free;
    }

    metac_deserialization_task_t * p_task = metac_new_deserialization_task(p_val, in_json);
    if (p_task == NULL) {
        return -(ENOMEM);
    }

    metac_recursive_iterator_t * p_iter = metac_new_recursive_iterator(p_task);
    for (metac_deserialization_task_t * p = (metac_deserialization_task_t*)metac_recursive_iterator_next(p_iter); p != NULL;
        p = (metac_deserialization_task_t*)metac_recursive_iterator_next(p_iter)) {
        int state = metac_recursive_iterator_get_state(p_iter);
        struct cJSON * json = (struct cJSON *)p->p_external;

        // we use value kind (dst) to identify to where we need to store and match it to src (json) 
        metac_kind_t final_kind = metac_value_final_kind(p->p_val, NULL);
        switch (final_kind) {
            case METAC_KND_base_type: {
                handle_state(p_iter, p->p_val, metac_value_base_type_from_cjson(p->p_val, json));
                continue;
            }
            case METAC_KND_enumeration_type: {
                handle_state(p_iter, p->p_val, metac_value_enumeration_type_from_cjson(p->p_val, json));
                continue;
            }
            case METAC_KND_pointer_type: {
                handle_state(p_iter, p->p_val, _metac_value_pointer_from_cjson(p_iter, p, final_kind, state, p_mode, calloc_fn, free_fn, p_tag_map));
                continue; 
            }
            case METAC_KND_union_type:
            case METAC_KND_struct_type: {
                handle_state(p_iter, p->p_val, _metac_value_with_members_from_cjson(p_iter, p, final_kind, state));
                continue;
            }
            case METAC_KND_array_type: {
                handle_state(p_iter, p->p_val, _metac_value_with_elements_from_cjson(p_iter, p, final_kind, state));
                continue;
            }
            // fail in case we couldn't find anythin
            default: {
                metac_recursive_iterator_fail(p_iter);
                continue;
            }
        }
    }
    int fail = 0;
    metac_recursive_iterator_get_out(p_iter, (void **)&p_task, &fail);
    metac_deserialization_task_delete(p_task);
    metac_recursive_iterator_free(p_iter);
    return fail;
}
