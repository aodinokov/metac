#include "metac/backend/entry.h"
#include "metac/backend/helpers.h"

#include <assert.h> /* assert */
#include <errno.h> /* EINVAL... */
#include <stdlib.h> /*calloc, free*/
#include <string.h> /*memcpy*/

static metac_entry_t * _init_entry(metac_entry_t *p_entry, metac_entry_t *p_entry_from) {
    _check_(p_entry == NULL || p_entry_from == NULL, NULL);
    memcpy(p_entry, p_entry_from, sizeof(*p_entry));
    return p_entry;
}

metac_flag_t metac_entry_is_dynamic(metac_entry_t *p_entry) {
    _check_(p_entry == NULL, 0);
    return p_entry->dynamically_allocated;
}

metac_entry_t * metac_new_entry(metac_entry_t *p_entry_from) {
    _check_(p_entry_from == NULL, NULL);

    metac_entry_t * p_entry = calloc(1, sizeof(*p_entry));
    _check_(p_entry == NULL, NULL);

    _init_entry(p_entry, p_entry_from);
    p_entry->dynamically_allocated = 1;
    return p_entry;
}

void metac_entry_delete(metac_entry_t * p_entry) {
    if (metac_entry_is_dynamic(p_entry) == 0) {
        return;
    }
    // if needed we can make this recursivly with subtypes.. now it's not needed yet
    free(p_entry);
}

metac_kind_t metac_entry_kind(metac_entry_t * p_entry) {
    _check_(p_entry == NULL, METAC_KND_invalid);
    return p_entry->kind;
}

metac_name_t metac_entry_name(metac_entry_t * p_entry) {
    _check_(p_entry == NULL, NULL);
    return p_entry->name;
}

metac_num_t metac_entry_parent_count(metac_entry_t * p_entry) {
    _check_(p_entry == NULL, 0);
    return p_entry->parents_count;
}

metac_entry_t * metac_entry_parent_entry(metac_entry_t * p_entry, metac_num_t id) {
    _check_(p_entry == NULL, NULL);
    _check_(id < 0 || id >= p_entry->parents_count, NULL);
    return p_entry->parents[id];
}

int metac_entry_byte_size(metac_entry_t *p_entry, metac_size_t *p_sz) {
    _check_(p_sz == NULL, -(EINVAL));
    metac_size_t multipier = 1;
    while (p_entry != NULL) {
        switch (p_entry->kind) {
        case METAC_KND_base_type:
            *p_sz = p_entry->base_type_info.byte_size * multipier;
            return 0;
        case METAC_KND_enumeration_type:
            *p_sz =  p_entry->enumeration_type_info.byte_size * multipier;
            return 0;
        case METAC_KND_pointer_type:
            *p_sz =  sizeof(void*) * multipier;
            return 0;
        case METAC_KND_class_type:
        case METAC_KND_union_type:
        case METAC_KND_struct_type:
            *p_sz =  p_entry->structure_type_info.byte_size * multipier;
            return 0;
        case METAC_KND_reference_type:
	    case METAC_KND_rvalue_reference_type:
            *p_sz =  p_entry->reference_type_info.byte_size * multipier;
            return 0;
        case METAC_KND_variable:
            p_entry = p_entry->variable_info.type;
            continue;
        case METAC_KND_subprogram_parameter:
            p_entry = p_entry->subprogram_parameter_info.type;
            continue;
        case METAC_KND_member:
            p_entry = p_entry->member_info.type;
            continue;
        case METAC_KND_typedef:
            p_entry = p_entry->typedef_info.type;
            continue;
        /* qual types */
        case METAC_KND_const_type:
        case METAC_KND_volatile_type:
        case METAC_KND_restrict_type:
        case METAC_KND_mutable_type:
        case METAC_KND_shared_type:
        case METAC_KND_packed_type:
            p_entry = p_entry->qual_type_info.type;
            continue;
        case METAC_KND_array_type:
            if (p_entry->array_type_info.count <= 0) {
                return -(EFAULT); // not sure, maybe need to return 0?
            }
            multipier *= p_entry->array_type_info.count;
            p_entry = p_entry->array_type_info.type;
            continue;
        default:
            return -(EFAULT);
        }
    }
    return -EFAULT;
}

/**
 * metac_entry_final_entry
 * returns actual type of such entities as variable, struct/union/class member or subrogram parameter
 * the val can be a member, variable or parameter and simultaniously array, struct, base type and etc
*/
metac_entry_t * metac_entry_final_entry(metac_entry_t *p_entry, metac_quals_t * p_quals) {
    _check_(p_entry == NULL, NULL);
    metac_quals_t quals = { .flags = 0, };
    /* make it NON-recursivly */
    do {
        if (p_entry == NULL) {
            /* void */
            return p_entry;
        }

        switch (p_entry->kind) {
        case METAC_KND_typedef:
            p_entry = p_entry->typedef_info.type;
            break;
        case METAC_KND_variable:
            p_entry = p_entry->variable_info.type;
            break;
        case METAC_KND_subprogram_parameter:
            p_entry = p_entry->subprogram_parameter_info.type;
            break;
        case METAC_KND_member:
            p_entry = p_entry->member_info.type;
            break;
        /* qual types */
        case METAC_KND_const_type:
        case METAC_KND_volatile_type:
        case METAC_KND_restrict_type:
        case METAC_KND_mutable_type:
        case METAC_KND_shared_type:
        case METAC_KND_packed_type:
            quals.flag_const |= (p_entry->kind == METAC_KND_const_type);
            quals.flag_volatile |= (p_entry->kind == METAC_KND_volatile_type);
            quals.flag_restrict |= (p_entry->kind == METAC_KND_restrict_type);
            quals.flag_mutable |= (p_entry->kind == METAC_KND_mutable_type);
            quals.flag_shared |= (p_entry->kind == METAC_KND_shared_type);
            quals.flag_packed |= (p_entry->kind == METAC_KND_packed_type);
            p_entry = p_entry->qual_type_info.type;
            break;
        default:
            if (p_quals != NULL) {
                p_quals->flags = quals.flags;
            }
            return p_entry;
        }
    }while(1);
}

static metac_entry_t * _entry_with_base_type_info(metac_entry_t *p_entry) {
    _check_(p_entry == NULL, NULL);
    
    metac_entry_t * p_final_entry = metac_entry_final_entry(p_entry, NULL);
    _check_(p_final_entry == NULL, NULL);
    _check_(metac_entry_kind(p_final_entry) != METAC_KND_base_type, NULL);

    return p_final_entry;
}

static int _entry_check_base_type(metac_entry_t *p_entry, metac_name_t expected_name, metac_encoding_t expected_encoding, metac_size_t expected_size) {
    _check_(p_entry == NULL, -(EINVAL));
    _check_(p_entry->kind != METAC_KND_base_type, -(EINVAL));

    assert(p_entry->name != NULL);

    if (strcmp(p_entry->name, expected_name) == 0 && (
            p_entry->base_type_info.encoding == METAC_ENC_undefined || 
            p_entry->base_type_info.encoding == expected_encoding) && (
            p_entry->base_type_info.byte_size == 0 ||
            p_entry->base_type_info.byte_size == expected_size)) {
        return 0;
    }

    return -(EINVAL);
}

metac_flag_t metac_entry_is_base_type(metac_entry_t *p_entry) {
    return _entry_with_base_type_info(p_entry) != NULL;
}

metac_name_t metac_entry_base_type_name(metac_entry_t *p_entry) {
    metac_entry_t * p_final_entry = _entry_with_base_type_info(p_entry);
    _check_(p_final_entry == NULL, NULL);
    return metac_entry_name(p_final_entry);
}

int metac_entry_base_type_byte_size(metac_entry_t *p_entry, metac_size_t *p_sz) {
    metac_entry_t * p_final_entry = _entry_with_base_type_info(p_entry);
    _check_(p_final_entry == NULL, -(EINVAL));
    if (p_sz != NULL) {
        *p_sz = p_final_entry->base_type_info.byte_size;
    }
    return 0;
}

int metac_entry_base_type_encoding(metac_entry_t *p_entry, metac_encoding_t *p_enc) {
    metac_entry_t * p_final_entry = _entry_with_base_type_info(p_entry);
    _check_(p_final_entry == NULL, -(EINVAL));
    if (p_enc != NULL) {
        *p_enc = p_final_entry->base_type_info.encoding;
    }
    return 0;
}

int metac_entry_check_base_type(metac_entry_t *p_entry, metac_name_t type_name, metac_encoding_t encoding, metac_size_t var_size) {
    metac_entry_t * p_final_entry = _entry_with_base_type_info(p_entry);
    _check_(p_final_entry == NULL, -(EINVAL));

    int err = _entry_check_base_type(p_final_entry, type_name, encoding, var_size);
    if (err != 0) {
        return err;
    }
    return 0;
}

static metac_entry_t * _entry_with_pointer_info(metac_entry_t *p_entry) {
    _check_(p_entry == NULL, NULL);
    
    metac_entry_t * p_final_entry = metac_entry_final_entry(p_entry, NULL);
    _check_(p_final_entry == NULL, NULL);
    _check_(p_final_entry->kind != METAC_KND_pointer_type, NULL);

    return p_final_entry;
}

metac_flag_t metac_entry_is_pointer(metac_entry_t *p_entry) {
    return _entry_with_pointer_info(p_entry) != NULL;
}

metac_flag_t metac_entry_is_void_pointer(metac_entry_t *p_entry) {
    _check_(p_entry == NULL, 0);
    metac_entry_t * p_final_entry = _entry_with_pointer_info(p_entry);
    return p_final_entry != NULL && p_final_entry->pointer_type_info.type == NULL;
}

metac_flag_t metac_entry_is_declaration_pointer(metac_entry_t *p_entry) {
    _check_(p_entry == NULL, 0);
    metac_entry_t * p_final_entry = _entry_with_pointer_info(p_entry);
    if (p_final_entry != NULL && p_final_entry->pointer_type_info.type != NULL) {
        return p_final_entry->pointer_type_info.type->declaration != 0;
    }
    return 0;
}

metac_entry_t * metac_entry_pointer_entry(metac_entry_t *p_entry) {
    metac_entry_t * p_final_entry = _entry_with_pointer_info(p_entry);
    if (p_final_entry != NULL) {
        return p_final_entry->pointer_type_info.type;
    }
    return NULL;
}

static metac_entry_t * _entry_with_struct_info(metac_entry_t *p_entry) {
    _check_(p_entry == NULL, NULL);
    
    metac_entry_t * p_final_entry = metac_entry_final_entry(p_entry, NULL);
    _check_(p_final_entry == NULL, NULL);
    _check_(
        p_final_entry->kind != METAC_KND_struct_type &&
        p_final_entry->kind != METAC_KND_union_type &&
        p_final_entry->kind != METAC_KND_class_type, NULL);

    return p_final_entry;
}

metac_flag_t metac_entry_has_members(metac_entry_t *p_entry) {
    return _entry_with_struct_info(p_entry) != NULL;
}

metac_num_t metac_entry_member_count(metac_entry_t *p_entry) {
    metac_entry_t * p_final_entry = _entry_with_struct_info(p_entry);
    _check_(p_final_entry == NULL, -(EINVAL));
    return p_final_entry->structure_type_info.members_count;
}

metac_num_t metac_entry_member_name_to_id(metac_entry_t *p_entry, metac_name_t name) {
    metac_entry_t * p_final_entry = _entry_with_struct_info(p_entry);
    _check_(p_final_entry == NULL, -(EINVAL));
    for (metac_num_t i = 0; i < p_final_entry->structure_type_info.members_count; ++i) {
        if (p_final_entry->structure_type_info.members[i].name != NULL &&
            strcmp(p_final_entry->structure_type_info.members[i].name, name) == 0) {
                return i;
            }
    }
    return -(ENOENT);
}

metac_entry_t * metac_entry_by_member_id(metac_entry_t *p_entry, metac_num_t member_id) {
    metac_entry_t * p_final_entry = _entry_with_struct_info(p_entry);
    _check_(p_final_entry == NULL, NULL);
    _check_(member_id < 0 || member_id >= p_final_entry->structure_type_info.members_count, NULL);
    return  &p_final_entry->structure_type_info.members[member_id];
}

metac_entry_t * metac_entry_by_member_ids(metac_entry_t * p_entry, metac_flag_t final, metac_entry_id_t* p_ids, metac_num_t ids_count) {
    for (metac_num_t i = 0; i < ids_count; ++i) {
        _check_(p_entry == NULL, NULL);
        metac_entry_t * p_final_entry = _entry_with_struct_info(p_entry);
        _check_(p_final_entry == NULL, NULL);

        metac_num_t id = p_ids[i].i;
        if (p_ids[i].n != NULL) { /*find id */
            id = metac_entry_member_name_to_id(p_final_entry, p_ids[i].n);
            if (id < 0) {
                return NULL;
            }
        }
        p_entry = metac_entry_by_member_id(p_final_entry, id);
    }
    _check_(p_entry == NULL, NULL);
    if (final == 0) {
        return p_entry;
    }
    return metac_entry_final_entry(p_entry, NULL);
}

metac_flag_t metac_entry_is_member(metac_entry_t *p_entry) {
    _check_(p_entry == NULL, 0);
    return metac_entry_kind(p_entry) == METAC_KND_member;
}

metac_entry_t * metac_entry_member_entry(metac_entry_t *p_entry) {
    _check_(p_entry == NULL, NULL);
    _check_(metac_entry_kind(p_entry) != METAC_KND_member, NULL);
    return p_entry->member_info.type;
}

int metac_entry_member_raw_location_info(metac_entry_t *p_entry, struct metac_member_raw_location_info * p_bitfields_raw_info) {
    _check_(p_entry == NULL, -(EINVAL));
    _check_(metac_entry_kind(p_entry) != METAC_KND_member, -(EINVAL));
    if (p_bitfields_raw_info != NULL) {
        p_bitfields_raw_info->byte_offset = p_entry->member_info.byte_offset;
        p_bitfields_raw_info->p_byte_size = p_entry->member_info.p_byte_size;
        p_bitfields_raw_info->p_data_bit_offset = p_entry->member_info.p_data_bit_offset;
        p_bitfields_raw_info->p_bit_offset = p_entry->member_info.p_bit_offset;
        p_bitfields_raw_info->p_bit_size = p_entry->member_info.p_bit_size;
    }
    return 0;
}

/**
 * returns 3 important values: byte_offset, bit_offset and bit_size (in dwarf 4 meaning)
*/
int metac_entry_member_bitfield_offsets(metac_entry_t *p_memb_entry,
    metac_offset_t *p_byte_offset, metac_offset_t *p_bit_offset, metac_offset_t *p_bit_size) {
    _check_(p_memb_entry == NULL, -(EINVAL));
    _check_(metac_entry_kind(p_memb_entry) != METAC_KND_member, -(EINVAL));
    _check_(p_byte_offset == NULL || p_bit_offset == NULL || p_bit_size == NULL, -(EINVAL));

    /* depedning on dwarf version there are options how to fill in the 3 vars above */
    if (p_memb_entry->member_info.p_bit_offset != NULL) { /* dwarf 3 */
        // in data
        metac_offset_t _bit_offset = (p_memb_entry->member_info.p_bit_offset?(*p_memb_entry->member_info.p_bit_offset):0);
        metac_offset_t _byte_size = (p_memb_entry->member_info.p_byte_size?(*p_memb_entry->member_info.p_byte_size):0);

        // check if we have enough info - dwarf 3 must have this, we also check it in metacdb_builder.go, but this is just in case:
        _check_(_byte_size == 0,  -(EINVAL));

        *p_bit_size = (p_memb_entry->member_info.p_bit_size?(*p_memb_entry->member_info.p_bit_size):0);

        // calculate complete data_bit_offset
        metac_offset_t _data_bit_offset = (p_memb_entry->member_info.byte_offset + _byte_size) * 8 - (_bit_offset + (*p_bit_size));

        // out
        *p_byte_offset = _data_bit_offset >> 3;
        *p_bit_offset = _data_bit_offset & 0x7;
    } else  if (p_memb_entry->member_info.p_data_bit_offset != NULL) { /* dwarf 4 */
        // in data

        metac_offset_t _data_bit_offset = *p_memb_entry->member_info.p_data_bit_offset;
        *p_bit_size = p_memb_entry->member_info.p_bit_size != NULL?(*p_memb_entry->member_info.p_bit_size): 0;
        // out

        *p_byte_offset = _data_bit_offset >> 3;
        *p_bit_offset = _data_bit_offset & 0x7;
    }

    return 0;
}

static metac_entry_t * _entry_with_array_info(metac_entry_t * p_entry) {
    _check_(p_entry == NULL, NULL);
    
    metac_entry_t * p_final_entry = metac_entry_final_entry(p_entry, NULL);
    _check_(p_final_entry == NULL, NULL);
    _check_(metac_entry_kind(p_final_entry) != METAC_KND_array_type, NULL);

    return p_final_entry;
}

metac_flag_t metac_entry_has_elements(metac_entry_t * p_entry) {
    return _entry_with_array_info(p_entry) != NULL;
}

metac_num_t metac_entry_element_count(metac_entry_t * p_entry) {
    metac_entry_t * p_final_entry = _entry_with_array_info(p_entry);
    _check_(p_final_entry == NULL, -(EINVAL));
    return p_final_entry->array_type_info.count;
}

metac_entry_t * metac_entry_element_entry(metac_entry_t *p_entry) {
    metac_entry_t * p_final_entry = _entry_with_array_info(p_entry);
    if (p_final_entry != NULL) {
        return p_final_entry->array_type_info.type;
    }
    return NULL;
}

static metac_entry_t * _entry_with_enumeration_info(metac_entry_t * p_entry) {
    _check_(p_entry == NULL, NULL);
    
    metac_entry_t * p_final_entry = metac_entry_final_entry(p_entry, NULL);
    _check_(p_final_entry == NULL, NULL);
    _check_(metac_entry_kind(p_final_entry) != METAC_KND_enumeration_type, NULL);

    return p_final_entry;
}

metac_flag_t metac_entry_is_enumeration(metac_entry_t *p_entry) {
    return _entry_with_enumeration_info(p_entry) != NULL;
}

metac_name_t metac_entry_enumeration_to_name(metac_entry_t * p_entry, metac_const_value_t var) {
    metac_entry_t * p_final_entry = _entry_with_enumeration_info(p_entry);
    _check_(p_final_entry == NULL, NULL);

    for (metac_num_t i = 0; i < p_final_entry->enumeration_type_info.enumerators_count; ++i) {
        if (p_final_entry->enumeration_type_info.enumerators[i].const_value == var) {
            return p_final_entry->enumeration_type_info.enumerators[i].name;
        }
    }
    return NULL;
}

metac_num_t metac_entry_enumeration_info_size(metac_entry_t * p_entry) {
    metac_entry_t * p_final_entry = _entry_with_enumeration_info(p_entry);
    _check_(p_final_entry == NULL, 0);
    return p_final_entry->enumeration_type_info.enumerators_count;
}

struct metac_type_enumerator_info const * metac_entry_enumeration_info(metac_entry_t * p_entry, metac_num_t id) {
    metac_entry_t * p_final_entry = _entry_with_enumeration_info(p_entry);
    _check_(p_final_entry == NULL, NULL);
    _check_(id < 0 || id >= p_final_entry->enumeration_type_info.enumerators_count, NULL);
    return &p_final_entry->enumeration_type_info.enumerators[id];
}

static metac_entry_t * _entry_with_paremeter_info(metac_entry_t *p_entry) {
    _check_(p_entry == NULL, NULL);
    
    metac_entry_t * p_final_entry = metac_entry_final_entry(p_entry, NULL);
    _check_(p_final_entry == NULL, NULL);
    _check_(
        p_final_entry->kind != METAC_KND_subprogram, NULL);

    return p_final_entry;
}

metac_flag_t metac_entry_has_paremeter(metac_entry_t * p_entry) {
    return _entry_with_paremeter_info(p_entry) != NULL;
}

metac_num_t metac_entry_paremeter_count(metac_entry_t *p_entry) {
    metac_entry_t * p_final_entry = _entry_with_paremeter_info(p_entry);
    _check_(p_final_entry == NULL, -(EINVAL));
    return p_final_entry->subprogram_info.parameters_count;
}

metac_num_t metac_entry_paremeter_name_to_id(metac_entry_t *p_entry, metac_name_t name) {
    metac_entry_t * p_final_entry = _entry_with_paremeter_info(p_entry);
    _check_(p_final_entry == NULL, -(EINVAL));
    metac_num_t par_count = metac_entry_paremeter_count(p_entry);
    _check_(par_count < 0, -(EFAULT));
    for (metac_num_t i = 0; i < par_count; ++i) {
        if (p_final_entry->subprogram_info.parameters[i].name != NULL &&
            strcmp(p_final_entry->subprogram_info.parameters[i].name, name) == 0) {
                return i;
            }
    }
    return -(ENOENT);
}

metac_entry_t * metac_entry_by_paremeter_id(metac_entry_t *p_entry, metac_num_t param_id) {
    metac_entry_t * p_final_entry = _entry_with_paremeter_info(p_entry);
    _check_(p_final_entry == NULL, NULL);
    _check_(param_id < 0 || param_id >= p_final_entry->subprogram_info.parameters_count, NULL);
    return  &p_final_entry->subprogram_info.parameters[param_id];
}

metac_entry_t * metac_entry_by_parameter_ids(metac_entry_t * p_entry, metac_flag_t final, metac_entry_id_t* p_ids) {
     metac_num_t ids_count = 1;

    _check_(p_entry == NULL, NULL);
    metac_entry_t * p_final_entry = _entry_with_paremeter_info(p_entry);
    _check_(p_final_entry == NULL, NULL);

    metac_num_t id = p_ids->i;
    if (p_ids->n != NULL) { /*find id */
        id = metac_entry_paremeter_name_to_id(p_final_entry, p_ids->n);
        if (id < 0) {
            return NULL;
        }
    }
    p_entry = metac_entry_by_paremeter_id(p_final_entry, id);
    _check_(p_entry == NULL, NULL);
    if (final == 0) {
        return p_entry;
    }
    return metac_entry_final_entry(p_entry, NULL);
}

metac_flag_t metac_entry_has_result(metac_entry_t * p_entry) {
    metac_entry_t * p_final_entry = _entry_with_paremeter_info(p_entry);
    _check_(p_final_entry == NULL, 0);
    return p_final_entry->subprogram_info.type != NULL;
}

metac_entry_t * metac_entry_result_type(metac_entry_t * p_entry) {
    metac_entry_t * p_final_entry = _entry_with_paremeter_info(p_entry);
    _check_(p_final_entry == NULL, NULL);
    return p_final_entry->subprogram_info.type;
}

metac_flag_t metac_entry_is_parameter(metac_entry_t * p_entry) {
    _check_(p_entry == NULL, 0);
    return metac_entry_kind(p_entry) == METAC_KND_subprogram_parameter;
}

metac_flag_t metac_entry_is_unspecified_parameter(metac_entry_t * p_entry) {
    _check_(p_entry == NULL || metac_entry_kind(p_entry) != METAC_KND_subprogram_parameter, 0);
    return p_entry->subprogram_parameter_info.unspecified_parameters;
}

metac_entry_t * metac_entry_parameter_entry(metac_entry_t *p_entry) {
    _check_(p_entry == NULL, NULL);
    _check_(metac_entry_kind(p_entry) != METAC_KND_subprogram_parameter, NULL);
    return p_entry->subprogram_parameter_info.type;
}

