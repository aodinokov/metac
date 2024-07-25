#include "metac/reflect.h"
#include "metac/backend/string.h"
#include "metac/backend/iterator.h"
#include "metac/backend/value.h"

#include <assert.h>

#ifndef dsprintf
dsprintf_render_with_buf(64)
#define dsprintf(_x_...) dsprintf64( _x_)
#endif

char * metac_value_string_ex(metac_value_t * p_val, metac_value_walk_mode_t wmode, metac_tag_map_t * p_tag_map) {
    if (p_val == NULL) {
        return NULL;
    }

    metac_recursive_iterator_t * p_iter = metac_new_recursive_iterator(p_val);

    for (metac_value_t * p = (metac_value_t *)metac_recursive_iterator_next(p_iter); p != NULL;
        p = (metac_value_t *)metac_recursive_iterator_next(p_iter)) {
        int state = metac_recursive_iterator_get_state(p_iter);

        metac_kind_t final_kind;
        if (metac_value_kind(p) != METAC_KND_subprogram_parameter || metac_value_has_load_of_parameter(p) == 0) {
            final_kind = metac_value_final_kind(p, NULL);
        } else {
            final_kind = METAC_KND_subprogram_parameter;    // unspecified and va_arg;
        }
         
        switch(final_kind) {
        case METAC_KND_base_type: {
            char * out = metac_value_base_type_string(p);
            if (out == NULL) {
                metac_recursive_iterator_fail(p_iter);
                continue;  
            }
            metac_recursive_iterator_done(p_iter, out);
            continue;
        }
        case METAC_KND_enumeration_type: {
            char * out = metac_value_enumeration_string(p);
            if (out == NULL) {
                metac_recursive_iterator_fail(p_iter);
                continue;  
            }
            metac_recursive_iterator_done(p_iter, out);
            continue;
        }
        case METAC_KND_pointer_type: {
            switch (wmode) {
                case METAC_WMODE_shallow: {
                    char * out = metac_value_pointer_string(p);
                    if (out == NULL) {
                        metac_recursive_iterator_fail(p_iter);
                        continue;
                    }
                    metac_recursive_iterator_done(p_iter, out);
                    continue;
                }
                case METAC_WMODE_deep: {
                    switch(state) {
                        case METAC_R_ITER_start: {
                            void *p_addr = NULL;
                            if (metac_value_pointer(p, &p_addr) != 0) {
                                metac_recursive_iterator_fail(p_iter);
                                continue;
                            }
                            if (p_addr == NULL) {
                                char * out = metac_value_pointer_string(p);
                                if (out == NULL) {
                                    metac_recursive_iterator_fail(p_iter);
                                    continue;
                                }
                                metac_recursive_iterator_done(p_iter, out);
                                continue;
                            }
                            // check pointer destination for cycles, fail if we already met that pointer
                            if (metac_value_level_introduced_loop(p_iter) > 0) { 
                                // TODO: we can output comment "/*loop &<add here string fom the top till looping element>*/"
                                /* actually we may even not commit. e.g. the following works ok:
                                struct _list_itm {
                                    int data;
                                    struct t8_list_itm * next;   
                                } *_head = (struct _list_itm []){{.data = 0, .next = (struct _list_itm []){{.data = 1, .next = _head->next,},},},};
                                potentially containerof also works to some extend. if we don't care about structure, we can point to the field
                                struct _list_itm {
                                    int data;
                                    struct t8_list_itm * next;   
                                } *_head = (struct _list_itm []){{.data = 0, .next = &((struct _list_itm []){{.data = 1, .next = NULL,},})[0].next,},};
                                ignore them as an option*/
                                metac_recursive_iterator_fail(p_iter);
                                continue;                                    
                            }

                            /* we need to convert pointer to array (can be flexible), try handler first */
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
                                /* check if handler created object with differnt address
                                   this is a valid scenario (e.g. containerof), but we can't use it in cinit; */
                                if (p_arr_val != NULL) {
                                    if (metac_value_addr(p_arr_val) != p_addr) {
                                        metac_value_delete(p_arr_val);
                                        metac_recursive_iterator_fail(p_iter);
                                        continue;      
                                    }
                                }
                            }
                            if (p_arr_val == NULL && metac_value_is_void_pointer(p)) {
                                /* if p is void * we won't be able to do anything - there are no arrays of void. fallback to shallow */
                                char * out = metac_value_pointer_string(p);
                                if (out == NULL) {
                                    metac_recursive_iterator_fail(p_iter);
                                    continue;
                                }
                                metac_recursive_iterator_done(p_iter, out);
                                continue;
                            }
                            if (p_arr_val == NULL) {
                                p_arr_val = metac_new_element_count_value(p, 1); /*create arr with len 1 - good default */
                            }
                            if (p_arr_val == NULL) {
                                metac_recursive_iterator_fail(p_iter);
                                continue;
                            }
                            if (metac_value_has_elements(p_arr_val) == 0) {
                                metac_value_delete(p_arr_val);
                                metac_recursive_iterator_fail(p_iter);
                                continue;                        
                            }
                            /* try to process everything as array */
                            metac_recursive_iterator_create_and_append_dep(p_iter, p_arr_val);
                            metac_recursive_iterator_set_state(p_iter, 1);
                            continue;
                        }
                        case 1: {
                            metac_value_t * p_arr_val;
                            char * arr_out = metac_recursive_iterator_dequeue_and_delete_dep(p_iter, (void**)&p_arr_val, NULL);
                            assert(p_arr_val != NULL);
                            if (arr_out == NULL) {
                                metac_value_delete(p_arr_val);
                                metac_recursive_iterator_fail(p_iter);
                                continue;
                            }
                            if (p_arr_val == NULL) {
                                free(arr_out);
                                metac_recursive_iterator_fail(p_iter);
                                continue;  
                            }
                            // check underlaying types and prepend (<type>[]);
                            metac_value_t * p_flex_arr_val = metac_new_element_count_value(p_arr_val, -1);
                            if (p_flex_arr_val == NULL) {
                                metac_value_delete(p_arr_val);
                                free(arr_out);
                                metac_recursive_iterator_fail(p_iter);
                                continue;   
                            }
                            metac_value_delete(p_arr_val);

                            char * p_flex_arr_type_pattern = metac_entry_cdecl(metac_value_entry(p_flex_arr_val)); /*contains %s on var name place*/
                            metac_value_delete(p_flex_arr_val);
                            if (p_flex_arr_type_pattern == NULL) {
                                free(arr_out);
                                metac_recursive_iterator_fail(p_iter);
                                continue;
                            }
                            char * p_flex_arr_type = dsprintf(p_flex_arr_type_pattern, "");
                            free(p_flex_arr_type_pattern);
                            if (p_flex_arr_type == NULL) {
                                free(arr_out);
                                metac_recursive_iterator_fail(p_iter);
                                continue;
                            }
                            char *out = dsprintf("(%s)%s", p_flex_arr_type, arr_out);
                            free(arr_out);
                            free(p_flex_arr_type);
                            if (out == NULL) {
                                metac_recursive_iterator_fail(p_iter);
                                continue;  
                            }
                            metac_recursive_iterator_done(p_iter, out);
                            continue;
                        }
                    }
                }
                default: {
                    /*not supported */
                    metac_recursive_iterator_fail(p_iter);
                    continue;
                }
            }
        }
        //case METAC_KND_class_type: 
        case METAC_KND_union_type:
        case METAC_KND_struct_type: {
            switch(state) {
                case METAC_R_ITER_start: {
                    metac_flag_t failure = 0;
                    assert(metac_value_has_members(p) != 0);

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
                                    }
                                }
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
                        }
                    }
                    if (failure != 0) {
                        metac_recursive_iterator_set_state(p_iter, 2);  // failure cleanup
                        continue;  
                    }
                    metac_recursive_iterator_set_state(p_iter, 1);
                    continue;
                }
                case 1: {
                    metac_flag_t failure = 0;
                    char * out = NULL;

                    /* build center part .<name> = out*/
                    while(metac_recursive_iterator_dep_queue_is_empty(p_iter) == 0) {
                        metac_value_t * p_memb_val;
                        char * memb_out = metac_recursive_iterator_dequeue_and_delete_dep(p_iter, (void**)&p_memb_val, NULL);
                        assert(p_memb_val != NULL);

                        metac_name_t memb_name = metac_value_name(p_memb_val);
                        metac_value_delete(p_memb_val);

                        if (memb_out == NULL) {
                            failure = 1;
                            break;
                        }

                        if (strcmp(memb_out, "") == 0) { /* skip empty results */
                            free(memb_out);
                            continue;
                        }

                        char *prev_out = out;
                        if (memb_name == NULL) {
                            /*special case - anonymous struct or union member, just add as - is*/
                            out = dsprintf("%s%s%s",
                                prev_out == NULL?"":prev_out, /*must include , at the end */
                                prev_out == NULL?"":" ",
                                memb_out
                            );
                        } else {
                            out = dsprintf("%s%s.%s = %s,",
                                prev_out == NULL?"":prev_out, /*must include , at the end */
                                prev_out == NULL?"":" ",
                                memb_name,
                                memb_out
                            );
                        }
                        if (prev_out) {
                            free(prev_out);
                        }
                        free(memb_out);
                        if (out == NULL) {
                            failure = 1;
                            break; 
                        }
                        
                    }
                    if (failure != 0) {
                        if (out != NULL) {
                            free(out);
                        }
                        metac_recursive_iterator_set_state(p_iter, 2); // failure cleanup
                        continue;
                    }
                    if (out == NULL) { /*empty string, or union without handler*/
                        out = dsprintf("");
                        if (out == NULL) {
                            metac_recursive_iterator_fail(p_iter);
                            continue;                  
                        }
                    }
                    // out contains internal thing.
                    if (metac_value_kind(p) == METAC_KND_member && 
                        metac_value_name(p) == NULL) {
                        /*special case - anonymous struct or union member, don't wrap with {} */
                        metac_recursive_iterator_done(p_iter, out);
                        continue;
                    }
                    char *prev_out = out;
                    out = dsprintf("{%s}", prev_out);
                    free(prev_out);
                    if (out == NULL) {
                        metac_recursive_iterator_done(p_iter, out);
                        continue;                        
                    }
                    metac_recursive_iterator_done(p_iter, out);
                    continue;
                }
                case 2: 
                default: {
                    /* failure cleanup*/
                    while(metac_recursive_iterator_dep_queue_is_empty(p_iter) == 0) {
                        metac_value_t * p_memb_val;
                        char * memb_out = metac_recursive_iterator_dequeue_and_delete_dep(p_iter, (void**)&p_memb_val, NULL);
                        if (memb_out != NULL) {
                            free(memb_out);
                        }
                        if (p_memb_val != NULL) {
                            metac_value_delete(p_memb_val);
                        }
                    }
                    metac_recursive_iterator_fail(p_iter);
                    continue;
                }
            }
        }
        case METAC_KND_array_type: {
            switch(state) {
                case METAC_R_ITER_start: {
                    metac_flag_t failure = 0;
                    assert(metac_value_has_elements(p) != 0);

                    metac_value_t * p_local = p;
                    metac_value_t * p_non_flexible = NULL;
                    if (metac_value_element_count_flexible(p) != 0) {
                        if (p_tag_map != NULL) {
                            metac_value_event_t ev = {.type = METAC_RQVST_flex_array_count, .p_return_value = NULL};
                            metac_entry_tag_t * p_tag = metac_tag_map_tag(p_tag_map, metac_value_entry(p));
                            if (p_tag != NULL && p_tag->handler) {
                                if (metac_value_event_handler_call(p_tag->handler, p_iter, &ev, p_tag->p_context) != 0) {
                                    metac_recursive_iterator_fail(p_iter);
                                    continue;
                                }
                                p_non_flexible = ev.p_return_value;
                            }
                            if (p_non_flexible == NULL) { /* this is a way to skip that item */
                                char * out = dsprintf("");
                                if (out == NULL) {
                                    metac_recursive_iterator_fail(p_iter);
                                    continue;
                                }
                                metac_recursive_iterator_done(p_iter, out);
                                continue;
                            }
                        } else {
                            p_non_flexible = metac_new_element_count_value(p_local, 0); /* this will generate {} - safe default */
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
                    }
                    if (p_non_flexible) {
                        metac_value_delete(p_non_flexible);
                        p_local = p;
                    }
                    if (failure != 0) {
                        metac_recursive_iterator_set_state(p_iter, 2);  /* failure cleanup */
                        continue;  
                    }
                    metac_recursive_iterator_set_state(p_iter, 1);
                    continue;
                }
                case 1: {
                    metac_flag_t failure = 0;
                    char * out = NULL;

                    /* build center part .<name> = out*/
                    while(metac_recursive_iterator_dep_queue_is_empty(p_iter) == 0) {
                        metac_value_t * p_el_val;
                        char * el_out = metac_recursive_iterator_dequeue_and_delete_dep(p_iter, (void**)&p_el_val, NULL);
                        assert(p_el_val != NULL);

                        metac_name_t memb_name = metac_value_name(p_el_val);
                        metac_value_delete(p_el_val);

                        if (el_out == NULL) {
                            failure = 1;
                            break;
                        }

                        char *prev_out = out;
                        out = dsprintf("%s%s%s,",
                            prev_out == NULL?"":prev_out, /*must include , at the end */
                            prev_out == NULL?"":" ",
                            el_out
                        );
                        if (prev_out) {
                            free(prev_out);
                        }
                        free(el_out);
                        if (out == NULL) {
                            failure = 1;
                            break; 
                        }
                        
                    }
                    if (failure != 0) {
                        if (out != NULL) {
                            free(out);
                        }
                        metac_recursive_iterator_set_state(p_iter, 2); // failure cleanup
                        continue;
                    }
                    char *prev_out = out;
                    out = dsprintf("{%s}",
                        prev_out==NULL?"":prev_out
                    );
                    if (prev_out) {
                        free(prev_out);
                    }
                    metac_recursive_iterator_done(p_iter, out);
                    continue;
                }
                case 2: 
                default: {
                    /* failure cleanup*/
                    while(metac_recursive_iterator_dep_queue_is_empty(p_iter) == 0) {
                        metac_value_t * p_el_val;
                        char * el_out = metac_recursive_iterator_dequeue_and_delete_dep(p_iter, (void**)&p_el_val, NULL);
                        if (el_out != NULL) {
                            free(el_out);
                        }
                        if (p_el_val != NULL) {
                            metac_value_delete(p_el_val);
                        }
                    }
                    metac_recursive_iterator_fail(p_iter);
                    continue; 
                }
            }
        }
        case METAC_KND_subprogram_parameter:// this is only it's unspecified param // TODO: we need also va_arg here
        //case METAC_KND_subroutine_type:
        case METAC_KND_subprogram: {
            // TODO: support arguments
            switch(state) {
                case METAC_R_ITER_start: {
                    metac_flag_t failure = 0;
                    assert(metac_value_has_load_of_parameter(p) != 0);

                    metac_num_t mcount = metac_value_load_of_parameter_count(p);
                    for (metac_num_t i = 0; i < mcount; ++i) {
                        metac_value_t * p_param_val = metac_value_load_of_parameter_value(p, i);
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
                    metac_recursive_iterator_set_state(p_iter, 1);
                    continue;
                }
                case 1: {
                    metac_flag_t failure = 0;
                    char * out = NULL;

                    /* build center part .<name> = out*/
                    while(metac_recursive_iterator_dep_queue_is_empty(p_iter) == 0) {
                        metac_value_t * p_param_val;
                        char * param_out = metac_recursive_iterator_dequeue_and_delete_dep(p_iter, (void**)&p_param_val, NULL);
                        assert(p_param_val != NULL);

                        if (param_out == NULL) {
                            failure = 1;
                            break;
                        }

                        char * param_cdecl = NULL;
                        if (metac_value_kind(p_param_val) == METAC_KND_subprogram_parameter) {
                            // TODO: skip this for ...
                            param_cdecl = metac_entry_cdecl(metac_entry_parameter_entry(metac_value_entry(p_param_val)));
                        } else {
                            param_cdecl = metac_entry_cdecl(metac_value_entry(p_param_val));
                        }
                        if (param_cdecl == NULL) {
                            free(param_out);
                            failure = 1;
                            break;
                        }

                        char * param_name = NULL;
                        if (metac_value_kind(p_param_val) == METAC_KND_subprogram_parameter) {
                            param_name = metac_entry_name(metac_value_entry(p_param_val));
                        }
                        char * param = dsprintf(param_cdecl,
                            param_name == NULL?"":param_name);
                        free(param_cdecl);
                        if (param == NULL) {
                            free(param_out);
                            failure = 1;
                            break;
                        }
                        
                        char *prev_out = out;
                        out = dsprintf("%s%s%s%s%s",
                            prev_out == NULL?"":prev_out, /*must include , at the end */
                            prev_out == NULL?"":", ",
                            param,
                            param_name == NULL?" ":" = ",
                            param_out
                        );
                        if (prev_out) {
                            free(prev_out);
                        }
                        free(param);
                        free(param_out);
                        if (out == NULL) {
                            failure = 1;
                            break; 
                        }
                    }
                    if (failure != 0) {
                        if (out != NULL) {
                            free(out);
                        }
                        metac_recursive_iterator_set_state(p_iter, 2); // failure cleanup
                        continue;
                    }

                    if (final_kind == METAC_KND_subprogram_parameter) {
                        metac_recursive_iterator_done(p_iter, out);
                        continue;
                    }

                    char *prev_out = out;
                    out = dsprintf("%s(%s)",
                        metac_entry_name(metac_value_entry(p)),
                        prev_out==NULL?"":prev_out
                    );
                    if (prev_out) {
                        free(prev_out);
                    }
                    metac_recursive_iterator_done(p_iter, out);
                    continue;
                }
                case 2: 
                default: {
                    /* failure cleanup*/
                    while(metac_recursive_iterator_dep_queue_is_empty(p_iter) == 0) {
                        metac_value_t * p_param_val;
                        char * el_out = metac_recursive_iterator_dequeue_and_delete_dep(p_iter, (void**)&p_param_val, NULL);
                        if (el_out != NULL) {
                            free(el_out);
                        }
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
    char * res = metac_recursive_iterator_get_out(p_iter, NULL, NULL);
    metac_recursive_iterator_free(p_iter);

    return res;
}