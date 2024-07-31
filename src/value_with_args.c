#include "metac/reflect.h"
#include "metac/backend/helpers.h"
#include "metac/backend/iterator.h"
#include "metac/backend/value.h"

#include <assert.h> /*assert*/
#include <errno.h>  /*EINVAL... */
#include <string.h> /*strcmp*/

// disable this to avoid supporting of this feature
#ifndef VA_ARG_IN_VA_ARG
#define VA_ARG_IN_VA_ARG 1
#endif

typedef struct metac_parameter {
    metac_num_t size;   // if size is not 0: this was created by metac_parameter_storage_new_item
    metac_value_t * p_val;
} metac_parameter_t;

typedef struct metac_parameter_storage {
    metac_num_t params_count;
    metac_parameter_t * params;
} metac_parameter_storage_t;

metac_parameter_storage_t * metac_new_parameter_storage(metac_num_t params_count) {
    _check_(params_count < 0, NULL);

    metac_parameter_storage_t * p_param_load = calloc(1, sizeof(metac_parameter_storage_t));
    _check_(p_param_load == NULL, NULL);

    if (params_count > 0) {
        p_param_load->params = calloc(params_count, sizeof(metac_parameter_t));
        if (p_param_load->params == NULL) {
            free(p_param_load);
            return NULL;
        }
    }
    p_param_load->params_count = params_count;
    return p_param_load;
}

metac_num_t metac_parameter_storage_size(metac_parameter_storage_t * p_param_load) {
    _check_(p_param_load == NULL, 0);
    return p_param_load->params_count;
}

metac_value_t * metac_parameter_storage_item(metac_parameter_storage_t * p_param_load, metac_num_t id) {
    _check_(id < 0 || id >= p_param_load->params_count , NULL);
    return p_param_load->params[id].p_val;
}

metac_value_t * metac_parameter_storage_set_item(metac_parameter_storage_t * p_param_load, metac_num_t id, metac_value_t * p_value) {
    _check_(id < 0 || id >= p_param_load->params_count , NULL);
    p_param_load->params[id].size = 0;
    p_param_load->params[id].p_val = p_value;
    return p_param_load->params[id].p_val;
}

metac_value_t * metac_parameter_storage_new_item(metac_parameter_storage_t * p_param_load,
    metac_num_t id,
    metac_entry_t *p_entry,
    metac_num_t size) {
    _check_(id < 0 || id >= p_param_load->params_count , NULL);
    _check_(p_entry == NULL, NULL);
    _check_(p_param_load->params[id].p_val != NULL, NULL); // already exists

    metac_value_t *p_value = NULL;
    if (metac_entry_has_parameter_load(p_entry) != 0) {// create another metac_parameter_storage_t * - size is number of subaargs
        metac_parameter_storage_t * p_in_param_load = metac_new_parameter_storage(size);
        if (p_in_param_load == NULL) {
            return NULL;
        }

        p_value = metac_new_value(p_entry, p_in_param_load);
        if (p_value == NULL) {
            metac_parameter_storage_delete(p_in_param_load);
        }
    } else { // allocate memory with correct size;
        void * addr = calloc(1, size);
        if (addr == NULL) {
            return NULL;
        }
        p_value = metac_new_value(p_entry, addr);
        if (p_value == NULL) {
            free(addr);
        }
    }
    p_param_load->params[id].size = size;
    p_param_load->params[id].p_val = p_value;
    return p_value;
}

metac_parameter_storage_t * metac_parameter_storage_copy(metac_parameter_storage_t * p_param_load) {
    _check_(p_param_load == NULL, NULL);
    metac_num_t i, count = metac_parameter_storage_size(p_param_load);
    metac_parameter_storage_t * p_param_load_copy = metac_new_parameter_storage(count);
    _check_(p_param_load == NULL, NULL);

    for (i = 0; i< count; ++i) {
        if (p_param_load->params[i].size <= 0) {
            p_param_load_copy->params[i].size = p_param_load->params[i].size;
            p_param_load_copy->params[i].p_val = metac_new_value_from_value(p_param_load->params[i].p_val);
        } else {
            if (metac_entry_has_parameter_load(metac_value_entry(p_param_load->params[i].p_val)) != 0) {
                p_param_load_copy->params[i].size = p_param_load->params[i].size;
                p_param_load_copy->params[i].p_val = metac_new_value_from_value(p_param_load->params[i].p_val);
            } else {
                p_param_load_copy->params[i].size = p_param_load->params[i].size;

                void * addr = calloc(1, p_param_load_copy->params[i].size);
                if (addr != NULL) {
                    memcpy(addr, metac_value_addr(p_param_load->params[i].p_val), p_param_load_copy->params[i].size);
                    p_param_load_copy->params[i].p_val = metac_new_value(metac_value_entry(p_param_load->params[i].p_val), addr);
                }
            }
        }

        if (p_param_load->params[i].p_val != NULL && p_param_load_copy->params[i].p_val == NULL) {
            metac_parameter_storage_delete(p_param_load_copy);
            return NULL;
        }
    }

    return p_param_load_copy;
}

metac_flag_t metac_parameter_storage_delete(metac_parameter_storage_t * p_param_load) {
    _check_(p_param_load == NULL, 0);
    for (metac_num_t id = 0; id < p_param_load->params_count; ++id) {
        metac_value_t * p_value  = metac_parameter_storage_item(p_param_load, id);
        if (p_value == NULL) {
            continue;
        }
        metac_entry_t * p_entry = metac_value_entry(p_value);

        if (metac_entry_has_parameter_load(p_entry) == 0) {
            void * addr = metac_value_addr(p_value);
            if (addr != NULL) {
                free(addr);
            }
        } // don't do anything otherwise - metac_value_delete will take case of metac_parameter_storage_t

        metac_value_delete(p_value);
    }
    
    if (p_param_load->params != NULL) {
        free(p_param_load->params);
    }
    free(p_param_load);
    return 1;
}

#if VA_ARG_IN_VA_ARG != 0
static void _va_list_cp_to_container(struct va_list_container * dst, va_list src) {
    va_copy(dst->parameters, src);
}
#endif 

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
    metac_parameter_storage_t * p_subprog_load = (metac_parameter_storage_t *)metac_value_addr(p);
    assert(p_entry != NULL);
    assert(p_subprog_load != NULL);
    assert(metac_entry_has_parameters(p_entry) != 0);
    assert(metac_entry_parameters_count(p_entry) == metac_parameter_storage_size(p_subprog_load));

    for (metac_num_t i = 0; i < metac_entry_parameters_count(p_entry); ++i) {
        metac_entry_t * p_param_entry = metac_entry_by_paremeter_id(p_entry, i);
        if (metac_entry_is_parameter(p_param_entry) == 0) {
            // something is wrong
            metac_recursive_iterator_fail(p_iter);
            return;
        }

        if (metac_entry_is_unspecified_parameter(p_param_entry) != 0
#if VA_ARG_IN_VA_ARG != 0
            || metac_entry_is_va_list_parameter(p_param_entry) != 0
#endif
            ) {
            if (p_tag_map == NULL) {
                metac_recursive_iterator_fail(p_iter);
                return;
            }
            // handle va_paremeter as event
            metac_value_event_t ev = {
                .type = METAC_RQVST_va_list,
                .va_list_param_id = i,  // TODO: to remove?
                .p_va_list_param_entry = p_param_entry,
                .p_va_list_container = p_va_list_container,
            };

            metac_entry_tag_t * p_tag = metac_tag_map_tag(p_tag_map, p_param_entry);
            
            if (p_tag != NULL && p_tag->handler) {

                // if parameter is va_list - we need to extract it
                int local = 0;
                struct va_list_container local_cntr = {};
#if VA_ARG_IN_VA_ARG != 0
                if (metac_entry_is_va_list_parameter(p_param_entry) != 0) {
                    local = 1;
#if __linux__
                    // linux (gcc) can't extract va_arg from va_arg
                    void * p = va_arg(p_va_list_container->parameters, void*);
                    memcpy(&local_cntr, p, sizeof(local_cntr));
#else
                    _va_list_cp_to_container(&local_cntr, va_arg(p_va_list_container->parameters, va_list));
#endif
                    ev.p_va_list_container = &local_cntr;
                }
#endif

                if (metac_value_event_handler_call(p_tag->handler, p_iter, &ev, p_tag->p_context) != 0) {
                    if (local != 0){va_end(local_cntr.parameters);}

                    metac_recursive_iterator_fail(p_iter);
                    return;
                }
                if (local != 0){va_end(local_cntr.parameters);}

                if (ev.p_return_value == NULL) {
                    metac_recursive_iterator_fail(p_iter);
                    return;
                }
                metac_parameter_storage_set_item(p_subprog_load, i, ev.p_return_value);
            }
        } else { /* normal param */
            void * addr = NULL;

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
#define _base_type_paremeter_(_type_, _va_type_, _pseudoname_) \
                do { \
                    if (addr == NULL && strcmp(param_base_type_name, #_pseudoname_) == 0 && param_byte_sz == sizeof(_type_)) { \
                        metac_value_t * p_param_value = metac_parameter_storage_new_item(p_subprog_load, i, p_param_entry, sizeof(_type_)); \
                        if (p_param_value == NULL) { \
                            metac_recursive_iterator_fail(p_iter); \
                            return; \
                        } \
                        addr = metac_value_addr(p_param_value); \
                        if (addr != NULL) { \
                            _type_ val = va_arg(p_va_list_container->parameters, _va_type_); \
                            memcpy(addr, &val, sizeof(val)); \
                        } \
                    } \
                } while(0)
                // handle all known base types
                _base_type_paremeter_(char, int, char);
                _base_type_paremeter_(unsigned char, int, unsigned char);
                _base_type_paremeter_(short, int, short int);
                _base_type_paremeter_(unsigned short, int, unsigned short int);
                _base_type_paremeter_(int, int, int);
                _base_type_paremeter_(unsigned int, unsigned int, unsigned int);
                _base_type_paremeter_(long, long, long int);
                _base_type_paremeter_(unsigned long, unsigned long, unsigned long int);
                _base_type_paremeter_(long long, long long, long long int);
                _base_type_paremeter_(unsigned long long, unsigned long long, unsigned long long int);
                _base_type_paremeter_(bool, int, _Bool);
                _base_type_paremeter_(float, double, float);
                _base_type_paremeter_(double, double, double);
                _base_type_paremeter_(long double, long double, long double);
                _base_type_paremeter_(float complex, float complex, complex float);
                _base_type_paremeter_(double complex, double complex, complex double);
                _base_type_paremeter_(long double complex, long double complex, long complex double);
#undef _base_type_paremeter_
            } else if (metac_entry_is_pointer(p_param_type_entry) != 0) {
                do {
                    if (addr == NULL ) {
                        metac_value_t * p_param_value = metac_parameter_storage_new_item(p_subprog_load, i, p_param_entry, sizeof(void *));
                        if (p_param_value == NULL) {
                            metac_recursive_iterator_fail(p_iter);
                            return;
                        }
                        addr = metac_value_addr(p_param_value);

                        if (addr != NULL) {
                            void * val = va_arg(p_va_list_container->parameters, void *);
                            memcpy(addr, &val, sizeof(val));
                        }
                    }
                } while(0);
            } else if (metac_entry_is_enumeration(p_param_type_entry) != 0) {
#define _enum_paremeter_(_type_, _va_type_) \
                do { \
                    if (addr == NULL && param_byte_sz == sizeof(_type_)) { \
                        metac_value_t * p_param_value = metac_parameter_storage_new_item(p_subprog_load, i, p_param_entry, sizeof(_type_)); \
                        if (p_param_value == NULL) { \
                            metac_recursive_iterator_fail(p_iter); \
                            return; \
                        } \
                        addr = metac_value_addr(p_param_value); \
                        if (addr != NULL) { \
                            _type_ val = va_arg(p_va_list_container->parameters, _va_type_); \
                            memcpy(addr, &val, sizeof(val)); \
                        } \
                    } \
                } while(0)
                _enum_paremeter_(char, int);
                _enum_paremeter_(short, int);
                _enum_paremeter_(int, int);
                _enum_paremeter_(long, long);
                _enum_paremeter_(long long, long long);
#undef _enum_paremeter_          
            } else if (metac_entry_has_members(p_param_type_entry) != 0) {
                do {
                    if (addr == NULL) {
                        /*  NOTE: we can't call calloc AFTER metac_entry_struct_va_paremeter, we can only copy data.
                            calloc, printf and other functions damage the data 
                        */
                        metac_value_t * p_param_value = metac_parameter_storage_new_item(p_subprog_load, i, p_param_entry, param_byte_sz);
                        if (p_param_value == NULL) {
                            metac_recursive_iterator_fail(p_iter);
                            return;
                        }
                        addr = metac_value_addr(p_param_value);

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
            if (addr == NULL) {
                metac_recursive_iterator_fail(p_iter);
                return;
            }
        }
    }
    metac_recursive_iterator_done(p_iter, NULL);
}

static metac_value_t * _new_value_with_parameters(metac_tag_map_t * p_tag_map, metac_entry_t * p_entry, struct va_list_container *p_va_list_container) {
    _check_(p_entry == NULL || metac_entry_has_parameters(p_entry) == 0, NULL);

    metac_parameter_storage_t * p_subprog_load =  metac_new_parameter_storage(metac_entry_parameters_count(p_entry));
    _check_(p_subprog_load == NULL, NULL);
    metac_value_t * p_val = metac_new_value(p_entry, p_subprog_load);
    if (p_val == NULL) {
        metac_parameter_storage_delete(p_subprog_load);
        return NULL;
    }

    // we need to wrap our code into iterator to be able to reuse p_tag_map.
    // actually there is no iteration - we'll have only main p_val
    // alternativly it was possible to iterate here by params as well, but let's keep it simple for now
    // the only drawback is - we need to add special parameters into event:
    // va_list_param_id and p_va_list_load. 
    metac_recursive_iterator_t * p_iter = metac_new_recursive_iterator(p_val);
    for (metac_value_t * p = (metac_value_t *)metac_recursive_iterator_next(p_iter); p != NULL;
        p = (metac_value_t *)metac_recursive_iterator_next(p_iter)) {
        int state = metac_recursive_iterator_get_state(p_iter);

        metac_kind_t final_kind = metac_value_final_kind(p, NULL);
        switch(final_kind) {
            case METAC_KND_subroutine_type:
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
        metac_value_delete(p_val);
        p_val = NULL;
    }

    metac_recursive_iterator_free(p_iter);
    return p_val;
}

metac_value_t * metac_new_value_with_parameters(metac_tag_map_t * p_tag_map, metac_entry_t * p_entry, ...) {
    struct va_list_container cntr = {};
    va_start(cntr.parameters, p_entry);
    metac_value_t * p_val = _new_value_with_parameters(p_tag_map, p_entry, &cntr);
    va_end(cntr.parameters);
    return p_val;
}

metac_value_t * metac_new_value_with_vparameters(metac_tag_map_t * p_tag_map, metac_entry_t * p_entry, va_list parameters) {
    struct va_list_container cntr = {};
    va_copy(cntr.parameters, parameters);
    metac_value_t * p_val = _new_value_with_parameters(p_tag_map, p_entry, &cntr);
    va_end(cntr.parameters);
    return p_val;
}
