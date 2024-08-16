#include "metac/backend/entry.h"
#include "metac/backend/helpers.h"
#include "metac/backend/iterator.h"
#include "metac/backend/value.h"
#include "metac/endian.h"
#include "metac/reflect.h"

#include <assert.h> /* assert */
#include <stdlib.h> /* calloc */
#include <errno.h> /* EINVAL... */
#include <string.h> /* strcmp */
#include <inttypes.h> /* PRIi8... */

#include "metac/backend/string.h"
#ifndef dsprintf
dsprintf_render_with_buf(64)
#define dsprintf(_x_...) dsprintf64( _x_)
#endif

struct metac_value_walker_hierarchy {
    metac_recursive_iterator_t * p_iterator;
};

int metac_value_walker_hierarchy_level(metac_value_walker_hierarchy_t *p_hierarchy) {
    _check_(p_hierarchy == NULL, -(EINVAL));
    _check_(p_hierarchy->p_iterator == NULL, -(EINVAL));
    return metac_recursive_iterator_level(p_hierarchy->p_iterator);
}

metac_value_t * metac_value_walker_hierarchy_value(metac_value_walker_hierarchy_t *p_hierarchy, int req_level) {
    _check_(p_hierarchy == NULL, NULL);
    _check_(p_hierarchy->p_iterator == NULL, NULL);
    return (metac_value_t *)metac_recursive_iterator_get_in(p_hierarchy->p_iterator, req_level);
}

int metac_value_event_handler_call(metac_value_event_handler_t handler, metac_recursive_iterator_t * p_iterator, metac_value_event_t * p_ev, void *p_context) {
    _check_(handler == NULL, -(EINVAL));
    metac_value_walker_hierarchy_t hierarchy = {
        .p_iterator = p_iterator,
    };
    return handler(&hierarchy, p_ev, p_context);
}

int metac_value_level_introduced_loop(metac_recursive_iterator_t * p_iterator) {
    int level = metac_recursive_iterator_level(p_iterator);
    if (level < 1) {
        return -1;
    }
    metac_value_t * p_cur_level = metac_recursive_iterator_get_in(p_iterator, 0);
    assert(p_cur_level != NULL);
    if (p_cur_level == NULL) {
        return -1;
    }
    if (metac_value_is_pointer(p_cur_level) == 0) {
        return -1;
    }
    void * p_val = NULL;
    if (metac_value_pointer(p_cur_level, &p_val) != 0) {
        assert(0);
        return -1;
    }
    for (int l = 1; l < level + 1; ++l) { /* if level = 1 there are 2 levels: 0 and 1 */
        metac_value_t * p_cmp_level = metac_recursive_iterator_get_in(p_iterator, l);
        if (p_cmp_level == NULL) {
            continue;
        }
        if (metac_value_is_pointer(p_cmp_level) == 0) {
            continue;
        }
        void * p_cmp_val = NULL;
        if (metac_value_pointer(p_cmp_level, &p_cmp_val) != 0) {
            continue;
        }
        if (p_val == p_cmp_val) {
            return l;
        }
    }
    return -1;
}

/*
    generate fieldname we're looking based on the current fieldname and pattern
    if name is null, we don't accept %s in pattern
*/
static char * _dgenfieldname(char * flex_current_field_name, char * pattern){
    metac_flag_t is_pattern = (strstr(pattern, "%s") != NULL);
    
    if (flex_current_field_name == NULL && is_pattern != 0) {
        return NULL;
    }

    if (is_pattern != 0) {
        return dsprintf(pattern, flex_current_field_name);
    }

    return dsprintf(pattern);
} 

/*
bitfields helpers 
*/

static metac_flag_t _entry_bitfield_supported(metac_encoding_t encoding) {
    if (encoding != METAC_ENC_boolean &&
        encoding != METAC_ENC_unsigned_char &&
        encoding != METAC_ENC_unsigned &&
        encoding != METAC_ENC_signed_char &&
        encoding != METAC_ENC_signed) {
        return 0;
    }
    return 1;
}

/**
 * read bitfield to normal var
 * Note: base_addr pointing on the beginning of structure; 
*/
static int _entry_bitfield_read(metac_entry_t *p_memb_entry, void * base_addr, void * p_var, metac_size_t var_size, metac_flag_t correct_sign) {
    _check_(base_addr == NULL, -(EINVAL));
    _check_(metac_entry_kind(p_memb_entry) != METAC_KND_member, -(EINVAL));

    metac_offset_t byte_offset;
    metac_offset_t bit_offset;
    metac_offset_t bit_size;
    uint8_t buf[16];

    _check_(metac_entry_member_bitfield_offsets(p_memb_entry, &byte_offset, &bit_offset, &bit_size) != 0, -(EINVAL));

     /* TODO: Caution: this has been tested only with little-endian machines*/
    if (IS_LITTLE_ENDIAN) {
        base_addr += byte_offset;
        // read only number of bytes that we really need
        // TODO: make it without extra copy if possible
        // TODO: need to make it for both endians
        assert(bit_size != 0);
        memcpy(buf, base_addr, (bit_offset + bit_size-1)/8 +1);
        base_addr = buf;
    } else {
        assert(metac_entry_parent_count(p_memb_entry) == 1);
        metac_entry_t * p_memb_parent = metac_entry_parent_entry(p_memb_entry, 0);
        assert(p_memb_parent != NULL);
        metac_size_t parent_byte_size = 0;
        _check_(metac_entry_byte_size(p_memb_parent, &parent_byte_size) != 0, -(EFAULT));
        assert(parent_byte_size > 0);
        base_addr += parent_byte_size - byte_offset;
    }

#define _read_(_type_) \
    do{ \
        _type_ data = *((_type_*)base_addr); \
        data = (data >> bit_offset) & ((1 << bit_size) - 1); \
        \
        if ((correct_sign) && (data & (1 << (bit_size-1)))) { \
            data = ((-1) << ((bit_size))) ^ data; \
        } \
        *((_type_*)p_var) = data; \
    } while(0)

    if (var_size == sizeof(unsigned char)) {
        _read_(unsigned char);
    } else if (var_size == sizeof(unsigned short)) {
        _read_(unsigned short);
    } else if (var_size == sizeof(unsigned int)) {
        _read_(unsigned int);
    } else if (var_size == sizeof(unsigned long)) {
        _read_(unsigned long);
    } else if (var_size == sizeof(unsigned long long)) {
        _read_(unsigned long long);
    } else {
        return -(EINVAL);
    }
#undef _read_

    return 0;
}

static int _entry_bitfield_write(metac_entry_t *p_memb_entry, void * base_addr, void * p_var, metac_size_t var_size) {
    _check_(base_addr == NULL, -(EINVAL));
    _check_(metac_entry_kind(p_memb_entry) != METAC_KND_member, -(EINVAL));

    metac_offset_t byte_offset;
    metac_offset_t bit_offset;
    metac_offset_t bit_size;
    void * base_addr_orig = NULL;
    uint8_t buf[16];

    _check_(metac_entry_member_bitfield_offsets(p_memb_entry, &byte_offset, &bit_offset, &bit_size) != 0, -(EINVAL));

     /* TODO: Caution: this has been tested only with little-endian machines*/
    if (IS_LITTLE_ENDIAN) {
        base_addr += byte_offset;
        // write only number of bytes that we really need
        // TODO: make it without extra copy if possible
        // TODO: need to make it for both endians
        assert(bit_size != 0);
        base_addr_orig = base_addr;
        memcpy(buf, base_addr, (bit_offset + bit_size-1)/8 +1);
        base_addr = buf;
    } else {
        assert(metac_entry_parent_count(p_memb_entry) == 1);
        metac_entry_t * p_memb_parent = metac_entry_parent_entry(p_memb_entry, 0);
        assert(p_memb_parent != NULL);
        metac_size_t parent_byte_size = 0;
        _check_(metac_entry_byte_size(p_memb_parent, &parent_byte_size) != 0, -(EFAULT));
        assert(parent_byte_size > 0);
        base_addr += parent_byte_size - byte_offset;
    }

#define _write_(_type_) \
    do { \
        _type_ data = ((*(_type_*)p_var) & ((1 << bit_size) - 1)) << bit_offset; \
        *((_type_*)base_addr) = *((_type_*)base_addr) & (~(((1 << bit_size) - 1) << bit_offset )) | data;\
    } while (0)

    if (var_size == sizeof(unsigned char)) {
        _write_(unsigned char);
    } else if (var_size == sizeof(unsigned short)) {
        _write_(unsigned short);
    } else if (var_size == sizeof(unsigned int)) {
        _write_(unsigned int);
    } else if (var_size == sizeof(unsigned long)) {
        _write_(unsigned long);
    } else if (var_size == sizeof(unsigned long long)) {
        _write_(unsigned long long);
    } else {
        return -(EINVAL);
    }
#undef _write_

    if (IS_LITTLE_ENDIAN) {
        // write back only needed data
        memcpy(base_addr_orig, buf, (bit_offset + bit_size-1)/8 +1);
    }

    return 0;
}

/**
 * metac_value
*/
/* internal implementation */
struct metac_value {
    metac_entry_t *p_entry;
    void * addr;
};

metac_kind_t metac_value_kind(metac_value_t * p_val) {
    _check_(p_val == NULL, METAC_KND_invalid);
    assert(p_val->p_entry != NULL);

    return metac_entry_kind(p_val->p_entry);
}

metac_name_t metac_value_name(metac_value_t * p_val) {
    _check_(p_val == NULL, NULL);
    assert(p_val->p_entry != NULL);

    return metac_entry_name(p_val->p_entry);
}

metac_kind_t metac_value_final_kind(metac_value_t * p_val, metac_quals_t * p_quals) {
    _check_(p_val == NULL, METAC_KND_invalid);
    assert(p_val->p_entry != NULL);
    assert(p_val->addr != NULL);

    metac_entry_t * p_final_entry = metac_entry_final_entry(p_val->p_entry , p_quals);
    if (p_final_entry == NULL) {
        return METAC_KND_invalid;
    }

    return metac_entry_kind(p_final_entry);
}

static metac_entry_t * _value_with_base_type_info(metac_value_t *p_val) {
    _check_(p_val == NULL, NULL);
    
    metac_entry_t * p_final_entry = metac_entry_final_entry(p_val->p_entry, NULL);
    _check_(p_final_entry == NULL, NULL);
    _check_(metac_entry_kind(p_final_entry) != METAC_KND_base_type, NULL);

    return p_final_entry;
}

metac_flag_t metac_value_is_base_type(metac_value_t *p_val) {
    return _value_with_base_type_info(p_val) != NULL;
}

int metac_value_check_base_type(metac_value_t * p_val, metac_name_t type_name, metac_encoding_t encoding, metac_size_t var_size) {
    metac_entry_t * p_entry = metac_value_entry(p_val);
    if (p_entry == NULL) {
        return -(EINVAL);
    }

    // this funciton check final_entry
    int err = metac_entry_check_base_type(p_entry, type_name, encoding, var_size);
    if (err != 0) {
        return err;
    }
    return 0;
}

int metac_value_base_type(metac_value_t * p_val,
    metac_name_t type_name, metac_encoding_t encoding,
    void * p_var, metac_size_t var_size) {
    _check_((p_val == NULL) || (p_var == NULL), -(EINVAL));
    assert(p_val->p_entry != NULL);
    assert(p_val->addr != NULL);

    metac_entry_t * p_final_entry = metac_entry_final_entry(p_val->p_entry , NULL);
    if (p_final_entry == NULL) {
        return -(EINVAL);
    }

    int err = metac_entry_check_base_type(p_final_entry, type_name, encoding, var_size);
    if (err != 0) {
            return err;
    }

    if (metac_entry_kind(p_val->p_entry) != METAC_KND_member) { /* simple case - just copy from addr */
        memcpy(p_var, p_val->addr, var_size);
        return 0;
    }

    struct metac_member_raw_location_info bitfields_raw_info;
    if (metac_entry_member_raw_location_info(p_val->p_entry, &bitfields_raw_info) != 0) {
        return -(EFAULT);
    }
    if (bitfields_raw_info.p_bit_offset == NULL && /* member of the struct. addr points to the beginning of the struct */
        bitfields_raw_info.p_bit_size == NULL &&
        bitfields_raw_info.p_data_bit_offset == NULL) { /* 2nd simple case */
        memcpy(p_var, p_val->addr, var_size);
        return 0;
    }

    if (_entry_bitfield_supported(encoding)) {
        // because of DWARF 4 which introduced data_bit_offset we need to calc from the structure base addr;
        void * p_addr = p_val->addr - bitfields_raw_info.byte_offset;
        return _entry_bitfield_read(p_val->p_entry, p_addr, p_var, var_size, 
            encoding == METAC_ENC_signed_char || encoding == METAC_ENC_signed);
    }
    return -(ENOTSUP);
}

int metac_value_set_base_type(metac_value_t * p_val,
    metac_name_t type_name, metac_encoding_t encoding,
    void * p_var, metac_size_t var_size) {
    _check_((p_val == NULL) || (p_var == NULL), -(EINVAL));
    assert(p_val->p_entry != NULL);
    assert(p_val->addr != NULL);

    metac_quals_t quals = {.flags = 0};
    metac_entry_t * p_final_entry = metac_entry_final_entry(p_val->p_entry, &quals);
    if (p_final_entry == NULL) {
        return -(EINVAL);
    }

    if (quals.flag_const != 0) {
        return -(EPERM);
    }

    int err = metac_entry_check_base_type(p_final_entry, type_name, encoding, var_size);
    if (err != 0) {
            return err;
    }

    if (metac_entry_kind(p_val->p_entry) != METAC_KND_member) { /* simple case - just copy from addr */
        memcpy(p_val->addr, p_var, var_size);
        return 0;
    }
    
    struct metac_member_raw_location_info bitfields_raw_info;
    if (metac_entry_member_raw_location_info(p_val->p_entry, &bitfields_raw_info) != 0) {
        return -(EFAULT);
    }
    if (bitfields_raw_info.p_bit_offset == NULL && /* member of the struct. addr points to thsizeof(*p_var)e beginning of the struct */
        bitfields_raw_info.p_bit_size == NULL &&
        bitfields_raw_info.p_data_bit_offset == NULL) { /* 2nd simple case */
        memcpy(p_val->addr, p_var, var_size);
        return 0;
    }

    if (_entry_bitfield_supported(encoding)) {
        // because of DWARF 4 which introduced data_bit_offset we need to calc from the structure base addr;
        void * p_addr = p_val->addr - bitfields_raw_info.byte_offset;
        return _entry_bitfield_write(p_val->p_entry, p_addr, p_var, var_size);
    }
    return -(ENOTSUP);
}

static metac_entry_t * _value_with_array_info(metac_value_t *p_val) {
    _check_(p_val == NULL, NULL);
    
    metac_entry_t * p_final_entry = metac_entry_final_entry(p_val->p_entry, NULL);
    _check_(p_final_entry == NULL, NULL);
    _check_(metac_entry_kind(p_final_entry) != METAC_KND_array_type, NULL);

    return p_final_entry;
}

metac_flag_t metac_value_has_elements(metac_value_t *p_val) {
    return _value_with_array_info(p_val) != NULL;
}

metac_num_t metac_value_element_count(metac_value_t *p_val) {
    metac_entry_t * p_final_entry = _value_with_array_info(p_val);
    _check_(p_final_entry == NULL, -(EINVAL));
    return metac_entry_element_count(p_final_entry);
}

metac_value_t * metac_new_value_by_element_id(metac_value_t *p_val, metac_num_t element_id) {
    metac_entry_t * p_final_entry = _value_with_array_info(p_val);
    _check_(p_final_entry == NULL, NULL);
    metac_num_t element_count = metac_entry_element_count(p_final_entry);
    _check_(element_count < 0, NULL);
    _check_(element_id < 0 || element_id >= element_count, NULL);
    metac_entry_t * p_element_entry = metac_entry_element_entry(p_final_entry);
    _check_(p_element_entry == NULL, NULL);

    metac_size_t element_size = 0;
    _check_(metac_entry_byte_size(p_element_entry, &element_size) != 0, NULL);
    // TODO: Caution: array_type_info has p_stride_bit_size. This has been tested only when this param is NULL
    return  metac_new_value(p_element_entry, p_val->addr + element_size * element_id);
}

static metac_entry_t * _value_with_struct_info(metac_value_t *p_val) {
    _check_(p_val == NULL, NULL);
    
    metac_entry_t * p_final_entry = metac_entry_final_entry(p_val->p_entry, NULL);
    _check_(p_final_entry == NULL, NULL);
    _check_(
        metac_entry_kind(p_final_entry) != METAC_KND_struct_type &&
        metac_entry_kind(p_final_entry) != METAC_KND_union_type &&
        metac_entry_kind(p_final_entry) != METAC_KND_class_type, NULL);

    return p_final_entry;
}

metac_flag_t metac_value_has_members(metac_value_t *p_val) {
    return _value_with_struct_info(p_val) != NULL;
}

metac_num_t metac_value_member_count(metac_value_t *p_val) {
    return metac_entry_member_count(_value_with_struct_info(p_val));
}

metac_value_t * metac_new_value_by_member_id(metac_value_t *p_val, metac_num_t member_id) {
    metac_entry_t * p_final_entry = _value_with_struct_info(p_val);
    _check_(p_final_entry == NULL, NULL);
    _check_(member_id < 0 || member_id >= metac_entry_member_count(p_final_entry), NULL);
    metac_entry_t * p_member_entry = metac_entry_by_member_id(p_final_entry, member_id);
    _check_(p_member_entry == NULL, NULL);
    
    struct metac_member_raw_location_info bitfields_raw_info;
    if (metac_entry_member_raw_location_info(p_member_entry, &bitfields_raw_info) != 0) {
        return NULL;
    }
    return  metac_new_value(p_member_entry, 
        p_val->addr + bitfields_raw_info.byte_offset);
}

static metac_entry_t * _value_with_enumeration_info(metac_value_t *p_val) {
    _check_(p_val == NULL, NULL);
    
    metac_entry_t * p_final_entry = metac_entry_final_entry(p_val->p_entry, NULL);
    _check_(p_final_entry == NULL, NULL);
    _check_(metac_entry_kind(p_final_entry) != METAC_KND_enumeration_type, NULL);

    return p_final_entry;
}

metac_flag_t metac_value_is_enumeration(metac_value_t *p_val) {
    return _value_with_enumeration_info(p_val) != NULL;
}

metac_num_t metac_value_enumeration_info_size(metac_value_t * p_val) {
    metac_entry_t * p_final_entry = _value_with_enumeration_info(p_val);
    return metac_entry_enumeration_info_size(p_final_entry);
}

struct metac_type_enumerator_info const * metac_value_enumeration_info(metac_value_t * p_val, metac_num_t id) {
    metac_entry_t * p_final_entry = _value_with_enumeration_info(p_val);
    return metac_entry_enumeration_info(p_final_entry, id);
}

int metac_value_enumeration(metac_value_t * p_val, metac_const_value_t *p_var) {
    _check_(p_var == NULL, -(EINVAL));

    metac_entry_t * p_final_entry = _value_with_enumeration_info(p_val);
    _check_(p_final_entry == NULL, -(EINVAL));
    _check_(p_val->addr == NULL, -(EINVAL));

    metac_size_t enum_byte_size = 0;
    if (metac_entry_byte_size(p_final_entry, &enum_byte_size) != 0) {
        return -(EFAULT);
    }

    if (enum_byte_size == sizeof(char)) {
        *p_var = *(char*)p_val->addr;
        return 0;
    }
    if (enum_byte_size == sizeof(short)){
        *p_var = *(short*)p_val->addr;
        return 0;
    }
    if (enum_byte_size == sizeof(int)){
        *p_var = *(int*)p_val->addr;
        return 0;
    }
    if (enum_byte_size == sizeof(long)){
        *p_var = *(long*)p_val->addr;
        return 0;
    }
    if (enum_byte_size == sizeof(long long)){
        *p_var = *(long long*)p_val->addr;
        return 0;
    }
    return -(ENOTSUP);
}

metac_name_t metac_value_enumeration_to_name(metac_value_t * p_val, metac_const_value_t var) {
    metac_entry_t * p_final_entry = _value_with_enumeration_info(p_val);
    return metac_entry_enumeration_to_name(p_final_entry, var);
}

int metac_value_set_enumeration(metac_value_t * p_val, metac_const_value_t var) {
    metac_entry_t * p_final_entry = _value_with_enumeration_info(p_val);
    _check_(p_final_entry == NULL, -(EINVAL));
    _check_(p_val->addr == NULL, -(EINVAL));

    metac_size_t enum_byte_size = 0;
    if (metac_entry_byte_size(p_final_entry, &enum_byte_size) != 0) {
        return -(EFAULT);
    }

    if (enum_byte_size == sizeof(char)) {
        *(char*)p_val->addr = (char)var;
        return 0;
    }
    if (enum_byte_size == sizeof(short)){
        *(short*)p_val->addr = (short)var;
        return 0;
    }
    if (enum_byte_size == sizeof(int)){
        *(int*)p_val->addr = (int)var;
        return 0;
    }
    if (enum_byte_size == sizeof(long)){
        *(long*)p_val->addr = (long)var;
        return 0;
    }
    if (enum_byte_size == sizeof(long long)){
        *(long long*)p_val->addr = (long long)var;
        return 0;
    }
    return -(ENOTSUP);
}

int metac_value_equal_enumeration(metac_value_t *p_val1, metac_value_t *p_val2) {
    metac_const_value_t v1, v2;
    if (metac_value_enumeration(p_val1, &v1) != 0) {
        return -(EFAULT);
    }
    if (metac_value_enumeration(p_val2, &v2) != 0) {
        return -(EFAULT);
    }
    return (v1 == v2)?1:0;
}

metac_value_t * metac_value_copy_enumeration(metac_value_t *p_src_val, metac_value_t *p_dst_val) {
    metac_const_value_t v;
    if (metac_value_enumeration(p_src_val, &v) != 0) {
        return NULL;
    }
    if (metac_value_set_enumeration(p_dst_val, v) != 0) {
        return NULL;
    }
    return p_dst_val;
}


char *metac_value_enumeration_string(metac_value_t * p_val) {
    _check_(p_val == NULL, NULL);
    _check_(p_val->p_entry == NULL, NULL);

    if (metac_value_is_enumeration(p_val) != 0) {
        metac_const_value_t v;
        if (metac_value_enumeration(p_val, &v) != 0) {
            return NULL;
        }
        metac_name_t name = metac_value_enumeration_to_name(p_val, v);
        if (name != NULL) {
            return dsprintf("%s", name);
        }
        return dsprintf("%"PRIi64, v);
    }

    return NULL;
}

static metac_entry_t * _value_with_pointer_info(metac_value_t *p_val) {
    _check_(p_val == NULL, NULL);
    
    metac_entry_t * p_final_entry = metac_entry_final_entry(p_val->p_entry, NULL);
    _check_(p_final_entry == NULL, NULL);
    _check_(metac_entry_kind(p_final_entry) != METAC_KND_pointer_type, NULL);

    return p_final_entry;
}

metac_flag_t metac_value_is_pointer(metac_value_t *p_val) {
    return _value_with_pointer_info(p_val) != NULL;
}

metac_flag_t metac_value_is_void_pointer(metac_value_t *p_val) {
    metac_entry_t * p_final_entry = _value_with_pointer_info(p_val);
    return metac_entry_is_void_pointer(p_final_entry);
}

metac_flag_t metac_value_is_declaration_pointer(metac_value_t *p_val) {
    metac_entry_t * p_final_entry = _value_with_pointer_info(p_val);
    return metac_entry_is_declaration_pointer(p_final_entry);
}

int metac_value_pointer(metac_value_t *p_val, void ** pp_addr) {
    _check_(pp_addr == NULL, -(EINVAL));
    metac_entry_t * p_final_entry = _value_with_pointer_info(p_val);
    _check_(p_final_entry == NULL, -(EINVAL));
    _check_(p_val->addr == NULL, -(EINVAL));

    *pp_addr = *((void**)p_val->addr);
    return 0;
}

int metac_value_set_pointer(metac_value_t *p_val, void * addr) {
    metac_entry_t * p_final_entry = _value_with_pointer_info(p_val);
    _check_(p_final_entry == NULL, -(EINVAL));
    _check_(p_val->addr == NULL, -(EINVAL));

    *((void**)p_val->addr) = addr;
    return 0;
}

int metac_value_equal_pointer(metac_value_t *p_val1, metac_value_t *p_val2) {
    void *v1, *v2;
    if (metac_value_pointer(p_val1, &v1) != 0) {
        return -(EFAULT);
    }
    if (metac_value_pointer(p_val2, &v2) != 0) {
        return -(EFAULT);
    }
    return (v1 == v2)?1:0;
}


metac_value_t * metac_value_copy_pointer(metac_value_t *p_src_val, metac_value_t *p_dst_val) {
    void* v;
    if (metac_value_pointer(p_src_val, &v) != 0) {
        return NULL;
    }
    if (metac_value_set_pointer(p_dst_val, v) != 0) {
        return NULL;
    }
    return p_dst_val;
}

char *metac_value_pointer_string(metac_value_t * p_val) {
    _check_(p_val == NULL, NULL);
    _check_(p_val->p_entry == NULL, NULL);

    if (metac_value_is_pointer(p_val) == 0) {
        return NULL;
    }
    void* v;
    if (metac_value_pointer(p_val, &v) != 0) {
        return NULL;
    }
    if (v == NULL) {
        return dsprintf("NULL");    
    }
    return dsprintf("%p", v);
}

// special type of value - parameters of functions. we have a special load for it and need to cleanup addr
// when delete such objects
metac_flag_t metac_value_has_parameter_load(metac_value_t * p_val) {
    _check_(p_val == NULL, 0);
    _check_(p_val->p_entry == NULL, 0);
    return metac_entry_has_parameter_load(p_val->p_entry);
}

metac_num_t metac_value_parameter_count(metac_value_t * p_val) {
    _check_(metac_entry_has_parameter_load(p_val->p_entry) == 0, 0);


    metac_parameter_storage_t * p_pload = p_val->addr;
    // we have checked metac_entry_has_parameter_load in the beginning, so it's unspecified or va_list
    _check_(p_pload == NULL, 0);
    return metac_parameter_storage_size(p_pload);
}

metac_value_t * metac_value_parameter_new_item(metac_value_t * p_val, metac_num_t id) {
    _check_(metac_entry_has_parameter_load(p_val->p_entry) == 0, 0);

 
    // we have checked metac_entry_has_parameter_load in the beginning, so it's unspecified or va_list
    metac_parameter_storage_t * p_pload = p_val->addr;

    _check_(p_pload == NULL, 0);
    return metac_parameter_storage_new_param_value(p_pload, id);
}

static metac_value_t * metac_init_value(metac_value_t * p_val, metac_entry_t *p_entry, void * addr) {
    assert(p_val != NULL);

    metac_entry_t *p_entry_copy = p_entry;
    if (metac_entry_is_dynamic(p_entry) != 0) {
        p_entry_copy = metac_new_entry(p_entry);
        if (p_entry != NULL && p_entry_copy == NULL) {
            return NULL;
        }
    }

    p_val->p_entry = p_entry_copy;
    p_val->addr = addr;

    return p_val;
}

metac_value_t * metac_new_value(metac_entry_t *p_entry, void * addr) {
    if (addr == NULL) {
        return NULL;
    }

    metac_value_t * p_val = calloc(1, sizeof(*p_val));
    if (p_val == NULL) {
        return NULL;
    }

    if (metac_init_value(p_val, p_entry, addr) == NULL) {
        free(p_val);
        return NULL;
    }
    return p_val;
}

/**
 * this is to convert pointer or flexible array value to 
 * array with known length. 
 * count can be -1 to create flex array from pointer.
*/
metac_value_t * metac_new_element_count_value(metac_value_t *p_val, metac_num_t count) {
    _check_(p_val == NULL, NULL);
    _check_(p_val->addr == NULL, NULL);
    
    metac_entry_t * p_final_entry = metac_entry_final_entry(p_val->p_entry, NULL);

    _check_(p_final_entry == NULL, NULL);
    _check_(metac_entry_kind(p_final_entry) != METAC_KND_array_type && metac_entry_kind(p_final_entry) != METAC_KND_pointer_type, NULL);

    void * p_addr = p_val->addr;
    switch (metac_entry_kind(p_final_entry))
    {
    case METAC_KND_array_type:
        break;
    case METAC_KND_pointer_type:
        p_addr = *((void**)p_addr);
        _check_(p_addr == NULL, NULL);
        break;
    default:
        return NULL;
    }

    metac_entry_t *p_new_entry = metac_new_element_count_entry(metac_value_entry(p_val), count);
    if (p_new_entry == NULL) {
        return NULL;
    }

    metac_value_t *p_new_val = metac_new_value(p_new_entry, p_addr);
    metac_entry_delete(p_new_entry);

    return p_new_val;
}

metac_entry_t * metac_value_entry(metac_value_t * p_val) {
    _check_(p_val == NULL, NULL);
    return p_val->p_entry;
}

void * metac_value_addr(metac_value_t * p_val) {
    _check_(p_val == NULL, NULL);
    return p_val->addr;
}

metac_value_t * metac_new_value_from_value(metac_value_t * p_val) {
    void * addr = metac_value_addr(p_val);

    return metac_new_value(metac_value_entry(p_val), addr);
}

void metac_value_delete(metac_value_t * p_val) {
    // cleanup entry if it's dynamic (init creates a copy of dynamic entries)
    if (metac_entry_is_dynamic(p_val->p_entry)!=0){
        metac_entry_delete(p_val->p_entry);
    }
    // clean our mem
    free(p_val);
}
