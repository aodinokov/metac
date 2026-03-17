#include "metac/backend/serialization/common.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>

metac_deserialization_task_t * metac_new_deserialization_task(metac_value_t * p_val, void * p_external) {
    metac_deserialization_task_t * p_task = calloc(1, sizeof(*p_task));
    if (p_task == NULL) {
        return NULL;
    }
    p_task->p_val = p_val;
    p_task->p_external = p_external;
    return p_task;
}

void metac_deserialization_task_delete(metac_deserialization_task_t * p_task) {
    if (p_task == NULL) {
        return;
    }
    free(p_task);
}

metac_value_t * _metac_deserialization_task_value_extractor(void * p_in) {
    if (p_in == NULL) {
        return NULL;
    }
    metac_deserialization_task_t * p_task = (metac_deserialization_task_t *)p_in;
    return p_task->p_val;
}

metac_deserialization_task_t * _metac_deserialization_task_find_task_with_allocation(metac_recursive_iterator_t * p_iterator) {
    int level = metac_recursive_iterator_level(p_iterator);
    if (level < 1) {
        return NULL;
    }
    for (int l = 0; l < level + 1; ++l) { /* if level = 1 there are 2 levels: 0 and 1 */
        metac_deserialization_task_t * p_task = metac_recursive_iterator_get_in(p_iterator, l);
        if (p_task->p_allocated != 0) {
            return p_task;
        }
    }
    return NULL;
}

int _metac_deserialization_task_cleanup_and_fail(metac_recursive_iterator_t * p_iterator) {
    while (metac_recursive_iterator_dep_queue_is_empty(p_iterator) == 0) {
        metac_deserialization_task_t * p_task = NULL;
        metac_recursive_iterator_dequeue_and_delete_dep(p_iterator, (void**)&p_task, NULL);
        if (p_task != NULL) {
            assert(p_task->p_allocated == NULL); // this memory has to be freed up already
            if (p_task->p_val != NULL ) {
                metac_value_delete(p_task->p_val);
            }
            metac_deserialization_task_delete(p_task);
        }
    }
    return -(EFAULT);
}    

int _metac_deserialization_task_dequeue_check_or_fail(metac_recursive_iterator_t * p_iterator, int cleanup_and_fail_state_id) {
    while (metac_recursive_iterator_dep_queue_is_empty(p_iterator) == 0) {
        metac_deserialization_task_t * p_task = NULL;
        metac_value_t * p_val_out = (metac_value_t *)metac_recursive_iterator_dequeue_and_delete_dep(p_iterator, (void**)&p_task, NULL);
        if (p_task != NULL) {
            if (p_task->p_val != NULL) {
                metac_value_delete(p_task->p_val);
            }
            metac_deserialization_task_delete(p_task);
        }
        if (p_val_out == NULL) {
            return cleanup_and_fail_state_id; // failure
        }
    }
    return 0; // success
}

metac_name_t _metac_value_name_per_protocol_default(metac_value_t * p_memb_val) {
    metac_name_t memb_name = metac_value_name(p_memb_val);
    if (memb_name == NULL) {
        return NULL; // anonymous field
    }
    return strdup(memb_name);
}

metac_name_t metac_value_name_per_protocol(
    // in
    metac_value_t * p_memb_val, char * protocol, metac_tag_map_t * p_tag_map,
    // out
    metac_flag_t * p_ingore,
    metac_flag_t * p_omitempty,
    metac_flag_t * p_omitzero) {
    // TODO: omitnil
    metac_name_t protocol_tag_value = NULL;
    if (p_tag_map != NULL) {
        metac_entry_tag_t * p_tag = metac_tag_map_tag(p_tag_map, metac_value_entry(p_memb_val));
        if (p_tag != NULL) {
            protocol_tag_value = metac_entry_tag_string_lookup(p_tag, protocol);
        }
    }
    if (protocol_tag_value != NULL) {
        /* metac_entry_tag_string_lookup actually returns more than only the name, e.g.
           https://pkg.go.dev/encoding/json#pkg-examples:
            // Field appears in JSON as key "myName".
            Field int `json:"myName"`

            // Field appears in JSON as key "myName" and
            // the field is omitted from the object if its value is empty,
            // as defined above.
            Field int `json:"myName,omitempty"`

            // Field appears in JSON as key "Field" (the default), but
            // the field is skipped if empty.
            // Note the leading comma.
            Field int `json:",omitempty"`

            // Field is ignored by this package.
            Field int `json:"-"`

            // Field appears in JSON as key "-".
            Field int `json:"-,"`
        */
        // ignore case
        if (strcmp(protocol_tag_value, "-") == 0) {
            if (p_ingore != NULL) {
                *p_ingore = 1;
            }
            free(protocol_tag_value);
            return NULL;
        }

        // find commas
        char * prev_flag_beginning = NULL;
        size_t protocol_tag_value_len = strlen(protocol_tag_value);
        for (size_t i = 0; i < protocol_tag_value_len; ++i) {
            if (protocol_tag_value[i] == ',') {
                protocol_tag_value[i] = '\0';

                char * flag_beginning = &protocol_tag_value[i + 1];
                if (prev_flag_beginning != NULL) {
                    // handle all possible flags except p_ingore
                    if (strcmp(prev_flag_beginning, "omitempty") == 0) {
                        if (p_omitempty != NULL) {
                            *p_omitempty = 1;
                        }
                    } else if (strcmp(prev_flag_beginning, "omitzero") == 0) {
                        if (p_omitzero != NULL) {
                            *p_omitzero = 1;
                        }
                    } //etc .. we can do this more univerally probably later
                }
                prev_flag_beginning = flag_beginning;
            }
        }

        if (strlen(protocol_tag_value) == 0) {
            free(protocol_tag_value);
            return _metac_value_name_per_protocol_default(p_memb_val);
        }
        return protocol_tag_value;
    }

    // default
    return _metac_value_name_per_protocol_default(p_memb_val);
}
