#include "metac/reflect.h"
#include "metac/backend/helpers.h"
#include "metac/backend/iterator.h"
#include "metac/backend/value.h"

#include <assert.h> /*assert*/
#include <string.h> /*strcmp*/

static struct metac_value_with_args_load * new_value_with_args_load(metac_num_t args_count) {
    // offset_of(struct metac_value_with_args_load, args[args_count + 1])
    size_t sz = (size_t)(&((struct metac_value_with_args_load*)NULL)->args[args_count + 1]);

    struct metac_value_with_args_load * p_load = calloc(1, sz);
    _check_(p_load == NULL, NULL);

    p_load->args_count = args_count;
    return p_load;
}

static void value_with_args_load_delete(struct metac_value_with_args_load * p_load) {
    if (p_load == NULL) {
        return;
    }
    for (metac_num_t i = 0; i < p_load->args_count; ++i) {
        if (p_load->args[i].p_buf != NULL) {
            free(p_load->args[i].p_buf);
        }
    }
    free(p_load);
}

static void _handle_subprogram(
    metac_recursive_iterator_t * p_iter,
    metac_value_t * p,
    int state,
    metac_kind_t final_kind,
    // global params
    metac_tag_map_t * p_tag_map,
    struct va_list_container * p_va_list_container) {
    assert(p_iter != NULL);
    assert(p != NULL);
    assert(p_va_list_container != NULL);

    metac_entry_t * p_entry = metac_value_entry(p);
    struct metac_value_with_args_load * p_load = (struct metac_value_with_args_load *)metac_value_addr(p);
    assert(p_entry != NULL);
    assert(p_load != NULL);
    assert(metac_entry_has_paremeters(p_entry) != 0);
    assert(metac_entry_paremeters_count(p_entry) == p_load->args_count);

    for (int i = 0; i < metac_entry_paremeters_count(p_entry); ++i) {
        metac_entry_t * p_param_entry = metac_entry_by_paremeter_id(p_entry, i);
        if (metac_entry_is_parameter(p_param_entry) == 0) {
            // something is wrong
            metac_recursive_iterator_fail(p_iter);
            return;
        }
        void * addr = NULL;
        if (metac_entry_is_unspecified_parameter(p_param_entry) != 0) {
            // we don't support printing va_args yet
            if (p_tag_map != NULL){
                // TODO: handle va_arg as event
                // metac_value_event_t ev = {.type = METAC_RQVST_pointer_array_count, .p_return_value = NULL};
                // metac_entry_tag_t * p_tag = metac_tag_map_tag(p_tag_map, metac_value_entry(p));
                // if (p_tag != NULL && p_tag->handler) {
                //     if (metac_value_event_handler_call(p_tag->handler, p_iter, &ev, p_tag->p_context) != 0) {
                //         metac_recursive_iterator_fail(p_iter);
                //         continue;
                //     }
                //     p_arr_val = ev.p_return_value;
                // }
            }
        } else {
            metac_entry_t * p_param_type_entry = metac_entry_parameter_entry(p_param_entry);
            if (p_param_type_entry == NULL) {
                // something is wrong
                metac_recursive_iterator_fail(p_iter);
                return;
            }
            metac_size_t param_byte_sz = 0;
            if (metac_entry_byte_size(p_param_type_entry, &param_byte_sz) != 0) {
                // something is wrong
                metac_recursive_iterator_fail(p_iter);
                return;
            }
            if (metac_entry_is_base_type(p_param_type_entry) != 0) {
                // take what type of base type it is. It can be char, unsigned char.. etc
                metac_name_t param_base_type_name = metac_entry_base_type_name(p_param_type_entry);
#define _base_type_arg_(_type_, _va_type_, _pseudoname_) \
                do { \
                    if (addr == NULL && strcmp(param_base_type_name, #_pseudoname_) == 0 && param_byte_sz == sizeof(_type_)) { \
                        addr = calloc(sizeof(_type_), 1); \
                        if (addr != NULL) { \
                            _type_ val = va_arg(p_va_list_container->args, _va_type_); \
                            memcpy(addr, &val, sizeof(val)); \
                        } \
                    } \
                } while(0)
                // handle all known base types
                _base_type_arg_(char, int, char);
                _base_type_arg_(unsigned char, int, unsigned char);
                _base_type_arg_(short, int, short int);
                _base_type_arg_(unsigned short, int, unsigned short int);
                _base_type_arg_(int, int, int);
                _base_type_arg_(unsigned int, unsigned int, unsigned int);
                _base_type_arg_(long, long, long int);
                _base_type_arg_(unsigned long, unsigned long, unsigned long int);
                _base_type_arg_(long long, long long, long long int);
                _base_type_arg_(unsigned long long, unsigned long long, unsigned long long int);
                _base_type_arg_(bool, int, _Bool);
                _base_type_arg_(float, double, float);
                _base_type_arg_(double, double, double);
                _base_type_arg_(long double, long double, long double);
                _base_type_arg_(float complex, float complex, complex float);
                _base_type_arg_(double complex, double complex, complex double);
                _base_type_arg_(long double complex, long double complex, long complex double);
#undef _base_type_arg_
            } else if (metac_entry_is_pointer(p_param_type_entry) != 0) {
                do {
                    if (addr == NULL ) {
                        addr = calloc(sizeof(void *), 1);
                        if (addr != NULL) {
                            void * val = va_arg(p_va_list_container->args, void *);
                            memcpy(addr, &val, sizeof(val));
                        }
                    }
                } while(0);
            } else if (metac_entry_is_enumeration(p_param_type_entry) != 0) {
#define _enum_arg_(_type_, _va_type_) \
                do { \
                    if (addr == NULL && param_byte_sz == sizeof(_type_)) { \
                        addr = calloc(sizeof(_type_), 1); \
                        if (addr != NULL) { \
                            _type_ val = va_arg(p_va_list_container->args, _va_type_); \
                            memcpy(addr, &val, sizeof(val)); \
                        } \
                    } \
                } while(0)
                _enum_arg_(char, int);
                _enum_arg_(short, int);
                _enum_arg_(int, int);
                _enum_arg_(long, long);
                _enum_arg_(long long, long long);
#undef _enum_arg_          
            } else if (metac_entry_has_members(p_param_type_entry) != 0) {
                do {
                    if (addr == NULL) {
                        /*  NOTE: we can't call calloc AFTER metac_entry_struct_va_arg, we can only copy data.
                            calloc, printf and other functions damage the data 
                        */
                        addr = calloc(param_byte_sz, 1);
                        if (addr != NULL) {
                            void * val = metac_entry_struct_va_arg(p_param_type_entry, p_va_list_container);
                            if (val == NULL) {
                                free(addr);
                                addr = NULL;
                                break;
                            }
                            memcpy(addr, val, param_byte_sz);
                        }
                    }
                } while(0);
            }
        }
        if (addr == NULL) {
            metac_recursive_iterator_fail(p_iter);
            return;
        }
        // save region
        p_load->args[i].p_buf = addr;
    }
    metac_recursive_iterator_done(p_iter, NULL);
}

void metac_value_with_args_delete(metac_value_t * p_val) {
    if (p_val == NULL) {
        return;
    }

    metac_entry_t * p_entry = metac_value_entry(p_val);
    if (p_entry == NULL || metac_entry_has_paremeters(p_entry) == 0)  {
        return;
    }

    struct metac_value_with_args_load * p_load = (struct metac_value_with_args_load *)metac_value_addr(p_val);
    if (p_load == NULL) {
        return;
    }

    metac_value_delete(p_val);
    value_with_args_load_delete(p_load);
}

static metac_value_t * _new_value_with_args(metac_tag_map_t * p_tag_map, metac_entry_t * p_entry, struct va_list_container *p_va_list_container) {
    _check_(p_entry == NULL || metac_entry_has_paremeters(p_entry) == 0, NULL);

    struct metac_value_with_args_load * p_load =  new_value_with_args_load(metac_entry_paremeters_count(p_entry));
    _check_(p_load == NULL, NULL);
    metac_value_t * p_val = metac_new_value(p_entry, p_load);
    if (p_val == NULL) {
        value_with_args_load_delete(p_load);
        return NULL;
    }

    // we need to wrap our code into iterator to be able to reuse p_tag_map.
    // actually there is no iteration - we'll have only main p_val
    metac_recursive_iterator_t * p_iter = metac_new_recursive_iterator(p_val);
    for (metac_value_t * p = (metac_value_t *)metac_recursive_iterator_next(p_iter); p != NULL;
        p = (metac_value_t *)metac_recursive_iterator_next(p_iter)) {
        int state = metac_recursive_iterator_get_state(p_iter);

        metac_kind_t final_kind = metac_value_final_kind(p, NULL);
        switch(final_kind) {
            //case METAC_KND_subroutine_type: // TODO: try to support them as well
            case METAC_KND_subprogram: {
                _handle_subprogram(p_iter, p, state, final_kind, p_tag_map, p_va_list_container);
                continue;
            }
            default:  {
                /* quickly fail if we don't know how to handle */
                metac_recursive_iterator_fail(p_iter);
                continue;
            }
        }
    }
    // check iterator result
    int failed = 0;
    metac_recursive_iterator_get_out(p_iter, NULL, &failed);
    if (failed != 0) {
        metac_value_with_args_delete(p_val);
        p_val = NULL;
    }

    metac_recursive_iterator_free(p_iter);
    return p_val;
}

metac_value_t * metac_new_value_with_args(metac_tag_map_t * p_tag_map, metac_entry_t * p_entry, ...) {
    struct va_list_container cntr = {};
    va_start(cntr.args, p_entry);
    metac_value_t * p_val = _new_value_with_args(p_tag_map, p_entry, &cntr);
    va_end(cntr.args);
    return p_val;
}

metac_value_t * metac_new_value_with_vargs(metac_tag_map_t * p_tag_map, metac_entry_t * p_entry, va_list args) {
    struct va_list_container cntr = {};
    va_copy(cntr.args, args);
    metac_value_t * p_val = _new_value_with_args(p_tag_map, p_entry, &cntr);
    va_end(cntr.args);
    return p_val;
}

// potential API
//metac_value_t * metac_new_value_with_args(metac_tag_map_t * p_tag_map, metac_entry_t * p_entry, ...)
//metac_value_t * metac_new_value_with_vargs(metac_tag_map_t * p_tag_map, metac_entry_t * p_entry, va_list args)
//metac_flag_t metac_value_is_value_with_args(metac_value_t * p_val)
//void metac_value_with_args_set_res(metac_value_t * p_val, void *)
//void metac_value_with_args_res(metac_value_t * p_val, void *)
//...
//void metac_value_with_args_delete(metac_value_t * p_val)
