/*
 * metac_type.c
 *
 *  Created on: Aug 31, 2015
 *      Author: mralex
 */

//#define METAC_DEBUG_ENABLE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>			/*uint64_t etc*/
#include <assert.h>
#include <errno.h>

#include "metac/base.h"

#include "oop.h"		    /*_create, _delete, ...*/
#include "debug.h"

static const metac_type_annotation_t*
_annotation_by_key(metac_type_annotation_t *annotations, const char *key) {
    int i = 0;
    if (annotations == NULL) {
        msg_stddbg("type %s doesn't have specifications\n",
                type->name != NULL ? type->name : "(nil)");
        return NULL;
    }
    while (annotations[i].key != NULL) {
        if (strcmp(annotations[i].key, key) == 0)
            return &annotations[i];
        ++i;
    }
    return NULL;
}

const metac_type_annotation_t*
metac_type_annotation(struct metac_type *type, const char *key,
        metac_type_annotation_t *override_annotations) {
    const metac_type_annotation_t *result;

    if (type == NULL) {
        msg_stderr("invalid argument value: return NULL\n");
        return NULL;
    }

    if (override_annotations != NULL) {
        result = _annotation_by_key(override_annotations, key);
        if (result != NULL)
            return result;
    }
    return _annotation_by_key(type->annotations, key);
}

struct metac_type* metac_type_actual_type(struct metac_type *type) {
    if (type == NULL) {
        msg_stderr("invalid argument value: return NULL\n");
        return NULL;
    }
    while (type->kind == METAC_KND_typedef || type->kind == METAC_KND_const_type) {
        switch (type->kind) {
        case METAC_KND_typedef:
            type = type->typedef_info.type;
            break;
        case METAC_KND_const_type:
            type = type->const_type_info.type;
            break;
        }
        if (type == NULL) {
            msg_stderr(
                    "typedef has to contain type in attributes: return NULL\n");
            return NULL;
        }
    }
    return type;
}

metac_byte_size_t metac_type_byte_size(struct metac_type *type) {
    if (type == NULL) {
        msg_stderr("invalid argument value: type\n");
        return 0;
    }

    type = metac_type_actual_type(type);
    assert(type);

    switch (type->kind) {
    case METAC_KND_base_type:
        return type->base_type_info.byte_size;
    case METAC_KND_enumeration_type:
        return type->enumeration_type_info.byte_size;
    case METAC_KND_structure_type:
        return type->structure_type_info.byte_size;
    case METAC_KND_union_type:
        return type->union_type_info.byte_size;
    case METAC_KND_pointer_type:
        if (type->pointer_type_info.byte_size > 0)
            return type->pointer_type_info.byte_size;
        return sizeof(void*);
    case METAC_KND_array_type:
        return type->array_type_info.elements_count
                * metac_type_byte_size(type->array_type_info.type);
    case METAC_KND_subprogram:
        return sizeof(metac_type_byte_size); /*it's always a constant*/
    }
    return 0;
}

metac_name_t metac_type_name(struct metac_type *type) {
    if (type == NULL) {
        msg_stderr("invalid argument value: type\n");
        return NULL;
    }
    return type->name;
}

int metac_type_enumeration_type_get_value(struct metac_type *type,
        metac_name_t name, metac_const_value_t *p_const_value) {
    metac_num_t i;
    if (type == NULL) {
        msg_stderr("invalid argument value: return NULL\n");
        return -(EINVAL);
    }
    if (type->kind != METAC_KND_enumeration_type) {
        msg_stderr("invalid argument type: return NULL\n");
        return -(EINVAL);
    }

    for (i = 0; i < type->enumeration_type_info.enumerators_count; ++i) {
        if (strcmp(name, type->enumeration_type_info.enumerators[i].name)
                == 0) {
            if (p_const_value != NULL)
                *p_const_value =
                        type->enumeration_type_info.enumerators[i].const_value;
            return 0;
        }
    }
    msg_stddbg("wan't able to find const_value for %s\n", name);
    return -(EFAULT);
}

metac_name_t metac_type_enumeration_type_get_name(struct metac_type *type,
        metac_const_value_t const_value) {
    metac_num_t i;
    if (type == NULL) {
        msg_stderr("invalid argument value: return NULL\n");
        return NULL;
    }
    if (type->kind != METAC_KND_enumeration_type) {
        msg_stderr("invalid argument type: return NULL\n");
        return NULL;
    }

    for (i = 0; i < type->enumeration_type_info.enumerators_count; ++i) {
        if (type->enumeration_type_info.enumerators[i].const_value
                == const_value) {
            return type->enumeration_type_info.enumerators[i].name;
        }
    }

    msg_stddbg("Wan't able to find const_value for 0x%Lx\n",
            (unsigned long long )const_value);
    return NULL;
}

int metac_type_array_subrange_count(struct metac_type *type,
        metac_num_t subrange_id, metac_count_t *p_count) {
    if (type == NULL) {
        msg_stderr("invalid argument value: type\n");
        return -(EINVAL);
    }
    if (type->kind != METAC_KND_array_type) {
        msg_stderr("type id isn't ARRAY\n");
        return -(EINVAL);
    }
    if (subrange_id >= type->array_type_info.subranges_count) {
        msg_stderr("subrange_id is too big\n");
        return -(EINVAL);
    }

    if (p_count != NULL)
        *p_count = type->array_type_info.subranges[subrange_id].count;
    return type->array_type_info.subranges[subrange_id].is_flexible;
}

int metac_type_array_member_location(struct metac_type *type,
        metac_num_t subranges_count, metac_num_t *subranges,
        metac_data_member_location_t *p_data_member_location) {
    int res = 0;
    metac_num_t i;
    metac_data_member_location_t offset = 0;
    metac_data_member_location_t ratio = 1;

    if (type == NULL) {
        msg_stderr("invalid argument value: type\n");
        return -(EINVAL);
    }
    if (type->kind != METAC_KND_array_type) {
        msg_stderr("type id isn't ARRAY\n");
        return -(EINVAL);
    }
    if (subranges_count > type->array_type_info.subranges_count) {
        msg_stderr("subranges_count is too big\n");
        return -(EINVAL);
    }

    for (i = type->array_type_info.subranges_count; i > 0; i--) {
        metac_num_t index = i - 1;
        metac_num_t current_index_value =
                (index < subranges_count) ? subranges[index] : 0;
        if (current_index_value
                < type->array_type_info.subranges[index].lower_bound) {
            msg_stderr(
                    "ERROR: %x index must be in range [%x, %x), but it's %x\n",
                    index, type->array_type_info.subranges[index].lower_bound,
                    type->array_type_info.subranges[index].lower_bound
                            + type->array_type_info.subranges[index].count,
                    current_index_value);
            return -(EINVAL);
        }
        if (current_index_value
                >= type->array_type_info.subranges[index].lower_bound
                        + type->array_type_info.subranges[index].count) {
            msg_stddbg(
                    "WARNING: %x index should be in range [%x, %x), but it's %x\n",
                    index, type->array_type_info.subranges[index].lower_bound,
                    type->array_type_info.subranges[index].lower_bound
                            + type->array_type_info.subranges[index].count,
                    current_index_value);
            ++res;
        }

        current_index_value -=
                type->array_type_info.subranges[index].lower_bound;
        offset += ratio * current_index_value;
        msg_stddbg("DBG: index %d, current index %d, ratio %d\n", index,
                current_index_value, ratio);
        ratio *= type->array_type_info.subranges[index].count;

    }
    msg_stddbg("DBG: offset %d\n", offset);
    if (p_data_member_location)
        *p_data_member_location = offset
                * metac_type_byte_size(type->array_type_info.type);
    return res;
}

/*****************************************************************************/
static inline struct metac_type* metac_unknown_object_get_metac_type(
        struct metac_unknown_object *p_metac_unknown_object) {
return _container_of(
        p_metac_unknown_object,
        struct metac_type,
        refcounter_object.unknown_object);
}

static int metac_type_delete(struct metac_type **pp_metac_type) {
_delete_(metac_type);
return 0;
}

static int _metac_type_unknown_object_delete(
    struct metac_unknown_object *p_metac_unknown_object) {

struct metac_type *p_metac_type;

if (p_metac_unknown_object == NULL) {
    msg_stderr("p_metac_unknown_object is NULL\n");
    return -(EINVAL);
}

p_metac_type = metac_unknown_object_get_metac_type(p_metac_unknown_object);
if (p_metac_type == NULL) {
    msg_stderr("p_metac_type is NULL\n");
    return -(EINVAL);
}

return metac_type_delete(&p_metac_type);
}

static metac_refcounter_object_ops_t _metac_type_refcounter_object_ops = {
    .unknown_object_ops = { .metac_unknown_object_delete =
            _metac_type_unknown_object_delete, }, };

struct metac_type* metac_type_create_pointer_for(struct metac_type *p_type) {

_create_(metac_type);

if (metac_refcounter_object_init(&p_metac_type->refcounter_object,
        &_metac_type_refcounter_object_ops, NULL) != 0) {
    msg_stderr("metac_refcounter_object_init failed\n");

    metac_type_delete(&p_metac_type);
    return NULL;
}

p_metac_type->kind = METAC_KND_pointer_type;
p_metac_type->pointer_type_info.type = p_type;

return p_metac_type;
}

struct metac_type* metac_type_get(struct metac_type *p_metac_type) {
if (p_metac_type != NULL
        && metac_refcounter_object_get(&p_metac_type->refcounter_object)
                != NULL) {
    return p_metac_type;
}
return NULL;
}

int metac_type_put(struct metac_type **pp_metac_type) {
if (pp_metac_type != NULL && (*pp_metac_type) != NULL
        && metac_refcounter_object_put(&(*pp_metac_type)->refcounter_object)
                == 0) {
    *pp_metac_type = NULL;
    return 0;
}
return -(EFAULT);
}

struct metac_type* metac_type_get_actual_type(struct metac_type *p_metac_type) {

struct metac_type *p_metac_type_actual = metac_type_actual_type(p_metac_type);

if (p_metac_type_actual != NULL) {

    return metac_type_get(p_metac_type_actual);
}

return NULL;
}

/*****************************************************************************/
struct metac_type* metac_type_by_name(struct metac_type_sorted_array *array,
    metac_name_t name) {
if (array == NULL || name == NULL)
    return NULL;
/*binary search*/
metac_num_t min = 0, max = array->number - 1;
do {
    metac_num_t i = min + (max - min) / 2;
    int cmp_res = strcmp(array->item[i].name, name);
    switch ((cmp_res > 0) ? (1) : ((cmp_res < 0) ? (-1) : (0))) {
    case 0:
        return array->item[i].ptr;
    case 1:
        max = i - 1;
        break;
    case -1:
        min = i + 1;
        break;
    }
} while (min <= max);
return NULL;
}

struct metac_object* metac_object_by_name(
    struct metac_object_sorted_array *array, metac_name_t name) {
if (array == NULL || name == NULL)
    return NULL;
/*binary search*/
metac_num_t min = 0, max = array->number - 1;
do {
    metac_num_t i = min + (max - min) / 2;
    int cmp_res = strcmp(array->item[i].name, name);
    switch ((cmp_res > 0) ? (1) : ((cmp_res < 0) ? (-1) : (0))) {
    case 0:
        return array->item[i].ptr;
    case 1:
        max = i - 1;
        break;
    case -1:
        min = i + 1;
        break;
    }
} while (min <= max);
return NULL;
}
/*****************************************************************************/
int metac_array_elements_single(char *annotation_key, int write_operation,
    void *ptr, metac_type_t *type, void *first_element_ptr,
    metac_type_t *first_element_type, metac_num_t *p_subrange0_count,
    void *array_elements_count_cb_context) {
metac_count_t i;
if (write_operation == 0) {
    if (p_subrange0_count)
        *p_subrange0_count = 1;
    return 0;
}
return 0;
}

int metac_array_elements_1d_with_null(char *annotation_key, int write_operation,
    void *ptr, metac_type_t *type, void *first_element_ptr,
    metac_type_t *first_element_type, metac_num_t *p_subrange0_count,
    void *array_elements_count_cb_context) {

if (write_operation == 0) {
    metac_byte_size_t j;
    metac_byte_size_t element_size = metac_type_byte_size(first_element_type);
    metac_count_t i = 0;
    msg_stddbg("elements_size %d\n", (int )element_size);
    do {
        unsigned char *_ptr;
        for (j = 0; j < element_size; j++) { /*non optimal - can use different sized to char,short,int &etc, see memcmp for reference*/
            _ptr = ((unsigned char*) first_element_ptr) + i * element_size + j;
            if ((*_ptr) != 0) {
                ++i;
                break;
            }
        }
        if ((*_ptr) == 0)
            break;
    } while (1);
    /*found all zeroes*/
    ++i;
    if (p_subrange0_count)
        *p_subrange0_count = i;
    msg_stddbg("p_elements_count %d\n", (int )i);
    return 0;
}
return 0;
}

