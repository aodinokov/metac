#include "metac/serialization/cjson.h"

#include "metac/backend/serialization/common.h"
#include "metac/backend/iterator.h"
#include "metac/backend/value.h" // metac_value_event_handler_call to handle container_of case

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <cjson/cJSON.h>

int metac_value_base_type_from_cjson(metac_value_t* p_val, struct cJSON* json) {
    if (p_val == NULL || !metac_value_is_base_type(p_val) ||
        json == NULL) {
        return -(EINVAL);
    }

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
                // TODO: we're losing precision in case of long double and cjson. one options is to use string for that
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
        if (metac_value_is_ulong(p_val)) return metac_value_set_ulong(p_val, (unsigned long)num);
        if (metac_value_is_llong(p_val)) return metac_value_set_llong(p_val, (long long)num);
        if (metac_value_is_ullong(p_val)) return metac_value_set_ullong(p_val, (unsigned long long)num);
        if (metac_value_is_float(p_val)) return metac_value_set_float(p_val, (float)num);
        if (metac_value_is_double(p_val)) return metac_value_set_double(p_val, num);
        // TODO: we're losing precision in case of long double and cjson. one options is to use string for that
        if (metac_value_is_ldouble(p_val)) return metac_value_set_ldouble(p_val, num);
    }
    return -(EINVAL); // Type mismatch
}

int metac_value_enumeration_type_from_cjson(metac_value_t* p_val, struct cJSON* json) {
    if (p_val == NULL || !metac_value_is_enumeration(p_val) ||
        json == NULL || !cJSON_IsString(json)) {
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

// Helper: Create task and append to iterator
// Returns 0 on success, 2 on failure (for cleanup)
static int _metac_create_and_append_task(metac_recursive_iterator_t * p_iter, metac_value_t * p_val, void * p_external, metac_value_t ** p_cleanup_val) {
    metac_deserialization_task_t * p_task = metac_new_deserialization_task(p_val, p_external);
    if (p_task == NULL) {
        if (p_cleanup_val) *p_cleanup_val = p_val;
        return 2; // cleanup and fail
    }
    if (metac_recursive_iterator_create_and_append_dep(p_iter, p_task) != 0) {
        metac_deserialization_task_delete(p_task);
        if (p_cleanup_val) *p_cleanup_val = p_val;
        return 2; // cleanup and fail
    }
    return 0; // success
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
            // we need to detect what mode we used when serialized. if it's a string - that was shallow or void*
            if (cJSON_IsString(json)) {
                char *json_string_value = cJSON_GetStringValue(json);

                // first - try a special case - zero-ended char*
                // getting type of the object to which our pointer is pointing
                metac_entry_t * p_allocating_entry = metac_entry_pointer_entry(metac_value_entry(p->p_val));

                if (p_allocating_entry == NULL) { // pointer to void*
                    // all we need is - to get type, we know everything else (but can verify on this end)
                    if (p_tag_map != NULL) {
                        metac_value_event_t ev = {.type = METAC_RQVST_pointer_array_count, .p_return_value = NULL};
                        metac_entry_tag_t * p_tag = metac_tag_map_tag(p_tag_map, metac_value_entry(p->p_val));
                        if (p_tag != NULL && p_tag->handler) {
                            // NOTE: out pointer has NULL value right now, we haven't deserialized anything yet,
                            // in order to veryfy that it's possible to move this route, use cjson buffer
                            
                            metac_value_set_pointer(p->p_val, json_string_value);

                            if (metac_value_event_handler_call(p_tag->handler, p_iter, &_metac_deserialization_task_value_extractor, &ev, p_tag->p_context) != 0) {
                                return -(EFAULT);
                            }
                            // once called, we can put back NULL to our pointer
                            metac_value_set_pointer(p->p_val, NULL);

                            if (ev.p_return_value != NULL) {
                                // ignore errors
                                // taking the type of the object to which pointer is pointing
                                assert(metac_value_addr(ev.p_return_value) == json_string_value);
                                p_allocating_entry = metac_entry_element_entry(metac_value_entry(ev.p_return_value));
                                metac_value_delete(ev.p_return_value);
                            }
                        }
                    }
                }
                if (p_allocating_entry != NULL) {
                    // check allocating_entry
                    metac_entry_t * p_element_final_entry = metac_entry_final_entry((p_allocating_entry), NULL);
                    if (p_element_final_entry != NULL) {
                        if (metac_entry_is_char(p_element_final_entry)) {
                            // potentially we had to go via arrays, but lets make it here for now
                            p->allocated_el_sz = 1;
                            p->allocated_el_number = strlen(json_string_value) + 1;

                            p->p_allocated = calloc_fn(
                                p->allocated_el_number, 
                                p->allocated_prefix_size + p->allocated_el_sz + p->allocated_flexible_el_number * p->allocated_flexible_el_sz);
                            if (p->p_allocated == NULL) {
                                return -(EFAULT);
                            }
                            memcpy(p->p_allocated, json_string_value, p->allocated_el_number);
                            metac_value_set_pointer(p->p_val, p->p_allocated);
                            return 0; // done!
                        }
                    }
                }

                if (p_mode->string_ptr_mode == METAC_DESER_string_ptr_allow) {
                    if (metac_value_pointer_from_string(p->p_val, json_string_value) == NULL) {
                        return -(EFAULT);
                    }
                    return 0;
                }
            }
            if (cJSON_IsObject(json)) { // pointer pointing to a single object - it can have flex array in it
                // getting type of the object to which our pointer is pointing
                metac_entry_t * p_allocating_entry = metac_entry_pointer_entry(metac_value_entry(p->p_val));

                if (p_allocating_entry == NULL) { // pointer to void*
                    // all we need is - to get type, we know everything else (but can verify on this end)
                    if (p_tag_map != NULL) {
                        metac_value_event_t ev = {.type = METAC_RQVST_pointer_array_count, .p_return_value = NULL};
                        metac_entry_tag_t * p_tag = metac_tag_map_tag(p_tag_map, metac_value_entry(p->p_val));
                        if (p_tag != NULL && p_tag->handler) {
                            // NOTE: out pointer has NULL value right now, we haven't deserialized anything yet,
                            // but if we call tag_map it won't work, because normally it is supposed to return our array/flex_array of
                            // identified type and addr will be taken from our pointer. We assume that
                            // tag map won't read memory at this pointer in order to identify its type, so we can call it with some dummy address:
                            // so, lets store to our pointer temporary some valid, addr. We make it char, because we want the math in bytes
                            char dummy = 0;
                            metac_value_set_pointer(p->p_val, &dummy);

                            if (metac_value_event_handler_call(p_tag->handler, p_iter, &_metac_deserialization_task_value_extractor, &ev, p_tag->p_context) != 0) {
                                return -(EFAULT);
                            }
                            // once called, we can put back NULL to our pointer
                            metac_value_set_pointer(p->p_val, NULL);

                            if (ev.p_return_value == NULL) {
                                return -(EFAULT);
                            }

                            // extra check if we got non-flexible array, we may check that its length = 1
                            if (!metac_value_element_count_flexible(ev.p_return_value)) {
                                // that must be array with size 1 or flexible.. we anyway have object - nothing to check
                                if (metac_value_final_kind(ev.p_return_value, NULL) != METAC_KND_array_type ||
                                    metac_value_element_count(ev.p_return_value) != 1) {
                                    metac_value_delete(ev.p_return_value);
                                    return -(EFAULT);
                                }
                            }

                            // in case of container_of the returned array may have a different address (typically smaller) than &dummy
                            char *p_addr = (char*)metac_value_addr(ev.p_return_value); 
                            assert(p_addr != NULL);
                            assert(p_addr <= &dummy);
                            if (p_addr == NULL || p_addr > &dummy) {
                                metac_value_delete(ev.p_return_value);
                                return -(EFAULT);
                            }
                            p->allocated_prefix_size = &dummy - p_addr;

                            // taking the type of the object to which pointer is pointing
                            p_allocating_entry = metac_entry_element_entry(metac_value_entry(ev.p_return_value));
                            metac_value_delete(ev.p_return_value);
                        }
                    }
                }
                // if we were not able to identify the type of can't get its size
                if (p_allocating_entry == NULL ||
                    metac_entry_byte_size(p_allocating_entry, &p->allocated_el_sz) != 0) {
                    return -(EFAULT);
                }                

                // check if p_allocating_entry has flexible array and if so we'll also get it's lenght based on the corresponding json data
                int flex_res = 0;
                // NOTE: be careful! 
                // for now we don't plan to modify this memory, so we're using pointer to flex_res as dummy value of valid address, but
                // its lenght isn't enough. we're using it only for metac_value_from_cjson_determine_flexible_sz, which
                // right now has only _metac_value_with_members_from_cjson and _metac_value_with_elements_from_cjson - and they don't read memory.
                // also they don't use tag_map, that means it's ok for now to use this dummy address. in future it may change
                metac_value_t * p_dummy_value = metac_new_value(p_allocating_entry, &flex_res);
                if (p_dummy_value == NULL) {
                    return -(EFAULT);
                }
                p->allocated_flexible_el_number = 0;
                p->allocated_flexible_el_sz = 0;
                flex_res = metac_value_from_cjson_determine_flexible_sz(p_dummy_value, json, p_mode, p_tag_map, &p->allocated_flexible_el_number, &p->allocated_flexible_el_sz);
                metac_value_delete(p_dummy_value);
                if (flex_res != 0) {
                    return -(EFAULT);
                }

                p->allocated_el_number = 1;

                p->p_allocated = calloc_fn(
                    p->allocated_el_number, 
                    p->allocated_prefix_size + p->allocated_el_sz + p->allocated_flexible_el_number * p->allocated_flexible_el_sz);
                if (p->p_allocated == NULL) {
                    return -(EFAULT);
                }

                metac_value_t * p_allocating_value = metac_new_value(p_allocating_entry, p->p_allocated);
                if (p_allocating_value == NULL ) {
                    return 2; // cleanup and fail
                }
                int res = _metac_create_and_append_task(p_iter, p_allocating_value, json, &p_allocating_value);
                if (res != 0) {
                    if (p_allocating_value) metac_value_delete(p_allocating_value);
                    return res;
                }
                // schedule children deserialization
                return 1;
            }
            if (cJSON_IsArray(json)) {
                metac_value_t * p_arr_val = NULL;

                p->allocated_el_number = cJSON_GetArraySize(json);
                p->allocated_el_sz = 0; // don't know yet, need to identify

                if (p->allocated_el_number == 0) {
                    // in C we can't create pointer which will point to the 0-lenght array, so pointer will point to NULL
                    return 0;
                }

                // NOTE: out pointer has NULL value right now, we haven't deserialized anything yet,
                // but if we call tag_map it won't work, because normally it is supposed to return our array/flex_array of
                // identified type and addr will be taken from our pointer. We assume that
                // tag map won't read memory at this pointer in order to identify its type, so we can call it with some dummy address:
                // so, lets store to our pointer temporary some valid, addr. We make it char, because we want the math in bytes
                char dummy = 0; metac_value_set_pointer(p->p_val, &dummy);

                // getting type of the object of the array we're pointing to
                metac_entry_t * p_allocating_entry = metac_entry_pointer_entry(metac_value_entry(p->p_val));
                if (p_allocating_entry == NULL) {
                    // it's void *, all we need is - to get type, we know everything else (but can verify on this end)
                    if (p_tag_map != NULL) {
                        metac_value_event_t ev = {.type = METAC_RQVST_pointer_array_count, .p_return_value = NULL};
                        metac_entry_tag_t * p_tag = metac_tag_map_tag(p_tag_map, metac_value_entry(p->p_val));
                        if (p_tag != NULL && p_tag->handler) {

                            if (metac_value_event_handler_call(p_tag->handler, p_iter, &_metac_deserialization_task_value_extractor, &ev, p_tag->p_context) != 0) {
                                return -(EFAULT);
                            }

                            if (ev.p_return_value == NULL) {
                                return -(EFAULT);
                            }

                            // extra check - if we got non flexible array, it must be size at least the same as json has
                            // if it's flexible - the size is based on json
                            if (!metac_value_element_count_flexible(ev.p_return_value)) {
                                p->allocated_el_number = metac_value_element_count(ev.p_return_value);
                                if (metac_value_final_kind(ev.p_return_value, NULL) != METAC_KND_array_type ||
                                    p->allocated_el_number < cJSON_GetArraySize(json)) {
                                    metac_value_delete(ev.p_return_value);
                                    return -(EFAULT);
                                }
                            }

                            // we don't allow container_of for array case (when count is bigger than 1)
                            char *p_addr = (char*)metac_value_addr(ev.p_return_value); 
                            assert(p_addr != NULL);
                            if (p_addr == NULL) {
                                metac_value_delete(ev.p_return_value);
                                return -(EFAULT);
                            }

                            p->allocated_prefix_size = 0;
                            if (p->allocated_el_number == 1) { // 1 element - special case - similar to object
                                assert(p_addr <= &dummy);
                                if (p_addr > &dummy) {
                                    metac_value_delete(ev.p_return_value);
                                    return -(EFAULT);
                                }
                                p->allocated_prefix_size = &dummy - p_addr;
                            } else { // we don't allow container_of in case of many objects
                                assert(p_addr == &dummy);
                                if (p_addr != &dummy) {
                                    metac_value_delete(ev.p_return_value);
                                    return -(EFAULT);
                                }
                            }
                            
                            p_allocating_entry = metac_entry_element_entry(metac_value_entry(ev.p_return_value));
                            p_arr_val = ev.p_return_value;
                        }
                    }
                } else {
                    // the way to create array from pointer knowing length of the array it points
                    // we trust json length here 
                    p_arr_val = metac_new_element_count_value(p->p_val, p->allocated_el_number);
                }

                // once called, we can put back NULL to our pointer
                metac_value_set_pointer(p->p_val, NULL);

                if (p_allocating_entry == NULL ||
                    metac_entry_byte_size(p_allocating_entry, &p->allocated_el_sz) != 0) {
                    metac_value_delete(p_arr_val);
                    return -(EFAULT);
                }

                // NOTE: if we're parsing array we assume that the elements don't use flexible. array impl checks for space
                // so it will fail in case json has some data for flexible
                p->allocated_flexible_el_number = 0;
                p->allocated_flexible_el_sz = 0;

                assert(
                    (p->allocated_el_number > 1 && p->allocated_prefix_size == 0  && p->allocated_flexible_el_number * p->allocated_flexible_el_sz == 0) ||
                    (p->allocated_el_number  == 1)
                );

                p->p_allocated = calloc_fn(
                    p->allocated_el_number, 
                    p->allocated_prefix_size + p->allocated_el_sz + p->allocated_flexible_el_number * p->allocated_flexible_el_sz);
                if (p->p_allocated == NULL) {
                    metac_value_delete(p_arr_val);
                    return -(EFAULT);
                }

                // NOTE: p_arr_val contains address of our dummy char, we can't use that - need to recreate as soon as we allocate mem
                metac_value_t * p_arr_val_new = metac_new_value(metac_value_entry(p_arr_val), p->p_allocated);
                metac_value_delete(p_arr_val);
                p_arr_val = p_arr_val_new;
                p_arr_val_new = NULL;
                if (p_arr_val == NULL) {
                    return 2; // cleanup and fail
                }
                
                int res = _metac_create_and_append_task(p_iter, p_arr_val, json, &p_arr_val);
                if (res != 0) {
                    if (p_arr_val) metac_value_delete(p_arr_val);
                    return res;
                }
                // schedule children deserialization
                return 1;
            }
            // nothing matched
            return -(EFAULT);
        }
        case 1: { // we returned after children finished
            void * addr = NULL;
            int counter = 0;
            while (metac_recursive_iterator_dep_queue_is_empty(p_iter) == 0) {
                metac_deserialization_task_t * p_task = NULL;
                if (counter++ != 0) return 2; // expect only 1 child
                metac_value_t * p_val_out = (metac_value_t *)metac_recursive_iterator_dequeue_and_delete_dep(p_iter, (void**)&p_task, NULL);
                if (p_task != NULL) {
                    if (p_task->p_val != NULL) {
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

    _metac_deserialization_task_cleanup_and_fail(p_iter);

    if (p->p_allocated != NULL) {
        free_fn(p->p_allocated);
        p->p_allocated = NULL;
    }

    return  -(EFAULT);
}

static int _metac_value_with_members_from_cjson(
    // iter param
    metac_recursive_iterator_t * p_iter,
    metac_deserialization_task_t * p,
    metac_kind_t final_kind,
    int state,
    // params
    metac_tag_map_t* p_tag_map) {
    struct cJSON * json = (struct cJSON *)p->p_external;
    switch (state) {
        case METAC_R_ITER_start: {
            if (!cJSON_IsObject(json)) {
                return -(EFAULT);
            }

            metac_num_t found_members_in_json = 0;
            // Struct deserialization succeeds as long as we could process all members
            // Individual field failures are silently ignored (partial deserialization)
            metac_num_t mcount = metac_value_member_count(p->p_val);
            for (metac_num_t i = 0; i < mcount; ++i) {

                metac_value_t* p_memb_val = metac_new_value_by_member_id(p->p_val, i);
                if (p_memb_val == NULL) {
                    return 2; // cleanup and failure
                }

                metac_name_t memb_name = metac_value_name_per_protocol(p_memb_val, "json", p_tag_map);//metac_value_name(p_memb_val);
                struct cJSON* memb_json = NULL;

                if (memb_name && memb_name[0] != '\0') {
                    // Named member - find by name in JSON object
                    memb_json = cJSON_GetObjectItemCaseSensitive(json, memb_name);
                } else {
                    // Anonymous member - handle nested struct specially
                    // Anonymous members inherit the parent JSON object
                    memb_json = json;
                }
                if (memb_name != NULL) {
                    free(memb_name);
                    memb_name = NULL;
                }

                if (memb_json) {
                    // make sure that there only 1 field in JSON which will be handled
                    // fail if there are many which match - otherwise it may be a vulnarability
                    if (final_kind == METAC_KND_union_type) {
                        if (found_members_in_json > 0) {
                            metac_value_delete(p_memb_val);
                            return 2; // cleanup and fail
                        }
                        ++found_members_in_json;
                    }

                    int res = _metac_create_and_append_task(p_iter, p_memb_val, memb_json, NULL);
                    if (res != 0) {
                        metac_value_delete(p_memb_val);
                        return res;
                    }
                } else {
                    metac_value_delete(p_memb_val);
                }
            }
            return 1; // next state
        }
        case 1: {
            return _metac_deserialization_task_dequeue_check_or_fail(p_iter, 2);
        }
        case 2: break;
    }
    return _metac_deserialization_task_cleanup_and_fail(p_iter);
}

static int _metac_value_with_elements_from_cjson(
    // iter param
    metac_recursive_iterator_t * p_iter,
    metac_deserialization_task_t * p,
    metac_kind_t final_kind,
    int state,
    // in_param
    metac_value_deserialization_mode_t * p_mode,
    // determine_flexible_sz case
    metac_size_t* p_flexible_el_number,
    metac_size_t* p_flexible_el_sz) {
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

            if (!metac_value_element_count_flexible(p->p_val)) {
                switch (p_mode->array_len_mode) {
                case METAC_DESER_array_len_allow_less:
                    if (json_count > count) {
                        return 2; // cleanup and failure
                    }
                    // break isn't put here purpously
                case METAC_DESER_array_len_ignore_extra:
                    count = json_count; // json may contain less - those elements will be init with zeros
                    break;
                case METAC_DESER_array_len_precise:
                default:
                    if (json_count != count) {
                        return 2; // cleanup and failure
                    }
                    break;
                }
            } else {
                // support flexible arrays (needs reallocation of allocated memory in hierarchy)
                count = json_count; // trust json - assume that it contains the needed number of elements
                p_non_flexible = metac_new_element_count_value(p->p_val, count);


                // support flexible estimate case - called from metac_value_from_cjson_determine_flexible_sz
                if (p_flexible_el_number != NULL) {
                    *p_flexible_el_number = count;
                }
                if (p_flexible_el_sz != NULL) {
                    if (metac_entry_byte_size(metac_value_entry(p_non_flexible), p_flexible_el_sz) !=0 ) {
                        metac_value_delete(p_non_flexible);
                        return 2; // cleanup and failure
                    }
                    metac_value_delete(p_non_flexible);
                }
                if (p_flexible_el_sz != NULL || p_flexible_el_number != NULL) {
                    return 0; // finish earlier - we don't need to do anything for this case
                }
                // end of flexible estimate case

                // otherwise (not flexible estimate case) verify that we have enough space  
                metac_size_t flexible_sz = 0;
                if (metac_entry_byte_size(metac_value_entry(p_non_flexible), &flexible_sz) !=0 ) {
                    metac_value_delete(p_non_flexible);
                    return 2; // cleanup and failure
                }
                // find in hierarchy if we need/can reallocate parent to fit this flexible array
                // see _metac_value_pointer_from_cjson to understand what and how was allocated
                metac_deserialization_task_t * p_tast_with_allocation = _metac_deserialization_task_find_task_with_allocation(p_iter);
                if (p_tast_with_allocation == NULL) {
                    // hierarchy wasn't allocated
                    metac_value_delete(p_non_flexible);
                    return 2; // cleanup and failure
                }
                if (flexible_sz > p_tast_with_allocation->allocated_flexible_el_number * p_tast_with_allocation->allocated_flexible_el_sz) {
                    metac_value_delete(p_non_flexible);
                    return 2; // cleanup and failure                    
                }
                // everything is good - we have enough space
            }
            // handle children
            for (metac_num_t i = 0; i < count; ++i) {
                metac_value_t* p_el_val = metac_new_value_by_element_id(p_local, i);
                struct cJSON* el_json = cJSON_GetArrayItem(json, i);
                if (p_el_val == NULL) {
                    metac_value_delete(p_non_flexible);
                    return 2; // cleanup and failure
                }
                if (el_json) {
                    int res = _metac_create_and_append_task(p_iter, p_el_val, el_json, NULL);
                    if (res != 0) {
                        metac_value_delete(p_el_val);
                        metac_value_delete(p_non_flexible);
                        return res;
                    }
                }
            }
            // if array was flexible and we created non-flexible value
            if (p_non_flexible) {
                metac_value_delete(p_non_flexible);
                p_local = p->p_val;
            }
            return 1; // next state
        }
        case 1: {
            return _metac_deserialization_task_dequeue_check_or_fail(p_iter, 2);
        }
        case 2: break;
    }
    return _metac_deserialization_task_cleanup_and_fail(p_iter);
}

/* default mode - failsafe */
static metac_value_deserialization_mode_t const _default_value_deserialization_mode = {
    .string_ptr_mode = METAC_DESER_string_ptr_deny,
    .flex_array_mode = METAC_FLXARR_fail,
    .union_mode = METAC_UNION_fail,
    .unknown_ptr_mode = METAC_UPTR_fail,
};

int metac_value_from_cjson_determine_flexible_sz(metac_value_t* p_val, struct cJSON* in_json,
    metac_value_deserialization_mode_t * p_mode,
    metac_tag_map_t* p_tag_map,
    metac_size_t* p_flexible_el_number,
    metac_size_t* p_flexible_el_sz) {

    // set defaults
    if (p_mode == NULL) {
        p_mode = (metac_value_deserialization_mode_t *)&_default_value_deserialization_mode;
    }
        
    metac_size_t flexible_el_number = 0;
    metac_size_t flexible_el_sz = 0;

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
            case METAC_KND_base_type:
            case METAC_KND_enumeration_type:
            case METAC_KND_pointer_type: {
                // don't go further for those kinds
                metac_recursive_iterator_done(p_iter, p->p_val);
                continue;
            }
            case METAC_KND_union_type:
            case METAC_KND_struct_type: {
                METAC_R_ITER_handle_state(p_iter, p->p_val, _metac_value_with_members_from_cjson(p_iter, p, final_kind, state, p_tag_map));
                continue;
            }
            case METAC_KND_array_type: {
                METAC_R_ITER_handle_state(p_iter, p->p_val, _metac_value_with_elements_from_cjson(p_iter, p, final_kind, state, p_mode, &flexible_el_number, &flexible_el_sz));
                continue;
            }
            default: {
                metac_recursive_iterator_fail(p_iter);
                continue;
            }
        }
    }
    
    int fail = 0;
    metac_recursive_iterator_get_out(p_iter, (void **)&p_task, &fail);

    // return determined flexible params if not failed
    if (fail == 0) {
        if (p_flexible_el_number != NULL) {
            *p_flexible_el_number = flexible_el_number;
        }
        if (p_flexible_el_sz != NULL) {
            *p_flexible_el_sz = flexible_el_sz;
        }
    }
    metac_deserialization_task_delete(p_task);
    metac_recursive_iterator_free(p_iter);

    return fail;
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
    if (p_mode == NULL) {
        p_mode = (metac_value_deserialization_mode_t *)&_default_value_deserialization_mode;
    }
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
                METAC_R_ITER_handle_state(p_iter, p->p_val, metac_value_base_type_from_cjson(p->p_val, json));
                continue;
            }
            case METAC_KND_enumeration_type: {
                METAC_R_ITER_handle_state(p_iter, p->p_val, metac_value_enumeration_type_from_cjson(p->p_val, json));
                continue;
            }
            case METAC_KND_pointer_type: {
                METAC_R_ITER_handle_state(p_iter, p->p_val, _metac_value_pointer_from_cjson(p_iter, p, final_kind, state, p_mode, calloc_fn, free_fn, p_tag_map));
                continue; 
            }
            case METAC_KND_union_type:
            case METAC_KND_struct_type: {
                METAC_R_ITER_handle_state(p_iter, p->p_val, _metac_value_with_members_from_cjson(p_iter, p, final_kind, state, p_tag_map));
                continue;
            }
            case METAC_KND_array_type: {
                METAC_R_ITER_handle_state(p_iter, p->p_val, _metac_value_with_elements_from_cjson(p_iter, p, final_kind, state, p_mode, NULL, NULL));
                continue;
            }
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
