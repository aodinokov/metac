#include "metac/backend/iterator.h"
#include "metac/backend/list.h" /* part of <sys/queue.h> - copied because Windows didn't have this */

#include "metac/inherit.h"

#include <stdlib.h>
#include <errno.h>
#include <assert.h>

LIST_HEAD(recursive_iterator_entry_deps, recursive_iterator_entry);

#define struct_recursive_iterator_entry_as_name(_name_...) \
struct _name_ { \
    struct recursive_iterator_entry * p_parent; \
    metac_r_iter_state_t state; \
    void * p_in;  /**< task */ \
    void * p_context; /**< context: user can store here anything to share between cyckles */ \
    void * p_out; /**< result */ \
    struct recursive_iterator_entry_deps deps; /**< dependent items to be finished before visiting this again */ \
    struct recursive_iterator_entry * last_dep; /**< cache it and not to find it every time */ \
    LIST_ENTRY(recursive_iterator_entry) links; /**< to be added to parent list */ \
}
typedef METAC_STRUCT_CREATE(recursive_iterator_entry) recursive_iterator_entry_t;

static recursive_iterator_entry_t * metac_init_recursive_iterator_entry(
    struct recursive_iterator_entry *p_iterator_entry,
    struct recursive_iterator_entry * p_parent,
    void * p_in) {
    if (p_iterator_entry == NULL) {
        return NULL;
    }
    p_iterator_entry->p_parent = p_parent;
    p_iterator_entry->p_in = p_in;
    LIST_INIT(&p_iterator_entry->deps);
    return p_iterator_entry;
}

static void metac_cleanup_recursive_iterator_entry(struct recursive_iterator_entry *p_iterator_entry) {
    assert(LIST_EMPTY(&p_iterator_entry->deps)); // must be empty
}

static recursive_iterator_entry_t * metac_new_recursive_iterator_entry(struct recursive_iterator_entry * p_parent, void * p_in) {
    return metac_init_recursive_iterator_entry(calloc(1, sizeof(struct recursive_iterator_entry)), p_parent, p_in);
}

static void metac_recursive_iterator_entry_free(recursive_iterator_entry_t * p_iterator_entry) {
    metac_cleanup_recursive_iterator_entry(p_iterator_entry);
    free(p_iterator_entry);
}

static void * metac_recursive_iterator_entry_get_out(recursive_iterator_entry_t * p_iterator_entry, void ** pp_in, int * p_fail) {
    if (p_iterator_entry == NULL) {
        return NULL;
    }
    assert(metac_r_iter_state_is_end(p_iterator_entry->state));

    if (pp_in != NULL) {
        *pp_in = p_iterator_entry->p_in;
    }

    int fail = p_iterator_entry->state == METAC_R_ITER_failed;
    if (fail) {
        assert(p_iterator_entry->p_out == NULL);
    }
    if (p_fail != NULL) {
        *p_fail = fail;
    }

    return p_iterator_entry->p_out;
}

struct metac_recursive_iterator {
    METAC_STRUCT_INHERIT0(recursive_iterator_entry);
    recursive_iterator_entry_t *p_current;
};

metac_recursive_iterator_t * metac_new_recursive_iterator(void * p_in) {
    metac_recursive_iterator_t * p_iterator = calloc(1, sizeof(*p_iterator));
    if (p_iterator == NULL) {
        return NULL;
    }
    metac_init_recursive_iterator_entry(&p_iterator->recursive_iterator_entry, NULL, p_in);
    p_iterator->p_current = &p_iterator->recursive_iterator_entry;
    return p_iterator;
}

void metac_recursive_iterator_free(metac_recursive_iterator_t * p_iterator) {
    metac_cleanup_recursive_iterator_entry(&p_iterator->recursive_iterator_entry);
    free(p_iterator);
}

void * metac_recursive_iterator_next(metac_recursive_iterator_t * p_iterator) {
    if (p_iterator == NULL) {
        return NULL;
    }
    // the main idea of the code below:
    // if current doesn't have deps or deps are all done -> call it -> it will move forward
    // if it has deps ther are not finished -> repeat this analisys with it
    do {
        if (metac_r_iter_state_is_end(p_iterator->p_current->state)) {
            if (p_iterator->p_current->p_parent == NULL) {
                return NULL; // end of iterator
            }
            p_iterator->p_current = p_iterator->p_current->p_parent;
            continue;
        }
        // check all deps
        if (LIST_EMPTY(&p_iterator->p_current->deps)) {
            return p_iterator->p_current->p_in;
        }
        // if we have deps, we need to check deps
        int failed = 0;
        recursive_iterator_entry_t * p_dep = LIST_FIRST(&p_iterator->p_current->deps);
        while (p_dep != NULL) {
            if (p_dep->state == METAC_R_ITER_failed) {
                failed = 1;
            }
            if (!metac_r_iter_state_is_end(p_dep->state)) {
                break;
            }
            p_dep = LIST_NEXT(p_dep, links);
        }
        if (p_dep != NULL) { // found not finished dep - need to try to finish - ignore fails of other for now
            p_iterator->p_current = p_dep;
            continue; // go and try to get as deep as possible
        }
        // all deps are finished (some of them may fail)
        if (failed) {
            // p_iterator->p_current->state = METAC_R_ITER_dep_failed;
        }
        return p_iterator->p_current->p_in;
    }while(1);
}

void * metac_recursive_iterator_get_context(metac_recursive_iterator_t * p_iterator) {
    if (p_iterator == NULL) {
        return NULL;
    }
    assert(p_iterator->p_current != NULL);
    return p_iterator->p_current->p_context;
}

void * metac_recursive_iterator_set_context(metac_recursive_iterator_t * p_iterator, void * p_context) {
    if (p_iterator == NULL) {
        return NULL;
    }
    assert(p_iterator->p_current != NULL);

    p_iterator->p_current->p_context = p_context;
    return p_iterator->p_current->p_context;
}

int metac_recursive_iterator_level(metac_recursive_iterator_t * p_iterator) {
    if (p_iterator == NULL) {
        return -1;
    }
    assert(p_iterator->p_current != NULL);

    int level = 0;
    recursive_iterator_entry_t * p_entry = p_iterator->p_current;
    while (p_entry->p_parent != NULL) {
        p_entry = p_entry->p_parent;
        ++level;
    }
    return level;
}

void * metac_recursive_iterator_get_in(metac_recursive_iterator_t * p_iterator, int req_level) {
    if (p_iterator == NULL) {
        return NULL;
    }
    assert(p_iterator->p_current != NULL);

    int level = 0;
    recursive_iterator_entry_t * p_entry = p_iterator->p_current;
    while (1) {
        if (level == req_level) {
            return p_entry->p_in;
        }
        if (p_entry->p_parent == NULL) {
            return NULL;
        }
        p_entry = p_entry->p_parent;
        ++level;
    }
}

void * metac_recursive_iterator_get_out(metac_recursive_iterator_t * p_iterator, void ** pp_in, int * p_fail) {
    if (p_iterator == NULL) {
        return NULL;
    }
    assert(p_iterator->p_current != NULL);
    return metac_recursive_iterator_entry_get_out(p_iterator->p_current, pp_in, p_fail);
}

metac_r_iter_state_t metac_recursive_iterator_get_state(metac_recursive_iterator_t * p_iterator) {
    if (p_iterator == NULL) {
        return METAC_R_ITER_error;
    }
    assert(p_iterator->p_current != NULL);
    return p_iterator->p_current->state;
}

metac_recursive_iterator_t *metac_recursive_iterator_set_state(metac_recursive_iterator_t * p_iterator, metac_r_iter_state_t state) {
    if (p_iterator == NULL || state == METAC_R_ITER_error) {
        return NULL;
    }
    assert(p_iterator->p_current != NULL);
    assert(!metac_r_iter_state_is_end(p_iterator->p_current->state));
    p_iterator->p_current->state = state;
    return p_iterator;
}

metac_recursive_iterator_t *metac_recursive_iterator_fail(metac_recursive_iterator_t * p_iterator) {
    return metac_recursive_iterator_set_state(p_iterator, METAC_R_ITER_failed);
}

metac_recursive_iterator_t *metac_recursive_iterator_done(metac_recursive_iterator_t * p_iterator, void * p_out) {
    if (p_iterator == NULL) {
        return NULL;
    }
    assert(p_iterator->p_current != NULL);
    assert(!metac_r_iter_state_is_end(p_iterator->p_current->state));
    p_iterator->p_current->p_out = p_out;
    return metac_recursive_iterator_set_state(p_iterator, METAC_R_ITER_done);
}

int metac_recursive_iterator_dep_queue_is_empty(metac_recursive_iterator_t * p_iterator) {
    if (p_iterator == NULL) {
        return -(EINVAL);
    }
    assert(p_iterator->p_current);
    return LIST_EMPTY(&p_iterator->p_current->deps);
}

int metac_recursive_iterator_dep_queue_size(metac_recursive_iterator_t * p_iterator) {
    if (p_iterator == NULL) {
        return -(EINVAL);
    }
    assert(p_iterator->p_current);
    int size = 0;
    recursive_iterator_entry_t * p_entry = LIST_FIRST(&p_iterator->p_current->deps);
    while (p_entry != NULL) {
        ++size;
        p_entry = LIST_NEXT(p_entry, links);
    }
    return size;
}

int metac_recursive_iterator_create_and_append_dep(metac_recursive_iterator_t * p_iterator, void * p_in) {
    if (p_iterator == NULL) {
        return -(EINVAL);
    }
    assert(p_iterator->p_current);
    assert(!metac_r_iter_state_is_end(p_iterator->p_current->state));
    
    recursive_iterator_entry_t * p_new = metac_new_recursive_iterator_entry(p_iterator->p_current, p_in);
    if (p_new == NULL) {
        return -(ENOMEM);
    }

    if (LIST_EMPTY(&p_iterator->p_current->deps)) {
        assert(p_iterator->p_current->last_dep == NULL);
        LIST_INSERT_HEAD(&p_iterator->p_current->deps, p_new, links);
    }else {
        assert(p_iterator->p_current->last_dep != NULL);
        LIST_INSERT_AFTER(p_iterator->p_current->last_dep, p_new, links); 
    }
    p_iterator->p_current->last_dep = p_new;

    return 0;
}

void * metac_recursive_iterator_dequeue_and_delete_dep(metac_recursive_iterator_t * p_iterator, void ** pp_in, int * p_fail) {
    if (p_iterator == NULL) {
        return NULL;
    }
    assert(p_iterator->p_current);

    if (LIST_EMPTY(&p_iterator->p_current->deps)) {
        return NULL;
    }

    recursive_iterator_entry_t * p_entry = LIST_FIRST(&p_iterator->p_current->deps);
    assert(metac_r_iter_state_is_end(p_entry->state));
    LIST_REMOVE(p_entry, links);

    void * p_out = metac_recursive_iterator_entry_get_out(p_entry, pp_in, p_fail);
    metac_recursive_iterator_entry_free(p_entry);
    return p_out;
}

void * metac_recursive_iterator_get_dep_out(metac_recursive_iterator_t * p_iterator, int i, void ** pp_in, int * p_fail) {
    if (p_iterator == NULL) {
        return NULL;
    }
    assert(p_iterator->p_current);

    if (LIST_EMPTY(&p_iterator->p_current->deps)) {
        return NULL;
    }

    recursive_iterator_entry_t * p_entry = LIST_FIRST(&p_iterator->p_current->deps);
    while (i > 0) {
        --i;
        p_entry = LIST_NEXT(p_entry, links);
        if (p_entry == NULL) {
            return NULL;
        }
    }

    return metac_recursive_iterator_entry_get_out(p_entry, pp_in, p_fail);
}

