#ifndef PRINT_ARGS_H_
#define PRINT_ARGS_H_

#include "metac/reflect.h"
#include "metac/backend/iterator.h"
#include "metac/backend/value.h"
#include "mr.h"

static int _process_unspecified_params(
    //context
    metac_parameter_storage_t * p_param_storage,
    metac_value_t * p_val,
    metac_entry_t *p_param_entry,
    metac_tag_map_t * p_tag_map,
    metac_num_t param_id, 
    // and all params
    int n, ...) {

    metac_recursive_iterator_t * p_iter = metac_new_recursive_iterator(p_val);

    struct va_list_container cntr = {};
    va_start(cntr.parameters, p_tag_map);
    
    metac_value_t * p = (metac_value_t *)metac_recursive_iterator_next(p_iter); /* value itself */
    metac_entry_tag_t * p_tag = metac_tag_map_tag(p_tag_map, p_param_entry);
    if (p_tag != NULL && p_tag->handler) {
        metac_value_event_t ev = {
            .type = METAC_RQVST_va_list,
            .va_list_param_id = param_id,  /* TODO: to remove?*/
            .p_return_value = metac_parameter_storage_new_param_value(p_param_storage, param_id),
            .p_va_list_container = &cntr,
        };
        if (ev.p_return_value == NULL) {
            metac_recursive_iterator_fail(p_iter);
        } else {
            if (metac_value_event_handler_call(p_tag->handler, p_iter, &ev, p_tag->p_context) != 0) {
            }
            metac_value_delete(ev.p_return_value);
            metac_recursive_iterator_done(p_iter, NULL);
        }
    } else {
        metac_recursive_iterator_fail(p_iter);
    }

    va_end(cntr.parameters);

    int failed = 0;
    metac_recursive_iterator_get_out(p_iter, NULL, &failed);
    metac_recursive_iterator_free(p_iter);
    return failed;
}

#define _APPEND_PARAM_2(_NEXT_, _N_, args...) if (failure == 0) { \
        metac_entry_t *p_param_entry = metac_entry_by_paremeter_id(p_val_entry, param_id); \
        if (metac_entry_is_unspecified_parameter(p_param_entry) == 0 && metac_entry_is_va_list_parameter(p_param_entry) == 0) { \
            /* normal argument */ \
            typeof(MR_FIRST(args)) _x_val = MR_FIRST(args); \
            metac_entry_t *p_param_entry = metac_entry_by_paremeter_id(p_val_entry, param_id); \
            metac_size_t param_entry_byte_size = 0; \
            if (metac_entry_byte_size(p_param_entry, &param_entry_byte_size) != 0) { \
                printf("param %d metac_entry_byte_size failed\n", param_id); \
                failure = 1; \
                break; \
            } \
            if (metac_parameter_storage_append_by_buffer(p_param_storage, p_param_entry, param_entry_byte_size) == 0) { \
                metac_value_t * p_param_value = metac_parameter_storage_new_param_value(p_param_storage, param_id); \
                if (p_param_value != NULL) { \
                if (param_entry_byte_size != sizeof(_x_val)) { \
                    printf("param %d got sz %d, expectect sz %d\n", param_id, (int)sizeof(_x_val), (int)param_entry_byte_size); \
                    /*TODO: - handle that */ \
                } \
                    memcpy(metac_value_addr(p_param_value), &_x_val, param_entry_byte_size); \
                    metac_value_delete(p_param_value); \
                } \
            } \
        } else if (metac_entry_is_va_list_parameter(p_param_entry) != 0) { \
        } else if (metac_entry_is_unspecified_parameter(p_param_entry) != 0) { \
            if (metac_parameter_storage_append_by_parameter_storage(p_param_storage, p_param_entry) != 0) { \
                failure = 5; \
                break; \
            } \
            if (_process_unspecified_params(p_param_storage, p_val, p_param_entry, p_tag_map, param_id, _N_ , args) != 0) { \
                failure = 4; \
                break; \
            } \
            break; \
        } else { \
            failure = 3; \
            break; \
        } \
        if (failure == 0) { \
            ++param_id; \
            _NEXT_ \
        } \
    }


// this gets called in the context where p_param_storage is declared
#define _WRAP(_tag_map_, _fn_, _args_...) ({ \
        metac_tag_map_t * p_tag_map = _tag_map_; \
        metac_entry_t *p_val_entry = METAC_GSYM_LINK_ENTRY(_fn_); \
        metac_value_t * p_val =  metac_new_value(p_val_entry, p_param_storage); \
        metac_flag_t failure = 0; \
        metac_num_t param_id = 0; \
        /* append params*/ \
        do { \
            MR_FOREACH_EX(_APPEND_PARAM_2, _args_) \
        } while(0); \
        if (failure != 0) { \
            printf("failure %d\n", failure); \
            /* TODO: */ \
        } \
        p_val; \
    })

// alternative implementation
#define METAC_WRAP_FN_RES(_tag_map_, _fn_, _args_...) ({ \
        metac_value_t * p_val = NULL; \
        metac_parameter_storage_t * p_param_storage = metac_new_parameter_storage(); \
        if (p_param_storage != NULL) { \
            p_val = _WRAP(_tag_map_, _fn_, _args_); \
        } \
        if (p_val != NULL) { \
            char * s = metac_value_string_ex(p_val, METAC_WMODE_deep, _tag_map_); \
            if (s != NULL) { \
                printf("Calling %s\n", s); \
                free(s); \
            } \
        } \
        WITH_METAC_DECLLOC(loc, typeof(_fn_(_args_)) res = _fn_(_args_)); \
        if (p_val != NULL) { \
            metac_value_t *p_res_val = METAC_VALUE_FROM_DECLLOC(loc, res); \
            char * s = metac_value_string_ex(p_val, METAC_WMODE_deep, _tag_map_); \
            char * s_res = NULL; \
            if (p_res_val != NULL) { \
               s_res = metac_value_string_ex(p_res_val, METAC_WMODE_deep, _tag_map_); \
               metac_value_delete(p_res_val); \
            } \
            if (s != NULL && s_res != NULL) { \
                printf("Returned %s from %s\n", s_res, s); \
            } \
            if (s_res != NULL) { \
                free(s_res); \
            } \
            if (s != NULL) { \
                free(s); \
            } \
            metac_value_delete(p_val); \
        } \
        if (p_param_storage != NULL) { \
            metac_parameter_storage_delete(p_param_storage); \
        } \
        res; \
    })


#endif