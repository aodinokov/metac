#include "metac/serialization/cjson.h"
#include "metac/backend/iterator.h"
#include "metac/backend/value.h"

#include <cjson/cJSON.h>
#include <assert.h>

// Helper function to convert a base type value to a cJSON object.
static cJSON* metac_value_base_type_to_cjson(metac_value_t* p_val) {
    if (metac_value_is_bool(p_val)) {
        bool v;
        metac_value_bool(p_val, &v);
        return cJSON_CreateBool(v);
    }
    // TODO: if we want TAGS to define this behavior
    // if (metac_value_is_char(p_val)) {
    //     char v;
    //     metac_value_char(p_val, &v);
    //     char str[2] = {v, 0};
    //     return cJSON_CreateString(str);
    // }
    if (metac_value_is_float(p_val) || metac_value_is_double(p_val) || metac_value_is_ldouble(p_val)) {
        double v;
        metac_value_double(p_val, &v);
        return cJSON_CreateNumber(v);
    }
    // Default to handling as a number. This covers short, int, long, long long, and their unsigned variants.
    metac_num_t num;
    if (metac_value_num(p_val, &num) == 0) {
        return cJSON_CreateNumber((double)num);
    }

    return NULL; // Should not happen for a valid base type
}


struct cJSON* metac_value_to_cjson(metac_value_t* p_val, metac_value_walk_mode_t wmode, metac_tag_map_t* p_tag_map) {
    if (p_val == NULL) {
        return cJSON_CreateNull();
    }

    metac_recursive_iterator_t* p_iter = metac_new_recursive_iterator(p_val);

    for (metac_value_t* p = (metac_value_t*)metac_recursive_iterator_next(p_iter); p != NULL;
         p = (metac_value_t*)metac_recursive_iterator_next(p_iter)) {
        int state = metac_recursive_iterator_get_state(p_iter);
        metac_kind_t final_kind = metac_value_final_kind(p, NULL);

        switch (final_kind) {
            case METAC_KND_base_type: {
                cJSON* out = metac_value_base_type_to_cjson(p);
                if (out == NULL) {
                    metac_recursive_iterator_fail(p_iter);
                    continue;
                }
                metac_recursive_iterator_done(p_iter, out);
                continue;
            }
            case METAC_KND_enumeration_type: {
                char* enum_str = metac_value_enumeration_string(p);
                if (enum_str == NULL) {
                    metac_recursive_iterator_fail(p_iter);
                    continue;
                }
                cJSON* out = cJSON_CreateString(enum_str);
                free(enum_str);
                metac_recursive_iterator_done(p_iter, out);
                continue;
            }
            case METAC_KND_pointer_type: {
                if (wmode == METAC_WMODE_shallow) {
                    void* ptr_addr = NULL;
                    metac_value_pointer(p, &ptr_addr);
                     if (ptr_addr == NULL) {
                        metac_recursive_iterator_done(p_iter, cJSON_CreateNull());
                    } else {
                        char buffer[32];
                        snprintf(buffer, sizeof(buffer), "%p", ptr_addr);
                        metac_recursive_iterator_done(p_iter, cJSON_CreateString(buffer));
                    }
                    continue;
                }

                // Deep mode from here
                switch(state) {
                    case METAC_R_ITER_start: {
                        void* p_addr = NULL;
                        if (metac_value_pointer(p, &p_addr) != 0) {
                            metac_recursive_iterator_fail(p_iter);
                            continue;
                        }
                        if (p_addr == NULL) {
                            metac_recursive_iterator_done(p_iter, cJSON_CreateNull());
                            continue;
                        }
                        if (metac_value_level_introduced_loop(p_iter) > 0) {
                            cJSON* err_obj = cJSON_CreateObject();
                            cJSON_AddStringToObject(err_obj, "$error", "circular reference");
                            metac_recursive_iterator_done(p_iter, err_obj);
                            continue;
                        }

                        // For now, assume pointer to single item. Tag map handling will come later.
                        metac_value_t * p_arr_val = metac_new_element_count_value(p, 1);
                        if (p_arr_val == NULL) {
                            metac_recursive_iterator_fail(p_iter);
                            continue;
                        }
                        metac_recursive_iterator_create_and_append_dep(p_iter, p_arr_val);
                        metac_recursive_iterator_set_state(p_iter, 1);
                        continue;
                    }
                    case 1: {
                        metac_value_t * p_arr_val;
                        cJSON* arr_json = (cJSON*)metac_recursive_iterator_dequeue_and_delete_dep(p_iter, (void**)&p_arr_val, NULL);
                        metac_value_delete(p_arr_val);

                        if (arr_json == NULL || !cJSON_IsArray(arr_json) || cJSON_GetArraySize(arr_json) != 1) {
                            if (arr_json) cJSON_Delete(arr_json);
                            metac_recursive_iterator_fail(p_iter);
                            continue;
                        }
                        // Detach the item from the temporary array.
                        cJSON* item = cJSON_DetachItemFromArray(arr_json, 0);
                        cJSON_Delete(arr_json);

                        metac_recursive_iterator_done(p_iter, item);
                        continue;
                    }
                }
                continue;
            }
            case METAC_KND_union_type:
            case METAC_KND_struct_type: {
                switch (state) {
                    case METAC_R_ITER_start: {
                        if (final_kind == METAC_KND_union_type) {
                            if (p_tag_map != NULL) {
                                metac_value_event_t ev = {.type = METAC_RQVST_union_member, .p_return_value = NULL};
                                metac_entry_tag_t * p_tag = metac_tag_map_tag(p_tag_map, metac_value_entry(p));
                                if (p_tag != NULL && p_tag->handler) {
                                    if (metac_value_event_handler_call(p_tag->handler, p_iter, &ev, p_tag->p_context) == 0 && ev.p_return_value != NULL) {
                                        metac_recursive_iterator_create_and_append_dep(p_iter, ev.p_return_value);
                                    }
                                }
                            }
                        } else { // struct
                            metac_num_t mcount = metac_value_member_count(p);
                            for (metac_num_t i = 0; i < mcount; ++i) {
                                metac_value_t* p_memb_val = metac_new_value_by_member_id(p, i);
                                if (p_memb_val == NULL) {
                                    metac_recursive_iterator_set_state(p_iter, 2); // cleanup
                                    continue;
                                }
                                metac_recursive_iterator_create_and_append_dep(p_iter, p_memb_val);
                            }
                        }
                        metac_recursive_iterator_set_state(p_iter, 1);
                        continue;
                    }
                    case 1: {
                        cJSON* obj = cJSON_CreateObject();
                        if (obj == NULL) {
                             metac_recursive_iterator_set_state(p_iter, 2); // cleanup
                             continue;
                        }

                        while (metac_recursive_iterator_dep_queue_is_empty(p_iter) == 0) {
                            metac_value_t* p_memb_val;
                            cJSON* memb_json = (cJSON*)metac_recursive_iterator_dequeue_and_delete_dep(p_iter, (void**)&p_memb_val, NULL);

                            metac_name_t memb_name = metac_value_name(p_memb_val);
                            metac_value_delete(p_memb_val);

                            if (memb_json == NULL) {
                                cJSON_Delete(obj);
                                metac_recursive_iterator_set_state(p_iter, 2); // cleanup
                                continue;
                            }
                            
                            if (memb_name) {
                                cJSON_AddItemToObject(obj, memb_name, memb_json);
                            } else {
                                cJSON_Delete(memb_json);
                            }
                        }
                        metac_recursive_iterator_done(p_iter, obj);
                        continue;
                    }
                    case 2: // Failure cleanup
                    default: {
                        while (metac_recursive_iterator_dep_queue_is_empty(p_iter) == 0) {
                            metac_value_t* p_memb_val;
                            cJSON* memb_json = (cJSON*)metac_recursive_iterator_dequeue_and_delete_dep(p_iter, (void**)&p_memb_val, NULL);
                             if (memb_json != NULL) {
                                cJSON_Delete(memb_json);
                            }
                            if (p_memb_val != NULL) {
                                metac_value_delete(p_memb_val);
                            }
                        }
                        metac_recursive_iterator_fail(p_iter);
                        continue;
                    }
                }
                continue;
            }
            case METAC_KND_array_type: {
                switch (state) {
                    case METAC_R_ITER_start: {
                        metac_num_t ecount = metac_value_element_count(p);
                        for (metac_num_t i = 0; i < ecount; ++i) {
                            metac_value_t* p_el_val = metac_new_value_by_element_id(p, i);
                            if (p_el_val == NULL) {
                                metac_recursive_iterator_set_state(p_iter, 2); // cleanup
                                continue;
                            }
                            metac_recursive_iterator_create_and_append_dep(p_iter, p_el_val);
                        }
                        metac_recursive_iterator_set_state(p_iter, 1);
                        continue;
                    }
                    case 1: {
                        cJSON* arr = cJSON_CreateArray();
                        if (arr == NULL) {
                            metac_recursive_iterator_set_state(p_iter, 2); // cleanup
                            continue;
                        }

                        while (metac_recursive_iterator_dep_queue_is_empty(p_iter) == 0) {
                            metac_value_t* p_el_val;
                            cJSON* el_json = (cJSON*)metac_recursive_iterator_dequeue_and_delete_dep(p_iter, (void**)&p_el_val, NULL);
                            
                            metac_value_delete(p_el_val);

                            if (el_json == NULL) {
                                cJSON_Delete(arr);
                                metac_recursive_iterator_set_state(p_iter, 2); // cleanup
                                continue;
                            }
                            cJSON_AddItemToArray(arr, el_json);
                        }
                        metac_recursive_iterator_done(p_iter, arr);
                        continue;
                    }
                    case 2: // Failure cleanup
                    default: {
                        while (metac_recursive_iterator_dep_queue_is_empty(p_iter) == 0) {
                            metac_value_t* p_el_val;
                            cJSON* el_json = (cJSON*)metac_recursive_iterator_dequeue_and_delete_dep(p_iter, (void**)&p_el_val, NULL);
                            if (el_json != NULL) {
                                cJSON_Delete(el_json);
                            }
                            if (p_el_val != NULL) {
                                metac_value_delete(p_el_val);
                            }
                        }
                        metac_recursive_iterator_fail(p_iter);
                        continue;
                    }
                }
                continue;
            }
            // TODO: Handle other kinds like unions, etc.
            default: {
                metac_recursive_iterator_fail(p_iter);
                continue;
            }
        }
    }

    cJSON* result = (cJSON*)metac_recursive_iterator_get_out(p_iter, NULL, NULL);
    metac_recursive_iterator_free(p_iter);

    return result;
}
