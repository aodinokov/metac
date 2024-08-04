#include "metac/reflect.h"
#include "metac/backend/helpers.h"
#include "metac/backend/iterator.h"
#include "metac/backend/list.h"
#include "metac/backend/value.h"

#include <assert.h> /*assert*/
#include <errno.h>  /*EINVAL... */
#include <string.h> /*strcmp, memcpy*/

// disable this to avoid supporting of this feature
#ifndef VA_ARG_IN_VA_ARG
#define VA_ARG_IN_VA_ARG 1
#endif

typedef struct metac_parameter {
    metac_num_t size;
    metac_entry_t *p_entry;
    void * addr;

    LIST_ENTRY(metac_parameter) links;
} metac_parameter_t;

LIST_HEAD(metac_parameter_storage_list, metac_parameter);
typedef struct metac_parameter_storage {
    struct metac_parameter_storage_list list;
    metac_num_t list_size;  // cache
    struct metac_parameter * p_last_added_param;  // cache to add to the end of the list;
} metac_parameter_storage_t;

metac_parameter_storage_t * metac_new_parameter_storage() {
    metac_parameter_storage_t * p_param_storage = calloc(1, sizeof(*p_param_storage));
    _check_(p_param_storage == NULL, NULL);

    LIST_INIT(&p_param_storage->list);
    p_param_storage->list_size = 0;
    p_param_storage->p_last_added_param = NULL;

    return p_param_storage;
}

void metac_parameter_storage_cleanup(metac_parameter_storage_t * p_param_storage) {
    if (p_param_storage == NULL) {
        return;
    }

    while (!LIST_EMPTY(&p_param_storage->list)) {
        metac_parameter_t * p_param = LIST_FIRST(&p_param_storage->list);
        if (p_param->size == 0) {
            assert(p_param);
            metac_entry_t * p_entry = p_param->p_entry;

            metac_parameter_storage_t * p_inner_param_storage = (metac_parameter_storage_t *)p_param->addr;
            if (p_inner_param_storage != NULL) {
                metac_parameter_storage_delete(p_inner_param_storage);
            }
        } else {
            free (p_param->addr);
        }

        if (metac_entry_is_dynamic(p_param->p_entry)!=0){
            metac_entry_delete(p_param->p_entry);
        }

        LIST_REMOVE(p_param, links);
        free(p_param);
    }
    p_param_storage->list_size = 0;
    p_param_storage->p_last_added_param = NULL;
}

void metac_parameter_storage_delete(metac_parameter_storage_t * p_param_storage) {
    if (p_param_storage == NULL) {
        return;
    }

    metac_parameter_storage_cleanup(p_param_storage);

    free(p_param_storage);
}

metac_num_t metac_parameter_storage_size(metac_parameter_storage_t * p_param_storage) {
    _check_(p_param_storage == NULL, 0);
    return p_param_storage->list_size;
}

metac_parameter_t * _parameter_storage_append_by_buffer(metac_parameter_storage_t * p_param_storage,
    metac_num_t size,
    metac_entry_t *p_entry,
    void * addr) {
    _check_(p_param_storage == NULL, NULL);

    metac_parameter_t * p_param = calloc(1, sizeof(*p_param));
    _check_(p_param == NULL, NULL);

    metac_entry_t *p_entry_copy = NULL;
    if (metac_entry_is_dynamic(p_entry) != 0) {
        p_entry_copy = metac_new_entry(p_entry);
        if (p_entry != NULL && p_entry_copy == NULL) {
            free(p_param);
            return NULL;
        }
    }

    p_param->size = size;
    p_param->addr = addr;
    if (p_entry_copy != NULL) {
        p_param->p_entry = p_entry_copy;
    } else {
        p_param->p_entry = p_entry;
    }

    // add to the end of list
    if (p_param_storage->p_last_added_param == NULL) {
        LIST_INSERT_HEAD(&p_param_storage->list, p_param, links);
    } else {
        LIST_INSERT_AFTER(p_param_storage->p_last_added_param, p_param, links);
    }
    ++p_param_storage->list_size;
    p_param_storage->p_last_added_param = p_param;

    return p_param;
}

int metac_parameter_storage_append_by_buffer(metac_parameter_storage_t * p_param_storage,
    metac_entry_t *p_entry,
    metac_num_t size) {
    
    _check_(p_param_storage == NULL, -(EINVAL));
    _check_(p_entry == NULL || size <= 0, -(EINVAL));

    void * addr = calloc(1, size);
    _check_(addr == NULL, -(ENOMEM));

    metac_parameter_t * p_param = _parameter_storage_append_by_buffer(p_param_storage,
        size,
        p_entry,
        addr);

    if (p_param == NULL) {
        free(addr);
        return -(EFAULT);
    }

    return 0;
}

int metac_parameter_storage_append_by_parameter_storage(metac_parameter_storage_t * p_param_storage,
    metac_entry_t *p_entry) {

    assert(metac_entry_is_unspecified_parameter(p_entry) != 0 || metac_entry_is_va_list_parameter(p_entry) != 0);

    metac_parameter_storage_t * p_inner_param_storage = metac_new_parameter_storage();
    _check_(p_entry == NULL, -(ENOMEM));

    metac_parameter_t * p_param = _parameter_storage_append_by_buffer(p_param_storage,
        0,
        p_entry,
        p_inner_param_storage);

    if (p_param == NULL) {
        metac_parameter_storage_delete(p_inner_param_storage);
        return -(EFAULT);
    }

    return 0;
}

int _parameter_storage_copy(metac_parameter_storage_t * p_copy_param_storage, metac_parameter_storage_t * p_param_storage) {
    metac_parameter_t * p_param = NULL;
    LIST_FOREACH(p_param, &p_param_storage->list, links) {
        if (p_param->size != 0) {
            if (metac_parameter_storage_append_by_buffer(p_copy_param_storage, 
                p_param->p_entry, p_param->size) != 0) {
                return -(EFAULT);
            }
            assert(p_copy_param_storage->p_last_added_param != NULL);
            assert(p_copy_param_storage->p_last_added_param->addr != NULL);
            assert(p_copy_param_storage->p_last_added_param->size == p_param->size);
            memcpy(p_copy_param_storage->p_last_added_param->addr, p_param->addr, p_param->size);
        } else {
            if (metac_parameter_storage_append_by_parameter_storage(p_copy_param_storage,
                p_param->p_entry) != 0) {
                return -(EFAULT);
            }
            assert(p_copy_param_storage->p_last_added_param != NULL);
            assert(p_copy_param_storage->p_last_added_param->addr != NULL);
            assert(p_copy_param_storage->p_last_added_param->size == p_param->size);
            metac_parameter_storage_t * p_inner_param_storage = (metac_parameter_storage_t *)p_param->addr;
            metac_parameter_storage_t * p_inner_copy_param_storage = (metac_parameter_storage_t *)p_copy_param_storage->p_last_added_param->addr;
            if (_parameter_storage_copy(p_inner_copy_param_storage, p_inner_param_storage) != 0) {
                return -(EFAULT);
            }
        }
    }
    return 0;
}

metac_parameter_storage_t * metac_parameter_storage_copy(metac_parameter_storage_t * p_param_storage) {
    _check_(p_param_storage == NULL, NULL);
    metac_parameter_storage_t * p_copy_param_storage = metac_new_parameter_storage();
    _check_(p_copy_param_storage == NULL, NULL);
    if (_parameter_storage_copy(p_copy_param_storage, p_param_storage) != 0) {
        metac_parameter_storage_delete(p_copy_param_storage);
        return NULL;
    }
    return p_copy_param_storage;
}

metac_value_t * metac_parameter_storage_new_param_value(metac_parameter_storage_t * p_param_storage, metac_num_t id) {
    metac_num_t current = 0;
    metac_parameter_t * p_param = NULL;
    LIST_FOREACH(p_param, &p_param_storage->list, links) {
        if (id == current) {
            return metac_new_value(p_param->p_entry, p_param->addr);
        }
        ++current;
    }
    return NULL;
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

            if (metac_parameter_storage_append_by_parameter_storage(p_subprog_load, p_param_entry) != 0){
                metac_recursive_iterator_fail(p_iter);
                return;   
            }

            // handle va_paremeter as event
            metac_value_event_t ev = {
                .type = METAC_RQVST_va_list,
                .va_list_param_id = i,  // TODO: to remove?
                // .p_va_list_param_entry = p_param_entry,
                .p_return_value = metac_parameter_storage_new_param_value(p_subprog_load, i),
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
                    if (local != 0){
                        va_end(local_cntr.parameters);
                    }

                    metac_recursive_iterator_fail(p_iter);
                    return;
                }
                if (local != 0){
                    va_end(local_cntr.parameters);
                }
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
                        if (metac_parameter_storage_append_by_buffer(p_subprog_load, p_param_entry, sizeof(_type_)) != 0) { \
                            metac_recursive_iterator_fail(p_iter); \
                            return; \
                        } \
                        metac_value_t * p_param_value = metac_parameter_storage_new_param_value(p_subprog_load, i); \
                        if (p_param_value == NULL) { \
                            metac_recursive_iterator_fail(p_iter); \
                            return; \
                        } \
                        addr = metac_value_addr(p_param_value); \
                        metac_value_delete(p_param_value); \
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
                        if (metac_parameter_storage_append_by_buffer(p_subprog_load, p_param_entry, sizeof(void *)) != 0) {
                            metac_recursive_iterator_fail(p_iter);
                            return;
                        }
                        metac_value_t * p_param_value = metac_parameter_storage_new_param_value(p_subprog_load, i);
                        if (p_param_value == NULL) {
                            metac_recursive_iterator_fail(p_iter);
                            return;
                        }
                        addr = metac_value_addr(p_param_value);
                        metac_value_delete(p_param_value);

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
                        if (metac_parameter_storage_append_by_buffer(p_subprog_load, p_param_entry, sizeof(_type_)) != 0) { \
                            metac_recursive_iterator_fail(p_iter); \
                            return; \
                        } \
                        metac_value_t * p_param_value = metac_parameter_storage_new_param_value(p_subprog_load, i); \
                        if (p_param_value == NULL) { \
                            metac_recursive_iterator_fail(p_iter); \
                            return; \
                        } \
                        addr = metac_value_addr(p_param_value); \
                        metac_value_delete(p_param_value); \
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
                        if (metac_parameter_storage_append_by_buffer(p_subprog_load, p_param_entry, param_byte_sz) != 0) {
                            metac_recursive_iterator_fail(p_iter);
                            return;
                        }
                        metac_value_t * p_param_value = metac_parameter_storage_new_param_value(p_subprog_load, i);

                        if (p_param_value == NULL) {
                            metac_recursive_iterator_fail(p_iter);
                            return;
                        }
                        addr = metac_value_addr(p_param_value);
                        metac_value_delete(p_param_value);

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

static metac_value_t * _new_value_with_parameters(metac_parameter_storage_t * p_subprog_load, metac_tag_map_t * p_tag_map, metac_entry_t * p_entry, struct va_list_container *p_va_list_container) {
    _check_(p_entry == NULL || metac_entry_has_parameters(p_entry) == 0, NULL);
    _check_(p_subprog_load == NULL, NULL);

    metac_value_t * p_val = metac_new_value(p_entry, p_subprog_load);
    if (p_val == NULL) {
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

metac_value_t * metac_new_value_with_parameters(metac_parameter_storage_t * p_subprog_load, metac_tag_map_t * p_tag_map, metac_entry_t * p_entry, ...) {
    struct va_list_container cntr = {};
    va_start(cntr.parameters, p_entry);
    metac_value_t * p_val = _new_value_with_parameters(p_subprog_load, p_tag_map, p_entry, &cntr);
    va_end(cntr.parameters);
    return p_val;
}

metac_value_t * metac_new_value_with_vparameters(metac_parameter_storage_t * p_subprog_load, metac_tag_map_t * p_tag_map, metac_entry_t * p_entry, va_list parameters) {
    struct va_list_container cntr = {};
    va_copy(cntr.parameters, parameters);
    metac_value_t * p_val = _new_value_with_parameters(p_subprog_load, p_tag_map, p_entry, &cntr);
    va_end(cntr.parameters);
    return p_val;
}
